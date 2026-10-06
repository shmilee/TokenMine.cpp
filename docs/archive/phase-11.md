# Phase 11 细节存档

> Phase 30 文档整理：由 `SESSION_CONTEXT.md`「Phase 11 已完成（细节存档）」节、`KNOWN_ISSUES.md`「已修复（归档）」条目 合并而成（未改写原文）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/...` 驱动脚本）指向当时状态；当前文档布局
> 见 `README.md`「文档地图」。

## 阶段报告（原 KNOWN_ISSUES「已修复（归档）」）

- **`[2026-10-04, Phase 11]` 破坏不掉落物品 / 无拾取 / 无背包** — Phase 11 实现完整物品闭环：`Item`/`ItemStack`/`ItemRegistry`（每个数据方块隐式同名 block item + `assets/data/items/*.json` 显式工具，严格校验）/`Inventory`（27 格 = 9 hotbar + 18 背包，确定性合并入包）；破坏按 `properties.drops`（物品名，init 严格校验）生成 `ItemEntity`（现持物品名 + 数据 maxStack，合并上限尊重物品数据），玩家 1.0 格内自动拾取（0.5s 延迟防 Q 丢出即回吸）；hotbar 改为背包前 9 格（`hotbar.json` 的 `items` 列表定义初始内容，满堆栈），放置消耗选中格，`Q` 丢弃 1 个。
- **`[2026-10-04, Phase 11]` 破坏瞬时 / 选择方块临时方案** — 破坏改为按住驱动：`breakDurationSeconds(hardness, breakSpeed) = 1.5s × hardness ÷ speed`（工具 = 数据 `break_speed`，徒手/方块物品 = 1.0；hardness ≤0 且可破坏 = 即时），准星移开/松键/换工具（Tab）进度清零；`properties.hardness` 数据首次投入使用。hotbar 从"临时 BlockID 列表"换成背包物品格（Tab 循环 9 格）。已知简化（flat 工具乘数/无裂纹动画/无 UI）记入「物品 / 背包」节。

## 会话存档（原 SESSION_CONTEXT）

- `src/item/{item.hpp,item_registry.{hpp,cpp},inventory.{hpp,cpp}}`：模型/注册表/库存（细节见
  核心架构节）。`src/block/block.hpp` 的 drops 注释更新（物品名，init 严格校验）。
- 数据：`assets/data/items/{wooden_pickaxe(break_speed 2.0),iron_pickaxe(4.0)}.json`
  （max_stack 1）；`hotbar.json` schema `blocks`→`items`（8 方块 + wooden_pickaxe = 9 格）。
- 行为变化：**放置消耗库存**（初始 hotbar 满堆栈 block 64/工具 1，不再无限）；破坏改按住驱动
  （grass 0.9s / stone 2.25s 徒手，wooden pickaxe stone 1.125s）；Q 丢弃 1 个。
- 新键 `GameKey::Q`（SDLK_Q；kKeyCount 同步——坑位合集有记录）。
- 测试 351/351（23 新增，见项目状态）。E2E（`/tmp/opencode/p11_e2e.zsh`，配方 A|B|C）：
  A 组 = 锁定→直下(dy=520)→**按住左键 1.5s**（grass 0.9s 破坏，~2.2s 完成）→玩家掉进
  1 格深洞（底心 (0.5,53,0.5) 精确）→掉落物在脚下 0.5s 延迟后**自动拾取**（临时日志确认
  `dirt x1 -> slot 9`：hotbar 9 格全满 64/64 → 正确溢出到背包格）→抬头(dy=-460, pitch -0.40)
  →Tab 切 slot 1（dirt）→右键放置于 (0,54,-1)（准星=放置 dirt +Z 面 (83,62,40)≈预测
  (80,57,40)=dirt 平均×0.6 面着色）；B 组=短按 0.3s<0.9s **不破坏**（准星仍草地绿）；
  C 组=无交互对照（准星草地）。A 组两次运行逐字节一致（玩家零水平移动+只精确掉坑，
  静止位整数精确；放置点击用初始满堆栈不依赖拾取时序）。
- 基线：60 帧无输入与 P10 `base3.png` 逐字节一致（物品/库存无渲染路径；含移除调试日志
  后的最终二进制，A 组截图与调试构建也逐字节一致）。
- 刻意简化（KNOWN_ISSUES「物品 / 背包」节）：工具 flat 乘数无分块类型效率；无裂纹动画
  （Phase 15）；无库存 UI/手持视图（Phase 15）；工具无 in-world 表现（Q 丢出=不可见实体）；
  拾取固定 1.0 半径无磁力；iron_pickaxe 不可获得（Phase 12 合成）；库存/实体不持久化
  （Phase 13 必须含 Inventory 27 格 + 实体列表）。
