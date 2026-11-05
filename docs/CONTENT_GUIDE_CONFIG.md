# 内容添加指南 · 配置 / 存档 / 资产纪律（数据驱动）

> 数据布局与校验策略总览 + 全部「新增一个 X」how-to 节（生物群系 / worldgen 完整参考 / hotbar·物品 / 方块 / 配方 / 附魔 / 染料 / 床 / Mob / 状态效果 / 流体桶 / 熔岩场景 / 可生长作物 / 树种 / 块音效材质）= `docs/CONTENT_GUIDE.md`（主文件）；本文 = 调节玩家参数（config `player.*`）+ 其它游戏设置（config 全键参考）+ 使用存档（format 30 布局）+ 纹理 / 图集 / 字体 / 声音资产纪律（重锚红线）。

## 调节玩家参数（config/config.json `player.*`）

玩家运动/生存模型的全部数值在 `Player::Params`，可在 `config/config.json`
覆盖：**缺键 = 结构体默认值**；**已存在的键必须是有限 JSON 数字**且过
范围检查（非数字/非正 = 启动报错点名，**不会静默回退默认**——player.cpp
严格读取纪律，Config getters 类型失配静默回落的已知缺陷在此绕过）；未知
`player.*` 键容忍（前向兼容：退役键 `fire_damage` /
`fire_damage_interval_seconds`（P74）与 `water_gravity`（FLUID-2，角色翻转
为 `water_buoyancy`）仍带着的旧 config 照常启动）。

| 键 | 默认 | 说明 / 约束 |
|---|---|---|
| `max_health` | 20 | 最大生命；心带 = ceil(max/2) 心；>0 |
| `fall_damage_offset` | 3 | 免坠落伤害格数（MC: 3）；≥0（允许 0 的三键之一） |
| `regen_delay_seconds` | 10 | 脱离受伤后多久开始自然回复；>0 |
| `regen_interval_seconds` | 4 | 每 +1 HP 间隔（回复门 bar ≥ `regen_hunger_threshold`；SURV-9：满血区间免费流逝，回复耗 1 点饥饿）；>0 |
| `max_hunger` | 20 | 最大饥饿（10 图标 × 2）；>0 |
| `hunger_decay_seconds` | 36 | 衰减 tick 间隔（MC 被动节奏 1 点 = 720 tick）；>0 |
| `starve_damage_interval_seconds` | 20 | 饥饿 = 0 时每间隔 1 HP 饥饿伤害（MC normal: 400 tick）；>0 |
| `regen_hunger_threshold` | 6 | 低于此饥饿无自然回复（MC 规则）；0 ≤ v ≤ max_hunger |
| `sprint_hunger_decay_factor` | 2 | 疾跑衰减加速因子（间隔 ÷ 因子）；>0 |
| `lava_damage_interval_seconds` | 1.0 | 熔岩格灼烧节奏（MC 4 HP/s，real-dt 生存计时器；离熔岩重置未到期 tick）；>0（P54） |
| `lava_damage` | 4 | 每次灼烧 HP（MC: 4）；0 < v ≤ max_health（P54） |
| `fire_duration_seconds` | 8 | 点燃的 `ignites_on_lava` 效果时长（MC Normal 8 s；熔岩/火块内每帧重施加 = 持续燃烧，离源后 countdown）；0 < v ≤ 100（P57） |

运动/几何全套键（同样可覆盖，均 >0）：`width`（0.6）、`height`（1.8）、
`eye_height`（1.62，须 < height）、`walk_speed`（4.3）、`sprint_speed`
（5.6）、`jump_speed`（8.5）、`gravity`（32）、`terminal_velocity`（38）、
`swim_speed`（3.1）、`swim_up_speed`（4.5）、`water_buoyancy`（12，
FLUID-2：上浮/下沉加速度）、`water_rise_speed`（1.5，被动上浮帽）、
`water_sink_speed`（1.5，潜行下潜帽）、`water_terminal_velocity`（4）、
`lava_swim_speed`（1.9）、`lava_swim_up_speed`（1.5）、`lava_gravity`（4）、
`lava_terminal_velocity`（2）（P54：熔岩游泳节奏——比水更慢/更沉，比值
对齐 MC 运动系数比 0.014/0.0231）、`mouse_sensitivity`（0.0025 rad/px）、
`crouch_height`（1.6，须 < height）、`crouch_speed_scale`（0.3）。

交叉字段检查（启动报错点名）：eye_height < height、crouch_height <
height、regen_hunger_threshold ≤ max_hunger、lava_damage ≤ max_health、
fire_duration_seconds ≤ 100。已提交 `config/config.json` 的 player 节 =
9 个生存节奏键（值均 = 默认；**用户调值，不还原**）。

## 其它游戏设置（config/config.json 全键参考）

**纪律**：`getBool/getString/getDouble/getInt` 读键——缺键 = 代码默认，
类型失配 = **静默回退默认**（player./save. 两例外 + render.weather /
render.night_sky_level / render.cave_ambient / render.ao 四个严格读键
= 现严格读取点名，见上、存档节与下表）。已提交 `config/config.json`
（**用户调值，不还原**）现含：
window{title/1280×720/fullscreen false}、**ui.scale 1.5**、render{vsync
true, fps_cap 0}、world{**seed 123456789, view_distance 80,
day_length_seconds 600**, start_time_of_day 0.25}、player{9 键}、
audio{volume 0.7}——**render 各特性键 / mobs / fire / beacon / save 均
不写也开 = 代码默认**（fire.particles 先例，config.json 零改动纪律）。

| 键 | 类型 | 代码默认 | 说明（引入 phase） |
|---|---|---|---|
| `window.title` / `window.width` / `window.height` / `window.fullscreen` / `window.resizable` | str/int/int/bool/bool | "TokenMine.cpp"/1280/720/false/true | 窗口（种子 config 带出 title/width/height/fullscreen） |
| `render.vsync` / `render.fps_cap` | bool/int | true/0 | 0 = 不封顶 |
| `render.fog` | bool | **true** | PRE-2 边缘距离雾（流送盘缘溶解，同帧天空色跟随昼夜；**V3-6 填充感知**：end = min(静态, 实际填充盘 cheb 半径 ×16)，稳态满盘位级恒等，base19 重锚；off = 对前版位级恒等） |
| `render.water_ripples` | bool | **true** | PRE-3 水面 shimmer（帧锁定 120 帧周期；lava 不闪） |
| `render.clouds` | bool | **true** | PRE-4 云（y=108 平板层、100 s/格漂移、alpha 0.55；相机云格/t 变化才重建 mesh） |
| `render.alpha_test` | bool | **true** | PRE-5 透明 discard（cutout 段写深度 + alpha<0.5 丢弃；off = 混合段序偏离记档） |
| `render.drop_light` | bool | **true** | PRE-6 掉落光照（掉落物随世界光，每帧采样中心格） |
| `render.mob_flames` | bool | **true** | PRE-8 燃烧 mob 余烬（每 36 帧一批，自发光） |
| `render.furnace_flames` | bool | **true** | CRAFT-5 炉口火焰粒子（燃烧中每 36 帧自炉上格发射） |
| `render.weather` | bool | **true** | V2-4 雨/雷天气（严格读，非 bool 点名拒；off = 状态机永不运行 + 零雨滴 + 零交互 = v2.3.0 管线位级恒等；推进式流首段恒 CLEAR ≥ 9000 帧 = 150 s → 60 帧无输入基线结构免疫根；流种子 = world seed，雷独立 salt） |
| `render.night_sky_level` | int | **7**（kNightSkyLevel） | UX-1 深夜天空地板（严格读：absent = 7 位级 v1.34.0；present = 整数 [0, 15]——0 = 全黑夜（ambient 地板随之归零），15 = 无 retreat；非整/越界/非数点名拒） |
| `render.cave_ambient` | number | **0.03**（kAmbientFloor） | UX-9c 全遮挡 ambient 地板（严格读：absent = 0.03 = pre-UX-9c 管线逐位；present = 有限数 [0, 1]——0 = 洞穴/深水/密封室无残光，1 = 全亮；shader 单消费者） |
| `render.ao` | bool | **true** | V2-18 AO 环境光遮蔽（严格读 weatherEnabled 范式：非 bool 点名拒；**off = 查询不跑 + greedy 合并键 ao[4] 恒零 + 顶点系数恒 1.0 = pre-V2-19 位级恒等**【门内 off ≡ 旧锚实证】；on = 经典 MC 顶点 AO 4 档 1.0/0.75/0.5/0.3（side1∧side2 双邻特例 = 0.3）× opaque ∧ 高度带闭区间，bake 入既有 sky/block 光 attribute（零 shader / 零顶点格式 15 float / 零新 uniform）；生效面 = greedy 满块（MaskCell 合并键扩 ao[4]，共享边角 AO 异断 run）+ slab 三态 + stairs；**fluid/cross/wall/fence/glass_pane 免 AO**（MC 精确：水面不受 smooth lighting 影响 + 薄几何范围外）；per-draw 光覆盖面（信标光束/掉落/XP 球）AO 消失 = 特效光覆盖优先记档偏离；门 190 重锚 base18 ← 224468 B） |
| `audio.volume` | number | 0.7 | 种子 config 带出 |
| `audio.ambience` | bool | **true** | PRE-4 环境音（**4 上下文【V3-2 Part 2：nether 优先** > cave/day/night，inNether 默认参 = 主世界位级恒等；ambience_nether 音】，每 300 帧纯函数 roll，2D 全局） |
| `world.seed` | int64 | getter 回落 0（种子 config 生成值 = 123456789） | 已提交 = 123456789；`--seed` 覆盖并写回 |
| `world.view_distance` | int | 8 | **已提交 = 80（用户调值）**；流送半径 = min(vd, 64)（STRM-1 远平面帽 R_eff） |
| `world.day_length_seconds` | number | 240 | **已提交 = 600（用户调值）** |
| `world.start_time_of_day` | number | 0.25 | `--time` 覆盖（仅新世界） |
| `mobs.enabled` | bool | true | 生成总开关（**下界池同款消费** = 会话策略继承【V3-3】） |
| `mobs.limb_swing` | bool | true | ENT-1 四肢摆动（off = v0.9.0 静态对照） |
| `mobs.difficulty` | string | "normal" | Phase 25/ENT-8：**"easy" / "normal" / "hard" / "peaceful"**（大小写敏感，其他值点名拒）——敌对保护环 8/12/16/**全球(peaceful)**、伤害 ×0.5/1/4÷3/0、生成速率 ÷0.5/1/1.5（check_every_frames 30 → 60/30/20）；peaceful = 敌对自然生成全拒 + 被动豁免（无"现有敌对消亡"= mob 瞬态，记档偏离）；只 init 读取（无运行时切换） |
| `fire.lifetime_seconds` | number | 4.0 | P64 火块寿命（real-dt 倒计时；>0 点名） |
| `fire.spread_interval_frames` | int | 6 | P66 蔓延 roll 节奏（0.1 s MC tick；≥1 点名） |
| `fire.spread_chance` | number | 0.05 | P66 每 5-邻（水平 4 + 上，不向下）独立 roll 概率（[0,1] 点名） |
| `fire.particles` | bool | true | P66 火焰余烬粒子（E2E 对照组） |
| `beacon.range_blocks` | int | 50 | P83 信标效果范围（玩家脚到信标格 Chebyshev；>0 点名） |
| `beacon.beam` | bool | true | P83 半透明光束（E2E 对照） |
| `save.auto_save_seconds` | number | **120.0**（代码默认，SAVE-1） | 自动存档节奏（**真实 dt**，`--save` 槽门控）；**严格读取**：缺 = 默认、present 须有限数 ≥ 0（**0 = 关闭**、负数/非有限/非数字点名拒）；成功归零、失败不清零下帧重试；日志 "World auto-saved to" 区别于 F5/退出 "World saved to" |
| `save.compression` | string | **"gzip"（absent = gzip，V3-7 用户定案）** | 存档编码（**严格读**：`"gzip"` / `"none"` = 明文旧行为位级恒等；非 string/空串/未知词 init 点名拒）——与 format 号正交（魔数 `1f 8b` 探测唯一判据），见「使用存档」 |
| `ui.scale` | number | 1.0 | P34 屏幕空间 UI 缩放，**[1.0, 4.0]** 越界/非数值点名；**已提交 = 1.5（用户调值）**。全部屏幕空间 UI（字体/hotbar/心饥饿带/面板/F3/死亡屏/手持/准星）在 "UI 像素" 空间（窗口逻辑 ÷ scale）正交投影拉伸——世界渲染与截图尺寸不受影响 |

## 使用存档（Phase 13 框架 → format 30）

```bash
./build/bin/tokenmine --save myworld        # 首次：新游戏；此后：读档
./build/bin/tokenmine --save myworld --new  # 推倒重来（退出时覆盖 myworld 槽）
./build/bin/tokenmine --list-saves          # SAVE-3：槽位列表（摘要纯读，确定性名序）
./build/bin/tokenmine --copy-save S D       # SAVE-3：字节保真分支（D 已存在拒；原子写）
./build/bin/tokenmine --rename-save S D     # SAVE-3：POSIX 原子改名
./build/bin/tokenmine --delete-save S       # SAVE-3：删除
```

管理旗标**独占**（每跑至多一个 + 与游戏旗标互斥 = rc 2 点名；管理跑不
启动游戏）。

- 存档槽名只允许字母/数字/`_`/`-`（防路径穿越）；文件在 `data/saves/
  <slot>.json`（git 忽略）。**V3-7 起 on-disk = 确定性 gzip(JSON) 容器
  （默认开）**：JSON 格式化输出、`gunzip` 后可直接阅读；加载按魔数
  `1f 8b` 自动探测编码——**旧明文档（format 28 及更早）零迁移直读**；
  `config` 键 `save.compression: "gzip"|"none"` 严格读（absent = gzip
  代码默认；"none" = 明文 = 旧行为位级恒【同状态文件与 v3.0.0 差异
  = 恰 root.format 单键】；非 string / 未知词 init 点名拒）。gzip 容器
  每字节可复现（固定 10B 头 MTIME=0/无文件名 + raw deflate level 6 +
  CRC32/ISIZE 尾）——同世界存档逐字节可复现（确定性一级纪律）；解
  码侧同 10B 头逐字节全钉（系统 gzip CLI 产物点名拒收）
- **读档语义**：`--time` 只对全新世界生效（存档时间优先）；`--seed` 与
  存档 seed 不一致 = 启动报错（编辑会落到错误地形上）；存档里的玩家
  位置/朝向/速度覆盖出生点与初始视角
- **何时写**：干净退出（ESC 二次/关窗/`--frames` 到点）自动写回；游戏内
  **F5 立即存档**（`--save` 槽激活时）；**SAVE-1 自动存档**（每
  `save.auto_save_seconds` 真实秒，槽门控）；初始化失败或崩溃不写（保护
  旧档）。写失败只记 ERROR 日志、退出码仍 0。**三路共用原子写**（同目录
  `.tmp` + flush/close + `rename` POSIX 原子——kill 中途最多留陈旧 tmp，
  失败写永不毁旧档）
- 存档是机器生成的：字段集由 format 号锁定（**当前 format 30**），加载器
  对未知字段一律报错（与用户手写数据文件的"警告但容忍"策略相反）。
  **25→30 谱系**（一句话）：25 = 教程（V2-3）/ 26 = XP 三向键（player.xp
  + entities 第二 kind xp）/ 27 = 附魔（player.xp_seed + 物品 enchants）/
  28 = 床（player.respawn 恒写）/ **29 = gzip 时代（V3-7：schema 与 28
  完全一致，format 号仅标记写入构建开始默认 gzip 编码；编码与 format
  号正交，魔数探测唯一判据）**/ **30 = 下界维度（V3-1：player.dimension
  严格词表恒写 + nether 段 {blocks, fluids, states} 恒写 + nether_
  entities 恒写 + 派生 seed 不落盘【worldSeed ^ kSaltNether parse 重
  建】+ 两维 portal 载入 rescan + 交叉检；29 缺 dimension = overworld
  迁移 / 29 携 = version lie 点名）**；完整 layout 与逐版证据见
  `docs/archive/v2x/CHANGELOG.md` 各版条目 + `phase-2.x.0.md` 档（29 →
  `docs/archive/phase-3.1.0.md`，30 → `docs/archive/phase-3.4.0.md`），
  不在此重抄全文
- **PITS**：`--save slot` 第二次运行会加载第一次写的存档（E2E 对跑每拍
  从源副本 `cp` 新鲜存档）

### format 30 完整布局（on-disk = gzip 容器，JSON 内容 = 明文 payload）

```json
{
  "format": 30,
  "seed": 123456789,
  "time": 0.2541666,
  "player": {
    "pos": [22.5, 53.0, 0.5],
    "velocity": [0, 0, 0],
    "yaw": 0.0, "pitch": -0.25,
    "health": 20.0, "absorption": 0.0,
    "hunger": 20.0, "saturation": 6.0,
    "effects": [ { "name": "speed", "remaining": 12.5, "tier": 1 } ],
    "stats": { "deathCount": 1, "kill.chicken": 3 },
    "armor": { "head": "iron_helmet", "chest": { "item": "leather_chestplate", "damage": 12.5 } },
    "oxygen": 150,
    "xp": 1395,
    "xp_seed": "5eed1a2b3c4d5e6f",
    "respawn": [0, 54, -3],
    "dimension": "overworld"
  },
  "nether": { "blocks": [], "fluids": [], "states": [] },
  "nether_entities": [],
  "inventory": { "selected": 0,
                 "slots": [ { "item": "stone", "count": 64 },
                            { "item": "iron_axe", "count": 1, "damage": 2.0,
                              "enchants": { "efficiency": 1 } },
                            { "item": "", "count": 0 } ] },
  "inv_grid": [ { "item": "", "count": 0 } ],
  "furnaces": [ { "pos": [0, 53, 0], "input": { "item": "", "count": 0 },
                 "fuel": { "item": "", "count": 0 }, "output": { "item": "", "count": 0 },
                 "progress_substeps": 0, "fuel_substeps": 0 } ],
  "fires": [ { "pos": [0, 53, 0], "remaining": 3.2 } ],
  "campfires": [ { "pos": [0, 53, 0], "slots": [ { "item": "", "count": 0, "progress_substeps": 0 } ] } ],
  "states": [ { "pos": [0, 54, 0], "state": 2 } ],
  "tables": [ { "pos": [0, 53, 0], "slots": [ { "item": "", "count": 0 } ] } ],
  "entities": [ { "type": "item", "item": "dirt", "count": 1, "damage": 2.0,
                  "pos": [0.5, 52.5, 0.5], "age": 12.5, "pickup_delay": 0.25 },
                { "type": "xp", "xp": 5, "pos": [1.5, 52.5, 0.5],
                  "age": 12.5, "pickup_delay": 0.25 } ],
  "blocks": [ [0, 53, 0, "air"] ],
  "fluids": [ [4, 50, 4, 5, "water"] ]
}
```

（上方 campfires/inv_grid/tables 数组为示意——实际各恰 4 / 9 / 9 个槽
条目。顶层各段**恒写**（空 = 空数组/空对象，含 `tables`/`states`/`fires`/
`campfires`/`furnaces`/`inv_grid`——新格式必键纪律）；player 的
health/hunger/saturation/absorption/oxygen/xp/xp_seed/respawn = 游戏恒有
值故实际恒写（−1"未指定" = 省略字段，仅老 format 读入语义）。）

**player 段**（键纪律：format 30 必键 = pos/**velocity**/yaw/pitch/health/
absorption/hunger/saturation/effects/armor/**stats**/**oxygen**/**xp**
/**xp_seed**/**respawn**/**dimension**；低版本带新键 = 版本谎报点名）：

| 键 | 引入 | 纪律 |
|---|---|---|
| `pos` / `yaw` / `pitch` | 1 | AABB 底面中心；[0, 128) 世界 y |
| `velocity` | **22（SAVE-2）** | 3 数组**恒写**（静止 = [0,0,0]——ENT-7 停止 epsilon 使静止恰 0）；分量区间 ±100（`kSavedVelocityMax`，越界点名）；≤21 读入 = 零速 legacy（值保真免迁）；onGround 不存（位置派生，记档偏离） |
| `health` | 2+ | base 池（≤ base max；absorption 分池后不含层）；format 1 = 缺省满 |
| `absorption` | 12 | [0, 64]，**恒写**（无层 = 0）；须 ≤ 存档效果 absorb_hp 总和（不变量点名）；≤11 读入 = 共享 health 迁移（超 base 部分入池） |
| `hunger` | 4+ | format 1-3 读入 = 满 |
| `saturation` | 5+ | [0, max_hunger]；format 1-4 读入 = 0 |
| `effects` | 8+ | **必键**（空数组合法）；名序；`{name, remaining, tier?}`——remaining > 0 秒（real-dt 倒计时；**tick 相位不存** = 读档重启满 interval）；**tier 0 基，仅 >0 时写**（omit-when-default，20+；范围 = 效果 tier_max，点名） |
| `stats` | 21 | **必键**（空对象合法）；词表**封闭**：`deathCount` + 每注册 mob 一条 `kill.<name>`（非零才存，名序）；未知 `kill.<name>` = game 加载点 validateStats 点名（parseSave 不加 MobRegistry 参 = 184 调用点涟漪，rescan 范式孪生） |
| `armor` | 16 | **必键**（空对象 = 无甲）；**只含占位槽**、固定槽序 head/chest/legs/feet；值 = 物品名字符串（满耐久保字符串形 = fmt17 逐字节同形）**或** `{item, damage}` 对象（damage > 0，18+；可携 enchants 27+ 同款）；解析期对 ItemRegistry 校验（物品存在 + 其 armor_slot 匹配键名） |
| `oxygen` | 17 | **必键**，整数 [0, 150]（kOxygenMax）；drownTicks 不存（读档重启 0——MC 同款记档偏离） |
| `xp` | 26（V2-11） | **必键**，整数 ≥ 0 **恒写**（累计经验池；HUD 条/等级 = 池派生，**等级永不存储** = 单源真值，core/xp_curve 全整数 MC 现代公式）；pre-26 载 = 迁 0（velocity 零化迁移同款）；≤25 文件携 = 版本谎报点名 |
| `xp_seed` | 27（V2-12） | **必键**，**16 位小写 hex 串恒写**（附魔 offer 去相关种子；存档 JSON number = double 53 位容不下 64-bit = hex 串记档偏离）；26 载 = worldSeed ^ kXpSeedInit 迁移；pre-27 文件携 = 版本谎报点名 |
| `respawn` | 28（V2-13） | **必键**，int 三元组 **恒写**（重生点 = 记忆位置的格分量 floor；新档 = 世界出生点 init 写入）；分量非整数 / y ∉ [0,128) 点名拒（x/z 无界）；27 载 = 哨兵 -1 → `applySave` 覆写世界出生点（parse 无法知世界出生点 = game 侧迁移）；pre-28 文件携 = 版本谎报点名 |
| `dimension` | **30（V3-1）** | **必键**，严格词表 overworld\|nether **恒写**（当前维度；下界死亡重生回主世界 = 维度换绑既有语义）；29 载 = overworld 迁移（免迁）；29 携 = version lie 点名；未知词点名 |

**nether 段（30+）**：`nether = {blocks, fluids, states}` 三子段恒写（空
合法）= 下界维度的 journal 同款结构（blocks 位置序 / fluids 五元组 /
states 全量含默认值）；`nether_entities`（item kind 掉落物）恒写空合法；
**mob 与箭不存**（两维同款瞬态纪律）；两维 portal 载入 rescan（孤立 →
air，双向交叉检）；nether 内 stateful 块须 nether.states 条目（PITS 383①
孪生）。派生 seed 不落盘 = parse 重建（单源真值）。gzip 编码零改动
（V3-7 容器自动覆盖两维）。

**世界段**：`inventory`（27 格 `slots`：{item, count, **damage? >0**,
**enchants? 27+（omit-when-empty，{id: level} 对象，载时交叉校验：未知
id / level > max / 类不兼容 / incompatible 对全点名**）} 严格白名单——
grid 槽不携 damage = 18 范围记档偏离；`selected` 0..8）+
`inv_grid`（E 面板 2×2 的 9 格，恒 9 条目）+ `furnaces`（7+，**每炉子块
一条目**位置序，空态含；双向交叉校验 = 块编辑 ⇔ 条目）+ `fires`（10+，
每火块一条目，remaining > 0 = **real-dt** 燃尽倒计时）+ `campfires`（13+，
每营火**家族**块（点燃 + 未点燃）一条目，**恰 4 槽**，每槽进度不变量
progress > 0 ⇔ 槽恰 1 可熔炼物品）+ `states`（15+，每个 stateful 块恰一条
{pos, state}，state 对块 stateCount() 范围校验 + 双向交叉）+ `tables`
（19+，每工作台块一条目 {pos, slots[9]}，槽 format-3 纪律【槽带 damage
拒 = SURV-3 grid 范围】）+ `entities`（`type` ∈ {item, xp} 两种：item
条目 item/count [1,64]/pos/age/pickup_delay/**damage? >0**/**enchants?
27+（omit-when-empty）**；**xp 条目（26+）= 严格形 {type, xp 整数 1..9,
pos, age, pickup_delay}**（球不携堆叠，无 item/count/damage；item 条目
携 xp 字段 = 版本谎报点名）；**mob 与箭 = 瞬态实体不存**（读档后不
出现，不是错误）+ `blocks`（位置序 [x,y,z,块名]，"air" = 移除）
+ `fluids`（位置序 [x,y,z,level,种类名]，种类 = water/lava/honey；level 0 =
抽干）。

**键纪律三向**（与用户数据文件相反）：① 新版本**必键**（effects/fires/
campfires/armor/oxygen/stats/tables/states/velocity/**xp/xp_seed/respawn**
等，缺 = 拒）；② 低版本**带新键 = 版本谎报点名**（如 fmt14 带 states、
pre-21 带 stats、pre-22 带 velocity、pre-26 带 player.xp 或 xp 实体、
pre-27 带 xp_seed 或堆栈 enchants、pre-28 带 respawn、**pre-30 带
dimension/nether/nether_entities**）；③ 未知字段一律报错。**迁移先例**：值保真（14 stairs
→ 15 states facing→state）/ 免迁（tier 0 omit-when-default ⇒ f19 文件 f20
直接读）/ sole 归属（f18 table_grid 非空 ∧ 恰 1 表 → 归属，0/多 → 丢弃，
P44 furnace 先例）/ 系统不存在 = 默认（f11 无吸收 → 共享 health 拆分、
f16 无甲 → 空、f17 无氧 → 满、f20 无 stats → 空、pre-22 无 velocity → 0、
pre-26 无 xp → 0 且无 xp 实体、pre-27 无 xp_seed → worldSeed ^
kXpSeedInit、pre-28 无 respawn → 哨兵覆写世界出生点、**pre-30 无
dimension → overworld + nether 段空**）。

## 纹理 / 图集 / 字体 / 声音资产纪律（重锚红线）

**块图集**（单纹理多 tile）：tile 源 = `assets/textures/blocks/*.png`
（块，**130 件**，3.x 增 nether portal/netherrack/下界内容 12/建筑 20 族
等）+ `assets/textures/mobs/*.png`（mob/玩家，**35 件**，V3-3 下界 mob
9 件：per-face 脸 4 + 身体 4 + magma blob 1），均
16×16 RGBA；
布局 = `assets/data/textures/block_atlas.json` `{columns, rows, tiles:
{name: [col, row]}}`——**显式格子，非计算排布**。解析纪律（纯函数，
init 双向交叉）：columns/rows 必须**2 的幂**（UV 角 = 精确二进分数，浮点
管线永不跨界采样）；tile 格须在网格内；**两 tile 不得共格**；PNG 无布局
条目 / 条目无 PNG = 数据错误点名。

- **shipped = 16 × 16 = 256 格，已用 165，空 91**（3.x 增长谱系：
  122【v2.19.0】→ 136【V3-2】→ 145【V3-3】→ 165【V3-5】；全部既有格
  坐标 JSON 逐字不变，新增 = 尾部空格）。
- **空位添加 = 无既有 UV 移位 = base19 免重锚**（绝大多数内容相走的路；
  3.x 三次空位增长全实证）。
- **网格扩容 = u = col/列数 全部既有 UV 移位 = 无输入基线门必须重锚**
  （P41 4×8→8×8、P59 8×8→16×8 先例）——**例外先例 = 幂二增长零移位
  【V3-2：16×8→16×16，行数 ×2 时 u 不变 = 既有像素行恒等，门 195 S1
  逐字节实证免重锚】**（列数变 = 移位必须重锚；行数幂二 = 免）。

**物品图标**：每物品独立 PNG（`assets/textures/items/`，32×32 RGBA，**无
atlas**；可多物品共享同一文件——block item 不用 2D 图标，以 3D 模型/
立方呈现）。物品 `model.texture` 惯例 = 复用 icon 文件（CRAFT-2 零新纹理
免重锚）。

**effects.png**：16 px/tile 条带（当前 176×16 = 11 tile）；新效果 =
**尾部 append**（既有 tile 逐像素零变化验证——SURV-4/SURV-5 先例）。

**font.png**（256×2112，32 px 格 = 8 列 × 66 行）：rows 4..15 = ASCII
32..126；rows 0..3 = **32 个 ASCII 控制字符死格复用** = PRE-1 的 32 个
CJK 字形（18 UI 字 + 10 中文标点 + 4 余量）；cell 127（ASCII DEL 死格）
= CRAFT-4 补的「作」（U+4F5C——PRE-1"18 UI 字"实为 19 字误计，工作台
标题 v0.25.0 起渲染"工?台"）= legacy **CJK 33 字形**。**UX-6 扩展 +
V2-12 附魔 4 字（锋利限魔，魔 re-cell 鸡麦黄默 +1）= 现
`kCjkGlyphs[427]`**（码点序）：legacy 33 保 PRE-1/CRAFT-4 格 0..31/127
位级不变；394 扩展字形占 cells 128..521 = rows 16..65（row 65 = 部分
余量，cells 522..527）；未知 CJK 落 `?` 格。**加 CJK 字形 = 重新生成
脚本**：glyph 静态表（font.hpp / generate_font.py 双份）加条目 +
font.png 重跑——**顶 512 px（rows 0..15 = ASCII + legacy CJK 33 格）
逐字节不变红线**（测试钉），新字形追加尾部 cell；rows 16..65 余量耗尽
= 字体布局再扩展（按重锚纪律处理）。ASCII 区位级不变红线 + 既有 CJK
格逐位复现自校验 + FNV-1a 64 字节钉在测试里；TTF 仅工具期输入，游戏
构建不依赖。

**声音**：`assets/sounds/` **63 件** 16-bit mono 44.1 kHz WAV
（SoundBank 载入时转 stereo 居中，CRAFT-3；2.x 增 thunder【V2-4】/
sleep【V2-13】/ squeak【V2-14】/ shear【V2-15】/ snow×3【V2-16】；
3.x 增 portal×3【V3-1】/ 下界族 11 = nether·nylium·stem 9 + evaporate +
ambience_nether【V3-2 Part 2】/ 下界 mob 氛围 3 = piglin_whisper/
hoglin_grunt/strider_hiss【V3-3】）。
`tools/generate_sounds.py` 确定性生成（固定
种子 LCG/sine；**重跑后既有文件字节零变化 = md5 实证 + 幂等**）；材质 3
件见 CONTENT_GUIDE.md「块音效材质」节。

**确定性生成工具总则**（全部 `tools/*.py`，无运行时依赖，纯标准库写 PNG）：
generate_block_textures.py（块 16×16，per-pixel hash(x, y, salt)）/
generate_mob_textures.py（mob 16×16，splitmix64）/ generate_item_icons.py
（物品 32×32，splitmix64，DYE_PALETTE 等）/ generate_effect_icons.py
（条带）/ generate_font.py（字体表）/ generate_sounds.py（wav）。加纹理 =
**新 salt**（既有纹理零变化）+ 重跑须 md5 验证幂等。

---

*（完）——各 phase 细节存档：v1.x 见 `docs/archive/v1x/phase-*.md`、
v2.x 见 `docs/archive/v2x/phase-2.x.0.md`；活跃已知问题与记档偏离见
`docs/KNOWN_ISSUES.md`；玩法语义见 `docs/GAMEPLAY.md` /
`docs/MECHANICS.md`。*
