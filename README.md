<div align="center">

<h1>
  <img alt="TokenMine.cpp — 立体分层合成图（暗色 banner）：底层架构总览 · 上层帧管线，近大远小轴测层" src="docs/architecture-hero.svg">
</h1>

**本地 LLM 编写的 C++ 版 Minecraft：可运行、可扩展的 3D 体素沙盒<br>
（C++20 / SDL3 / OpenGL 3.3，自研 JSON/PNG，内容全数据驱动）。<br>
确定性是一级要求——无输入运行逐帧字节可复现。<br>
v2.0.0 已发布（1.x 收官里程碑）· v2.x 进行中（V2-1 前视角 / V2-2 Steve 皮肤已完成，任务表见 [docs/BACKLOG.md](docs/BACKLOG.md)）**

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
| 音频 | SDL3 Audio（`SoundBank`，SDL 内置音频后端） | Phase 15 引入，38 个程序化 WAV（确定性生成，含 CRAFT-3 材质音效 + PRE-4 环境音） |
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

- **v2.10.0**（2026-11-03）：**2.x「上手与体验补全」收官里程碑**（tag
  `v2.10.0`）——任务矩阵 V2-1~V2-10 十项全完成（门 171-181 全 PASS，
  八版每版发布前性能 A/B 全同世界同成本）：第三人称前视角 / Steve
  皮肤（per-face 纹理通用能力）/ 教程系列化（format 25）/ 天气（雨雷）
  / 猪 / 体验打磨批（手臂/阴影/磁力/月相）/ 材料补齐批（块级 per-face
  + 金钻块 + spider 掉落）/ sky rescan clobber 修复 / step-up quirk
  修复 / 网格修复批（叶子面剔除 + cutout 普查确认）。2181/2181 测试；
  2.x 全史 = `docs/archive/v2x/HISTORY.md`（+ 同目录 BACKLOG/CHANGELOG/
  phase 存档 11 份）。**收官日定案：原 3.x 八项并入 2.x 第二批
  （V2-11~V2-18，`docs/BACKLOG.md` 执行权威）；原 4.x「世界扩展」
  保持大版本。**
- **v2.0.0**（2026-11-02）：**1.x 收官里程碑**——任务矩阵 40/40 + 两轮实机报障
  系列（UX-8g/UX-9：云方块化/教程火把步/洞穴底光可配置）全完成 + 发布扫尾
  （1.x 文档归档 `docs/archive/v1x/` / 第三人称准星不压头 / 活文档刷新 2.0 状态）。
  2075/2075 测试。Release 见 GitHub Releases "TokenMine.cpp v2.0.0"。
  1.x 全史 = `docs/archive/v1x/HISTORY.md`。
- **v1.0.0**（2026-10-27）：**首个正式版**，任务矩阵 34/34 全完成
  （十大系统：流送内存/帧管线/光照/世界生成/流体/实体 Mob/生存战斗/物品
  合成/存档/表现层 + 2 项实机报障修复）。正式版 release 见 GitHub
  Releases "TokenMine.cpp v1.0.0"。v1.0 后进入 v1.x 迭代（任务矩阵已刷新，
  见 `docs/BACKLOG.md`）。
- **v0.1.0**（2026-10-17，首个公开版）：功能阶段 P0–P99 + 性能系列 99.x
  全部完成；1195/1195 测试、`-Werror` 0 警告。

v0.2.0 → v2.10.0 逐版明细：v2.x 见 [`docs/archive/v2x/CHANGELOG.md`](docs/archive/v2x/CHANGELOG.md)；v3.x 起见 [`docs/CHANGELOG.md`](docs/CHANGELOG.md)；
v1.x（v1.50.0→v1.0）见 `docs/archive/v1x/CHANGELOG.md`；v0.x
（v0.12.0→v0.1.0）见 `docs/archive/v0x/CHANGELOG.md`；每版工程细节
（机制/证据/偏离/性能 A/B）见 `docs/archive/*/phase-*.md` 阶段存档与
GitHub Releases。
- **0.x 承诺**：可玩、可扩展、确定性运行；但**不承诺稳定**——存档格式
  （现为 format 25，向前迁移）与内容数据格式仍可能随阶段演进，旧存档
  通常可读但请定期备份。已知限制见 `docs/KNOWN_ISSUES.md`。
- 许可证：MIT（见 `LICENSE`）。
- **内存提示**：默认 `config/config.json` 为 `view_distance: 80`（圆盘环
  满环 ~103k chunk ≈ 4.6 GB，会随探索持续加载）。内存 <8 GB 的机器建议
  改为 16–32（`world.view_distance`，16 ≈ 2 GB 量级）。

## 文档地图

文档已按主题分块：**新会话先读 [AGENTS.md](AGENTS.md)**，再按需读下表中的文件。历史细节全部在 `docs/archive/`。

| 文档 | 内容 | 何时读 |
|---|---|---|
| [AGENTS.md](AGENTS.md) | 代理快速接收：现状/命令/硬性规则/E2E 协议/阶段流程/坑位速览 | 新会话第一件事 |
| [docs/GAMEPLAY.md](docs/GAMEPLAY.md) | 玩法指南（操作/生存循环/战斗与食物/工具路线/昼夜） | 玩法问题 |
| [docs/MECHANICS.md](docs/MECHANICS.md) | 机制详解（截至 P28：生存/Mob/弓/合成/UI/世界生成） | 改玩法逻辑前 |
| [docs/CONTENT_GUIDE.md](docs/CONTENT_GUIDE.md) | 数据驱动内容添加（biome/方块/配方/Mob/hotbar）+ 存档使用 + worldgen 参数 | 加内容/调世界 |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | 目录结构 + 核心架构（模块/线程池/存档格式/帧管线） | 改架构前 |
| [docs/PROTOCOL.md](docs/PROTOCOL.md) | 开发协议：命令/技术栈/硬性约定/测试/**E2E 协议** | 写代码与验证前 |
| [docs/PITS.md](docs/PITS.md) | 坑位合集（383 条，都踩过） | 遇到怪问题 |
| [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md) | 已知问题清单（活跃；pre-2.0 已修复索引 → `docs/archive/PRE2.0-FIXED-INDEX.md`） | 阶段验收前后 |
| [docs/archive/v0x/ROADMAP.md](docs/archive/v0x/ROADMAP.md) | 0.x 时代 Phase 0–29 规划（已归档） | 了解历史 |
| [docs/CHANGELOG.md](docs/CHANGELOG.md) | 版本历史（v2.11.0 起逐版明细；2.x 第一批 → `docs/archive/v2x/CHANGELOG.md`、v1.x → `docs/archive/v1x/CHANGELOG.md`、v0.x → `docs/archive/v0x/CHANGELOG.md`） | 查某版改了什么 |
| [docs/archive/](docs/archive/) | 阶段细节存档（v2x/：11 phase + HISTORY.md 全史 + v2x/CHANGELOG.md + v2x/BACKLOG.md；1.x：181 phase + HISTORY.md 全史；2.x 二批新阶段按同款） | 深挖某阶段 |
| [docs/archive/v0x/](docs/archive/v0x/) | 0.x 时代文档（ROADMAP 规划 / DISCUSSION 记录 / v0x/CHANGELOG.md） | 深挖 0.x |
| `opencode-sessions/SESSION_CONTEXT.md`（本地，不入库） | 会话交接：最新状态/提交列表/下一步（历史块/遗留附录 → 同目录 `archive/`；新会话启动 prompt 同目录） | 任何"继续"会话 |

## 开发约定

- 游戏逻辑不直接调用 GL/SDL API —— 图形走 Renderer（Phase 1），窗口走 `Window`。
- 数据驱动：方块/物品/配方通过注册表（Registry）管理，新增内容 = 资源 + 数据，不改核心代码。
- 世界坐标 / Chunk 坐标 / Local 坐标严格区分，转换函数集中定义。
- 错误必须显式传播（`Result<T>` + `MC_TRY`），不允许静默失败。
- 每个阶段完成后：编译 → 运行 → 测试 → 修复 → 总结，再进入下一阶段。
