# Phase CRAFT-2 — 物品 3D 模型（v1.0 任务表 24/32，门 105）

> per-item 数据驱动 3D box 模型，消费三个上下文——手持（viewmodel）/
> 世界（掉落物）/ 进食举到嘴（= 手持的进食动画态，既有
> `m_eatChannel.progress()` 驱动的手持 lift/pull 变换零额外代码继承）。
> 纯渲染层：AI/物理/存档/合成零改动。

## 实现内容

1. **物品 JSON `model` 段（可选）**（`src/item/item.hpp` +
   `item_registry.cpp parseModel`）：`{texture: 纹理文件名, elements:
   [1..16 个 {from:[x,y,z], to:[x,y,z], uv?:[u0,v0,u1,v1]}]}`——
   单位立方模型空间 [0,1]³、axis-aligned box（**无旋转** = MC primitive
   rotation 记档不做）、缺省 uv = 整纹理；严格解析：未知键三层点名
   （model/element/corner）/ 3 元非数字 / 越 [0,1] / from>=to 任轴 /
   uv 退化矩形（u0==u1 ∨ v0==v1）/ 空 elements / >16 元素全拒
   （no-op 纪律）
2. **模型网格纯函数**（新文件 `src/entity/item_model_mesh.{hpp,cpp}`）：
   `makeItemModelGeometry`——每 box 6 面 quad，kCubeFaces 角序/法线泛化
   到任意 from/to（硬规则 9 faceShade 约定：顶 1.0/±X 0.8/±Z 0.6/底
   0.5，与块立方手/掉落立方同款）；居中单位空间（−0.5..0.5，icon 手
   同款）；skyLight 1.0/blockLight 0.0（块立方掉落约定）；双面 =
   caller 关裁剪（beacon 光束/十字粒子惯例）；**幂等纯几何**
   （build-once 缓存 = 跨 run 字节可复现）
3. **init 纹理**（`buildScene`）：`m_itemModelTextures` per-item 模型
   纹理（texture 存在性交叉校验 = icon 文件纪律，grows_to 范式）；
   texture==icon 时同一 PNG 双上传（16×32×32 RGBA ≈ 64 KB VRAM，
   一次性）
4. **手持接入**（`drawHeldHand`）：model 物品 → 模型网格替代 icon 面
   片（kHandIconScale 足迹、相机系变换、**弓拉/进食举到嘴变换继承**、
   关裁剪双面）；block 立方手路径位级不动
5. **世界掉落接入**（`drawItemEntities`）：model 物品 → 模型网格替代
   **原不可见掉落**（blending pass only——icon PNG 带 alpha，opaque
   pass 会黑盒 = icon 手同款模式；帧锁定 bob+spin = 块立方掉落
   约定）
6. **`Game::itemModelMesh(ItemID)`** 惰性缓存（m_itemModelMeshes，
   init 定尺寸，首帧构建）

**零 format（19）/ 零 config / 零新纹理 / 零音效 / 零物理**。HUD 2D
图标（热栏/面板/手持角标）全程走未动的 `drawIcon` 路径。

## 内容（16 模型，纹理 = 既有图标 PNG）

- 工具 9：wooden/stone/iron × pickaxe（3 元素：头横杆+头冠+柄）/
  axe（2：头+柄）/ shovel（2：铲头+柄）
- 弓 5：bow/power_bow/speed_bow/infinite_bow/poison_bow（3：上臂+下臂
  +弦，中部开口 = C 弧近似；**master_bow 无 model** = 保留 icon 面片）
- 苹果 2：apple/golden_apple（2：果体圆近似+柄）

元素几何按图标剪影派生（记档"v1 简化体素造型"）；元素间 slab 严格
嵌套或分离 = 无共面重叠面（Z-fight 守卫由单测
`ShippedModelsHaveNoFaceCoincidentBoxes` 钉死；解析器不强查，
shipped 数据是唯一作者）。

## 变更文件

- `src/item/item.hpp`（`ItemModel`/`ModelElement` + `optional<ItemModel>`）
- `src/item/item_registry.{hpp,cpp}`（`parseModel` + 已知键表加 "model"）
- `src/entity/item_model_mesh.{hpp,cpp}`（新，纯函数）
- `src/core/game.{hpp,cpp}`（init 纹理 + `itemModelMesh` 缓存 +
  `drawHeldHand`/`drawItemEntities` model 分支）
- `CMakeLists.txt`（mc_core + 测试各 1 文件）
- `assets/data/items/×16`（追加 `model` 段，其余字段零改动）
- `tests/test_item.cpp`（+8：解析坏值/round-trip/缺省/17 元拒/无 model
  回归/16 件精确集/内容钉死全值）
- `tests/test_item_model_mesh.cpp`（新 +6：全盒居中/绕序守护/子盒
  偏移/UV 映射/元素拼接/16 shipped 无共面）
- `docs/KNOWN_ISSUES.md`（消费 2 条：非 block 掉落不可见 = 16 件已解决
  / 物品无 3D 模型 = 16 件落地注记）

## 测试（1549/1549，+14 新）

`-Wall -Wextra -Wpedantic -Werror` 0 警告；真实 `loadDirectory` 冒烟 =
78 物品解析成功、恰 16 带 model。

## 门 105

- **S1 PASS = 第 105 次结构性免疫**（10-26 02:37，fresh 60f ×2 RAW
  字节一致 + vs base18 **字节一致** = 免重锚；论证 = 手持 model 分支仅
  pointer locked 渲染【无输入从不锁定】+ fresh 60f 零破坏零掉落
  【drawItemEntities model 分支零执行】+ HUD 2D 路径未动 + 16 模型
  纹理只 init 加载【无渲染副作用】+ 全部新代码以
  `it.model.has_value()` 静态门控）
- **S2 掉落场景 A/B PASS**（10-26 03:35）：3 model 物品
  （wooden_pickaxe@(22.5,38,−4.5) / bow@(23.5,38,−3.5) / apple@(21.5,39,−3.5)，
  真实地形 h 探针定案——非陈旧 y45 常量）置玩家 (22.5,36,0.5) yaw0 pitch0
  视野内 4-5 块（> 拾取半径 1.0）+ 无输入 600f，old = frozen v0.21.0 +
  v0.21.0 精确资产（无 model 段，旧解析器拒未知键）vs new = CRAFT-2 构建
  + 工作树资产，各 ×2 RAW 字节一致 + **A/B diff = 181 采样 px（≈724 实际
  px）限世界区 x[538..826] y[434..588] maxy<676 = HUD 零**（OLD 侧掉落
  不可见 → diff = 模型渲染像素隔离；对照对 old_b/new_b 同量级 181）+
  双侧存档 entities **位级一致**（age 剥离，item/count/pos 钉死 = 模型
  纯渲染实证）+ 3 物品未拾取。坑：首轮 A/B 字节相同 = **ent1_dense
  scratch 资产漏同步**（16 model JSON 未进 E2E 资产根，hasModel=0 =
  PITS 255-258 漏同步坑第 4 次）+ 原断言脚本 `c==d and c!=d` 恒假
  diff 块死代码（独立断言 gate105_assert.py 修正后全 8 项 PASS）
- **S3** = 套件 1549 PASS
- 性能 A/B v0.21.0（发布前必做，10-26 03:41）：**fall 900f 三窗
  fluidCells 123879/167781/236573 双侧逐位一致（同世界）+ upd/render/
  swap/ents/stream 全同档**（model 分支无输入零执行 = 结构惰性实证）；
  **drop 场景 900f 三窗 fluidCells 123871/167781/236573 双侧逐位一致 +
  全指标同档**（render med 0.333/0.493/0.661 vs 0.316/0.479/0.627 =
  3 物品 box 网格成本 +0.02-0.03 ms 同档内）= **成本可忽略**
- **H 系输入跑 = 补跑清单**（门 104 S2 协议 PITS 260 已固化）：
  H1 手持 model locked 态（icon 面片 vs box 模型像素隔离）/ H2 进食
  举到嘴继承（apple 按住右键 1.6 s = 通道完成存档断言 + lift 态
  截图）/ H3 弓拉继承（bow 按住右键 0.5-1 s，模型整体后拉无弦张角
  = 记档偏离）

## 偏离 / 记档

- **v1 简化体素造型**：axis-aligned box 无旋转无弧（MC primitive
  rotation 不做）——镐/斧对角柄 = 单盒近似、弓 C 弧 = 2 臂盒 + 中部
  开口、苹果 1 盒圆近似；图标绿叶未建模
- **弓弦无动态张角**：弓拉动画态 = 模型整体刚性变换继承（弦不随拉弓
  形变）
- **master_bow 无 model**（5 弓清单外 = 保留 icon 面片手 + 不可见掉落）
- **模型纹理 = icon 双上传**：机制支持 texture ≠ icon 独立纹理，
  shipped 16 件全用 icon（零新纹理 = 免重锚）
- **世界 model 掉落走 blending pass**：icon PNG 带 alpha（opaque 会
  黑盒），双面关裁剪
- **进食举到嘴 = 零额外代码**：手持进食变换（kHandEatLiftY/PullZ）
  自动继承 model 分支

## H 手持输入跑补跑收讫（10-26 19:0x-19:3x）

PITS 247/260 协议（窗口定位 + windowfocus --sync + 点击取锁 +
**"Pointer captured" debug 日志轮询确认锁生效** + R 动作前再 focus——
缺 focus 时 R 边沿丢失 19:2x 实证；首帧 GL 编译期 L/R 同 pre-lock
poll = R 边沿被 !locked 分支丢弃 19:1x 实证）。A/B 对 = 同 binary
v0.30.0 + 剥 model 段资产（世界零污染，19:2x 定案）。

- **H1 手持 model 锁定态 PASS**：iron_pickaxe box 模型 vs icon 面片
  A/B diff 7152 采样 px 质心 (946,587) = 手持区隔离；双侧 ×2 RAW
  字节一致
- **H2 进食继承 PASS**：H2a lift 态（通道中途终帧：apple 在手 +
  饥饿 16 未变 = 1.6 s 通道未完成的中间态实证）+ H2b 通道完成
  （apple 消耗 slot0 空 + 饥饿 16+4=20 exact = model 手继承进食举到
  嘴变换零额外代码实证）
- **H3 弓拉继承 PASS**：H3a 拉状态 box vs icon A/B diff 4345 采样 px
  质心 (922,614) + H3b 射出（bow damage 1 = SURV-3 射出磨损 + arrow
  消耗 = 弓拉模型整体后拉后正常发射实证）
