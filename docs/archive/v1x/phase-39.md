# Phase 39 细节存档

> 用户确认方向（原话）："ok,继续按你的建议进行 P38 。完成后继续 P39."
> （P38 已完整收尾 = `d3ebe1a`；"按你的建议" = 剩余候选首位：P25 遗留
> 「生成权重按类型」。）

## 阶段报告

- **`[2026-10-11, Phase 39]` 生成权重按类型（P25 遗留 KNOWN_ISSUES 修复）** —
  `mob_spawning.json` 的 `mobs` 从字符串列表改为 **`{"id", "weight"?}` 对象
  条目**：weight = 相对生成概率（1..10000 整数，缺省 1）。类型抽取保持
  **每次 1 次流值**：`splitmix64(seed ⊕ G·frame)` 映射进 [0, Σweight) 后
  按累计权重走名单序。**全 weight 1 与旧 `pick % 名单长度` 逐位相同** →
  已发行数据（全 1）零行为变化。

  **schema 与解析（`entity/mob_spawning.hpp/cpp`）**
  - 新 `SpawnMobEntry {std::string id; int weight = 1;}`；
    `MobSpawningConfig::mobs` = `std::vector<SpawnMobEntry>`。
  - 条目校验（严格）：必须是对象——**旧纯字符串形式显式拒绝**（迁移防护，
    Phase 28 drops 先例，报错文案指明新格式）；`id` 必填非空字符串；
    `weight` 可选，出现时须整数且 ∈ [1, 10000]；重复 id = MC_FAIL；
    条目内未知字段警告忽略（house style）。
  - 新纯函数：`totalWeight()`（Σweight）与 `pickIndex(v)`（v < totalWeight，
    累计权重走名单序；全 1 时返回 v 本身 = 旧 mod 结果）。
  - game.cpp 两处调用点：init 交叉校验（`entry.id` 对 MobRegistry）与
    抽取（`pickIndex(pick % totalWeight())`）；抽取注释更新（"ONE stream
    value per attempt"）。

  **测试（+5，718/718，0 警告）**
  - `ParsesWeightedMobList`：显式 weight + 缺省 1 + totalWeight。
  - `RejectsBadMobListEntries`：9 种坏条目（字符串形式/非对象/缺 id/空
    id/weight 0/−1/1.5/10001/重复 id）。
  - `WeightedPickUniformMatchesLegacyMod`：全 1 → v∈[0,5) 恒等 + 2 个大值
    （先 mod 再 pickIndex，与调用点同序）。
  - `WeightedPickCoversRangesDeterministically`：[1,1,1,1,10]（total 14）
    区间覆盖钉值（v=4 起全落 skeleton、v=13 仍 skeleton）。
  - `WeightedPickSingleEntry`：单条目任意 v → 0。
  - 既有 4 个 `MobSpawningConfigTest` 迁移到新 schema（ParsesValid 顺带
    断言缺省 weight 1）。

  **E2E（单批全绿，`/tmp/opencode/p39_e2e.zsh`，P27 场景复用：p27far seed 9
  玩家 (22.5,45,0.5) + 夜景 day1200 start 0.536）**
  - **S1 base18 门**：P39 ×2 + P38 参考构建，60f 无输入截图三方字节一致 =
    base18（**第 14 次结构性免疫**——first_spawn_frame=60，60f 基线无
    mob；schema/抽取改动只在生成尝试路径内运行）。
  - **S2 uniform 回归 A/B（核心证据）**：P38（worktree @d3ebe1a 独立
    build）vs P39 同场景 360f：**存档逐字节一致** + shots 6=6 / hits 6=6 /
    health 8 + 出生序列文本相同（f60 cow (22.5,46,−5.5) / f90 sk
    (19.5,46,−4.5) / f120 cow (20.5,46,−4.5) / f150 sk (21.5,45,−4.5)，
    cap 4 后 7 次 cap-skip）——全 weight 1 对 P27 验证场景是 no-op。
    截图 = 100% 像素 ±1 红量子 real-dt 红纱边界（通道和
    [845410, 27, 76190]，红 91.6% ≥ 90%）——存档 A/B 为权威。
  - **S3 加权场景**（scratch cwd `/tmp/opencode/p39w`：assets 副本 +
    skeleton weight 10 / total 14，游戏从该目录启动 → 加载副本数据）：
    出生序列**与 python 预测逐条精确一致**——raw 流
    `splitmix64(9 ⊕ G·f)` 的 v14 = 1,2,6,1 → f60 cow / f90 **sheep** /
    f120 skeleton / f150 cow（sheep 出现、第二个 skeleton 槽被替换）+
    存档 ×2 字节一致（确定性）+ health 14（唯一 skeleton 中 3 箭 =
    6 伤害）+ 组成 ≠ S2 uniform 场景。

  **PITS +2**
  - A/B 参考构建与数据 schema 迁移：参考侧必须从**自己的 worktree cwd**
    运行（仓库 assets 已是新 schema，旧解析器迁移防护直接拒绝启动——
    防护本身工作正常；首跑因此 FATAL，表象 "game window not found"）。
  - 游戏启动辅助函数里 `cd <gdir>` 使相对二进制路径失效（launch() 的
    `(cd $gdir && exec "$bin")` 下 `./build/bin/minecraft` 指向 scratch
    目录 → "没有那个文件或目录"；批脚本二进制恒绝对路径）。

  **变更文件**：`src/entity/mob_spawning.{hpp,cpp}`（SpawnMobEntry +
  pickIndex/totalWeight + 新解析）、`src/core/game.cpp`（两处调用点 +
  注释）、`assets/data/mob_spawning.json`（对象形式、全 weight 1）、
  `tests/test_mob.cpp`（4 迁移 + 5 新增）、文档全套（ROADMAP /
  KNOWN_ISSUES P25 条划线 / MECHANICS 生成段 / CONTENT_GUIDE 名单段 /
  PITS +2 / AGENTS / 本存档）。

## 会话存档

- **方向**：P38 报告交付后用户说"ok,继续按你的建议进行 P38。完成后继续
  P39."——P38 已完整收尾（`d3ebe1a`），解读为"按报告建议进入下一阶段"；
  剩余候选 3/4/5/6 中取首位（P25 遗留生成权重）= P39。范围最小、纯数据
  + 一个纯函数、确定性论证最强（全 1 逐位 no-op）。
- **逐位 no-op 的构造**：`pickIndex(v)` 在全 weight 1 时对 v∈[0,n) 返回
  v 本身，而调用点 `pick % totalWeight()` 在 total == size 时与旧
  `pick % size` 同一表达式 → 不仅"同分布"而是**逐位相同**，因此 base18
  门 + P27 夜景 A/B 都能用字节断言（而不是统计断言）。
- **S3 预测流程（钉值惯例）**：加权场景不靠"跑出来看看"，先用 python
  按 `splitmix64(seed ⊕ G·frame)`（noise.hpp 纯 finalizer 原文）逐帧算
  raw → v14 → 累计走名单，得期望序列 cow/sheep/skeleton/cow；模型先拿
  S2 的 uniform 钉值（cow/sk/cow/sk）自校验通过后才用于断言。夜景下
  所有类型过光门 + cap 4 → 4 次尝试全出生，组成 = 抽取序列本身。
- **scratch-cwd 加权场景**：`mob_spawning.json` 走 assets 路径（CWD
  相对），改已提交数据文件做实验不可接受 → 临时目录复制 assets + 只改
  该文件，游戏 `cd` 过去启动（save 也落临时目录）。launch/shot 辅助
  函数加 `<gdir>` 首参，`BIN` 改绝对路径（PITS 新坑）。
- **批脚本事故两起（均重试/修复后 0 真实失败）**：① S3 首跑 rc=139
  （启动稳定化窗口内 WM 平铺瞬态 origin (640,440)，P36 既有 WM 竞态
  类），重试 1 次即过且与 s3w_a 存档逐字节一致；② 前两起 FATAL 都是
  脚本自身问题（参考侧 cwd + 相对 BIN），见 PITS +2。
- **测试脚本小坑（未入 PITS，一次性）**：C++ 测试里
  `std::string(R"({...)"` + `+ variant + "\n    })"` 的拼接，raw string
  收尾 `)"` 之后紧跟的 `)` 已闭合 `std::string(`，后缀字符串里再带一个
  `)` 就进了字符串内容（"trailing characters at line 9 column 6"）；
  且 python 生成 C++ 源码时 `"\n"` 被写成真换行（字符串字面量跨行）。
  两坑同修后一次过。
