# Phase V2-1 — 第三人称前视角（v2.1.0，门 172 全 PASS）

> 2.x 首个功能项（docs/BACKLOG.md v2.x 表 1/∞）。用户 11-02 实机诉求：
> 「第三视角下，我能看到自己的前面。当前我只能看到后背」= MC F5
> 前视角。

## 设计定案（编排者核全部调用点后派单）

**核心 = 前视是后视同款数学在翻转视角角下执行**：相机放玩家前方、
看向玩家 ⇔ 相机视角 yaw' = yaw + π、pitch' = −pitch，此时
`−forward(yaw',pitch') = +forward(yaw,pitch)`（玩家前方）。
`resolveThirdPersonCamera` / `thirdPersonCameraOffset` /
`thirdPersonOrbitCenter` **零改动**（签名本就吃 yaw/pitch，agnostic）——
翻转只发生在调用点。up_cam 对翻转不变（up_cam(yaw+π,−p) =
up_cam(yaw,p) ± 1ulp）⇒ 轨道点仍用玩家角 = 同款 0.4 抬升
（v2.0.0 扫尾②）⇒ **头同样在准星下方 29-96px**（前视准星不压脸）。
遮挡收敛/穿流体/embedded 三分支对前视自动对称生效（同一段几何，
关于轨道点镜像）。

## 实现（f3dc85e）

- `CameraMode` 尾追加 `ThirdFront`；**C 键三循环 First → ThirdBack →
  ThirdFront → First**（MC F5 序，locked-pointer 门控不变）
- 新纯函数 `thirdPersonFrontAngles(yaw, pitch) = {yaw + kPi, −pitch}`
  （third_person.{hpp,cpp}，campfire 范式；kPi 全仓无既有命名 float
  常量 = 新增）
- game_frame 相机块三分支（ThirdBack/First 零动 = 结构免疫根；
  ThirdFront 新分支覆写 m_camera.yaw/pitch 后走既有 resolve）
- **瞄准（UX-8g 眼位射线）**：origin/方向零动——前视方向 =
  `m_camera.forward()` = −f 玩家向 = 屏幕中心向（自动翻转）；
  新 `aimReach()`：前视 = kThirdPersonDistance + kReach（相机在眼前
  D 块 = 该视角的玩家视觉眼，眼射线补 D 保 MC 完整有效射程 = 玩家
  身后 5 块带），First/ThirdBack 返回同一 constexpr kReach = 位级
  不变——4 个 reach 消费点（targetBlock/targetMob/tryFluidBucket×2），
  fireBow 投射物零改动
- `drawHeldHand` 门控 `== ThirdBack` → `!= First`（前视隐藏第一人称
  手臂 = 必要追加；First/ThirdBack 逐位等价）；`drawPlayerBody` 现
  门控 `!= First` 前视自动渲染 = 功能本体零改动
- +4 单测（翻转角钉 6 朝向 / up_cam 翻转不变 / 空世界解析恒等
  bit-exact + 后视镜像 1e-5【~20-40 量级 ulp 约定】/ 墙钳止镜像到
  +f 侧 = 关于轨道点精确对称）

## 门 172（编排者执行；old 侧 = v2.0.0 = a6843d8 = tm_v210_base）

- **S1 = 第 172 次结构性免疫 + base18 锚连续 179116 B 免重锚**：
  fresh 60f 四方 ×2（new v2.1.0 ×2 / old ×2）全字节一致 + 跨版
  savenorm EXACT（无 C = First 恒走 legacy 路径，前视代码零执行）
- **S2 = 三模式矩阵 PASS**（同一 fresh 场景 × 3 模式 ×2 跑）：
  - a（First 未捕获）×2 RAW 179116 B 且 **a ≡ S1 n1 字节一致**
  - b（后视 c）×2 RAW 281133 B / c（前视 c c）×2 RAW 153129 B
  - **三模式 savenorm 两两 EXACT = 相机会话态不入档 + 同世界**
  - 皮肤判别（双色调：亮 (224,178,132)±15 + 0.6 面阴影 (137,109,81)
    ±15）：**a = 0**（无身体）/ **b = 10,151**（后脑 + 头顶 cap 亮色）/
    **c = 10,125 全阴影调**（脸 = ±Z 面 0.6 shade 朝向相机）——前视
    看到的前侧面实证；身体带均在中心下方（准星不压头 = v2.0.0 扫尾
    ② 继承）
  - 世界朝向判别：b vs c 中景带（排除身体列）diff 10,674 vs 同色
    1,326 = 两侧看相反地形（后视 −Z 草地 / 前视 +Z 天际）
  - 六跑玩家 pos/yaw/pitch/hp 全精确零漂移
- **S2b = 前视挖矿互斥 PASS**（受控平台：地板石 y53 + 内腔清 y54/55
  + 靶石 (0,55,5)【玩家身后 4.5 块，+Z】+ 前墙石 (0,55,−4)【面前
  3.5 块】+ 石镐 slot0，pitch 0 水平）：
  - f（前视 c c + L hold 0.6s）×2 RAW 141792 B：**靶 (0,55,5) 破
    + 墙在** = 前视射线方向 −f + aimReach 9（4.5 < 9）实证
  - b（后视 c + 同 hold）：墙 (0,55,−4) 破 + 靶在 = **互斥**
  - 首轮 f 侧失靶根因 = 场景 pitch −0.12 经前视翻转后射线仰 6.9°
    从靶上方过（场景设计错非代码错，pitch 0 重跑定案）
- **S3** = 2079/2079。**无 S6**（零 parse 面——纯几何常量 + 纯函数 +
  枚举尾追加）
- **性能 A/B v2.0.0 vs v2.1.0（发布前硬门槛）PASS**：fall 900f 无
  输入 First——**首轮 win2 changesApplied 2 vs 3 偏置重跑证伪**（重跑
  9/3/1 逐位 = PITS 283 类同二进制 churn 相位）+ PERF-WIN 三窗
  render med 0.58-0.65 全同档 + 终档 fluids 18/18 + blocks 55/55
  集合恒等 diff 0 = 同世界同成本（前视代码 = ThirdBack 分支外的惰性
  路径，无输入零执行）

## 偏离/记档

- 前视瞄准方向 = 屏幕中心向（−f 玩家向）= 玩家**身后**区域（MC F5
  前视同款语义：看自己的同时瞄向后背方向）；reach = D + kReach 使
  身后有效带 = kReach 5 块（MC 前视惯例：完整射程）
- 前视隐藏第一人称手臂（drawHeldHand `!= First`）；2D 手持角标 HUD
  层照画（UX-4 既有定案）
- up_cam 翻转不变性 = ±1ulp（float sin(y+π) vs −sin(y)），门 S2 实证
  无视觉影响
- 2.x tag/release 政策未定（10-31 政策 = 系统收官挂 tag）：v2.1.0
  暂不挂 tag
