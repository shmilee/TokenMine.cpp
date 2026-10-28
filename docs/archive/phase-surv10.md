# SURV-10 挤奶冷却（v1.x 21/33，v1.21.0，门 136，SURV 行 5/5 收官，新组 1.21-1.25 首版）

v1.x 任务矩阵第二十一项。原始定义已佚 = 代理定案最小完整版：**MC 5 分
钟挤奶冷却**（消费 KNOWN_ISSUES Phase-43 条"无 5 分钟挤奶冷却（MC
有）、可无限连续挤"）。

## 定案

- **冷却 = per-mob 帧锁定计数器**：`MobEntity::m_milkCooldownFrames`
  （int，`kMilkingCooldownSeconds = 300` × 60 = **`kMilkingCooldown
  Frames = 18000`** 命名常量，整数帧计数 = Phase 58 off-by-one 纪律）；
  `update` 每帧 −1（固定 1/60 mob 步长，硬规则 8 = 确定性）；
  `milkReady()` = ≤0；`startMilkCooldown()` 启动。
- **门控 = tryMilk 单点**：milkWith 匹配后、取物前——冷却中 = 不挤
  （不消耗 bucket、不产奶、不发声），**点击照吞**（failed-milk 既有
  规则）；成功挤奶 → `startMilkCooldown()`。
- mob ephemeral（不入存档）→ 冷却从不持久化（记档 = MC 同款：读档
  后 mob 重生无冷却）。
- 零 format/零 config/零内容（cow JSON 零改动——milk_with/milk_gives
  既有）。

## 实现

`src/entity/mob_entity.{hpp,cpp}`（常量 + 成员 + 两方法 + update 减量）、
`src/core/game.cpp`（tryMilk 门控 + 启动 + 两处注释）。

## 测试（1837/1837，+2 净，-Werror 0 警告）

- `MobMilkCooldownTest.CooldownBlocksAndExpiresOnTheFrameBoundary`：
  start → 17999 帧后仍冷却 → **第 18000 帧精确过期** → 再挤再冷。
- `MobMilkCooldownTest.CooldownIsPerMobInstance`：per-instance（兄弟
  mob 不受影响；空闲 mob 无倒计时）。

## E2E 门 136

- **S1 = 第 136 次结构性免疫免重锚**：fresh 60f 四方 ×2（nomobs = 无
  cow 无挤奶；冷却字段恒 0 惰性）+ base18 + savenorm。
- **S2 挤奶判别（输入跑 ×4）**：s136_cage 场景（cage 生成格 = 环
  5-6 首有效格【PITS 307 真环】+ 密封墙 + 洞区）+ 临时数据脚手架
  （cow-only + max_mobs 1 + **cow speed 0.01**【readPositiveNumber 拒
  0】= 牛停在确定性位置）：捕获 + 双右键（2 s 间隔）：
  - **NEW**：click1 挤奶（bucket 2→1、milk 0→1）+ click2 **冷却拒
    绝** → 终态 **bucket 1 + milk 1**；
  - **OLD（v1.20.0）**：双挤 → 终态 **bucket 0 + milk 2**；
  - 终态互斥 + ×2/侧 RAW（牛的姿态/位置 = 帧锁定 = fps 无关）。
  - **瞄准链坑（PITS 313）**：1.2 宽牛经物理 leading-cell 检查**隧穿
    1 格墙**（v1 墙基座 y44 缺口实证挤出 → 封死仍穿 = 宽 AABB 盲
    点）→ 设计改用"spawn 重叠 → 确定性挤出驻车"（牛恒停在墙西
    (24.4,45,-22.5)）+ 射线被墙挡 → 墙开 1 格 sightline 窗（牛 1.2 宽
    穿不过 1.0 窗）+ **点击时相机 = 场景值**（终相机漂移 = 脚本自
    身跑后 mousemove 时指针仍锁定 = 视角旋转，非 grab delta——修正
    PITS 309 归因；预补偿反而瞄高 7cm 脱靶实证）。
- **S3 = 套件 1837**。
- **无 S6**：零 parse 面（冷却 = 核心命名常量，无数据模型变更）。
- **S2 首跑 old_b 失奶重跑记录**：首批（g136_cowonly 未禁衰减）期间
  机器掉入低帧率相位（900f 实跑 >72 s = 12fps 级，PITS 272 类）→
  衰减 tick 次数随实跑时长漂移（hunger 18/19）→ old 侧 ×2 RAW 破
  （饥饿图标差）+ old_b 双点击静默失奶（无诊断，当时探针已撤）。
  处置 = **g136_cowonly 加 `player.hunger_decay_seconds 9999`**（g135
  nodecay 先例，消 real-dt 变量）+ 重跑脚本加**奶数证据门**（终态
  奶数不达期望 = 整跑重试，不留在证据里）→ S2 四跑全过（黑窗相位
  重试 i≤4 覆盖；new×2 RAW 452077 B / old×2 RAW 452070 B 全 EXACT）。

## 性能 A/B（发布前必做）

tm_v120_perf（banner 实证 v1.20.0）vs tm_v121_perf（**全新 rsync 树 +
插桩 fluid.cpp 移植 + step0 patch + hunk#6 前台手工 + 全新 cmake 配
置** = PITS 306/312 纪律；banner 实证 v1.21.0），同 seed fall 900f：
规则 = mob update 一行 int 减量 + tryMilk 一个早退分支（fall 场景无
 mob 无输入 = 双结构惰性）⇒ **PASS = 成本可忽略（结构性惰性实证）**：fluidCells win1
51930/51927（±3 fill 窗 real-dt 类）+ **win2 29803 = 29803 逐位 +
win3 29800 = 29800 逐位** + **PIPE3 三项全逐位**（visitedChunks
266 / fluidCellsSeen 43048 / changesApplied 1404）+ upd/render/swap
全同档。fall 场景无 mob 无输入 = 冷却减量 + tryMilk 门控双零执行。

## 坑（门内）

- **宽 AABB 穿墙**（PITS 313）：1.2 宽 cow 的 x 移动 leading-cell
  检查只看西面格（air）→ 穿过 1 格石墙（东半仍嵌墙内）；v1 场景墙
  基座 y44 被 ring-hole 分支覆盖 = 出口 → 封基座后仍穿（leading 盲
  点与缺口无关）→ 笼子设计弃"密封"改"确定性挤出驻车 + sightline 窗"。
- **点击时相机 = 场景值**（PITS 313 修 PITS 309 归因）：MILKPROBE
  dump 实证点击时 m_camera = 场景 yaw/pitch（无漂移）；终存档的
  (−0.0175,−0.1675) 漂移 = 脚本跑后 mousemove 到角落时指针仍锁定
  （相对位移 = 视角旋转，固定窗几何 → 固定量 = 可复现根因）。按"终
  相机"预补偿瞄准 = 脱靶（射线高过牛头 7cm 实证）。
- **后台性能树构建静默死**（PITS 312 第 3 连）：v121 树 rsync+patch
  后台步死于 patch RC（set -e）零输出 → hunk#6 前台补（15 markers +
  banner 实证）。
- speed 0.01 非 0：mob `speed` = readPositiveNumber（0 拒）= 临时缓
  牛的最小合法值。

## 发布

v1.21.0 = **新组 1.21-1.25 首版**（组内不打 tag；组末 v1.25.0 → tag +
GitHub release）。正确性 A/B 旧侧 = v1.20.0 = fdcbc91（tm_v120_src）。
