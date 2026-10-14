# Phase 79 细节存档

> 范围（用户"好的,请继续按建议进行下两个phase" = 建议 1，代理定案为
> 最小完整版：**弓 v4 = 力量弓 + 速度弓**——P42 遗留"无箭矢强化"的
> 力量/速度部分；`shoot_speed`/`shoot_damage` 本就是发射器的数据驱动
> 参数，新弓 = 不同数据的新物品 = **纯内容零核心代码**（P62/P67/P68
> 范式，P77 `infinite` 字段的姊妹））。

## 阶段报告

- **`[2026-10-15, Phase 79]`** 弓 v4：力量弓 + 速度弓 — 1112/1112
  （+3：内容钉死），`-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 52 次
  结构性免疫【无重锚】+ S2/S2b/S2c 三弓俯角连射【各 ×2 savenorm +
  EXACT】+ 弹道像素判别 + S6 init 失败 ×2）。

### 实现内容

**内容（2 新物品 + 2 配方 + 2 图标，零核心代码）**：
- `assets/data/items/power_bow.json`：bow 同款数据（shoot_speed 15、
  ammo arrow、max_stack 1、attack_damage 1）+ **`shoot_damage` 8**
  （弓的 5 +3 = MC 力量 III 档的项目压缩刻度）。
- `assets/data/items/speed_bow.json`：同上 + **`shoot_speed` 20**
  （弓的 15 +5 = 箭初速 +33%，弹道更远）。
- 配方（shapeless，flint_and_steel 范式）：`power_bow` = bow + 2
  diamond（顶级材料，与 infinite_bow 同级门槛）；`speed_bow` = bow +
  gold_ingot。
- 图标（generate_item_icons.py，新 salt 97-100，既有图标确定性再生零
  变化）：`power_bow` = 弓 + 弓心橙红迸裂（伤害标记）；`speed_bow` =
  弓 + 弦中青色速度线（速度标记）。
- **不在默认 hotbar**（结构性免疫，P67/P68/P77 工具/弓范式）。
- **无新字段**：发射语义（拉弓/消耗/威力缩放/击退）全部走 P21/24 既
  有数据驱动路径（`fireBow` 读 `item.shootSpeed/shootDamage`）。

**测试（1109 → 1112）**：
- `PowerBowContentPinned` / `SpeedBowContentPinned`（真实资产字段全
  钉：speed/damage/ammo/infinite false/icon/max_stack）+
  `BowEnhancementRecipesPinned`（两配方原料/数量/产物钉死，表驱动）。

### E2E（全绿；批跑脚本 `/tmp/opencode/p79_e2e.zsh`）

- **射出可观测性设计**：俯角 pitch −0.5 连射 2 次（右键满拉 1.7 s）
  ——箭以 15/20 b/s 飞出 **2.6 / ~4.2 块**插地：**超出 P42 拾取半径
  1.0**（留在世界 = 终态截图可见）+ 100 s 插墙寿命 > run。每弓 2 箭
  **消耗**（arrow ×2 → ×0；消耗只发生在 `fireBow` = 两箭射出的存档证
  明，无需活体靶）。相机 yaw/pitch 由 save EXACT 钉死（无 grab
  delta）→ 世界像素跨场景可比。
- **S2 力量弓 / S2b 速度弓 / S2c 普通弓对照**（各输入 600f ×2 savenorm
  + EXACT）：三弓终态 arrow ×0（都射了 2 箭）。
- **弹道像素判别**（世界区 y<676 掩码 HUD——热栏/手部显示不同弓图标）：
  - power vs plain = **82 px ≤ 150**：力量只改伤害不改弹道（恒等；
    82 px = real-dt 锚定释放帧的相位抖动——见 PITS）。
  - speed vs plain = **326 px ∈ [100, 1000]**：20 b/s 的箭落点远 ~1.6
    块 = 速度数据进入箭物理的证明。
- **S6 init 失败 ×2**（scratch 资产）：配方原料 `diamondz` → "uses
  unknown ingredient item" 点名 / 产物 `speed_bowz` → "outputs
  unknown item" 点名。
- **S1 第 52 次结构性免疫**：新物品不在默认 hotbar + 图标独立 PNG +
  配方只在 UI = 无输入零活动。
- **PITS +1**：real-dt 锚定输入 → 固定步长实体帧相位抖动（同弓跨批
  0/70/82 px；像素恒等断言改"抖动界 + 位移界"双界关系式）。

### 变更文件

- `assets/data/items/power_bow.json`（新）、
  `assets/data/items/speed_bow.json`（新）、
  `assets/data/recipes/power_bow.json`（新）、
  `assets/data/recipes/speed_bow.json`（新）、
  `assets/textures/items/power_bow.png`（新，工具链产物）、
  `assets/textures/items/speed_bow.png`（新，工具链产物）、
  `tools/generate_item_icons.py`
- `tests/test_item.cpp`（+3）
- `docs/ROADMAP.md`、`docs/KNOWN_ISSUES.md`、`docs/MECHANICS.md`、
  `docs/GAMEPLAY.md`、`docs/PITS.md`（+1）、`AGENTS.md`、
  `docs/archive/phase-79.md`

### 已知问题（本阶段）

- 弓/箭节"弓强化 = 独立物品而非附魔"条扩到力量/速度（`[Phase 77]` →
  `[Phase 79]`）；**无组合弓**（力量+速度+无限同弓 = MC 附魔台产物，
  项目无附魔系统）、无穿透/火焰附魔。
- 力量数值（8）无活体靶 E2E（箭伤害机制 = P21/24 既有已证路径；数值
  由内容钉死 + 弹道恒等像素证明"力量不改弹道"）。

### 下一步建议

1. **内容扩展线**（P45/P61 范式零核心代码：砂岩 = 沙的材料链【3 沙
   锻砂岩，P61 锻石同款】+ 纹理 + 配方 + 破坏掉落 E2E）。
2. 弓 v5（组合弓：power_speed_bow / 全附魔弓 = 三效果叠加物品，同
   范式低成本）。
3. 信标（吸收第二来源，需方块实体系统 = 高成本）。
4. 熔岩湖 worldgen（用户明示"留到很久的未来"，优先级最低）。
