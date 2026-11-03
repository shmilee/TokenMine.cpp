# Phase V2-2 — 玩家皮肤 Steve 化（v2.2.0，门 173 全 PASS）

> 2.x 第二项。用户 11-02 实机反馈（V2-1 前视角上线后）：「看到正脸了，
> 没有眼睛，没有头发，皮肤也丑，请仿照史蒂夫更新」。

## 根因

`makeMobModelGeometry` 每盒**六面全贴同一 tile**（mob 单 tile 惯例），而
UX-3 的 head tile = 四侧脸 8×8 的 2×2 拼格 → 每面渲染整格缩图 = 五官
亚像素（无眼无发本体）；body/arm/leg tile 同款拼格 = 躯干四肢糊色。

## 实现（d9d62b0）

### A 模型 per-face 纹理（数据驱动通用能力）

- `MobBox` 可选 `faces` 段：六面名词表 {front, back, left, right, top,
  bottom}，允许部分面（其余回落默认 tile）；严格解析（未知键/非串/空串
  点名 box+面）；**无 faces = 默认路径位级不变**
- `makeMobModelGeometry` 按 kCubeFaces **外法线**定面名（(0,0,−1)=front
  yaw0 惯例，normal 字面量精确比较）选覆写 tile，UV 逐面计算；角序/
  绕序/顶点布局零动（FaceWinding 守护测保持）
- init 双站点（mob + player body）faces tile 存在性交叉校验（点名
  box+面）
- `mob_swing.cpp` 零改动（摆动帧整顶点拷贝，UV per-vertex 随行）
- **既有 6 mob JSON 零改动 + 全族测试零语义改动通过**（唯一测试侧
  diff = `MobBox{...}` 聚合初始化 14 处机械补 `{}` = 新成员后的
  -Werror 必然编译修复，断言未动）

### B Steve 风格皮肤（生成器确定性显式像素，零 hash 范式保持）

- **8×8 脸**（经典 Steve 比例，显式 RGB 数组入生成器注释 = 可审计）：
  棕发双色调（顶 2 行 HAIR_DARK (66,49,35) + 侧列 HAIR_LIGHT
  (84,63,44) 到第 5 行）+ 肤 (164,140,116) + **2×2 眼 ×2**（鼻侧白
  (244,244,244) / 外侧蓝虹膜 (48,98,170)）+ 鼻 2px 肤阴影 + 嘴 2px
  (112,88,70)
- 头 back/side = 双色发格 + side 肤耳；躯干 = Steve 青蓝衫 (52,148,152)
  + 领口/下摆深色行 (38,110,114)；臂 = 衫袖 4 行 + 肤手 7 行；腿 =
  蓝裤 (58,66,140) 9 行 + 棕靴 (84,64,44) 3 行
- **7 个 per-face 独立 16×16 派生 tile**：ph_face/ph_hair/ph_side（头
  8×8→16×16 **2× 精确放大** = 五官像素级；back·top·bottom 复用 hair，
  left·right 复用 side）+ pb_shirt/pa_arm/pl_pants/pl_boot（平面色带
  非均匀放大零失真）
- `player_body.json` 全 6 box 挂 faces（head 6 面 / body 全 pb_shirt /
  arms 全 pa_arm / legs 5 面 pl_pants + bottom pl_boot）
- **图集 94→97**：新 7 条 row-5 连续空位 [9..15,5]，**90 遗留条目零
  移位免重锚**（LegacyAtlasEntriesUnshifted 逐条目钉）；旧 4 拼格 tile
  退役（查定仅 player_body.json 引用 = 4 PNG 删除 + 4 条目移除）
- 生成器 ×2 重跑 md5 恒等自证；ph_face 独立解码 6 色点检全 exact

## 门 173（编排者执行；old 侧 = v2.1.0 = 6a42b30 = tm_v220_base）

- **S1 = 第 173 次结构性免疫 + base18 锚连续 179116 B 免重锚**：
  fresh 60f 四方 ×2（new ×2 / old ×2）全字节一致 + 跨版 savenorm EXACT
  （身体仅第三人称渲染，无输入 First = 零身体绘制；纹理 = init 期
  组装，90 遗留条目零移位 = 基线视图零触碰）
- **S2 = Steve 脸 A/B PASS**（三模式 ×2，同一 fresh 场景，gate-172
  捕获协议）：
  - a（OLD v2.1.0 前视）×2 RAW 153129 B / b（NEW 前视）×2 RAW 152754 B
    / c（NEW 后视）×2 RAW 281195 B
  - **savenorm a≡b≡c 两两 EXACT**（纹理 init 期零帧状态）
  - 头带（中心 ±20px 宽 × 中心下 20..108px）0.6 面阴影换算色分类：
    | | 眼白 | 虹膜蓝 | 发(深+浅) | 新肤 | 旧肤 |
    |---|---|---|---|---|---|
    | A 旧前视 | 0 | 0 | 0 | 0 | **2840**（无五官 = 报障本体）|
    | B 新前视 | **306** | **324** | **1421** | 1224 | 0（Steve 脸实证）|
    | C 新后视 | 0 | 0 | **5280** | 0 | 0（后脑全发）|
  - 六跑玩家 pos/yaw/pitch/hp 全精确零漂移
- **S3** = 2084/2084。
- **S6 = init 点名 ×2 PASS**：未知 face tile → "player body: model box
  '0' face 'front' texture 'ph_nothere' not in the atlas"（rc=1）；
  未知面键 → "player body model box 0: unknown face 'frontside' in box
  '#0' faces (allowed: front, back, left, right, top, bottom)"（rc=1）
- **性能 A/B v2.1.0 vs v2.2.0（发布前硬门槛）PASS**：fall 900f 无
  输入 First（身体零绘制 = 纹理变更结构惰性）——**首轮 render med
  偏置（+0.02/+0.06/+0.08 同向）重跑证伪**（重跑 0.594/0.683/0.575
  vs 0.615/0.634/0.631 = ±0.05 内方向混合）+ PIPE3-S0 changesApplied
  **9/3/1 三窗逐位**（双跑）+ 终档 fluids 18/18 + blocks 55/55 集合
  恒等 diff 0 = 同世界同成本

## 偏离/记档

- 头 8×8 五官在 ~4 块距离 ≈ 每脸像素 8 屏幕 px（眼 16 px 宽 = 门 S2
  判别量充足）；更近距离更大
- 旧 4 拼格 tile 退役而非保留（查定零其他引用）
- per-face 能力是通用的（mob JSON 同款可用）= 2.x 内容扩展面
  （mob 自定义脸/花纹）
