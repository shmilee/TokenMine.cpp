# Phase V3-8 — 手柄支持（v3.3.0，门 193 全 PASS）

> 11-05 用户实机诉求定案（BACKLOG UX 行 V3-8，v3.2.0 收口后紧接执行）。
> 设计定案 = `/tmp/opencode/v330_design.md`（源位掩码 OR 语义 + v1
> Xbox 映射表 + 无 config 键 + 无物理手柄门强度记档）。SDL3 一阶
> GameController API（本机头文件核证：SDL_events.h 0x650 族事件 +
> SDL_gamepad.h 轴/钮枚举；`SDL_GetGamepadNameForID` = 设计稿
> `SDL_GetGamePadNameFromID` 的本机真实名，fixer 偏离 2 实证）。

## 实现

### 核心机制：源位掩码 OR 语义（input.cpp/hpp）
- `m_keyDown`/`m_buttonDown`：`array<bool>` → `array<uint8_t>` 源位
  掩码（bit 0 = 键盘/鼠标 `kInputSourceKeyboard`，bit 1 = 手柄
  `kInputSourceGamepad`）；`isDown` = 掩码非 0（任一源按下 = 按下）；
  每源独立 rising edge → wasPressed；**wasReleased 仅本源 falling
  edge 且掩码清 0 时置**（另一源仍按着 = 不释放 = 弓不松手，Phase 24
  弓语义保全）。
- **无手柄 = bit 1 永不置位 → 与 pre-V3-8 bool 语义逐位恒等**（结构
  免疫根 + legacy 位级恒等 fixture 单测钉死）。
- 单手柄模型：`m_gamepadWhich`（Added 记录；第二个 Added 忽略；仅
  其 REMOVED 断连；pump 层 which 预过滤）——记档偏离 v1。

### v1 映射表（9 钮 + 2 摇杆轴；命名常量映射纯函数）
左摇杆（死区 `kGamepadAxisDeadzone 0.2` 外数字化，+Y→W −Y→S +X→D
−X→A，对角 = 双键，每次 AXIS_MOTION 全量重算四方向源位 = 幂等回中清
键）/ 右摇杆（死区外重映射 `remapAxisOutsideDeadzone` = sign×min(1,
(|v|−dz)/(1−dz)) × `kGamepadLookPixelsPerUnit 100` 合成 mouseDelta：
杆右 = xrel+，杆上 = yrel−）/ A=Space（双击飞行自动继承）/ B=LeftShift
疾跑 / X=E 背包 / Y=Q 丢弃 / LB=LMB 挖掘 / RB=RMB 使用 / Back=C 相机
循环 / Start=Escape / D-pad 右=Tab 热栏。不映射（记档偏离）：
LT/RT（trigger 轴）/ Guide / L3 / R3 / D-pad 上下左 / paddles /
touchpad / REMAPPED 事件（内置标准布局表）。

### 集成
- `pump()` 加 5 个 GAMEPAD 事件 case（ADDED/REMOVED/AXIS_MOTION/
  BUTTON_DOWN/BUTTON_UP）→ 4 个公开 handler（**单测入口**，handleKey
  同款模式；未连接 = which 哨兵 0 = 自含 no-op 守卫）。
- `window.cpp`：`SDL_Init(VIDEO)` → `SDL_Init(VIDEO | SDL_INIT_GAMEPAD)`
  （无手柄环境 = 惰性零成本；失败同款 MC_FAIL）。
- 连断日志（info）："Gamepad connected: <name>"（
  `SDL_GetGamepadNameForID`，NULL → "unknown"，无 init 时调用安全
  实证）/ "Gamepad disconnected"。断连清全部 bit-1 键位 + 摇杆缓存
  复位 + **不置 wasReleased 边**（弓的 stale-up 分支取消蓄力而非悬挂
  ——记档）。
- F3 覆盖层：`DebugInfo` +gamepadConnected/gamepadName；末行
  "gamepad: none" / "gamepad: <name>"（18→19 行，test_debug_overlay
  钉有意识更新 + GamepadLine 四形新例）。
- **零改动红线**：player 物理/相机/渲染/存档/场景/config（v1 无
  config 键——映射表 + 阈值全命名常量，未来 config 化的锚点 =
  kGamepadLookPixelsPerUnit）/ 现有键盘映射逐位不变。

## 门 193（结构免疫第 74 次；锚 224468 B 免重锚）

**结构免疫论证**：本环境无物理手柄 = 零 SDL 手柄事件 = bit 1 永不置
位 = 输入层位级恒等（S1 零 gamepad 日志行实证）。
- **S1**：无输入 60f ×2 截图 RAW ≡ 锚 224468 逐字节 + savenorm EXACT
  + gamepad 事件日志行 = 0 + banner v3.3.0。
- **S3**：全套 2555/2555（编排者独立复跑）。
- **S6**：无 config/内容面（映射 = 代码常量，无 init 拒收面）→ 跳过
  记档。
- **性能 A/B（用户硬要求）**：旧 = v3.2.0（tm_v330_perf = c9bf08d +
  g139，banner v3.2.0 实证），新 = v3.3.0（tm_v330_perf_new = worktree
  + g139，banner v3.3.0 实证）：主对 900f 无输入同 seed——PERF-WIN
  三窗全指标同档（upd med 0.426/0.543/0.583 vs 0.526/0.582/0.583 ms /
  render 0.548/0.610/0.523 vs 0.602/0.614/0.552 / swap vsync 主导同档 /
  fluid·fires·spread·ents·stream 同档）+ **changesApplied 9/1/3 逐位
  恒等** + **终档 savenorm EXACT**（零 format 最强形）。手柄 per-frame
  成本本环境不可测（无物理手柄）= 门强度记档：全部 handler 逻辑单测
  覆盖（handle 入口 = handleKey 同款单测模式同强度）+ pump 分发 =
  SDL 标准事件泵（零项目自定义状态机）+ **实机验收 = 用户**（release
  提示）。

## 单测族（tests/test_gamepad.cpp 新 18 例 + test_debug_overlay
GamepadLine 1 例；2536 → 2555）

映射纯函数钉 / 9 映射钮按压释放（含 RB 弓释放边）/ **17 个不映射钮
零变化**（primed 态对照，超设计列举）/ 左摇杆死区边界（6225 内·6553
内·6554 外 = 0.2×32768 精确边）/ 四方向数字化 / 对角双键 / 回中清键 /
方向切换幂等 / 右摇杆死区零 delta / 符号+增益（16384 = 0.5 杆 →
remap 0.625 × 100 = 62.5 px 精确）+ 累积 + frameBegin 复位 / 重映射
公式钉（0.7→0.625、0.9→0.875、±dz→0、clamp）+ 常量钉 / trigger 零
变化 / 键 OR 语义（键松杆按保持）/ 钮 OR 弓语义双序（RB 按住时鼠标
RMB 松 = 不释放）/ 断连清位（键盘位幸存 + 再连 + 陈旧 REMOVED
no-op）/ 单手柄模型（第二 Added 忽略 · 他 which REMOVED no-op）/ 未
连接 handler no-op / **legacy 位级恒等 fixture**（纯键鼠序列全读法 =
bool 时代语义）。

## 偏离/记档

1. 设计 §4 算例算术错（0.7→0.875 实为 0.625）——公式文本为准，实现
   未动，单测两值留证。
2. SDL 函数名 = 本机真实 `SDL_GetGamepadNameForID`（设计稿记名有误；
   无 init 时调用安全 = NULL → "unknown"）。
3. 弓语义示例 "LB" → "RB"（弓 = RMB = RIGHT_SHOULDER）。
4. 不映射测试覆盖全部 17 个其余有效 SDL 钮（超设计列举）。
5. SDL_JoystickID = Uint32 → m_gamepadWhich 无符号（handler 签名按
   设计 int，pump 无符号比较；负 which 自然 no-op）。
6. v1 映射缺项（LT/RT/L3/R3/Guide/D-pad 上下左/paddles）+ 单手柄模型
   + 断连不置释放边（弓 stale-up 取消蓄力）= 记档偏离。
7. **门强度**：无物理手柄环境——逻辑全单测 + 结构免疫 + 实机验收归
   用户（release 记档）。

## 变更文件（显式路径）

src/core/input.hpp（源位掩码 + handler 族 + 映射纯函数 + 常量 +
访问器）/ src/core/input.cpp（pump 5 case + 4 handler + 源位逻辑 +
摇杆数字化/重映射 + 连断）/ src/core/window.cpp（SDL_INIT_GAMEPAD）/
src/ui/debug_overlay.{hpp,cpp}（DebugInfo 字段 + 末行）/
src/core/game_ui.cpp（F3 info 填充）/ tests/test_gamepad.cpp（新 18
例）/ tests/test_debug_overlay.cpp（F3 钉 + GamepadLine）/
CMakeLists.txt（3.3.0 + 测试列表）。

零改动红线核证：player/相机/渲染/存档/场景/config 零触碰（git
status 10 文件 = 合同全清单）；legacy 键盘映射位级恒等 fixture 钉。

## V3-8a 修订（11-06 实机验收，d5a5ea6）

用户山湾克隆（045e:028e）实机验收发现两处实机缺口（门 193 合成事件单
测永不暴露，PITS 395 三坑全记档）：① **SDL3 的 BUTTON/AXIS 事件须显式
`SDL_OpenGamepad` 后才投递**（游戏从不 open = "连接日志正常但零按键"）
→ `handleGamepadAdded` 补 open（失败 = WARN 点名 `SDL_GetError()`、
跟踪态保留）/ `handleGamepadRemoved` 补 close（belt-and-braces 对称）+
`gamepadOpen()` 诊断 getter + 1 新测（open 失败不破坏跟踪/映射，
2784/2784、-Werror 0）。② 该克隆"未 open + 按键 = USB 自断"（~6 s 周
期、dmesg 设备先消失非驱动 reset），**open 状态 = 稳定**（探针 5 分钟
狂按零 REMOVED）= ①的附带治愈（四象限实证，机制未实证记档）。本版
"本环境无物理手柄 = 全逻辑单测 + 实机验收归用户"的门强度记档 = 本次
实机验收完成并修复，手柄功能自此实机可用。
