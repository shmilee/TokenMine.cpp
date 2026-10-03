# MineCraft

一个以 Minecraft 为设计参考的 3D 体素沙盒游戏，从零构建，目标是**可运行、可扩展、可维护**的长期项目。

## 技术栈

| 领域 | 选择 | 说明 |
|---|---|---|
| 语言 | C++20 | |
| 构建 | CMake + Ninja | |
| 窗口/输入 | SDL3 | |
| 图形 | OpenGL 3.3 Core (GLEW) | |
| 音频 | OpenAL（音频阶段引入） | |
| 图像 | 内置 PNG 编解码器（`src/core/png.hpp`，zlib） | 8/16 位 RGB/RGBA/灰度/调色板，全 filter，CRC 校验 |
| 测试 | GoogleTest | |
| JSON | 内置实现（`src/core/json.hpp`） | 无外部依赖 |

## 构建与运行

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 运行（打开窗口，ESC 或关闭窗口退出）
./build/bin/minecraft

# 运行 120 帧后自动退出（可用于自动化验证）
./build/bin/minecraft --frames 120

# 运行 300 帧并保存最后一帧截图
./build/bin/minecraft --frames 300 --screenshot shot.png

# 其他参数
./build/bin/minecraft --config config/config.json --seed 42 --log-level debug

# 从指定时刻开始（0 = 黎明前, 0.25 = 正午, 0.5 = 日落, 0.75 = 午夜）
./build/bin/minecraft --time 0.75
```

## 当前可玩内容（Phase 8）

- **第一人称玩家**：`W/A/S/D` 行走，`Space` 跳跃（仅贴地时），`LeftShift` 冲刺，
  鼠标转向（**左键点击锁定指针；转向仅在指针锁定时生效**，未锁定的光标
  不会转动视角——保证无输入运行下截图可逐字节复现）
- **方块交互**（DDA 体素 raycast，reach 5 格）：
  - `左键`（锁定时）：破坏准星指向的方块（`properties.destroyable` 数据驱动，
    水/基岩不可破坏）
  - `右键`（锁定时）：在命中面的邻格放置当前选中的方块（不能放进 solid 方块
    或玩家自身包围盒内）
  - `Tab`：切换选中方块（`assets/data/hotbar.json` 数据驱动列表，临时方案，
    Phase 11 换成物品/背包）
  - `ESC`（锁定时）：释放指针；未锁定时 ESC 退出游戏
  - 破坏/放置复用 `setBlock` 脏标记管线，下一帧自动重建受影响 chunk 的 mesh
- **AABB 体素碰撞 + 滑动**：玩家 0.6×1.8 包围盒（底面中心为位置），轴分离解析
  （X→Z→Y）——撞墙时该轴停住、另一水平轴继续走（滑动）；贴面精确解析（无 epsilon
  抖动）；子步化防高速穿透（每子步 ≤0.4 块）；重力 + 终端速度 + 跳跃高度 1.13 块
  （可跳上 1 格台阶，跳不上 2 格）；掉出世界（y < -32）回出生点
- **程序化体素世界（Seed 可复现）**：16×16×16 chunk，世界高度 128（y ∈ [0,128)），X/Z 无限。
  同一 seed 逐块复现（单元测试 + 像素级截图比对保证）；不同 seed 生成不同世界
- **WorldGenerator**（无状态、纯函数 `generateChunk`，Phase 14 可直接并行）：
  - **地形**：fBm 2D 噪声高度场（±24 格，基线 40），y=0 bedrock 层，深处 stone
  - **生物群系**：低频噪声 + 权重选择（`assets/data/biomes/*.json`：desert/forest/plains），
    各 biome 有自己的表层/次表层/树密度/高度系数
  - **水**：低于海平面（y=34）的柱体填充水到海平面（静态，流体模拟在 Phase 9）
  - **洞穴**：3D 噪声阈值雕刻，保留表层 4 格与底层 2 格的壳
  - **矿脉**：世界空间格网 + 随机游走（coal/iron/gold/diamond，`max_y` 分层限制），
    跨 chunk 无缝；只替换 stone（不破坏洞穴）
  - **树木**：世界空间格网候选 + 数据驱动的树密度；树干 4~7 格 + 两层树冠 + 顶帽；
    跨 chunk 边界由邻接 chunk 各自补齐（边际格网机制），只写入 air（不破坏地形）
- **演示区**：4×4 chunk 列（64×64 方块）；玩家按 seed 自动出生在干燥陆地上
  （候选列需满足：高于海平面 + 出生包围盒内无方块，避开树干/树冠）
- **Face Culling 网格化**：只渲染暴露给 air/非 opaque 邻居的面；跨 chunk 边界通过
  `World` 查询邻居剔除；每个 chunk 输出 opaque + transparent 两个 mesh，
  渲染顺序不透明 → alpha 混合（leaves/glass/water）
- **脏标记增量重网格**：`setBlock` 标记本 chunk（及共享面的邻居 chunk）dirty，
  下一帧只重建 dirty chunk 的 GPU mesh
- **光照（Sky/Block Light 0-15）**：每个 chunk 两个 16³ 字节数组；天空光由列顶扫描
  （最高 opaque 方块上方=15）+ BFS 传播（垂直无衰减、水平每格 -1）；方块光由
  `properties.light_emission` 源 BFS（全方向每格 -1）。破坏/放置后只重传播受影响区域
  （天空光按变化点 ±16 的整列——垂直天空光无距离限制；方块光按 ±16 盒），光值实际
  变化的 chunk 才重建 mesh。渲染：每个面烘焙"面后方一格"的光值到顶点（块状光照），
  shader 取 `max(sky, block)` × MC 经典面着色（顶 1.0 / 侧 0.8、0.6 / 底 0.5）+ 3%
  环境光下限。glowstone（emission 15）在 hotbar 里，放进暗处会发光（**夜间也亮**——
  方块光不受昼夜影响）
- **昼夜循环**：游戏内时间（`world.day_length_seconds` 默认 240s 一天，
  `world.start_time_of_day` 默认 0.25=正午；`--time <0..1>` 可覆盖起点）。天空光按
  `dayFactor` 缩放（黎明 [0.05,0.15) / 黄昏 [0.45,0.55) smoothstep 渐变，白天=1、
  夜晚=0——MC 式呈现层近似，光值本身仍按正午存储）；天空背景色按时间关键帧插值
  （夜深蓝 → 黎明橙 → 昼蓝 → 黄昏橙 → 夜深蓝）；太阳/月亮是像素级圆形 billboard
  （40/28px，越靠近地平线越淡），太阳轨迹：t=0 正东地平线 → 正午天顶 → t=0.5 正西
  地平线 → 午夜天底（月亮恒在太阳对面）；夜空有 300 颗确定性星星（世界空间
  r=800 上半球，`splitmix64` 哈希生成）。时间按每帧固定 1/60s 推进（不用墙钟），
  保证无输入运行逐字节可复现
- **数据驱动方块**：14 个方块（stone/dirt/grass/sand/wood/leaves/glass/water/bedrock/glowstone +
  4 种矿石）+ 内置 air 哨兵（`BlockID{0}`，非数据方块，注册表从 id 1 分配）
- 16×16 瓦片 TextureAtlas（Minecraft 同款规格），16 张占位纹理
- 光照贴图（lightmap 顶点烘焙，Phase 7）+ 昼夜循环（Phase 8）
- 窗口标题实时显示 FPS 与 draw call 数

### 新增一个生物群系 / 矿石（无需改核心代码）

1. 生物群系：`assets/data/biomes/<name>.json` —— 权重/表层/次表层/深度/树密度/高度系数：

   ```json
   {
     "id": "swamp",
     "weight": 3,
     "surface": "grass",
     "subsurface": "dirt",
     "subsurface_depth": 3,
     "tree_density": 0.08,
     "height_scale": 0.8
   }
   ```

2. 矿石：`assets/data/worldgen.json` 的 `ores.veins` 数组加一项（方块需已注册）：

   ```json
   { "block": "diamond_ore", "probability": 0.05, "vein_size": 5, "max_y": 20 }
   ```

地形/洞穴/树木参数同样在 `assets/data/worldgen.json`（缺省=默认值，类型错误=启动报错）。

### 调整可放置方块列表（hotbar）

`assets/data/hotbar.json` 的 `blocks` 数组（游戏内 `Tab` 循环切换）：

```json
{ "blocks": ["stone", "dirt", "grass", "sand", "wood", "leaves", "glass", "glowstone"] }
```

方块名必须已注册（未知名=启动报错）；列表不能为空。

### 新增一个方块（无需改核心代码）

1. `assets/textures/blocks/<name>.png` —— 16×16 RGBA 纹理（可参考 `tools/generate_block_textures.py`）
2. `assets/data/blocks/<id>.json` —— 方块定义（属性/掉落/模型/纹理），例如：

   ```json
   {
     "id": "glowstone",
     "properties": { "hardness": 0.3, "light_emission": 12, "drops": "glowstone" },
     "model": { "textures": { "top": "glowstone", "side": "glowstone", "bottom": "glowstone" } }
   }
   ```

   属性缺省即默认值（solid/opaque/destroyable=true, hardness=1.0, light_emission=0, drops 为空=不掉落）。
   校验严格：类型错误、越界值、重复 id、缺失纹理面都会报明确错误；未知字段警告但不拒绝。

## 测试

```bash
ctest --test-dir build --output-on-failure
# 或直接运行
./build/bin/minecraft_tests
```

## 目录结构

```
mine-craft/
├── assets/          # 游戏资源（textures/ models/ shaders/ sounds/ data/）
├── config/          # 用户配置（config.json，不存在时自动生成默认值）
├── data/            # 存档目录（后续阶段使用）
├── logs/            # 运行日志（logs/mine-craft.log）
├── src/
│   ├── block/       # 数据驱动方块系统：Block 数据模型 + BlockRegistry（JSON 加载/校验）
│   ├── core/        # 核心框架：日志、错误处理、JSON、配置、资源、窗口、输入、相机、PNG、主循环
│   │   └── math/    # 数学库（header-only）：Vec2/Vec3/Vec4/Mat4
│   ├── player/      # 玩家：第一人称状态 + AABB 体素碰撞/滑动/重力跳跃（驱动相机）
│   ├── renderer/    # 渲染器（GL 封装）：Shader/Texture/Mesh/Renderer/TextureAtlas
│   └── world/       # 体素世界：coordinates（world/chunk/local 转换）、Chunk（块+光照数组）、
│                    # World、ChunkMesher（按面烘焙光照）、day_cycle（昼夜循环：时间/
│                    # dayFactor/天空色/日月方位/确定性星空）、raycast（DDA 体素射线+放置校验）、
│                    # lighting（Sky/Block Light 0-15 传播与局部更新）、noise（确定性
│                    # value noise/fBm）、BiomeRegistry、WorldgenParams（worldgen.json）、
│                    # WorldGenerator（Seed 可复现生成）
├── tools/           # 开发工具：generate_block_textures.py（16x16 占位纹理生成）
└── tests/           # 单元测试（GTest）+ PNG 测试夹具（tests/data/，可用 tests/generate_fixtures.py 重新生成）
```

## 架构规划（分阶段）

- [x] **Phase 0** — 项目基础：构建系统、主循环、日志、配置、资源系统、错误处理、测试框架
- [x] **Phase 1** — 基础 3D 引擎：数学库、Input、Camera、Renderer（Shader/Texture/Mesh/VAO）、PNG 编解码、demo 场景
- [x] **Phase 2** — 方块系统：Block 数据模型、BlockRegistry（JSON 数据驱动 + 严格校验）、16×16 TextureAtlas、透明 pass
- [x] **Phase 3** — Chunk 系统：World/Chunk/坐标转换（负坐标安全）/Chunk Mesh（Face Culling；Greedy Meshing 留待性能需要时）
- [x] **Phase 4** — 世界生成：WorldGenerator（地形/生物群系/洞穴/矿脉/树木），Seed 可复现（无状态生成器，数据驱动 biome/ore/参数）
- [x] **Phase 5** — 玩家系统：第一人称移动、AABB 碰撞、滑动处理
- [x] **Phase 6** — Raycast：方块破坏/放置
- [x] **Phase 7** — 光照：Sky/Block Light（0-15），传播与更新
- [x] **Phase 8** — 昼夜循环：太阳/月亮/星星/天空色（dayFactor 缩放天空光）
- [ ] **Phase 9** — 流体：水（FluidState/FluidLevel/水平扩散）
- [ ] **Phase 10** — 实体系统：Entity/Player/ItemEntity/Projectile
- [ ] **Phase 11** — 物品与背包：Item/ItemStack/Inventory
- [ ] **Phase 12** — 合成：数据驱动 Recipe
- [ ] **Phase 13** — 存档：Seed + 修改数据
- [ ] **Phase 14** — Chunk Streaming + 多线程（Job Queue/Thread Pool）
- [ ] **Phase 15** — UI / 音效 / 粒子 / 性能统计（F3 Debug Overlay）

## 开发约定

- 游戏逻辑不直接调用 GL/SDL API —— 图形走 Renderer（Phase 1），窗口走 `Window`。
- 数据驱动：方块/物品/配方通过注册表（Registry）管理，新增内容 = 资源 + 数据，不改核心代码。
- 世界坐标 / Chunk 坐标 / Local 坐标严格区分，转换函数集中定义。
- 错误必须显式传播（`Result<T>` + `MC_TRY`），不允许静默失败。
- 每个阶段完成后：编译 → 运行 → 测试 → 修复 → 总结，再进入下一阶段。
