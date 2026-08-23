# FTEC flag protocol general verifier

判斷一個 fault-tolerant 協定是否真的容錯:給定 FPDL 寫的協定描述與它引用的 QASM 電路,
枚舉所有可能的執行路徑,在每個 fault location 注入所有可能的 fault,然後檢查**是否存在
一條路徑,使得兩個無法區分的 error 相乘落在 `N(S)\S`**(也就是 decoder 修正其中一個就會
傷到另一個)。

```
.fpdl ──► fpdl::Parser ──┬─► code: [[n,k,d]] + generators ──► tau = ⌊(d−1)/2⌋
                          ├─► SE 宣告(qp/qm/qf, cm/cf, file)
                          └─► SymbolicPaths ──► ftec::build_dag ──► trie
                                                                     │
                                                          DFS + 條件路由
                                                                     ▼
                                                         ┌─── ftec::Backend ───┐
                                                         │  mock               │
                                                         │  dd(BDD,尚未接上) │
                                                         │  未來其他解法       │
                                                         └─────────────────────┘
```

## 檔案結構

```
CMakeLists.txt
cmake/              BuDDy 的取得與建置
docs/               FPDL 與 parser 的說明,samples/ 是文件裡指令的產出範例
protocols/          協定測資,一個目錄一個協定(.fpdl + 它引用的 .qasm)
include/fpdl/       前端:協定解析、路徑圖
include/ftec/       驗證核心:trie、backend 介面、走訪
src/fpdl/  src/ftec/
tools/              各個執行檔的進入點
tests/
backends/dd/        decision-diagram backend(自成一套,含自己的文件與測試)
```

## 建置

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build
```

不需要 autotools。BuDDy(decision-diagram backend 的依賴)由 `cmake/BuDDy.cmake` 直接當
一般 CMake target 編;已有 checkout 可用 `-DFTEC_BUDDY_SOURCE_DIR=<path>` 指過去省下下載。

## 使用

```bash
./build/ftec-verify "protocols/CR17_[[5,1,3]]/CR17_[[5,1,3]].fpdl"          # 走完所有路徑
./build/ftec-verify "protocols/CR17_[[5,1,3]]/CR17_[[5,1,3]].fpdl" --dag    # 只看合併後的結構
```

`--bound=N` 固定 BMC bound、`--first` 遇到第一條失效路徑就停、`--backend=NAME` 選解法。

其餘工具維持原樣:`fpdlc`(解析成 paths JSON)、`fpdl-path-graph`、`fpdl-path-dag`、
`fpdl-dag-graph`(視覺化),以及 `build/backends/dd/dd-propagate`(直接對一串 QASM 跑
decision-diagram backend,不經過 FPDL)。

## 為什麼要合併路徑

協定的符號路徑共用很長的前綴。把它們合併成 **trie** 之後深度優先走訪,每個 SE 電路只算
一次而不是每條路徑各算一次:

| 協定 | paths | SE 節點 | 逐路徑列舉需要 | 倍數 |
|---|---|---|---|---|
| CR17 [[5,1,3]] | 5 | 8 | 18 | 2.3× |
| Bha23 [[5,1,3]] | 13 | 24 | 62 | 2.6× |
| CB18 [[17,1,5]] plain | 21 | 28 | 97 | 3.5× |
| **CB18 [[17,1,5]]** | **532** | **1000** | **12538** | **12.5×** |

(上表用 `--bound=200`,好跟舊數字對照;實際收斂需要 512,見下。)

深度優先還有第二個好處:每個節點的狀態集算完就留在該層的 stack frame,底下所有 subtree
共用,**不需要 memo 表**;同時存活的只有 root→目前節點這一條鏈的狀態,而不是整個 frontier。
`tests/ftec_tests.cpp` 會斷言合併後的結構真的是樹——如果 builder 出錯把節點接到兩個父節點
上,走訪會悄悄退化成逐路徑列舉,唯一的症狀只是變慢。

## BMC bound 會自動收斂,而且不能只看 `truncated`

符號展開需要有限的 transition bound。`ftec-verify` 用倍增法找到讓展開完整的最小 bound,
因為**一次解析算不算完整要看兩件事**:`ParseResult::truncated`(路徑總數被砍)以及
每條路徑各自的 `bound_exceeded`(那條路徑被砍)。

實測 CB18 [[17,1,5]]:

| bound | paths | `truncated` | `bound_exceeded` 的路徑數 |
|---|---|---|---|
| 200 | 532 | **false** | **120** |
| 256 | 889 | false | **120** |
| 512 | 1701 | false | 0 |

在 bound=200 與 256 時 `truncated` 都是 **false**,但各有 **120 條**路徑其實被截斷了。
只檢查 `truncated` 會得到「分析完整」的錯覺,而實際上有一批路徑只走到被砍的地方——而且
從 200 加到 256 雖然多找出 357 條路徑,被截斷的那 120 條**一條都沒有解決**,所以「加一點
bound 看看數字有沒有變」也不是可靠的判準。

## Backend

`ftec::Backend` 的粒度是**一個電路**而不是一整條路徑:

```cpp
virtual std::vector<std::pair<Outcome, StateId>> step(StateId, const CircuitRef&) = 0;
```

只有 backend 知道一個電路實際能產生哪些測量結果,在那裡分裂才讓 driver 能依條件路由,
也才讓前綴共用有意義。`merge` 不是可選的:同一節點、相同 record、相同 fault 數的兩個
狀態是同一個情況(不管 fault 分布在哪一輪),分開處理會報出 decoder 其實分得出來的假失效。

目前的實作狀態:

- **mock** —— 回報所有可能的測量結果、從不失效。用來檢查走訪與路由正確、以及證明
  `ftec::Backend` 真的是抽象層而不只是把 BDD 程式碼包一層。**它不判斷容錯性。**
- **dd** —— decision diagram over Pauli sets,在 `backends/dd/`,自身功能完整且有 7 組
  測試(含與窮舉實作的比對),但**還沒接到這條管線上**:它的 QASM 前端目前只吃受限方言
  (兩個叫 `qd`/`qm` 的暫存器、沒有自訂 gate、沒有 `reset`、沒有 mid-circuit `measure`),
  而這些協定四項都用到。詳見 `backends/dd/document/`。

## 還沒做的事

1. **QASM 前端升級**(最大的一塊):自訂 `gate` 內聯、`reset`、`measure`、`bit` 宣告、
   純量 `qubit`、任意暫存器名稱與三個暫存器(qd/qm/qf)、`s`/`sdg` gate。
   `s`/`sdg` 在 phase-free 下是同一個變換 `(x,z) → (x, x⊕z)`,一份實作涵蓋兩者。
2. **傳遞模型改成「`measure` 事件即分裂」**。目前 dd backend 在檔案結尾才分裂,但
   `FSE_b.qasm` 用一顆 ancilla 配合 `reset` 產生 4 個 syndrome bit,那個模型表達不出來。
   順序是**先把測量結果寫進 record,再 reset**。
3. **接上 dd backend**,先用手改成受限方言的 CR17 驗證管線,再換成真正的前端。
