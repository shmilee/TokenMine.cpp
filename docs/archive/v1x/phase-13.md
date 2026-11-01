# Phase 13 细节存档

> Phase 30 文档整理：由 `SESSION_CONTEXT.md`「Phase 13 已完成（细节存档）」节、`KNOWN_ISSUES.md`「已修复（归档）」条目 合并而成（未改写原文）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/...` 驱动脚本）指向当时状态；当前文档布局
> 见 `README.md`「文档地图」。

## 阶段报告（原 KNOWN_ISSUES「已修复（归档）」）

- **`[2026-10-04, Phase 13]` 无存档 / 背包与实体不持久化 / 流体改动不入档** — Phase 13 实现完整存档（`--save <slot>`/`--new`，`src/core/save.{hpp,cpp}`）：`data/saves/<slot>.json` = seed + 与 `generateChunk(seed)` 的 diff（`blocks`/`fluids` 编辑列表，只存不同的格子）+ 玩家 pos/yaw/pitch + 昼夜 timeOfDay + 27 格背包 + 选中格 + 掉落物实体（item/count/pos/age/pickup_delay）。读档 = 生成世界后逐格应用（先方块后流体）→ `propagateAll` → 恢复玩家/背包/实体（`setAge`/`setPickupDelay`）/`DayCycle::setTimeOfDay`。严格加载校验（format 号/seed 一致/注册表交叉/位置不重复/方块+流体不同格/未知字段=错误）。JSON 浮点序列化本就最短可往返精度（位精确）。新增 17 个单测（round-trip 位精确/严格校验全路径/diff 精确检出/apply 恢复与失败/diff→fresh→apply 全格等价）+ E2E（无 flag 基线字节不变；存档文件跨运行字节一致；加载路径截图字节一致；含实体存档 age 恢复精确）。

## 会话存档（原 SESSION_CONTEXT）

- `src/core/save.{hpp,cpp}`（新，无 GL 依赖）：`SaveGame`（seed/time/playerPos/yaw/pitch/
  selectedSlot/27×ItemStack/实体列表/blockEdits/fluidEdits，编辑按 pos 排序）；
  `diffWorld(world, reference 回调, blocks, ...)`——对每个已有 chunk 用**全新 scratch Chunk**
  跑参考生成器（与 World 创建时同一纯函数，generateChunk 逐格全量写入）比对 block+fluid，
  差异格入列表；`applyWorldEdits(save, blocks, world)`——先方块（setBlock 自动排水+标脏+
  必要时生成缺失 chunk）后流体（setFluid），逐条检查返回值、失败点名位置；
  `serializeSave(save, items)`（Result<Json>，未注册槽物品=显式失败）/
  `parseSave(json, items, blocks, expectedSeed)`（**严格**：format==1、seed 一致、
  time∈[0,1)、pos/yaw/pitch 数字、27 槽精确（空槽 item="" 且 count=0；否则已注册且
  1..maxStack）、selected∈[0,9)、实体 type=="item"+count 1..min(maxStack,64)+
  age∈[0,120)+pickup_delay∈[0,0.5]、编辑 y∈[0,128)+名字已注册+位置不重复+
  方块/流体不同格、**未知字段=错误**（机器生成文件，与用户数据文件的警告容忍相反））。
  JSON 浮点序列化本就是最短可往返精度（json.cpp 的 prec 1..17 搜索）→ 存档位精确。
- `game.{hpp,cpp}`：GameArgs 加 `saveSlot`(optional<string>)/`newWorld`；
  `makeChunkGenerator()`（同一 lambda 供世界创建与存档 diff 参考）；buildScene 在
  相机同步后 `tryLoadSave()`（无 slot/--new/文件不存在=静默跳过；否则
  readSaveFile→applySave：编辑→`Lighting::propagateAll`+`m_lastLightedChunks` 同步→
  玩家 teleportTo+yaw/pitch+相机→背包 27 格覆盖 hotbar 填充→实体重建
  （setAge/setPickupDelay）→`m_dayCycle.setTimeOfDay`）；run() 循环结束后
  （截图在循环内，先于存档）`writeSave()`（收集→序列化→`data/saves/<slot>.json`
  pretty；写失败只记 ERROR、退出码仍 0）。
- `day_cycle.{hpp,cpp}`：`setTimeOfDay(t)`（解 totalSeconds；远离 start_time 的值
  timeOfDay() 往返至多 1 ulp，呈现无影响）。`item_entity.hpp`：
  `pickupDelayRemaining()`/`setAge`/`setPickupDelay`（存/读恢复计时器）。
- `main.cpp`：`--save <slot>`（槽名限字母/数字/_/-，防路径穿越）/`--new`（须配 --save）
  + usage。`.gitignore`：`data/saves/`。
- 测试 397/397（17 新增：round-trip 深等含位精确 double；严格校验全拒绝路径
  [注意：槽级子用例必须保持 27 槽全长，否则被槽数检查先行拒绝测不到目标分支]；
  diff 精确检出 4 改动/未改动为空；apply 恢复后**全格** block+fluid 等价；
  apply 失败路径[流体格被占/ chunk 不存在]）。
- E2E（驱动 `/tmp/opencode/p13_e2e.zsh`，config seed 123456789）：
  - R1 无 flag 60 帧：与 `base3.png` **逐字节一致**（默认路径零影响）。
  - R2 `--save` 无输入：截图逐字节一致 + 存档 0 编辑、状态完整（time=0.2541666… 精确）。
  - R3/R4 加载×2（快照恢复）：截图**逐字节一致**（加载路径确定性）。
  - R5 交互配方(P11-A)+`--save`×2（先删档）：存档**逐字节一致**且逐字段核对
    （blocks=[[0,53,0,air],[0,54,-1,dirt]]、slot1 dirt 63、slot9 dirt 1、selected 1、
    玩家 (0.5,53,0.5) pitch -0.4、time=0.266666…）。
  - R6/R7 加载修改世界×2：截图逐字节一致 + 准星=放置 dirt 面 + 与基线不同。
  - R8 `--new`：全新世界（截图=基线）；损坏存档（format 99）→ 启动明确报错。
  - inter2（P11-A + 再破所放 dirt，420 帧）：掉落物停在 (0.5,54,-0.5)（距玩家 1.414>1.0
    不拾取）→ 存档含 1 实体；两次运行截图逐字节一致、存档除 age 外逐字节一致
    （age 差 2 帧=真实 dt 抖动）；loadm2×2 逐字节一致 + 日志 "1 entities" +
    再存档 age = 原值 + 60 帧真实时间（setAge 生效实锤）。
- 刻意简化（KNOWN_ISSUES「存档（Phase 13）」节）：opt-in 无默认世界/无多槽管理 UI；
  退出路径重跑生成器做 diff（CPU 尖峰，P14 并行化）；无会话中 autosave；
  玩家速度/onGround 不存（静止存档回落到整数平面逐位精确，空中存档速度丢失）；
  实体 age 真实 dt（存档文件 age 字段不逐字节）；setTimeOfDay ≤1ulp；
  演示区外编辑带上生成的 chunk；无 format 迁移（未知 format=拒绝）。
