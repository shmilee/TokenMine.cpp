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

### B1-B4（per-block 族 / 流送族 / mob 族 / 交互面板族 struct 收拢）【待执行】

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
