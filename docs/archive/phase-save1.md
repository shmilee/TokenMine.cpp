# Phase SAVE-1 — 自动存档（v1.0 任务表 26/32，门 107，SAVE 行 1/1 收尾）

> 周期性自动存档 + 崩溃韧性原子写。消费 KNOWN_ISSUES "无周期性自动存档"
> （Phase 13 条，F5 手动保存 Phase 15 已解决部分）。

## 实现内容

1. **config 键 `save.auto_save_seconds`**（代码默认 `kAutoSaveDefaultSeconds
   = 120.0`；**config/config.json 零改动** = fire/mobs/beacon 节同款模式）：
   `save.autoSaveIntervalSeconds(Config)` 严格读（player.cpp config-reader
   纪律——Config getters 对类型失配静默回落默认，故走严格读取）：缺省节/键
   = 默认值；present = 有限数 ≥ 0（**显式 0 = 关闭** = 当前行为）；负数/
   非有限（1e999）/非数字类型（字符串/bool/null/object）点名拒
2. **自动存档计时器**（真实 dt = 生存计时器同款纪律，硬规则 8）：
   `save.autoSaveStep(accum, dt, interval)` 纯函数（campfire-tick 范式：
   interval ≤ 0 恒 inert 不动 accum）+ `Game::updateAutoSave(dt)` 薄胶水
   ——**`--save` 槽门控**（无槽 = 计时器永不累加 = 结构惰性）→ 到点
   `writeSave(true)`：成功归零 / **失败不清零 = 下帧重试**（无退避 = 记档
   简化）+ ERROR 日志；在 `update()` 帧末（updateSoundTimers 后，World
   单写窗口内 = F5 先例）
3. **原子写**（崩溃韧性核心，**F5/退出/自动三路共用**增强）：
   `save.writeAtomicFile(path, content)` —— 同目录 `<name>.tmp` → flush +
   close → `std::filesystem::rename`（同目录 POSIX 原子）；任一步失败 =
   删 tmp（若存在）+ MC_FAIL 原语义（错误消息仍指目标路径）；**失败写永不
   毁旧档**（rename 前目标不动）；陈旧 tmp（崩溃残留）= 下次写复用清理
4. **日志行可区分**：`World auto-saved to '...'（...）` vs 既有
   `World saved to`（F5/退出原样保留）

**零 format（19）/ 零 config 文件改动 / 存档内容逐字节同形**。

## 变更文件

- `src/core/save.hpp`（`kAutoSaveDefaultSeconds` + 3 纯函数声明）
- `src/core/save.cpp`（3 纯函数实现）
- `src/core/game.hpp`（`writeSave(bool byAutoSave = false)` +
  `updateAutoSave` + `m_autoSaveInterval/m_autoSaveAccum`）
- `src/core/game.cpp`（buildScene 严格读 config + writeSave 原子写 + 日志
  区分 + update 帧末挂钩 + updateAutoSave 定义）
- `tests/test_auto_save.cpp`（新，23 测）
- `CMakeLists.txt`（测试 +1 文件）

## 测试（1590/1590，+23 新）

- config 解析 11：缺省 120 / 显式 0=off / 整数 / 小数 / 负数点名含值 /
  字符串 / bool / null / object 点名 "must be a number" / 1e999 非有限点名
- 计时器步进 5：未达 / 恰达 / 部分帧累积 / off 恒 inert 不动 accum /
  失败下帧 dt=0 仍重试
- 原子写 7：成功无 tmp 残留 / 建父目录 / 覆盖 / **陈旧 tmp（崩溃残留）
  复用清理** / open 失败旧档原封 / rename 失败（目标是目录 EISDIR）tmp 清
  + 目标不动 / 父路径被文件堵死点名 + 无 tmp
- Game 侧本体（窗口/GL 依赖）不可 headless 单测 = 项目既有模式，节奏经
  门 107 S2 实证钉死（3 s 间隔 = 实测 3.017/3.016/3.016 s）

`-Werror` 0 警告。

## 门 107（fixer 实跑，编排者证据核验毕）

- **S1 PASS = 第 107 次结构性免疫**（10-26 04:5x，fresh 60f ×2 RAW 字节
  一致 + vs base18 **字节一致** = 免重锚；论证 = ① 无 `--save` 槽 = 计时器
  永不累加（槽门控）② S1 跑自带 `--save` 槽仍字节一致 = 60f 实时间 ≤ ~50 s
  ≪ 120 s 默认间隔 → 窗内零自动存档事件 + 退出存档为纯文件副作用 ③ 真实 dt
  accumulator 非帧状态（不入档/不碰确定性帧，音频计时器同款）④ 渲染/存档
  内容零触碰【只写路径变原子 + 日志行区分】）
- **S2 自动存档实证 PASS**：mov-config `save.auto_save_seconds: 3`（临时
  文件，用户 config 未动）600f 载入场景跑：日志 **3 行 "World auto-saved"**
  （实间隔 3.017/3.016/3.016 s = 3 s + 帧量化）+ 退出 "World saved to" 1 行
  （可区分断言成立）；终档 time 0.251667 → **0.268333 精确匹配**（scene +
  600f×1/36000）+ 玩家 pos/hp/yaw/pitch 恒等 + 世界推进 blockEdits
  9→10→13→14（熔岩湖火 churn，帧锁定 = **PIPE-2 增量 journal 经自动存档
  路径工作实证**）；零 `.tmp` 残留；**×2 终档 savenorm（fires remaining
  剥离 = PITS 244 先例）EXACT 字节一致**（未剥离差 = fires real-dt ±0.0004 s
  抖动非缺陷）
- **S2b 崩溃韧性 PASS**：interval=3 跑 ~4 s 见首行 auto-save 后 **kill -9**
  （rc=137）：slot 存在 + 可解析 + **format 19 + 玩家状态在**（time 0.256722
  = 中途 auto-save 快照，> scene 0.2517 < 终值 0.2683）+ **零 `.tmp`
  残留** = 目标恒为完整档（旧或新），半写档不可达（rename 完成早于日志行）
- **S6 config 点名 ×2 PASS**（rc=1）：`-5` → `config save.auto_save_seconds
  must be >= 0 (0 = auto-save off), got -5.000000`；`"fast"` → `config
  save.auto_save_seconds must be a number`
- **S3** = 套件 1590 PASS
- 性能 A/B v0.23.0（发布前必做）：fall 900f 三窗 **fluidCells
  123879/167781/236573 双侧逐位一致（同世界）+ upd/render/swap/ents/stream
  全同档** + **双侧 "World auto-saved" 事件 = 0**（跑期 ≪ 120 s = 结构惰性
  实证；GREEN/GREEN-timing 判 = swap med ×3 < 20 ms 无 stall）= 成本可忽略

## 偏离 / 记档

- **真实 dt 节奏**（MC 按游戏内 auto-save-ticks；本项目生存计时器 real-dt
  纪律一致 + 实现简单——记档）
- **无多槽轮转 / 无增量自动存档**（PIPE-2 增量机制在退出路径；自动存档 =
  同款 writeSave 全量快照 O(会话编辑数) ~17 ms 级够快——记档）
- **失败重试 = 下帧（1/60 s）级无退避**（记档简化）
- **残余（矩阵外）**："保存并退出"流程 / 多存档槽选择 UI（KNOWN_ISSUES 存档
  条保留）

## 坑位

- ofstream 未 flush 时同进程读 = 空文件（单测首轮 13 败全此因，helper 已
  修）——候选 PITS 收录
- `stabilize_window` 的 windowsize 会把窗打成 1280×720 = S1 尺寸门
  （1362×744）失配——S1 禁调（probe 轮询模式）
- zsh 独立 `===` 行触发 `=word` 展开报错（既有 gate 脚本尾行同款，rc=1
  假象，无害）
