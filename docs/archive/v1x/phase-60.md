# Phase 60 细节存档

> 范围（用户"下两个 phase，请继续建议1 和建议2" = 建议 1，代理定案为
> 最小完整版：**flint 用途 = 箭头配方（MC 1.14+，替换 P24 两木棍配方）
> + 燧石与钢（物品 + 配方 + 点燃交互）**，消费 P59 遗留"flint v1 无
> 用途"；stone→cobblestone 材料链 = 建议 2 → Phase 61）：
> ① **箭头配方替换**（shapeless flint+stick+feather → 4 箭 = MC 1.14+；
>    旧 2 stick 竖排 → 2 = 弓 phase 时代无 flint 的替代设计 → **内容
>    契约变更**，flint 成为真材料：gravel→flint、chicken→feather、
>    树→stick = MC 闭环）；
> ② `flint_and_steel` 物品（material、64 叠、图标 = 钢杆 + 燧石尖）+
>    配方（shapeless flint+iron_ingot，MC 配方）；
> ③ **物品字段 `ignites_flamable`**（严格 bool、缺省 false、非 bool
>    点名拒绝；与五个右键通道互斥 = 通道所有权纪律 P47 范式；init
>    白名单 = P58 教训）；
> ④ **右键链点燃分支**（interact/bucket 之后、placeBlock 之前：目标
>    `flamable` 块 → setBlock air **无掉落**【P58 熔岩烧语义】、**不
>    消耗**【MC：燧石与钢用不坏】；炉子/工作台仍开面板；非 flamable
>    fall through = material 的 placeBlock no-op）；
> ⑤ `ignite` 声音（爆裂噪声 burst）；
> ⑥ 8 个单测（字段解析 5 + 内容钉死 3）。

## 阶段报告

- **`[2026-10-14, Phase 60]`** flint 用途：箭头 + 燧石与钢
  — 1018/1018（+8），`-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 33 次
  结构性免疫【P59 重锚后首次过门、无重锚】+ S2 点燃【wheat_3 烧成
  air、零掉落、×2 savenorm】+ S2b 非可燃隔离【stone 保留】+ S2ctl
  无输入【×2 RAW】+ S6 init 失败 ×2）。

### 实现内容

**核心（1 字段 + 1 parse 函数 + 1 分支 + 1 声音）**：
- `src/item/item.hpp`：`Item::ignitesFlamable`（bool，缺省 false，
  Phase 60 注释块：语义 + 互斥 + v1 范围）。
- `src/item/item_registry.{hpp,cpp}`：`parseIgnites`（严格 bool；
  true 时逐一检查 restoreHunger>0 / drinkTime>0 / shootSpeed>0 /
  dyeColor 非空 / 桶对字段非空 → 点名拒绝 "one item owns one
  right-click channel"）；parse 流最后调用（互斥检查需要其他通道
  先解析完）；unknown-field 白名单加 `ignites_flamable`（P58 教训：
  白名单漏列 = 每跑警告）。
- `src/core/game.cpp`：右键链 `tryFluidBucket` 之后、`placeBlock`
  之前的点燃分支——`m_inventory.at(m_selectedSlot)` 非空且
  `ignitesFlamable` → `targetBlock()` 命中 `flamable` 块 →
  `setBlock(air)`（Game 收口 = 作物 map 维护）+ `Lighting::
  updateLocal` + `m_sound.play("ignite")`。
- `tools/generate_sounds.py`：`ignite()`（Lcg 0xF1DE、0.18 s、亮噪声
  + 高频下滑 pop、lowpass3 链同款）+ SOUNDS 注册 →
  `assets/sounds/ignite.wav`。

**内容（数据）**：
- `assets/data/items/flint_and_steel.json`：material + icon +
  `ignites_flamable: true`。
- `assets/data/recipes/arrow.json`：**替换**——shapeless，output
  {arrow, 4}，ingredients [flint×1, stick×1, feather×1]（MC 1.14+）。
- `assets/data/recipes/flint_and_steel.json`：shapeless，output
  {flint_and_steel, 1}，ingredients [flint×1, iron_ingot×1]（MC 配方）。
- `tools/generate_item_icons.py`：`flint_and_steel()`（2 设计 px 对角
  钢杆 (4,13)→(9,8) + 2 段暗燧石尖 (10,7)→(11,6) + 2 点高光，salt
  41..45）+ ITEMS 注册 → `assets/textures/items/flint_and_steel.png`。

**测试（+8，1018/1018）**：
- `test_item`：`IgnitesFlamableDefaultsFalse` / `AcceptsBool` /
  `RejectsNonBool`（点名 "ignites_flamable" + "boolean"）/
  `RejectsFood`（"one item owns one right-click channel"）/
  `RejectsLauncher`（点名 launcher）+ `Phase60FlintAndSteelItemContent`
  （真实 assets：Material / 64 叠 / ignitesFlamable true / icon 名）。
- `test_recipe`：`Phase60ArrowRecipeContent`（真实 recipes：shapeless /
  output arrow×4 / 恰好 3 原料 flint+stick+feather 各 1）/
  `Phase60FlintAndSteelRecipeContent`（output ×1 / 恰好 flint+
  iron_ingot 各 1）；`RealAssets` fixture（P55/59 内容钉死范式）。
- 顺手：`RegistersVerticalSingleColumnPattern` 的注释更新（"arrow
  recipe's shape" → "former arrow recipe's shape (Phase 24, replaced
  in Phase 60)"——测试本身用内联 JSON 与真实文件解耦，不受影响）。

**无 format 变更**（format 9）。

### 关键决策

- **箭头配方 = 替换而非并存**：建议 1 原文"箭头合成链"= 箭进 flint
  链。旧 2 木棍配方是无 flint 时代的替代设计；并存 = 非 MC 的双轨
  获取（冗余 + 削弱 flint 的材料地位）。MC 1.14+ 单轨 = 定案。代价
  = 内容契约变更（早期箭需 flint + 羽毛）→ 文档化（KNOWN_ISSUES
  条目改写 + GAMEPLAY/MECHANICS/CONTENT_GUIDE 同步）；无测试/配方
  依赖旧配方（审计：test_recipe 用内联 JSON；无配方以 stone 等
  旧产物为链——箭头物品本身不变，只是获取路径变）。
- **燧石与钢 = 点燃 flamable 块（不是火块系统）**：v1 语义 = P58
  熔岩烧的同款"直接移除、无掉落"，复用 `flamable` 属性（当前仅
  wheat_1..7）；引火块/火焰粒子/引燃 Mob 都超出最小范围（KNOWN_
  ISSUES 文档化 v1 范围）。不消耗 = MC 值。
- **点燃分支位置 = tryFluidBucket 之后、placeBlock 之前**：面板交互
  优先（炉子/工作台仍开面板 = MC）；material 的 placeBlock 是 no-op
  → 落空 fall through 无副作用。
- **S2 场景 = P52 几何 + 静态论证**：wheat_3 放 (22,46,−5)（yaw 0、
  pitch atan2(−0.12,5)，P59 已复验 HIT）；**下方格 = 草 ≠ farmland →
  `grows_on` 生长门失败 → 场景全静态**（无需像 P59 那样预计算
  roll——点燃无概率成分，单点击边缘事件、无计时器、无随机）。
  entities [] 断言区分点燃 vs 破坏（破坏会掉 wheat_1 种子）。

### E2E 证据链（点燃的证明方式）

- **S1 第 33 次结构性免疫**（P59 重锚后首次过门，**无重锚**）：
  无新块 / 无 worldgen 改动 / 默认场景纹理零变化（flint_and_steel =
  不在默认背包的物品；配方只在 UI）→ fresh 60f ×2 字节一致 + ==
  base18（P59 基线）字节一致。脚本对 mismatch **FAIL 而非重锚**。
- **S2 点燃**（420f ×2 savenorm 字节一致 + save EXACT）：wheat_3 放
  (22,46,−5)、slot0 = flint_and_steel、瞄准锚定存档（零鼠标移动），
  t_ml+2.9 s **单次 `xdotool click 3`** → 终态 diff = {}（目标格恢复
  生成态 air）、entities = []（**零掉落** = 熔岩烧语义；若是破坏会
  掉 wheat_1）、health 20 / pos 不变 / yaw-pitch 不变。
- **S2b 非可燃隔离**（同格 stone、同物品、同点击）：stone 保留
  （diff = {stone}）、零实体 = 点燃器只烧 `flamable` 块。
- **S2ctl 无输入**（S2 场景零输入，240f ×2 RAW 字节一致）：wheat
  保留（diff = {wheat_3}）、零实体 = 烧来自点燃输入而非场景装载。
- **S6 init 失败 ×2**（scratch assets）：flint_and_steel.json
  `ignites_flamable: "yes"` → "item file 'data/items/
  flint_and_steel.json': 'ignites_flamable' must be a boolean" /
  arrow.json flint→flintt → "recipe 'arrow' uses unknown ingredient
  item 'flintt'"（点名）。

### 会话存档

- **zsh 多行 echo 续行（本批唯一脚本 bug）**：S1 PASS 分支的 echo
  跨两行但漏了行尾反斜杠（P59 同款写法有）→ 第二行被当命令执行
  （command not found，rc 127）：日志 PASS 行截断、脚本不受阻（无
  set -e）、S1 判定正确（"S1 PASS" 只在 cmp 通过分支打印）。教训
  入 PITS；修脚本时对照 P59 逐行 diff 续行符。
- **`write` 覆盖既有文件**：arrow.json 早已存在（P24 配方）——
  `write` 前必须 `git status` 确认是 `??` 而非 `M`；本 phase 靠
  提交前审计发现（diff 即契约变更记录）。
- **右键单点击 E2E**：`xdotool click 3`（mousedown+mouseup 毫秒级）
  对 `wasPressed(Right)` 边缘检测 = 恰好一帧 press = 一次点燃；无需
  P55 的 hold 时序（那是按住破坏的续破问题）。
- **生成器确定性复验**：两生成器重跑后 `git status` 只新增 2 个
  PNG/1 个 WAV，既有资源全字节一致。
- **P61 调研（本 phase 会话内完成，未动工作树）**：无配方以
  "stone" 为原料（stone 自掉变更不破坏配方链）；只有 stone.json
  掉 "stone"；atlas 65/128、下一空位 [9,0]（空位加 tile = 无 UV
  移位 = 无重锚 → 第 34 次免疫）；smeltable 物品全为 material
  （block item 无 item 数据 = cobblestone 不能熔炼回 stone → P61
  只走 4 锻石合成路线，熔炼偏离文档化）。
- 脚本：`/tmp/opencode/p60_e2e.zsh`（全批：S1/S2/S2b/S2ctl/S6；
  助手 = p59_helpers.zsh 前缀改 p60_，`run_ignite` = run_break 的
  单右键变体）；日志 `/tmp/opencode/p60e2e/p60_e2e.log`。
