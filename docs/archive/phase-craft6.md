# CRAFT-6 燃料熔炼（v1.x 矩阵 24/33，v1.24.0，门 139，CRAFT 行 3/4）

## 定案

原始定义已佚 = 定案 **block 物品的炉子字段（wood 燃料 + sand→glass
熔炼 + lava_bucket 燃料）**（消费 KNOWN_ISSUES 炉子节条「可熔炼/燃料
名单小且 wood 不可当燃料」）：

- **技术路线 = block 属性继承**（非显式物品文件）：`buildBlockItems`
  合成 block 物品且**显式同名物品文件被注册表拒绝**（"block items are
  defined by their block"）= 物品侧无文件可挂字段 → 新增 block 可选属性
  `fuel_time`（number (0,1000]，显式 0 = 点名拒，item 侧同款纪律）+
  `smeltable_to`（string 非空，显式空串 = ammo 纪律点名拒；产物存在性
  交叉校验 = 既有 game init 全物品遍历自动覆盖 block 物品，零新校验
  代码）→ `buildBlockItems` 两字段继承进合成 item。**零新资产**（wood/
  sand/glass/lava_bucket 全既有；物品图标 = 块纹理既有路径）。
- **内容（3 项，MC 语义）**：
  - `wood` 块 + `fuel_time: 20`（项目木族刻度：planks 7.5 / stick 5，
    coal 80 = MC 精确锚；20 s ≈ 2.7× 单 planks = MC log-vs-plank 比值
    方向一致）
  - `sand` 块 + `smeltable_to: "glass"`（MC 10 s 缺省 = 不写
    smelt_time）
  - `lava_bucket` 物品 + `fuel_time: 1000`（MC 精确值 = 区间上限）
- **炉子 tick 零改动**（`fuelSubstepsFor`/`smeltSubstepsFor` 读 item
  字段，从不读 block id = 硬规则 3；"非燃料（burn 0）永不消耗"既有
  语义 = OLD 侧 wood 恒保留的行为根）。
- **零 format（21）/零 config/零新资产/零音效**。

## 实现

- `src/block/block.hpp`：`BlockProperties` + `fuelTime`（0.0 缺省）/
  `smeltableTo`（空缺省）2 字段。
- `src/block/block_registry.cpp`：`parseProperties` + 2 checkField +
  fuel_time 显式 0/(0,1000] 区间点名 + smeltable_to 显式空串点名。
- `src/item/item_registry.cpp`：`buildBlockItems` 继承 2 字段。
- 内容 3 文件 + CMakeLists 1.24.0 + test_debug_overlay 版本串。

## 测试

1852/1852（+5：`BlockFurnaceFieldsTest.ParseKeepsFuelAndSmeltable`【2
字段 + 双缺省 = plain block 恒等】/ `ParseRejectsBadValues`【显式 0 /
1001 / 类型 / smeltable_to 空串五例 + 12.5 合法对照】/
`BlockContentTest.Craft6FuelAndSmeltableContentPinned`【真资产 wood 20 /
sand glass】/ `FurnaceBlockItemTest.RealAssetsInheritFurnaceFields`【真
文件 + buildBlockItems 继承 + lava_bucket 1000 s = substep 60000】/
`RealTickWithBlockFuelAndSmeltable`【真 tick：sand+wood → 首帧点火
1200 + 第 600 帧 glass ×1 + 仍燃】），`-Werror` 0 警告。

## 门 139

- **S1 = 第 139 次结构性免疫**（fresh 60f 四方 ×2 **168140 B** 全 EXACT
  + savenorm 同侧/跨侧；furnace 永不 worldgen = m_furnaces 空双结构惰性
  + 属性字段 = 纯数据零渲染变更。**base18 锚随服务器重启 /tmp 清空丢失
  = 四方字节一致后刷新采用**【168140 B (679,744) 与丢失前尺寸一致 =
  同一 WM 平铺态延续；PITS 265/315/317 先例】）。
- **S2 = 炉子场景无输入跑（判别 = 新旧终态互斥）**：scene（炉
  (21,39,-4) + 预置 input sand ×1 + fuel wood ×1 + progress 0 +
  fuel_substeps 0；玩家 (21.5,39,-2.5) pitch −0.2）900f 四跑：
  - NEW（v1.24）：frame 1 点火（wood 消耗 → 1200，`Furnace lit` 日志）/
    frame 600 熔炼完成（glass ×1，`Furnace smelt complete: glass`
    日志）/ 900 帧 fuel_substeps **301**【点火帧授予不消耗 = 1200−899；
    冒烟 30f → 1171 = 1200−29 精确实证】仍燃 → 火焰在场 = P66 活粒子
    类（new×2 RAW 差 **1453 px 100% 含于火焰带** + savenorm = blocks
    集合恒等 + fluids 5 格 @ 102 块视图外 churn【PITS 244/283 类】+
    **炉子态 ×2 逐位一致** glass×1/301）
  - OLD（v1.23.0）：wood 非燃料恒保留 → 恒无火 → 无熔炼 → 终态
    {input sand×1, fuel wood×1, fuel_substeps 0, output 空} 日志 0 行 +
    old×2 RAW **235855 B EXACT**（无火焰 = 像素级确定性）+ savenorm =
    blocks 集合恒等 + fluids 1 格 @ 97 块 churn【同款】+ 炉子态 ×2 逐
    位一致
  - **互斥**：new output glass×1 + fuel_substeps 301 vs old input
    sand×1 + fuel wood×1 + 0（玻璃在产 vs 原料恒存）+ 像素 = new vs old
    diff **4337 px bbox 130×150 紧凑火焰簇**（上中画面不触热栏带）+
    日志 2 vs 0 行
- **S3 = 套件 1852**。
- **S6 = init 失败 ×3 点名 PASS**（scratch 资产 headless）：
  - 6a wood `fuel_time: 0` 显式 → `block property 'fuel_time' must be a
    number in (0, 1000], got 0.000000`
  - 6b sand `smeltable_to: ""` → `block property 'smeltable_to' must be
    a non-empty item name (omit it for a plain block)`
  - 6c sand `smeltable_to: "glassz"` → `item 'sand' references unknown
    smelting product 'glassz'`（game init 产物交叉校验点名 = 既有全物品
    遍历零新代码覆盖 block 物品实证）
- **性能 A/B 必做 = PASS**（真 v1.23.0 tm_v123_perf vs v1.24.0-dev tm_v124_perf，banner 实证，**同款 g139 插桩补丁器两侧同打**【/tmp 重启丢失旧 step0 树 = 重建，PITS 317】，fresh `--new --seed 123456789` 无输入 900f 三窗 + 终档集合对比）：PERF-WIN 全指标同档（upd med 0.329/0.351/0.354 vs 0.337/0.347/0.406 ms【win3 +15% = 窗口噪声级】/ render med 0.38-0.42 同档 / swap med 15.8 双侧 vsync 锁定 / fluid 0.042-0.048 / fires 0.007-0.010 / spread 0.0004-0.0006 / ents 0.003 / stream 0.012-0.014）+ PIPE3-S0 tickMs med 0.043-0.046 vs 0.046-0.054 同档 + **changesApplied max 9/3/1 三窗逐位一致** + **终档 fluids 18/18 集合恒等（diff 0）+ blocks 55/55 集合恒等（diff 0）= 同世界同成本**【结构惰性：fresh 无炉子 = m_furnaces 空 + 解析 init-only】= 成本可忽略，零回归最强形。

## PITS

- **PITS 317（服务器重启清空 /tmp = 全部门资产丢失）**：opencode 服务器
  重启清掉 /tmp/opencode——参照树（tm_v1XX_src/perf）、门脚本/场景/
  config/断言产物、base18 锚、插桩 step0 patch/hunk 脚本、性能 seed
  全失（仓库无损）。重建清单 = ① xenv.sh（DISPLAY=:0）② g117 对比器
  （raw 字节比 + savenorm real-dt 剥离：fires[*].remaining /
  entities[*].age / player.saturation）③ configs（以已提交
  config/config.json 用户调值为底 + nomobs/nodecay/perf 变体）④ 参照树
  （git archive <commit> + mkdir data/saves；性能树 = 同款补丁器两侧同
  打——CRAFT-6 零 game.cpp 变更 = v1.23/v1.24 树锚点字节一致）⑤ base18
  锚（S1 四方字节一致后刷新采用 = PITS 265/315 先例）⑥ 性能冻结 seed
  可换 `--new --seed`（世界生成跨二进制确定性 = 项目核心不变量，
  GEN-7/8/9 同款协议）。教训 = **门协议资产必须可从仓库 + commit
  重建**；只存在于 /tmp 且不可推导者（step0 patch）是风险点——本次重建
  为 g139_instrument.py 同款补丁器，长期须入 tools/（下轮基础设施项）。
- **门 139 场景终态 301 的 off-by-one**：点火 tick 授予整段燃烧时间且
  该 tick 不消耗（furnace.cpp 既有语义"每根燃料实际多烧 1/60 s，刻意、
  确定性"）→ 900f 终态 = 1200−899 = 301 非 300——冒烟 30f → 1171
  实证后修正断言（门脚本初版 300 = 首跑证据门拦截）。
