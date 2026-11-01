# PRE-3 水面波纹（v0.27.0，矩阵 29/32，PRE 行 3/6）

BACKLOG 原文："PRE-3 水面波纹 — 水面波纹动画（渲染条）"。

## 实现（最小完整版，代理定案）

静态水面 → 帧锁定确定性 shimmer：

- **原理**：水纹理 = `generate_block_textures.py` 的 `water(x,y)` = 纯白
  噪声 (47,119,189)±10（per-pixel hash，16×16 tile，GL_NEAREST）——采样
  窗任何亚 texel 位移都改变所采 texel → **水顶面 UV 逐帧平移 = 水面
  shimmer**（MC 观感最小实现，无新纹理需求）
- **水顶面编码走既有 tiled 采样路径**（门 110 S2 首轮实测修复后的定案）：
  `emitFluid` 水顶面 4 顶点 = a_uv 存 uvMin（tile 原点）+ a_uvLocal 存
  角内坐标 (0|1,0|1) + a_tiled=1 + a_water=1（标记顶点）——采样在 frag
  tiled 分支 `sampleUv = (v_uv + fract(v_uvLocal + v_rippleOff)) × u_tileSize`
  = **fract 自动 tile 内 wrap，零 tile 识别**。〔首轮实现用
  `floor(a_uv/u_tileSize)` 识别 tile base：uvMax 角恰在邻 tile 边界网格线
  上 → floor 错指邻 tile（wood_side/planks）→ 水面混棕 8750 px 实证；
  根因 = 边界角点的 tile 识别歧义，tiled 路径编码结构性消除〕
- **vert 算 per-vertex 偏移**：`v_rippleOff = (sin(wp.x·0.8 + φ),
  cos(wp.y·0.8 − φ)) × 0.06`（wp = 世界 XZ，φ = u_ripplePhase；仅
  a_water=1 且 u_ripplesEnabled=1 时非零，否则 (0,0) → fract(x+0.0) 位级
  恒等 = 既有 tiled/全部非水几何渲染零变化）
- **相位帧锁定**：φ = 2π·(m_frameCount % kRipplePeriodFrames)/
  kRipplePeriodFrames，kRipplePeriodFrames = 120（2.0 s 整周期，命名
  常量零 config；取模防长会话 float 精度流失）= 硬规则 8（同帧数 ×2 run
  字节一致）
- **波纹参数**：幅度 0.06 tile = 0.96 texel（NEAREST 清晰 texel 台阶；
  再大单帧跳 2 texel 显跳帧、再小亚 texel 被 NEAREST 吞没）/ 波数 0.8
  rad/block（空间周期 7.85 块 > 池宽 = 池内单一平滑梯度无摩尔纹）/
  x 驱 u、z 驱 v ±φ 错相 = 采样窗走圆轨迹池内互不同步
- **water 纹理 tileable 化**：噪声坐标周期化（x%16,y%16 进 hash）→
  wrap 缝 = 水对水无接缝；block_atlas.png 仅 water tile 区域字节变化
  （其余 tile 逐像素 diff 0 实证）
- **config**：`render.water_ripples` getBool 代码默认 **true**
  （render.fog 先例，config.json 零改动）
- 顶点格式 +location 7 `a_water`（MeshVertex 尾字段默认 0 = 全部既有
  生产方零改动，52→56 字节）

零 format（19）/零内容（仅 water tile 纹理周期化）/零音效。

## 测试

1631/1631（+10：水顶面标记 4【顶面/被盖零/lava kind 门/level 4】+
stride 钉死/默认构造/config 缺省 true/显式 false 4 + **tiled 编码 2**
【水顶面 uv==tileIndex ∧ uvLocal∈{(0,0),(1,0),(1,1),(0,1)} ∧ tiled==1
∧ water==1 / 侧底保持 legacy 编码】），`-Werror` 0 警告，双 shader
glslangValidator 通过。water.png 重跑字节零变化（周期化在 16 格网内
恒等 = 无内容变更）。

## 门 110

- **S1 PASS = 第 110 次结构性免疫**（fresh 60f ×2 RAW + vs base18 字节
  一致，免重锚 = 基线视图零水面【demo 区唯一水体海在相机后方】→ 零水顶
  面标记顶点 → 分支惰性；11:4x 实证。注：base18 锚 10:4x 清理时误删后
  从干净 v0.26.0 源码重建【门 109 S1 已证 v0.26.0 对原锚字节一致 =
  重建锚等价】，本 S1 再次验证锚连续性）
- **S2 PASS**（12:3x，600f ×2/侧，场景 = 上节）：三侧 ×2 RAW 一致 +
  **old ≡ new-off 字节一致**（代码级免疫 = off 路径位级恒等实证）+
  on vs off **diff 7203 px 全分辨率**（池区 x[543..818] y[357..394]，
  **100% 蓝色系零邻 tile 棕色** = 首轮 tile 识别 bug 修复确证）+
  六存档 savenorm 全等（纯渲染层）。首轮 11:5x 曾 diff 8750 px 混棕
  8750→7203 = 修复后纯水 shimmer。
- 性能 A/B v0.26.0（发布前必做，12:4x-12:5x 双跑）：fall 900f 三窗
  **fluidCells 123879/167781/236573 四次跑全逐位一致** + 重跑对全指标
  同档（upd 47.8/50.6/48.4 vs 47.2/48.1/49.8、render 0.336/0.487/0.641
  vs 0.335/0.495/0.662、stream 1779/2107/2427 vs 1843/2104/2467 =
  ±3.5% 内；首轮 win1 upd +5.3% = run 间噪声，重跑 ±1% 级）=
  **成本可忽略**（波纹 = 水顶面每顶点 2 sin + 每帧 2 setUniform，fall
  场景水面占比极小）。

## 记档偏离

1. 仅水顶面 shimmer（侧/底静；MC 水面侧面亦近静止）
2. **lava 不闪**（kind 门排除 = 最小版记档，后续可调）
3. 波纹 = UV 采样窗平移（白噪声 shimmer 观感），非顶点波/法线扰动；
   幅度 0.06 tile / 波数 0.8 rad/block / 周期 120 帧 = 命名常量零 config
4. `render.water_ripples` 默认 true；off 对照 = scratch config false
5. 「石围」= 平台石齐平包围池口（flush 池；加高石沿在 8 块外平视下
   遮挡池面 = 解析证）

## S2 场景（fixer 构建 + headless 全验）

`/tmp/opencode/pre3_scene.json`（format 19，521 块 + 34 流体编辑）：
石块平台 x∈[9,25] z∈[−6,1] 顶 y44 + 4×4 齐平池 (20..23, 44, −4..−1)
L8（200 FluidSim tick 0 格变化 = 帧 0 起全静态）+ **天然熔岩湖填石除源**
（PITS 262：首次流体 tick 点燃草 rim = real-dt 非定帧源）+ 玩家
(12.5,45,−2) 距池 7.5-9.5 块 yaw −π/2 pitch −0.168902 + time 0.25。
视锥探针：池 9/9 采样点入视锥 0 遮挡、海面 130 顶面 0 可见 = 屏上水面
= 池唯一。

## 上报事项

- **存档系统潜伏不对称**（未修，矩阵外建议立项，KNOWN_ISSUES 已收录）：
  抽干生成熔岩的格 diff 记为 level-0 水编辑（fluidKindOf(0) 特例），
  回读只清水不清熔岩 → 读档后生成熔岩复活。触发 = 桶抽天然熔岩湖后
  存档重开。
- **PITS 262**（天然熔岩湖点火源 = real-dt 非定帧，无输入场景须除源）。
- 派发件 GLSL 笔误（wp.z → wp.y，vec2 无 .z，glslangValidator 捕获，
  首轮已修）。
