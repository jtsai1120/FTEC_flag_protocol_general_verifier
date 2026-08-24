# PauliSetBDD 專案交接報告

這份文件是給接手實作的人(或 Claude Code)看的專案現況說明,不需要回頭翻對話記錄,
讀完這份 + 附帶的原始碼就能接續開發。

---

## 1. 目標

用一個 **BDD(Binary Decision Diagram)** 表示一個 **multi-qubit Pauli operator 的集合**
(例如某個 syndrome 對應的所有可能 error、某個 error-weight 上限內的所有 Pauli 字串等)。

**編碼方式(symplectic / binary representation)**:每個 qubit 用兩個布林變數
`(x_i, z_i)` 表示該 qubit 上的 Pauli 分量:

```
I = (0,0)   X = (1,0)   Z = (0,1)   Y = (1,1)
```

`Y = XZ` 差一個 `-i` 相位,本專案**完全不追蹤相位**,只在乎 Pauli 的 *type*(I/X/Y/Z
的組合)是否存在於集合中。n 個 qubit 的 Pauli 字串集合,就對應到 GF(2)^(2n) 上的
一個布林特徵函數,直接用 BDD(2n 個變數)表示。

這個想法最初設想是用 4-way DD(每層代表一個 qubit,4 種分支 I/X/Y/Z),後來確認
可以等價地拆成兩層二元分支(X 層 + Z 層)來取代,理由與可行性已在開發過程中確認過
(見下方「已確認的設計決策」)。

---

## 2. 目前已完成的部分

### 2.1 技術選型(已定案,不要再更動)

- **不使用 MEDUSA 的 interface 抽象層。** 原因:MEDUSA 的 `interface_gate_ops.h`/
  `interface_leaf.h` 整套 apply 機制是綁死在複數振幅 leaf(`LEAF_TYPE`,量子態模擬用)
  上的,跟這裡需要的「純布林集合特徵函數」語意不合,硬套會出錯(terminal 型別不相容)。
- **直接用原生 BuDDy**(`https://github.com/utwente-fmt/buddy`),不是 MEDUSA 依賴的
  MoToBuddy fork,因為完全不需要 MTBDD 的多終端(multi-terminal)機制。
- **用 BuDDy 的 C++ 介面**(`bdd` class),reference counting 全自動處理(建構子/
  解構子包好),不需要手動 `bdd_addref`/`bdd_delref`。
- **不追蹤相位**,terminal 只有 `bddtrue`/`bddfalse` 兩種。

### 2.2 已確認的設計決策(附驗證證據)

1. **變數宣告**:`bdd_setvarnum(2*n)`,每個 qubit 佔用兩個變數,`xvar(q) = 2*q`,
   `zvar(q) = 2*q + 1`,初始宣告順序交錯(X₀,Z₀,X₁,Z₁,...)。
2. **變數順序不用手動維護相鄰性。** 呼叫 `bdd_varblockall()`(每個變數各自成一個
   獨立 reorder block,無任何相鄰限制)+ `bdd_autoreorder(BDD_REORDER_SIFT)`
   後,BuDDy 會自動用 sifting 演算法找比較好的內部順序。
   **已用 `test/reorder_test.cpp` 實測驗證**:6 qubit(12 變數)插入 40 個隨機 Pauli
   字串後手動觸發 `bdd_reorder`,level 順序從 `0,1,2,...,11` 被打散成
   `3,0,8,4,9,2,10,1,6,5,7,11`,node 數從 132 降到 115,但透過固定變數編號
   (`xvar`/`zvar`)查詢的 `contains()` 全部仍然正確——證實了「變數 identity
   (`bdd_var()`)」跟「變數目前的 level(`bdd_var2level()`,會被 reorder 改變)」
   是分開的兩件事,寫程式邏輯時只要用固定的變數編號,完全不用管 reorder 何時發生。
3. **Complement 不需要額外處理 universe 邊界。** 因為 2n 個變數的所有
   `2^(2n) = 4^n` 種指派都對應到合法的 Pauli 字串(沒有「非法編碼」的情況),
   所以 `complement(S) = bdd_not(S)` 直接可用。

### 2.3 已完成的程式碼

**`include/pauli_bdd.hpp` + `src/pauli_bdd.cpp`** —— `pbdd::PauliSetBDD` 類別:

```cpp
namespace pbdd {

enum class Pauli : uint8_t { I = 0, X = 1, Z = 2, Y = 3 };  // bit0=X, bit1=Z

class PauliSetBDD {
public:
    static void init(int n_qubits, int node_num = 100000, int cache_size = 10000);
    static void done();
    static int  num_qubits();
    static int  xvar(int q);   // = 2*q
    static int  zvar(int q);   // = 2*q + 1

    static PauliSetBDD empty();
    static PauliSetBDD universe();
    static PauliSetBDD single(const std::vector<Pauli> &paulis);
    static PauliSetBDD single(const std::string &paulis);      // e.g. "IXYZ"

    explicit PauliSetBDD(bdd f);   // 包裝一個已建好的 bdd(給 gate 程式碼用)

    PauliSetBDD operator|(const PauliSetBDD &rhs) const;  // union
    PauliSetBDD operator&(const PauliSetBDD &rhs) const;  // intersect
    PauliSetBDD operator-(const PauliSetBDD &rhs) const;  // difference
    PauliSetBDD operator!() const;                         // complement

    bool operator==(const PauliSetBDD &rhs) const;
    bool operator!=(const PauliSetBDD &rhs) const;

    bool   contains(const std::vector<Pauli> &paulis) const;
    bool   contains(const std::string &paulis) const;
    bool   is_empty() const;
    double size() const;         // bdd_satcount
    int    node_count() const;   // bdd_nodecount

    void print_raw() const;      // bdd_printset
    bdd  raw() const;            // 存取底層 bdd(給 gate 程式碼用)

    // --- gates(見第 3 節,composition vs. conjugation 語意) ------------
    PauliSetBDD apply_X(int q) const;
    PauliSetBDD apply_Z(int q) const;
    PauliSetBDD apply_H(int q) const;
    PauliSetBDD apply_CX(int control, int target) const;
    PauliSetBDD apply_CY(int control, int target) const;
    PauliSetBDD apply_CZ(int control, int target) const;

    // --- 乘上整個 Pauli 子群 / reset(見第 5、7 節) ---------------------
    PauliSetBDD multiply_by_all_paulis_on(const std::vector<int> &qubits) const;
    PauliSetBDD multiply_by_all_paulis_on(int a, int b) const;
    PauliSetBDD reset_qubits(const std::vector<int> &qubits) const;  // 忘掉並釘回 I

    // --- 列舉元素(BuDDy 沒有內建的,見 document/BuDDy_ref.md 第 9 節) -----------
    void for_each(const std::function<bool(const std::string &)> &fn) const;
    std::vector<std::string> to_strings(std::size_t max_count = 4096) const;

    static void grow(int n_qubits);   // 增加 qubit 數(見 7.4)
};

} // namespace pbdd
```

**`test/demo.cpp`** —— 集合代數功能測試(union/intersect/difference/complement/
equality),全部通過,包含驗證 3-qubit universe size 剛好是 `4^3=64`。

**`test/reorder_test.cpp`** —— 上述 reorder 行為的實測驗證(見 2.2 第 2 點)。

**`test/gates_test.cpp`** —— 6 個 gate 的功能測試,涵蓋:單一 Pauli 上的
composition/conjugation 具體數值(如 `single("XI").apply_CX(0,1)==single("XX")`)、
X/Z/H/CX/CY/CZ 皆為對合(套用兩次等於沒套用)、gate 作用在多元素集合上、以及
qubit index 超界 / `control==target` 的例外處理,全部通過。

**`test/subgroup_test.cpp`** —— `multiply_by_all_paulis_on` 的測試(見第 5 節),
涵蓋具體數值、冪等、`S ⊆ 結果`、結果大小必為 `4^k` 倍數、不相交 qubit 對可交換、
「忘掉全部 qubit = universe」、空集合/universe 邊界、例外處理,並且**跟 naive
實作(16 次相乘 + 15 次 union)做了 20 組隨機集合 × 12 種 qubit 配對的逐一比對**,
結果完全一致。

**`test/qasm_test.cpp`** —— QASM 解析/檢查、fault 傳遞、syndrome 分裂的測試(見第 6
節),含一份用 `std::set<std::string>` 寫的獨立參考實作做逐一比對。

**`BuDDy_ref.md`** —— 本專案用到的(以及未來可能用到的)BuDDy 函數參考:參數、
回傳值、功能,內容整理自 BuDDy 原始碼裡的官方 doc++ 註解。

**建置狀態**:已實際 clone `utwente-fmt/buddy`、跑 `autoreconf -fi` +
`./configure --disable-shared --enable-static` + `make -C src` 編出
`libbdd.a`。**這段已經過時**:併入 `FTEC_flag_protocol_general_verifier` 之後,
BuDDy 改由 `cmake/BuDDy.cmake` 當成一般 CMake target 直接編(它唯一需要的 `config.h`
只有三個版本巨集,由我們 `configure_file` 產生),**不再需要 autotools**。

---

## 3. 6 個 Gate(已完成)

範圍鎖定在 **X, Z, H, CX, CY, CZ** 這 6 個 gate。**這裡有兩種不同語意混用,已跟使用者
確認過,不是筆誤**:

- **X、Z 是 composition(群乘法)**:這兩個本身就是 Pauli 群的元素,「施加 X/Z
  gate」代表把它的 `(x,z)` 直接乘(XOR)到集合裡每個字串「目前」的 `(x,z)` 上
  ——例如目前是 X,施加 X gate 變成 I;目前是 X,施加 Z gate 變成 Y。**不是**
  「不追蹤相位所以是 identity」(這是原本 handoff 舊版的誤解,已修正)。
- **H、CX、CY、CZ 是 conjugation(標準 stabilizer tableau update rule,對應
  Aaronson–Gottesman 那套)**:因為這三個是 Clifford 群元素、不是 Pauli 群元素,
  沒有自己的 `(x,z)` 可以乘,唯一有意義的作用方式是 `P → U P U†`。

| Gate | 語意 | 變換規則(在 `(x,z)` 上) | 局部 / 跨 qubit |
|---|---|---|---|
| X(q) | composition | `(x_q, z_q) → (x_q ⊕ 1, z_q)`(只翻 x 分量) | 局部,只碰 `x_q` |
| Z(q) | composition | `(x_q, z_q) → (x_q, z_q ⊕ 1)`(只翻 z 分量) | 局部,只碰 `z_q` |
| H(q) | conjugation | `(x_q, z_q) → (z_q, x_q)`(swap) | 局部,只影響同一 qubit 的兩個變數 |
| CX(c,t) | conjugation | `x_t' = x_t ⊕ x_c`,`z_c' = z_c ⊕ z_t`,其餘不變 | 跨 qubit |
| CY(c,t) | conjugation | `z_c' = z_c ⊕ x_t ⊕ z_t`,`x_t' = x_t ⊕ x_c`,`z_t' = z_t ⊕ x_c` | 跨 qubit |
| CZ(c,t) | conjugation | `z_c' = z_c ⊕ x_t`,`z_t' = z_t ⊕ x_c`,其餘不變 | 跨 qubit |

實作已完成,見 `include/pauli_bdd.hpp` 的 `apply_X`/`apply_Z`/`apply_H`/
`apply_CX`/`apply_CY`/`apply_CZ`,對應測試在 `test/gates_test.cpp`(全部通過,`-Wall -Wextra`
零警告)。以下 3.1–3.3 節保留作為實作方式的紀錄。

### 3.1 實際採用的作法(取代原本「遞迴比較 level」的構想)

最後全部 gate 都不需要手刻遞迴比較 level,而是統一用 BuDDy 的
**`bdd_compose`** 做「把某個變數替換成一個布林函數」這個原生操作
(`bdd_compose(f, g, v)`:把 `f` 裡所有標記為 `v` 的節點替換成函數 `g`,單次
走訪、靠 operator cache 記憶化,`O(|f|)`):

- **X(q)/Z(q)(composition,翻轉單一 bit)**:等價於「把集合裡每個字串在變數
  `v` 上的值取反」,做法是 `bdd_compose(f, bdd_nithvar(v), v)`——把 `v` 替換成
  `¬v`,直接讓 `v=0`/`v=1` 的兩個分支互換,一次走訪解決,不需要手動
  `bdd_restrict` 兩次 + `bdd_ite`(那樣要走訪兩次,常數因子較大)。`apply_X`
  對 `xvar(q)` 做、`apply_Z` 對 `zvar(q)` 做。
- **H(q)(conjugation,變數互換)**:`x_q`、`z_q` 兩個變數整個對調,是純粹的
  改名,用 `bdd_replace` + `bdd_newpair`(把 `xvar(q)↔zvar(q)` 設成互換的
  pair)直接做,不需要 XOR。
- **CX(c,t)、CZ(c,t)(conjugation,跨 qubit XOR)**:不是重新命名而是要在替換
  時「代入一個 XOR 運算式」,而且是**兩個變數同時替換**,所以用
  `bdd_newpair` + `bdd_setbddpair`(把 pair 裡的目標設成一個 BDD 運算式,不是
  單純的變數編號)建一個 pair table,再用 **`bdd_veccompose`** 一次做完兩個替換:
  - CX:`xvar(t) := xvar(t) ^ xvar(c)`、`zvar(c) := zvar(c) ^ zvar(t)`,兩個
    pair 一起丟給 `bdd_veccompose`
  - CZ:`zvar(c) := zvar(c) ^ xvar(t)`、`zvar(t) := zvar(t) ^ xvar(c)`,同樣一起
    丟給 `bdd_veccompose`
  - CY:`zvar(c) := zvar(c) ^ xvar(t) ^ zvar(t)`、`xvar(t) := xvar(t) ^ xvar(c)`、
    `zvar(t) := zvar(t) ^ xvar(c)`,三個一起丟

  **CY 特別之處:對它來說「同時替換」是正確性需求,不只是效率。** `z_c'` 引用的
  `x_t`、`z_t` 本身也是替換目標;`bdd_veccompose` 用舊值(官方文件明講 pair 裡的
  BDD 可以依賴它們正在替換的變數),而依序兩次 `bdd_compose` 會把已更新的值餵進去,
  答案就錯。CX、CZ 沒有這個交纏,兩種做法都對。CY 的規則由
  `CY = (I⊗S)·CX·(I⊗S†)` 推出(因為 `S X S† = Y`),測試裡也用這個分解對全部 16 個
  2-qubit Pauli 做了獨立交叉驗證。

  一開始的版本是連續呼叫兩次 `bdd_compose`(先後各替換一個變數),邏輯正確但
  等於走訪 `f_` 兩遍;`bdd_veccompose` 的官方文件明講這種「一次替換多個變數」
  的情境該用它、不要用連續 `bdd_compose`(效率較差),所以改成 `bdd_veccompose`
  一次做完,只走訪一遍——這是實際採用的最終版本。

  這裡沒有用到原本設想的 `bdd_exist`/`bdd_appex` + transition relation 那條路
  (那是處理「非雙射」的一般線性變換時才需要的重量級做法)。CX、CZ 在
  symplectic 表示法下都是**對合(involution,做兩次等於沒做)**,所以可以直接
  把「正向變換規則」原地代入原本的變數,不需要额外建構 transition relation
  BDD、也不需要 quantify 掉任何變數。正確性已用 `test/gates_test.cpp` 的
  具體例子(`single("XI").apply_CX(0,1) == single("XX")` 等)驗證過。

`bdd_replace`/`bdd_newpair`/`bdd_setbddpair`/`bdd_compose`/`bdd_veccompose`/
`bdd_exist`/`bdd_appex` 都已確認在 `buddy_src/src/bdd.h` 有 C++ overload
(`friend` 宣告),不需要額外包裝。完整的函數簽章/參數/回傳值/用途說明見
`BuDDy_ref.md`。

### 3.2 API 設計(已定案)

`PauliSetBDD` 所有運算(含 6 個 gate)都是 **const、回傳新物件**(functional
style,不原地修改),例如:

```cpp
PauliSetBDD apply_H(int q) const;
PauliSetBDD apply_CX(int control, int target) const;
```

跟既有的集合代數(`operator|` 等)風格一致。

---

## 4. 檔案結構

```
pauli_bdd/
├── CMakeLists.txt
├── document/
│   ├── PROJECT_HANDOFF.md   # 本文件
│   ├── README.md            # 建置說明 + API 速覽 + 設計重點
│   └── BuDDy_ref.md         # 用到的 BuDDy 函數參考
├── include/
│   ├── pauli_bdd.hpp
│   ├── qasm_propagate.hpp   # QASM 傳遞 + PauliFlow 介面
│   └── stabilizer.hpp       # StabilizerCode + N(S)\S 查詢
├── src/
│   ├── pauli_bdd.cpp
│   ├── qasm_propagate.cpp   # QASM 解析 + fault 傳遞 + syndrome 分裂 + PauliFlow
│   ├── stabilizer.cpp       # symplectic Gram-Schmidt + 座標變換 + 查詢
│   ├── flow_check.cpp       # 逐分支掃描
│   └── propagate_main.cpp   # propagate CLI
├── examples/
│   ├── example.qasm         # 支援語法的範例電路
│   ├── five_qubit_se.qasm   # [[5,1,3]] 的一輪 syndrome extraction
│   └── five_qubit.code      # [[5,1,3]] 的 generators
├── test/                     # 測試原始碼 + make 產生的執行檔
│   ├── demo.cpp              # 集合代數 + 元素列舉
│   ├── reorder_test.cpp      # reorder 行為驗證
│   ├── gates_test.cpp        # X/Z/H/CX/CY/CZ 6 個 gate
│   ├── subgroup_test.cpp     # multiply_by_all_paulis_on + reset_qubits + grow
│   ├── qasm_test.cpp         # QASM 解析/傳遞/syndrome 分裂/chain
│   ├── stabilizer_test.cpp   # N(S)\S 查詢(含窮舉參考比對)
│   └── flow_check_test.cpp   # 端到端逐分支檢查(含窮舉比對)
                             # (BuDDy 由 CMake 取得,不在版控裡)
```

**所有執行檔都由 CMake 產生在 `build/backends/dd/` 底下**(`test/demo`、`test/qasm_test`、
`dd-propagate` 等),`ctest --test-dir build` 會跑完全部。

BuDDy 由根目錄的 CMake 自動 fetch 並編譯;已有 checkout 時可用
`-DFTEC_BUDDY_SOURCE_DIR=<path>` 指過去。詳見 `document/README.md`「建置」。

---

## 5. 乘上整個 Pauli 子群(已完成)

```cpp
PauliSetBDD multiply_by_all_paulis_on(const std::vector<int> &qubits) const;
PauliSetBDD multiply_by_all_paulis_on(int a, int b) const;   // 16 個 2-qubit Pauli
```

**需求**:給定一個 Pauli 集合 `S`,把某些 qubit(例如指定的一對 `a`、`b`)上
**全部** `4^k` 個 Pauli 都乘到 `S` 的每個元素上,輸出聯集。乘法是 composition
(群乘法、忽略相位),跟 `apply_X`/`apply_Z` 同一套語意。

**關鍵觀察(這是整個實作的重點)**:那 `4^k` 個 Pauli 作為 GF(2) 向量,**不是
隨便一堆向量**,而恰好是由那些 qubit 的 x/z 座標張成的**線性子空間**。集合乘上
一整個子空間 = 各個 coset 的聯集 = 「在這些座標以外相同、在這些座標上任意」,
而這件事的布林特徵函數,逐字就是**把那些變數存在量化掉**:

```
S · V  =  { w : ∃v ∈ S, w 與 v 在那些座標以外完全相同 }
       =  ∃ x_a, z_a, x_b, z_b, ... . S
```

所以實作是**單一個 `bdd_exist`**(varset 用 `bdd_makeset` 建),不是 `4^k` 次
相乘再 union。直觀講:乘上這些 qubit 上的所有 Pauli,效果就是「把這些 qubit 上
原本是什麼**忘掉**」。成本與 `4^k` 無關,子集變大只是多量化幾個變數。

**效果比較**:

| 方案 | BDD 運算次數 | 空間 |
|---|---|---|
| naive:建 16 個 G、各乘一次、再 union | ~31 | 中間結果最多 16 份全尺寸 BDD |
| 手動 cofactor-OR(等於手工版 `bdd_exist`) | ~31 | 同上 |
| **`bdd_exist`(採用)** | **1** | 無中間結果,結果通常更小 |

結果 BDD 裡完全不會再有那些變數標記的節點,所以通常**比原本更小**——測試裡
node count 從 19 降到 5。naive 做法則是先把 BDD 撐成 16 份再靠 union 縮回去。

**15 vs 16 的差別**(使用者說兩者皆可,最後採用 16 版本):16 版本包含 identity,
所以 `S ⊆ 結果`。若要精確排除 identity(15 版本),結果會是 16 版本扣掉「孤立
點」——即 `S` 裡那些在自己的 outside-class 中沒有其他同伴的元素;要精確算出來
需要 16 次 leave-one-out OR(用 prefix/suffix 技巧可壓到 ~48 個運算),比一個
`bdd_exist` 貴 40 倍以上,不划算。

**注意適用範圍**:這個捷徑成立的前提是「乘上的是**完整**子群」。如果之後 error
model 變成只取某個**子集**(例如 weight ≤ 1 的 error),子空間性質就沒了,存在
量化不再適用,得改用 `bdd_appex` 做 relational product(需要 primed 變數,
`result(v') = ∃v ∃e. S(v) ∧ E(e) ∧ (v' = v ⊕ e)`)——也就是 3.1 節提過、CX/CZ
最後用不到的那套重機具。順帶一提,把 `E` 設成「全部 `4^k` 個」時這條式子會退化
回單純的 `∃v`,跟這裡的作法互相印證。

---

## 6. OpenQASM 3 電路的 fault 傳遞 + syndrome 分裂(已完成)

```cpp
QasmPropagation propagate_qasm(const std::string &path, int tau);
```

從全 identity 出發,經過電路,在每個 2-qubit gate 注入所有可能 fault,算出各 fault
數 `t = 0..tau` 的可達 Pauli 集合,最後依 measurement qubit 上的 syndrome 把每層切開。

### 6.1 兩個 register 的攤平

電路必須宣告**恰好兩個** register:`qd`(data)與 `qm`(measurement),兩個都不能是
空的(使用者指定 `nm = 0` 不允許)。攤平成單一 qubit 索引空間,**data 在前**:

```
qd[i] → 全域 qubit i          (0 .. nd-1)
qm[j] → 全域 qubit nd + j     (nd .. nd+nm-1)
```

Pauli 字串第 `i` 個字元在 `i < nd` 時是 `qd[i]`、之後是 `qm[i-nd]`。**2-qubit gate
可以跨 register**(`cx qd[0], qm[0];`)——syndrome extraction 的核心。
`data_qubit(i)` / `measure_qubit(j)` 給呼叫端拿全域索引。

字串佈局(哪個 register 佔低索引)純粹是可讀性選擇,**跟變數順序無關**——reorder 動的
是 level,不會改變「字串第 i 個字元 ↔ qubit i ↔ 變數 2i, 2i+1」這個固定對應。

### 6.2 Tic 與傳遞規則

檔案裡的 gate 依出現順序、**一個 gate 一個 tic**(即使物理上可平行也拆開,tic 數 =
gate 數)。每個 tic 依序:

1. **套用 gate** —— 對**所有** `t` 的集合都做
2. **生 fault** —— **只在 2-qubit gate(`cx`/`cy`/`cz`)發生**。每個 `t < tau` 的集合各生
   一個新集合放到 `t+1`:`multiply_by_all_paulis_on(i, j)`。原集合保留。
   **單 qubit gate 假定不會 fault**(使用者指定的模型)。
3. **合併** —— 新生的跟原本已在 `t+1` 的集合做 union

**關鍵實作細節**:所有 spawn 都以**步驟 1 之後、步驟 3 之前**的狀態計算(先把所有
spawn 算進暫存 vector,再一起 merge)。這是「一個 tic 內任何 lineage 最多多一個
fault」的來源;邊算邊寫回去的話,這個 tic 剛生出來的 `t+1` 會被誤當成來源再生 `t+2`。

### 6.3 Gate 語意

`x`/`z` 用 **composition**,`h`/`cx`/`cz` 用 **conjugation**。理由見第 3 節。這是
使用者明確指定的 Pauli frame tracking 模型,不是筆誤。

### 6.4 Syndrome 分裂

**測量是 Z basis**(使用者說:想要 x-meas 就在前面加 H)。`qm[j]` 的測量結果翻轉,
恰好在它身上的 Pauli 跟 Z **反對易**時,也就是 **x 分量為 1**(X、Y 翻;I、Z 不翻):

```
syndrome_j = qm[j] 上 Pauli 的 x 分量
```

> **這一點使用者一開始答成「z 分量」,經過推導後更正為 x 分量。** 最直觀的檢查:
> 一個即將被 Z-basis 測量的 qubit,身上的 Z error 是無害的(跟 Z 對易,不可能改變
> 結果),所以不該產生 syndrome——只有 x 分量符合這個行為。使用者「加 H 把 x-meas
> 轉成 z-meas」的設計也正是支持 x 分量:H 交換 x↔z,把 z 資訊搬到 x 上讀出來。

每層最多切成 `2^nm` 個分支,**空的直接丟掉**(空集合 = 這個 fault 數下不可能出現這個
syndrome)。`t = 0` 只有一個 Pauli,所以不管 `nm` 多大都只有**一個**非空分支。

結果放在 `branches()`,依 `(t, mr)` 排序,`mr[j]` 對應 `qm[j]`:

```cpp
struct SyndromeBranch { int t; std::string mr; PauliSetBDD set; };
```

**用交集(`S & cube`)而不是 `bdd_restrict`**——這點我一開始提案錯了。restrict 會把
`x_qm` 變成自由變數,集合本身就變大(每個 qm qubit 讓 size 加倍),`size()`/`contains()`
會很誤導。用交集則保證**各分支恰好是原集合的一個分割**:同 `t` 的分支兩兩互斥、聯集
等於 `at(t)`、大小加總等於 `at(t).size()`。這也是很好的測試性質。

**不 reorder**(使用者決定)。切分用的交集/restrict 跟變數順序**無關**,所以不需要
把 syndrome 變數搬到頂層。順序確實影響成本——BuDDy 的 `restrict_rec`
(`bddop.c:945`)在 `LEVEL(r) > quantlast` 時短路,syndrome 變數在頂層時每次切只走
幾層——但實作是**深度優先遞迴、一遇空集合就整棵剪掉**,對現實規模已經夠快。

### 6.5 支援的 QASM 子集

**接受**:`OPENQASM 3;` / `OPENQASM 3.0;`(必須第一個)、`include "stdgates.inc";`
(忽略)、`qubit[n] qd;` 與 `qubit[n] qm;`、`x`/`z`/`h`/`cx`/`cy`/`cz` 作用在**明確索引**
的 qubit、`barrier`(忽略)、`//` 與 `/* */` 註解。

**報錯**:其他 gate、`measure`、`reset`、broadcast(`h qd;`)、`qd`/`qm` 以外的
register 名稱、缺少任一 register、重複宣告、超界索引、`cx qd[0], qd[0];`、缺標頭、
缺分號。錯誤訊息帶行號與出問題的語句。只支援 OQ3 原生語法,不接受 2.0 的 `qreg`。

### 6.6 生命週期(重要,不是可有可無的包裝)

BuDDy 是 process-wide singleton,而 `~bdd()` 會呼叫 `bdd_delref`——所以**所有 BDD
都必須在 `bdd_done()` 之前解構完畢**。因此不能「函數內 init + 跑完 + done 再回傳
BDD」,那樣回傳的 BDD 全是 dangling。

採用 RAII:`QasmPropagation` **擁有整個 session**,解構子裡**先 `branches_.clear()`
和 `by_t_.clear()`(在 package 還活著時放掉所有 bdd 參照)、再 `PauliSetBDD::done()`**。
連帶限制:

- 交出來的 `PauliSetBDD` 只在它活著的期間有效
- 同一時間只能存在一個 `QasmPropagation`
- 只能 move、不能 copy;move 過的來源不會重複 `done()`
- 解析失敗或建構中途 throw 時,session 會被收乾淨(測試有涵蓋)

### 6.7 BuDDy 變數順序是全域的(回答使用者的疑問)

`bddvar2level` / `bddlevel2var` 在 `kernel.c` 是**單一組全域陣列**,所有 BDD 共用一份
順序,**不可能讓不同 BDD 各有各的順序**。這是 shared BDD package 的根本前提:正因為
所有節點遵守同一順序,canonical form 才成立,`f == g` 才能是 O(1) 指標比較。

變數增刪:`bdd_setvarnum` / `bdd_extvarnum` **只能增加**,BuDDy 沒有刪除變數的操作。
BDD 世界裡「刪掉變數」的正確做法是**量化掉**(`bdd_exist`,即
`multiply_by_all_paulis_on` 在做的事);槽位留著但沒有 BDD 依賴它,成本只是一個閒置
slot。新增的變數會被附加在順序**最底層**,要放別處得再 reorder。

### 6.8 可驗證的性質(測試有涵蓋)

- `t = 0` 的集合**永遠只有 1 個元素**,就是累積的 Pauli frame;因此 `t = 0` 恰好只有
  一個 syndrome 分支
- 可達的最大 `t` = 2-qubit gate 的個數;`tau` 超過的那幾層是空集合
- 可達範圍內層層包含:`at(t-1) ⊆ at(t)`
- 同 `t` 的分支互斥、聯集 = `at(t)`、大小加總 = `at(t).size()`
- syndrome 慣例:X→1、Z→0、Y→1、I→0;`z qm[0]; h qm[0];` → 1(H 讓 Z error 現形)

`test/qasm_test.cpp` 另外內含一份**用 `std::set<std::string>` 寫的獨立參考實作**
(同演算法、不同資料結構),拿 4 組電路逐一比對每個 `t` 的集合內容**以及每個 syndrome
分支**,完全一致。

(CLI 見 7.7,它走的是 `PauliFlow`,單一電路就是只有一回合的 chain。)

---

## 7. 多回合串接:`PauliFlow`(已完成)

```cpp
PauliFlow flow(tau);            // tau 是整條 path(chain)的總 fault 預算
flow.run("round1.qasm");         // mr = "s"
flow.run("round2.qasm");         // mr = "s|s"
flow.branches();                 // 所有存活的 (t, mr, set)
```

一條 path 串起多個電路,共用一個 BuDDy session 和一個 fault 預算。使用者的目標是驗
**adaptive flow 的 SE sequence path**,這就是那個結構。

### 7.1 每次 `run()` 做的事

1. 解析電路;第一次 `run()` 決定 `nd` 並開 session
2. 把目前的分支**依 `mr` 分組**,每組還原成一個 `by_t` 向量
3. 每組各跑一次 tic 迴圈(跟第 6 節完全一樣的程式碼)
4. 依這回合的 syndrome 分裂,把結果接到 `mr` 後面(分隔符 `|`)
5. 預設 **reset measurement qubit**,讓下一回合從乾淨 ancilla 開始
6. 依 `(t, mr)` 排序

### 7.2 關鍵設計:分組傳遞自動處理必要的合併

**`mr` 相同的分支恰好構成一個 `by_t` 向量**,所以收在一起當單一狀態跑。獨立傳遞次數
= 相異 `mr` 的個數,不是分支數。

這同時自動解決了一個**必要**的合併:

| 上一輪分支 | 這輪 fault 數 | 這輪之後 |
|---|---|---|
| `(t=0, mr="00")` | 1 | `(t=1, mr="00\|s")` |
| `(t=1, mr="00")` | 0 | `(t=1, mr="00\|s")` |

同一個 `(t, mr)` **必須 union**(總 fault 數與觀測記錄都相同 = 對外不可區分)。因為
兩者本來就在同一個 `by_t` 向量裡,現有 merge 邏輯自然就合併了,不需要額外處理。

### 7.3 `reset_qubits`:為什麼不能只做 `∃`

```cpp
PauliSetBDD reset_qubits(const std::vector<int> &qubits) const;
```

做的是 `(∃ x,z on qubits . S) ∧ (那些 qubit 都是 I)`。

**只做前半段的 `∃` 是錯的**:集合會膨脹 `4^nm` 倍,語意變成「ancilla 上可以是任何
Pauli」;餵進下一個電路後,那些憑空冒出來的 error 會傳播進 data,產生沒發生過的錯誤。

**丟掉 ancilla 資訊為什麼正確**:x 分量決定測量結果,已記進 `mr`;z 分量跟 Z 對易、
不影響測量結果,且 qubit 隨即重置,不傳播到下游——物理上本來就對未來無影響。所以
reset **在物理上資訊無損**,只是把原本無法區分的狀態合併起來。

這個方法同時就是未來 **`reset` 指令**的實作。

**代價(要記得)**:reset 之後,同 `t` 的分支**不再是 `at(t)` 的分割**——不同 `mr`
的分支可能含相同字串,靠 `mr` 區分而非內容。要看 reset 前的樣子傳
`reset_measure = false`(`propagate_qasm` 和 `PauliFlow::run` 都有這個參數,預設 true)。

### 7.4 Register 約束與變數空間成長

- **`qd` 寬度必須每個電路相同**(同一組邏輯 data qubit),不同就報錯
- **`qm` 寬度可以不同**。變數空間長到看過最寬的那個,用 `PauliSetBDD::grow()`

```cpp
static void PauliSetBDD::grow(int n_qubits);   // 只能增加
```

**成長後的陷阱**:舊的 BDD 作為**函數**沒變,所以它對新變數毫無約束——作為**集合**
會悄悄多出 `4^added` 種組合。所以 `PauliFlow` 一成長就立刻對所有分支
`reset_qubits(新槽位)` 把它們釘成 I。

### 7.5 實測踩到的 BuDDy 坑:`bdd_setvarnum` 之後 satcount 會過期

`bdd_satcount` 的值取決於**總變數數**(它對每個被跳過的 level 乘 `2^gap`,包含往下到
terminal 的那段,而 terminal 的 level 就是 `bddvarnum`)。但 `bdd_setvarnum` 內部只
呼叫 `bdd_operator_varresize()`,那個函數**只重配 `quantvarset`,不清 operator
cache**。

實測:4 qubit 下 `single("XYZI").size()` = 1;`grow(5)` 後應該是 4,但**若成長前查詢
過 size()**,回傳的仍是 1——同時 `contains("XYZIX")` 卻是 true,自相矛盾。成長前沒查
過就正確回傳 4。

**解法**:`grow()` 裡在 `bdd_setvarnum` 之後呼叫 `bdd_gbc()`,它內部會執行
`bdd_operator_reset()` 清空 cache。已修,並有測試涵蓋。

### 7.6 測試

`test/qasm_test.cpp` 裡有一份**用 `std::set<std::string>` 寫的獨立 chain 參考實作**
(同演算法、不同資料結構,連分組/合併/reset 都各自實作一遍),拿 4 組 chain 逐一比對
每個 `(t, mr)` 分支的內容,完全一致。另外驗證了 `tau` 是整條 path 的預算(tau=0 跑三
回合仍只有一條路徑)、`mr` 分隔符、`qd` 寬度不符報錯、`qm` 從 1 長到 3 時空間成長且
新槽位從 identity 開始。`test/subgroup_test.cpp` 則涵蓋 `reset_qubits` 與 `grow`
本身(含上面那個 cache 陷阱)。

### 7.7 CLI

```bash
./build/backends/dd/dd-propagate <tau> <circuit.qasm> [more.qasm ...] [--list[=N]]
```

`tau` 在前,之後可以接**多個** qasm 檔(就是一條 chain)。

`--list` **只印 data register**——ancilla 已被 reset 成 identity,它原本帶的資訊也
已經在 `mr` 裡,印出來只是雜訊。不同的完整字串可能共用同一段 data,所以印前會去重
(列出的行數可能少於 `|set|`)。

---

## 8. 判斷 N(S)\S 配對:`StabilizerCode`(已完成)

```cpp
StabilizerCode code(n_data, generators);           // 前處理只做一次
LogicalCollision hit = find_undetectable_logical_pair(code, some_set);
bool b = has_undetectable_logical_pair(code, some_set);   // 只要 bool 時
```

**問題**:給定一個 Pauli 集合,是否存在兩個元素 `E₁`、`E₂` 使 `E₁E₂ ∈ N(S)\S`。

### 8.1 使用者的假說已證明,而且第二個疑問不需要處理

在 GF(2)^{2n} 上,`σ(e)_i = ⟨g_i,e⟩` 與 `λ(e)_j = (⟨e,Z̄_j⟩, ⟨e,X̄_j⟩)` **都是線性映射**。
於是:

```
σ(v₁⊕v₂) = σ(v₁)⊕σ(v₂)  ⟹  E₁E₂ ∈ N(S) ⟺ σ(v₁)=σ(v₂)   (fault-free full syndrome 相同)
λ(v₁⊕v₂) = λ(v₁)⊕λ(v₂)  ⟹  在此前提下 E₁E₂ ∈ S ⟺ λ(v₁)=λ(v₂)
```

所以 **`E₁E₂ ∈ N(S)\S ⟺ σ 相同且 λ 不同`**——假說成立,而且是 iff。

**「syndrome 不同時要不要也檢查」的答案是:不用。** 那時 `σ(v₁⊕v₂) ≠ 0`,乘積連 `N(S)`
都不在。相同 syndrome 不是限制性前提,而是完整的故事。

自動處理好的邊界:`E₁=E₂` 乘積是 I ∈ S;相異但 `(σ,λ)` 都相同的兩個元素差一個
stabilizer,乘積 ∈ S;忽略相位是對的(`E₁E₂ = ±g` 在 recovery 上等價)。

### 8.2 判準與演算法

合成 `Φ(e) = (σ(e), λ(e))`,線性且 **`ker Φ = S`**。答案為 TRUE ⟺ 某個 syndrome
類別裡出現兩種以上的 logical signature ⟺ `|Φ(B)| > |σ(B)|`。

**關鍵**:「忘掉 `Φ` 的 kernel」= 「乘上整個 stabilizer group S」——`Φ` 的 fiber 就是
`S` 的 coset。這跟 `multiply_by_all_paulis_on` **是同一個操作**,差別只在 `S` 一般
**不是座標對齊**的。

解法:symplectic Gram–Schmidt 把 `g₁..g_k` 補成完整 symplectic 基底
(destabilizers `d_i`、logicals `X̄_j, Z̄_j`),新座標為

```
a_i = ⟨e,d_i⟩   b_i = ⟨e,g_i⟩ (syndrome)   c_j = ⟨e,Z̄_j⟩   f_j = ⟨e,X̄_j⟩
```

在這組座標下 **`S` 就是「b=c=f=0」的座標對齊子空間**。查詢變成三步:

1. `bdd_veccompose` 換座標(`χ_{T(B)}(y) = χ_B(T⁻¹y)`,所以代入 `T⁻¹`)。
   **同時替換是正確性需求**,跟 `apply_CY` 同理但交纏更嚴重。
2. `bdd_exist` 忘掉 `a`(這就是「乘上 S」)。
3. 找出有 ≥2 種 logical 的 fiber:

```
Multi(b) = ⋁_{v ∈ logical 變數} [ (∃logicals. L ∧ ¬v) ∧ (∃logicals. L ∧ v) ]
```

一個 fiber 有 ≥2 個點 ⟺ 存在某個 logical 變數在裡面同時出現 0 和 1。**這比數
`|Φ(B)|` 與 `|σ(B)|` 好**:精確(不用浮點 `bdd_satcount`)、`O(m)` 次 BDD 運算,而且
直接給出撞在一起的 syndrome 當診斷。

### 8.3 複雜度與但書

| 階段 | 成本 |
|---|---|
| 建 `StabilizerCode` | `O(n_d³)` GF(2) 位元運算,每個 code 一次 |
| 每次查詢 | 1 veccompose + 1 exist + `O(m)` 次 exist/and |

窮舉配對是 `O(|B|²)` 次乘法而 `|B|` 可達 `4^{n_d}`,完全不可行。

**但書**:線性基底變換**可能**讓 BDD 大小暴增(BDD 對基底極度敏感,本質問題,不是實作
缺陷)。但它是**一個** native 操作而非指數級建構。

### 8.4 輸入約束與診斷輸出

generators 是長度 `n_data` 的 Pauli 字串,必須**兩兩對易、線性獨立**,且
**`m = n_data − k > 0`**(`m=0` 時 `N(S)=S`,依使用者決定視為輸入錯誤)。集合可以定義
在比 code 更多的 qubit 上,多出來的(measurement register)會先被 `exist` 掉。

回傳 `LogicalCollision`:`found`、撞在一起的 `syndrome`、兩個相異的
`logical_a`/`logical_b`、以及**具體的 witness pair**(`witness_1`、`witness_2`,
data qubit 上的 Pauli 字串)。

### 8.5 測試

`test/stabilizer_test.cpp` 內含**窮舉參考實作**(真的把每一對乘起來,檢查是否對易於所有
generator 且不在其 span 裡),做了 100 組隨機比對([[5,1,3]] 60 組、Steane 40 組)。
另外驗證:Steane 所有 weight-1 error 集合乾淨(distance 3)、`{I, 邏輯算符}` 觸發、
`{I, stabilizer}` 不觸發、syndrome 兩兩相異的集合不觸發、witness pair 確實在原集合裡
且乘積真的落在 `N(S)\S`、以及 measurement register 有被正確投影掉。

---

## 9. 掃過整條 path:`check_flow`(已完成)

```cpp
FlowCheckResult check_flow(const StabilizerCode &, const PauliFlow &,
                           bool stop_at_first_failure = false);
FlowCheckResult check_flow(const StabilizerCode &, const QasmPropagation &, bool = false);
```

把第 8 節的查詢對 **每個 `(t, mr)` 分支**各跑一次。回傳:

- `min_fault_count` —— **最少幾個 fault 就能讓電路失效**,`-1` 表示 `tau` 以內都撐得住。
  這是整個工具的重點數字。
- `failures` —— 每條失效 path 的 `t`、`mr`,以及第 8 節那份完整診斷(syndrome、兩個
  logical signature、witness pair)
- `branches_checked`

### 9.1 兩個自動成立的性質

- **`t = 0` 永遠不會失效**:那個分支只有一個元素(fault-free 的 Pauli frame),一個元素
  沒有配對對象。不需要特判,演算法自然如此。
- 分支本來就照 `(t, mr)` 排序,所以第一個 failure 的 `t` 就是 `min_fault_count`。
  `stop_at_first_failure = true` 提早結束**不影響這個數字**,只是不再列出其他失效路徑。

### 9.2 CLI

```bash
./build/backends/dd/dd-propagate 1 examples/five_qubit_se.qasm --code=examples/five_qubit.code
```

`--code=FILE` 一行一個 generator(空行與 `#`、`//` 註解忽略)。

範例輸出的結果**有物理意義**:`five_qubit_se.qasm` 是天真版 SE 電路,ancilla 依序接觸
多個 data qubit,所以單一 fault 就能傳播成 weight-2 的 data error,distance-3 的碼擋不住
——這正是 flag qubit 要防的現象,也是這個專案要驗的東西。

### 9.3 測試

`test/flow_check_test.cpp` 是**端到端**驗證:實際跑 QASM 傳遞與 syndrome 分裂,然後把每個
分支的元素**真的列舉出來、砍掉 ancilla、兩兩相乘窮舉檢查**,跟 `check_flow` 比對
(`min_fault_count` 與失效分支數都比)。另驗證 witness pair 確實在對應分支裡、`t=0` 從不
失效、提早結束不影響 `min_fault_count`、寬度不符要報錯。

---

## 10. 目前狀態小結

底層集合代數、6 個 gate、乘上整個 Pauli 子群、元素列舉、OpenQASM 3 傳遞與 syndrome
分裂、多回合串接、`N(S)\S` 配對查詢、以及逐分支掃描,全部已實作並通過測試:

```bash
ctest --test-dir build -R '^dd\.'
```

下一步(尚未開始):

- **adaptive**:目前每回合對所有分支跑**同一個**電路。真正的 adaptive flow 是「下一回合
  跑哪個電路取決於目前的 `mr`」,需要在 `PauliFlow::run()` 之外加一個依 `mr` 選電路的介面。
  這是目前最自然的下一步。

- **`measure` 與 `reset` 指令**:目前都報錯。`reset` 的語意已經有現成實作
  (`PauliSetBDD::reset_qubits`),只要接上 parser 即可。`measure` 則要決定它跟
  `PauliFlow` 的回合邊界怎麼對應——目前回合邊界是「一個 qasm 檔 = 一回合」,若
  `measure` 要能在檔案中間標記回合,傳遞邏輯得再拆一層。
- **adaptive**:目前每回合對所有分支跑**同一個**電路。真正的 adaptive flow 是「下一
  回合跑哪個電路取決於目前的 `mr`」,所以 `run()` 之外會需要一個「依 `mr` 選電路」的
  介面(例如 `run_adaptive(std::function<std::string(const std::string &mr)>)`)。
- **記憶體**:`QasmPropagation` 仍保留分裂前的 `by_t`;`PauliFlow` 沒有(只留分支)。
  回合數一多要回來審視分支數成長。
- 若 error model 需要「乘上子群的某個**子集**」(weight ≤ k 之類),照第 5 節最後一段
  的 relational product 做法實作。
