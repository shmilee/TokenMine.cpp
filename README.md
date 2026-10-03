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
| 图像 | SDL3_image（纹理阶段引入） | |
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

# 其他参数
./build/bin/minecraft --config config/config.json --seed 42 --log-level debug
```

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
│   └── core/        # 核心框架：日志、错误处理、JSON、配置、资源、窗口、主循环
└── tests/           # 单元测试（GTest）
```

## 架构规划（分阶段）

- [x] **Phase 0** — 项目基础：构建系统、主循环、日志、配置、资源系统、错误处理、测试框架
- [ ] **Phase 1** — 基础 3D 引擎：Renderer、Camera、Shader/Texture/Mesh/Buffer
- [ ] **Phase 2** — 方块系统：Block/BlockState/BlockRegistry，数据驱动
- [ ] **Phase 3** — Chunk 系统：World/ChunkManager/坐标转换/Chunk Mesh（Face Culling → Greedy Meshing）
- [ ] **Phase 4** — 世界生成：WorldGenerator（地形/生物群系/洞穴/矿脉/树木），Seed 可复现
- [ ] **Phase 5** — 玩家系统：第一人称移动、AABB 碰撞、滑动处理
- [ ] **Phase 6** — Raycast：方块破坏/放置
- [ ] **Phase 7** — 光照：Sky/Block Light（0-15），传播与更新
- [ ] **Phase 8** — 昼夜循环：太阳/月亮/天空色
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
