# Phase v3.28.0 — V3-12a 多人设计定案（门 218 全 PASS，2026-11-07，纯文档版）

## 范围与定案

**V3-12 多人（矩阵最后项，唯一「超大」= 架构级）相位 12a = 设计定
案——立项出设计稿无代码**（BACKLOG 11-06 定案拆分同款处理 = 设计
先行；交付物 = 设计权威入仓 `docs/V3-12-MULTIPLAYER-DESIGN.md`
【侦察全文 390 行，代码引用 文件:行 实证】+ 本阶段文档 + BACKLOG
12b 行范围/门臂更新）。**v3.0.0 开启版同款**：纯文档 + banner
（CMakeLists 3.27.0→3.28.0，F3 首行文本面），零玩法/零模拟变更。

### 设计核心裁决（侦察全采纳 + 编排者裁定 2 点）

- **方案 1 = 主机权威 lockstep 定案**（方案 2 专用服权威状态同步 ❌
  ——确定性资产 ~0 利用 + 预测校正层项目零现有；方案 3 全 P2P ❌
  ——时钟源/权威分散，2 人 PoC 零收益）。项目确定性纪律 = lockstep
  免费午餐（BACKLOG 原话）：demo 内核（注入 + dts 唯一时间源 +
  pacing + sim 门）= 客户端循环现成形状，输入中继 = Input handle*
  收口第二消费方，世界传输 = 零（worldgen 纯函数同 seed 各机位级
  一致）。
- **时间源 = 主机每帧广播 clamped wall dt（raw f64 位）**——real-dt
  系统（玩家物理/粒子/饥饿/火倒计时/效果/进食/挖掘…）结构性零
  desync，无需转帧锁定；帧锁定系统随帧钟（既有低帧率语义 = 多人
  时钟语义）；全库 wall-clock 直读仅 2 处（Space 双击/库存双击）
  → m_simTime 化（solo 门分支位级）。
- **网络 = POSIX BSD socket TCP（零新依赖，SDL3_net 门机未装实证）
  / 无 net 线程（主循环帧头 poll）/ 协议 schema 定案**（HELLO 握手
  四指纹 {TM_VERSION, sim, assetFNV, simConfigHash} + window
  parity / WELCOME 世界 spec（fresh = 仅 seed 零字节）/ INPUT
  FrameState ~40B 每帧 / FRAME dt+全员帧态+周期哈希 / CTRL 断线；
  帧序律 N 帧 = {dt[N], FrameState[N−D]×全员}，D=2 统一延迟无预
  测（38-42ms LAN 定案可接受）/ 状态哈希每 30 帧字段清单定案）。
- **Player 多人化 = 多 Player 实例 + 双别名最小方案**（否决实体化）：
  `m_player` 保留 = primaryPlayer()（index 0 共享定义，R1 红线禁
  本地相对锚）→ 354 处引用零改动 + localPlayer() 给相机/HUD +
  10 Game 成员 per-player 迁移清单 + pass 归属二分表（mob 目标/
  生成锚 = primary 零改；物理/生存/交互 ×N 循环化）。
- **streaming 跨机一致性 = 2 处修正**（盘锚 m_player → primary；
  回退门本机 pool.pendingCount() → 共享账本 streamPending.size()）
  ——worker 生成纯 generator 位级一致，零世界传输。
- **12b 定案 = format 32 零 bump（主机单存档，远程个人态不落盘）+
  sim 2 不 bump（canary 4 EXACT 执法；输入路由条款只管回放语义）**；
  12b 玩法面 = 全 sim 覆盖（lockstep 验证需全 sim 面，半残 = 哈希
  面不全 PoC 失义）；12b 不做 = 大厅/聊天/重连/预测/跨维度/
  creative 多人/玩家碰撞（全 12c 候选，偏离 D1-D15 预列）。
- **编排者裁定 ×2**：① 12a 独立成版 v3.28.0（v3.0.0 开启版同款——
  纯文档 + banner，零玩法）；② **12b = v3.29.0 门 219**（侦察稿
  「门 218 预判」为 12a 未占门号时的编号，顺延修正）。
- 风险 R1-R20 / 偏离 D1-D15 全文见设计稿 §5/§6（主机单点/无预测/
  全局维度 = primary/creative 多人禁用/window parity 握手门/xpSeed
  撞流 kSaltPlayerXp[0]=0 位级技巧…）。

## 变更文件

CMakeLists.txt（VERSION 3.28.0）/ tests/test_debug_overlay.cpp
（banner re-pin）/ docs/V3-12-MULTIPLAYER-DESIGN.md 新（设计权威
入仓）/ docs/archive/phase-3.28.0.md 新 / docs/CHANGELOG.md /
docs/BACKLOG.md（12a ✅ + 12b 行范围/门臂 = 门 219）/ AGENTS.md。

## 门 218 结果

- **零代码变更面**：banner = F3 首行文本（base19 锚无 F3 = 零像素
  面）——S1 结构免疫第 99 次：≡ base19 逐字节 ×2 + savenorm EXACT
  （v3.0.0「banner = 第 71 次」先例同款）。
- **canary 4 ×2 RAW + savenorm EXACT**（sim 2 不 bump——纯文档零
  模拟面，执法同款）。
- **性能 A/B 同档**（用户「每版发布前必做性能测试」纪律的文档版
  形态；旧 = 3f9ae83 v3.27.0 + g139 vs 新 = 本树 + g139，OW 900f
  无输入独占跑——banner 实证 v3.27.0/v3.28.0 + 二进制差异仅 F3
  文本串【关时零顶点差】：PERF-WIN 三窗 upd med 0.563/0.663/0.649 vs 0.520/0.610/0.580 同档 + render 0.562/0.744/0.726 vs 0.543/0.682/0.602 + swap 15.3 同档 + changesApplied 9/3-2/1【win2 = streaming 边界 real-dt 域，门 207 先例带】+ 终档 savenorm EXACT = 零性能面实证）。
- **2964/2964**（banner re-pin 1 项，无新增——设计稿无代码无单
  测面）；`-Wall -Wextra -Wpedantic -Werror` 恒 0 警告。

## 下一步

**v3.29.0 = V3-12b 2 人 PoC（门 219）**：src/net/（协议 + socket）
+ player 多人化重构 + streaming 2 处修正 + simTime 2 处 + host/
client 双循环 + 断线移除 + 哈希交换 + CLI --listen/--connect/
--script；门臂 = 双进程 headless 同帧哈希 ×2（--demo-author 时间
线输入，demo 内核第四消费方）+ 断线臂 + S6 八面点名 + canary 4 +
S1 第 100 次结构免疫（多人 off 位级）+ 性能 A/B。其后可选 12c
（完整化：大厅/聊天/重连/N>2/per-player 维度…范围 PoC 后定案）。
