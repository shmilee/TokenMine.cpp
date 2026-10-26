# PRE-6 掉落光照（v0.30.0，矩阵 32/32 = v1.0 候选前最后一项，PRE 行 6/6）

BACKLOG 原文："PRE-6 掉落光照 — 掉落物受本地光照并参与遮挡（实体条）"。

## 实现（最小完整版，代理定案）

两半：① 掉落物受**本地光照**（本项代码）② 掉落物**参与遮挡**
（PRE-5 已结构性闭合——opaque 掉落走 opaque pass 写深度、cutout 掉落走
cutout pass 写深度、blend 掉落不写深度 = MC 纪律；本项 S2 ③ 补回归
证明，零新增代码）。

### 光照

- **根因**：块立方掉落（`makeItemCubeGeometry`）与 model 物品掉落
  （`makeItemModelGeometry`）顶点 `skyLight = 1.0 / blockLight = 0` 硬
  编码 = 掉落恒白（洞穴里的石头立方亮如白昼、火把旁无增亮）
- **vert 端 per-draw uniform 覆盖**（basic.vert 唯一改动，frag 零改动）：
  `u_useDropLight / u_dropSky / u_dropBlock` 3 uniform +
  `v_skyLight/v_blockLight` 赋值后 1 行门控分支
  `if (u_useDropLight != 0) { v_skyLight = u_dropSky; v_blockLight =
  u_dropBlock; }`——放 vert 不放 frag = 零 frag 分歧、零新 varying、
  chunk/mob/UI 全不受影响；frag 既有公式 `max(v_skyLight×u_dayFactor,
  v_blockLight)` 自动完成昼夜缩放（天光缩、块光夜亮 = glowstone 纪律）
  + kAmbient 0.03 底
- **纯函数** `src/entity/item_light.{hpp,cpp}`（campfire 范式）：
  `itemDropLight(world, item)` = AABB **中心格**（position 底面中心 +
  height/2，逐轴 std::floor 含负坐标 = 硬规则 1）→
  `getSkyLight/getBlockLight ÷ kMaxLight` 归一化 (sky, block)；出界/缺
  chunk 走 World getter 既有回落（上界 sky 15 / 下界双 0）
- **每帧采样无缓存**（光照变化 = 放火把/昼夜 → 自然跟随；成本 = 2 次
  world 读/掉落/帧，玩家尺度 <20 掉落可忽略；bob ±0.05 不跨 y 格边界 =
  采样稳定）
- **接线**（`drawItemEntities` 双路径）：model 路径 bind 后 set 3
  uniform → draw → **rebind 复位 u_useDropLight=0**；块立方路径
  同款（bind+set 先于 draw）——program uniform 状态跨 draw 残留（
  Renderer::draw 后解绑）= 不复位则下一 draw（mob/chunk/箭）被本掉落
  格光污染，两路径都复位
- **config `render.drop_light`** getBool 代码默认 **true**（render.fog/
  ripples/clouds/alpha_test 先例，config.json 零改动）；off = 3 uniform
  从不 set（链接期默认 0，分支结构惰性）= **对 v0.29.0 位级恒等**
- 几何文件 item_mesh.cpp / item_model_mesh.cpp **零改动**（顶点
  skyLight=1.0 保留 = off 红线的几何侧：不重烘、位级不变）

零 format（19）/零存档（entities 既有段）/零内容/零音效/零 frag 改动。

## 测试

1672/1672（+8：ItemDropLightTest 6【CenterCellSelection /
CenterOnBoundaryFLoorsToUpperCell / NegativeCoordinatesUseFloor /
ExactNormalizedValues（1.0 与 8/15 精确）/ WorldFallbacks（void+
below-world 双回落）/ DayIndependentAndPure（双调恒等 + 不接
dayFactor 架构断言）】+ DropLightConfigTest 2【缺省 true / 显式
false】），`-Werror` 0 警告，basic.vert glslangValidator 通过。

## 门 113

- **S1 PASS = 第 113 次结构性免疫（免重锚，17:2x）**：fresh 60f ×2 RAW
  + old(v0.29.0) ×2 + new-off ×2 三侧 + **old ≡ new-off 字节一致**
  （代码级免疫）+ **on ≡ old 字节一致**（零掉落 = 零 item draw = 分支
  结构惰性）+ on ≡ base18 锚（锚连续性）
- **S2 PASS**（17:2x，掉落光照 A/B 一次全过，600f ×2/侧）：fixer 石砌
  密封长廊场景（内腔 x4..9 z−25..−9 + 地板 slab y53 全密封【预验否决
  开洞方案：天光漏进大厅破坏 ① 黑暗】+ 唯一 2×3 眼位窗 z−8 + 熔岩湖
  18 格排干 PITS 262）3 掉落覆盖两路径：**① stone 块立方**（4.5,54,
  −24.5）远西角 (sky 0, block 0) = 新侧 ambient 0.03 近黑 / **② apple
  model 物品**（7.5,54,−10.5）窗+火把旁 (11,11) = 0.733 亮 / **③ dirt
  块立方**（6.5,50,−15.5）slab 下天然洞穴 = 新旧皆不可见（遮挡回归）。
  旧侧 ②≈① 恒白同亮 vs 新侧 ②>>① = 互斥判别。断言全过：三侧 ×2 RAW +
  old ≡ new-off 字节一致 + **on vs off diff 77 px 全分辨率限定
  x[614..683] y[402..450]（与 fixer 预验逐像素同值，③ 零贡献带外 0）**
  + 六存档 savenorm 全等（剥 entities age / saturation / fires
  remaining，PITS 244 纪律）
- 性能 A/B v0.29.0（发布前必做，17:3x-17:4x 双跑）：fall 900f 三窗
  **fluidCells 123879/167781/236573 四次跑逐位一致**；首轮 NEW 侧
  stream win1 +36% = 环境噪声（重跑对称性证伪：win1 双侧 1837 位级
  一致）+ 重跑对全指标同档（upd 46.0/48.1/46.9 vs 48.2/49.5/48.7、
  render 0.372/0.558/0.719 vs 0.368/0.549/0.723、stream 1837/2086/
  2384 vs 1837/2115/2469）= **成本可忽略**（fall 场景零掉落 = 结构
  惰性实证：itemDropLight 零调用、3 uniform 零 set）

## 记档偏离

1. 光照采样 = AABB 中心格（MC 同款"实体所在格"；跨格边界不混合双格光
   = 简化）
2. 每帧采样无缓存（光照变化自然跟随；成本可忽略）
3. **箭头/手持恒白不变**（箭 = 投射物视觉、手持 = UI 层，矩阵外）
4. "参与遮挡"半 = PRE-5 结构性闭合 + S2 ③ 回归证明（零新增代码）；
   ③ 遮挡用"slab 下天然洞穴"而非隔墙暗格（隔墙方案预验失败：可见地板
   区 6×6 无法同时满足 ① 距 torch ≥15 与距窗 ≥15，地下洞对任意射线
   全遮蔽且 slab 密封性更强）
5. `render.drop_light` 默认 true；off = 对 v0.29.0 位级恒等（S1/S2
   三方 RAW 实证）
