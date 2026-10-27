# FLUID-4 倒向水面（v1.x 9/33，v1.9.0 候选，门 124）

v1.x 任务矩阵第九项（docs/BACKLOG.md，FLUID 行 1/3）。消费 P47 流体桶
记档偏离"倒水只能倒向方块受击面——对着水面/纯天空倒 = no-op（MC 对
水面的倒法未实现）"。"倒向水面"原始定义即此缺口，代理定案最小完整版
= **桶倒水命中水面**（MC parity）：满桶对着一格流动水（level 1-7）
倒 → 该格变 level-8 源，桶换空桶。

## 定案

- **纯函数 `bucketPourTarget`**（world/raycast，canPlaceAt/raycastFluid
  同款落点）：候选 = ① block 射线的受击面邻格（P47 放置约定——block
  射线**穿过流体**，1 深水池的地面受击使顶面水格成为候选 = "对着池子
  倒"成立）；② block 射线**什么都没打中**（超 block 射程的开阔水面）
  → fluid 射线自身命中的格。无 cross-fallback（block 命中恒优先 = MC
  桶对块面瞄准）。候选须过 `canPlaceAt`（Y 范围/实心/玩家重叠——与旧
  路径共享，旧行为位级不变）且满足其一：
  - (a) 空气且无流体 = 旧放置（**位级不变**）；
  - (b) 桶自身种类的**流动**格（level < 8）= 新的倒向水面（变源）。
  no-op（桶保留、点击 fall through）：已源格（level 8）/ 异种流体格 /
  实心 / 越界 / 玩家重叠。
- **Game 接线**（tryFluidBucket pour 分支）：`targetBlock()` +（无命中
  时惰性 `raycastFluid`）→ `bucketPourTarget` → 写入路径零改动
  （setFluid L8 + m_editedColumns 列保护 + Lighting::updateLocal +
  "splash" + 物品交换）。
- **零 format（19）/零 config/零内容/零音效**：水/熔岩桶对既有
  （P47/P51），流体 = 世界编辑（存档既有路径）。
- **范围外记档**：① 流动水舀取维持 P47 现状（记档称"MC 一致"，实际
  MC 可舀流动格——舀侧扩展另立项，本项名义 = 倒侧）；② 水桶对熔岩面
  = no-op（kind 不匹配；水-熔岩反应交互 = FLUID-5 行）；③ 倒出的源走
  正常 FluidSim（既有）。

## 实现

2 文件：`world/raycast.{hpp,cpp}`（bucketPourTarget 纯函数 + chunk.hpp
include 供 FluidKind）+ `core/game.cpp`（tryFluidBucket pour 分支重接 +
方法头注释同步）。

## 测试（1756/1756 = 1745 + 11，-Werror 0 警告）

RaycastPourTargetTest 11：旧空气邻格（位级 legacy）/ 倒向流动邻格 /
已源 no-op / 异种 no-op（水对熔岩拒 + 熔岩桶对熔岩收 = 种类正确性）/
实心邻格 no-op / 玩家重叠 no-op / 无 block 命中开阔水 / 无 block 命中
已源 no-op / 双无命中 no-op / Y 越界 no-op（顶行 +Y 邻格）/ block 命中
优先于 fluid 命中（无 cross-fallback）。

## E2E 门 124

**S1 = 第 124 次结构性免疫，免重锚**：fresh 60f 四方 ×2 字节一致
（new ≡ old v1.8.0 ≡ base18）+ savenorm EXACT。论证：无输入 = 无右键 =
tryFluidBucket 零执行；桶在存档默认库存中不在默认热栏（starter 槽
纪律）；渲染/世界零触碰。

**S2 = 单次右键输入跑**（受控平台场景：石台 y44 x22-27 z-9..-7 +
封源池 = 源 L8 (25,45,-8) + 流动 L7 (24,45,-8) + 6 石封【PITS 291
纪律：不封则源流扩散毁场景】+ 库存 slot0 = water_bucket）。瞄准几何
= CPU harness 实证（真实场景世界 + 真实 raycastBlock +
bucketPourTarget，kReach 5）：
- **主场景**：玩家 (27.5,45,-7.5) yaw +π/2（朝 −X）pitch −0.30——射线
  在 x≈25.4 进入 y45 水行，穿过源格 (25,45,-8) 与流动格 (24,45,-8)
  （流体对 block 射线透明），打**西封石 (23,45,-8) 的 +X 面** → 邻格
  = 流动格 → 倒水成立（bucketPourTarget 实证 = (24,45,-8)）；
- **S2b**：玩家 (22.5,45,-7.5) yaw −π/2（朝 +X）——射线打**东封石
  (26,45,-8) 的 −X 面** → 邻格 = 源格 (25,45,-8) L8 → no-op
  （bucketPourTarget 实证 = nullopt）。
- **输入协议**（PITS 295）：左点捕获（轮询 "Pointer captured" DEBUG
  日志）→ 单次右键；**存档状态 = 权威断言**（输入跑协议定位），截图
  ×2 RAW 尽力而为（左点裂纹帧差）；
- new ×2：流动格 (24,45,-8) → **L8** + slot0 = **bucket**（点击送达 +
  倒向水面生效，互斥终态）+ "poured water at (24,45,-8)" 日志行；
- old v1.8.0 ×2：流动格恒 **L7** + slot0 = **water_bucket**（P47 旧
  no-op 行为不变）；
- S2b new：玩家 (22.5,45,-7.5) 经东封石瞄准**已源格** (25,45,-8) →
  no-op：源恒 L8 + 流动恒 L7 + slot0 = **water_bucket 保留**（level-8
  no-op 守卫 = 桶不消耗）；
- ×2 RAW/侧 + A/B 截图 diff = 热栏图标带 + 池水面（L7→L8 水面 +1/8
  块）。

- 收集注：首轮批（03:2x）6 连败 = 断言串引号 bug（zsh 双引号截断
  `d["player"]` → NameError，PITS 纪律条）+ 黑窗；修复后 clean 二进制
  一次/三次内全收（s2_new_a i=1 / s2_new_b i=4 / s2_old_a i=3 /
  s2_old_b i=4 / s2b i=1）。
- A/B 截图 diff 19,819 px 分解 = 手持物品图标（右下角，水满桶→空桶）
  + 热栏槽 0 图标（3,203 px 底带）+ 池区（~11.7k px，流动→源水面
  +1/8 块 + 局部光照重烘，new 侧更蓝）= 全部为倒水直接后果，无系统性
  差异；左点捕获的 0.15 s 破坏进度（~4-7% 裂纹 <100%）不入存档、
  跨侧帧差 = 截图尽力项（存档 = 权威断言，输入跑协议定位）。

## 性能 A/B（发布前必做）

**PASS = 成本可忽略**（tm_v180_perf v1.8.0 插桩 vs tm_v190_perf
v1.9.0-dev 插桩，同 seed（perf_ab17 剥 fluids）fall 900f 三窗，PITS 15
纪律 = 每跑前重 cp 干净 seed）：
- **fluidCells 29803=29803 / 29800=29800（win2/3 逐位一致**——零
  worldgen 改动 = 同世界 oracle 兑现**）+ win1 51974 vs 51937
  （−0.07% = 流送填充期 real-dt 类 PITS 244/283，填充前沿采样噪声）；
- PIPE3 changesApplied 1404=1404 逐位 + tick med 6.8/6.9 ms 同档
  （win1 visitedChunks 261 vs 268 / fluidCellsSeen 42270 vs 43996 =
  填充窗时序噪声，settled 窗逐位一致已证世界同）；
- upd/render/swap 三窗全同档（med ±25% 内 = 环境噪声带）；
- 无输入 = 无右键 = tryFluidBucket 零执行 = 桶代码结构惰性实证。

## 坑位（PITS 追加）

- **PITS 295（右键只在指针锁定时执行）**：`handleInteraction` 在
  !locked 分支提前 return——裸右键被静默丢弃（SDL 收到事件、游戏层零
  反应，INPUT-DBG/RCLICK-DBG 双层日志实证）。输入跑协议 = 左点 →
  轮询 "Pointer captured"（DEBUG 级）→ **单次**右键；窗口归属用
  `windowactivate --sync`（raise+focus 不够——指针落 root 0x3f3 坑）。
  单击纪律：倒水后桶 = 空桶（collects 侧）——多一次右键 = 舀回刚倒的
  源 → 重试 = 整跑重来（新 cp 场景），不补点。
- **PITS 296（同槽双进程终档竞态）**：慢速探针跑未竟时下一轮复用同
  `--save` 槽 → 交错写档 = 读到另一进程终态（flow 7 vs 8 矛盾）。
  纪律：启动前 pgrep 验无残留 + 断言前验 'Game shut down cleanly'
  （无 clean exit = 槽内是场景文件，断言恒败且误诊"功能失效"）。
- 断言串引号纪律：zsh 双引号参数里的 python 键用**单引号**
  （`d['player']`）——双引号 `d["player"]` 会截断 zsh 字符串 = 断言
  NameError 恒败（门 124 首轮 6 连败真因，功能本身早已成功）。
