# HISTORY.md — 2.x「上手与体验补全」期全史（v2.0.0 → v2.10.0，已归档）

> 11-03 收官归档。2.x 期 = 1.x 收官里程碑（v2.0.0 文档整理）后的
> 「上手与体验补全」期：V2-1~V2-10 十项任务全部完成，八项
> （V2-3~V2-10）= 11-03 用户定案执行矩阵，每版 = 阶段流程
> （设计定案 → fixer 实现 → 编排者独立核证 → E2E 门 → 性能 A/B →
> 文档 → 显式路径提交 → 中文阶段报告）。逐版细节 = 本目录
> `phase-2.x.0.md`；逐版明细 = `CHANGELOG.md`（同目录）；任务表 +
> 变更记录 = `BACKLOG.md`（同目录）。

## 编年

- **v2.0.0（11-02）— 1.x 收官里程碑**（无新玩法）：1.x 任务表
  40/40 + UX-8g/UX-9 实机报障 4 项全完成（v1.50.0 = 最后 v1.x）+
  扫尾三件事（1.x 文档归档 archive/v1x/ 181 份 + HISTORY 全史 /
  第三人称准星不压头 = MC 同款轨道点沿相机 up 抬高 0.4 块 /
  活文档刷新 2.0 状态）。门 171 全 PASS。
- **v2.1.0（门 172，提交链 → 6a42b30）— V2-1 第三人称前视角**：
  C 键三循环（First/ThirdBack/ThirdFront），前视 = 后视同款数学
  在翻转角下执行（纯函数零改动），aimReach = D+kReach。
- **v2.2.0（门 173，d9d62b0 + 65ca4e7）— V2-2 玩家皮肤 Steve 化**：
  模型 per-face 纹理**通用能力**（MobBox faces 段，既有 mob 位级
  不变红线）+ Steve 脸/衫/裤/靴 7 tile（图集零移位免重锚）+ 脸
  8×8 像素。
- **v2.3.0（门 174，9b8305b）— V2-3 教程系列化（农耕 + 厨房）**：
  tutorials/ 多文件名序 = 系列序 + format 25（三向键纪律）+
  pickup./smelt. 帧锁定事件 + T 跳当前系列；门内抓到并修复 UX-7
  潜伏 bug（recipe 行合成漏发 craft 事件，PITS 360）。
- **v2.4.0（门 175，3b88e61）— V2-4 天气（雨/雷）**：core/weather
  纯状态机（推进式流首段恒 CLEAR = 60f 基线免疫根；雷独立 salt）
  + rain shader 静态 300 滴 + 天空 dayFactor 单收口（RAIN ×0.85 +
  雷 5 帧脉冲）+ 交互四件 + render.weather strict 默认 true（off =
  v2.3.0 位级恒等，门内三方恒等实证）。
- **v2.5.0（门 176，e317ec2）— V2-5 新 mob 猪**：纯数据零核心代码
  验证「加 mob = JSON + 纹理」+ V2-2 per-face 首个 mob 消费
  （pig_face 显式 8×8）+ pork 食物链 + 图集 99 零移位 + 字体 423。
  门内环境事故 PITS 368-370（XTEST 键盘服务器级失效等）。
- **v2.6.0（门 177，cd4c62f）— V2-6 体验打磨批**：第一人称手臂
  （arm_r 数据 box 零新纹理）+ mob blob 阴影 + 拾取磁力（帧锁定）
  + 月相（8 相纯函数，绘制序食相先/月后 = kSkyDistance 平面
  24-bit 深度 ~160 块/档，PITS 371）；四记档偏离消费，零 format。
- **v2.7.0（门 178，9869617）— V2-7 材料补齐批**：planks 块化
  （既有 tile 零新纹理 + 8 配方按名零改动 + 同名物品文件点名拒）
  + gold/diamond_block（beacon_base 入列）+ spider 掉
  string/spider_eye ×0-2（纯收集记档）+ **块级 per-face 纹理通用
  能力**（无 faces 位级不变红线，首消费 furnace_front）；核心唯一
  新增 = faces 段，其余全数据驱动。门协议发现 PITS 373/374。
- **v2.8.0（门 179，38a778a）— V2-8 sky rescan clobber 修复**：
  LGT-1 记档潜伏 bug 定向消除（天花板骑跨光照 footprint 边界 →
  stale-dark/stale-partial 暗带）= sky BFS 种子盒 +1 块 shell 环
  （rescan/BFS 写界/独立性判据/confluence 零改动——shell 读 = 既有
  1-chunk 边界读环算术实证）；TDD 双回归（pre-fix 2464/544 diff
  格复现）+ CPU 探针 + E2E slab 无输入流送判别。环境事件：机器
  重启 /tmp 重建 + **XTEST 键盘恢复**（PITS 368 根治 = X 会话
  重启）+ 锚重导（PITS 375 tile 几何协议）。
- **v2.9.0（门 180，08466fe）— V2-9 step-up quirk 修复**：
  player/mob 共享 Entity::stepUp 两 quirk 定向消除——① +X/+Z 恰
  1 格宽台阶踏不过（front 格对称化）② 非整数脚高巧合对齐凭空
  抬升 1.0（无状态真阻挡验，严格不等式）；整数脚高位级不变红线；
  TDD 3 新 + 1 反转。门 180 S2 = 1 格台阶 E2E（旧树 x15.700 精确
  clamp 实机命名复现 vs 新树踏过；键盘协议回归）。
- **v2.10.0（门 181，3a5504e）— V2-10 网格修复批（收官前最后一
  项）**：① 叶子-叶子内部面剔除（块属性 cull_same_id 严格布尔，
  leaves 族 3 块首消费，mesher greedy pass 同 id 剔面纹理选择前，
  glass/水/cross 族红线零触碰）② alpha cutout discard = 零代码
  确认（PRE-5 桶无关 discard 普查全覆盖 + 普查单测钉）。
  **base18 第 2 次重锚 = 178148 B**（diff 4185 px 全叶冠区零泄漏
  论证，Phase 18 先例）。

## 收官判据回顾（11-03 三期分工定案）
- 2.x 矩阵 V2-3~V2-10 八项**全 ✅**（V2-1/V2-2 = 定案前用户实机
  诉求项，同门同流完成）；各版门 172-181 全 PASS；**每版发布前
  性能 A/B = 用户硬要求，八版全执行**（全同世界同成本：
  changesApplied 逐位 + 终档集合恒等 + PERF-WIN 全指标同档）。
- 三期总量判据：2.x = 10 项（V2-1~V2-10，含定案前 2 项）< 
  pre-v2.0 的 78-84 ✓（用户采纳判据满足）。
- 测试增长：2075（v2.0.0 时点）→ 2181（v2.10.0）= +106（各版
  只增不减）；format 25（V2-3 起未动）；PITS 355→375（门内发现
  全录）；base18 锚 17 次过门 + 2 次重锚（重启重导 179116 B =
  原锚字节数吻合连续性 / V2-10 叶冠 178148 B 论证重锚）。
- tag/release 政策：v2.1~v2.9 未挂 tag（用户「立项时问」未决）；
  **v2.10.0 = 2.x 系统收官 release tag**（编排者按收官授权自主
  定案，2.x 收官报告说明——用户可推翻）。

## 环境大事记（门内事故族，全录 PITS）
- PITS 359-367（门 174）：E2E 输入协议族（warp 原点/帧预算/槽名/
  饥饿满吃 no-op/燃料判别…）
- PITS 368-372（门 175-177）：XTEST 键盘服务器级失效（11-03 重启
  后根治）/ drawin 窗盖中央 / mob 不持久化 / kSkyDistance 深度
  量化 / 载档 time 优先
- PITS 373-375（门 178-179）：敌对生成双门 / hold-swing 帧预算族 /
  WM tile 几何锚协议
