# Phase V2-6 — 体验打磨批（v2.6.0，门 177 全 PASS）

> 2.x 第六项（UX 行 ④）。UX-8/9 同款多格批：四子项各消费一处记档
> 偏离，全为表现层/手感层。设计定案 = 11-03（/tmp/opencode/v26_design.
> md，范围权威）。预期**零 format（25 不动）/ 零 config 新键**——实际
> 零新键（未加 render.* 开关 = 无偏离报告）。

## 实现

### ① 第一人称手臂（「无手部皮肤——只渲染手持物品」记档消费）
- **几何 = 数据驱动**：玩家模型 `arm_r` box（data/player_body.json，
  V2-2 既有）re-center 到原点后走既有 box-model 管线
  （`makeMobModelGeometry` = chunk-mesher-verified UVs）→
  **Steve 皮肤 pa_arm tile 六面全贴，零新纹理**（tile 16×16：上 6 行
  衫袖 (52,148,152)、下 10 行肤 (164,140,116)——肤手 = 同 tile 下行，
  与第三人称臂同款映射）
- **门控 = 既有手持门三态矩阵化**：`firstPersonArmVisible(mode,
  locked, panel)`（core/first_person_arm.hpp 纯函数）= 指针锁定 ∧ 无
  面板 ∧ **仅 First**（ThirdBack/ThirdFront 隐藏 = 世界空间身体可见，
  V2-1 范式）
- **姿态**：臂中心 = 手持 offset（kHandOffset + 弓拉/进食通道调整）
  + kArmCenterDelta {0.12, −0.28, 0.18}（右、下、后于手），rotateZ(π
  + 0.5 rad)——**π 翻转使肤端朝持物端**（原始 tile 映射袖在上）+ 0.5
  rad 斜倾（顶端向左朝视野中心）。**静态无行走摆动 = 记档偏离**；
  弓拉/进食携臂微动（offset 平移继承）
- **无手持也画空手**（肤手 = tile 下行）；itemless/无 icon 无 model
  物品 = 臂保留、物品不画
- **懒构建**：`firstPersonArmMesh()` 首次锁定第一人称帧才建（无输入
  运行永不建 mesh = 结构性免疫：zero GL calls）；init 校验 `arm_r`
  box 存在（named-box 纪律，缺 = MC_FAIL 点名）

### ② mob blob 阴影（「无 blob 阴影 = 仍活跃」记档消费）
- **几何 = 纯函数**（entity/blob_shadow.hpp）：中心 = 实体底心正下方
  **底格顶面**（`blobShadowCenter` = floor(footY) + kMobShadowLift
  0.01 防 z-fight 抬升——站立实体 footY 整数 = 精确承托面；腾空贴下方
  格顶 = MC blob 观感，不悬浮脚边）；边长 = 1.5×模型宽
  （`blobShadowSide`，kMobShadowSideScale——猪 0.9→1.35 / 玩家 1.0→
  1.5 / chicken 0.45→0.675）
- **呈现**：世界空间扁 quad（unit quad 懒构建 `mobShadowMesh()`）双
  面（culling off 该 draw）、UI shader 纯色路径（outline 范式，
  u_useTexture 0）、半透明黑 **alpha 0.3**（kMobShadowAlpha）；blend
  pass 内（深度写关测开：山丘遮挡、地形承托、不遮蔽后续混合几何）
- **范围**：每 mob 一只 + **玩家仅第三人称**（`m_cameraMode != First`
  = 身体门控同款镜像；第一人称无自身阴影 = MC 观感，记档）
- **帧锁定零状态**：几何 = 实体当前位置的纯函数（render of frame N =
  f(N 的实体状态)），**无新帧状态成员**；无阴影可画 → 早退（mesh
  永不建 = 免疫）

### ③ 物品拾取磁力（「固定 1.0 格半径无磁力」记档消费）
- **纯函数**（entity/pickup_magnet.hpp，header-only = bow.hpp 范式）：
  位移 = f(距离, 帧, h) —— 水平径向速度 = kMagnetMaxSpeed 6 ×
  (1 − d/kMagnetRadius 2.5)²（**距离衰减 + 加速趋近**：边缘 0、越近
  越快，平方刻度远场缓近场快 = MC 手感）+ 侧向 **swirl**（周期
  kMagnetSwirlPeriodFrames 24 帧、幅值 = 径向 25% = 收紧螺旋；frame
  有界 modulo = kRipplePeriodFrames 纪律）；y 分量恒 0（磁力仅水平，
  落差仍靠走近 = 记档偏离）
- **帧锁定**：updateEntities 内**固定 kMobStepPerFrame 步长**应用
  （mob 同款帧步长纪律，hard rule 8；物品自身物理仍 real-dt 不变）
- **不穿墙**：掉落物步进过 `hasRoomAt`（MC 规则；受阻 = 留在原地
  下帧重试）；**插墙箭同款消费**（Phase 42 半径 1.0 拾取同步）=
  stuck 箭被同款磁力**拉出方块**（无 room 检查：卡住箭 AABB 本就在
  solid 内 + stuck 不做物理步）
- **拾取半径 1.0 / 延迟 0.5s / 入包规则零动**（磁力只把物品送进
  半径）；无 ready 掉落物/插墙箭 = 零执行（60f 基线零实体 = 免疫）
- 应用点 = merge pass 之后、item/arrow 拾取 pass 之前（同帧可被
  吸进而被拾取；确定性索引序不变）

### ④ 月相（billboard「无月相」记档消费）
- **相序 = 帧锁定派生**：8 相 = day cycle **累计天 index**（
  `DayCycle::dayIndex` = floor(start + total/len)，total 固定 1/60
  步推进 = 纯帧函数）**% 8**（`moonPhaseIndex/moonPhase`）；相序表
  `moonPhaseOffset(phase)`（纯函数，单测钉）：
  (1,0) 新月同心 / (1,0.25) 娥眉 / (1,0.5) 上弦 / (1,0.75) 盈凸 /
  (1,1.0) 满月相切 / (−1,0.75) 亏凸 / (−1,0.5) 下弦 / (−1,0.25)
  残月——x = 食相滑移方向（盈 = sprite +X 光弧在左 / 亏 = −X），
  y = 偏移量（月盘半径单位：可见光弧宽 = y×r）
- **呈现 = 零新纹理**：既有月 quad 上叠暗色「食相」quad（sky shader
  纯色路径同款 28px 圆盘，颜色 = 深夜天色 kNightSky (0.02,0.03,
  0.08) → 覆盖区读作天空 = 娥眉幻觉；沿 view-plane X 滑移（billboard
  自身轴，任意相机 yaw 读相正确）；同 moonAlpha 地平线淡出）
- **绘制序 = 食相先 / 月后（PITS 371 根因）**：两 quad 同在 r=800
  天空平面（相机 54 高 → 深度 ~746）；near 0.1/far 1000 的 24-bit
  深度在该处只分辨 **~160 块** → 食相后画 + 亚块 nudge 朝相机 =
  必被量化吞掉（nudge 0.05 实证：月恒全亮）；正解 = 食相先写深度，
  月后画在食相已写入处因**严格 LESS** 被拒 → 食相透出、月光弧
  （食相未覆盖侧）正常亮。食相 halo（超出月的圆盘部分）= 夜空色
  不可见，其天空平面深度写无害（后续 blend 段几何全在 y≤108 更近
  处）；地形遮挡照常（深度测开：近处山脊同时拒月与食相片段）
- **星星不动**（无星闪保持记档）；**首夜 = 新月**（day 0 的夜
  月被全遮 = 不可见，第 2 夜起娥眉 = 记档偏离：MC 无固定创世月相
  的近似）
- 正午基线月在地平线下（moon.y < 0 → moonAlpha 0 → **整分支不
  执行** = 结构性免疫）

## 变更文件
- `src/world/day_cycle.hpp/.cpp`：+dayIndex/moonPhase/moonPhaseIndex/
  moonPhaseOffset（纯函数 + 实例派生）
- `src/entity/pickup_magnet.hpp`（新）：磁力纯函数族
- `src/entity/blob_shadow.hpp`（新）：阴影几何纯函数族
- `src/core/first_person_arm.hpp`（新）：臂门控 + 姿态纯函数
- `src/core/game.hpp`：+firstPersonArmMesh/mobShadowMesh/drawMobShadows
  声明、PlayerBody.armMesh、m_mobShadowMesh
- `src/core/game_render.cpp`：drawHeldHand 重构（臂先画 + 空手画臂）、
  drawSkyObjects 月相（食相先/月后 + PITS 371 注释）、blend 段
  +drawMobShadows 调用
- `src/core/game_entities.cpp`：磁力 pass（item + stuck arrow）、
  drawMobShadows/mobShadowMesh/firstPersonArmMesh 实现
- `src/core/game_init.cpp`：arm_r box 存在性 init 校验
- `CMakeLists.txt`：VERSION 2.6.0（单点）+ 3 个新测试文件
- `tests/test_day_cycle.cpp`（+5）、`tests/test_pickup_magnet.cpp`
  （新 9）、`tests/test_blob_shadow.cpp`（新 3）、
  `tests/test_first_person_arm.cpp`（新 5）、
  `tests/test_debug_overlay.cpp`（F3 版本钉 v2.6.0）
- 文档：CHANGELOG v2.6.0 / BACKLOG V2-6 ✅ + 变更记录 / KNOWN_ISSUES
  四条（月相/磁力/阴影/手臂 = 划解 + 新偏离记档）/ PITS 371 /
  AGENTS 现状刷新 / 本文件

## 实现侧自证
- 构建 `-Wall -Wextra -Wpedantic -Werror` 0 警告
- **2157/2157**（2135 既有 + 22 新）
- banner v2.6.0（`--frames 60 --new` 实证）
- **60f 无输入截图 = base18 锚 179116 B 逐字节恒等**（md5
  0f510126140f7aebf6a2e3b2936534ba 双侧）= 四子项全惰性实证（
  locked-only / 无 mob 零绘制 / 无掉落物零执行 / 正午月不画）
- 功能像素实证（12s 天测试 config，**config/config.json 未动**）：
  - 月相：dayIndex 1 夜天顶 = 0.25r 光弧（左缘 3.5px 亮弧，余暗盘
    隐于夜空色）；dayIndex 3 夜 = 0.75r 光弧（中点行宽 10.5px，
    全盘高 26px 透镜形）——两相宽度与 `moonPhaseOffset` 表精确吻合
  - 臂：锁屏态右下 = 袖蓝 (52,148,152) 800px 斜带 + 肤手 (164,
    140,116) 竖带至石方块（slot 0 stone）持握端
  - 阴影：猪/牛脚下草地精确 ×0.7 变暗（lit (58,94,36) → (41,66,25)
    带 = 0.3 alpha 黑合成解析值）+ 玩家脚区 16800px **100% 无阴影**
    （第一人称自身阴影门控实证）
## 门 177（编排者执行；old 侧 = v2.5.0 = e317ec2 树）— 全 PASS

### S1 = 第 177 次结构性免疫 + base18 锚连续 179116 B 免重锚
- fresh 60f 四方（new v2.6.0 n1/n2 vs old v2.5.0 o1/o2）全 179116 B
  **RAW 全字节一致** + n1 ≡ base18 锚 + 跨版 savenorm **EXACT**
  （零 format——四子项全惰性：locked-only 臂 / 无 mob 零阴影 /
  无掉落物零磁力 / 正午月整分支不执行）

### S2 四子项判别全 PASS
- **手臂**（locked 态截图协议，鼠标-only——门内 XTEST 键盘失效
  PITS 368）：new-locked ×2 RAW 恒等（434606 B，确定性）/ locked vs
  free diff 存在（bbox (370,364)-(1343,744) 下半 = 臂+持物区）/
  **new vs old locked diff bbox (849,553)-(1198,744) = 349×191 右下
  象限（臂本体，旧侧有同款持物无臂）**——质心 (856,554) @ 1362×744
- **阴影**：mobs-on 360f new ×2 恒等 / **mobs-off new ≡ old RAW
  （179116 B，零 quad 免疫实证）** / mobs-on new vs old diff bbox
  (0,349)-(473,453) 垂直跨度下半 = 实体脚下地面影
- **磁力**（场景 = pork 掉落 1.41 块 @ pickupDelay 0，无输入帧锁定）：
  new ×2 终档恒等（剥 real-dt 火寿命 churn 类 PITS 244/283 后）+
  **pork 吸入拾取（entities 空 + inv pork 1）** vs old 侧
  **pork 原地不动**（无磁力，240f 终档同位）= 互斥终态
- **月相**（场景 = 存档 time 0.519 + 朝东 pose——PITS 372：载档 time
  压过 --time；低空月窗口 t∈[0.51,0.53] 东 / [0.97,0.99] 西，朝南
  默认 pose 永远拍不到）：new ×2 恒等（77683 B）+ **new vs old diff
  bbox (256,286)-(284,314) = 恰 28×28 px（kMoonSpriteSize 月盘）@
  预测位 (270,300)**——dayIndex 0 = 新月相 0 = 食相盘全盖月（新侧
  月暗）vs 旧侧亮月。8 相序列/娥眉几何/绘制序 PITS 371 = 单测钉 +
  实现侧 12s 天测试像素实证（dayIndex1 = 0.25r 光弧 / dayIndex3 =
  0.75r 光弧宽度与偏移表精确吻合）

### S3 套件
- 2157/2157（编排者独立复跑；2135 + 22 新：day_cycle 5【8 相 mod8 /
  偏移表逐格 / 盈亏镜像 / 天 index 边界含 60f 基线 dayIndex 0 钉 /
  实例随天推进】+ pickup_magnet 9 + blob_shadow 3 + first_person_arm
  5）

### S6 精确点名（真实二进制）
- player_body.json 删 arm_r box+swing → 「player body: the V2-6
  first-person arm requires a box named 'arm_r'」init 拒 ✓
- （磁力度量/阴影几何/臂门控矩阵 = 单测钉）

### 性能 A/B PASS 同世界同成本（用户硬要求，发布前）
- v2.5.0（tm_v260_perf = e317ec2 + g139）vs v2.6.0（tm_v260_perf_new
  = 最终工作树 + g139，双 27 标记）：fall 900f 无输入 seed 123456789，
  banner 双侧实证：
  - PERF-WIN 三窗全指标同档（upd med 0.67/0.68/0.66 vs
    0.67/0.64/0.68 ms；p95 窗间摆动无系统偏移；render med
    0.60/0.65/0.57 vs 0.59/0.60/0.63；swap 15.2 vs 15.2 vsync 主导
    同值；fluid/fires/spread/ents/stream 全同档）
  - **PIPE3-S0 changesApplied 三窗 max 9/3/1 双侧逐位**
  - **终档 fluids 18/18 + blocks 55/55 集合恒等**（同世界最强形——
    四子项无输入全惰性）
  - **mobs-on 阴影自成本**（双侧 900f）：ents 0.039/0.095/0.117 vs
    0.039/0.097/0.107 ms = 同档（阴影 quad ≈ 零增量）
  - **磁力场景自成本**（v2.6.0 only 900f）：ents 0.039/0.094/0.099 ms
    ≈ 既有实体物理同档

### 门内环境事故（非项目缺陷）
- **XTEST 键盘服务器级失效**（PITS 368）：09:05 起 xdotool 键【含
  F5】全被 X 服务器拒（dlopen libXtst 直调 XTestFakeKeyEvent rc=1 实证；
  鼠标 XTEST 正常；xmodmap/setxkbmap/xinput disable-enable 无效；
  XGrabKeyboard 四组合 BadParameter）——绕过 = 鼠标-only 协议 + 存档
  selected 预置 + SIGTERM 退出自动存档；根治 = 重启 X 会话（等用户）
- Awesome drawin 窗盖游戏窗中央 = capture 点击被吞（PITS 369）→
  capture 安全点 (X+300,Y+120) + windowraise
- 载档 time 字段压过 --time（PITS 372）→ 时刻场景 = 改存档 time

## 偏离（记档）
- 手臂**静态无行走摆动**（定案「静态（无摆动动画 = 记档偏离）」）；
  弓拉/进食通道携臂微动（定案「随手持物变换微动」）
- 阴影 = **方块扁影**（非 MC 径向渐变软影）；玩家第一人称无自身阴影
  （定案「MC 观感」）
- 磁力 = 水平径向 + 弦式 swirl 简化（非 MC 三维螺旋）；插墙箭被拉出
  方块（定案「同款半径同步消费」的必然结果）；磁力不穿墙（hasRoomAt
  保守整步拒绝）
- 月相 = **同半径圆滑移**（非 MC 椭圆明暗界线，无直线 terminator）；
  相序起点 = 创世夜新月（首夜月不可见）；无星闪保持
- 臂/阴影/物品手恒亮（baked skyLight 1.0 = mob 既有约定，夜间不随
  世界变暗——与 V2-2 身体同款）
- 零 format / 零 config 新键（未加 render.* 开关 = 设计「预期」达成，
  无偏离报告）
