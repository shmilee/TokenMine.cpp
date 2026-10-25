# TokenMine.cpp

本地 LLM 编写的 C++ 版 Minecraft：可运行、可扩展的 3D 体素沙盒
（C++20 / SDL3 / OpenGL 3.3 core，
自研 Result/JSON/PNG，内容全数据驱动）。**确定性是项目一级要求**：无输入运行逐帧
字节可复现。功能阶段 P0–P99 + 性能审计系列 99.x 全部完成并提交，测试
1195/1195。

> 文档已按主题分块（Phase 30）：**新会话先读 [AGENTS.md](AGENTS.md)**（代理快速
> 接收），再按需读下表中的文件。原文档（README 765 行 / KNOWN_ISSUES / 本地
> SESSION_CONTEXT 2355 行）已拆分，历史细节全部在 `docs/archive/`。

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
| 音频 | SDL3 Audio（`SoundBank`，SDL 内置音频后端） | Phase 15 引入，17 个程序化 WAV |
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

- **v0.18.0**（2026-10-25）：**GEN-5 浮空树**（任务表 20/31，用户报障
  修复）——洞穴挖掉的地面上不再长树（caveCarvesCell 纯谓词守卫，
  carveCaves 行为恒等，零 format/config/content）。
- **v0.17.0**（2026-10-25）：**GEN-4 洞穴破碎度**（任务表 19/31，用户
  报障修复）——洞穴重标定：洞体更大更少，小洞窟 −91%，中位洞体积
  9-130×（noise_scale 0.09→0.05 单参数，零核心改动）。
- **v0.16.0**（2026-10-25）：**SURV-4 药水饮用**（任务表 18/29）——6 种
  单效果药水（虚弱 1:30 / 迅捷 3:00 / 再生 0:45 / 中毒 0:45 / 治疗 +4
  瞬时 / 伤害 −6 瞬时护甲豁免）+ 玻璃瓶舀水-倒水循环 + `instant_hp`
  瞬时效果原语；喝 = 按住右键 1.6 s 返还玻璃瓶，再喝同效果重置不叠加。
  无酿造台（starter 发放，记档偏离）。format 18 不变。
- **v0.15.0**（2026-10-25）：**SURV-3 耐久**（任务表 17/29）——27 件工具/护甲/弓
  MC 精确耐久值 + 五类磨损（破块/打 mob/射出/点燃/护甲减伤事件）+ format 18 +
  HUD 2px 耐久条；无修复系统（记档偏离）。
- **v0.14.0**（2026-10-25）：**SURV-2 氧气窒息**（16/29）——水下氧气条 + 溺水伤害
  （帧锁定 15 s 空气 / 每 2.5 s 2 HP，溺水豁免护甲减伤 = MC 伤害表实证），format 17。
- **v0.13.0**（2026-10-25）：**SURV-1 护甲**（15/29）——12 件护甲 MC 精确值 + 整数
  floor 减伤（全伤害源含坠落）+ 右键穿戴/交换/脱下，format 16。
- **v0.12.0**（2026-10-25）：**ENT-4 水体交互**（14/29，ENT 行收尾）——mob/掉落物
  浮力（被动上浮/潜行潜水/水面漂浮），陆上行为逐位不变。
- **v0.11.0**（2026-10-25）：**ENT-3 mob 踏阶**（13/29）+ **ENT-1b 模型修复**
  （实机反馈）——mob 可上 1 格台阶（与 player 同判定）；四足比例/关节/穿模修正。
- **v0.10.0**（2026-10-25）：**ENT-1 四肢摆动**（12/29）——mob 行走摆腿动画：
  纯渲染层，数据驱动 `swings` 段 + 编译期定点正弦表（零运行期 libm）。
- **v0.9.0**（2026-10-24）：**99.11 流体光照异步化**——“转摄像头卡”深层根因修复：
  列粒度 worker 追赶，海洋世界静置 update p95 秒级 → ~90 ms，vd16 冷填充 5.5×。
- **v0.8.0**（2026-10-24）：**99.10 光照 job 并行**——流送期光照下限 16.7 → ~10 ms；
  新旧二进制 300 帧字节一致（confluence 证明）。
- **v0.7.0**（2026-10-24）：**游泳划水**（FLUID-2）——被动上浮/潜行潜水/水面跳出，
  对齐 MC。
- **v0.6.0**（2026-10-23）：**水量守恒**（FLUID-1）——每格流体变化归入具名事件
  （会计恒等式单测钉死），熔岩同款。
- **v0.5.0**（2026-10-22）：**矿脉形态 MC 化**（GEN-3，GEN 行收尾）——3×2×1 旋转
  椭球脉；旧存档可读。
- **v0.4.0**（2026-10-22）：**树生长**（GEN-2）——3 树种可种植树苗 + 数据驱动树形
  生长（format 15）。
- **v0.3.0**（2026-10-22）：**BlockState 通用 per-block 状态**（ENT-2）——楼梯朝向
  泛化为状态机制（format 15，旧存档直接可读）。
- **v0.2.0**（2026-10-22）：**任务表前五项**（STRM-1/PIPE-1/PIPE-2/LGT-2/GEN-1）
  ——流送环圆盘化（vd80 内存 −49.5%）、网格化搬 worker（主线程 remesh −96%）、
  存档增量（退出 6.2 s → 17 ms）、叶子光衰减、面条洞穴。
- **v0.1.0**（2026-10-17，首个公开版）：功能阶段 P0–P99 + 性能系列 99.x 全部完成；
  1195/1195 测试、`-Werror` 0 警告。

每版细节（机制/证据/偏离/性能 A/B）见 `docs/archive/phase-*.md` 阶段存档与
GitHub Releases。
- **0.x 承诺**：可玩、可扩展、确定性运行；但**不承诺稳定**——存档格式
  （现为 format 18，向前迁移）与内容数据格式仍可能随阶段演进，旧存档
  通常可读但请定期备份。已知限制见 `docs/KNOWN_ISSUES.md`。
- 许可证：MIT（见 `LICENSE`）。
- **内存提示**：默认 `config/config.json` 为 `view_distance: 80`（圆盘环
  满环 ~103k chunk ≈ 4.6 GB，会随探索持续加载）。内存 <8 GB 的机器建议
  改为 16–32（`world.view_distance`，16 ≈ 2 GB 量级）。

## 文档地图

| 文档 | 内容 | 何时读 |
|---|---|---|
| [AGENTS.md](AGENTS.md) | 代理快速接收：现状/命令/硬性规则/E2E 协议/阶段流程/坑位速览 | 新会话第一件事 |
| [docs/GAMEPLAY.md](docs/GAMEPLAY.md) | 玩法指南（操作/生存循环/战斗与食物/工具路线/昼夜） | 玩法问题 |
| [docs/MECHANICS.md](docs/MECHANICS.md) | 机制详解（截至 P28：生存/Mob/弓/合成/UI/世界生成） | 改玩法逻辑前 |
| [docs/CONTENT_GUIDE.md](docs/CONTENT_GUIDE.md) | 数据驱动内容添加（biome/方块/配方/Mob/hotbar）+ 存档使用 + worldgen 参数 | 加内容/调世界 |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | 目录结构 + 核心架构（模块/线程池/存档格式/帧管线） | 改架构前 |
| [docs/PROTOCOL.md](docs/PROTOCOL.md) | 开发协议：命令/技术栈/硬性约定/测试/**E2E 协议** | 写代码与验证前 |
| [docs/PITS.md](docs/PITS.md) | 坑位合集（193 条，都踩过） | 遇到怪问题 |
| [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md) | 已知问题清单（活跃）+ 已修复存档索引 | 阶段验收前后 |
| [docs/ROADMAP.md](docs/ROADMAP.md) | Phase 0–29 规划 + 细节存档索引 | 了解历史 |
| [docs/archive/phase-NN.md](docs/archive/) | 单阶段细节存档（阶段报告 + 会话存档） | 深挖某阶段 |
| `opencode-sessions/SESSION_CONTEXT.md`（本地，不入库） | 会话交接：最新状态/提交列表/下一步（+ 历史块/遗留附录/新会话启动 prompt，同目录） | 任何"继续"会话 |

## 开发约定

- 游戏逻辑不直接调用 GL/SDL API —— 图形走 Renderer（Phase 1），窗口走 `Window`。
- 数据驱动：方块/物品/配方通过注册表（Registry）管理，新增内容 = 资源 + 数据，不改核心代码。
- 世界坐标 / Chunk 坐标 / Local 坐标严格区分，转换函数集中定义。
- 错误必须显式传播（`Result<T>` + `MC_TRY`），不允许静默失败。
- 每个阶段完成后：编译 → 运行 → 测试 → 修复 → 总结，再进入下一阶段。
