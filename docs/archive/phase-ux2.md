# Phase UX-2 — Game 拆分/装配层收敛（v1.x 任务表 35/40，门 150+，UX 行 2/7）

> ~8776 行 game.cpp 单类按子系统拆为多 TU + 域状态 struct 收拢（装配点
> 显式化）。**路线 = A+B（10-30 用户定案）**：A 纯文件拆分（零结构变更）
> + B per-block/流送/mob/面板族 struct 收拢；C（子系统对象化）记档矩阵
> 外。验收 = 行为位级不变（每子相无输入 ×2 字节一致 + 套件 + 收官性能
> A/B，无像素门）。

## 勘察（explorer，只读）

game.cpp 8776 行 / 105 out-of-line 方法 / 313 行文件局部常量区；game.hpp
1376 行 / ~145 m_ 成员 / 12 嵌套类型。方法九分组（行数占比）：① init
装配 19.1%（buildScene 1266 行 = 最大单方法）② 帧管线 8.2%（update
33 步序列，顺序 = 正确性）③ 渲染 25.7%（renderUi 927 行第二大门面）
④ 交互 16.4%（handleInteraction 右键链 12 级分支顺序 = 所有权）
⑤ 存档 6.2%（collectSave/applySave 收口）⑥ 流送 9.9%（十方法 job
生命周期）⑦ 实体 6.2%（updateEntities/attemptMobSpawn）⑧ 生存 2.1%
⑨ UI 面板 3.8%。装配点 A1-A8 全枚举（init 校验/帧 pass 挂钩/右键链/
setBlock 9 项簿记 + 4 镜像站点/存档收口/渲染 pass/流送 worker 窗/相机
监听器）。已外提纯函数模块 40+（campfire 范式全覆盖）= Game 只剩编排。
**构造序 = 初始化序 = 行为面**（items < inventory < recipes < UI 三
构造红线）；无任何测试 #include game.hpp / 构造 Game（1929 测试全纯
函数级 = 拆分零测试面影响，E2E 二进制协议 = 唯一验证面）。

## 子相执行

### A1（首批文件拆分：init/流送/存档三组，零结构变更）
- game_local.hpp（私有共享头，7 项跨 TU 常量/助手，plain namespace
  mc + 2 项加 inline = 多 TU 链接纪律）+ game_init.cpp（13 法 1718 行）
  + game_streaming.cpp（10 法 868 行）+ game_save.cpp（5 法 558 行）
  + game.cpp 8776→5669 行 + CMakeLists mc_core +3 源
- **方法体 verbatim**（编排者抽验 6 法整法 diff：collectSave/run/
  streamChunks/integrateStreamedColumns 零差异；buildScene/attemptMobSpawn
  差异 = 恰 UX-1 叠加块纯 + 行）；game.hpp 零触碰
- 门 151 PASS：构建 0 警告 + 1929/1929 + **二进制级证明：A1 无输入
  fresh 60f ×2 与 UX-1 拆分前二进制字节一致（168140 B）**【纯搬移最强
  形】+ 对 tm_v136_base（v1.35.0 冻结基线 = git archive 453b338 + build
  + banner 实证）old 侧 ×2 **a1n1 ≡ a1o1 字节一致** + base18 锚连续
  168140 B + savenorm old×2/跨版 EXACT

### A2（剩余 6 组全量搬移 + game.cpp 退役）
- 5 新 TU：game_frame.cpp（814 行 7 法 + 13 常量）/ game_interaction.cpp
  （1465 行 26 法 + 5 常量）/ game_entities.cpp（1046 行 12 法 + 7
  常量）/ game_render.cpp（894 行 9 法 + 25 常量）/ game_ui.cpp（1549
  行 23 法 + 14 常量）；**game.cpp 删除（零残留）**；CMakeLists mc_core
  = 8 game_*.cpp 字母序；game_local.hpp +kArrowSpawnOffset（fireBow +
  attemptMobSpawn 双 TU 共享，8 项全部 ≥2 TU 引用复核）；65 文件局部项
  = 64 单 TU 入各自匿名命名空间 + 1 共享
- 逐字符证明（编排者独立复验）：5 最长方法整法 diff VERBATIM（update
  365 / handleInteraction 330 / setBlock 214 / updateEntities 330 /
  render 236 行）+ 全部方法签名在位 + game.cpp 对 b6edcf8 纯删
- 门 152 PASS：构建 0 警告 + 1929/1929 + **a2n1 ≡ a2n2 ≡ a1o1（v1.35.0
  冻结基线）≡ a1n1（A1 态）≡ base18 锚 全字节一致 168140 B**【A 段完整
  = game.cpp 8776 行 → 8 TU，终态对拆分前位级恒等的传递链闭合】+ 跨版
  savenorm EXACT
- 顺带修 4 处陈旧注释（fog.hpp / chunk_streaming.hpp /
  test_chunk_streaming / test_mesh_job：常量现居 game_local.hpp /
  game_render.cpp / coordinates.hpp）

### B1（per-block 族 → PerBlockStates）
- 23 数据成员收拢（m_tables…m_beaconEffectId，**族内相对序逐字保持** =
  构造序红线）；`PerBlockStates m_states;` 在 m_tables 原位；
  m_openFurnacePos/m_furnaceUi 留原位（B4 面板族）；**BeaconBeam 纯类型
  前移**（嵌套 struct 内名字查找不可见外层后声明类型 = C++ [basic.
  scope]，fallback 按预案启用）；setBlock 9 项簿记 + 五消费者站点全
  显式化（146 处 m_states.*：frame 44 / init 37 / interaction 24 /
  save 18 / ui 10 / streaming 7 / render 6）+ 4 跨模块头纯注释同步
- 门 153 PASS：构建 0 警告 + 1929 + **b1n1 ≡ b1n2 ≡ a1o1（v1.35.0 基线）
  ≡ 锚 全字节一致 168140 B** + 跨版 savenorm EXACT + **读档往返**（载入
  60f 终态 + 30f：全状态恒等，仅 time = 30 步浮点累加 2e-17 内 = B 段
  读路径 applySave/rescan* 经 m_states.* 行为零差证明）

### B2（流送/worker 族 → StreamingState）
- 14 成员收拢（m_pool…m_retainedColumns，**相对序逐字保持 = 零初始化
  重排**：struct 首成员原位声明）；StreamJob/LightJob/MeshJob 三嵌套
  类型前移（B1 BeaconBeam 范式，纯类型无构造序效应）；m_flammable-
  Chunks/m_lightPendingColumns/m_fluidLightPending 跨域留平铺（域注释
  点名 = B 段记档残余）；115 处调用点改写（streaming 73 / init 8 /
  save 7 / render 5 / interaction 5 / frame 3 / ui 3 / entities 0）；
  发生数守恒复核：131 旧名 = 115 新 + 14 struct 声明 + 2 注释
- 门 154 PASS：构建 0 警告 + 1929 + **b2n1 ≡ b2n2 ≡ a1o1（v1.35.0
  基线）≡ 锚 全字节一致 168140 B** + 跨版 savenorm EXACT + 读档往返
  全状态恒等（dt = 30 步浮点累加 2e-17 内）

### B3（mob 族 → MobSystem）
- 11 成员收拢（m_mobTypes…m_spawnCounter，相对序逐字保持 = 零初始化
  重排，B2 同型；无类型前移需求 = struct 原位）；实例名 `m_mobs` 全
  src 冲突核实（MobRegistry 私有同名 vector = 另一类无遮蔽）；51 处
  调用点（entities 35 / init 14 / save 1 / ui 1）+ 2 跨文件注释；
  发生数守恒 65 = 53 新 + 11 声明 + 1 struct 内注释，逐 TU 对上
- 门 155 PASS：构建 0 警告 + 1929 + **b3n1 ≡ b3n2 ≡ a1o1（v1.35.0
  基线）≡ 锚 全字节一致 168140 B** + 跨版 savenorm EXACT + 读档往返
  全状态恒等

### B4（交互/面板族 → InteractionState + PanelState，B 段收官）
- 交互 11 成员 → `InteractionState m_interaction`（m_swingCooldown 原位；
  安全重排 = break 族 + stepTimer 自红线块后前移，11 成员相对序保持，
  全默认构造零跨读论证）+ 面板 8 成员 → `PanelState m_panel`
 （m_inventoryGrid 原位；openFurnacePos/furnaceUi 自 PerBlockStates 邻位
  移入 = 构造时点提前论证：furnaceUi 3 参构造只存引用零跨成员读 +
  红线 m_recipes < UI 三构造保持）；**Game 构造改 `m_panel(m_inventory,
  m_items, m_recipes)` 初始化器**（UI 取引用绑定自构造体移入 struct
  构造 = 绑定机制变更，行为零差由门 156 字节级 + 往返恒等证明）+
  panelOpen() 留 Game 层改读 m_panel.openPanel；93 处调用点
  （interaction 55 / ui 32 / render 8 / save 3；frame/init/streaming/
  entities 零触点）；发生数守恒 93=93 逐名对上
- 门 156 PASS：构建 0 警告 + 1929 + **b4n1 ≡ b4n2 ≡ a1o1（v1.35.0
  基线）≡ 锚 全字节一致 168140 B** + 跨版 savenorm EXACT + 读档往返
  全状态恒等——**B 段 4/4 收：4 域 struct 全部落位**

## 子相门协议（无像素门）
每子相：new ×2 + 基线侧 RAW 字节一致（tm_v136_base = v1.35.0 冻结）+
base18 锚连续 + 跨版 savenorm EXACT + 构建 0 警告 + 1929 套件；B 段每
子相附**声明序映射表**（构造序 = 初始化序 = 行为面）。

## 性能 A/B（UX-2 收官必做，发布前硬门槛）
<!-- old = tm_v136_base（v1.35.0）/ new = UX-2 终态；预期 = 同世界同成本
（纯重构零行为变更最强形）-->

## 偏离/记档
- C 方案（子系统对象化）= 矩阵外（立项需用户定案；流送域试点候选）
- game.hpp 成员平铺 → struct 收拢 = B 段逐步完成（本相结束前 hpp 仍
  1376 行平铺 = A 段零结构变更的代价）

## PITS
<!-- 子相执行中追加 -->
