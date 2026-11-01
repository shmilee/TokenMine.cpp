# Phase 23 细节存档

> Phase 30 文档整理：由 `SESSION_CONTEXT.md`「Phase 23 已完成（细节存档）」节 移入（未改写原文）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/...` 驱动脚本）指向当时状态；当前文档布局
> 见 `README.md`「文档地图」。

## 会话存档（原 SESSION_CONTEXT）

### 设计（饥饿条 + 食物）
- **物品字段** `restore_hunger`（整数 [1,20]）：缺省 = 非食物，显式 0 拒绝
  （P21 bow `shoot_speed` 同款纪律）。解析 = `ItemRegistry::parseFood`（仅在场
  存在分支内做整数 + 范围校验）。当前唯一食物 = `rotten_flesh`（=4，zombie
  掉落）→ "打 mob → 吃掉落" 闭环。`leather` 刻意非食物（对齐 MC，纯收集品）。
- **玩家饥饿**（`player.hpp` Params + 状态）：`maxHunger=20`、
  `hungerDecaySeconds=4`（每 4s −1，从满条就开始衰减，无饱和度）、
  `starveDamageIntervalSeconds=4`（饥饿 0 时每 4s `damage(1)`——走正常伤害
  路径，红闪/音效/死亡全套自动复用）、`regenHungerThreshold=6`（自然回复
  门控：饥饿 < 6 时 regen 累加器**暂停不重置**，Minecraft "低于 6 无自然
  回复" 规则）。衰减/饥饿计时在 `update()` 生存段，用实际 dt 累加
  （与 P16 regen 同款）。`restoreHunger`/`setHunger` 钳制 [0,max]；
  `respawn()` 恢复满饥饿 + 清两个累加器。
- **吃食物**（`Game::tryEat`）：右键分支最前面调用——选中格非空且
  `item.restoreHunger > 0` → 食物；满饥饿 = no-op 但仍 `return true`（吞掉
  该次右键，不放置/不交互，对齐 MC "满条不能放置"）；否则回复 +
  `m_inventory.take(sel,1)` + `play("eat")`。
- **HUD**：心带（居中，178px）右侧同行 6px 间距再放 10 只鸡腿（178px）；
  两段共用图标度量（16px、2px 间隔、三态 48×16 条带）。**只游玩态显示**
  （锁定 + 背包未开）→ 无输入运行零新增绘制 → base18 门字节不变。
- **存档格式 4**：`player.hunger`（format 1-3 缺省 = 满，-1 = 未指定，
  与 health 同款省略模式）；`kSaveFormat=4`，读 1-4。
- **F3**：`DebugInfo` +hunger/maxHunger，行 `hunger %.0f / %.0f`；
  版本行 "Phase 16" → "Phase 23"（此前 P17-P22 一直未更新该行，本次顺手改）。
- **eat 音效**：低通噪声 burst × ~8Hz 咀嚼门控 + 低频 thump，0.30s
  （`tools/generate_sounds.py`，11→12 WAV）。
- **hunger.png**：`tools/generate_hunger_icons.py`（health 同款结构）——
  鸡腿 11×9 轮廓（M=肉 B=骨，offset (2,3)），肉 (168,118,72)/高亮 (192,142,94)、
  骨 (228,220,204)、空态 DARK (56,56,62)，三态 tile（满/左半/空），噪声 ±8。

### 测试（+12，615/615）
- `test_item.cpp` +3：`ParsesFood`（rotten_flesh 式 =4）、`FoodDefaultsToNotFood`
  （缺省 0）、`RejectsBadRestoreHunger`（0 / -1 / 21 / 2.5 非整数 / "a lot" 全拒）。
- `test_player.cpp` +6：`HungerDecaysEvery4Seconds`（25s→14、+5s→13）、
  `StarvationDamagesAtZeroHunger`（5s→19、+5s→18）、`StarvationKillsAtZeroHunger`
  （81s→死）、`LowHungerBlocksRegen`（hp19 + 饥 5，15s 后仍 19、饥 2）、
  `RestoreHungerClampsToMax`、`RespawnRestoresHunger`。**时序注意**：累加器加
  double 1/60，240 帧和略小于 4.0 → 首个 4s 边界落第 241 帧，断言窗口各留整帧
  余量（注释已写明）。
- `test_save.cpp` +3：`HungerRoundTrips`（7.0）、`FormatThreeLoadsWithoutHunger`
  （format 3 无 hunger → -1）、`RejectsNegativeHunger`；另 `RejectsUnknownOrMissingFormat`
  的"未来版本" 4→5、`CraftingGridsRoundTrip` 格式断言 3→4。
- `test_debug_overlay.cpp`：9→10 行、版本行 Phase 23、`hunger 14 / 20` 行。

### E2E（驱动 /tmp/opencode/p23_e2e.zsh，p21 驱动改造）
- **base 60f ×2 vs base18：逐字节通过**（HUD 仅游玩态；无输入 = 未锁定 = 零新增
  绘制）。
- **手写存档** `/tmp/opencode/hungertest.json`（format 4，seed 123456789，time 0.25，
  player [0.5,54,0.5] yaw0 pitch-0.25 hp20 **hunger 20**，热栏 0 = rotten_flesh×16，
  selected 0，其余同 bowtest 模板）。
- **decay 460f**：C1 锁定（xact+0.15，帧 ~31-53），无其他输入。帧 459 截图（t≈7.65s）：
  4s 衰减已触发 1 次（帧 ~241）→ 饥饿 19 → 最后一只鸡腿**半**（31 肉 px）。
  460f 取长 = 与第 2 次衰减（帧 ~481）留 22 帧余量（480f 只隔 2 帧，不安全）。
- **eat 460f**：C1 锁定 + **C 3 右键**（xact+3.96，帧 ~260，在衰减后）→ 吃
  （19+4=23→钳 20 满）→ 截图最后一只鸡腿**满**（52 肉 px）、热栏计数 15。
- **结果**：decay ×2 逐字节一致、eat ×2 逐字节一致（运动实体/确定性保持）；
  **eat vs decay = 59 px**：55 = 最后一只鸡腿右半（半→满，x[900,908] y[649,661]，
  eat 侧 53 肉色）、4 = 热栏计数 16→15（4 个白字 px，x[466,471] y[700,704]）；
  **UI 两带之外 0 px**（mob/场景完全抵消）。
- **实际 HUD 坐标**（1280×720，kHotbarSlotSize=44/Gap=2/Margin=8）：热栏
  x[434,846] y[668,711]；心带 x[551,729] y[646,661]；**饥饿带 x[735,913]
  y[646,661]**（最后一只 x[897,912]）。注意：与早期会话记录的 y[664,680] 不同，
  以本实测为准。

### 本会话踩坑（重要，勿再踩）
1. **X11/SDL 按钮编号：1=左 2=中 3=右**——P23 驱动初版用 `C 2`/`B 2` 当"右键"，
   实际发的是**中键**（SDL 收到 button 2 = MIDDLE，`wasPressed(Right)` 永假）→
   吃永不触发、eat/decay diff 恒 0。P17 驱动里右键用的就是 `C 3`。xact 的
   `C/B/U <n>` 直接透传 X 按钮号，**右键 = 3**。
2. **纹理噪声打散颜色直方图**——判"某 HUD 带内有没有图标"不能用
   top-N 颜色计数（每像素 ±8 噪声 → 每个颜色只出现 1 次，被背景高频色淹没，
   曾误判"饥饿带没画"）。正确做法：按 18px 图标格子逐格数"肉色/红色"像素
   （鸡腿 52 px/只、心 69 px/颗），或用已知轮廓 bbox。
3. **排查链**（供参考）：batch 顶点/纹理指针 DBG（饥饿 batch 60 顶点、v0=(735,646)、
   tex=&m_hungerTexture 全对）→ 洋红 quad 覆盖测试（区域无遮挡）→ 游戏自带
   `Png::decode` 探针（hunger.png 解码正确）→ GL `glGetTexImage` 回读（纹理内容
   正确）→ 最终在 `Input::pump` + `handleInteraction` 双层 DBG 看到
   `R=0 M=1`（中键）→ 定位按钮号。结论：**渲染链全对，是输入编号错**。
4. **E2E 里有一个来源不明的第二组左键点击**（C1 后 ~3.7s 内，xact 在 sleep、
   非 xact 所发，疑似 tiling WM 合成事件）：确定性出现（各 pair 逐字节一致）、
   锁定态下 = 一次空挥（未命中任何 mob/方块）、在 eat/decay 对照中两边都有
   → diff 抵消，无影响。未来输入 E2E 若依赖精确左键次数需注意。
5. **驱动/探针**：`p23_e2e.zsh`（decay/eat 两例，460f，hungertest 存档每跑前
   重拷 pristine）、`probe_png.cpp`（游戏 Png::decode dump 像素）、
   `probe_item.cpp`（真实资产 registry 断言 restoreHunger）——throwaway，
   在 /tmp/opencode。xact 右键 = `C 3`/`B 3`+`U 3`。

### 变更文件
- `src/item/item.hpp`（`restoreHunger` int 字段）、`src/item/item_registry.{hpp,cpp}`
  （`parseFood` + 已知字段表 + `<cmath>`）、`assets/data/items/rotten_flesh.json`
  （+`restore_hunger:4`）
- `src/player/player.{hpp,cpp}`（Params 4 字段、hunger 状态 3 成员、
  `hunger()/maxHunger()/restoreHunger()/setHunger()`、update 衰减+饥饿+regen
  门控、respawn 满饥饿）
- `src/core/game.{hpp,cpp}`（`tryEat` + 右键钩子、`m_hungerTexture` 加载、
  HUD 心带/饥饿带合并 `drawStrip`（共享图标度量）、F3 填值、collectSave/apply
  的 hunger、版本无关）
- `src/ui/debug_overlay.{hpp,cpp}`（+hunger/maxHunger、+hunger 行、版本行
  Phase 23）
- `src/core/save.{hpp,cpp}`（kSaveFormat 4、playerHunger 字段、序列化/解析）
- `tools/generate_hunger_icons.py`（新）、`tools/generate_sounds.py`（+eat）、
  `assets/textures/hunger.png`（新）、`assets/sounds/eat.wav`（新）
- `tests/test_item.cpp`（+3）、`tests/test_player.cpp`（+6）、
  `tests/test_save.cpp`（+3 +2 处修正）、`tests/test_debug_overlay.cpp`（行更新）
- README（生存系统节 +饥饿/食物数据/门控/重生/HUD、存档节 +生命/饥饿、
  音效 11→12 +进食、物品 JSON 示例 +restore_hunger、标题 Phase 23、roadmap
  +P23 行）、KNOWN_ISSUES（生存节"自然回复"条目更新为门控版 + 新增
  "饥饿/食物（Phase 23）"节 8 条）、SESSION_CONTEXT（本文件）。
- `config/config.json` 仍为用户手改未提交（view_distance 80 / day_length 600），
  与 P23 无关。
