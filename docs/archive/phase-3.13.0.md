# Phase v3.13.0 — demo 回放独立 sim 版本（门 203 全 PASS，2026-11-06）

> 3.x「世界扩展」期矩阵后第 3 个报障/设计版（v3.12.0 阴影缩尺之后）。
> 零 format / 零 config 新键 / 零玩法变更 = demo 文件族的版本治理。

## 背景与定案

**用户报障**：`--demo assets/demos/example.dmo`（v3.11.0 录制）在
v3.12.0 构建下被拒：

```
Demo error: demo recorded with v3.11.0, this is v3.12.0 - incompatible
(the simulation semantics differ)
```

**根因**：旧设计把**构建版本串（TM_VERSION）当模拟语义的代理**——
但 v3.12.0 是纯渲染常量版（阴影缩尺），模拟语义零变化，旧 demo 的
回放结果本应逐位不变；严格指纹却把全部已录 demo 一刀杀。

**用户问题**：「dmo 的格式要求版本完全对应，这有点太严格了。如何设计
可以保证 dmo 格式也有独立的版本号，还能在游戏版本变化时，保证兼容
性？」

**定案（用户问答后编排者实现）**：

1. **`kDemoSimVersion = 1`**（`src/core/demo.hpp` 新常量）= 回放准入
   **唯一判定轴**，独立于 TM_VERSION。**bump 纪律**：仅帧锁定**模拟
   语义**变更（update 循环 / 物理 / AI / 流体 / 生成 / 输入路由 / demo
   pacing）才 +1——纯渲染 / UI / 音频 / 内容数据（JSON）/ 文档版本
   **不 bump**（v3.12.0 阴影缩尺 = 不 bump 实例，canary 恒等实证）。
2. **`recorded_with`**（录制时 TM_VERSION 串）**仅信息展示**（回放
   日志行 + 结束面板），**永不拒播**。
3. **legacy 映射零迁移**：旧 schema（仅 `version` 键）→ sim := 1 +
   recorded_with := version。合理性 = 旧严格门已证 record-build ==
   replay-build（同构建录制回放 = 同期系），故 legacy 文件全部 sim
   1 映射。**用户手头所有 .dmo（v3.11/v3.12 时代）继续可放**。
4. 新 schema = `{sim int ≥1, recorded_with 非空串, world, dts,
   events}`；`version` 与 `sim`/`recorded_with` 同现 = **混用 schema
   点名拒**；非 int sim / sim <1 / 空 recorded_with 全点名拒。
5. sim 不匹配 = **rc 1 点名**「demo sim X != this build sim Y
   (recorded with Z) - the simulation semantics differ (use
   --demo-force to replay anyway)」+ **`--demo-force` 逃生门**（仅
   bypass sim 门，schema 严格性保留，WARN 点名；无 `--demo` 带
   force = rc 2 参数点名）。
6. **canary 纪律**（PROTOCOL.md E2E 协议新条）：4 个 shipped demo
   （example / tutorial_dig / tutorial_build / tutorial_fly）= 模拟
   语义金丝雀，**每版门内回放 savenorm EXACT**——核心改动破坏 canary
   = 必须 bump sim + 重新 author 4 个时间线（`--demo-author` /
   `make_example_demo.py` 字节可复现，PITS 393）。

## 实现（代码 7 文件）

- **`src/core/demo.hpp`**：`kDemoFileVersion` 注释降为信息性（=
  TM_VERSION 串）；新增 `kDemoSimVersion = 1` + bump 纪律注释；
  `DemoFile{version}` → `{recordedWith, sim}`；`loadDemoFile(path,
  forceSimMismatch = false)` 签名 + 门注释；parse 注释重写双 schema。
- **`src/core/demo.cpp`**：`parseDemoJson` 双 schema 闭集（`version`
  存在 = legacy 闭集 {version,world,dts,events} + 映射；否则现行闭集
  {sim,recorded_with,world,dts,events} + 严格形校验；混用 = 点名拒）；
  `loadDemoFile` sim 门（`file->sim != kDemoSimVersion &&
  !forceSimMismatch` = 拒）；`serializeDemoJson` 写 sim +
  recorded_with（现行 schema 恒写）；`DemoRecorder` 构造 +
  `authorDemoFile` 两处 writer = sim/recordedWith。
- **`src/core/main.cpp`**：`--demo-force` 旗解析 + 校验（无 `--demo`
  = rc 2 点名）+ `loadDemoFile(args.demo, args.demoForce)` + 强制
  时 WARN 打印（sim/recorded_with 点名）。
- **`src/core/game.hpp`**：`Args.demoForce` 字段 + 注释。
- **`src/core/game_init.cpp`**：回放日志行加 `", recorded with
  <build>"`。
- **`src/core/game_ui.cpp`**：demo 结束/暂停面板 4 行 → 5 行
  （title/name/speed/**recorded with**/hint；合并 name 行方案被否 =
  16px 字形 ×ui.scale 2 下 32px/字，32 字行 1024 px > 679 px 窗溢出，
  独立行 20 字 = 640 px 在界内）。
- **`src/world/day_cycle.hpp`**：`startTimeOfDay = 0.25; // noon`
  注释更正为「10:48 by the clockString mapping (the day-plateau
  sky)」——门内时钟映射误读的顺手卫生（零行为）。

**拒收面**（全点名，单测钉）：mixed schema ×2（version+sim /
version+recorded_with）/ sim 字符串 / sim 0 / sim -1 / recorded_with
缺失 / recorded_with 空串 / legacy version 空串（既有面保留）。

## 测试（2777 → 2784：本版 +6 + V3-8a 手柄 +1）

`tests/test_v3110_demo.cpp`：
- **重钉**：`fixtureFile` / `PlayerInjectOrderWithinFrame` 的
  `f.version` → `f.sim = kDemoSimVersion; f.recordedWith =
  kDemoFileVersion`；`ParseRoundTrip` 字段对；`authorDemoFile` 字段对；
  `LoadVersionMismatch` → **`LoadSimMismatch`**（sim 2 + recorded
  "3.0.0" = 拒消息三片段断言 + **force 旁路载荷恒等**）。
- **新 6**：`SimPinnedToOne`（kDemoSimVersion == 1）/
  `ParseLegacyMapping`（legacy → sim 1 + recorded_with := version）/
  `ParseCurrentSchema`（sim 2 解析通过 = 门是 load 的活不是 parse 的）/
  `ParseRejectMixedSchemas`（×2 形态）/
  `ParseRejectCurrentSchemaFieldShapes`（5 形）/
  `LoadLegacyFileAccepted`（**load 级**：手写 legacy gzip 容器直放 =
  用户报障场景的单元面）。

既有 legacy 形 parse 测试（`{"version": "v", ...}` 家族 ≈20 例）=
天然走 legacy schema 继续通过（零改动 = 映射兼容的既有面证据）。

`tests/test_debug_overlay.cpp`：F3 首行 v3.12.0 → v3.13.0。
`CMakeLists.txt`：project VERSION 3.12.0 → 3.13.0。

## 资产（4 demo 重新 author = 现行 schema）

- `assets/demos/example.dmo`：**285 B → 294 B**（sim/recorded_with
  键）+ `tools/make_example_demo.py` 同步（`SIM_VERSION = 1` 常量
  镜像 + 现行键序，重跑字节恒等）。
- `tutorial_{dig,build,fly}.dmo`：`--demo-author` 时间线 JSON 源不变
  重新展开（md5 恒等实证 ×2）。
- 全部 4 件 = **canary 家族**（每版门内回放 savenorm EXACT）。

## 门 203（全 PASS）

- **S1 结构免疫第 83 次**：无输入 60f ×2（committed config 口径
  `ui.scale 1.5`）≡ **base19 225272 B 逐字节** + savenorm EXACT；
  用户工作树口径（ui.scale 2.0）×2 自洽。
  **S1 首跑假失败排查**（PITS 394）：工作树跑 ≠ 锚，3846 px diff
  全在 UI 文本带（右上时钟 + 底部 XP「0」）——glyph 尺寸比 =
  152/114 = **4/3 = 2.0/1.5 精确** + `git show HEAD:config` vs 工作树
  diff = 恰 ui.scale 单键 = **用户调值（锚在 1.5 时代捕获）非本回归
  档**；锚语义 = committed config 态，S1 按 committed 副本 +
  `--config` 重跑 = 逐字节恒等。伴生误读：时钟 = dawn 对齐映射
  （hour24 = (t−0.05)×24+6），start 0.25 表读 **10:48 非直觉 6:00**
  （`--time 0.25` 显式实证）。
- **S2 canary 四臂**：example / tutorial_dig / tutorial_build /
  tutorial_fly 各 `--frames 600 --save` ×2 = **RAW 恒等 + savenorm
  EXACT**（面板 5 行新布局下复跑确认）。
- **S2b 用户报障场景实证**：git `4531c42`（v3.11.0）提取的旧
  example.dmo（legacy `version: 3.11.0`）= **回放成功**（rc 0，日志
  「recorded with 3.11.0」）×2 RAW 恒等 + savenorm EXACT + **终档
  savenorm ≡ 现行 schema example 回放终档**（映射行为恒等 = 零迁移
  最强形）。
- **S2c sim 门面**：sim-2 文件（legacy 提取 + 手工改写现行 schema
  sim=2）= **rc 1 点名**（消息三片段：`demo sim 2 != this build sim
  1` / `(recorded with 3.11.0)` / `simulation semantics differ`）+
  `--demo-force` = **rc 0 + WARN 点名** + 回放完成。
- **S6 参数面**：`--demo-force` 无 `--demo` = **rc 2** 点名（「--
  demo-force requires --demo (it bypasses the replay's sim version
  gate only)」）。
- **S3 全套**：2784/2784（2777 基线 + V3-8a 手柄 open 修复 1【用户
  11-06 并行提交 d5a5ea6，本提交前已入树——门证据全在合并树上】+ 本版
  +6），`-Wall -Wextra -Wpedantic -Werror` 0 警告。
- **性能 A/B（用户硬要求）**：旧 = `648b80d` v3.12.0 + g139（banner
  实证）vs 新 = v3.13.0 + g139（banner 实证）：900f 无输入同 seed
  （perf_gate_config）——PERF-WIN 三窗 upd med 0.702/0.643/0.679 vs
  0.644/0.664/0.677（窗口间机器噪声带）+ render 0.743/0.740/0.770 vs
  0.710/0.774/0.758 + swap med 15.007/15.040/15.034 vs
  15.040/14.960/15.015（新侧 win1 max 14955 ms = 一次性 OS 停顿，med
  正常）+ fluid·fires·spread·ents·stream 同档 + PIPE3-S0 tickMs 同档；
  changesApplied 9/2/1 vs 9/3/1（win2 差 1 = **real-dt 追赶抖动**
  PITS 385 域，非语义差）+ **终档 savenorm EXACT**（模拟面恒等权威）。
  demo 加载/门 = 开窗前冷路径，无输入/无 demo 运行零触碰 = 结构性
  零成本论证。

## 记档偏离

1. **legacy 映射 = 设计内**（非妥协）：旧严格门的证明力（record-build
   == replay-build）直接支撑 sim 1 映射的合理性；不引入「按 version
   串查兼容表」的中间层（无消费方，YAGNI）。
2. **`recorded_with` 面板独立行**：合并 name 行方案在 ui.scale 2
   （16px 字形 ×2 = 32 px/字）下 32 字行 1024 px > 679 px 门窗溢出，
   独立行 20 字 640 px 在界；面板 4 行 → 5 行（行距 -68/-36/-8/+16/
   +40 相对 h/2，UI 逻辑坐标）。
3. **canary = 4 个 shipped demo**（非全部可回放文件）：用户自录 demo
   的跨版本回放由 sim 门 + force 逃生门覆盖，不进门。
4. **day_cycle.hpp 注释更正**随版（零行为）：`// noon` → 10:48 映射
   说明——门内时钟误读的根因卫生。

## PITS

- **PITS 394**（S1 锚假失败 = 用户 config 调值 + 时钟映射误读）：锚 =
  committed config 态；S1 锚比较失败先 diff config（committed vs 工作
  树），UI 几何调值（ui.scale/font 族）= 用 committed 副本 + `--config`
  重跑；判别三件套 = diff 仅 UI 文本带 / glyph 尺寸比 = scale 比精确 /
  config diff 单键。

## 变更文件（显式路径，26）

src/core/demo.hpp / src/core/demo.cpp / src/core/game.hpp /
src/core/game_init.cpp / src/core/game_ui.cpp / src/core/main.cpp /
src/world/day_cycle.hpp（注释）/ tests/test_v3110_demo.cpp /
tests/test_debug_overlay.cpp / CMakeLists.txt /
assets/demos/example.dmo / assets/demos/tutorial_dig.dmo /
assets/demos/tutorial_build.dmo / assets/demos/tutorial_fly.dmo /
tools/make_example_demo.py / AGENTS.md / README.md / docs/BACKLOG.md /
docs/CHANGELOG.md / docs/GAMEPLAY.md / docs/KNOWN_ISSUES.md /
docs/MECHANICS.md / docs/MECHANICS_WORLD.md / docs/PITS.md /
docs/PROTOCOL.md / docs/archive/phase-3.13.0.md（本文）。

Release 政策 = 11-05 定案延续：v3.13.0 只留 **tag + 提交**，无新
release（Latest = v3.9.0 期合并 release 不变）。
