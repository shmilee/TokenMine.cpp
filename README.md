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

- **v0.14.0**（2026-10-26）：**SURV-2 氧气窒息**（v1.0 任务表 16/29）：
  水下氧气条 + 窒息（溺水）伤害，MC 语义（15 s 空气、耗尽后每 2.5 s
  2 HP、出水即满）：
  - 帧锁定氧气系统（60fps = MC tick × 3：900 帧耗尽 / 150 帧一伤，
    确定性 E2E 精确值）；头没入 = 眼位单格流体判定（水/熔岩同款，
    MC 眼在水里的规则——站浅池眼上水面 = 换气）
  - **溺水豁免护甲减伤**（MC 伤害调整表实证：Drowning = Armor
    No effect；Resistance 与吸收池照常生效）
  - HUD 气泡排：水下时替换饥饿排原位（10 泡 = ceil(氧气/15)；出水
    立即消失）+ 新纹理 oxygen.png
  - 存档 format 17 `player.oxygen`（恒写、键纪律、15/16 迁移满空气；
    溺水相位不存档 = MC 只存 airSupply 同款）
  - 已知偏离：无气泡声、mob 不溺水（MC 同款）、无呼吸附魔/水肺、
    无 config 键（MC 值 = 命名常量）
- **v0.13.0**（2026-10-26）：**SURV-1 护甲**（v1.0 任务表 15/29）：
  12 件护甲（leather/iron/diamond × 头盔/胸甲/护腿/靴子 = MC 精确值
  1/3/2/1、2/6/5/1、3/8/6/3）+ MC 整数 floor 减伤（全伤害源生效，
  含坠落 = 摔落缓冲）+ 右键穿戴/交换/脱下（热栏 Tab 间槽脱整套）+
  存档 format 16 `player.armor` + HUD 心行上方护甲排（无甲零绘制）：
  - 内容：12 物品（max_stack 1）+ 12 配方（MC 1.14+ 图案）+ 12 图标；
    材质档 = leather → iron → diamond 三链
  - 减伤：`floor(amount × (100 − 4×点数)) / 100`，次序 armor →
    resistance → 吸收池；点数 0 = 位级恒等（无甲时代零回归）
  - 交互：持护甲右键 = 穿上（占位槽 = 交换）；空手右键 = 按
    head→chest→legs→feet 序脱到选中槽
  - 已知偏离：chain 无材料、float HP 无半心量化（1.0 伤×3 点 = 0.88）、
    窒息未实现（SURV-2，届时按 MC 规则豁免护甲）、无耐久（SURV-3）
- **v0.12.0**（2026-10-25）：**ENT-4 水体交互**（v1.0 任务表 14/29，
  ENT 行 4/4 收尾）：mob/掉落物游泳 + 浮力——玩家 FLUID-2 的无输入
  孪生（`refreshFluidState`/`integrateFluid`/`isRestingOnSolid` 上提
  `Entity` 共享）：
  - mob：被动上浮（buoyancy 12 帽 1.5，vy≥0 门 = 带动量入水沉底
    MC dive 语义）+ 水面漂浮几何钳制（W' = 中心列最高 L8 +1，fps
    无关不动点）+ 涉水/水下水平 ×0.6 + 水下禁 step-up；AI 状态机
    零改动（追逐/游走照常，只改物理）；站立 mob 不上浮（同 MC）
  - 掉落物：同款浮力（预置静水浮面静止无漂移 = MC 简化；带动量
    坠水沉底站立互斥）+ 熔岩对称同浮力（记档偏离：player 熔岩弱节奏）
  - 陆上行为逐位不变（存量 1389 零回归 + 双 inert 锚单测）；箭穿过
    水不动（投射物边界记档）；零 format/config/内容
  - 1400/1400（+11）、`-Werror` 0 警告、门 94 全绿（base 对 gate88
    锚 + item 浮面 39.0/沉底 36.0 exact 互斥 + 涉水三态互斥
    【new+膜 卡面 hp20 Δ3.80 / old+膜 跨越击杀 hp0 / new 无膜 击杀
    hp0】+ ×2 RAW + savenorm EXACT）；性能（插桩 A/B v0.11.0）
    render med 0.41-0.72 ms / ents 2.7-3.4 ms/300f = 成本可忽略
- **v0.11.0**（2026-10-25）：**ENT-3 mob 踏阶**（v1.0 任务表 13/29）+
  **ENT-1b 模型修复**（用户实机反馈）：
  - ENT-3：step-up 纯几何上提 `Entity::stepUp`（player 行为逐位不变 =
    test_player 零改动钉死）+ mob 同款镜像调用——判定与 player 完全一致
    （slab 0.5/满块 1.0/玻璃板可踏，墙/栅栏 1.5 不可踏 = P99 TRUE-1-block），
    修追逐/游走卡 1 格台阶；帧锁定纯几何、零 format/config/内容；
    门 93 竞技场三态互斥（new+台阶 zombie 跨越击杀 / old+台阶卡住 /
    new+墙不可踏）。记档 2 个 P16 潜伏 quirk（player/mob 共享，修复 =
    player 行为变更单独立项）。
  - ENT-1b：四足身体比例修正（身长>身高 = MC 比例带：cow 1.9:1 /
    sheep 1.73:1 / chicken 1.43:1，纯数据）+ 关节出体（pivot = 身体底
    缘）+ 摆幅下调（0.5/0.4/0.45 rad）+ spider 腿加长加细外移 + 新穿
    模谓词 `checkSwingClip`（旋转腿∩body 相交体积 + 插地检查，4 mob ×
    14 部件 × 64 相位 = 896 验收单测）。zombie/skeleton 人形未动。
  - 1389/1389（+22）、`-Werror` 0 警告、门 93 全绿（base 对锚 + 三态
    互斥 + 模型 A/B 40,703 world px + 摆动 on/off 157,158 world px +
    ×2 确定性 + savenorm EXACT）；性能（插桩）render med 0.47-0.60 ms
    / ents 11-26 ms/300f = 动画+踏阶成本可忽略。
- **v0.10.0**（2026-10-25）：**ENT-1 mob 四肢摆动**（v1.0 任务表 12/29，
  消费 99.x 残余 #4）：mob 行走摆腿动画 = **纯渲染层**（AI/物理/存档零
  改动）——per-mob 整数 swing 相位（帧边界水平位移严格不等式判移动，
  +2/帧 整周期 0.533 s / −4/帧 16 帧收步，从不入档）+ constexpr 16.16
  定点正弦表（编译期 Taylor，零运行期 libm = 位级确定）+ mob JSON
  `swings` 数据段（amplitude (0,1] 弧度 + 半周相位组；三级未知键拒绝、
  part 存在性解析期交叉校验）+ CPU 逐帧部件顶点变换（pivot = box 顶面
  中心、绕局部 X 轴、法线同旋；phase 0 位级恒等 = 静态 fast path）+
  per-mob 动态 VBO + config `mobs.limb_swing`（off = v0.9.0 静态对照）。
  内容 = chicken 2 腿 / cow·sheep 4 腿对角步态（FL+RR·FR+RL = MC 四足）/
  spider 8 腿偶奇分组；翅不摆（MC 只飞行扇翅）；zombie/skeleton 99.7
  几何无独立 arm part = 不造几何（记档）。1367/1367（+17）、`-Werror`
  0 警告、门 92 全绿（出生点对 gate88 锚 = 结构性免疫 + 白天 240f
  on×2/off×2 字节一致 + on/off 像素 diff 11,749 world px 隔离（sky 3 =
  AA / hud 24 = 饥饿饱和度 real-dt）+ savenorm EXACT + 夜间 N120 diff
  8,060 px（sky 0 / hud 0）+ S6 init 点名）。性能（插桩 900f dense mob
  场景）：render med 0.46-0.56 ms / ents 10-22 ms/300f / update p95
  <82 ms（流送期）= 动画成本可忽略。
- **v0.9.0**（2026-10-24）：**99.11 流体光照异步化**（用户实感"跑一段距离后
  转摄像头很卡、FPS 均值正常"的深层根因修复）：流体 tick 后对全部变更格主
  包围盒的**主线程同步 re-light**（shipped 海洋海岸线 film 级联 713→918→
  1357 格/tick → 每 300 帧窗口 130s→396s→661s、update p95 秒级）→ 列粒度
  待集 + 每帧 2 列预算 worker 追赶 job（复用 99.10 位级独立判据/并行/
  dirtyOut，跨类同窗位级独立排除）。主线程成本降为 set 插入；光逐列幂等
  收敛（待集排空 == propagateAll 全量，单测 oracle 钉死），滞后有界、
  film 光值近乎不变 = 视觉零差（文档化偏离，99.4 pop-in 先例）。1350/1350
  （+11）、`-Werror` 0 警告、门 91 全绿（出生点对锚 = 结构性免疫 + 流送
  300f ×2 字节一致 + 状态 vs v0.8.0 逐位相同 + **新旧 300f 像素 diff = 0**）。
  发布前性能测试（T450，同工作负载 A/B）：vd16 冷填充 **244.7 s → 44.6 s
  （5.5×）**；海洋世界 900f 静置三窗口 **402 s/405 s/644 s → 15.5 s/15.5 s/
  16.2 s（~26× 实时）**、fluid 段 130/396/661 s → 1.0/1.9/2.9 s、update
  med/p95/max 52/2075/2564 ms → 50/72-92/90-167 ms（秒级尖峰消除）；
  流体 churn 逐 tick 规模不变 = 流体行为零变更。
- **v0.8.0**（2026-10-24）：**99.10 光照 job 并行**（用户实感卡顿的
  流送期 CPU 下限修复）：流送填充期每帧 2 个新列的光照由单 worker 顺序
  2 box（16.7 ms/帧硬下限，LGT-1 实测）改为位级独立时拆 2 个 job 双
  worker 并行（flush 下限 → ~10 ms）；依赖对（双轴 gap≤3）/3+ 列回退
  单 job 顺序 = 逐位不变（含 LGT-1 定案不修的 overlap clobber 语义）。
  判据 = `lightRegionsIndependent` 纯函数（写 reach ±1 列 + block 光 BFS
  读环 +1 列，gap≥4 任轴）；并行 job 净变 chunk 走 job-local `dirtyOut`
  解共享写竞态。硬规则 7 修订（≤1 → ≤2 且仅位级独立）。1339/1339
  （+11，含三轨位级等价 + propagateAll oracle）、`-Werror` 0 警告、
  门 90 全绿（出生点对锚字节一致 + 流送 ×2 确定 + **新旧二进制 300f
  A/B 字节一致 = confluence 的 E2E 级证明**）。性能 A/B（T450 vd16
  冷填充）253.3→244.7 s（本机 CPU 饥饿下 burst-stall 主导，深层剖析
  = 99.11 立项）。
- **v0.7.0**（2026-10-24）：v1.0 任务表第 11 项——**游泳划水**（FLUID-2）：
  水下物理对齐 MC——无输入**被动上浮**（浮力，cap 1.5 b/s）并**漂浮**在水面
  （L8 水柱顶 +1 的几何钳制，fps 无关终态）；**潜行下沉**（1.5 b/s 潜水）；
  水面 Space = 正常跳（可出 1 格池沿）。`player.water_gravity` 退役为
  `water_buoyancy`/`water_rise_speed`/`water_sink_speed` 三键（旧配置容忍）。
  熔岩行为不变（P54 原样）。测试 1328/1328、`-Werror` 0 警告。
- **v0.6.0**（2026-10-23）：v1.0 任务表第 10 项——**水量守恒**（FLUID-1）：
  修复流体"水量凭空增减"——落池抬升（入流把池水位拉高）删除，落池行为统一
  为 MC 源/流动语义（源上补充 / 流入更深池被吸收 / 同高或更高则静态沉积），
  每格流体变化 100% 归入四类具名事件（扩散创生/落池吸收/源转换/边界漏排，
  逐 tick 会计恒等式单测钉死）；熔岩同款。存档格式不变（format 15）。
  测试 1312/1312、`-Werror` 0 警告。
- **v0.5.0**（2026-10-22）：v1.0 任务表第 9 项——**矿脉形态 MC 化**
  （GEN-3，**GEN 行 3/3 全清**）：矿石/砾石脉从随机游走改为
  Minecraft 同款 3×2×1 旋转椭球脉（脉更大更稀、确定性标定保持各矿
  密度 ±15% 内；diamond/lapis 略稀为标定残留，已记档）。旧存档照常
  可读（未挖区域按新脉形生成 = 世界生成演进）。测试 1297/1297、
  `-Werror` 0 警告。
- **v0.4.0**（2026-10-22）：v1.0 任务表第 8 项——**树生长**（GEN-2）：
  3 树种可种植树苗（oak/spruce/birch，E 面板库存槽 24-26 初始各 3 颗）——
  树苗按 2 s/阶段生长（光照/土壤门控），成熟后落树种特定树形（橡树宽冠 /
  云杉下宽上尖 / 白桦细高小冠）；树叶破坏概率掉树苗（0.05）= MC 种植闭环。
  基于 v0.3.0 的 BlockState 机制（生长阶段 = 每格状态 byte，存档 format 15）。
  世界生成森林保持现状单橡种（多树种野外生成 = 后续版本）。测试 1285/1285、
  `-Werror` 0 警告。
- **v0.3.0**（2026-10-22）：v1.0 任务表第 7 项——**BlockState 通用 per-block
  状态**（ENT-2）：楼梯朝向（每格 byte）泛化为数据驱动状态机制（块属性
  `state_count`/`state_default` 双门控 + `World::get/setBlockState` +
  init 不变量：世界生成不得放置 stateful 块）——树生长（GEN-2）/工作台分块
  （CRAFT-1）/slab 叠合的前提。存档 format 14 → **15**（`states` 段，旧
  存档加载值保真迁移、直接可读）。测试 1260/1260、`-Werror` 0 警告。
- **v0.2.0**（2026-10-22）：v1.0 任务表前五项落地（29 项中的 5 完成 +
  1 经实测砍除记档）——
  - **流送环方形 → 圆盘 + 远平面帽**（STRM-1）：默认 view_distance 80 满环
    内存 9.08 → **4.59 GB（−49.5%）**
  - **网格化搬 worker**（PIPE-1）：主线程 remesh −96%，填充后拖尾 2.8 min
    → <25 s
  - **存档增量**（PIPE-2）：退出存档 vd80 全环 6.2 s → **17 ms（≈190×）**，
    退出成本 = O(会话编辑数)、与环大小脱钩
  - **叶子光衰减**（LGT-2）：新 `light_opacity` 块属性（leaves 2 / glass 1
    = MC 值）——叶冠下光按层衰减，玻璃透光 MC 化
  - **面条洞穴**（GEN-1）：单噪声奶酪洞 → 两 3D 噪声相交管状隧道
    （Minecraft 同款 spaghetti 洞 + 可达洞口）；出生区结构保护、熔岩湖
    静态契约保持
  - 测试 1240/1240、`-Werror` 0 警告。**存档格式不变（format 14）**；
    旧存档可读，加载后世界含新洞穴与叶光衰减（世界生成/光照演进，
    预期行为）。
- **v0.1.0**（2026-10-17，首个公开版）：功能阶段 P0–P99 + 性能系列 99.x
  全部完成；1195/1195 测试、`-Werror` 0 警告。
- **0.x 承诺**：可玩、可扩展、确定性运行；但**不承诺稳定**——存档格式
  （现为 format 15，向前迁移）与内容数据格式仍可能随阶段演进，旧存档
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
