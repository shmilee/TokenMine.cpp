# Phase SURV-1 存档（护甲系统）

> v1.0 任务表 15/29（BACKLOG SURV 行首项）。commit `785715b`（实现）+
> `c8d289d`（MC 值修正），发布 v0.13.0。
> 任务条：「护甲系统（减伤/材质档/穿戴）（生存条）」= KNOWN_ISSUES
> 「无护甲/摔落缓冲机制」[Phase 16]。

## 定案

最小完整版 = **数据驱动 12 件护甲（三材质档 MC 精确值）+ MC 整数 floor
减伤（全伤害源）+ 右键穿戴 + format 16 + HUD 护甲排**。护甲 = Material
物品 + `armor_slot`/`armor_points` 严格配对字段（不加 ItemType 枚举值）。

## 实现

- **内容**（12 物品 + 12 配方 + 12 图标）：
  | 材质 | helmet | chest | legs | boots | 总 |
  |---|---|---|---|---|---|
  | leather | 1 | 3 | 2 | 1 | 7 |
  | iron | 2 | 6 | 5 | 1 | 14 |
  | diamond | 3 | 8 | 6 | 3 | 20 |
  （MC 精确值——首轮按代理定案错值 2/4/3/1、3/6/3/2 实现，c8d289d 纯
  JSON 修正 + 内容钉死测试同步）；max_stack 全 1（MC 不叠）；配方 = MC
  1.14+ 图案（LLL / L L·LLL·LLL / LLL·L L·L L / L L·L L）；图标 = 生成器
  4 剪影函数 + salt 105-132（重跑既有 57 张 PNG 零变化 = 确定性证明）。
  默认 hotbar 不加（结构性免疫先例）。
- **parse**（item_registry）：`armor_slot`（4 槽词汇表点名）+
  `armor_points`（int [0,20]）严格配对（缺一/空串/非整数/越界点名拒绝）；
  **material only**（tool 带 armor 字段拒绝）；与 food/drink/launcher/
  dye/bucket/ignites **6 通道逐对互斥**（parseIgnites 同款，parse 流最后）；
  白名单加 2 键。
- **减伤**（`Player::damage` 单收口，resistance 之前、吸收池之前）：
  `pts = min(Σ佩戴, kArmorMaxPoints 20)`；
  `amount = floor(amount × (100 − kArmorReductionPerPoint 4 × pts)) / 100`
  （MC 整数 floor 语义；**pts=0 位级恒等** = 存量伤害测试零改动；命名
  常量）。**全伤害源生效** = 坠落/饥饿/熔岩/虚空/mob/箭/火 tick 全走
  damage() → **摔落缓冲已覆盖**（MC 豁免的窒息 = SURV-2 未实现，届时
  单独立项记档）。偏离记档：float HP 无半心量化 → 1.0×3 点 = 0.88 非 0
  （MC「3 点挡火 tick」不成立，单测 ArmorDamageFloorExactValues 钉死）。
- **穿戴**（`Game::tryArmor`，右键链：点火器之后、潜行旋转之前）：
  持 armor 右键 = 穿上/占位槽交换 + cloth 音效（既有 16 wav 最接近 MC
  护甲音，无新 wav）；空手右键 = 固定序 head→chest→legs→feet 第一个占位
  槽到手上（循环全脱）。既有通道优先回归保持（营火/食物/桶/染色先于
  armor——parse 互斥 + 链序双保险）。
- **存档 format 16**：`player.armor` 恒写（只含占位槽，固定序）；16 必需
  / ≤15 带键 = 版本谎报拒绝；物品存在性 + armor_slot 匹配槽名 parse 期
  交叉校验；15→16 迁移空 {}；applySave 4f 段恢复。
- **HUD**：心行上方护甲排（hy−16−kStripGap 避 PRE-10 效果图标/吸收红心
  重叠坑；hx0 左对齐 4 固定位 16px+2gap；仅占位槽绘制 = 无甲零绘制
  结构性免疫）+ F3 armor 点行。
- **零核心 ItemType 变更、零 config**；死亡不清甲（既有死亡语义 = 背包保留）。

## 变更文件（785715b，54 文件 +1214/−19；c8d289d 6 文件 +11/−11；f9c2c17 9 文件 +481/−55 = 门内修复：新增 item/armor.{hpp,cpp} + test_armor.cpp 9 测 + game.cpp tryArmor 薄适配层 + PITS/KNOWN_ISSUES/AGENTS）

- `src/item/item.hpp`（+39：4 槽词汇表/常量/字段）、`item_registry.{hpp,cpp}`（+113 parseArmor）
- `src/player/player.{hpp,cpp}`（+79：4 槽 + damage floor 减伤）
- `src/core/game.{hpp,cpp}`（+124：tryArmor + 链插入 + collect/applySave + HUD + F3）
- `src/core/save.{hpp,cpp}`（+86：format 16）
- `src/ui/debug_overlay.{hpp,cpp}`（+5：armor 行）
- `tools/generate_item_icons.py`（+90）+ 12 物品 JSON + 12 配方 JSON + 12 图标 PNG
- `tests/test_item.cpp`（+188，9 测试）/ `test_player.cpp`（+103，5）/ `test_save.cpp`（+188−19，7 新 + 降级适配）/ `test_campfire.cpp`（+9−5 适配）/ `test_debug_overlay.cpp`（字面）
- `docs/KNOWN_ISSUES.md`（生存条「无护甲/摔落缓冲」关闭〔窒息 → SURV-2 保留〕）

## 测试

1430/1430（首轮 1421 = +21；10-26 门内修复 +9 = 护甲交互纯函数
`test_armor.cpp`）、干净重建 `-Werror` 0 警告、存量 1400 零回归（damage
侧 = pts 0 位级恒等背书）、图标确定性（重跑既有 57 PNG 0 修改）。
- item parse 9：配对缺失/空串/非整数/越界/未知槽/tool 拒绝/6 通道互斥/
  12 物品逐字段 + 12 配方 pattern/key 内容钉死
- damage 5：floor 精确值（10×10 点=6.0 / 1×2 点=0.92 / 1×3 点=0.88）/
  pts=0 四类源位级恒等 / 32 点帽 20 / armor→resistance→池次序精确值 / 槽 API
- 护甲交互 9（10-26）：穿上/自槽只穿/换槽往返/**脱下件必到选中槽
  （PITS 246 回归）**/首占位序/跳空到 feet/选中槽写入/非护甲 fall-through/
  空手无甲 fall-through
- save format16 7：RoundTrips 只含占位槽 / 空对象 / 16 必需键 / 15 带键拒绝
  + 迁移 / 槽不匹配点名 / 未知物品 / 未知槽键

## E2E（门 95）

- **S1 base（无输入 ×2）**：新二进制 + 新资产（12 护甲物品已注册但默认
  hotbar 无）b1/b2 全帧字节一致 + **对 gate88 锚字节一致 = 真结构性
  免疫**（护甲无输入零活动：无甲 = HUD 护甲排零绘制 + damage 减伤
  pts 0 位级恒等 + 存档恒写空 armor 段）。
- **S2 坠落（13 格 = 基础 10 伤）**：fn1/fn2（新·无甲）hp=**10.0**、
  ×2 RAW 截图字节一致 + 存档字节一致；fo1/fo2（**旧 v0.12.0**·无甲）
  hp=10.0 = 新旧无甲恒等（damage 路径零回归）；fi1/fi2（新·iron 14
  点）hp=**15.6**（floor(10×(100−4×14))/100 = 4.4 → 10−4.4）×2 RAW
  一致；fd1/fd2（新·diamond 20 点）hp=**18.0**（floor(10×60)/100 = 6）
  ×2 RAW 一致——**10.0/15.6/18.0 三值互斥 = MC floor 公式进入 E2E**。
- **S3 穿戴/脱下（输入驱动，PITS 247 协议：PID 定位窗口 + viewable+top
  窗口期 + 每输入前 windowfocus --sync；热栏 = Tab 循环无数字键）**：
  e1/e2（equip 场景 slot0=iron_helmet，序列 L R）→ armor=
  {'head':'iron_helmet'}、slots 全空、sel=0，**×2 存档字节一致**；
  u1/u2（unequip 场景 4 件全穿，序列 L R Tab R Tab R Tab R）→ armor=
  {}、slots0-3=[iron_helmet, iron_chestplate, iron_leggings,
  iron_boots]、sel=3（整套脱下按 head→feet 序、每点之间 Tab 换槽 =
  震荡语义的正确用法），**×2 存档字节一致**。
- **S4 护甲排（HUD 仅指针捕获时绘制 → 捕获协议 + 重捕）**：h/hn 跑
  （hud 场景 4 件 leather / 无甲；早捕获 + T0+16.5s/19.0s 两次
  phase-gated re-L 抗 away 重平铺释放 grab，4 跑 "Pointer captured" ×1
  实证）：干净帧对 h1 vs hn1（皆 1362x744 重平铺窗）全帧 diff =
  **81 px，包围盒 x[552,646] y[604,620] = 恰 4 件皮甲图标行
  （helmet/chest/legs/boots 顺序渲染 = 裁剪目视实证），带外零差异**；
  确定性：h1/h2 存档 savenorm2 EXACT + hn1/hn2 savenorm2 EXACT（hn1
  yaw/pitch 漂移 = PITS #1 grab delta 环境性，存档其余逐字节一致）。
  黑相期 ~70 min（GLX swap stall）：e/u 跑 3-6 拍内出干净帧、hn 全黑 =
  存档级证据不受渲染影响（PITS 247 条款）。
- **S6 init 失败 ×2**：`'armor_points' must be in [0, 20], got 21`（
  iron_chestplate 点名）/ `'armor_slot' 'hood' is not a known slot
  (vocabulary: "head", "chest", "legs", "feet")`（leather_helmet 点名），
  跑后资产恢复。
- **10-26 门内修复（PITS 246）**：脱下分支 dangling reference（引用
  跨清空 = 护甲件消失，lock2 现场 L R R 实证）→ 逻辑上提纯函数
  `item/armor.{hpp,cpp}`（campfire 范式）+ 9 单测含回归；修复后受控
  实验 A（L R → 穿上）/ B（L R Tab R → 卸下到 slot1 无丢失）全中 =
  lock2 之谜定案。

## 性能（插桩 A/B v0.12.0，发布前测试）

- **fall 900f 三 300f 窗**（场景 = 13 格坠落 + 大流送）：render med
  0.371-0.470（old 0.371-0.454 同档）、upd med 45.8-48.7 ms（old
  46.7-49.9 同档）、swap med 0.476-0.498（old 同款）、stream 1.8-2.4 s
  （old 1.8-2.4 s 同档）、player 2.3-2.7 ms（old 2.5-2.6）、ents
  0.27-0.31（old 0.27-0.28）——**无甲/穿甲（iron 14 点）/旧二进制
  三者全指标同档**。
- **vd16 fresh 流送填充 600f**：render med 0.713-0.724（old
  0.692-0.753）、upd med 0.218-0.290（old 0.222-0.311）、stream
  44.7/30.5 ms（old 45.6/30.8）——同档。
- **结论：SURV-1 成本可忽略，无回归**（减伤 = 每次伤害事件一次 floor
  计算；HUD 护甲排仅指针捕获时绘制 = 无输入零活动；存档恒写空段 =
  退出 O(1)）。
- 环境注：10-26 凌晨 GLX 黑相波 70+ min，性能跑经 1 帧探针门控在
  波隙完成（黑相期 render timing 失真 = 探针判废重跑）。

## 偏离

- chain 无材料（三档 = leather/iron/diamond，chain 记档）
- float HP 无半心量化：1.0 伤×3 点 = 0.88（非 MC 整数屏蔽；单测钉死）
- 全源减伤（窒息未实现 = SURV-2，届时按 MC 豁免规则单独立项）
- 音效 = 既有 cloth（无新 wav）
- mob 无护甲、护甲无耐久（= SURV-3 项）、无附魔
- **脱下/穿上震荡语义**（10-26 定案）：脱下件回到选中槽 → 下一次右键
  = 手非空 = 原槽（已空）= 重新穿上；"连点脱整套"不成立，脱整套须每
  点之间换空热栏槽（**热栏只有 Tab 循环，无 MC 数字键直选** = 既有
  交互面）。E2E 序列 L R Tab R Tab R Tab R 验证整套脱下。
