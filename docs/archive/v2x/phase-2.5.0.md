# Phase V2-5 — 新 mob 猪（v2.5.0，门 176 全 PASS）

> 2.x 第五项。纯数据驱动内容任务 = 「加 mob = JSON + 纹理」承诺的
> 验证。设计定案 = 11-03 夜间（/tmp/opencode/v25_design.md）。

## 实现（零核心代码）

- **pig.json**（chicken.json 结构样板）：passive（detect_radius 0 /
  attack 0）、health 10（MC 猪 10）、speed 0.6、spawn_max_light 15、
  wander 2.0；drops = pork ×1-2（count_min/max 随机区间既有机制）+
  leather ×0-1（cow/sheep 先例）
- **model**：宽体 6-box（body 0.6×0.5×0.75 + 4 粗短腿 0.2×0.4×0.2 +
  head 0.45³ 嵌入 body 前方——cow 嵌入范式；head 前方 snout box 方案
  被 0.9×0.9 AABB 边界否决 = 定案「或」取 per-face 鼻纹侧）；swings
  4 腿 0.4 rad 对角分组（leg_fl/rr group 0，leg_fr/rl group 1 =
  chicken 范式）；全 64 相位 clip 验收（ShippedAllPhasesNoClip 扩展）
- **head `faces.front` = pig_face**（V2-2 MobBox faces 范式的**首个
  mob 消费**——此前只有玩家身体用过）：显式 8×8 猪脸（粉 + 深粉鼻口
  + 2×2 眼，Steve 脸范式零 hash）→ 2× 放大 16×16 tile
- **纹理**：generate_mob_textures.py + pig_body（MC 猪粉 (228,170,160)
  + noise 8，新 salt 0x5011）+ pig_face；**既有 6 mob tile 零变化
  （176 文件全 cmp 字节一致实证）**
- **食物链**：pork（+3 / 1.8 sat / smeltable_to）+ cooked_pork（+8 /
  12.8 sat = beef 熟同款刻度）；icon 生成器新 salt（生粉/熟棕）；
  无新配方（杀猪→熔炼 MC 同款）
- **生成**：mob_spawning.json += {"id":"pig","weight":1}（6→7，加权
  抽取既有机制零代码）
- **图集 97→99**：pig_body [8,7] + pig_face [9,7] 进 row-7 空位，
  **97 既有条目零移位**（git diff = 纯 +8 行；ShippedV25AtlasFullCensus
  99 条全 census 逐格钉）
- **字体 422→423**：猪 U+732A 码点序插入（cell 383，其后 15 字 +1
  re-cell），**顶 512 行 131072 px 逐字节不变**（BMP 解码 cmp 实证）
  = 免重锚；424 预算剩 1 字
- 版本 2.5.0（CMakeLists 单点 + F3 钉 + banner 实证）

## 门 176（编排者执行；old 侧 = v2.4.0 = 3b88e61 树）

### S1 = 第 176 次结构性免疫 + base18 锚连续 179116 B 免重锚
- fresh 60f 六跑（new-on n1/n2 / old v2.4.0 o1/o2 / new mobs-on m1/m2）
  全 179116 B：**n1 ≡ o1 ≡ o2（跨版 RAW 字节一致）+ n1 ≡ m1
  （mobs-on 60f ≡ mobs-off = first_spawn_frame 60 内零 spawn，生成扫描
  render-inert 实证）+ m1 ≡ m2（mobs-on 确定性）+ n1 ≡ base18 锚**
- 跨版 savenorm **EXACT**（零 format——pig 是纯内容，无存档影响）

### S2 = 猪判别
- **a 生成确定性 + 互斥**（mobs-on 3600f headless ×2 侧）：新侧 pn1≡pn2
  生成序列 118 行逐条恒等（含 **pig f60 (0.5,54,-5.5) + pig f300
  (0.5,54,-5.5)** = spawner 外环 6 块优先 PITS 359① 复现），旧侧
  po1/po2 118 行**零 pig**（6 mob 词表）= 互斥判别
- **b 杀猪链**（两会话，全存档权威）：
  - 会话 1（mobs-on，场景 = 炉子 (0,54,-8) 预置 + coal slot2 +
    hunger 14 + iron_axe slot0 预选）：**瞄准 = 离线探针解**（headless
    f210 截图 → 粉簇反投影【kFovDegrees 70 垂直 + 眼高 1.62 +
    Ry·Rx 相机模型】→ 猪 f210 位 (1.19,-5.97)【wander 1 段后】→ 走位
    自洽迭代【W 沿 KILL pose yaw 走 0.8s，2-3 轮收敛】→ yaw 0.0525…
    终解 yaw -0.1066 pitch -0.2460，猪距 3.07）→ 实跑零相机修正：
    截图粉簇 8532px 中心偏十字 (-25,+15)px → LMB-hold 2.0s（8 swings，
    3+ 命中：iron_axe 4 伤 × 击退沿射线漂移 3 击内 < reach 5）
  - **掉落区间钉（存档权威）**：pork ×1（∈1-2 ✓）+ leather ×0（∈0-1
    ✓）@ (1.38,-6.55)
  - 会话 2（mobs-OFF quiet，save-posed 传送到掉落格 PITS 347，aim =
    炉子 +Z 面中心解析解 yaw 0.969 pitch -0.588）：拾取（0.5s 延迟 +
    1.0 半径，pork → slot1）→ RMB 开炉（**一次命中**，panel-bg 22147px
    判别）→ 面板 pork→input / coal→fuel（uiSize 908×496 × 1.5 坐标，
    g174 标定复用）→ 12s 熔炼（**燃料点火即耗 PITS 366：substeps
    4757 判别**）→ output 取熟猪排 → slot1 → E → re-capture → 吃
    （**selected 预置 = 1，免数字键——见 PITS 368**）→ **hunger 14+8=20**
    全链存档断言 ✓

### S3 套件
- 2135/2135（2128 + 7 新：PigPassiveAndDropsPinned /
  PreV25MobFilesUnchanged 6 JSON FNV 钉 / ShippedSpawnListPinsPig 7
  词表 / ShippedPigGeometryPinned / ShippedV25AtlasFullCensus /
  V25PigMobTilesPinned / V25PorkChainPinned + 10 既有测试扩展 pig 覆盖）

### S6 精确点名（真实二进制 2/2）
- mob_spawning.json += unknown id → 「mob_spawning.json: unknown mob
  'dragon'」init 拒 ✓
- 图集删 pig 条目 → 「texture tile 'pig_face' has no entry in the
  atlas layout」init 拒 ✓
- （pig salt md5 / 图集零移位 / 字体成员 / 权重词表 = 单测钉）

### 性能 A/B PASS 同世界同成本（用户硬要求）
- v2.4.0（tm_v250_perf = 3b88e61 + g139）vs v2.5.0（tm_v250_perf_new =
  最终工作树 + g139）：fall 900f 无输入（mobs off）seed 123456789，
  banner 双侧实证：
  - PERF-WIN 三窗全指标同档（upd med 0.56/0.62/0.63 vs 0.58/0.62/0.64；
    render med 0.52/0.58/0.58 vs 0.55/0.62/0.64；swap 15.3 vs 15.2
    vsync 主导同值；fluid/fires/spread/ents/stream 全同档）
  - **PIPE3-S0 changesApplied 三窗 max 9/3/1 双侧逐位**
  - **终档 fluids 18/18 + blocks 55/55 集合恒等**（同世界最强形——pig
    纯内容 + mobs off 零 spawn = 结构惰性实证）
  - **mobs-on 猪自成本预算**（同二进制）：ents 0.002→0.040-0.097 ms
    （4 mob 含 2 猪）vs v2.4.0 mobs-on 参照 0.039-0.102 ms（4 mob 6
    词表）= **同档，猪成本 ≈ 既有 mob，无回归**；upd/render/swap 全同档

## 偏离（记档）
- **食物 icon 未进图集**（定案字面「2 食物 icon 全进图集空位」）：
  shipped 事实 = 食物 icon 全为 `textures/items/*.png` 独立 PNG
  （beef/chicken 等零图集条目；init 两路加载 m_itemIcons/
  m_itemModelTextures）——icon 条目进 block_atlas.json 直接 init FAIL
  （「atlas layout entry has no texture tile」实证）；图集只加
  pig_body/pig_face 2 条，零移位纪律保持
- 字体 +1 字（定案 +1~2）= 取最小
- saturation 刻度（定案只钉 +3/+8）= beef 链同款 1.8/12.8
- 模型具体几何（定案钉范式未钉数值）= MC 猪 AABB 0.9×0.9 族
- **猪掉皮革 0-1 vs MC 上限 2** / **pork 1-2 vs MC 固定 1** = 与既有
  被动 mob 刻度一致（KNOWN_ISSUES 记档）

## 环境事故（门内，非项目缺陷）
- **XTEST 键盘事件服务器级失效**（PITS 368）：门内 ~09:05 起 xdotool
  键事件（含 F5）全被 X 服务器拒绝（XTestFakeKeyEvent rc=1；鼠标
  XTEST 正常）——eat 段改用 selected 预置 + 退出自动存档绕过；**输入
  门（V2-6+）前须重启 X 会话或修复**（xmodmap/setxkbmap/xinput
  disable-enable 均无效）
- Awesome drawin 窗 (682,368) 120×400 盖住游戏窗中央 = capture 点击
  被吞（PITS 369）：capture 点击改安全点 (X+300,Y+120) + windowraise
- mob 不持久化（save entities = item drops only）= S2 杀猪链必须单
  会话或掉落接力（PITS 370）
