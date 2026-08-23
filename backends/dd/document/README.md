# PauliSetBDD

一個用 BDD 表示 **multi-qubit Pauli operator 集合**的資料結構,基於 symplectic
(binary)表示法:每個 qubit 用兩個布林變數 `(x_i, z_i)` 編碼 `I,X,Y,Z`,不追蹤相位。

```
I = (0,0)   X = (1,0)   Z = (0,1)   Y = (1,1)   (Y == XZ,差一個相位,本結構不追蹤)
```

底層直接使用 **原生 BuDDy**(不依賴 MEDUSA / MoToBuddy),透過它的 C++ 介面
(`bdd` class),refcounting 全自動,不需要手動 `bdd_addref`/`bdd_delref`。

本版本範圍:**集合代數**(union / intersect / difference / complement /
membership)+ **X, Z, H, CX, CY, CZ 這 6 個 gate**(在 symplectic representation
上的變換)+ **乘上整個 Pauli 子群**(把某些 qubit 上的全部 Pauli 都加上去)
+ **OpenQASM 3 電路的 fault 傳遞**(qd/qm 兩個 register,從全 identity 出發,在每個
2-qubit gate 注入所有可能 fault,算出各 fault 數 `t` 的可達 Pauli 集合,最後再依
measurement qubit 的 syndrome 分裂)+ **多回合串接**(`PauliFlow`:一條 path 串起多個
電路,共用一個 fault 預算,每回合把 syndrome 往 `mr` 後面接、並重置 ancilla),
+ **判斷集合中是否存在乘積落在 `N(S)\S` 的一對元素**(`StabilizerCode` /
`find_undetectable_logical_pair`)+ **把這個檢查掃過整條 path 的每個 `(t, mr)` 分支**
(`check_flow`),全部已實作並測試通過。

## 檔案結構

```
pauli_bdd/
├── CMakeLists.txt
├── document/
│   ├── README.md              # 本文件
│   ├── PROJECT_HANDOFF.md     # 專案交接報告
│   └── BuDDy_ref.md           # 用到的 BuDDy 函數參考(參數/回傳值/功能)
├── include/
│   ├── pauli_bdd.hpp          # PauliSetBDD 類別宣告
│   ├── qasm_propagate.hpp     # QASM 傳遞 + PauliFlow 的介面
│   ├── stabilizer.hpp         # StabilizerCode + undetectable-pair 查詢
│   └── flow_check.hpp         # 把查詢掃過整條 path 的每個分支
├── src/
│   ├── pauli_bdd.cpp          # PauliSetBDD 實作
│   ├── qasm_propagate.cpp     # QASM 解析 + fault 傳遞 + syndrome 分裂 + PauliFlow
│   ├── stabilizer.cpp         # symplectic Gram-Schmidt + 座標變換 + 查詢
│   ├── flow_check.cpp         # 逐分支掃描
│   └── propagate_main.cpp     # propagate 這支 CLI
├── examples/
│   ├── example.qasm           # 支援語法的範例電路(qd/qm 兩個 register)
│   ├── five_qubit_se.qasm     # [[5,1,3]] 的一輪 syndrome extraction
│   └── five_qubit.code        # [[5,1,3]] 的 generators
├── test/                       # 測試原始碼,編出來的執行檔也放這裡
│   ├── demo.cpp               # 集合代數 + 元素列舉
│   ├── reorder_test.cpp       # 驗證 dynamic reordering 下變數 identity 仍然正確
│   ├── gates_test.cpp         # X/Z/H/CX/CY/CZ 6 個 gate
│   ├── subgroup_test.cpp      # 乘上整個 Pauli 子群 + reset_qubits + grow
│   ├── qasm_test.cpp          # QASM 解析/傳遞/syndrome 分裂/chain(含獨立實作比對)
│   ├── stabilizer_test.cpp    # N(S)\S 查詢(含窮舉參考實作比對)
│   └── flow_check_test.cpp    # 端到端:傳遞→分裂→逐分支檢查(含窮舉比對)
└── buddy_src/                  # BuDDy 原始碼(自行 clone,見下)
```

**執行檔**都由 `make` 產生在 `test/` 底下:`test/demo`、`test/reorder_test`、
`test/gates_test`、`test/subgroup_test`、`test/qasm_test`、`test/stabilizer_test`、`test/flow_check_test`、`test/propagate`。

## 建置

這個目錄現在是 `FTEC_flag_protocol_general_verifier` 的一個 backend,由專案根目錄的
CMake 一起建置——**不需要 autotools**。BuDDy 由 `cmake/BuDDy.cmake` 直接當成一般的
CMake target 編(它唯一真正需要的 `config.h` 只有三個版本巨集,由我們產生)。

```bash
cd <專案根目錄>
cmake -S . -B build
cmake --build build -j
ctest --test-dir build            # 全部測試,dd.* 是這個 backend 的
```

BuDDy 預設會自動 fetch。已經有 checkout 的話可以指過去,省下下載:

```bash
cmake -S . -B build -DFTEC_BUDDY_SOURCE_DIR=/path/to/buddy
```

建出來的執行檔在 `build/backends/dd/`:各個 `dd_*` 測試,以及單機驅動程式
`dd-propagate`(不經過 FPDL、直接跑一串電路時最方便)。

## 設計重點回顧(對應之前討論的決定)

- **變數宣告**:`bdd_setvarnum(2*n)`,初始順序交錯 `x0,z0,x1,z1,...`(`xvar(q)=2q`,
  `zvar(q)=2q+1`),但**不強制**任何相鄰關係。
- **Reordering**:`init()` 裡呼叫 `bdd_varblockall()`(每個變數各自獨立一個 block,
  完全自由排列)+ `bdd_autoreorder(BDD_REORDER_SIFT)`。`reorder_test.cpp`
  實際驗證了這件事:reorder 前後 level 順序被打散(`0,1,2,...` 變成
  `3,0,8,4,9,2,10,1,...`),node 數從 132 降到 115,但所有原本插入的 Pauli
  字串透過 `contains()` 查詢**仍然全部正確**——證明變數 identity(`bdd_var()`
  用的編號)跟 level(`bdd_var2level()`,會被 reorder 改變)是分開的兩件事,
  你寫的邏輯只要用固定的變數編號(`xvar`/`zvar`)就不用管 reorder 何時發生。
- **相位**:完全不追蹤,leaf 只有 `bddtrue`/`bddfalse`。
- **complement**:直接用 `bdd_not`(`operator!`),因為 2n 個變數的所有
  `2^(2n) = 4^n` 種指派都對應到合法的 Pauli 字串,不需要額外的「universe」邊界處理。

## API 參考

四個標頭檔,由下往上疊。以下列出**所有** public 進入點;後面各節講的是設計理由。

### `include/pauli_bdd.hpp` —— `PauliSetBDD`

```cpp
enum class Pauli : uint8_t { I = 0, X = 1, Z = 2, Y = 3 };   // bit0 = x, bit1 = z
```

**生命週期(process-wide singleton)**

```cpp
static void init(int n_qubits, int node_num = 100000, int cache_size = 10000);
static void done();
static int  num_qubits();
static void grow(int n_qubits);   // 只能增加;舊集合會多出 4^added 種組合,見下
static int  xvar(int q);          // = 2q,變數 identity(不受 reorder 影響)
static int  zvar(int q);          // = 2q + 1
```

`grow()` 之後,既有的 BDD 作為**函數**沒變,所以它對新變數毫無約束——作為**集合**
會悄悄多出 `4^added` 種組合。要用 `reset_qubits()` 把新 qubit 釘成 I。

**建構**

```cpp
static PauliSetBDD empty();                                  // {}
static PauliSetBDD universe();                               // 全部 4^n 個
static PauliSetBDD single(const std::vector<Pauli> &);       // {P}
static PauliSetBDD single(const std::string &);              // 例如 "IXYZ"
explicit PauliSetBDD(bdd f);                                 // 包裝現成的 bdd
```

**集合代數**(全部 const、回傳新物件)

```cpp
PauliSetBDD operator|(const PauliSetBDD &) const;   // union
PauliSetBDD operator&(const PauliSetBDD &) const;   // intersect
PauliSetBDD operator-(const PauliSetBDD &) const;   // difference
PauliSetBDD operator!() const;                       // complement
bool operator==(const PauliSetBDD &) const;
bool operator!=(const PauliSetBDD &) const;
```

**6 個 Gate**(語意見下節)

```cpp
PauliSetBDD apply_X(int q) const;                      // composition
PauliSetBDD apply_Z(int q) const;                      // composition
PauliSetBDD apply_H(int q) const;                      // conjugation
PauliSetBDD apply_CX(int control, int target) const;   // conjugation
PauliSetBDD apply_CY(int control, int target) const;   // conjugation
PauliSetBDD apply_CZ(int control, int target) const;   // conjugation
```

**子群運算**

```cpp
PauliSetBDD multiply_by_all_paulis_on(const std::vector<int> &qubits) const;
PauliSetBDD multiply_by_all_paulis_on(int a, int b) const;   // 那 16 個
PauliSetBDD reset_qubits(const std::vector<int> &qubits) const;  // 忘掉 + 釘回 I
```

**查詢**

```cpp
bool   contains(const std::vector<Pauli> &) const;
bool   contains(const std::string &) const;
bool   is_empty() const;
double size() const;         // 集合大小(bdd_satcount)
int    node_count() const;   // BDD 節點數
```

**列舉**(BuDDy 沒有內建的,見下)

```cpp
void for_each(const std::function<bool(const std::string &)> &fn) const;  // 回 false 提早停
std::vector<std::string> to_strings(std::size_t max_count = 4096) const;  // 超過上限 throw
```

**除錯 / 轉換**

```cpp
void print_raw() const;      // bdd_printset,壓縮格式(帶 don't-care)
bdd  raw() const;            // 底層 bdd
static Pauli char_to_pauli(char);
static char  pauli_to_char(Pauli);
```

### `include/qasm_propagate.hpp` —— QASM 傳遞與多回合串接

```cpp
struct SyndromeBranch {
    int         t;    // fault 數
    std::string mr;   // measurement record,回合間以 PauliFlow::kRoundSeparator 分隔
    PauliSetBDD set;
};
```

**單一電路**

```cpp
QasmPropagation propagate_qasm(const std::string &path, int tau,
                               bool reset_measure = true);

class QasmPropagation {            // 擁有整個 BuDDy session,只能 move
    int n_qubits() const;  int n_data() const;  int n_measure() const;
    int tau() const;       int n_tics() const;  int n_fault_locations() const;
    int data_qubit(int i) const;      // register index -> 全域 qubit index
    int measure_qubit(int j) const;
    const PauliSetBDD &at(int t) const;                 // 分裂前的第 t 層
    const std::vector<PauliSetBDD> &by_t() const;
    const std::vector<SyndromeBranch> &branches() const;  // 分裂後,依 (t, mr) 排序
};
```

**多回合串接**

```cpp
class PauliFlow {                  // 同樣擁有 session,只能 move
    static constexpr char kRoundSeparator = '|';
    explicit PauliFlow(int tau);   // tau 是整條 path 的總預算
    void run(const std::string &qasm_path, bool reset_measure = true);
    int tau() const;      int n_rounds() const;
    int n_data() const;   int n_measure() const;   int n_qubits() const;
    const std::vector<SyndromeBranch> &branches() const;
};
```

**同一時間只能存在一個 `QasmPropagation` 或 `PauliFlow`**(BuDDy 是 singleton),
而且它們交出來的 `PauliSetBDD` 只在物件活著時有效。

### `include/stabilizer.hpp` —— stabilizer code 與 `N(S)\S` 查詢

```cpp
struct LogicalCollision {
    bool        found = false;
    std::string syndrome;    // k bits,兩個 witness 共有
    std::string logical_a;   // 2m bits
    std::string logical_b;   // 與 logical_a 不同
    std::string witness_1;   // n_data 長的 Pauli 字串
    std::string witness_2;   // witness_1 * witness_2 落在 N(S)\S
};

class StabilizerCode {
    StabilizerCode(int n_data, const std::vector<std::string> &generators);
    int n_data() const;  int k() const;  int m() const;      // m = n_data - k > 0

    std::string syndrome_of(const std::string &pauli) const;   // k bits
    std::string logical_of(const std::string &pauli) const;    // 2m bits

    // 建構時自己選出來的代表元(不是你提供的,也不唯一)
    std::string logical_x(int j) const;
    std::string logical_z(int j) const;
    std::string destabilizer(int i) const;
};

LogicalCollision find_undetectable_logical_pair(const StabilizerCode &, const PauliSetBDD &);
bool             has_undetectable_logical_pair(const StabilizerCode &, const PauliSetBDD &);
```

**關於 `logical_x`/`logical_z`**:這些代表元是建構子用 symplectic Gram–Schmidt
**自己生的**,而且不唯一(乘上任何 stabilizer 都是同一類的合法代表)。所以
`logical_a`/`logical_b` 這兩個**標籤**、以及 `syndrome` 的 bit 順序,都會隨 generating
set 改變;但 **verdict(`found`)、witness pair 的乘積是否落在 `N(S)\S`** 不會——
判準 `λ(v₁)≠λ(v₂)` 等價於 `v₁⊕v₂ ∉ S`,這句話完全沒提到代表元。測試裡用 4 組等價
generating set 驗證過這點。

### `include/flow_check.hpp` —— 逐分支掃描

```cpp
struct FlowFailure   { int t; std::string mr; LogicalCollision collision; };
struct FlowCheckResult {
    int branches_checked;
    int min_fault_count;              // 最少幾個 fault 會失效,-1 表示都撐得住
    std::vector<FlowFailure> failures;
    bool clean() const;
};

FlowCheckResult check_flow(const StabilizerCode &, const PauliFlow &,
                           bool stop_at_first_failure = false);
FlowCheckResult check_flow(const StabilizerCode &, const QasmPropagation &,
                           bool stop_at_first_failure = false);
FlowCheckResult check_branches(const StabilizerCode &,
                               const std::vector<SyndromeBranch> &,
                               bool stop_at_first_failure = false);
```

### 一個最小的完整例子

```cpp
#include "flow_check.hpp"

pbdd::PauliFlow flow(/*tau=*/1);
flow.run("round1.qasm");
flow.run("round2.qasm");

pbdd::StabilizerCode code(flow.n_data(), {"IIIXXXX", "IXXIIXX", "XIXIXIX",
                                          "IIIZZZZ", "IZZIIZZ", "ZIZIZIZ"});
auto r = pbdd::check_flow(code, flow);

if (!r.clean()) {
    std::cout << "fails at t = " << r.min_fault_count << "\n";
    for (const auto &f : r.failures) {
        std::cout << "  mr=" << f.mr
                  << "  " << f.collision.witness_1
                  << " vs " << f.collision.witness_2 << "\n";
    }
}
```

### 列舉集合元素:`for_each` / `to_strings`

**BuDDy 沒有「逐一列出元素」的內建函數。** 它有 `bdd_allsat`,但那列舉的是
**cube**(帶 don't-care),一次 callback 可能代表 `2^k` 個元素——實測一個 16 元素的
集合只觸發**一次** callback,內容是 `----`;`bdd_printset` 也是同樣的壓縮格式。
而且 `bdd_allsat` 的 callback 是裸函數指標,沒有 user-data 參數,連有捕獲的 lambda
都不能傳。

所以 `for_each` 是自己寫的:用 `bdd_level2var`/`bdd_var`/`bdd_low`/`bdd_high` 逐
level 遞迴,把被跳過的變數(就是 don't-care)展開成兩個分支,每次 callback 剛好是
一個具體的 Pauli 字串。`to_strings(max_count)` 是收集版,**預設上限 4096**——這些
集合動輒 `4^n`,不小心整個展開是真的會出事,所以先看 `size()` 再決定。

CLI 也接了這個:

```bash
./build/backends/dd/dd-propagate 1 examples/example.qasm --list=6
```

`--list` **只印 data register 的部分**——ancilla 已經被 reset 成 identity,它原本
帶的資訊也已經在 `mr` 裡了,印出來只是雜訊。不同的完整字串可能共用同一段 data,
所以印之前會去重(因此列出的行數可能少於 `|set|`)。

## 6 個 Gate:composition vs. conjugation

X, Z, H, CX, CY, CZ 這 6 個 gate 對 Pauli 集合的作用,分成**兩種不同語意**,不要
混為一談:

- **X、Z 是 composition(Pauli 群乘法)**:X、Z 本身就是 Pauli 群的元素,施加
  X/Z gate 代表把它的 `(x,z)` 直接乘(XOR)到集合裡每個字串「目前」的
  `(x,z)` 上——例如目前是 X,施加 X gate 變成 I;目前是 X,施加 Z gate 變成
  Y。這**不是**「不追蹤相位所以是 identity」。
- **H、CX、CY、CZ 是 conjugation(標準 stabilizer tableau update rule,對應
  Aaronson–Gottesman)**:這三個是 Clifford 群元素、不是 Pauli 群元素,沒有
  自己的 `(x,z)` 可以乘,唯一有意義的作用方式是 `P → U P U†`。

在 symplectic representation 上的具體變換(不含相位):

| Gate | 語意 | 變換 |
|---|---|---|
| X(q) | composition | `(x_q, z_q) → (x_q ⊕ 1, z_q)`,只翻 x 分量 |
| Z(q) | composition | `(x_q, z_q) → (x_q, z_q ⊕ 1)`,只翻 z 分量 |
| H(q) | conjugation | `(x_q, z_q) → (z_q, x_q)`(swap) |
| CX(c,t) | conjugation | `x_t' = x_t ⊕ x_c`,`z_c' = z_c ⊕ z_t`,其餘不變 |
| CY(c,t) | conjugation | `z_c' = z_c ⊕ x_t ⊕ z_t`,`x_t' = x_t ⊕ x_c`,`z_t' = z_t ⊕ x_c`,`x_c` 不變 |
| CZ(c,t) | conjugation | `z_c' = z_c ⊕ x_t`,`z_t' = z_t ⊕ x_c`,其餘不變 |

**實作方式**:全部用 `bdd_compose(f, g, v)`(把 `f` 裡變數 `v` 替換成布林函數
`g`,單次走訪、靠 operator cache 記憶化)搞定:

- X/Z:`bdd_compose(f, bdd_nithvar(v), v)`——把 `v` 換成 `¬v`,等於把集合裡
  每個字串在該變數上的值取反。單一變數替換,一次走訪就是最有效率的做法。
- H:`bdd_replace` + `bdd_newpair`,把 `xvar(q)`、`zvar(q)` 設成互換的 pair,
  純粹改名,不涉及 XOR。
- CX/CY/CZ:多個變數要**同時**換成「XOR 運算式」(例如 CX 的 `xvar(t) := xvar(t) ^
  xvar(c)`、`zvar(c) := zvar(c) ^ zvar(t)`),用 `bdd_newpair` +
  `bdd_setbddpair`(pair 的替換目標可以是任意 BDD,不只是變數編號)建 pair
  table,再用 **`bdd_veccompose`** 一次做完兩個替換。BuDDy 文件明講:要同時替換
  多個變數時,`bdd_veccompose` 比連續呼叫 `bdd_compose` 有效率(只走訪一遍,
  不是兩遍)。因為 CX、CY、CZ 在這個表示法下都是對合(套用兩次等於沒套用),所以可
  以直接代入正向規則,不需要額外建構 transition relation BDD 或用
  `bdd_exist`/`bdd_appex` 做 relational product。

  **CY 這裡「同時」不只是效率問題,而是正確性問題**:它的 `z_c'` 引用了 `x_t` 和
  `z_t`,而那兩個本身也是被替換的目標。`bdd_veccompose` 用的是舊值(官方文件明講
  pair 裡的 BDD 可以依賴它們正在替換的變數),依序做兩次 `bdd_compose` 則會把已經
  更新過的值餵進去,答案就錯了。CX、CZ 沒有這個交纏,所以兩種做法都對。

`bdd_replace`/`bdd_newpair`/`bdd_setbddpair`/`bdd_compose`/`bdd_veccompose`
都有原生的 C++ overload(`friend bdd bdd_replace(const bdd&, bddPair*)` 之
類),不需要額外包裝。完整函數清單見 `document/BuDDy_ref.md`。

## 乘上整個 Pauli 子群:`multiply_by_all_paulis_on`

```cpp
PauliSetBDD multiply_by_all_paulis_on(const std::vector<int> &qubits) const;
PauliSetBDD multiply_by_all_paulis_on(int a, int b) const;   // 16 個 2-qubit Pauli
```

語意:把 `qubits` 上**全部** `4^|qubits|` 個 Pauli(identity 也算在內)逐一乘到
集合裡每個字串上,再取聯集:

```
{ P · G : P ∈ this, G 支撐在 qubits 上 }
```

**為什麼這是一個運算,而不是 4^k 次相乘再 union**:那些 Pauli 作為 GF(2) 向量,
不是隨便一堆向量,而恰好是由 `qubits` 的 x/z 座標張成的**線性子空間**。集合乘上
一整個子空間 = 各個 coset 的聯集 = 「這些座標以外相同、這些座標任意」,而這件事
的布林特徵函數,逐字就是**把那些變數存在量化掉**:

```
結果 = ∃ x_a, z_a, x_b, z_b, ... . S
```

所以實作是單一個 `bdd_exist`(varset 用 `bdd_makeset` 建)。直觀講:乘上這些
qubit 上的所有 Pauli,效果就是「**把這些 qubit 上原本是什麼給忘掉**」。成本與
`4^|qubits|` 無關,子集變大只是多量化幾個變數而已。

實測效果(`test/subgroup_test.cpp`):結果 BDD 通常**比原本更小**(那些變數層直接
消失),例如測試裡 node count 從 19 降到 5。相對地,naive 做法要先把 BDD 撐成 16
份再靠 15 次 union 縮回去。

`test/subgroup_test.cpp` 除了驗證冪等、`S ⊆ 結果`、結果大小必為 `4^k` 倍數、
不相交 qubit 對可交換等結構性質之外,也**跟 naive 實作(16 次相乘 + union)做
了 20 組隨機集合 × 12 種 qubit 配對的逐一比對**,結果完全一致。

**注意**:這個捷徑成立的前提是「乘上的是完整子群」。如果之後 error model 變成
只取某個**子集**(例如 weight ≤ 1),子空間性質就沒了,存在量化不再適用,得改用
`bdd_appex` 做 relational product(需要 primed 變數)——見 `document/BuDDy_ref.md` 第 5 節。

## OpenQASM 3 電路的 fault 傳遞:`propagate_qasm`

```cpp
#include "qasm_propagate.hpp"

pbdd::QasmPropagation p = pbdd::propagate_qasm("circuit.qasm", /*tau=*/2);

p.at(1);          // 分裂前:剛好 1 個 fault 時的可達 Pauli 集合
p.branches();     // 分裂後:所有非空的 (t, mr, set) 分支
```

從全 identity 出發,讓它經過電路,在每個 2-qubit gate 注入所有可能 fault,算出各
fault 數 `t = 0..tau` 的可達 Pauli 集合,最後再依 measurement qubit 上的 syndrome
把每層切開。

### 兩個 register

電路必須宣告**恰好兩個** register,名稱是 `qd`(data)與 `qm`(measurement),兩個
都不能是空的。它們被攤平成單一 qubit 索引空間,**data 在前**:

```
qd[i] → 全域 qubit i          (0 .. nd-1)
qm[j] → 全域 qubit nd + j     (nd .. nd+nm-1)
```

所以 Pauli 字串第 `i` 個字元在 `i < nd` 時是 `qd[i]`、之後是 `qm[i-nd]`。
**2-qubit gate 可以跨 register**(`cx qd[0], qm[0];`)——這正是 syndrome extraction
的核心。`data_qubit(i)` / `measure_qubit(j)` 可以拿到全域索引。

### Tic 與傳遞規則

檔案裡的 gate 依出現順序、**一個 gate 一個 tic**(即使物理上可平行也拆開)。
每個 tic 做三件事:

1. **套用 gate** —— 對**所有** `t` 的集合都做
2. **生 fault** —— 只在 2-qubit gate(`cx`/`cy`/`cz`)發生。每個 `t < tau` 的集合各生一個
   新集合放到 `t+1`:在該 gate 的兩個 qubit 上乘進全部 16 種 Pauli
   (`multiply_by_all_paulis_on`)。原集合保留,代表「這個 location 沒出錯」。
   **單 qubit gate 假定不會 fault,不生新集合。**
3. **合併** —— 新生的跟原本已在 `t+1` 的集合做 union

所有 spawn 都以**步驟 1 之後、步驟 3 之前**的狀態計算,所以一個 tic 內任何一條
lineage 最多只多一個 fault。

### Gate 語意(兩種並存,不是筆誤)

| QASM | 操作 | 語意 |
|---|---|---|
| `x q[i]` | `apply_X(i)` | **composition**(疊進 Pauli frame) |
| `z q[i]` | `apply_Z(i)` | **composition** |
| `h q[i]` | `apply_H(i)` | conjugation |
| `cx q[i],q[j]` | `apply_CX(i,j)` | conjugation |
| `cy q[i],q[j]` | `apply_CY(i,j)` | conjugation |
| `cz q[i],q[j]` | `apply_CZ(i,j)` | conjugation |

理由見上面「6 個 Gate」那節:X/Z 本身是 Pauli 群元素,可以直接乘;H/CX/CZ 是
Clifford 元素,只能 conjugate。

### Syndrome 分裂

跑完最後一個 tic 後,每個 `t` 的集合再依 syndrome 切開。

**測量是 Z basis**,所以 `qm[j]` 的測量結果會翻轉,**恰好在它身上的 Pauli 跟 Z
反對易時**——也就是 **x 分量為 1 時**(X、Y 會翻;I、Z 不會)。所以:

```
syndrome_j = qm[j] 上 Pauli 的 x 分量
```

直觀檢查:一個即將被 Z-basis 測量的 qubit,身上的 **Z error 是無害的**(跟 Z 對易,
不可能改變結果),所以它不該產生 syndrome——只有 x 分量符合這個行為。想改用 X basis
測量,就在電路末端該 qubit 上加一個 `h`(H 交換 x↔z,把 z 資訊搬到 x 上讀出來)。

每層最多切成 `2^nm` 個分支,**空的直接丟掉**——空集合代表「這個 fault 數下不可能出現
這個 syndrome」。`t = 0` 只有一個 Pauli,所以不管 `nm` 多大都只會有**一個**非空分支。

結果放在 `branches()`,依 `(t, mr)` 排序:

```cpp
struct SyndromeBranch {
    int         t;    // fault 數
    std::string mr;   // nm 個 '0'/'1',mr[j] 對應 qm[j]
    PauliSetBDD set;
};
```

**分支是原集合的一個分割**:同一個 `t` 的所有分支兩兩互斥,聯集恰好等於 `at(t)`,
大小加總等於 `at(t).size()`。這是因為切的方式是**交集**(`S & cube`)而不是 restrict
——後者會把 x_qm 變成自由變數,集合本身就變大了,`size()`/`contains()` 會很誤導。

**不需要 reorder**。`bdd_restrict`/交集這類操作跟變數順序無關,所以不必為了分裂而把
syndrome 變數搬到頂層。(順序確實會影響成本:BuDDy 的 `restrict_rec` 在
`LEVEL(r) > quantlast` 時就短路,syndrome 變數在頂層時每次切只走幾層。但實作是
深度優先遞迴、一遇空集合就整棵剪掉,對現實規模已經夠快,額外 reorder 的複雜度不划算。)

### 支援的 QASM 子集

**接受**:`OPENQASM 3;` / `OPENQASM 3.0;` 標頭(必須在最前面)、
`include "stdgates.inc";`(忽略)、`qubit[n] qd;` 與 `qubit[n] qm;`、
`x`/`z`/`h`/`cx`/`cy`/`cz` 作用在**明確索引**的 qubit 上、`barrier`(忽略)、
`//` 與 `/* */` 註解。

**報錯**:其他任何 gate、`measure`、`reset`、register 層級的 broadcast(`h qd;`)、
`qd`/`qm` 以外的 register 名稱、缺少任一個 register、重複宣告、超出範圍的索引、
`cx qd[0], qd[0];`、缺少標頭、缺少分號。錯誤訊息會帶行號與出問題的語句。

`measure` 和 `reset` 目前一律報錯——之後要處理時,`measure` 會跟 `mr`
(measurement record)接起來,`reset` 則對應「把 qm 上的資訊忘掉」。

### 生命週期(重要)

BuDDy 是 process-wide singleton,所以 `QasmPropagation` **擁有整個 BDD session**,
解構時會呼叫 `PauliSetBDD::done()`:

- 它交出來的每個 `PauliSetBDD` **只在它活著的期間有效**
- 同一時間**只能存在一個** `QasmPropagation`;要分析下一個檔案,先讓前一個離開作用域
- 只能 move、不能 copy

(這是必要的設計:`~bdd()` 會呼叫 `bdd_delref`,所以所有 BDD 都必須在 `bdd_done()`
之前解構完畢,`QasmPropagation` 的解構子就是照這個順序做的。)

### 可以拿來驗證的性質

- `t = 0` 的集合**永遠只有 1 個元素**(所有 gate 操作都是雙射),它就是累積的 Pauli
  frame;因此 `t = 0` 恰好只有一個 syndrome 分支
- 可達的最大 `t` = 電路裡 2-qubit gate 的個數;`tau` 超過這個數的那幾層是空集合
- 在可達範圍內,層層包含:`at(t-1) ⊆ at(t)`
- 同一個 `t` 的分支互斥、聯集等於 `at(t)`、大小加總等於 `at(t).size()`

`test/qasm_test.cpp` 除了驗證上述性質、syndrome 慣例(X→1、Z→0、Y→1、I→0,以及
「加 h 讓 Z error 現形」)與各種解析錯誤之外,還內含一份**用
`std::set<std::string>` 寫的獨立參考實作**(同樣的演算法、不同的資料結構),
拿 4 組電路逐一比對每個 `t` 的集合內容**以及每個 syndrome 分支**,結果完全一致。

### 順帶一提:BuDDy 的變數順序是全域的

`bddvar2level` / `bddlevel2var` 在 BuDDy 裡是**單一組全域陣列**,所有 BDD 共用一份
順序——不可能讓不同的 BDD 各有各的順序。這是 shared BDD package 的根本前提:正因為
所有節點遵守同一個順序,canonical form 才成立,`f == g` 才能是 O(1) 指標比較。

連帶一提變數的增刪:`bdd_setvarnum` / `bdd_extvarnum` **只能增加**變數數,BuDDy 沒有
刪除變數的操作。BDD 世界裡「刪掉一個變數」的正確做法是把它**量化掉**
(`bdd_exist`,也就是 `multiply_by_all_paulis_on` 在做的事);變數槽位會留著但沒有任何
BDD 依賴它,成本只是一個閒置 slot。

## 多回合串接:`PauliFlow`

```cpp
pbdd::PauliFlow flow(/*tau=*/2);   // tau 是整條 path 的總預算
flow.run("round1.qasm");            // mr = "s"
flow.run("round2.qasm");            // mr = "s|s"
flow.branches();                    // 所有存活的 (t, mr, set)
```

一條 **path** 串起多個電路,共用一個 BuDDy session 和一個 fault 預算。每次 `run()`:
把目前所有分支傳過下一個電路 → 依該回合的 syndrome 分裂 → 把結果接到每個 `mr` 後面
(以 `|` 分隔)→ 預設**重置 measurement qubit**,讓下一回合從乾淨的 ancilla 開始。

**只有 data register 會跨回合帶資訊。**

### 為什麼 ancilla 要 reset,以及為什麼不是「單純忘掉」

`reset_qubits(qubits)` 做的是「忘掉 + 釘回 identity」:

```
S' = (∃ x,z on qubits . S) ∧ (那些 qubit 都是 I)
```

**不能只做前半段的 `∃`**:那樣集合會膨脹 `4^nm` 倍,語意變成「ancilla 上可以是任何
Pauli」。拿去餵下一個電路,那些憑空冒出來的 error 會被傳播進 data,產生根本沒發生
過的錯誤。

**為什麼丟掉 ancilla 資訊是對的**:qm 上 Pauli 的 **x 分量**決定測量結果,已經記進
`mr` 了;**z 分量**跟 Z 對易、不影響測量結果,而且 qubit 隨即被重置,不會傳播到下游
——物理上本來就對未來沒有任何影響。所以 reset **在物理上是資訊無損的**,它只是把
原本就無法區分的狀態合併起來。

代價:reset 之後,同一個 `t` 的分支**不再是 `at(t)` 的分割**(不同 `mr` 的分支可能
含有相同的字串,它們靠 `mr` 區分而不是靠內容)。想看 reset 前的樣子,傳
`reset_measure = false`。

### 分組傳遞與 `(t, mr)` 合併

**`mr` 相同的分支恰好構成一個 `by_t` 向量**,所以它們被收在一起、當成單一狀態一次跑
完。獨立傳遞的次數 = **相異 `mr` 的個數**,不是分支數。

這樣做同時自動處理了一個必要的合併:

| 上一輪的分支 | 這一輪發生幾個 fault | 這一輪之後 |
|---|---|---|
| `(t=0, mr="00")` | 1 | `(t=1, mr="00\|s")` |
| `(t=1, mr="00")` | 0 | `(t=1, mr="00\|s")` |

同一個 `(t, mr)` **必須 union**——「總 fault 數相同、觀測記錄相同」的兩條路徑對外
不可區分。因為它們本來就在同一個 `by_t` 向量裡,現有的 merge 邏輯自然就合併了。

### Register 的約束

- **`qd` 寬度必須每個電路都相同**(是同一組邏輯 data qubit),不同就報錯
- **`qm` 寬度可以不同**。變數空間會長到目前看過最寬的那個(`PauliSetBDD::grow`),
  沒用到的 ancilla 槽位維持 identity

### 生命週期

跟 `QasmPropagation` 一樣:`PauliFlow` 擁有整個 BuDDy session,同一時間只能存在一個
(兩者也不能並存),只能 move 不能 copy。

### 測試

`test/qasm_test.cpp` 裡有一份**用 `std::set<std::string>` 寫的獨立 chain 參考實作**
(同演算法、不同資料結構,連分組/合併/reset 都各自實作一遍),拿 4 組 chain 逐一比對
每個 `(t, mr)` 分支的內容,完全一致。另外也驗證了 `tau` 是整條 path 的預算、
`mr` 的分隔符、`qd` 寬度不符要報錯、以及 `qm` 變寬時空間會成長且新槽位從 identity 開始。


## 判斷「是否存在一對元素的乘積落在 N(S)\S」

```cpp
#include "stabilizer.hpp"

pbdd::StabilizerCode steane(7, {"IIIXXXX", "IXXIIXX", "XIXIXIX",
                                "IIIZZZZ", "IZZIIZZ", "ZIZIZIZ"});

auto hit = pbdd::find_undetectable_logical_pair(steane, some_branch.set);
if (hit.found) {
    // hit.syndrome / hit.logical_a / hit.logical_b / hit.witness_1 / hit.witness_2
}
```

問題:給定一個 Pauli 集合,**是否存在兩個元素 `E₁`、`E₂`,使得 `E₁E₂ ∈ N(S)\S`**
(也就是一個非平凡的 logical error,code 分不出這兩個 error,修正其中一個就會傷到另一個)。

### 先把數學釐清

在 GF(2)^{2n} 上,對 `e=(x|z)` 與 `g=(a|b)`,symplectic 內積是 `⟨g,e⟩ = a·z ⊕ b·x`,
對易就是內積為 0。定義:

- **syndrome** `σ(e)_i = ⟨g_i, e⟩`,`e ∈ N(S) ⟺ σ(e) = 0`
- **logical signature** `λ(e)_j = (⟨e,Z̄_j⟩, ⟨e,X̄_j⟩)`,對 `e ∈ N(S)`:`e ∈ S ⟺ λ(e)=0`

**兩者都是線性映射**,整個結論就從這裡出來:

```
σ(v₁ ⊕ v₂) = σ(v₁) ⊕ σ(v₂)   ⟹  E₁E₂ ∈ N(S) ⟺ σ(v₁) = σ(v₂)
λ(v₁ ⊕ v₂) = λ(v₁) ⊕ λ(v₂)   ⟹  在上式成立時,E₁E₂ ∈ S ⟺ λ(v₁) = λ(v₂)
```

所以:

> **`E₁E₂ ∈ N(S)\S` ⟺ `σ(v₁)=σ(v₂)` 且 `λ(v₁)≠λ(v₂)`**

也就是「**fault-free full syndrome 相同、但 logical signature 不同**」。

**兩個 syndrome 不同的元素永遠不用檢查**:那時 `σ(v₁⊕v₂) ≠ 0`,乘積連 `N(S)` 都不在。
「相同 syndrome」不是限制性前提,而是完整的故事。

自動處理好的邊界:`E₁=E₂` 時乘積是 I ∈ S(不觸發);兩個相異元素若 `(σ,λ)` 都相同,
代表它們差一個 stabilizer,乘積 ∈ S(不觸發)。忽略相位也是對的——`E₁E₂ = ±g` 兩種
情況在 recovery 上等價。

### 判準

把兩者合成 `Φ(e) = (σ(e), λ(e))`,它是線性的且 **`ker Φ = S`**。於是

> 答案為 TRUE ⟺ **某個 syndrome 類別裡出現兩種以上的 logical signature**
> ⟺ `|Φ(B)| > |σ(B)|`

完全不需要列舉元素、兩兩相乘,也不需要跑 `4^k` 個 syndrome。

### 演算法:讓 S 變成座標對齊

「把 `Φ` 的 kernel 忘掉」就是「**把集合乘上整個 stabilizer group S**」——`Φ` 的 fiber
就是 `S` 的 coset。這跟 `multiply_by_all_paulis_on` **是同一個操作**,差別只在 `S` 一般
**不是座標對齊**的,所以不能直接 `bdd_exist`。

解法:用 symplectic Gram–Schmidt 把 `g₁..g_k` 補成完整的 symplectic 基底
(加上 destabilizers `d_i` 與 logicals `X̄_j, Z̄_j`),在新座標

```
a_i = ⟨e,d_i⟩ (stabilizer 分量)   b_i = ⟨e,g_i⟩ (syndrome)
c_j = ⟨e,Z̄_j⟩  f_j = ⟨e,X̄_j⟩      (logical signature)
```

之下,**`S` 恰好就是「b=c=f=0」這個座標對齊的子空間**。於是查詢只剩:

1. **換座標**——一次 `bdd_veccompose`。因為 `χ_{T(B)}(y) = χ_B(T⁻¹y)`,每個舊變數換成
   新變數的 XOR 式。「同時替換」在這裡是**正確性需求**(跟 `apply_CY` 同理,但交纏更嚴重)。
2. **忘掉 `a`**——一次 `bdd_exist`,這就是「乘上 S」。
3. **找出有兩種以上 logical 的 fiber**:

```
Multi(b) = ⋁_{v ∈ logical 變數} [ (∃logicals. L ∧ ¬v) ∧ (∃logicals. L ∧ v) ]
```

一個 fiber 有 ≥2 個點 ⟺ 存在某個 logical 變數在裡面同時出現 0 和 1(≥2 個點必定在
某個座標上不同)。這比數 `|Φ(B)|` 和 `|σ(B)|` 更好:**精確**(不用浮點 `bdd_satcount`)、
**O(m) 次 BDD 運算**,而且直接給出撞在一起的 syndrome 當診斷。

### 複雜度

| 階段 | 成本 |
|---|---|
| 建 `StabilizerCode`(每個 code 一次) | `O(n_d³)` GF(2) 位元運算 |
| 每次查詢 | 1 次 veccompose + 1 次 exist + `O(m)` 次 exist/and |

對照 naive 做法:窮舉所有配對是 `O(|B|²)` 次 Pauli 乘法,而 `|B|` 可達 `4^{n_d}` 量級。

**誠實的但書**:線性基底變換**可能**讓 BDD 大小暴增(BDD 對基底極度敏感,這是本質的)。
但它是**一個** native 操作而非指數級建構,dynamic reordering 也會幫忙。

### 輸入約束

generators 是長度 `n_data` 的 Pauli 字串,必須**兩兩對易且線性獨立**,而且
**`m = n_data − k` 必須大於 0**(`m=0` 時 `N(S)=S`,答案恆為 FALSE,視為輸入錯誤)。
集合可以定義在比 code 更多的 qubit 上(measurement register),那些變數會先被
`exist` 掉——stabilizer 只作用在 data qubit 上。

### 測試

`test/stabilizer_test.cpp` 內含一份**窮舉參考實作**(真的把每一對乘起來,檢查是否對易
於所有 generator 且不在 generator 的 span 裡),跟 BDD 版做了 100 組隨機比對
([[5,1,3]] 60 組、Steane 40 組)。另外驗證了:Steane 的所有 weight-1 error 集合是乾淨的
(distance 3)、`{I, 邏輯算符}` 會觸發、`{I, stabilizer}` 不會觸發、syndrome 兩兩相異的
集合不會觸發、以及回傳的 witness pair 確實在原集合裡且乘積真的落在 `N(S)\S`。


## 把檢查掃過整條 path:`check_flow`

```cpp
#include "flow_check.hpp"

pbdd::PauliFlow flow(2);
flow.run("round1.qasm");
flow.run("round2.qasm");

pbdd::StabilizerCode code(flow.n_data(), {"XZZXI", "IXZZX", "XIXZZ", "ZXIXZ"});
pbdd::FlowCheckResult r = pbdd::check_flow(code, flow);

r.min_fault_count;   // 最少幾個 fault 就會失效,-1 表示 tau 以內都撐得住
r.failures;          // 每條失效的 path:t、mr、以及 witness pair
```

對 `PauliFlow`(或單一 `QasmPropagation`)的**每個 `(t, mr)` 分支**各跑一次
`find_undetectable_logical_pair`。`min_fault_count` 是重點數字:**最少幾個 fault 就能
讓電路失效**。

兩個自動成立的性質:

- **`t = 0` 永遠不會失效**——那個分支只有一個元素(fault-free 的 Pauli frame),
  一個元素沒有配對對象
- 分支本來就照 `(t, mr)` 排序,所以第一個 failure 的 `t` 就是 `min_fault_count`;
  `stop_at_first_failure = true` 可以提早結束,**不影響這個數字**,只是不再列出其他失效路徑

### CLI

```bash
./build/backends/dd/dd-propagate 1 examples/five_qubit_se.qasm --code=examples/five_qubit.code
```

`--code=FILE` 的格式是一行一個 generator(空行與 `#`、`//` 註解會忽略)。輸出:

```
stabilizer code: [[5,1]], 4 generators from examples/five_qubit.code
2 of 3 path(s) unprotected; first failure at t = 1

  FAIL  t=1  mr=0  syndrome=1010  logical 00 vs 01
        E1 = ZIIII
        E2 = IXZII
```

這個範例的結果是有物理意義的:`five_qubit_se.qasm` 是一個**天真版**的 SE 電路,
ancilla 依序接觸多個 data qubit,所以**單一 fault 就能傳播成 weight-2 的 data error**,
distance-3 的碼擋不住——這正是 flag qubit 要防的現象。

### 測試

`test/flow_check_test.cpp` 做的是**端到端**驗證:實際跑 QASM 傳遞、syndrome 分裂,
然後把每個分支的元素**真的列舉出來、砍掉 ancilla、兩兩相乘窮舉檢查**,跟
`check_flow` 的結果比對(包含 `min_fault_count` 與失效分支數)。另外驗證回傳的
witness pair 確實存在於對應的分支裡、`t = 0` 從不失效、提早結束不影響
`min_fault_count`、以及 code 與電路的 data register 寬度不符要報錯。
