# PRE-2 边缘雾（v0.26.0，矩阵 28/32，PRE 行 2/6）

BACKLOG 原文："PRE-2 边缘雾 — 世界边缘雾，遮住流送悬空截面（渲染条）"。

## 问题

稳态下流送圆盘边缘（R_eff = min(view_distance, 64) chunk 列 = 16×R_eff 块，
可绘制到 kFar=1000）是原始 chunk 切面：玩家视角一条生硬世界边界
（"渲染条"）。流送是惰性的（kStreamTriggerDistance=1：玩家须在 missing
列 1 列内才 engaged），边界随玩家移动持续出现在视野边缘。

## 实现（最小完整版，代理定案）

- **距离雾 = basic shader**（世界/实体/粒子全部几何，不透明+透明两 pass
  同一 program）：`u_fogEnabled`（int，per-draw uniform 分支无 fragment
  分歧）/ `u_fogStart` / `u_fogEnd` / `u_fogColor`（vec3）。fragment 端
  线性 ramp：`fogT = clamp((v_viewDepth - start)/(end - start), 0, 1);
  rgb = mix(rgb, u_fogColor, fogT)`，门内条件 `u_fogEnabled != 0 &&
  u_fogEnd > u_fogStart`（退化范围双保险）
- **雾深度独立表达式**（basic.vert）：`v_viewDepth =
  -(u_view * u_model * vec4(a_position,1.0)).z`（右系 OpenGL 相机朝 −Z =
  硬性规则 5）。**gl_Position 表达式树一字未动**（位级红线：重排浮点乘序
  改变 ULP，可翻转共面剔除边/深度竞争像素；两树 1e-7 级差只喂美学
  ramp，跨 run 确定）
- **雾色 = 同帧 `m_dayCycle.skyColor()`**（= glClearColor 同值）→ 世界
  边缘溶解进天空，昼夜/晨昏自动跟随，零额外状态
- **雾程纯函数** `src/renderer/fog.{hpp,cpp}`（新文件）：`end =
  min(farPlane, streamRadiusBlocks); start = kFogStartScale × end`
  （kFogStartScale = 0.75f 命名常量，零 config）；世界常量 kFar /
  kStreamCapRadius / kChunkSize 由 game.cpp 传入不复制
- **game.cpp render() 接入**：共享 uniform 块（u_tileSize 之后）4 次
  setUniform/帧 = 零 per-draw-call 成本；`render.fog` getBool 代码默认
  **true**（fire.particles / mobs.limb_swing 先例）；`fogOn = config &&
  end > start`。流送代码零改动
- **雾程推导**：shipped vd80 → min(80,64)=64 列 ×16 = 1024 块 > kFar
  1000 → **750/1000 块**（盘缘外 24 块全灭 + 内 250 块过渡带）；vd8 →
  128 块 → **96/128 块**（恰覆盖 201 列盘悬空切面环）

## 内容 / 格式

零 format（19）/ 零内容 / 零音效 / 零 worldgen / config.json 零改动
（render.fog 走代码默认 = fire/mobs/beacon/save 节先例）。

## 测试

1621/1621（+11）：FogRangeTest 9 例【ShippedVd80Cap / Vd8 / Vd16 /
MidRadius / StartIsExactlyThreeQuartersOfEnd（6 半径位级 ==）/
EndAtMostMinOfRadiusAndFar / FarPlaneSmallerThanRadius /
MonotonicInRadius / ZeroAndNegativeRadiusDegenerate】+ FogConfigTest 2
例【MissingKeyDefaultsToOn / ExplicitFalseTurnsOff】。`-Werror` 0 警告；
双 shader glslangValidator GLSL 校验通过。

## 门 109

- **S1 PASS = 第 109 次结构性免疫（10-26 07:19，免重锚）**：fresh 60f ×2
  RAW 一致 + vs base18 字节一致。论证成立：60f 无输入跑流送永不
  engaged（PITS 261）= 世界恒 16 列（64 块）≪ fogStart 750 块 = 雾区
  零地形。
- **S2 PASS（10-26 10:32，三方雾 A/B，1500f ×2/侧）**：
  - 场景 = `g109_fogscene_h2.json`：玩家 (-23.5, 56, -23.5)（demo 角列
    (-2,-2) 顶上的 y55 高地，邻 missing 列 → 帧 0 流送 engaged，~22f
    Streaming complete 34/34 列 272 chunks，keepRadius=3 = 盘半径 →
    收敛后零驱逐 = 静态态）+ config vd3（fog 36/48 块 = 盘缘雾带）+
    mobs off。相机在高原顶平视远高原顶面 + 48 块盘缘切面 = 雾带可见。
  - 断言全过：old(v0.25.0) ×2 / new-off ×2 / new-on ×2 RAW 一致 +
    **old ≡ new-off 字节一致（代码级免疫 = 雾 off 路径位级恒等实证）**
    + **on vs off diff 5409 采样 px 限 x[150..1216] y[382..572]
    （地平带下方，近景零 diff）**（与 300f 手动探针 5409 同值 = 可复现）
    + 六存档 savenorm 全等（纯渲染层存档零 diff）。
- **S6**：无（零 parse 面；getBool 共享语义 2 例单测钉死）。

## 记档偏离

1. 太阳/月亮/星星（kSkyDistance=800，sky program）**不雾化**——天空即
   雾色本体，MC 同款（太阳保持清晰）
2. 仅线性距离雾：无高度雾 / 无 per-material 雾；fogStart 比例非 config
   （kFogStartScale 命名常量，可调性留后续 = 矩阵外）
3. `render.fog` 默认 true（PRE 功能默认开）；off 对照 = scratch config
   false（ENT-1 config 三验同款协议）
4. basic program 承载全部世界+实体+粒子几何，雾统一生效（手持/近处实体
   viewDepth ≪ fogStart → fogT 钳 0 → mix(x,y,0.0)=x 对非负色逐位精确 =
   启用时近景像素也 bit 不变）

## E2E 场景关键发现（PITS 候选）

**无输入出生点跑永不流送**（kStreamTriggerDistance=1 + 初始 4×4 demo 区
kDemoChunkMin/Max=-2..1：出生点在中心，离 missing 列 2 列 → 触发条件
`nearestMissingSq ≤ 1` 永不满足 → 世界恒 16 列 64 块，任何 view_distance
同）。首轮 S2 用 --new 出生点场景 1500f ×2 = 盘永不填满 → 雾带零几何
→ diff=0 误判（on/off 全同）。修正 = 场景存档把玩家放在 demo 区**缘列**
（邻 missing 列 → 帧 0 engaged，"Streaming complete: N/N columns" info
日志核收敛）。**收敛后零驱逐的条件 = keepRadius ≥ 盘半径**（vd 场景
config 的 view_distance = 盘半径 → keepRadius = view_distance = 盘 ✓；
但 **vd2 例外：keepRadius=2 < demo 角列 d²=8 → demo 角被非确定性驱逐**
（worker 集成实时锁定，PITS 261 次坑 B）→ ×2 RAW 破。最终定案场景 =
**h2**：玩家 (-23.5,56,-23.5)（demo 角列 (-2,-2) 顶上 y55 高地，触发 ✓）
+ vd3（盘 34 列 ~22f 收敛，keep=盘 零驱逐，fog 36/48 块 = 盘缘雾带）+
相机在高原顶（**低地相机的雾带全被近处高原+树冠遮挡** = 次坑 A）→
diff 5409 px 可复现。另：fire remaining = 真实 dt（PITS 244）→ 火区
（本场景在镜头后）的块状态跨 run 微漂 = 存档比对需剥 fires remaining。

## 性能 A/B v0.25.0（发布前必做，10-26 10:3x-10:4x）

fall 900f 三窗：**fluidCells 123879/167781/236573 双侧逐位一致（同
世界）+ 重跑对全指标同档**（upd 47.4/47.8/48.2 vs 47.5/49.3/49.6、
render 0.328/0.483/0.630 vs 0.316/0.466/0.629、swap 0.427/0.466/0.457
vs 0.431/0.477/0.474、stream 1853/2134/2419 vs 1802/2136/2477 = ±3%
级；首轮 win1 upd/stream +24-31% 偏 = 执行序环境噪声，重跑对称性证伪
= 非回归，PRE-1 同款纪律）= **成本可忽略**（雾 = 每 fragment 一次
clamp+mix，仅雾带内像素实际混合 + 每帧 4 次 setUniform；无输入 fall
场景雾带外 = 分支惰性）。
