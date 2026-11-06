# Phase v3.14.0 — V3-8b 手柄按键映射定案（门 204 全 PASS，2026-11-06）

> 3.x 矩阵扩容后首个 ⬜ 项执行（V3-8 手柄支持 + V3-8a 连接修复的续项）。
> 零 format / 零 config 键 / 无手柄位级恒等（结构免疫第 84 次）。

## 背景与定案

**用户 11-06 指令**（V3-8a 连接修复后）：「手柄连接问题已修复，但按键
习惯问题需要进一步确认（可能有多套常见的映射表?）」+ 方向项/内容遗留
入矩阵（= 独立提交 d9c8c0a 文档批）。

**调研结论（多表问题）**：SDL3 GameController 按内置手柄数据库把
**Xbox / PlayStation / Switch 三大布局归一化到同一逻辑位**（SOUTH/
EAST/NORTH/WEST/shoulder/stick/dpad——PS 的 ✕ = SOUTH = Xbox A 同角色，
Switch 同理）= **现行单表已结构性覆盖三大布局，无需多套映射表**（建多
表 = 无新信息可表达的冗余）。山湾克隆 = 数据库已识别（V3-8a 实机
open 铁证：事件投递 = 归一化生效）。

**用户三项习惯定案**（问答定案）：
1. **L3 = 蹲伏**——补键盘 LeftControl 的手柄缺口（v1 表最大习惯缺口；
   MC Java 惯例 = 左摇杆按下）。
2. **RT 扳机 = 疾跑**——原 B 位（EAST）→ RT 轴（0.2 死区数字化按住 =
   Shift，左摇杆移动同款模式）；B 位解放。（前冲自动疾跑 = 未选，
   记偏离：状态机式自动判定复杂度高且与显式输入习惯冲突。）
3. **B = 飞行 toggle 专用钮**——单击直接起飞/落地（原双击 A 手势保留
   = MC 创造同款，记偏离冗余）；**config 全自定义 = 定案不做**（定案
   表写死，YAGNI）。LT / R3 / GUIDE / Dpad 上下左保持未映射（记档）。

## 实现（代码 4 文件 + 测试 1）

- **`src/core/input.hpp`**：`GameKey` 尾部 + `FlyToggle`（**仅手柄键**
  ——`mapKey` 永不返回）+ `kKeyCount` 尾钉同步（= FlyToggle + 1；
  PITS 396②：漏同步 = 新键索引越出 array 边界，-Werror 零报警静默
  OOB）。
- **`src/core/input.cpp`**：`mapGamepadButton` 定案表（EAST →
  FlyToggle；+ LEFT_STICK → LeftControl）；`handleGamepadAxis` +
  RIGHT_TRIGGER 支（0.2 死区数字化 → 合成 LeftShift 按住；case 体花
  括号 = GCC 14 -Wjump-init 纪律）；LT 保持未映射。
- **`src/core/game_interaction.cpp`**：FlyToggle press 接线 =
  `setFlying(!isFlying())` 直接 toggle（同 Space 段守卫：locked
  pointer + alive only；无 suppressJumpGrant 需求 = FlyToggle 非
  Space 不授予跳跃）。
- **`src/core/demo.hpp`**：**`kDemoSimVersion` 1 → 2 = 首个实际
  bump**（v3.13.0 定案的 bump 纪律「输入路由」条款首实例：录制的
  gamepad 事件按**当前解释**重放——sim-1 demo 携 L3/RT/B 事件将静默
  变行为 = 拒播点名而非静默错放）。
- **`tests/test_gamepad.cpp`**：re-pin（mappedButtons 9→10 含
  LEFT_STICK / ButtonMappingTable EAST→FlyToggle + L3→LeftControl /
  表格行 / TriggersUnmapped → LeftTriggerUnmapped）+ 新 3
  （RTTriggerSprintDigitize【0.2 死区边界 6553/6554 同款钉 + 按下/
  保持/释放边 + LT 恒零】/ L3CrouchHold / FlyToggleKeyIsGamepadOnly
  【256 虚拟键黑盒扫描无键盘孪生 + B 边缘精确 + A/B 不串扰】）。

## 资产（4 canary 重新 author = sim 2）

`make_example_demo.py` 的 Python 镜像常量 `SIM_VERSION` 1 → 2（**门
内捕获**：初版漏 bump → example.dmo 静默写成 sim 1 + recorded_with
3.14.0 自相矛盾 → S2 首跑被 sim 门点名拒收，金丝雀先于用户发现 =
PITS 396①）。4 个 .dmo 重新 author 字节可复现（md5 恒等）。

## 门 204（全 PASS）

- **S1 结构免疫第 84 次**：无输入 60f ×2 ≡ **base19 逐字节**
  （committed config ui.scale 1.5 口径）+ savenorm EXACT——手柄映射 =
  无手柄冷路径（bit1 永不置位）+ GameKey 尾扩零像素零存档面。
- **S2 sim-2 canary**：4 demo ×2 RAW 恒等 + savenorm EXACT。
- **S2b 首个 bump 实证**：git 0705139 的 sim-1 canary（recorded
  with 3.13.0）= **rc 1 点名**「demo sim 1 != this build sim 2
  (recorded with 3.13.0) - the simulation semantics differ (use
  --demo-force to replay anyway)」+ `--demo-force` = rc 0 + WARN +
  **force 回放终档 savenorm ≡ sim-2 canary 终档**（纯键鼠录制 = 映射
  变更零行为影响 = bump 论证最强形）。
- **S3 全套**：2787/2787（+3 新 + re-pin），`-Werror` 0 警告。
- **S6**：无（零新解析面——定案表 = 命名常量，无 config 键）。
- **性能 A/B（用户硬要求）**：旧 = d9c8c0a v3.13.0 + g139 vs 新 =
  v3.14.0 + g139（banner 实证）：900f 无输入同 seed——PERF-WIN 三窗
  upd med 0.665/0.647/0.705 vs 0.671/0.628/0.693 + render
  0.700/0.766/0.765 vs 0.688/0.721/0.790 + swap med
  15.093/15.025/14.976 vs 15.036/15.111/14.901（旧侧 win1 max 15139 ms
  = 一次性 OS 停顿 med 正常）+ fluid·fires·spread·ents·stream 同档 +
  changesApplied 9/2/1 vs 9/3/1（win2 差 = real-dt 追赶抖动 PITS 385
  域）+ **终档 savenorm EXACT**（映射 = 无手柄冷路径结构性零成本）。
- **门强度记档**：本环境全合成事件单测 + 逻辑门；**实机手感验收归
  用户**（山湾克隆 L3/RT/B 三定案 + 「按一下就断线」V3-8a 未决项
  复测 = 一次实机终审）。

## 记档偏离

1. **A 双击飞行保留**（未删）：B = 专用钮后 A 双击仍经 Space 路径
   toggle = MC 创造同款冗余（用户定案「改专用钮」= 新增非替换；键盘
   双击 Space 语义零触碰）。
2. **前冲自动疾跑 = 未选**（用户选项含 RT/前冲自动二选一语义，定案
   落 RT = 显式输入优先；自动判定状态机留后）。
3. **config 全自定义 = 定案不做**：三大布局归一化 + 三项定案后无多表
   需求；`gamepad.binds` 留后（有消费方再立）。

## PITS

- **PITS 396**（门内捕获双坑）：① `tools/make_example_demo.py` 的
  `SIM_VERSION` = kDemoSimVersion 的 Python 镜像——C++ bump 后漏同步
  = 资产静默写错 sim（recorded_with 却新版 = 自相矛盾），S2 首跑被
  sim 门点名拒收捕获（金丝雀设计目的实证）；bump 时 grep 全仓镜像。
  ② `kKeyCount = GameKey::T + 1` 尾钉——GameKey 加成员须同步，漏 =
  新键索引越出 `std::array<..., kKeyCount>` 边界（-Werror 零报警静默
  OOB）；enum 尾「Keep in sync」注释是唯一守卫。

## 变更文件（显式路径，17）

src/core/input.hpp / src/core/input.cpp / src/core/game_interaction.cpp
/ src/core/demo.hpp / tests/test_gamepad.cpp / tests/test_v3110_demo.cpp
（sim re-pin 3 例）/ tests/test_debug_overlay.cpp（F3 v3.14.0）/
CMakeLists.txt（3.14.0）/ assets/demos/example.dmo /
assets/demos/tutorial_dig.dmo / assets/demos/tutorial_build.dmo /
assets/demos/tutorial_fly.dmo / tools/make_example_demo.py（SIM_VERSION
2）/ AGENTS.md / README.md / docs/CHANGELOG.md / docs/MECHANICS.md /
docs/GAMEPLAY.md / docs/KNOWN_ISSUES.md / docs/PITS.md /
docs/archive/phase-3.14.0.md（本文）。

（BACKLOG 矩阵 V3-8b ✅ 在 d9c8c0a 前批已入表，本提交仅 ✅ 翻转 +
变更记录。）

Release 政策 = 延续：v3.14.0 只留 **tag + 提交**，无新 release。
