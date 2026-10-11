# AGENTS.md — 代理快速接收

> Phase 30 文档整理产物。本文件是代理接手本项目的**第一入口**：读完本页即可开工，
> 细节按「文档地图」按需加载。所有规则原文见 `docs/PROTOCOL.md`。

## 项目一句话

可运行、可扩展的 Minecraft 风格 3D 体素沙盒：C++20 / SDL3 / OpenGL 3.3 core，
自研 `Result`/JSON/PNG，**内容全数据驱动**（加方块/物品/配方/Mob = JSON + 纹理，
不改核心代码），**确定性是一级要求**（无输入运行逐帧字节可复现）。

## 现状（Phase 42 完成时）

- 功能阶段 P0–P42 全部完成并提交；P40 = 炉子/熔炼（P28 遗留
  "熟肉"）：`furnace` 方块 + 物品字段 `smeltable_to`/`smelt_time`/`fuel_time`
  + 3 熟肉 + 燃料 + **单个全局 `FurnaceState`**（每帧 1/60 子步 + 整数字步
  倒计时）+ 存档 format 6、P41 = wool 后续（P12/P20 遗留"wool 无用途/
  无染色"）：9 wool 方块（`wool` 白 base + 8 染色变体 `wool_<color>`）+ 9
  染料物品 `dye_<color>` + 9 shapeless 配方 + 方块属性
  `dye_base`/`dye_color`；atlas 4×8→8×8（**所有既有 UV 移位 →
  base18 于 P41 重锚**）、**P42 = 弓后续 2**（P21/P24 遗留）：**3D 手持
  手部**（`drawHeldHand`，仅指针锁定时渲染 → 无输入基线结构性免疫；块物品
  = 3D 方块体 + atlas 光照、图标物品 = 2D 图标贴 3D 平面恒亮；拉弓 = 手部
  后拉 + 弓体后仰、进食 = 抬向面部；2D 图标拉移删除）+ **插墙箭寿命
  `kStuckMaxAge = 100s`**（飞行箭仍 30s，上限按**当前**状态取）+ **方块
  变更释放箭**（`Game::setBlock` 收口；判定 = 箭盒**触**卡住格面且中心在
  面外，非重叠）+ **插墙箭拾取**（stuck 箭距玩家底心 ≤ 1.0 → 返还发射器
  `ammo` 物品；飞行箭不可拾取）+ **击退随蓄力**（`arrowKnockback` 纯函数
  4.0×power / 2.5×power，power 1 = 旧常数逐位）+ **mob `ammo` 字段**
  （远攻必填 + init 校验物品存在，skeleton = arrow）。
- 测试 **769/769**，`-Wall -Wextra -Wpedantic -Werror` 恒 0 警告。
- 无输入基线门：`/tmp/opencode/base18.png`（字节一致；P41 重锚【atlas
  8×8】，P42 过门 = 第 16 次结构性免疫、无重锚）。
- 活跃已知问题：`docs/KNOWN_ISSUES.md`；下一阶段候选：`SESSION_CONTEXT.md`「下一步」。

## 命令

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build  # 构建（0 警告）
./build/bin/minecraft_tests                                                 # 测试（762）
./build/bin/minecraft                                                       # 运行（ESC 退出）
./build/bin/minecraft --frames N [--screenshot /abs.png] [--save slot] [--new] [--config path]
```

游戏必须从仓库 cwd 启动（资源相对路径）；独立测试程序链接
`build/lib/libmc_core.a -lSDL3 -lGLEW -lGL -lz -lpthread`。

## 硬性规则 Top 10（违反即 bug；全文 docs/PROTOCOL.md）

1. **坐标**：world/chunk/local 严格分离，换算只在 `coordinates.hpp`；负坐标 `floorDiv`。
2. **渲染隔离**：游戏逻辑不碰 GL/SDL（renderer/ 与 window/input 边界除外）。
3. **数据驱动**：核心不 switch 方块 id（读 properties）；air 是代码哨兵（无 air.json）。
4. **错误与代码质量**：`Result<T>` + `MC_TRY` 显式传播；**无 TODO/伪代码/魔法数字/
   硬编码方块检查/删功能过构建/禁用测试**。
5. **相机**：yaw 0 = 朝 -Z、正 = 左转；pitch 正 = 上、clamp ±~89°；**鼠标转向仅在
   指针锁定时生效**（无输入运行字节复现的保证）。
6. **玩家物理**：position = AABB 底面中心；重叠判定**严格不等式**（无 epsilon）；
   防穿透 = 子步化（每子步 ≤0.4 块、≤64 子步）+ 终端速度 38。
7. **chunk streaming**：worker job 只在 **post→flush 窗口**运行（窗口内主线程只读
   World）；**光照 job 每帧至多 1 个**（相邻列区域重叠 ±16）。
8. **确定性**：昼夜/Mob/箭/粒子 = 固定 1/60 帧步长；生存计时器（饥饿/进食/破坏/
   坠落/实体 age）= 真实 dt；截图只在帧边界字节一致。
9. **面表绕序**有回归测试守护（`FaceWindingIsCounterClockwiseFromOutside`），勿改
   顶点序；光照 = `max(sky,block)/15 × faceShade`（顶 1.0/±X 0.8/±Z 0.6/底 0.5）。
10. **出生点**：4×4 chunk 演示区列，按 `kSpawnCandidates` 自动找干燥陆地
    （h > seaLevel 且出生 AABB 内无 solid）。

## E2E 协议（摘要；全文 docs/PROTOCOL.md「E2E 协议」）

- **零并发 :0 活动**（用户手离开鼠标）+ 每拍截图前 `unmap_covers`。
- **无输入运行**：×2 全帧字节一致 + skygate（FRESH 模式色）+ base 门
  `/tmp/opencode/base18.png` 字节一致（新系统需论证"无输入下零活动"= 结构性免疫）。
- **输入驱动运行（xdotool 脚本）**：**存档状态（含 yaw/pitch）是确定性权威证据**——
  全帧字节一致受 SDL 指针锁定 grab delta 环境限制（一次性相机旋转 ±0.01 rad = 4 px；
  缓解 = warp 实际原点 + (632,352)、同批同 warp、手不动；存档 yaw/pitch = 漂移探测器）。
- 输入锚定窗口发现时刻，D ∈ [0.1, 0.5] s；tick 类断言需 ≥1.6 s 裕量（1/60 双累加
  边界：36 s 饥饿 tick = 帧 2161，20 s 饥饿伤害 = 帧 1200，1.6 s 进食通道完成 = 帧 97）。
- **HUD 红心检查**（心带 y[646,661] x[551,729]）：无输入运行 = 0（>50 = 污染废拍）；
  输入运行 >50（输入生效）。

## 阶段流程（每个 phase 按此顺序）

1. 用户确认方向 → 2. 实现（数据驱动、无 TODO）→ 3. 构建 + 全量测试（`-Werror`）→
4. E2E（base 门 + 回归对 + 场景专属验证，按上面协议）→ 5. 文档（ROADMAP 行、
KNOWN_ISSUES、SESSION_CONTEXT）→ 6. **显式路径列表** git commit（永不 `git add -A`；
永不提交 `SESSION_CONTEXT.md`（本地））→
7. 中文阶段报告（标准结构：实现内容/变更文件/测试/构建与验证结果/已知问题/下一步建议）。

## 坑位 Top 8（全文 docs/PITS.md，~98 条）

1. **SDL grab delta**（P28）：输入 E2E 全帧字节一致不保证——存档 yaw/pitch 为漂移
   探测器；驱动 warp 用实际窗口原点（实测 (4,64)）+ (632,352)。
2. **物理光标**（P5）：指针未锁定时 SDL 仍投递 mouse motion → 无输入 E2E 必须零
   :0 活动，红心检查 = 污染哨兵。
3. **粒子视觉验证视角**（P29）：直下视角可视锥不含破坏粒子——必须斜视角（pitch
   −1.02 级）；帧差分析先排除已知 UI 变量（饥饿/HP 图标也在 diff 里）。
4. **splitmix64 是纯 finalizer**（P29）：不推进 state——推进式流必须
   `state += 0x9E3779B97F4A7C15ULL` 后再 mix（同 burst 同值缺陷根因）。
5. **`std::string::replace(pos,len,const char*,n)` 截断**（P28）：只取替换串前 n 字符。
6. **`--save slot` 第二次运行会加载第一次写的存档**（P15）：E2E 对跑每拍从源副本
   `cp` 新鲜存档。
7. **按住破坏会自动续破下一块**（P15）：输入脚本的释放时机 = 破坏完成 + 余量，
   余量内的按住对下一块产生确定性裂纹进度。
8. **像素验证**：`glReadPixels` 必须在 `swapBuffers()` 后（前缓冲）；
   `magick ... txt:` 输出 `(r,g,b,a)` 开头 `(` 破坏数值转换，awk 取 `$2`。

## 禁止事项

- `config/config.json` 是**已提交文件**（P32 起：用户调值 view_distance 80 /
  day_length 600 + `player` 默认节）——**不还原用户的调值**；内容改动仅在用户
  要求时做，提交用显式路径。
- **永不 `git add -A`**——提交用显式路径列表。
- `SESSION_CONTEXT.md` 本地专用（`.git/info/exclude`），永不提交。
- 无 TODO/伪代码/硬编码检查/魔法数字；新内容一律走数据文件 + 注册表严格校验。

## 文档地图

| 文档 | 内容 | 何时读 |
|---|---|---|
| [docs/GAMEPLAY.md](docs/GAMEPLAY.md) | 玩法指南（操作/生存循环/战斗与食物/工具路线/昼夜） | 玩法问题 |
| [docs/MECHANICS.md](docs/MECHANICS.md) | 机制详解（截至 P28：生存/Mob/弓/合成/UI/世界生成） | 改玩法逻辑前 |
| [docs/CONTENT_GUIDE.md](docs/CONTENT_GUIDE.md) | 数据驱动内容添加（biome/方块/配方/Mob/hotbar）+ 存档使用 + worldgen 参数 | 加内容/调世界 |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | 目录结构 + 核心架构（模块/线程池/存档格式/帧管线） | 改架构前 |
| [docs/PROTOCOL.md](docs/PROTOCOL.md) | 开发协议：命令/技术栈/硬性约定/测试/**E2E 协议** | 写代码与验证前 |
| [docs/PITS.md](docs/PITS.md) | 坑位合集（~98 条，都踩过） | 遇到怪问题 |
| [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md) | 已知问题清单（活跃）+ 已修复存档索引 | 阶段验收前后 |
| [docs/ROADMAP.md](docs/ROADMAP.md) | Phase 0–29 规划 + 细节存档索引 | 了解历史 |
| [docs/archive/phase-NN.md](docs/archive/) | 单阶段细节存档（阶段报告 + 会话存档） | 深挖某阶段 |
| `SESSION_CONTEXT.md`（本地，不入库） | 会话交接：最新状态/提交列表/下一步 | 任何"继续"会话 |
