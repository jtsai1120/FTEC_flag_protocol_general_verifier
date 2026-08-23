# BuDDy 函數參考(本專案用到的 + 未來可能用到的)

這份文件整理 `PauliSetBDD` 目前用到、以及 handoff 文件裡提過未來可能用到的
BuDDy 函數:參數、回傳值、功能。內容直接根據 `buddy_src/src/*.c` 裡的官方
doc++ 註解(`NAME`/`PROTO`/`DESCR`/`RETURN` 區塊)整理,不是憑印象寫的。

每個函數標了**使用狀態**:
- **已使用** = `pauli_bdd.cpp` 或 `test/*.cpp` 裡實際呼叫到
- **未使用,備用** = handoff 討論過、目前沒用到,但屬於同一類操作,未來可能會用到

C++ 呼叫都透過 `bdd` class 的 overload(接受 `const bdd&`、回傳 `bdd`),不需要
手動處理底層 `BDD`(= `int`,節點的 handle)型別或 refcounting。

---

## 1. 套件生命週期

### `bdd_init(int nodesize, int cachesize)` → `int`
**已使用**(`PauliSetBDD::init()`)

初始化整個 BDD package,**必須在任何 BDD 操作之前呼叫一次**。`nodesize` 是
node table 的初始節點數(小範例約 10000,大範例可到 1000000),`cachesize` 是
內部 cache 的固定大小(10000 通常夠用)。初始節點數不影響正確性,只影響效率
(節點不夠時 table 會自動擴充)。回傳 0 表示成功,負值表示錯誤碼。

### `bdd_done()` → `void`
**已使用**(`PauliSetBDD::done()`)

釋放 BDD package 用掉的所有記憶體,把 package 重設回初始狀態。

### `bdd_setvarnum(int num)` → `int`
**已使用**(`PauliSetBDD::init()` 與 `PauliSetBDD::grow()`)

設定 package 要用的變數個數。可以呼叫多次,但只能**增加**變數數量,不能減少。
成功回傳 0,否則回傳負值錯誤碼。

> **⚠️ 實測踩到的坑:增加變數數之後,舊的 `bdd_satcount` 結果會過期。**
>
> `bdd_satcount` 的值取決於**總變數數**——它對每個被跳過的 level 都乘上 `2^gap`,
> 包含從最後一個節點往下到 terminal 的那段(terminal 的 level 就是 `bddvarnum`)。
> `bdd_setvarnum` 內部只呼叫 `bdd_operator_varresize()`,而那個函數**只重配
> `quantvarset`,不會清 operator cache**。所以變數數改變前算過並被 cache 住的
> satcount,之後會被原封不動地回傳,數字偏小。
>
> 實測:4 qubit 下 `single("XYZI").size()` = 1;`grow(5)` 之後應該是 4(新 qubit
> 自由),但**若成長前查詢過 size()**,回傳的仍是 1——而同時 `contains("XYZIX")`
> 卻是 true,自相矛盾。成長前沒查過就正確回傳 4。
>
> **解法**:成長後呼叫 `bdd_gbc()`(見第 1 節),它內部會執行
> `bdd_operator_reset()` 把 cache 清乾淨。`PauliSetBDD::grow()` 就是這樣做的。

### `bdd_gbc()` → `void`
**已使用**(`PauliSetBDD::grow()`)

立刻執行一次 garbage collection。除了回收沒有參照的節點之外,它**內部會呼叫
`bdd_operator_reset()` 清空所有 operator cache**——這正是變數數改變後必須做的事
(見上面的坑)。平常不需要手動呼叫,節點不夠時會自動觸發。

---

## 2. 建立基本 BDD / 常數

### `bdd_ithvar(int var)` → `BDD`
**已使用**(`single()`、`apply_CX`/`apply_CZ` 建 XOR 運算式時)

回傳代表「第 `var` 個變數」的 BDD(單一節點,low=false, high=true)。`var`
必須在 `bdd_setvarnum` 宣告的範圍內,從 0 開始算。這個回傳值不需要額外
`bdd_addref`(C++ `bdd` class 自動管理)。

### `bdd_nithvar(int var)` → `BDD`
**已使用**(`apply_X`/`apply_Z` 的 `flip` 運算)

回傳代表「第 `var` 個變數的否定」的 BDD(low=true, high=false)。其餘同
`bdd_ithvar`。

### `bddtrue` / `bddfalse`
**已使用**(`empty()`、`universe()`、`is_empty()` 等)

兩個全域常數,分別代表恆真 / 恆假的 BDD(這個專案唯一用到的兩個 terminal,
因為完全不追蹤相位,不需要 multi-terminal)。

---

## 3. 布林組合(apply 系列)

### `bdd_apply(BDD left, BDD right, int opr)` → `BDD`
**已使用**(透過 C++ operator,見下)

對兩個 BDD 做二元布林運算,`opr` 決定是哪種運算(`bddop_and`=0,
`bddop_xor`=1,`bddop_or`=2,`bddop_diff`=7 等)。C++ `bdd` class 把常用的
包成 operator,這個專案實際用到的對應關係:

| C++ operator | `bddop_*` | 本專案用途 |
|---|---|---|
| `f \| g` | `bddop_or` | 集合 union(`operator\|`) |
| `f & g` | `bddop_and` | 集合 intersect(`operator&`)、`contains()` |
| `f - g` | `bddop_diff` | 集合 difference(`operator-`) |
| `f ^ g` | `bddop_xor` | 建 CX/CZ 的 XOR 替換式(`bdd_ithvar(...) ^ bdd_ithvar(...)`) |

### `bdd_not(BDD r)` → `BDD`
**已使用**(透過 C++ `operator!`,即 `PauliSetBDD::operator!`)

對 `r` 取反(把 0-terminal 和 1-terminal 的參照互換)。用來做集合 complement,
因為 2n 個變數的所有指派都對應合法 Pauli 字串,不需要額外處理 universe 邊界。

---

## 4. 變數代換 / 改名

這幾個函數是 5 個 gate 實作的核心。

### `bdd_compose(BDD f, BDD g, int var)` → `BDD`
**已使用**(`apply_X`、`apply_Z`;內部命名為 `substitute()`)

把 `f` 裡所有標記為 `var` 的節點,替換成布林函數 `g`:結果是 `f[g/var]`。
單次走訪、靠 operator cache 記憶化,`O(|f|)`。適合**替換單一變數**的情況;
若要同時替換多個變數,官方文件建議改用 `bdd_veccompose`(見下)。

### `bdd_veccompose(BDD f, bddPair *pair)` → `BDD`
**已使用**(`apply_CX`、`apply_CZ`)

用 `pair` 裡的「變數 → BDD」對照表,**同時**做多個替換:
`f[g_1/V_1, ..., g_n/V_n]`。官方文件明講:當替換目標是單一變數以外的情況
(多個變數、或 `g_i` 不是單一變數而是複雜運算式),`bdd_veccompose` 比連續呼叫
多次 `bdd_compose` 有效率,因為只需要走訪 `f` 一次。這個專案裡 CX、CZ 都要
同時替換兩個變數(各自換成一個 XOR 運算式),所以用這個而不是連續兩次
`bdd_compose`。

**同時替換 ≠ 依序替換**:官方文件給的例子是
`(x1∨x2)[x3/x1, x4/x3] = (x3∨x2)`,跟先做 `[x3/x1]` 再做 `[x4/x3]` 得到的
`(x4∨x2)` 不一樣。這個專案裡 CX/CZ 剛好每次替換用到的變數(替換目標 vs.
替換式裡引用的變數)彼此不重疊,所以同時做跟依序做結果相同(已用
`test/gates_test.cpp` 的具體數值驗證過),但既然有現成的「同時替換」原語,
沒有理由不用它。

### `bdd_replace(BDD r, bddPair *pair)` → `BDD`
**已使用**(`apply_H`)

用 `pair` 定義的「舊變數 → 新變數」對照表,把 `r` 裡的變數**改名**。這是
`bdd_compose`/`bdd_veccompose` 的特化版本,專門處理「替換目標也是單一變數」
(不是任意布林函數)的情況,效率比用 `bdd_compose` 做同樣的事更好——官方文件
在 `bdd_veccompose` 的說明裡特別註明這點。H gate 的 `x_q↔z_q` 對調就是純改名,
所以用這個而不是 `bdd_compose`。

### `bddPair`(型態,不是函數)
**已使用**

```c
typedef struct s_bddPair {
   BDD *result;              // 每個舊變數對應到的新 BDD/變數
   int last;
   int id;
   struct s_bddPair *next;
} bddPair;
```

`bdd_replace`/`bdd_compose`/`bdd_veccompose` 用來描述「變數代換規則」的不透明
資料結構(opaque handle)。使用流程固定是:`bdd_newpair()` 建立空表 →
`bdd_setpair()`/`bdd_setbddpair()` 填規則 → 丟給 `bdd_replace`/`bdd_veccompose`
→ `bdd_freepair()` 釋放。不需要、也不應該直接碰內部欄位。

### `bdd_newpair()` → `bddPair*`
**已使用**(`apply_H`、`apply_CX`、`apply_CZ`)

配置一個空的 `bddPair` 表。

### `bdd_setpair(bddPair *pair, int oldvar, int newvar)` → `int`
**已使用**(`apply_H`)

在 `pair` 裡加入一條規則:`oldvar` 要被替換成**變數** `newvar`(給
`bdd_replace` 用的「純改名」版本)。成功回傳 0。

### `bdd_setbddpair(bddPair *pair, int oldvar, BDD newvar)` → `int`
**已使用**(`apply_CX`、`apply_CZ`)

跟 `bdd_setpair`的差別:`newvar` 這裡可以是**任意 BDD**(不只是單一變數),
專門給 `bdd_compose`/`bdd_veccompose` 用——`bdd_replace` 只會取這個 BDD
最頂端的變數,不會真的代入整個運算式。CX/CZ 的 `x_t' = x_t ⊕ x_c` 這種 XOR
運算式就是靠這個塞進 pair 表的。

### `bdd_setpairs(bddPair *pair, int *oldvar, int *newvar, int size)` → `int`
**未使用,備用**

`bdd_setpair` 的陣列版本,一次設定 `size` 條「變數→變數」規則。如果之後要
一次操作很多 qubit(例如「把所有 qubit 的 X/Z 互換」),比逐一呼叫
`bdd_setpair` 方便。

---

## 5. 量化 / relational product

### `bdd_exist(BDD r, BDD var)` → `BDD`
**已使用**(`multiply_by_all_paulis_on`)

對 `r` 做存在量化(existential quantification),把 `var`(一個「變數集合」,
用多個變數的 conjunction 表示,例如 `bdd_ithvar(1) & bdd_ithvar(3)`)裡列出的
所有變數消掉。單次遞迴走訪 + quantification cache,不會把各個 cofactor 實體化
出來再兩兩 OR。

本專案用它來實作「把某些 qubit 上的**全部** Pauli 都乘上去」:那些 Pauli 作為
GF(2) 向量構成一個座標對齊的子空間,乘上整個子空間 = 各 coset 的 union =
把對應座標存在量化掉。所以是 1 個運算,不是 `4^k` 次相乘再 union。

### `bdd_makeset(int *v, int n)` → `BDD`
**已使用**(`multiply_by_all_paulis_on`,建 `bdd_exist` 要的 varset)

從整數陣列 `v`(恰好 `n` 個元素)讀進一組變數編號,建出代表這個「變數集合」的
BDD。變數集合的表示法就是所有變數**正 literal 的 conjunction**,所以也可以自己
用 `&` 串出來;官方文件建議把回傳值存起來重複使用,不要每次都重建。

C++ 版實際名稱是 `bdd_makesetpp(int*, int)`(回傳 `bdd`),`bdd.h` 裡用
`#define bdd_makeset bdd_makesetpp` 轉過去,所以原始碼寫 `bdd_makeset` 即可。
注意參數是**非 const** `int*`,傳 `std::vector<int>` 時用 `vec.data()`。

---

### 以下幾個目前沒用到,備用

handoff 原本設想 CX/CZ 要用 relational product 這條路,實際上因為 CX/CZ 是
對合,改用 `bdd_veccompose` 就夠了(見上)。但如果以後要做「非雙射」的一般
線性變換——例如乘上的不是完整子群而是某個**子集**(weight ≤ 1 的 error model
之類),存在量化的捷徑就不成立了——這幾個是標準工具:

### `bdd_appex(BDD left, BDD right, int opr, BDD var)` → `BDD`
**未使用,備用**

先對 `left`、`right` 做 `opr` 運算,再對結果做 `bdd_exist(_, var)`——但是用
「bottom-up 邊算邊量化」的方式一次做完,比「先 apply 再 exist」分開做效率高
很多。如果 `opr` 是 `bddop_and`,這就是標準的 relational product(常用在
symbolic model checking 套用 transition relation)。

### `bdd_relprod(a, b, var)`(巨集,展開成 `bdd_appex(a, b, bddop_and, var)`)
**未使用,備用**

`bdd_appex` 在「and + exist」這個最常見組合下的簡寫。

---

## 6. 其他運算子(目前沒用到,備用)

### `bdd_ite(BDD f, BDD g, BDD h)` → `BDD`
**未使用,備用**(舊版 `flip_var` 草稿曾用過,後來改用 `bdd_compose` 取代)

計算 `(f ∧ g) ∨ (¬f ∧ h)`,比分開做三個運算有效率。也可以拿來做任何二元布林
運算,但那樣不如 `bdd_apply` 有效率。

### `bdd_restrict(BDD r, BDD var)` → `BDD`
**未使用,備用**(舊版 `flip_var` 草稿曾用過,後來改用 `bdd_compose` 取代)

把 `r` 裡 `var`(一個「變數集合」,用正/負 literal 的 conjunction 描述,例如
`bdd_ithvar(1) & bdd_nithvar(3)` 代表「變數1限制為 true、變數3限制為
false」)列出的變數限制成常數。官方文件也提到:如果 `g_i` 是單一變數,直接用
`bdd_compose` 比 `bdd_restrict`(組合兩次 restrict)更有效率,這正是這個專案
`apply_X`/`apply_Z` 選 `bdd_compose` 而不是 `bdd_restrict` 的原因。

---

## 7. Reorder / 變數順序

### `bdd_varblockall()` → `void`
**已使用**(`PauliSetBDD::init()`)

幫目前已宣告的每個變數各自建一個 reorder block(每個 block 只包含一個變數),
也就是不對任何變數強加相鄰限制,讓 sifting 演算法自由排列。

### `bdd_autoreorder(int method)` → `int`
**已使用**(`PauliSetBDD::init()`,傳入 `BDD_REORDER_SIFT`)

啟用自動 reorder,每次 node table 使用中的節點數翻倍時觸發。回傳舊的
method 設定值。

### `bdd_reorder(int method)` → `void`
**已使用**(`test/reorder_test.cpp`,手動觸發一次)

立刻執行一次 reorder,`method` 常見選項:`BDD_REORDER_SIFT`(每個 block
移到所有可能位置、取最好的一個,較慢但效果好)、`BDD_REORDER_WIN2`/
`WIN2ITE`/`WIN3`(相鄰 block 兩兩比較交換,較快)、`BDD_REORDER_RANDOM`
(除錯用)。

### `bdd_var(BDD r)` → `int`
**已使用**(`PauliSetBDD::for_each` 走 DAG 時判斷節點標記哪個變數)

回傳 BDD 節點 `r` 標記的變數編號(**變數 identity**,不受 reorder 影響)。
只能用在非終端節點上。

### `bdd_low(BDD r)` / `bdd_high(BDD r)` → `BDD`
**已使用**(`PauliSetBDD::for_each`)

取節點 `r` 的 0-分支 / 1-分支子圖。搭配 `bdd_var` 就可以自己走訪整個 DAG。

### `bdd_var2level(int var)` → `int`
**已使用**(`test/reorder_test.cpp`,用來印出 reorder 前後的 level 順序)

回傳變數 `var` 目前在內部順序中的 level(**會被 reorder 改變**)。這個專案的
關鍵設計就是:所有**語意**邏輯只依賴 `bdd_var()`(identity),不查 level,
所以 reorder 何時發生都不影響正確性。

### `bdd_level2var(int level)` → `int`
**已使用**(`PauliSetBDD::for_each`)

`bdd_var2level` 的反向:回傳目前排在第 `level` 層的變數編號。走訪 DAG 要**逐層**
往下時需要它——因為節點可能跳過某些變數(那就是 don't-care),不能假設「第 k 層
的節點一定標著變數 k」。

### `bdd_setvarorder(int *neworder)` → `void`
**未使用,備用**

把目前的變數順序直接設成 `neworder` 指定的排列。陣列**必須包含目前宣告的所有
變數**,順序就是新的 level 順序(例如 `[1,0,2]` 代表 `v1 < v0 < v2`)。
如果之後要強制某些變數排在頂層(例如把 syndrome 變數拉上來),用這個;但記得
同時 `bdd_autoreorder(BDD_REORDER_NONE)`,否則後續運算觸發的 sifting 會把它洗掉。

### `bdd_extvarnum(int num)` → `int`
**未使用,備用**

在現有基礎上再**增加** `num` 個變數(`bdd_setvarnum` 是設定總數,這個是增量)。
新變數會被附加在順序的**最底層**。

> **變數只能增加,不能刪除。** BuDDy 沒有刪除變數的操作。BDD 世界裡「刪掉一個
> 變數」的正確做法是把它**量化掉**(`bdd_exist`,見第 5 節);變數槽位會留著但
> 沒有任何 BDD 依賴它,成本只是一個閒置 slot。
>
> **變數順序是全域的。** `bddvar2level` / `bddlevel2var` 在 `kernel.c` 裡是單一組
> 全域陣列,所有 BDD 共用一份順序——不可能讓不同 BDD 各有各的順序。這是 shared
> BDD package 的根本前提:正因為所有節點遵守同一順序,canonical form 才成立,
> `f == g` 才能是 O(1) 指標比較。

---

## 8. 查詢 / 資訊

### `bdd_satcount(BDD r)` → `double`
**已使用**(`PauliSetBDD::size()`)

計算有幾種變數指派能讓 `r` 為真(所有已宣告的變數都算)。回傳 `double`
是因為指派數量可能超過 `int`/`long` 範圍(`4^n` 對大 `n` 會爆)。

### `bdd_nodecount(BDD r)` → `int`
**已使用**(`PauliSetBDD::node_count()`)

走訪 `r`,計算 BDD 用了幾個不同節點(diagram size)。

### `bdd_printset(BDD r)` → `void`
**已使用**(`PauliSetBDD::print_raw()`)

把 `r` 所有會讓它為真的變數指派印出來,格式是 `< 變數編號:0/1, ... >` 的列表
(用的是原始變數編號,不是 Pauli 字元,純除錯用)。**注意是壓縮格式**:沒被提到的
變數代表 don't-care,所以一筆輸出可能代表 `2^k` 個元素。

---

## 9. 列舉滿足指派(注意:沒有「逐一列出元素」的內建函數)

### `bdd_allsat(BDD r, bddallsathandler handler)` → `void`
**未使用,備用**(`PauliSetBDD::for_each` 沒有用它,原因見下)

對 `r` 的所有合法指派逐一呼叫 callback。callback 收到一個 `char*` 陣列,長度等於
全域變數數,每格是 `0`(假)、`1`(真)或 `-1`(**don't care**)。

**兩個實務上的坑**:

1. **它列舉的是 cube,不是單一元素。** 一次 callback 可能代表 `2^k` 個指派。實測:
   一個 16 元素的集合(2 qubit 全部 Pauli)只觸發**一次** callback,內容是 `----`。
   要拿到一個一個的元素,得自己把 don't-care 展開。
2. **callback 是裸函數指標**(`void (*)(char*, int)`),沒有 user-data 參數,所以
   **不能傳有捕獲的 lambda**,累積結果只能靠 file-scope/static 變數。

因為這兩點,本專案改用自己寫的 `PauliSetBDD::for_each`——直接用
`bdd_level2var`/`bdd_var`/`bdd_low`/`bdd_high` 逐 level 遞迴走訪,把被跳過的變數
(就是 don't-care)展開成兩個分支,每次 callback 剛好是一個具體的 Pauli 字串,
而且吃 `std::function` 可以捕獲外部狀態、回傳 `false` 可以提早停。

### `bdd_satone(BDD r)` → `BDD`
**未使用,備用**

回傳一個「每層最多一個變數」的 BDD,它蘊含 `r`,而且只要 `r` 不是 false 就不會是
false。也就是**找出一個**滿足指派(可能仍含 don't-care)。

### `bdd_fullsatone(BDD r)` → `BDD`
**未使用,備用**

跟上面類似但**每一層都恰好有一個變數**,也就是回傳一個完整指定的指派(沒有
don't-care)。要「隨便拿一個元素」時用這個比 `bdd_satone` 直接。

### `bdd_satoneset(BDD r, BDD var, BDD pol)` → `BDD`
**未使用,備用**

找出 `r` 裡的一個 minterm,並保證 `var` 這個變數集合裡的變數都會出現在結果中;
若這些變數在 `r` 裡本來是 don't-care,它們的極性由 `pol` 決定(`pol` 是 false BDD
就取負形,否則取正形)。

---

## 附註:C++ overload 在哪裡

以上函數在 `buddy_src/src/bdd.h` 都有對應的 C++ inline wrapper(接受
`const bdd&`、回傳 `bdd`,而不是底層 `BDD`/`int`),簽章統一在 `bdd.h` 裡用
`friend bdd bdd_xxx(...)` 宣告。這個專案全程只透過這層 C++ 介面呼叫,沒有
直接碰過底層 C API 或手動 `bdd_addref`/`bdd_delref`。
