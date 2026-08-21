# 時間型別與曆法契約

> 承接 [outline.md](outline.md)（全局常數：秒、stride 表、360 天曆）與
> [cpp-conventions.md](cpp-conventions.md)（型別約定）。
> 本文件只回答一件事：**時間在 core 裡是什麼型別、什麼運算合法、什麼值合法。**
> 通用原則見 [principles.md](principles.md)；用詞以 [glossary.md](glossary.md) 為準。

常數（1 旬 = 864,000 秒、1 年 = 36 旬 = 31,104,000 秒、各層 stride）**只定義在 outline.md**，
本檔引用而不複述。

## 兩個型別，不是一個

M0 只有一個 `Tick`，`kSecondsPerXun` 也被宣告成 `Tick`。這是**錯的**，
因為它讓「時刻 + 時刻」在型別上合法。

```cpp
enum class Tick     : std::int64_t {};   // 時刻：全局時間軸上的一個點
enum class Duration : std::int64_t {};   // 時距：兩個時刻之間的秒數
```

兩者都是秒，都是 `int64_t`，但**語意不同，不可互換**。這是 `chrono` 的
`time_point`／`duration` 之分，只是我們自己實作以避開 `chrono` 的模板負擔。

### 合法運算（只有這些）

| 運算 | 結果 | 意義 |
|---|---|---|
| `Tick + Duration`、`Duration + Tick` | `Tick` | 推進時鐘 |
| `Tick - Duration` | `Tick` | 回推 |
| `Tick - Tick` | `Duration` | 兩個時刻相隔多久 |
| `Duration ± Duration` | `Duration` | 時距加減 |
| `Duration * int64`、`int64 * Duration` | `Duration` | n 個回合 |
| `Duration / Duration` | `int64` | 「幾個 stride」 |
| `Duration % Duration` | `Duration` | 距離下一個邊界 |
| `Tick <=> Tick`、`Duration <=> Duration` | 比較 | |

**刻意不提供**：`Tick + Tick`、`Tick * n`、`Tick / n`。
時刻相加沒有意義，能寫出來就會有人寫。

### stride 常數一律是 Duration

outline.md 的 stride 表在 core 裡的對應常數全部是 `Duration`，
名稱不再帶 `kSecondsPer` 前綴（型別已經說明單位）：
`kXun`、`kYear`、`kHour`、`kMinute`、`kSiteCombatTurn`、`kLocalCombatTurn`。

回合推進因此永遠寫成 `now = now + stride`，`stride` 是 `Duration`。
**不准出現裸 `int` 的「回合數」**——理由見 [cpp-conventions.md](cpp-conventions.md)：
stride 是可變的，把回合數當時間會在交戰時全面錯亂。

## 合法域與 fail-fast

`CalendarDate::year` 是 `int32_t`，`Tick` 是 `int64_t` 秒。
`int64_t` 秒可表示約 2.9×10¹¹ 年，遠超過 `int32_t` 年能表達的範圍——
**兩者的域不相等，這個落差必須明文處理，不能靠靜默截斷。**

M0 的實作直接 `static_cast<int32_t>`，實測會給出**看起來合理的錯答案**：

```
tick=9223372036854775807 -> year=180565375 season=1 month=1 xun=3
```

### 裁定：縮小 Tick 的契約，不放大 year

`int32_t` 年 ≈ ±2.1×10⁹ 年，任何遊戲（含世界生成的歷史層）都不可能用到。
落到域外只有兩種原因：**存檔損壞，或程式有 bug**。那是不變式被破壞，
不是「預期內的失敗」——依 [cpp-conventions.md](cpp-conventions.md) 的錯誤處理約定，
該 fail-fast，不該回 `expected`，更不該回一個錯日期。

而 `int32_t` 年的**完整範圍換算成秒仍在 `int64_t` 內**
（2.1×10⁹ × 31,104,000 ≈ 6.7×10¹⁶ ≪ 9.2×10¹⁸），
所以合法域可以定得剛剛好，不必憑感覺砍一個數字：

```cpp
inline constexpr Tick kMinTick{/* year = INT32_MIN 的第一秒 */};
inline constexpr Tick kMaxTick{/* year = INT32_MAX 的最後一秒 */};

[[nodiscard]] constexpr bool is_representable(Tick) noexcept;
```

`to_date()` 進入時 `AETH_CHECK(is_representable(tick))`。
`to_tick()` 進入時檢查 `season`／`month`／`xun` 的 1-based 範圍。

### AETH_CHECK

[cpp-conventions.md](cpp-conventions.md) 早就要求「不變式用 `assert`，
**release 版保留關鍵的幾條（用自訂 `AETH_CHECK`）**」，但它一直沒被建立。
時間換算就是「關鍵的幾條」之一——它的錯誤會靜默污染整個世界狀態。

契約：

- **所有建置組態都生效**，包含 Release。它不是 `assert`，不吃 `NDEBUG`。
- 失敗即 `abort`，訊息含檔名、行號、被違反的條件。
- **不丟例外**——它要能跨 bridge 邊界，也要在 headless 與 Godot 進程裡行為一致。
- **不改變任何狀態**，因此不影響決定論：它只有「繼續」與「死」兩種結果。

放在 `core/base/check.h`。

## 曆法的精度界線

`CalendarDate` 是**旬精度**的日期，不是完整時間戳：

- `to_date(tick)` 丟棄旬內的秒數。
- `to_tick(date)` 回傳**該旬第一秒**。
- 因此 `date → tick → date` 無損，但 `tick → date → tick` **有損**。這是設計，不是 bug。

L1 Region 的回合就是一旬，M1 夠用。但 **L2 是小時、L3 是分鐘**
（見 [outline.md](outline.md) 的 stride 表），到 M2 就需要顯示時刻。

**現在不擴充 `CalendarDate`。** 時刻另立 `TimeOfDay`，等 M2 真的要顯示時再定形狀——
現在硬定很可能定錯。要守的是**別把 `CalendarDate` 當成完整時間戳用**，
任何需要旬內精度的地方直接用 `Tick`。

## ⚠ 旬內小時不是 Site 的狀態

`CityEconomy` 目前存了一個 `hours_into_xun`。**那是全局時鐘的第二份拷貝**，
與本檔末尾「全局時鐘唯一」直接牴觸。

**沒有人是故意加的，而它在單 Site 時代完全看不出問題**——只有一份拷貝，
它跟時鐘永遠一致。是 M5-pre 讓兩個 `L_FULL` Site 必須一起推進，
兩份拷貝可能不一致，這件事才第一次有形狀。

M5-pre 的處置是**在批次入口硬性拒絕旬內小時不同步的 Site**。
這是對的**過渡**行為（大聲拒絕勝過無聲算錯），但它把病徵當成了不變式：
那條檢查真正在說的是「這 N 份時鐘拷貝必須彼此同意」，
而它們之所以可能不同意，只因為時鐘被拷貝了 N 份。

> **裁定（M5.1 落地）：旬內小時由全局時鐘導出，不存在 Site 上。**
> 串流中途載入的 Site 去**讀**時鐘，而不是自帶一份。

代價要講清楚：`hours_into_xun` 在 `CityEconomy::serialize` 裡，拿掉就是改位元流，要升版。
所以這條排在 M5.1 一起做，不單獨為它升一次版。

### ⚠ 這條只適用一個欄位，不要擴大解釋

全庫掃過，帶時間語意的持久欄位分三類，**只有第三類是缺陷**：

| 類別 | 例子 | 判斷 |
|---|---|---|
| **時距**（倒數或累計） | `remaining_hours`、`aging_seconds`、`elapsed_seconds` | ✅ 本來就是各物件自己的量 |
| **時戳**（某事發生於何時） | `unload_tick`、`last_saved_tick` | ✅ 是紀錄，不是時鐘 |
| **時鐘讀數**（現在位於週期何處） | `hours_into_xun` | ❌ 唯一一個 |

分界是：**「這個值會隨時間自己前進嗎？」** 會的才是時鐘拷貝。
時距靠自己遞減、時戳寫下就不動，兩者都不會與全局時鐘失去同步。

**這是 M5 作為「回頭檢驗」的第一個戰果**（見 [lowmap.md](lowmap.md) 末段的論點）：
缺陷不在新寫的程式碼裡，在四個里程碑以前就長好了，只是一直沒有情境能照出它。

## 年份編號

**天文編號（proleptic）**：`…, -2, -1, 0, 1, 2, …`，**有第 0 年**，沒有「0 年不存在」的例外。
`Tick{0}` = 第 1 年第 1 季第 1 月第 1 旬的第一秒；日期欄位全部 1-based。
負值換算用 floor division，不用 C++ 的截斷除法。

理由與 360 天曆同源（[outline.md](outline.md)）：**避免模數例外**。
歷史層要往前推演多少年就推多少年，不會在 0 年附近撞到特例分支。

## 待細化

- `TimeOfDay` 的形狀（M2）。
- 季節對農產／行軍成本／海路的具體影響曲線——屬規則數值，不屬本檔。
- 跨時區／多世界時鐘：**不會有**。全局時鐘唯一（[outline.md](outline.md)）。
