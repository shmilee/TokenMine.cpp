# Phase UX-1 — 夜变暗程度（v1.x 任务表 34/40，门 150，UX 行 1/7）

> 深夜天空地板可配置（`render.night_sky_level`），可设全黑夜。消费 10-27
> 用户实机定案「深夜须可夜视」（LGT-3 初版深夜全黑被推翻 =
> `kNightSkyLevel 7` 的由来）→ 该定案转为**可回退设置**。UX 行首项
> （10-30 用户定案 7 项，矩阵 33→40）。

## 定案（10-30 用户确认方向）

- config `render.night_sky_level` int [0,15]，**代码默认 7 = 对 v1.34.0
  位级恒等**（门 S1 走结构性免疫，免重锚）
- **全黑夜 = L 0 且环境底随地板归零**（`kAmbient 0.03` → 0.0 = 真纯黑
  无残光）；L > 0 环境底恒 0.03
- spawn 成对语义任意 L 成立：夜露天天空等级 = L 本身 → L ≤ 7 敌对照生
  （0 ≤ 7）/ 火把圈 14 恒免疫 / L = 15 夜不生成（15 > 7）= 用户配置
  自己选择（规则结构不破）
- 10-30 用户定案 **UX 行 tag/release 政策 = 同款 10-28**：两两合并 tag
  （v1.36.0 = UX-1+2 / v1.38.0 = UX-3+4 / v1.40.0 = UX-5+6 / v1.41.0 =
  UX-7 收官）+ UX 系统 release 收官时发（notes 覆盖 v1.35-1.41）。本版
  不打 tag。

## 实现

1. **config 严格读** `world.readNightSkyLevel(Config)`（新纯函数，
   SAVE-1 `autoSaveIntervalSeconds` 同款纪律——Config getter 对类型失配
   静默回落，故 raw 读）：absent = `kNightSkyLevel` 7 / present = JSON
   number。**检查序 = 非 number → 非有限 → 范围（double 域）→ 整数 →
   int 投影返回**（范围先于一切 int 投影 = `static_cast<int>(1e18)`
   UB 路径封堵，编排者核证发现 + 修正）；越界 got 值走 `std::to_string(d)`
   双精度原样点名
2. **shader 两 const → 两 uniform**（`basic.frag`）：`u_nightRetreat =
   (15−L)/15` + `u_ambient`（L=0 → 0.0，否则 0.03 = `kAmbientFloor`
   新单一真源）；**表达式树一字未动**（`max(v_skyLight -
   u_nightRetreat*(1.0-u_dayFactor), v_blockLight)` /
   `base*(u_ambient + (1.0-u_ambient)*light*v_faceShade)`）；默认 L=7 时
   上传值 = 8.0f/15.0f、0.03f 与旧 GLSL 字面量（double→float 舍入）
   **位级同值** = S1 结构性免疫根
3. **uniform 注入 = init 一次性**（`Game::buildScene` 严格读后
   bind→set→unbind）：两 uniform 是**每 run 常量**（config 驱动、零
   per-draw 变化）→ UXFIX-1 泄漏类结构不适用（无受害者/无残留态）
4. **主机纯函数尾参参数化**（默认参 = 旧常量 = 既有调用点位级不变）：
   `effectiveSkyLight(stored, df, nightLevel = 7)`（double 运算序不变
   仅换常量）/ `effectiveSkyLightF(...)`（改调
   `nightRetreatScale(nightLevel)`，L=7 位级同 `kNightRetreatScale`）/
   `currentSpawnLight(world,x,y,z,df, nightLevel = 7)` /
   `findMobSpawnCell(..., nightLevel = 7)`（Game 调用点传
   `m_nightSkyLevel`）
5. **仿射撤退其它消费者零代码**：信标光束（LGT-4
   `beamLightSample`）/ 掉落（PRE-6 `itemDropLight`）返回 RAW 光，
   昼夜撤退由 shader 公式统一施加 → 自动继承配置地板（注释同步）；
   云/手持/面板块立方等 basic 段 draw 同款自动继承
6. **版本**：CMakeLists 1.34.0 → 1.35.0 + test_debug_overlay F3 banner
   钉意识更新（v1.0 纪律）+ 构建后 banner 实证（PITS 306）

**零 format（22）/ 零 config 文件改动 / 零内容 / 零音效 / 零帧状态**。

## 变更文件

- `assets/shaders/basic.frag`（uniform ×2 + 公式换源 + 注释演进）
- `src/world/lighting.hpp`（`kAmbientFloor` + 3 纯函数声明 + 2 函数尾参
  + 文档演进；include config/error）
- `src/world/lighting.cpp`（实现 + 严格读安全序）
- `src/entity/mob_spawning.{hpp,cpp}`（currentSpawnLight/findMobSpawnCell
  尾参 + 成对语义任意 L 文档）
- `src/core/game.{hpp,cpp}`（`m_nightSkyLevel` 成员 + buildScene 严格读 +
  uniform 注入 + spawn 调用点透传）
- `src/world/beam_light.hpp` / `src/entity/item_light.hpp` /
  `src/world/day_cycle.hpp`（公式引用注释同步，零逻辑）
- `tests/test_lighting.cpp`（+15：config 严格读 11【absent×2 / 0 / 15 /
  16 / -1 / 7.5 / "abc" / true / null / 1e999 / **1e18 大有限值防御**】+
  nightRetreatScale 位级钉 + ambientFloor 规则钉 + effectiveSkyLight
  配置地板【含 16×16 正午恒等网格】+ F 版位级钉）
- `tests/test_mob.cpp`（+1：spawn 镜像 L 0/7/10/15 深夜值 + 正午恒等 +
  默认参钉）
- `tests/test_debug_overlay.cpp`（banner 钉 v1.35.0）
- `CMakeLists.txt`（VERSION 1.35.0）

## 测试

1929/1929（+16 新，零既有改动）、`-Wall -Wextra -Wpedantic -Werror`
0 警告、banner v1.35.0（编排者独立复跑实证）。

## 门 150（编排者执行，old 侧 = v1.34.0 = 696bca1 = tm_v135_ref）

- **S1 PASS = 第 150 次结构性免疫 + base18 锚连续 168140 B 免重锚**：
  fresh 60f 四方（NEW×2 / OLD v1.34.0×2）RAW 全字节一致（168140 B）+
  savenorm new×2 / 跨版 EXACT
- **S2 PASS（深夜 time 0.75 无输入 600f 多向 A/B，nomobs 隔离 spawn，
  9 跑全 679×744）**：×2 RAW/侧 4/4 EXACT + **old ≡ new-L7 字节一致
  160959 B = 代码级免疫（默认地板在深夜帧亦位级恒等）** + old 侧
  `night_sky_level: 0` config ≡ old 默认 = 未知键容忍（forward-compat
  纪律实证）+ 六存档 savenorm 全对 EXACT（光照不入档）+ **像素单调
  判别：L0-vs-L7 diff 378,350 px 逐像素分量全仅暗化（违反 = 0）/
  L15-vs-L7 diff 378,350 px 全仅亮化（违反 = 0）**——双向 diff 同数
  = 时间不变集（全受照世界面 = 撤退公式的完整作用域，LGT-3 门 118
  同型特征一致性证明）；L0 全黑夜截图 22453 B ≈ 纯黑帧实证
- **S3 PASS**：1929/1929（编排者独立复跑 ×2）
- **S6 PASS ×3**：16 → `must be in [0, 15] (UX-1 night sky level: 0 =
  all-black night, 15 = no retreat), got 16.000000` / −1 → 同款 got
  −1.000000 / 7.5 → `must be an integer`（rc=1 init 死、无 clean exit）
- **性能 A/B PASS（发布前必做，fall 900f 无输入，零并行 CPU）**：
  tm_v135_perf_old（v1.34.0 + 27 标记）vs tm_v135_perf_new（v1.35.0 +
  同款 27 标记，banner 双侧实证）：PERF-WIN 三窗 upd/render/swap/
  fluid/fires/stream 全同档（±10% 噪声带内，绝对值极小）+ PIPE3-S0
  changesApplied **9/3/1 三窗逐位** + **终档 fluids 18/18 + blocks
  55/55 集合恒等 diff 0 = 同世界同成本（结构惰性最强形**：默认 L=7 =
  uniform init 一次注入零 per-frame 执行 + 纯函数 L=7 位级恒等**）**

### 门脚本坑（首轮失效记录，数据最终全收）
首轮 g150_gate.sh 因两处脚本缺陷误判全 FAIL（游戏本身零问题）：
① 截图参数漏 `$OUT/` 前缀 = 相对路径 → PNG 静默落游戏 CWD（仓库根 /
ref 树），脚本 stat 恒 sz=0 重试 15 轮——**修复 = 门脚本截图/场景路径
恒绝对 + 落盘后按路径复核**；② S6 函数 `local cfg=$1 label=$2
log=$OUT/s6_$label.log` 单行赋值的 `$label` 在 `set -u` 下自引用未绑定
→ 崩溃——修复 = 分语句。散落 PNG 归位 raw/ 后全部断言一次收讫
（= 数据本身有效，只换收集点）。另见 PITS 341/342。

## 偏离/记档

- 全黑夜（L=0）的**世界面、云、手持/面板块立方、掉落、光束**全读纯黑
  除非有方块光（火把/萤石/火 = 方块光不受昼夜影响恒亮 = MC 同款）——
  用户选 0 即选全黑
- L = 1..6 与 8..14 中间档：环境底恒 0.03（**不随 L 缩放**——「随地板
  归零」规则仅 L=0 触发；中间档暗区与虚空仍区分 = 既有设计意图保持）
- spawn 成对语义 = 规则结构任意 L 成立，但**玩法闭环只在默认 7 附近
  设计**（L=15 = 夜无敌对生成 = 用户自选 peaceful-夜；L≤7 = 夜险保持）
- 配置只 init 读取（difficulty 同款 = 运行中改 config 不生效，重启生效）
- `render.night_sky_level` 不进 config/config.json（代码默认纪律 =
  fire/mobs/beacon 同款；用户调值文件零触碰）

## 坑位

- **PITS 341**（门脚本截图/场景路径恒绝对 + 批内几何门用 PNG 尺寸非
  字节大小）+ **342**（门批前查活跃游戏进程 = 零并发 :0 活动纪律 +
  日志时钟 NTP 回拨非单调，awk 子串匹配命中前日时段行）——详见 PITS.md
- **PITS 270 第 3 次复现**：tm_v135_perf_new 构建 rsync 用无锚
  `--exclude data` 连 `assets/data/` 同排 → perf 新侧 init 秒死（blocks
  目录缺失 + block_atlas.json 读失败伪装）——性能树资产复制排除项恒
  根锚定 `--exclude '/data'`（补拷 assets/data 后 v135 侧重跑收讫，
  插桩补丁不受影响）
