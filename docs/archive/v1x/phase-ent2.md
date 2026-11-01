# Phase ENT-2 — BlockState（v1.0 任务表 7/29）

> 设计案 = ora-2（oracle）定稿 + 代理开放问题拍板（用户夜间授权"按建议的来"）。
> P94 楼梯朝向（World 每格 byte + format 14 `stairs`）泛化为通用 per-instance
> 状态机制；CRAFT-1 / GEN-2 / slab 叠合 / 栅栏顶等的前提。

## 一句话定案

**world-resident 逐格 byte 状态机制的通用化**：`Chunk::m_stairFacing` →
`m_blockState`（同尺寸换名 + 哨兵泛化 `kBlockStateUnset=255`）+ 双门控数据
驱动（`Shape::Stairs` 隐式 count 4/default=stair_facing ∪ model 字段
`state_count`/`state_default` 严格配对）+ World `get/setBlockState` + setBlock
收口/放置/潜旋三处泛化 + **format 15 `states` 段**（替 `stairs`，journal 派生
+ 全量 fallback + 双向交叉校验 + 14→15 值保真迁移）+ **init 交叉校验
（stateful 块不得出现在任何 worldgen 放置源 = journal 推导前提结构化为
不变量）**；**四个 per-block map 不迁移（YAGNI）**；门 85 = 结构性免疫
不重锚。

## 开放问题拍板（设计 §9，按推荐通过）

- **Q1 值域校验位置 = 是**：World::setBlockState 只验 <255（World 无
  registry，架构分离）；域 [0,stateCount) 由收口 chokepoint / 旋转 / parse
  交叉校验三处强制。
- **Q2 shipped 楼梯 = 否**：不显式声明 state_count（零内容改动 = 门 85 论证
  最简；冲突拒绝路径由单测覆盖）。
- **Q3 states 段 = 全量**：每个 stateful 格恰一条（含默认值）——P94/furnace/
  campfire 既有纪律、双向交叉校验统一形态、避免"非默认才存"手改静默回落陷阱。

## 架构 in/out 清单

### In

1. **Chunk**：`m_stairFacing` → `m_blockState`（同 array）；`kStairFacingUnset`
   → `kBlockStateUnset`（255）；`stateAt/setState/clearState` 换名。
   trivially-copyable / 快照 / 内存零变化。
2. **双门控数据驱动**：`BlockModel` + 可选字段 `state_count`（JSON
   `model.state_count`，int [1,255]，缺省 0 = 无状态）+ `state_default`（int
   [0,stateCount)）——严格配对（缺一 = 点名拒，fast_against 范式）；Stairs
   形状冲突拒（显式写 state_count = 矛盾点名）。`Block` 三纯访问器
   `hasState()`（shape==Stairs ∨ stateCount>0）/ `stateCount()`（Stairs→4 /
   stateCount）/ `stateDefault()`（Stairs→stairFacing / stateDefault）。
   **shipped 内容零改动**。
3. **World API**：`getBlockState`（void/未创建读哨兵）/ `setBlockState`
   （拒 y 越界/无 chunk/v≥255/无变化 false；变化 → 自身+邻 chunk 标脏）。
4. **Game 三写入点泛化**（行为对 shipped 内容零变化）：收口
   （hasState→setState(default) / else→清哨兵，非 stateful 格无变化 false 零脏）
   / 旋转（门 hasState()，(s+1)%stateCount()）/ 放置钩子（stair yaw 覆写保留，
   经 setBlockState；此槽 = 未来放置状态规则扩展点）。
5. **消费者换名**：entity solidTopHeight + mesher emitStairs（形状门 + 哨兵
   回落不变）。
6. **init 交叉校验（新）**：WorldGenerator::create()——任一 worldgen 放置源
   块名（biome surface/subsurface/decorations、ores.veins、trees
   trunk/leaves）hasState() → 点名拒。
7. **存档 format 15**：`states` 段（下详）。
8. **文档**：CONTENT_GUIDE（state 字段 + 双门控 + "状态不改光属性"约束）/
   MECHANICS（BlockState 节 + 两类状态存储分界）/ save.hpp 格式历史 15 条。

### Out（不做 + 理由）

- **m_furnaces/m_campfires 迁移**：struct 负载（~50-80B/格）进 chunk array
  不可行（数十 GB）；统一注册表 = 重写 format 7/11 + tick + 面板，无 v1.0
  任务要求（CRAFT-1 走 campfire map 范式 + 本相确定的纪律）。两类状态消费者
  集合不相交（byte：快照 mesher/碰撞热路径；struct：game tick/interact/
  overlay）= 消费驱动的自然分界，非历史欠债。
- **m_crops/m_beacons**：无状态（块 id 即阶段 / 每帧重算），不在机制范围。
- **命名多状态 / chunk 内 struct**：byte 上限 255 值够 v1.0 全部消费者
  （facing 0-3 / top-bottom 0-1 / sapling stage 0-7）。
- **slab 叠合 / 栅栏顶 / sapling 内容**：未来相消费本机制（各自加
  emit/collision 分支 + JSON，零核心改动）。
- **journal 单独立表**：不需要（值 live 读 + 位置靠 block 写，旋转先例）。

## 存档 format 15 段定义

`{ "states": [ { "pos": [x,y,z], "state": N }, ... ] }`，位置排序，每个
stateful 格恰一条（含默认值）。

- **域**：state ∈ [0, block.stateCount())，按 entry 位置对应块（blockEdits
  名 → registry）逐条目校验；越界点名。
- **键纪律**（版本谎报拒绝）：format 15 必需 `states`（空数组合法）；≤14 携
  `states` 拒；15 携 `stairs` 拒（顶层白名单加 "states"，"stairs" 限 14）；
  entry 严格字段 {pos, state}，未知键拒。
- **双向交叉校验**：stateCells = blockEdits 中 hasState() 的格；entryPos ↔
  stateCells 相等，逐格点名。
- **14→15 迁移**（parse 内）：format 14 按现有 stairs 解析（含交叉校验）后
  逐条 {pos,facing} → {pos,state=facing} 值保真；≤13 加载 = states 空。
- **collectSave 两路同改**：journal 路遍历 m_saveJournal.blocks → live 块
  hasState() → {pos, live 值（哨兵回落 stateDefault）} → 位置排序；legacy 路
  O(N) 扫描同泛化。两路等价 = init 不变量保证（stateful 格 ⊆ journal）。
- **applySave**：块/流体 replay（收口已设默认）→ 逐条 setBlockState（存值
  权威覆写）→ 既有槽位（4c-quater）顺序不动。
- **写者**：serializeSave 换段名/字段名 + root["format"]=15；kSaveFormat
  14→15 + 格式历史注释加 15 条。

## 约束（写入文档）

状态值**不得**改变块的光属性（opaque/light_opacity 是 per-block 非
per-state）——v1.0 全部消费者满足；per-state 光 = 未来扩展。

## 测试计划（~18 新 + ~8 换名适配）

- 解析（test_block）：配对合法+访问器 / 缺一拒×2 / count 域 [1,255] +
  default 域 / stair 显式 state_count 冲突拒 / 真实资产 4 楼梯变体钉死。
- 存储/World（test_world）：哨兵读 / set-get-无变化-标脏 / 拒 v=255、y 越界、
  无 chunk。
- 存档（test_save）：15 往返（旋转后保持+段值）/ 15 必需 states / **14→15
  迁移值保真** / ≤14 携 states 拒 + 15 携 stairs 拒 / 交叉校验双向 / 域越界拒
  / 空数组合法。
- journal（test_save_journal）：journal 派生 == O(N) 扫描（oracle 等价，
  既有 stair 版泛化）。
- 生成器（test_world_generator）：biome surface = stateful 拒点名 / ore =
  stateful 拒点名。
- 换名适配（非新增）：test_entity stair ×2 / test_mesh_job stair ×3 /
  test_save+test_block 存量引用——API 换名、语义断言不动（行为零变化实证）。

## E2E 计划（今晚 10 min，无输入为主）

- **S1 门 85**（~2 min）：新二进制 fresh 60f ×2 全帧字节一致 + **与 gate 84
  锚（/tmp/opencode/s1_l2c_r1.png）字节一致** + 红心 0 + skygate。论证 =
  无输入零 stateful 块（init 不变量）+ 收口对非 stateful 格零脏 + 放置/旋转
  纯交互零触发 + 换名不改生成/光/几何。
- **S2 预置状态场景**（~4 min）：scratch format 15 save（stone_stairs 场景：
  states 值 1 east + 玩家/相机锚定）→ 600f no-input ×2 RAW + savenorm ×2 +
  EXACT（重存 states 含该值 = load→save 往返存活）；对照 save（state 0）
  截图 A/B diff 限定 stair 楔形区（状态→mesh 通路视觉证明）。
- **S3 迁移 A/B**（~2 min，预算内可选）：format 14 stairs 文件新二进制加载 →
  600f 退出重存 15 → states 值 == 原 stairs 值（迁移 E2E 级背书）。
- **晨间（输入）**：潜行+右键旋转 stair state 0→1 循环 ×2 savenorm + EXACT
  （P94 S2 回归，泛化行为零变化实证）。

## 风险清单（实现注意）

1. 换名遗漏 → 编译错误（机械安全）；测试引用 ~8 处适配。
2. 迁移非值保真 → 单测 + E2E S3 双钉。
3. 交叉校验域按块类型：save.cpp findByName 槽复用。
4. 旋转/收口泛化对 shipped 内容 = 零变化（S2 对照实证）。
5. journal 不变量靠 init 校验结构性保证；位移双上报只涉 block/fluid 表，
   状态 live 读不受影响；unload 列 = 纯 fresh（无 stateful，init 不变量）
   → 无孤儿状态。
6. applySave 顺序：states 在块 replay 后（4c-quater 槽不动）。
7. -Werror：byte/int 提升显式 cast。
8. 哨兵回落三处（碰撞/mesh/save 推导）同规则 = stateDefault()。

## 构建与验证结果（E2E）

- **实测**：1260/1260（存量 1240 + 20 新；~12 处换名适配语义断言不动），
  干净重建 `-Werror` 0 警告。22 文件 +1084/−331。
- **S1 base18 门 85 = 结构性免疫（不重锚）**：新二进制 fresh 60f ×2 全帧
  字节一致 + **与门 84 锚（/tmp/opencode/s1_l2c_r1.png）字节一致** + 红心 0
  + skygate (120,181,242) 75,348 px 严格。论证兑现（无输入零 stateful 块 +
  收口零脏 + 换名不改生成/光/几何）。
- **S2 预置状态场景**（stone@(22,38,0) + stone_stairs@(22,39,0)，玩家
  (22.5,38,1.5) yaw 0 pitch −0.08 = 楼梯正对取景）：state 1（east）新 ×2
  **RAW 字节一致** + 全存档 savenorm **EXACT**（含 31,271 流体编辑 = 全环
  确定性）；state 0（north）新 ×2 RAW；**A/B 全帧 diff 298,211 px（中央
  掩码 82,725）= 状态→mesh 通路视觉实证**【per-instance 朝向首次像素级
  验证——P94 时代 E2E 仅存证】；round-trip（加载 a1 终态存档再跑 600f）：
  截图与 a1 **字节一致** + core 段（format/blocks/states/player/entities/
  seed）EXACT【fluids = 37,799 > 31,271 = 环边水 600f 继续演化，离屏预期】
  + 重存 states 含 (22,39,0,1) = 往返存活。
- **S3 迁移 A/B**：format 14 `stairs {facing:1}` 文件新二进制加载 → 600f
  退出重存 = **format 15 `states {state:1}`**（值保真）+ **无 stairs 键** +
  **截图与 S2A 字节一致**（迁移 = 同一渲染，端到端背书）。
- **晨间（输入，推迟）**：pent2_rot.zsh 就绪（LeftControl 按住 + 右键 =
  (s+1)%count 旋转回归，P94 S2 范式）——夜间 away 组件偷 lock（s3 白帧
  3 连 = 其间歇性窗口隐藏所致，重试守卫消化）。

## E2E 执行注记（PITS #208）

场景首版按 AGENTS 记载的 P94 几何（y45/y46）构造 → 楼梯悬空 8 格出框 +
A/B diff 0 误诊为 state→mesh 断链（旧二进制 A/B 对照才定位 = 场景几何错，
非代码）。落体探针实测 (22,0)/(22,1) 地面顶 = **38** → 重建场景全过。
教训：复用历史场景坐标前必须 ground 探针实测当前地形。
