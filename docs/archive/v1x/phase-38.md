# Phase 38 细节存档

> 用户确认方向（原话）："P38是建议2"（= P37 报告候选 2：skeleton 后续三
> 项——① 射击视线检查 ② 掉落 0–2 随机 + bone 物品 ③ 箭目标预判）。

## 阶段报告

- **`[2026-10-11, Phase 38]` skeleton 后续三项（LOS 门 + 掉落随机区间 +
  箭目标预判）** — P27 落地 skeleton 时的三个已知缺口（KNOWN_ISSUES Mob
  节）一次补齐，全部数据驱动 + 确定性。

  **① 射击视线检查（LOS 门）**
  - `hasLineOfSight(world, from, to)`（`world/raycast.hpp/cpp` 新纯函数）：
    `raycastBlock`（origin cell 豁免、与箭飞行同一 DDA）无命中，或命中
    距离 ≥ 目标距离 − 1e-3 → 通。与箭的实际飞行谓词一致（箭反正只会插墙
    消失，门只是省掉这支箭 + 给 AI 一个"被挡住"的信号）。
  - 门在 `Game` 的 skeleton 射击回调内：blocked 则**不发**，但 AI 已消耗
    本轮冷却 → 整间隔重试（Minecraft 行为：skeleton 被墙挡时不贴墙、不
    绕路，就是等下个间隔）；blocked 打 debug log（E2E 断言用）。
  - 单测 ×6：开阔 true / 中间墙 false / 墙在目标之后 true / origin cell
    豁免 true / 对角墙（DDA 逐格）false + 线外格 true / 重合点 true。

  **② 掉落 0–2 随机 + bone 物品**
  - **schema**（`mob_registry.cpp`，drops 条目两种 count 形式）：
    - `"count": n`（1..64 整数，P28 形式，旧数据不变）→ countMin =
      countMax = n；
    - `"count_min"`/`"count_max"`（各 0..64 整数，**必须成对**，
      min ≤ max）→ 随机区间。混用两种形式 / 只给一边 / 倒挂 / 越界 /
      非整数 = MC_FAIL（7 种坏数据单测覆盖）。
    - 缺省（不给任何 count 字段）= count 1（P28 既有默认）。
  - init 交叉校验：`countMax ≤ item.maxStack`（同 P28 count 的校验位）。
  - **掷落**（`MobEntity::rollDropCount(min,max)`）：死亡 despawn 时按
    drops 列表顺序逐条目掷；`min >= max`（固定形式）返回常量**不推进
    流** → P38 前的精确 RNG 流保留（混合列表里固定条目不扰动随机条目）；
    随机条目 = `min + nextMix(m_rng) % (max - min + 1)`（该 mob 自己的
    splitmix64 种子流，与 wander 决策同流）。n == 0 → 该条目不生成
    ItemEntity。
  - **bone 物品**：`assets/data/items/bone.json`（material、max_stack 64、
    icon bone）+ 图标（generate_item_icons.py 新 salt 63：白色对角骨
    shaft (10,22)–(22,10) w3 + 4 瓣 r²<6，ASCII 目检过，既有 17 图标零
    diff）。
  - `skeleton.json` drops = `[{"bone", count_min 0, count_max 2},
    {"arrow", count 1}]`（按用户规格：bone 随机 0–2、箭保持固定 1——
    Minecraft 还掉 0–2 箭，本项目不跟）。
  - **确定性事实**（单测注释 + E2E 实证）：skeleton 在玩家 detect 半径内
    恒处 RangedAttack/Chase（m_rng 只在 Wander 推进；knockback 窗口抑制
    AI 不推进流）→ 死亡时 m_rng 状态 = 初始种子状态 → **drop 数与死亡
    帧无关、是 f(mob 种子) 的纯函数**（E2E 两次运行同得 bone 2）。
  - 单测：`ParsesDropCountRange`（bone 0–2 + arrow 固定 1）、
    `RejectsBadDropRanges`（×7）、`RollDropCountFollowsTheSeededStream`
    （钉流值：seed 42 首抽 0xbdd732262feb6e95 %3=1、次抽
    0x28efe333b266f103 %5=1）、`RollDropCountFixedRangeDoesNotAdvance
    TheStream`（退化区间后仍取首抽值）、`RollDropCountStaysInRange`
    （200 次 ∈[0,2]）。

  **③ 箭目标预判（lead）**
  - `computeAimDirection(origin, target, targetVel, speed)`
    （`entity/projectile.hpp/cpp` 新自由函数）：`to = target − origin`，
    2 次迭代 `to = (target + targetVel·(dist/speed)) − origin`（飞行时间
    用当前 to 的模长），末 `to.y += 0.5·Projectile::kGravity·t²`（弧线
    提前量），归一化返回。
  - **静止目标（vel = 0）逐位复现 P27 公式**（迭代是无操作）——回归锚：
    站桩玩家被命中的行为与 P27 验证过的逐位相同（单测钉 + E2E 存档 A/B）。
  - 射击回调改用 `computeAimDirection(eye, playerEye, m_player->velocity(),
    shootSpeed)`（原 P27 的"眼高 + 0.5·g·t²"内联公式删除）。
  - 单测（`aimMiss` = 弹道 vs 目标直线 10 Hz × 3 s 全时域最小距离）：
    静止 == P27 逐位；逃离目标（+x 3 m/s）lead miss 1.73 < no-lead 2.09；
    侧向目标（+z 4 m/s）lead miss 0.90 < no-lead 2.83 + dir 获得 +z 分量；
    迎面目标只断几何（dir 更平）——数值上无预判反而略优（目标沿箭的
    飞行线自己撞过来），不占优就不声称占优。

  **测试 713/713（+13）**，`-Wall -Wextra -Wpedantic -Werror` 0 警告。

  **E2E（单批全绿、0 真实失败，P27 场景复用：seed-9 手工存档 p27far
  玩家 (22.5,45,0.5) yaw 0 pitch −0.25 + 夜景 day_length 1200 start
  0.536 + seed 9）**
  - **S1 base18 门（第 13 次结构性免疫）**：P38 ×2 60f 无输入截图
    字节一致 == base18（P36 锚）== P37 代码参考构建（worktree @bca7ec0
    独立 build）。论证：first_spawn_frame=60 → 60f 基线无 mob；bone 图标
    只被 UI 采样（无输入无 UI）；LOS/预判/掉落路径全部需要 skeleton 在场。
  - **S2 射击回归 A/B（核心证据）**：P37 参考 vs P38 同场景 360f：
    **存档逐字节一致** + 截图逐字节一致（本批恰好无红纱边界——上次批差
    ±1 红量子，见 PITS）+ shots 6=6 / hits 6=6 / blocked 0 / health 8 /
    出生点与 P27 钉值全同（f60 cow (22.5,46,−5.5) / f90 sk
    (19.5,46,−4.5) / f120 cow (20.5,46,−4.5) / f150 sk (21.5,45,−4.5)、
    cap 4）——站桩玩家 + 开阔视线 = P38 对 P27 验证场景是 no-op。
  - **S3 LOS 墙场景**：同场景 + 存档手工 96 格石墙（x 18..23、z −3/−2、
    y 45..52——从 S2 探针日志解析出的两 skeleton 位置推导，挡两眼线；
    z 行与玩家 cell（z 0）、spawn cell（z −5）结构不相交）→
    **blocked=6 / shot=0 / skeleton_spawns=2 / health 20**：门抑制射击
    不抑制生成/AI（墙只移除生成候选格，S2 选中的格不受影响 → 出生点
    与 S2 相同）。
  - **S4 掉落输入运行（P38 only）**：kill 存档 = p27far + **yaw
    atan2(1,5) 写进存档**（准星正对 skeleton #2 (21.5,45,−4.5)——P29
    的 grab-delta 免疫：运行中零鼠标移动）+ slot0 铁镐（damage 3 →
    7 挥击杀 20 HP）+ 锁指针 + **左键按住 5 s**（挥击走 isDown，
    `xdotool click 1` 同帧 press+release 永不登记——PITS 新坑位）→
    存档 entities = **arrow×1 + bone×2**（bone 掷落 = f(mob 种子)：
    探针与批两次同值 2）+ health 4（中箭存活）+ 无方块编辑（准星全程
    在 mob/空，铁镐没挖到东西）。首跑 k1 即过（0 重试）。

  **PITS +4**：挥击 isDown vs xdotool click；锁定下绝对 mousemove 变巨型
  相对 delta（+60px warp → yaw 1.75 rad / pitch 夹 −89°，虚拟指针在
  屏幕 (0,0)）；红纱 0.5 s 边界（real-dt 衰减 → 截图 ±1 红量子，存档
  A/B 为权威 + veil 容差比较器）；`--frames` 数模拟子步（40fps 下
  720 子步 = 18 s 墙钟，输入预算按子步时间估游戏内时刻、按墙钟估
  xdotool 序列）。

## 会话存档

- A/B 参考构建 = `git worktree add /tmp/opencode/p38ref bca7ec0`（P37
  代码）独立 build（惯例延续 P36/P37）。
- 单测钉值坑：`rollDropCount` 第二抽的 `%5` 值最初没算就写了 4，实跑
  得 2（实现正确、钉值错）——splitmix64 钉值必须**逐抽逐模数**算全，
  不能只验一个模数。
- 预判单测的 miss 度量第一次用"直线飞行时刻"评估，与 python 预研（用
  lead 距离的飞行时刻）口径不一致导致断言方向反了；改用**全时域最小
  距离**（10 Hz × 3 s 采样）后口径无关、断言稳健。
- 迎面目标（沿箭飞行线冲向射手）数值上 lead 不占优（0.34 vs 0.06）——
  迭代式预判的已知边界，单测只断几何性质不占便宜；文档如实记录。
- S4 首跑三连败全在批脚本不在代码：`shift 8` 对 6 参调用报错（改
  `${(@)@:9}`）、sky 标志（第 8 位置参数）漏进游戏参数（"unknown
  argument: 1"）、kill 存档 `inventory` 是 `{selected, slots[]}` 不是
  列表（KeyError: 0）。真正的行为坑（isDown vs click）由 3 个手动探针
  定位：probe1（焦点/几何正常但杀不成）→ probe2（+60px warp 后
  yaw −1.55 = 锁定成功 + 巨型 delta 实锤）→ probe3（mousedown/mouseup
  按住 5 s → entities = [arrow, bone] 一次过）。
- 批脚本 S2 截图 A/B 第一次真差 ±1 红量子（红纱 0.5 s 边界，100% 像素、
  每通道 ≤1、红占 91.6%）→ 加 veil 容差比较器（P27 veil-removal 先例）；
  最终批该对恰好字节一致（边界两侧随机性）。
- 本阶段结束后 p38ref worktree 待清理（下阶段开工前）。
