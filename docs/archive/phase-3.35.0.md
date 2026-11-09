# Phase v3.35.0 — V3-16d 扩展指导 demo 族：⑦ 皇宫（门 225）

> 3.x「世界扩展」V3-16 相位 D/7（build 章节首支）。内核零变更（16a 的
> scene/panel_click/索引全复用）——本相位 = 内容（1 scene + 1 demo + 1
> index 条目）+ 工具链增量（demo_author FLY/walk_leg(fly)/sweep_gradual/
> fly_toggle + demo_fly_patch dmo 后贴器）。

## 实现

### scene guide_palace（mk_scene 手术，幂等）
21×18 扩台平台（x0..20, z-13..4, 顶 y54）于**正午**（time 0.25）+
player (8.5,54,2.5) yaw 0.0 pitch -0.1 **creative**（scene flag——
即放即挖无消耗无掉落）+ 8301 edits（平台铺石 + 平整）。

### demo guide_palace（19536f = 05:25，1230 actions + 9 B toggle 后贴）
**定案结构（v11 定案；v1-v10 十版迭代，门内捕获全 PITS 416 十三坑）**：
- **P0 开场**（0-640）：f0 捕获 + **入口 B @f20（fly OFF——scene
  creative = 入场飞行，V3-11a；PITS 416① 的 v6 根因）** + 扫视。
- **C 第三人称**（600）：建造全程观众视角（C 单按，aim 射线自眼位不变）。
- **t1 stone_bricks 46 + t2 terracotta_yellow 46**（双层围墙，门洞
  x8/x9 双层开）：地面行走放置（v16c 验证域 4.3 b/s 精确 track）+
  绝对格 aim（漂移无关）。
- **四角塔 t3/t4 ×4 = 8**（**垂直-only 角循环**——脚手架 step-up 链 =
  死路 PITS 416③，水平飞行腿 = 1-m 损失死路 v8c 实证）：×4
  （SW→NW→NE→SE）：B 单按 fly ON → 45f 升 3.0m 精确（脚 57.0 眼
  58.62——v7 的 65f 眼 59.95 时 8 aim 全 4.12-4.15m 超 kReach 4.09 =
  0/8 门内捕获反解）→ aim 目标 t2 顶面/t3（aim 点在面平面上 = 命中
  与眼高无关，8 aim 逐一射线核证 2.2-3.0m 余量 ~1m）→ B 单按 fly OFF
  → 地面走到下一角（9/16/9-m，0.1-m 裕量墙线 = 92/92 实证域；walk_to
  2-m 分块绝对重锚）。
- **地板环 38 terracotta = CUT（v10 简化定案，偏离记档）**：走位漂移
  （reg loss 0.02-0.07m/leg = fps 函数）在开放路径累积 0.5-1.1m = 超
  1-m 格 aim 容差 0.5m（v8b 北 x3 落 x4 实证）+ 破 0.1m 放置线裕量
  （v8c 东列 0/7 / v9 级联出平台实证）；预计算输入无法自校正（ping-
  pong 再锚定 = 固定短停抵消 = 无效，核证）。皇宫 = 100 块（92 墙 +
  8 塔）视觉核心不变。
- **2 航拍展示**（墙成 = SW 循环内含 / 塔成 = SE 循环内含）：look_at
  中心 + 360° 渐进环绕（12 步 400f = 观众方位锚点；sweep_gradual 新
  DSL）。NW→NE 走线 z-12.0 外圈北绕行【v9 门内捕获：z-10.5 线在 t1
  北墙 z 带 [-11,-10) 内 = 16-m 墙隧道卡死；v10 门内捕获同族孪生：
  z-9.0 线 AABB z[-9.4,-8.6] ∩ 东墙 (15,54,-9) z 带 [-9,-8) 0.4-m =
  16-m leg 卡 x14.6 → NE/SE 塔 0/4 + tour 级联出平台——内圈 z 线 +X
  走必撞东墙（x15 连续带），环外 z-12.0（北墙带外 0.6-m / 平台北缘外
  0.9-m / x0.5+x16.5 端头 = 92/92 实证墙线）= 唯一解】。
- **结尾四面巡视**（东/北/西/正南门 + 2 扫视对）+ 收束。
- **飞行 toggle = gamepad B 后贴**（demo_fly_patch.py：side-car 9 tap
  [20, 11367, 12129, 12335, 12657, 13058, 13380, 13586, 14348] → 18
  gamepad_button 事件；timeline 词表
  拒 gamepad = author v1 键鼠面，dmo 事件层合法；字节 parity =
  pinned gzip + sorted JSON + no-op 自校每次断言，PITS 416⑥⑧）。

**终态钉**：100 块（t1 46 y54 + t2 46 y55 + t3/t4 8 y56/57）+ 零
stray（y≥50 名称比）+ 平台零挖（y50-53 完整）+ 零实体（creative 无
掉落）+ 终姿 (8.5,3.5)±1.5 y54±0.2。

## 测试
2998/2998（banner re-pin 1，内核零新增单测——内容相位先例 16b/16c）。

## 验证（门 225）
- A: guide_palace ×2 RAW + savenorm EXACT + check_palace（100/100 +
  零 stray + 平台零挖 + 终姿）。
- canary 4 ×2（example/tutorial_dig/tutorial_build/tutorial_fly，
  LEN+500，sim 2 不 bump——B toggle 仅本 demo，canary 零游戏杆事件）。
- S1: f60 ×2 ≡ base19（225272 B，第 106 次，banner 3.35.0 零像素）。
- 性能 A/B：旧 = 7e4a1a2 v3.34.0 worktree + g139 双树 27 markers，
  OW 900f 无输入三窗 upd/render/swap 同档 + changesApplied 9/3/1
  （old1 win2=2 = 旧树自身 run-to-run real-dt 域，门 207 先例）+
  **终档跨树 savenorm EXACT ×2**（内容相位结构性零成本最强形）。
- index build ⑦ seq1 + init boot 交叉检查（5 条目全量）。
- 实证：A/B 两臂终位逐字 (8.55,54,3.8) yaw 12.566（= 4 扫视对 2×2π
  之和，逐扫视恒等）+ 塔 8/8 f14400 检查点（v11 风险段前置确认）。

## 门内捕获（PITS 416 十三坑）
① creative scene = 入场飞行（applySave setFlying(true)）+ 双击墙钟
窗 flake → 入口 B @f20 fly OFF + 全 B 单按 toggle（v6 airwalk 15m
漂移实证 / v4 侥幸链）
② 飞行 7.12 实测 vs 6.88 名义（自锚定无 margin）
③ 脚手架死路（S2 AABB 恒重叠 + 角落 tie + 即挖连锁 2 深坑）→ 飞行悬停放
④ 帧停诊断法 + autosave 120s 中途覆写陷阱
⑤ scene inventory 严格对象形
⑥ dmo 后贴字节 parity（pinned gzip + sorted JSON + no-op 自校 + 文件序保持）
⑦ demo 玩家通道纯注入（用户鼠标 = audience orbit 仅渲染；唯一例外 = 帧节奏）
⑧ checker stray = 名称比较非存在性（air→block 替换面）
⑨ 飞行 aim 双坑（kReach 4.09 超 6cm 静默 0 放置 + track 飞行高度不随
  fly 变 = pitch 全错，v7/v8b 各 0/8 实证）
⑩ 游戏窗焦点 = 用户在机的演示臂杀手（ESC 暂停签名 + 关窗 rc 0 假健康
  → isolate_win.sh + 每臂 grep "Demo replay finished"）
⑪ 行走漂移 = fps 函数（reg loss 0.02-0.07m/leg）+ 地板线 0.1-m AABB
  裕量死局（v7 东列 0/7 / v8b 北 x3 落 x4 实证）
⑫ 走线在墙体内 = 墙 1-m 厚双向带（v9 z-10.5 北墙隧道 + v10 z-9.0
  东墙隧道双门内捕获 → 环外 z-12.0 定案，逐线三轴核）
⑬ 终态 checker 集合成员核对齐元数（3-tuple vs 4-tuple 恒 not-in =
  100 格全列 stray 假阳性 ×6 版）
+ 门内补记（pkill -f 自杀自匹配）

## 偏离
- 飞行建造（MC 创造无限制）= 展示目的，观众方位锚点 2 航拍。
- 15×11 皇宫 = 项目内原创设计（非 MC 建筑复制）。
- 门洞 2 格（MC 1 格可过，2 格 = 放置线容差面）。
- **内圈地板 38 块 = CUT（v10 用户在场定案简化）**：走位漂移与 1-m
  格 aim 容差的根本冲突（⑪⑫ 族），教学价值低，不进 3.x；皇宫 =
  100 块（92 墙 + 8 塔）视觉核心不变。
