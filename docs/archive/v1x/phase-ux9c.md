# Phase UX-9c — 环境底光可配置（v1.50.0，门 170 全 PASS）

> 消费用户 11-01 实机诉求：「洞穴底光调亮，我也需要，最好 config 中可以
> 配置不同环境、不同时间的视觉亮度，比如夜晚的、水底的、洞穴的等」。
> **UX-9 体验细化（二轮实机报障）行 ③ = UX-9 系统收官项**。一夜/洞穴/
> 水底三诉求收敛两键：夜晚露天 = 既有 `render.night_sky_level`（UX-1），
> 全遮挡环境 = 本项新键 `render.cave_ambient`。

## 现状与根因

全黑区（天空光 0 ∧ 块光 0 = 洞穴/深水/室内）的底光恒 = `u_ambient`
（per-run 常量，LGT-3 时代编译期 0.03 → UX-1 起随 `night_sky_level`
派生 `ambientFloorForLevel`）。用户下洞（UX-9b 教程现在会引导去下洞
挖煤）遭遇的"黑乎乎"= 该 3% 地板无独立调节面；且 `night_sky_level=0`
（全黑夜）时地板归零 = 洞穴彻底纯黑，无逃生亮度。

## 实现（单 uniform + 公式唯一改动）

- **`assets/shaders/basic.frag`**：光公式 `light = max(天空仿射撤退,
  块光)` 后加
  ```glsl
  float amb = light <= 0.001f ? u_caveAmbient : u_ambient;
  vec3 rgb = base * (amb + (1.0 - amb) * light * v_faceShade);
  ```
  全遮挡区烘焙光逐顶点恒 0 → 三目取 `u_caveAmbient`；插值跨界像素
  （一顶点有光一顶点全暗）= 纯 ALU select 无正确性问题。**雾/波纹/
  gl_Position/v_viewDepth 零触碰**（PRE-2/PRE-3 位级红线保持）。
- **`render.cave_ambient`**（double [0,1]，**代码默认 `kAmbientFloor`
  0.03f = 复用既有常量零新数字**）：`readCaveAmbient` 严格读纯函数
  （`lighting.{hpp,cpp}`，`readNightSkyLevel` 同位同范式：absent = 0.03
  / 非数 "must be a number" / 非有限 "must be finite" / 越界 "must be
  in [0, 1] … got X" 全点名；浮点合法**无整数检查** = 连续亮度旋钮；
  范围检查全在 double 域 = UB 路径免检）。
- **init 一次性上传**（game_init：扩进既有 u_nightRetreat/u_ambient 的
  bind→set→unbind per-run 常量块 = 第 3 个 uniform；**零新 Game 成员**
  = 无主机侧消费者 YAGNI）；config.json 零改动（代码默认键纪律，
  fire/mobs/beacon/night_sky_level 同款）。
- **深水自动同享**：水 light_opacity 2 衰减天空光 → 深处 light 0 走
  同分支零额外代码。
- **位级恒等根**：默认配置下 u_ambient = u_caveAmbient = 0.03f →
  三目两分支同值 → 后续表达式操作数与求值序逐位不变（门 S1 实证）。

## 测试

2072/2072（+11：absent exact kAmbientFloor / 0.0/0.5/1.0/0.25 合法 /
−0.1 与 1.5 的 "in [0, 1]" 点名 / "dark" 与 null 的 "must be a number" /
1e999 "must be finite" / 1e300 finite 过 + 范围点名）、`-Werror` 0
警告、banner v1.50.0。

## 门 170（编排者执行；old 侧 = v1.49.0 = e47660c = tm_v150_base）

- **S1 = 第 170 次结构性免疫 + 默认 0.03 位级恒等 + base18 锚连续
  179116 B 免重锚（最强形）**：fresh 60f 六跑——new-default ×2 /
  **new-explicit-0.03 ×2（显式 == absent 实证）/ old ×2 全字节一致**
  （六跑 vs 锚 vs 彼此 6 对 EXACT）+ 跨版 savenorm EXACT（config 非
  帧状态）。
- **S2 = 洞穴 A/B PASS**（受控密封石室 = LGT-5 门 120 判别场景范式：
  `build_cave_scene.py` 7×4×7 石盒 x[−3,3] z[−3,3] y[53..56]，地板
  y53 显式石 + 内腔 y54/55 air 50 格 + 周壁 + 顶板 = 室内 light 恒 0；
  玩家 (0.5,54,0.5) 室内、yaw 0 pitch −0.25，196 显式格，mobs off）：
  A = absent（0.03 地板）×2 RAW 32119 B 字节一致 / B = 0.3 ×2 RAW
  74863 B 字节一致 + **B vs A diff = 505,176 px = 100% 全帧**（无输入
  未锁定 = HUD 零绘制，相机在室内世界铺满全视图——比预期更纯，无
  HUD 带需归因）**0 px 变暗（全仅亮化）** + A(4,4,4)→B(36,36,36)
  ≈10× 地板亮化（0.03→0.3 解析值，round 量化后 9-10× 逐像素吻合）+
  **savenorm A≡B EXACT**（config 非帧状态最强形）。
- **S3** = 2072/2072。
- **S6 = init 点名 ×2 PASS**（−0.1 → "config render.cave_ambient must
  be in [0, 1] (0 = no residual glow in fully-occluded areas, 1 = full
  brightness), got -0.100000" / "dark" → "must be a number"，rc=1）。
- **性能 A/B v1.49.0 vs v1.50.0（发布前硬门槛）PASS**：fall 900f 无
  输入——**首轮 render med 偏置重跑证伪**（首轮 v150 win1 +0.08 / win2
  +0.20 / win3 −0.02 方向不一 = 执行序噪声；重跑 0.583/0.674/0.623 vs
  v149 0.641/0.624/0.588 = ±0.1 ms 内同档，PRE-3 门 110 同款重跑协议）
  + PIPE3-S0 changesApplied **9/3/1 三窗逐位** + **终档 fluids 18/18 +
  blocks 55/55 集合恒等 diff 0 = 同世界同成本**（amb 分支 = 每像素
  1 比较 + 1 select，结构惰性最强形：无全遮挡像素 = 分支恒走原路径）。

## 偏离/记档

- `night_sky_level=0`（全黑夜）时 u_ambient = 0 而 cave_ambient 默认
  仍 0.03 = 两键独立可配：全黑夜洞穴保留 3% 逃生亮度，用户显式
  `cave_ambient: 0` 可收敛彻底纯黑（fixer 上报，定案保持 = 独立旋钮
  语义）。
- 阈值 0.001（非 0）= 覆盖浮点残留（烘焙光 0 经仿射撤退后理论恒 0，
  阈值防 -0/ulp 边界）；光值最小非零档 1/15 ≈ 0.0667 ≫ 0.001 = 无
  误触。
- 手持/图标走 UI/basic 各自的烘焙光（hotbar 块立方 light ≥ 0.467）
  不触分支 = HUD 零影响（S2 全帧 diff 已含此实证）。

## 发布

**tag v1.50.0 = UX-9 系统收官 tag + GitHub release**（notes 覆盖
v1.48.0/v1.49.0/v1.50.0 三轮实机报障修复：云方块化+厚度 shader 化 /
教程火把步 11 步链 / 环境底光可配置）。
