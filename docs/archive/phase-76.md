# Phase 76 细节存档

> 范围（用户"下一 phase 选成本最低的" = P75 定案建议中的再点燃项，
> 代理定案为最小完整版：**营火 v3 = 再点燃**，全量消费 P75 遗留"水
> 浇灭后无再点燃"偏离；四槽图标全渲染不在本次范围——一格放不下 4 个
> 图标，保持 P75 简化）：新块属性 `ignites_to`（数据驱动的点火器目
> 标）+ `campfire_extinguished.ignites_to = "campfire"` + 营火分支内
> 的再点燃交互。

## 阶段报告

- **`[2026-10-15, Phase 76]`** 营火 v3：再点燃 — 1104/1104（+3：
  属性解析 2 + 内容钉死改写 1），`-Werror` 恒 0 警告；E2E 批跑全绿
  （S1 第 49 次结构性免疫【无重锚】+ S2 再点燃【×2 savenorm +
  EXACT】+ S2ctl P60 flamable 回归【×2 savenorm + EXACT】+ S6 init
  失败 ×1）。

### 实现内容

**数据（1 新块属性 + 1 个 JSON 字段）**：
- `src/block/block.{hpp, block_registry.cpp}`：新可选块属性
  **`ignites_to`**（字符串 = 目标块名）——语义：`ignites_flamable`
  物品（燧石与钢）右键该块 = **原位 setBlock 目标块**（无掉落、物品
  不消耗、"ignite" 音 = P60 点火器语义）。解析纪律：严格字符串、
  **显式空串拒绝**（ammo 纪律，grows_to 同款）、缺省 = 空 = 旧行为；
  **init 存在性交叉校验**（目标名必须注册，typo 点名拒绝——grows_to
  / grows_on / plant_on 同款纪律）。
- `assets/data/blocks/campfire_extinguished.json`：
  `ignites_to: "campfire"`（MC 语义：燧石与钢点回未点燃的营火）。

**核心**：
- `src/core/game.cpp`（两处）：
  - **营火分支内再点燃**（`tryCampfireCook` 开头）：手持
    `ignites_flamable` 物品 + 目标块 `ignitesTo` 非空 → 原位
    setBlock(目标) + 局部重光照（光 0→15）+ "ignite" 音，物品不消
    耗；烤制状态经 setBlock 收口存活并**续烤**（未点燃时钟暂停的逆
    过程）。**必须在营火分支内**：该分支（P71）先于通用点火器分支
    且消费全部营火家族点击——首轮 E2E 点击被截获（块不翻、进度不动）
    暴露了这一点（分支所有权纪律）。燧石与钢对**点燃**营火（无
    ignitesTo）落回既有放/取逻辑 = no-op（非可熔炼）。
  - **通用点火器分支加 (a) 路径**：目标块 `ignitesTo` 非空（优先于
    `flamable`）→ 原位 setBlock 目标。对营火家族实际不可达（被营火
    分支先截获），为**非营火家族块的统一数据驱动语义**（未来内容零
    代码复用）。
- **零 format、零新物品、零新纹理、零新配方**。

**测试（1101 → 1104）**：
- `IgnitesToIsAccepted`（注册 + 缺省空两断言）、
  `IgnitesToNonStringRejected`、`IgnitesToEmptyStringRejected`（ammo
  纪律点名）。
- `CampfireExtinguishedBlockContentPinned` 加 `ignitesTo == "campfire"`
  钉死。
- init 交叉校验（未知目标点名）= E2E S6（scratch 资产，Game 级无单
  测夹具，P60 先例）。

### E2E（全绿；批跑脚本 `/tmp/opencode/p76_e2e.zsh`）

- **S1 第 49 次结构性免疫**：属性在永不生成的块上 + 只经右键点击生效
  → fresh 60f ×2 + base18 字节一致。
- **S2 再点燃**（输入 600f ×2 savenorm + EXACT）：P71 瞄准几何（未点
  燃营火 @(22,45,−5)、pitch −0.220362 真实射线证明）、预置**半熟**槽
  0（beef progress 300 = 10 s 烤制的一半）、手持燧石与钢——单次右键
  → 块翻回 `campfire`（光 0→15）+ **续烤 300 子步至熟**（点击 2.9 s
  + 5 s = 7.9 s < 600f 上界 10 s @60Hz，fps 无关终态）+ 物品**未消
  耗**。终态：blocks [[22,45,−5,campfire]]、槽 0 = cooked_beef ×1、
  手 = flint_and_steel ×1。
- **S2ctl P60 flamable 回归**（输入 120f ×2 savenorm + EXACT）：同几
  何 wheat_3（flamable、无 ignites_to）+ 燧石与钢 → fire 块（新优先
  级分支未改变 flamable 路径）+ fires 1 条目（lifetime 300 mov-config
  先例保活——120f 任何 fps 下 fire 都活着）+ **无掉落** + 物品未消
  耗。
- **S6 init 失败**：scratch 资产 `campfire_extinguished.json` 改
  `ignites_to: "campfirez"` → "ignites_to names unknown block
  'campfirez'" 点名。
- **PITS +1**：项目改名（mine-craft → TokenMine.cpp）后 `xdotool
  search --name "TokenMine.cpp"` **子串误匹配用户终端窗口**（标题
  `user@host:~/project/TokenMine.cpp` 含游戏标题）→ 状态机对终端
  "稳定化"成功（日志无 stabilizing 行 = 特征），真游戏窗口被
  awesomeWM 平铺 1272×1552。修 = 锚定正则 `--name "^TokenMine.cpp$"`。

### 变更文件

- `src/block/block.hpp`、`src/block/block_registry.cpp`
- `src/core/game.cpp`（营火分支再点燃 + 通用点火器 (a) 路径 + init
  交叉校验）
- `assets/data/blocks/campfire_extinguished.json`
- `tests/test_block.cpp`（+3）、`tests/test_campfire.cpp`（钉死改
  写）
- `docs/ROADMAP.md`、`docs/KNOWN_ISSUES.md`、`docs/MECHANICS.md`、
  `docs/GAMEPLAY.md`、`docs/PITS.md`（+1）、`AGENTS.md`、
  `docs/archive/phase-76.md`

### 已知问题（本阶段）

- P75 遗留"水浇灭后无再点燃"**已解决**（KNOWN_ISSUES 营火条 →
  `[Phase 76]`；营火 v2 条更名 v3 边界的对应项划删）。
- 保留偏离（营火条不变）：四槽无面板 / 只渲染第一个非空槽图标 /
  未点燃的不能放食物 / 无燃料（= MC 一致）/ Mob 不可坐 / 坐姿 =
  crouch 高度 / 营火不是 fire 块。

### 下一步建议

1. **弓 v3 / 箭强化**（P42 遗留"无箭矢强化"——无限弓 `infinite` 物
   品字段或新箭物品，数据驱动，无 format 变更）。
2. **四槽图标全渲染**（营火条剩余视觉项——一格 4 个 0.5 图标需换排
   布/缩放，纯渲染层）。
3. 信标（吸收第二来源，需方块实体系统 = 高成本，可拆 v1）。
4. 熔岩湖 worldgen（用户明示"留到很久的未来"，优先级最低）。
