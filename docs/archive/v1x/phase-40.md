# Phase 40 细节存档

> 用户确认方向（原话）："按你的建议继续下一阶段"。
> 范围 = P28 遗留 KNOWN_ISSUES「熟肉（无烹饪系统）」四项里唯一可一次落地的
> 系统核心（面包需种植系统、金苹果需苹果来源、牛奶桶为独立小项，留后续
> 候选）；顺带消费 P12 遗留「coal 无用途」（燃料）。

## 阶段报告

- **`[2026-10-11, Phase 40]` 炉子/熔炼** — 全数据驱动的烹饪系统：
  3 种熟肉 + 燃料（coal/planks/stick）+ `furnace` 方块 + 单全局炉子状态 +
  炉子面板 UI + 存档 format 6。

  **数据驱动字段（`item_registry` 新 `parseFurnace`，whitelist +3 键）**
  - `smeltable_to`：产物物品名；非空 = 可熔炼；**init 交叉校验产物存在**
    （ammo 模式）；显式空串拒绝（ABSENT 是缺省 = ammo 纪律）。
  - `smelt_time`：(0,100] s，缺省 10.0 = MC 炉子时长；显式 0 拒绝。
  - `fuel_time`：(0,1000] s，缺省 0 = 非燃料；显式 0 拒绝（=
    restore_hunger 纪律）。
  - 数据：`cooked_beef` +8 / `cooked_mutton` +6 / `cooked_chicken` +6
    （MC 数值，32×32 图标）；`beef`/`mutton`/`chicken` 加
    `smeltable_to`；燃料 `coal` 80 s / `planks` 7.5 s / `stick` 5 s（MC
    数值）。wood 是隐式 block 物品（无数据文件）故不可燃料。

  **`furnace` 方块**：`interact: "furnace"`（交互集新已知值
  `kInteractFurnace`，registry 校验；破坏掉自身）；纹理
  `furnace_top`/`furnace_side` 落 atlas row 7 两个空位（4×8 不扩容，不扰
  既有 UV）。

  **单个全局 `FurnaceState`**（`item/furnace.{hpp,cpp}`，工作台网格同款
  v1 规则，per-block 容器留后续；KNOWN_ISSUES 记录）：input/fuel/output
  三 `ItemStack` + `progressSubsteps`/`fuelSubsteps` **整数字步倒计时** +
  活状态 `progressFor`（**不入库**——不变量 "progress>0 ⟹
  progressFor == input.item 且可熔炼" 由 tick 与 UI 双端维持，读档
  `deriveProgressFor()` 恢复）。`smeltSubstepsFor`/`fuelSubstepsFor` =
  `round(t×60)`（发行数据 10.0/80.0/7.5/5.0 精确）。

  **tick（`tickFurnace`，每帧固定 1/60 子步、紧跟 `m_dayCycle.update`，
  纯函数 over (state, items)）**，MC 规则削减：
  - 熄灭 tick 内 fuel 槽有燃料 = 立即消费一根并授予**整段**燃烧时间（该
    tick 同时推进一格进度 → 每根燃料实际多烧 1/60 s，确定性）；
    燃尽当 tick 立即点下一根。非燃料物品永不消费。
  - 推进条件 = 烧着 ∧ 输入可熔炼 ∧ output 空或同产物未满；进度满 =
    输入 −1 / output +1 / 进度清零。
  - output 被异物品占着 = 暂停但**火继续烧**（MC 同款）。
  - 输入物品变更（含取空/换非熔炼物）= 进度清零；**同物品替换不清零**
    （ProgressKeptWhenSameItemReplaced 钉住）。

  **UI**：`FurnaceLayout`（27 格同 Inventory + 专用 rect：input
  {812,326} / output {928,326} / fuel {812,382} / 进度条
  {868,346,56,12}；panel {288,246,704,228} @1280×720；gridW =
  9×52+8×4 = **500** 非 492）+ `FurnaceUI`（单元格 pick-and-place 语义
  抽成共享 free fn `cellLeftClick`/`cellRightClick`，InventoryUI 委托之）
  + `OpenPanel::Furnace`；`openPanel` 释放指针（同工作台）。

  **存档 format 6**：`"furnace"` 对象 = 3 槽位条目
  （`{"item","count"}`，空 = `{"item":"","count":0}`）+
  `progress_substeps`（≤6000）/`fuel_substeps`（≤60000）整数 + **进度
  不变量**（progress>0 要求输入可熔炼，违反 = 拒绝）。严格校验：未知
  字段/缺槽/负/越界/非整数全拒绝；format 1-5 读档 = 空炉；旧格式带
  `furnace` 字段 = 拒绝（迁移防护，同 P39 mob 名单先例）。

  **测试（+33，751/751，`-Wall -Wextra -Wpedantic -Werror` 0 警告）**
  - `tests/test_furnace.cpp`（新，23）：tick 语义 12【点火/非燃料/换燃料/
    无燃料/满熔炼/第二格/输出满暂停/异物品堵输出/进度清零×2/进度保留/
    空炉 no-op】+ 子步换算 + 布局钉值 2 + UI 8【取放/交换/合并/移 1/异物
    no-op/进度重置×2/点外丢堆】。
  - `tests/test_save.cpp`（+6）：round-trip ×2 / format5 读空炉 / 旧格式
    带 furnace 拒绝 / 进度不变量拒绝 / 坏字段 6 种；既有 5 个旧格式测试
    补 `withoutKey(root,"furnace")` + format 6→7 未来版本断言。
  - `tests/test_item.cpp`（+3）：解析 / 缺省 / 坏字段（显式空 smeltable_to
    、非字符串、smelt_time 0/−3/100.5/字符串）。
  - `tests/test_block.cpp`（+1）：`InteractParsesFurnace`。
  - 测试宏坑：`EXPECT_EQ(a, ItemStack{x, y})` 花括号内逗号被宏参数拆分 →
    `stk(id,n)` helper（PITS 已录）。

  **E2E（`/tmp/opencode/p40_e2e.zsh`，单批）**
  - **S1 base18 门**：P40 ×2 + base18，60f 无输入截图三方字节一致
    （**第 15 次结构性免疫**——新世界无炉子方块（非 worldgen 产出）、
    空炉 tick = no-op、无输入无面板、atlas 新 tile 落空位）。
  - **S2 回归 A/B（p27far seed 9 夜 360f）**：P40 ×2 **存档字节一致
    （确定性权威）** + 截图仅 real-dt 红纱边界（skeleton 攻击 0.5 s
    边界红闪：100% 像素 ≤1/通道、通道和 [845410, 27, 76190] 红 91.7%
    主导 = P27/P38/P39 同签名）+ P39 构建同场景截图纱容差一致（format
    升格后跨构建存档 A/B 不可能——P39 构建严格拒绝 fmt>5，PITS 新坑；
    用上一阶段 E2E 截图 `/tmp/opencode/p39_s2p39.png` 作参考，场景无
    炉子 = 休眠代码可比）。出生序列 f60 cow / f90 sk / f120 cow / f150
    sk、cap 4、health 8 = P39 记录精确一致。
  - **S3 熔炼场景**（format 6 存档预载 beef×3 + coal×1，正午，1500f =
    25 s 无输入）：炉状态与 python 钉值**精确一致**（input beef×1 /
    output cooked_beef×2 / fuel 空 / progress 300 / fuelSubsteps
    3301 = 4800−1499——点火 tick 白烧一格）+ 存档 ×2 字节一致（确定性）
    + health 20 / hunger 20（25 s < 36 s 衰减 tick）。
  - **S4 输入运行**（炉子方块 (22,45,−2) 平坦高原前 + 瞄准 pitch
    atan(−1.12/1.5) = −0.64136【世界生成 probe 定地形后手算：眼
    (22.5,46.62,0.5) → 面中心 (22.5,45.5,−1)】+ **选中格 coal 而非
    食物**【右键顺序 tryEat→launcher→interact，手持食物右键会先被吃 =
    PITS 新坑】）：锁指针 → 右键开炉面板（释放指针）→ 左键取 slot1
    牛肉 → 左键放 input 槽：存档**精确**（input beef×3 / slot1 空 /
    coal 未动 / 计数 0/0 / yaw·pitch 零漂移 / hunger 20 无进食）+ 截图
    vs 无输入对照：面板区内 100% 像素差、区外 0.00%（y≥600 HUD 条
    排除）。

  **PITS +3**
  - `EXPECT_EQ(a, ItemStack{x, y})`：brace-init 内逗号被宏参数拆分
    （"no matching function"）→ 栈值走 `stk(id,n)` helper。
  - 右键顺序 tryEat() → launcher → interact（game.cpp 既有）：手持食物
    右键永远开不了面板——E2E/交互脚本的选中格放非食物物品。
  - 存档 format 升格后跨构建 A/B 不可能：旧构建严格拒绝新 format
    （P39 构建对 fmt 6 FATAL）——回归参考改用**上一阶段 E2E 截图**
    （场景内无新系统 = 休眠代码可比，论证同 S1 结构性免疫）。

  **变更文件**：`src/item/furnace.{hpp,cpp}`（新）、`src/item/item.hpp`、
  `src/item/item_registry.{hpp,cpp}`、`src/block/block.hpp`、
  `src/block/block_registry.cpp`、`src/core/save.{hpp,cpp}`、
  `src/core/game.{hpp,cpp}`、`src/ui/inventory_ui.{hpp,cpp}`、
  `CMakeLists.txt`、`tests/test_furnace.cpp`（新）、`tests/test_save.cpp`、
  `tests/test_item.cpp`、`tests/test_block.cpp`、`assets/data/blocks/
  furnace.json`（新）、`assets/data/items/{beef,mutton,chicken,coal,
  planks,stick}.json` + 3 熟肉 json（新）、`assets/data/textures/
  block_atlas.json`、`assets/textures/blocks/furnace_{top,side}.png`
  （新）、`assets/textures/items/cooked_{beef,mutton,chicken}.png`
  （新）、`tools/generate_{block_textures,item_icons}.py`、文档全套
  （ROADMAP / KNOWN_ISSUES / MECHANICS / CONTENT_GUIDE / PITS +3 /
  AGENTS / 本存档）。

## 会话存档

- **范围裁剪**：P28 遗留"熟肉"四项（面包/金苹果/牛奶桶/烹饪系统）里
  只有烹饪系统不依赖新子系统（种植/苹果来源/桶）→ 一次落地；其余留
  后续候选。燃料取 coal 80 s / planks 7.5 s / stick 5 s（MC 数值）顺带
  消费 P12 遗留"coal 无用途"。
- **单全局炉子的 v1 决策**：工作台网格（P17）同款"全游戏一个容器状态"
  规则——每炉子块独立状态需要 per-block 存储（world 额外 map + 存档
  per-chunk 序列化 + 读档定位），复杂度不成比例；v1 全局 + KNOWN_ISSUES
  记录，与 MC 差异明确文档化。
- **整数字步倒计时**：`round(t×60)` 对发行数据（10.0/80.0/7.5/5.0）
  精确，无浮点累加漂移；tick 规则里"点火 tick 授予整段 + 同 tick 推进
  1 格"使每根燃料多烧 1/60 s——钉值测试（FullSmeltCompletes：599 tick
  progress 599、600 完成、fuel 4800−599）与 E2E 钉值（3301 =
  4800−1499）同源于此，确定性不依赖墙钟。
- **`progressFor` 不入库**：活状态由双端不变量保证，读档 derive；存档
  侧把"progress>0 但输入非熔炼"做成**拒绝**（不只是归零）——损坏/篡改
  存档应显式失败（house 纪律）。
- **S4 瞄准手算**：炉子块 (22,45,−2) 面中心 (22.5,45.5,−1)，眼
  (22.5,46.62,0.5)（出生 y45 + 眼高 1.12 上无遮挡，probe 确认 (22,49-
  50,3) 树叶不在视线内）→ pitch = atan(−1.12/1.5) = −0.6413643599084533
  直接写进存档。点击坐标 = layout rect 中心 × ui.scale(1)：slot0
  (326,436) / slot1 (382,436) / 炉 input (838,352)。
- **批脚本事故（1 起，已修）**：S2 首跑用严格字节比较器 `cmpf` 比较 P40
  ×2 截图——该场景 skeleton 打玩家（health 8），0.5 s real-dt 红纱边界
  使截图非字节一致（签名 [845410,27,76190] 与 P39 记录逐数相同）。
  修正 = 存档 ×2 为确定性权威（同构建 format 6 可比）+ 截图走
  `veilcmp`。教训：有 Mob 攻击玩家的黑夜场景，截图断言恒用纱容差，
  字节断言只留给存档/无伤场景。
- **地形 probe（throwaway 测试，已删）**：p27far 世界 (22.5,45,0.5)
  四周 = 平坦草原（地表 y44、y≥45 空气）；S4 炉子方块直接 setBlock 进
  派生存档（`p40_s4src.json`），场景源 `p27far.json` 0 block edits。
