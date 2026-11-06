<div align="center">

<h1>
  <img alt="TokenMine.cpp — 立体分层合成图（暗色 banner）：底层架构总览 · 上层帧管线，近大远小轴测层" src="docs/architecture-hero.svg">
</h1>

**本地 LLM 编写的 C++ 版 Minecraft：可运行、可扩展的 3D 体素沙盒<br>
（C++20 / SDL3 / OpenGL 3.3，自研 JSON/PNG，内容全数据驱动）。<br>
确定性是一级要求——无输入运行逐帧字节可复现。<br>
当前 v3.19.0（V3-14a 爆炸机制：衰减伤害/破块/点火 + blast_resistance 数据属性 + --explode seam；3.x 矩阵推进中）· tag `v3.19.0` · 任务表见 [docs/BACKLOG.md](docs/BACKLOG.md)**

[![GitHub stars](https://img.shields.io/github/stars/shmilee/TokenMine.cpp?style=flat-square)](https://github.com/shmilee/TokenMine.cpp/stargazers)
[![MIT license](https://img.shields.io/badge/license-MIT-555555?style=flat-square)](LICENSE)

**[项目名](#项目名) · [技术栈](#技术栈) · [构建与运行](#构建与运行) · [版本与发布](#版本与发布) · [文档地图](#文档地图) · [开发约定](#开发约定)**

</div>

## 项目名

**TokenMine.cpp** —— 一语三关：

- **Token**：这座矿是 token 一行行"挖"出来的——整个项目由本地 LLM 编写；
- **Mine**：Minecraft 的"挖矿"，也是英语的 "my"（这座矿是我的）；
- **.cpp**：程序员圈的命名梗，"X.cpp" 即 "X 的 C++ 重写版"。
- **起源与实现**：P0 之前的《开发总提示词》由 ChatGPT 起草（1164 行原始文本
  未做任何改动，见 [initial-chatgpt-prompt.md](initial-chatgpt-prompt.md)）；
  但自 P0 起，每一步——设计、实现、测试、文档——全部由本地 LLM
  **Qwen3.8 27B** 完成。ChatGPT 只参与了"画图纸"，挖矿的全程是本地 LLM 干完的。

## 技术栈

| 领域 | 选择 | 说明 |
|---|---|---|
| 语言 | C++20 | |
| 构建 | CMake + Ninja | |
| 窗口/输入 | SDL3 | |
| 图形 | OpenGL 3.3 Core (GLEW) | |
| 音频 | SDL3 Audio（`SoundBank`，SDL 内置音频后端） | 63 个程序化 WAV（确定性生成，含材质音效 + 4 上下文环境音 + 下界音效族） |
| 图像 | 内置 PNG 编解码器（`src/core/png.hpp`，zlib） | 8/16 位 RGB/RGBA/灰度/调色板，全 filter，CRC 校验 |
| 测试 | GoogleTest | |
| JSON | 内置实现（`src/core/json.hpp`） | 无外部依赖 |

## 构建与运行

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 运行（打开窗口，ESC 或关闭窗口退出）
./build/bin/tokenmine

# 运行 120 帧后自动退出（可用于自动化验证）
./build/bin/tokenmine --frames 120

# 运行 300 帧并保存最后一帧截图
./build/bin/tokenmine --frames 300 --screenshot shot.png

# 其他参数
./build/bin/tokenmine --config config/config.json --seed 42 --log-level debug

# 看预设教程 demo（只观察：转镜头/切视角/0.5×~2× 变速/暂停，Q 退出）
./build/bin/tokenmine --demo assets/demos/tutorial_fly.dmo   # 双击 Space 飞行
./build/bin/tokenmine --demo assets/demos/tutorial_dig.dmo   # 挖掘 + 换工具
./build/bin/tokenmine --demo assets/demos/tutorial_build.dmo # 数字键 + 环绕建造
./build/bin/tokenmine --demo assets/demos/example.dmo        # 基础操作巡礼

# 从指定时刻开始（0 = 黎明前, 0.25 = 正午, 0.5 = 日落, 0.75 = 午夜）
./build/bin/tokenmine --time 0.75

# 存档槽（Phase 13）：启动时加载 data/saves/<slot>.json（若存在），干净退出时写回
./build/bin/tokenmine --save myworld
# 忽略现有存档、从全新世界开始（退出时覆盖该槽）
./build/bin/tokenmine --save myworld --new
```

**窗口**：大小由 `config/config.json` 的 `window.width`/`window.height` 决定（默认
1280×720）。窗口模式下窗口可被用户缩放（`window.resizable`，默认 `true`）——
视口、投影纵横比与屏幕 UI 布局每帧读取窗口当前像素尺寸，缩放后场景立即跟随
（最小尺寸 320×200）；设为 `false` 则锁定在配置尺寸。

**内存提示**：默认 `config/config.json` 为 `view_distance: 80`（圆盘环
满环 ~103k chunk ≈ 4.6 GB，会随探索持续加载）。内存 <8 GB 的机器建议
改为 16–32（`world.view_distance`，16 ≈ 2 GB 量级）。

## 构建依赖

**平台**：Linux 验证（Arch；CMake 3.25+ / Ninja）。macOS / Windows 未测试
（SDL3 与 GLEW 在对应平台的可用性未验证）。

| 依赖 | 用途 | Arch 安装 |
|---|---|---|
| CMake ≥ 3.25 + Ninja | 构建 | `sudo pacman -S cmake ninja` |
| SDL3 | 窗口/输入/音频 | `sudo pacman -S sdl3` |
| GLEW + OpenGL 3.3 | 图形（需驱动，Intel iGPU 验证） | `sudo pacman -S mesa libglvnd-glew` |
| zlib | 内置 PNG 解码 | 系统自带 |
| GTest | 单元测试 | `sudo pacman -S googletest` |

其他发行版：装齐上表五项即可（`find_package`/`pkg_check_modules` 自动发现）。

## 版本与发布

- **v0.1.0**：可玩的 3D 体素沙盒（世界生成 / 昼夜光照 / 流体 / Mob 战斗 / 合成 / 存档）
- **v1.0.0**（1.x）：十大系统完成，生存玩法闭环——火与工具分类、护甲耐久、药水效果、第三人称、教程
- **v2.0.0**（2.x）：上手与体验补全——XP 与 7 种附魔、染色床睡觉跳夜、蝙蝠/染色羊/雪原、AO 环境光遮蔽、天气、飞行与滚轮缩放
- **v3.0.0**（3.x，当前）：**世界扩展**——下界维度（传送门 / 5 群系 / 熔岩海 / 5 专属 mob）+ netherite 材料链 + 建筑内容族（黏土/石砖/陶瓦）+ 存档 gzip + 手柄 + **demo 回放（录屏式只观察教程，含 4 个预设教程 demo + 独立 sim 版本号）**；v3.12.0 矩阵后报障修复 + v3.14.0 手柄习惯映射定案
- 方向项（末地 / 创造 / 多人 / 死亡统计屏）待立项

逐版明细与发布历史：[`docs/CHANGELOG.md`](docs/CHANGELOG.md)（当前时
代）+ `docs/archive/*/CHANGELOG.md`（历史时代）+ GitHub Releases。

> 存档与内容数据格式仍可能随阶段演进（存档向前迁移兼容）——旧存档通常
> 可读，但请定期备份。已知限制见 `docs/KNOWN_ISSUES.md`。

## 文档地图

文档已按主题分块：**新会话先读 [AGENTS.md](AGENTS.md)**，再按需读下表中的文件。历史细节全部在 `docs/archive/`。

| 文档 | 内容 | 何时读 |
|---|---|---|
| [AGENTS.md](AGENTS.md) | 代理快速接收：现状/命令/硬性规则/E2E 协议/阶段流程/坑位速览 | 新会话第一件事 |
| [docs/GAMEPLAY.md](docs/GAMEPLAY.md) | 玩法指南（操作/生存循环/下界/netherite/建筑材料/**demo 回放教程**；2026-11-06 全量刷新） | 玩法问题 |
| [docs/MECHANICS.md](docs/MECHANICS.md) | 机制详解（现状 v3.16.0，17 子系统分节含下界维度/demo 回放/死亡统计屏/传送门动画；2026-11-06 全量刷新；世界/渲染侧 = `docs/MECHANICS_WORLD.md`） | 改玩法逻辑前 |
| [docs/CONTENT_GUIDE.md](docs/CONTENT_GUIDE.md) | 数据驱动内容添加（biome/下界群系/方块/配方/Mob/下界池/附魔/sediment/worldgen 全参考）+ 配置/存档/资产纪律 = `docs/CONTENT_GUIDE_CONFIG.md` | 加内容/调世界 |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | 目录结构 + 核心架构（模块/线程池/存档格式/帧管线） | 改架构前 |
| [docs/PROTOCOL.md](docs/PROTOCOL.md) | 开发协议：命令/技术栈/硬性约定/测试/**E2E 协议** | 写代码与验证前 |
| [docs/PITS.md](docs/PITS.md) | 坑位合集（398 条，都踩过；pre-3.x → `docs/archive/PITS_PRE3X.md`） | 遇到怪问题 |
| [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md) | 已知问题清单（活跃条目；已修复（含修复史）→ `docs/archive/KI_FIXED.md`；pre-2.0 已修复索引 → `docs/archive/PRE2.0-FIXED-INDEX.md`） | 阶段验收前后 |
| [docs/archive/v0x/ROADMAP.md](docs/archive/v0x/ROADMAP.md) | 0.x 时代 Phase 0–29 规划（已归档） | 了解历史 |
| [docs/CHANGELOG.md](docs/CHANGELOG.md) | 版本历史（3.x 时代逐版明细，v3.0.0 起；v2.x 全两批 → `docs/archive/v2x/CHANGELOG.md`、v1.x → `docs/archive/v1x/CHANGELOG.md`、v0.x → `docs/archive/v0x/CHANGELOG.md`） | 查某版改了什么 |
| [docs/BACKLOG.md](docs/BACKLOG.md) | **3.x「世界扩展」任务表（执行权威，11 项矩阵 + 报障版全完成）**；2.x 全任务表 → `docs/archive/v2x/BACKLOG.md` | 取下一个任务 |
| [docs/archive/](docs/archive/) | 阶段细节存档（v2x/：20 phase 全两批 + HISTORY.md 全史 + v2x/CHANGELOG.md + v2x/BACKLOG.md；1.x：181 phase + HISTORY.md 全史；+ PITS_PRE3X.md pre-3.x 坑位 + KI_FIXED.md 已修复条目存档） | 深挖某阶段 |
| [docs/archive/v0x/](docs/archive/v0x/) | 0.x 时代文档（ROADMAP 规划 / DISCUSSION 记录 / v0x/CHANGELOG.md） | 深挖 0.x |
| `opencode-sessions/SESSION_CONTEXT.md`（本地，不入库） | 会话交接：最新状态/提交列表/下一步（历史块/遗留附录 → 同目录 `archive/`；新会话启动 prompt 同目录） | 任何"继续"会话 |

## 开发约定

- 游戏逻辑不直接调用 GL/SDL API —— 图形走 Renderer（Phase 1），窗口走 `Window`。
- 数据驱动：方块/物品/配方通过注册表（Registry）管理，新增内容 = 资源 + 数据，不改核心代码。
- 世界坐标 / Chunk 坐标 / Local 坐标严格区分，转换函数集中定义。
- 错误必须显式传播（`Result<T>` + `MC_TRY`），不允许静默失败。
- 每个阶段完成后：编译 → 运行 → 测试 → 修复 → 总结，再进入下一阶段。

## 许可证

MIT（见 `LICENSE`）。
