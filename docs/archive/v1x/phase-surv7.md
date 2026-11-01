# SURV-7 死亡统计（v1.x 18/33，v1.18.0，门 133，SURV 行 2/5）

v1.x 任务矩阵第十八项。原始定义已佚 = 代理定案最小完整版：**统计系统
v1 = 死亡相关计数器（deathCount + 每 mob kill.<name>），帧锁定事件
计数 + format 21 存档 + 封闭词表严格校验**。与 PRE-12 边界：SURV-7 =
统计系统本体（计数/存档/校验，行为数据）；PRE-12 = 呈现框架（F3
统计行/tooltip/统计菜单 = 后续行）。**本相零 UI**（HUD/死亡屏/F3 零
触碰——计数只可经存档观测 = 协议权威面）。

## 定案

- **词表封闭**：`deathCount`（玩家 alive→dead 过渡计数，固定名）+
  每注册 mob 一个 `kill.<mob name>`（**mob 注册表合成** = block item
  合成先例：加一个 mob JSON 自动获得 kill 统计，零核心改动）。无新数
  据文件（核心事件名引用 = 声音名同款范式）。
- **计数事件（帧锁定）**：① 玩家死亡过渡（`dead && prevHealth > 0`
  块，一次死亡 = 一次计数，任意死因）② mob despawn pass 的**每个死
  mob**（含无掉落 mob——死亡是事件不是掉落；距离 despawn 不算死——
  `dead()` 标志门）。
- **容器**：`Game::m_stats = std::map<string, uint64>`（位置序 = 确定
  性序列化；**只存非零** = omit-when-default）。
- **format 20 → 21 `player.stats`**：恒写对象（空对象合法 = furnaces/
  tables 键纪律）；entries = 名 + 整 ≥ 0（非整数/负数点名）；**pre-21
  文件带 stats 键 = 版本谎报**（格式依赖白名单自动点名）；pre-21 读
  入 = 空 stats（系统不存在 = 免迁）；**词表校验 = game 加载点
  `validateStats`**（parseSave 不加 MobRegistry 参 = 184 调用点涟漪，
  改单收口点名，rescan 范式孪生）。
- **顺带修复 SURV-6 漏网 bug**：game 存盘填充路径
  `SavedEffect{name, remaining}` 只传 2 值 = **活体 tier-1 效果存盘
  丢档降 0**（门 132 同侧 savenorm 未暴露）→ 补 `a.tier`；门 133 S2c
  终档断言 tier 1 往返 = 回归证。
- 零内容/零纹理/零音效；统计不入帧渲染 = 截图结构免疫。

## 实现

`src/core/save.{hpp,cpp}`（kSaveFormat 21 + SavedGame.playerStats + 序
列化/解析 + validateStats 自由函数 + format 历史注释）、
`src/core/game.{hpp,cpp}`（m_stats + 死亡过渡计数 + despawn pass 计数
+ 读档 validate/apply + 存盘填充 + **SURV-6 tier 填充修复**）。

## 测试（1827/1827，+7 净，-Werror 0 警告）

- test_save +5（SaveFormat21Test）：stats 往返 / 空对象合法 / format-21
  缺 stats 键点名 / pre-21 带 stats = 版本谎报 / 形状拒绝（负/分数/
  字符串三形）。
- test_save +2（ValidateStatsTest）：封闭词表接受（deathCount + 2 注册
  mob）/ 未知点名（kill.dragon 点名 dragon / 非 kill 形 unknown stat /
  空 mob 名）。
- 38 处旧 format 迁移/降级测试契约演进（序列化后降级 format 须剥
  stats 键 = 版本谎报；含 2 个 downgrade helper 内建剥离 + 20 处直插
  + 行尾注释/`(*j)` 写法漏网 3 处二轮补——**教训 = 机械批量后须全量
  套件验证，grep 正则对行尾注释与 `(*j)` 解引用形不鲁棒**）。

## E2E 门 133

- **S1 = 第 133 次结构性免疫免重锚**：fresh 60f 四方 ×2（new v1.18.0 /
  old v1.17.0 = 866bb05，g118_nomobs）+ base18 + savenorm（无死无
  mob = 零计数 = stats {} + 零 UI 触碰）。
- **S2a 昼步道 kills**（s133_walkway v3 = s129 平台改造：**真生成环
  5-6**（spawner 实际 min5 max6——ENT-8 文档"环 8-16"为误记，本轮
  SPAWNPROBE 实证 ring=6）的 band 石地板 + 5×5 玩家台 + 台间/带外
  全 22 深洞 y22-43，g130_normal 2400f）：被动 mob 生成于 band、游
  走掉洞 → **stats = kill.chicken ×2 + kill.sheep ×2（冒烟实证）**+
  deathCount 缺（玩家 hp 20 安全）；×2 RAW + savenorm EXACT（计数确
  定）。
- **S2b 夜 + resistance tier 1**（s133_night_t1 = s130_night + 注入
  {resistance,120,tier 1}，2400f）：**hp 0 + deathCount == 1 exact** +
  effects []（死亡清）×2 RAW + savenorm。
- **S2c 密封房 tier 往返**（s133_room_t1 = s131 房 + 注入 tier 1，
  nomobs 600f）：终档 effects[0] = {resistance, ~110, **tier 1**} =
  **SURV-6 填充 bug 回归证**（修复前此断言必败）×2 RAW + savenorm
  （剥 remaining 留 tier）。
- **S2d 旧侧**：v1.17.0 二进制 + 步道 → format 20 + **无 stats 键**
  （系统不存在）。
- **S6 ×3 点名**：kill.dragon（"no mob named 'dragon'"）/ deathCount
  −1（"whole number >= 0"）/ format-21 缺 stats 键（"requires a
  'player.stats' object"）。
- **S3 = 套件 1827**。

## 性能 A/B（发布前必做）

tm_v117_perf（banner 实证 v1.17.0）vs tm_v118_perf（**全新 rsync 树
（不继承 build/ = PITS 306 纪律）+ 插桩 fluid.cpp 移植 + step0 patch
+ hunk#6 手工 + 全新 cmake 配置**；banner 实证 v1.18.0），同 seed
fall 900f：**PASS = 成本可忽略（结构性惰性实证）**——fluidCells
win1 51929/51925（±4 fill 窗 real-dt 类）+ **win2 29803 = 29803 逐
位 + win3 29800 = 29800 逐位**（本对无相位翻转）+ PIPE3
fluidCellsSeen 43994 逐位 / changesApplied 1404 逐位 / visited 268
逐位 + upd/render/swap 全同档（v118 win3 fluid 段 +13% = fire spread
相位类同档）。零死亡零 kill 的 fall 场景下计数器零执行 = 结构惰
性；计数本身 = map 操作（微秒级）。

## 坑（门内）

- **生成环误记**：ENT-8 文档"spawner 环 8-16"实为 **5-6**
  （tm_scene_check.py 注释早有 min5 max6；SPAWNPROBE 实证 ring=6）→
  首版 8-16 annulus 场景零生成两连；v3 按真环重做。
- **`grep -c` 0 匹配 RC=1 断 && 链**：v3 场景脚本静默未跑（PITS 289
  同款"假象"家族）——批处理里 grep -c 后须 `|| true` 或不用 && 链。
- **手造 format-21 场景缺 stats 键 = init 死伪装**（PITS 289 第 N 次
  复现：init 失败后旧场景文件被回读 = "hp 20 remaining 120.0" 假象）
  ——场景 builder 落盘后须真实跑一次验 clean exit。
- 性能树 banner 验版本 = PITS 306 纪律落地（本轮双 banner 实证）。

## 发布

v1.18.0 = 新组 1.16-1.20 第 3 版（组内不打 tag；组末 v1.20.0 → tag +
GitHub release）。正确性 A/B 旧侧 = v1.17.0 = 866bb05（tm_v117_src）。
