# PRE-5 透明 discard（v0.29.0，矩阵 31/32，PRE 行 5/6）

BACKLOG 原文："PRE-5 透明 discard — alpha cutout discard，修透明孔深度写入
（渲染条）"。

## 根因

全项目零 depthMask（GL 默认恒 depth-write ON）→ 透明（blend）pass 的
**alpha-0 像素也写深度**：leaves（alpha {0,255}）/ cross 植物 / glass 框
的透明孔写入不可见深度 → 孔后几何（物品/mob/光源/后层叶冠）被深度
遮挡 = "从叶孔看不见后面" bug 本体。

## 实现（最小完整版，代理定案）

- **pass 三分**（opaque / cutout / blend）：mesher `ChunkMeshData` +
  cutout 桶（第三桶，MaskCell +cutout 字段 → cutout/blend 面永不并
  greedy quad）；每列 mesh 2→3
- **路由 = 块 property `cutout`**（严格 bool 点名 + cutout⇒transparent
  配对拒绝 + **shape 门控**：仅 full-cube/cross 可带，fluid/slab/stairs/
  wall/fence/glass_pane 上 = 死字段点名拒 = ammo 纪律）
- **内容 20 块**（纹理 16×16 alpha 普查为据）：leaves/spruce_leaves/
  birch_leaves + allium/tulip/cornflower + wheat_1..7 + 3 sapling +
  fire/torch + campfire/campfire_extinguished = cutout 桶；**glass /
  glass_pane / water / lava 保持 blend 桶**（半透/流体不写深度 = MC
  纪律），其 alpha-0 像素由下面的桶无关 discard 修复
- **物品 icon init 期 alpha 普查**：`isPureCutoutImage` 纯函数（全像素
  alpha ∈ {0,255} ⇔ cutout）per-item 单次 Png::decode + 普查 →
  m_itemCutout 并行向量 → 世界掉落/粒子按 RenderPass 三分路由；
  shipped 全 83 icon 均纯 cutout（16 件 model 物品 → cutout 段）
- **shader**：MeshVertex 尾 +`float cutout`（56→60 字节，location 8，
  默认 0 = 既有生产方位级不动）；**frag 唯一新行**
  `if (u_discardEnabled != 0 && alpha < 0.5) discard;`（alpha 算出后、
  雾分支前；桶无关：水 0.78 / glass 芯 0.86 / 云 0.55 / 光束 0.5 全不
  discard，纯孔 0 全 discard）；gl_Position / v_viewDepth / 波纹分支
  零触碰
- **渲染序 + depthMask**：opaque 段不变 → enableBlending → 天空对象 →
  **cutout 段**（discard 生效 + 深度写 ON【GL 默认】）→ alpha_test on
  时 **setDepthMask(false)** → **blend 段**（透明列/物品/粒子/信标/云/
  overlay 不写深度）→ disableBlending + 复位；`Renderer::setDepthMask`
  新 API（glDepthMask 直包）
- **config `render.alpha_test`** getBool 代码默认 **true**（config.json
  零改动）；**off = discard 关 + depthMask 恒 GL 默认 + cutout 段占位
  = 对 v0.28.0 位级恒等**（无 water/glass 场景；含半透混合场景的 off
  分段序 = 记档偏离 3）

## 测试

1664/1664（+16）：解析 3【true/缺省 false/非 bool 点名】+ shape 门控 1
（pane/fluid/slab 拒 + cube/cross 放行）+ 内容钉死 2（shipped 20 块精确
集 + glass/pane/water/lava/cactus/stone 反例）+ mesher 路由 5（cube→
cutout 24v / glass→blend / opaque→opaque / cross 正反 / cutout-blend 共
tile 不并 quad）+ 顶点布局 1（offset 56 / size 60 / 默认 0）+ 普查纯
函数 1 + 物品模型标记 1 + config 2；既有 test_ripple 布局钉演进 +
test_mesh_job 快照等价断言扩 cutout 桶（3 处）。`-Werror` 0 警告，双
shader glslangValidator 通过。

## 门 112

- **S1 PASS = 第 112 次门 + 重锚**（15:4x，LGT-2 门 83 先例）：
  new-on ×2 RAW（60f，t 恒 0 静态）+ old(v0.28.0) ×2 + **new-off ≡ old
  字节一致**（代码级免疫）+ on vs old **diff 5616 px 全限定 y[0..427]
  树冠区**（左树 x[0..500] / 远树冠 x[600..1000] y[200..430] / 右树
  x[1000..1361]；成因 = 重叠叶冠孔后见后层叶（1663 叶绿）+ 冠顶孔后
  见云（231 天蓝，y<120 零漏色实证）+ 顶边孔后云；**y>650 下带零 diff**；
  精确天空色计数 166282→165050（−1232 = 孔后见叶/云不见天空，重锚
  预期记档）→ **base18 锚刷新**（含云 + cutout 修复新基线）。首轮断言
  坑：y<120 "纯天空带"假设错——冠顶延伸到 y=0（画面边缘高树），该带
  diff = 修复本体（复核改色分类断言：全叶绿/天蓝 0 漏色）
- **S2 PASS**（15:48，叶孔判别场景一次全过）：fixer 设计石砌密封房间 +
  2×2 叶墙开口 + 室内 torch + 3 物品（2 leaves 立方 + apple），demo 区
  1474 cutout 内容格清空 = 除叶墙孔区外零变化源；old ≡ new-off 字节
  一致 + **on vs off diff = 210 px 全分辨率限定 x[648..776] y[442..511]**
  （与 fixer 预验逐像素同值；on = 孔透物品/云，off = 孔显室内石地 =
  bug 本体判别）+ 带外 0 px + 六存档 savenorm 全等（剥 real-dt：fires
  remaining / entities age / saturation，PITS 244 纪律）
- 性能 A/B v0.28.0（发布前必做，15:5x-16:0x 双跑）：fall 900f 三窗
  **fluidCells 123879/167781/236573 四次跑逐位一致** + upd/swap/stream
  全同档（±3%）；**render med 双侧持续偏高 +5-13%（绝对 +0.02-0.08
  ms，随视窗叶列占比增长**，双跑同向同幅 = 非噪声）= cutout 桶分流后
  混合列（叶+水/玻璃）+1 mesh/draw 的 iGPU 提交成本（P99.9 记录
  10-30 us/draw 量级的子集）；帧预算 16.7 ms 的 <0.5% = **成本可接受
  如实记档**（非"可忽略"）；优化选项（混合列并 mesh）受 depthMask
  全局状态限制不可行 = 记档

## 记档偏离

1. cutout 判据 = 块 property（数据驱动，纹理 alpha 普查为赋值依据）；
   glass/pane/water/lava = blend 桶（半透/流体不写深度 = MC 纪律），其
   alpha-0 像素由桶无关 discard 修复（frag 一行，与桶分流正交）
2. blend 段全体 depthMask false（云/水/玻璃/粒子透明段/信标/overlay）——
   粒子透明段与云行为变化 = 视觉正确性提升记录（基线 fresh 无粒子，
   云在门 111 锚内，S1 y>650 零 diff 实证）
3. off 时 cutout 段占位（天空后 blend 前）：无半透场景 = 位级恒等
   （S1 实证）；**含 water/glass 混合场景 off 的分段序 ≠ v0.28.0 单段
   交错序**（门场景无此组合，不影响过门）
4. 物品/手持 icon 纯 cutout 者（shipped 全 83 均纯）世界掉落进 cutout
   段（init 期 alpha 普查，零 JSON 字段）；**手持 drawHeldHand 保持
   opaque 段**（手恒最近 + discard 已修其 icon 背景深度写 = 零改动
   接入点定案）
5. cutout 仅 full-cube/cross 两 shape 生效（其余 shape = 死字段 init
   点名拒）
6. v_cutout varying 传递但 frag 未消费（discard 为 alpha 判据非顶点
   判据 = 几何身份留档；链接器自动失活，零成本）
