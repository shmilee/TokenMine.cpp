# AGENTS.md — 代理快速接收

> Phase 30 文档整理产物。本文件是代理接手本项目的**第一入口**：读完本页即可开工，
> 细节按「文档地图」按需加载。所有规则原文见 `docs/PROTOCOL.md`。

## 项目一句话

**TokenMine.cpp**——本地 LLM 编写的 C++ 版 Minecraft：可运行、可扩展的 3D 体素沙盒，
C++20 / SDL3 / OpenGL 3.3 core，
自研 `Result`/JSON/PNG，**内容全数据驱动**（加方块/物品/配方/Mob = JSON + 纹理，
不改核心代码），**确定性是一级要求**（无输入运行逐帧字节可复现）。

## 现状（Phase 78 完成时）

- 功能阶段 P0–P78 全部完成并提交；P78 = 营火 v4：四槽图标全渲染（用户"按你的建议，继续下两个 phase" = 建议 2，代理定案最小完整版 = **每个非空槽的图标都渲染**，全量消费 P75 遗留"只渲染第一个非空槽图标"偏离）：**0.35 图标 × 0.3 间距 1×4 横排**悬浮格上半部（槽 i 中心 = 格中心 + (i−1.5)×0.3——P71 的 0.5 单图标 4 槽排不下，缩小为行布局 = MC 展示全部烤位）；**纯渲染层**（无状态/存档/交互变更；无营火 = 零绘制 = 基线免疫保持）；1109/1109（0 新——渲染由 E2E 钉死）、`-Werror` 0 警告、base18 **第 51 次结构性免疫**【渲染只在非空槽发生 + fresh 无营火】；E2E 全绿（S1 第 51 次过门 / S2 四图标行【P71 瞄准几何 + 4 不同物品（beef/chicken/mutton/cooked_beef），600f no-input：3 生肉恰在帧 600 烤熟（10 s 熔炼 = 600 子步，帧精确）→ 终态 4 熟品 progress 0，×2 RAW 存档+截图 + EXACT】/ S2ctl 单图标对照【同场景仅槽 0（beef→cooked_beef），×2 RAW + EXACT】/ **像素判别**：S2 vs S2ctl 截图 diff = **1435 px**（∈(800,4000) = 恰 3 个额外图标——P75 单图标渲染 ≈0 互斥；世界 + 共享槽 0 图标两场景逐像素相同 = diff 隔离）；**PITS +1**【截图比对跑 120f 不够 chunk 流送落定——首轮 ×2 截图不一致，600f 解决（P64 60f 教训的截图版）】）；P77 = 弓 v3：无限弓（用户"按你的建议，继续下两个 phase" = 建议 1，代理定案最小完整版 = **Infinity 弓 = MC 弓附魔的数据驱动替身**，全量消费 P42 遗留"无箭矢强化"的 Infinity 部分）：**物品字段 `infinite`**（严格 bool、缺省 false、**要求 `ammo` 非空**——无 ammo 发射器本就无限，flag = no-op 拒绝【ammo 纪律】）+ **内容 `infinite_bow`**（bow 同款数据 + `infinite: true`；shapeless 配方 **bow + diamond → 1**；图标 = 弓 + 蓝色钻石微光【新 salt 91/92、既有图标零变化 = 确定性再生】）；**发射语义**（`fireBow`）：`infinite` 发射器仍须持有**至少 1 个** ammo 才能射（1 支箭永不耗尽 = MC Infinity 规则）、发射**不消耗**；普通弓路径零变化；**零 format**（箭不入存档 = 既有）；1109/1109（+5：解析 3 + 内容钉死 2）、`-Werror` 0 警告、base18 **第 50 次结构性免疫**【新物品不在默认 hotbar + 图标独立 PNG 无 atlas + 配方只在 UI + fireBow 跳过消耗需选中弓 = 无输入零活动】；E2E 全绿（S1 第 50 次过门 / S2 无限弓【**俯角连射 2 次 + 插地拾取返还**（P42 拾取 = 射出可观测证明）：infinite_bow + arrow ×2 → 终态 **arrow ×4**（2 射 0 耗 +2 返），600f ×2 savenorm + EXACT】/ S2b 普通弓对照【同输入 → **arrow ×2**（2 耗 +2 返 = 不变——自见证对：若拾取没发生 S2 也会是 2）×2 savenorm + EXACT】；**PITS +1**【弓拉弓 = 右键——输入脚本用左键 = 静默挥拳无箭，终态与"没射"同值】）；P76 = 营火 v3：再点燃（用户"下一 phase 选成本最低的" = P75 定案建议中的再点燃项，代理定案最小完整版 = **燧石与钢点回未点燃营火**，全量消费 P75 遗留"水浇灭后无再点燃"偏离）：**新块属性 `ignites_to`**（严格字符串、显式空串拒绝【ammo 纪律】、init 存在性交叉校验【grows_to 纪律】）+ 内容 `campfire_extinguished.ignites_to = "campfire"`——燧石与钢（任意 `ignites_flamable` 物品）右键未点燃营火 = **原位再点燃**（光 0→15 局部重光照、"ignite" 音、物品不消耗 = P60 语义、**烤制状态保留续烤**）；交互在**营火分支内**处理（该分支先于通用点火器分支消费点击——首轮 E2E 点击被截获暴露 = 分支所有权纪律）；通用点火器分支的 `ignites_to` 路径对非营火家族块同样生效（数据驱动统一语义）；**零 format、零新物品/纹理/配方**；1104/1104（+3：属性解析 2 + 内容钉死改写 1）、`-Werror` 0 警告、base18 **第 49 次结构性免疫**【属性在永不生成的块上 + 只经右键点击生效 = 无输入零活动】；E2E 全绿（S1 第 49 次过门 / S2 再点燃【P71 瞄准几何、预置**半熟**槽 0（beef progress 300）+ 手持燧石与钢，单次右键 → 块翻回 campfire + **续烤至熟** + 物品未消耗，600f ×2 savenorm + EXACT】/ S2ctl P60 flamable 回归【wheat_3 + 燧石与钢 → fire 块（lifetime 300 mov-config 先例保活）、无掉落、×2 savenorm + EXACT】/ S6 init 失败【scratch 资产 ignites_to "campfirez" → "ignites_to names unknown block" 点名】；**PITS +1**【项目改名后 xdotool 按名搜索子串误匹配终端窗口 → 锚定正则】）；P75 = 营火 v2：四槽 + 水浇灭（用户"下一 phase 完成建议1" = P74 定案建议 1，代理定案最小完整版 = **4 个独立烤位 + 水浇灭成未点燃块**，全量消费 P71 遗留"单槽" + "无水浇灭"偏离）：**`CampfireCookState` = `slots[4]` + `progressSubsteps[4]`**（各自 1/60 整数计时 = 炉子节奏、互相独立；放 = **第一个空槽** / 取 = **第一个非空槽**【索引序确定性、无面板 = v1 简化】；**只有点燃的烤**；**只渲染第一个非空槽图标**）+ **水浇灭**（点燃营火 6-邻任意水 → 瞬时变**新块 `campfire_extinguished`**【光 0、同 interact、掉自己、非 flamable、cross 纹理 = 柴堆无火苗、atlas 空位 [13,0] 免重锚】——P64 fire 块浇灭同款规则、局部重光照；**烤制状态保留** = 物品还在、时钟暂停、空手能取回；**v2 无再点燃** = 文档化偏离）+ **座位扩到未点燃**（MC：柴堆没变——玩家双 campfire id 检测）+ **format 13** `campfires`（**恰 4 槽对象**、家族【点燃 + 未点燃】双向交叉校验、每槽进度不变量、format 11/12 单槽形状 → 槽 0 迁移、旧形状带 `slots` 键 = 版本谎报拒绝）；1101/1101（tick 4 改写 + 独立 1 + save 3 + 座位 1 + 内容钉死 1）、`-Werror` 0 警告、base18 **第 48 次结构性免疫**【未点燃块永不生成 + 默认背包无 + atlas 空位无 UV 移位 + 浇灭 pass 无营火 fast-exit】；E2E 全绿（S1 第 48 次过门 / S2 四槽烤【P71 瞄准几何 pitch −0.220362、beef ×4 四次右键 0.5 s 间隔 → **4 槽全熟**，1200f 输入 ×2 savenorm + EXACT】/ S2b 取回【预置槽 0 熟品 + 空手右键 → 手取第一个非空，×2 savenorm + EXACT】/ S3 水浇灭【feet 格点燃营火 + 6-邻**封闭 L8 水**（3 石围 + 营火格自身停流），240f no-input **×2 RAW 存档+截图字节一致** + EXACT：块变 extinguished、水不流、**状态存活**（beef 槽 0 progress 0 = 浇灭先于首烤 tick）】/ S4 未点燃座位【ctrl 先按再 W 2s = pos 恒定 EXACT，×2 savenorm】/ S6 存档加载失败 ×2【恰 4 槽点名 / 旧形状 unknown field 点名】）；**PITS +1**【Json 数组无元素变异 API：`arr[i] = x` 运行期炸 = 重建数组】；P74 = 火 v4：玩家火块点燃（用户"如果下一步建议中，建议1是最简单的，则下一phase 完成 建议1" = P73 定案建议 1，代理定案最小完整版 = **fire 块点燃玩家 = 熔岩同款**，全量消费 P70"直接接触伤害"的非 MC 语义 + 消掉 P73 记录的"玩家/mob 火接触不对称"偏离）：**`Game::updateEffects` 点火门 `inLava() || isInFire()`**（P57 既有路径：重施加 duration 重置 + tick 相位保留、离开火源残留 8 s、水瞬时熄灭——玩家/mob/熔岩三源同节奏 1 HP/0.2 s）；**P70 直伤机制整体退役**（`m_fireAccum` real-dt 计时器 + `player.fire_damage` / `player.fire_damage_interval_seconds` config 键 + 交叉校验删除；旧 config 仍启动 = 未知键容忍，单测钉死）；`isInFire()` 改公开（原经 `inFire()` 暴露）；**零 format、零内容**；1095/1095（P70 直伤 8 测试删除 + AABB 谓词改写 + 退役 config 容忍 1 新）、`-Werror` 0 警告、base18 **第 47 次结构性免疫**【fire 块永不生成 = fresh 零 fire 格 = 点火门惰性】；E2E 全绿（S1 第 47 次过门 / S2 站火烧死【feet 格 fire remaining 8.0 + spread_chance 0 + mobs off，600f no-input ×2 savenorm + EXACT：health 0 + effects []【死亡清】+ fires []【8 s 燃尽】+ blocks []】/ S2b 残留判别【同格 fire remaining **1.0**：P74 点燃后**离火残留 8 s** 仍烧死——退役的 P70 直伤会在火灭后停 = health 15 存活，互斥，900f ×2 savenorm + EXACT】/ S2ctl 无重叠对照【fire 4.5 格外 → 永不点燃 health 20，600f ×2 **RAW 存档+截图字节一致**】；**无 S6**【零 parse 面】；**PITS +1**【E2E 场景生成的存在性守卫被冒烟残留文件整块跳过】）；P73 = 火 v3：Mob 火接触点燃（用户"好的,那就按难易程度，从建议1开始下一阶段" = P72 定案建议 1，代理定案最小完整版 = **火块 = P58 熔岩点燃的块版**，全量消费 P70 遗留"Mob 站火里不受伤害"）：`MobEntity::update` 点火条件 **`inLava || inFire`**（`inFire` = `Entity::overlapsBlock`【P70 既有】，fire 块 id 构造时注册表解析、无 fire 块 = invalid = 结构惰性 = P70 玩家同款容错）——同一 fire 效果同一节奏（1 HP/0.2 s、可致死）、火块内每帧重点燃、离火残留 8 s、水瞬时熄灭（P57 规则）；**零新 config、零 format、零内容**（全走 P58 既有路径）+ **顺带修复 P27 潜伏 UB**（skeleton 射击回调在实体 range-for 内 push_back = 重分配悬空 → SIGSEGV【P73 火把膜对照场景首触发】→ 实体更新循环改索引循环，PITS +1）；1102/1102（+5）、`-Werror` 0 警告、base18 **第 46 次结构性免疫**【fire 块永不生成 = fresh 零 fire 格 = inFire 恒 false 路径惰性 + 无 config/内容变更】；E2E 全绿（S1 第 46 次过门 / S2 火膜烧死【3×3 fire 膜 y46（21..23,−6..−4）+ fires remaining 1000 + spread_chance 0 + 夜 0.75 mobs on，2400f no-input ×2 savenorm 字节一致 + EXACT：5 mob（2 牛/1 鸡/2 羊）烧死 = 9 实体 12 掉落、膜 9 格完整、玩家 20 血【光 15 挡敌对】】/ S2ctl 火把膜对照【同几何 torch（光 14 缺口环 → 敌对杀无输入玩家 health 0 = 接受副产物，P58 水膜对照同款设计）：**entities [] = 无 mob 被烧死**——火把非 fire 块不点燃，×2 savenorm】；**无 S6**【零 parse 面】）；P72 = 吸收两分量模型（用户"请继续未完成的 phase" = 延续 P71 定案建议 2，代理定案最小完整版 = **health + absorption 独立池**，全量消费 P56 遗留"吸收层与正常心共用一个 health 值"）：**`m_health`（0..base max）+ `m_absorption`（0..64，`kAbsorptionMax`）独立池**（`damage()` **先扣池**【池单独不能致死，死亡只来自 health】、`heal()`/`setHealth()` 只钳 base max【heal 永不填池】、`maxHealth()` = base）+ **池 = 效果 `absorb_hp` 总和的纯函数只在同步点**（进食施加 / mob 命中 / updateEffects 内到期或水熄灭【效果总和变化才同步——每帧同步会与伤害扣减相抗，PITS】/ 牛奶清 / 死亡清——刷新恢复满层 = MC 属性重算；**读档不同步** = 存值权威、池 < 总和合法【伤害在同步点间持续】）+ **format 12 `player.absorption`**（键纪律：12 必需 / ≤11 带键拒绝 / [0,64] / ≤ 存档效果总和不变量 + player 白名单）+ **format ≤11 迁移**（共享 health 按 base max 拆分 22→20+2、不损伤害）+ **HUD 吸收红心改从池派生**（ceil(池/2) 颗、strip 值 = 池、池 0 = 零绘制 = 结构性免疫保持）+ F3 调试行；1097/1097（player 5→7 两分量语义改写 + save 套件 5 新 + 15 处旧 format 降级剥键适配）、`-Werror` 0 警告、base18 **第 45 次结构性免疫**【fresh 无输入 = 零效果零伤害零 heal = 池 0 零绘制】；E2E 全绿（S1 第 45 次过门 / S2 坠落 D=4 全吸收【存档预置空中 (22.5,52,0.5)、120f no-input ×2 savenorm + EXACT：池 0 + health 20 + 效果仍活】/ S2b D=8 部分【池 0 + **health 16**——与 S2c 的 4-HP 差 = 池先吸收】/ S2c 对照【无池 health 12，×2 savenorm】/ S3 迁移【format 11 共享 22 → health 20 + 池 2，池全程保 2 = 无每帧重同步，×2 savenorm】/ S4 HUD 池 4【locked run 吸收盒 **138 px** 红心 > 100】/ S4d 扣空池 + 效果活【**0 px** < 20——旧模型画 2 空红心 ≈138 px，互斥】/ S6 存档加载失败 ×2【12 缺键点名 / 100 越界点名】；**PITS +2**【每帧纯 SET 与帧间状态相抗 / 自然回血污染长跑伤害终态：run 时长须 < regen_delay 窗】）；P71 = 营火（用户"请继续未完成的 phase" = 延续定案建议 1，代理定案最小完整版 = **campfire = 烤食物 + 座位**，消费火把 v1 条"无 campfire"）：**`campfire` 块**（cross + 营火纹理 atlas 空位 [12,0] 免重锚、`light_emission 15` = MC、hardness 0.5、自掉、非 flamable、`interact: "campfire"` = 第三 interact 词汇表值）+ **配方 shaped SSS/LCL/LLL**（MC 营火配方：3 stick + 5 wood + 1 coal）+ **单槽烤制**（per-block `CampfireCookState` = 炉子 P44 范式：setBlock 收口 + std::map 位置序；手持可熔炼物品（`smeltable_to` 非空 = 数据驱动门）右键空槽放 1 / **空手**右键取回（生熟均可 = MC 空手规则）；固定 1/60 子步 + 整数 progressSubsteps = **炉子节奏**（复用 smelt_time、无燃料 = MC 自燃、无 config）；**右键链中营火分支先于进食通道**（MC 语义——全部可熔炼物品都是食物，无此前置则烤槽填不上；炉子/工作台既有行为不变）+ **烤制中物品渲染**（0.5 交叉图标 quad 悬浮格上半部，cross 惯例、opaque pass、+Y 假法线）+ **座位**（crouch + 贴地 + AABB 严格重叠 campfire 格 → 锁移动/跳，复用 crouch 高度 = 文档化偏离：无独立坐姿、不入存档）+ **format 11 `campfires`**（每 campfire 块一条目、双向交叉校验 + 进度不变量 progress>0 ⇒ 槽恰 1 可熔炼、format 10 迁移 = 空 + 手编旧文件 → 空状态）；1092/1092（+20：tick 4 + 存档 7 + interact 2 + 座位 5 + 内容钉死 2）、`-Werror` 0 警告、base18 **第 44 次结构性免疫**【campfire 永不生成 + 默认背包无 + 配方只在 UI + atlas 空位无 UV 移位】；E2E 全绿（S1 第 44 次过门 / S2 烤制完成【右键放 beef + 600 子步 → cooked_beef，900f ×2 savenorm + EXACT】/ S2b 熟取回【预置 590 + 空手右键，×2 savenorm】/ S2c 生取回【×2 savenorm】/ S2r 图标渲染【no-input 600f ×2 RAW + 1192 px 像素 diff vs 空槽对照】/ S3 座位【ctrl 先按再 W 2s = 坐位先于移动建立，pos 恒定 EXACT，×2 savenorm】/ S3ctl 对照【无 campfire = crouch-walk pos.z < 0】/ S6 init 失败 ×2【interact "campfirez" 点名 / 配方 key L "woodz" 点名】；**PITS +2**【xdotool 键名 ctrl ≠ Left_Control 静默忽略 / 键组合 keydown 顺序 = 确定性变量】）；P70 = 火接触伤害（用户"按建议，继续下两个 Phase" = 建议 2，代理定案最小完整版 = **玩家站 fire 块里受伤**，消费火 v2 条"无接触伤害"；**mob 火接触 = 文档化偏离**【v3 范围】）：config 2 键 **`player.fire_damage`**（默认 1 = MC 1 HP/0.2 s、**0 = 关**、`≤ max_health` 交叉校验）+ **`player.fire_damage_interval_seconds`**（默认 0.2、>0 硬校验）mirror P54 熔岩键 + **`Entity::overlapsBlock(BlockID)`**（mirror `overlapsFluid` 的 AABB 严格不等式扫描、caller 传 id = 数据驱动）+ Player `isInFire`（fire 块 id 构造时解析一次，fluid.cpp 回落 air 同款容错）+ **`m_fireAccum` real-dt 计时器**（mirror 熔岩块：在火里累积、离开重置、正常 damage 路径、0 = fast-exit）；**无 format 变更**（10）；1072/1072（+9：行为 5 + config 4）、`-Werror` 0 警告、base18 **第 43 次结构性免疫**【fresh 零 fire 块 = 路径惰性】；E2E 全绿（全 no-input：S2 玩家烧死【fire@(22,45,0) remaining 8.0 + spread_chance 0，600f：**4.0 s 死** fps 无关 + fires [] + blocks {} + 零实体，×2 savenorm】/ S2b fire_damage 0 对照【存活 health 20，零 real-dt 字段 → 存档+截图 ×2 RAW】/ S6 config 失败 ×2【interval 0 点名 / damage 21 点名】）；P69 = 镐分类（用户"按建议，继续下两个 Phase" = 建议 1，代理定案最小完整版 = **镐 = 石类/矿类分块效率**，**零核心代码** = 纯内容【机制 = P67，P62/P68 同款路径】，消费 P11 镐分类遗留）：3 镐各加 fast_against **[stone, cobblestone, coal_ore, iron_ore, gold_ore, diamond_ore, lapis_ore, glowstone]**（**项目石矿类全集**）+ 倍率 **2.0**；**obsidian 刻意排除**【MC 铁镐不能破黑曜石、项目无钻石镐 = 保留黑曜石 flat 镐速 3.75 s（铁）= 文档化偏离】；flat 档 2/3/4 不变 → 石矿类有效 4/6/8；**无 format 变更**（10）、**无新物品/配方/图标**；1063/1063（+2：8 块精确集钉死 1 / 真实资产纯函数集成 1）、`-Werror` 0 警告、base18 **第 42 次结构性免疫**【镐不在默认背包 = 零世界状态】；E2E 全绿（判别设计 = **hold 卡在 ×2 与无 ×2 破坏时间之间 = 两结果互斥**：S2 铁镐破石【hold **0.42 s**：×2 后 **0.28125 s < hold 破**、无 ×2 则 0.5625 s 存活 = 倍率证明；掉 cobblestone×1【P61 材料链】停证明落点 (22.5,45,−4.5)，×2 savenorm】/ S2b 铁镐对木【hold 0.55 s：flat 0.75 s > hold **存活**、泄漏 ×2 = 0.375 s 会破 = 互斥，×2 RAW 存档+截图】/ S6 init 失败 ×2【未知块点名 / 倍率 0 点名】）；P68 = 铲类工具（用户"好的,下一阶段完成建议1" = 建议 1，代理定案最小完整版 = **铲 = 土类分块效率**，**零核心代码** = 纯内容【机制 = P67 的 fast_against + effectiveBreakSpeed，P62 石镐同款路径】，消费 P11 铲分类遗留）：内容 **wooden/stone/iron_shovel**（fast_against [dirt, grass, sand, gravel] **×2.0 = MC 铲对土 2×**【**项目土类全集**——无 clay/sandstone/snow】、break_speed 缺省 1.0 = 手速【MC 铲对石 1×】、attack **2.5/3/3.5 = MC 精确值**【同档 = 镐 +0.5 = MC 增量】、max_stack 1）+ **3 配方**（shaped 竖列 M/S/S = MC 铲：木 = planks / 石 = cobblestone【P61 第三工具用途】/ 铁 = iron_ingot；单列 pattern = 解析器 1..3 字符行合法）+ **3 图标**（图标 = 每物品独立 PNG、无 atlas 重锚）；**无 format 变更**（10）、**无核心代码变更**；1061/1061（+2：钉死 2）、`-Werror` 0 警告、base18 **第 41 次结构性免疫**【铲不在默认背包 + 配方只在 UI = 零世界状态】；E2E 全绿（S2 铁铲破 dirt【hold 1.6 s：1.5×0.5/2.0 = **0.375 s << hold** 破、掉 dirt×1 停 P59/P61/P67 证明落点 (22.5,45,−4.5)，420f ×2 savenorm】/ S2b 石对照【2.25 s > hold **不破** = 倍率不越清单，存档+截图 **×2 RAW 字节**】/ S6 init 失败 ×2；批 1 失败 = E2E 脚本漏建 helpers【非项目缺陷】）；P67 = 斧类工具（用户"下两个阶段是 建议1 和 建议3" = 建议 3，代理定案最小完整版 = **斧 = 数据驱动分块效率工具型**，全量消费 P62"无斧类工具" + 部分消费 P11"工具无分块类型效率"）：**物品字段 `fast_against`（非空块名数组）+ `fast_against_multiplier`（>0）严格配对**（缺一/空数组/非 tool 拒绝、逐条目点名；init 交叉校验块名存在 = grows_on 同款）+ **`effectiveBreakSpeed(item, block)` 纯函数**（item.hpp：非 tool = 1.0；tool = breakSpeed ×（目标在 fast_against 内 ? 倍率 : 1））+ 内容 **wooden/stone/iron_axe**（fast_against [wood, leaves] **×2.0 = MC**、break_speed 缺省 1.0 = 手速【MC 斧对石 1×】、attack 3/3.5/4 = 各比同档镐 +1、max_stack 1）+ **3 配方**（shaped PP/PX/ X L 形 = MC 斧：木 = planks / 石 = cobblestone【P61 第二工具用途】/ 铁 = iron_ingot）+ **3 图标**（图标 = 每物品独立 PNG、无 atlas 重锚；既有图标字节不变 = 确定性再生）；**无 format 变更**（10）、**无新 ItemType**（斧 = 带 fast_against 的 tool）；1059/1059（+11：解析 6 / 纯函数 3 / 钉死 2）、`-Werror` 0 警告、base18 **第 40 次结构性免疫**【轴不在默认背包 + 配方只在 UI = 零世界状态 = P62 镐同款；block atlas 未动】；E2E 全绿（S2 铁斧破木【hold 1.6 s：1.5×2.0/2.0 = **1.5 s < hold** 破、掉 wood×1 停 P59/P61 证明落点 (22.5,45,−4.5)，420f ×2 savenorm】/ S2b 石对照【2.25 s > hold **不破** = 倍率不越清单，存档+截图 **×2 RAW 字节**】/ S6 init 失败 ×2【未知块点名 / 材料带 fast_against 点名】；**PITS +1**【掉落静止位 ≠ 被破格中心：用已证明落点常量断言】）；P66 = 火 v2：粒子 + 蔓延（用户"下两个阶段是 建议1 和 建议3" = 建议 1，代理定案最小完整版 = **火 v2**：火焰余烬粒子 + 蔓延引燃相邻可燃块，全量消费 P64 火 v1 遗留"无粒子/无蔓延"；斧类工具 = 建议 3 = P67）：**蔓延引擎**（`world/fire_spread.{hpp,cpp}` 纯函数镜像 P48 生长范式：每 `fire.spread_interval_frames` 帧【默认 6 = 0.1 s MC tick】每活体火对 **5 邻**（水平 4 + 上、**不向下** = MC）flamable 格独立 roll（`fire.spread_chance` 默认 0.05 = MC normal；专用 salt + 帧/邻序推进 = P45/P49 纪律）→ `setBlock(fire)` 收口注册满寿命 = 新火续燃续蔓延）+ **余烬粒子**（`spawnFire` 2 粒/0.1 s/火格、**负重力 −2 上升**、寿命 0.3–0.5 s、发射固定帧步种子 = 世界种子⊕位置哈希⊕帧推进；**燃尽/浇灭死亡爆发** = 复用 `spawnBreak` 16 粒）+ **`Particle.gravity` 逐粒字段**（破碎下落/余烬上升）+ **config 3 键**（interval ≥1 / chance [0,1] / `fire.particles` bool = mobs.enabled 范式 E2E 对照组）+ **渲染修复**：cross 粒子面原走 cube 法线 → ±Z 面 shade 0.6 偏暗（E2E 火焰像素 A<C 暴露）→ 假法线 (0,1,0) 满亮 = chunk_mesher/item_mesh 既有两 cross 分支同款约定【PITS】；**无 format 变更**（format 10）、**无内容/atlas 变更**（无重锚）；1048/1048（+12：fire_spread 8 / spawnFire 4）、`-Werror` 0 警告、base18 **第 39 次结构性免疫**【无 block/worldgen/atlas + 新 config 只作用于持 fire 块的格 = fresh m_fires 恒空双 fast-exit】；E2E 全绿（S2 蔓延【探针预计算 **帧 102** 点燃 +Z 木、**下方木存活**、fires = 2 条目，×2 savenorm】/ S3 余烬【A 默认 vs C fire.particles false：A≠C + 火焰像素 2426/2452 > 2383 + C×2 字节 + A×2 savenorm【活余烬 real-dt 运动 = 截图不可字节比，P54 条款】】/ S4 燃尽 + 死亡爆发【remaining 4.0 fps 无关终态 air + 粒子全灭后截图 ×2 RAW】/ S6 init 失败 ×2【interval 0 / chance 1.5 越界点名】；**PITS +2**【cross 粒子须假法线满亮 / 活粒子截图不可字节比 + config 键 A/C 对照组】）；P65 = 火把内容：torch 方块（用户"按建议顺序，继续完成下两个" = 建议 2，代理定案最小完整版 = **torch 方块**：可放置光源，光 14 = MC、cross、瞬破、掉自己；coal+stick 配方 = coal 首个合成用途；campfire/火蔓延超出最小内容范围【见 KNOWN_ISSUES 火把 v1 条】）：**`torch` 块**（cross + 火把纹理 atlas 空位 [11,0] 免重锚、`light_emission 14` = MC、瞬破、**drops torch** = 可复用、非 flamable = 光源不是火源）+ **配方 shapeless coal+stick → 4**（MC，coal 首个合成用途）+ **bug 修复：破坏 cross 块崩溃**（P45 潜在：`drawParticles` 粒子纹理查 `cubeFaceTexture`（top/side/bottom 槽）对 cross 模型 = 空名 → `AtlasLayout::uvMin` 抛 `unordered_map::at`；火把破坏 E2E 首次暴露 + gdb 定位；修复 = cross 分支，粒子立方体六面全用 `cross` 纹理，与 item_mesh/chunk_mesher 两分支同款）；**无 format 变更**（format 10，torch = 块编辑）；1036/1036（+2）、`-Werror` 0 警告、base18 **第 38 次结构性免疫**【torch 永不生成 + 只经合成、默认背包无 + atlas 空位无 UV 移位】；E2E 全绿（S2 放置【石块锚点顶面瞄准，torch 立顶面 + 物品消耗 + fires []【火把非 fire 块：无寿命条目】，×2 RAW】/ S3 夜间渲染+光【午夜 600f，A≠B + 火焰像素 + ×2 各】/ S4 破坏 → 掉 torch 物品【**左键按住** = 破坏、hardness 0 首帧即破、掉落物停锚点上，×2 savenorm】/ S2ctl 无输入 ×2 RAW / S6 init 失败 ×2【light_emission 16 越界点名 / 配方原料未知点名】；**PITS +3**【瞄准 pitch 正 = 朝上 / 破坏 = 左键按住、右键 = 放置交互链 / awesomeWM float toggle 是 TOGGLE + 60f 跑 ~1.85 s 预算】）；P64 = 火视觉 v1：fire 方块（用户"按建议顺序，继续完成下两个" = 建议 1，代理定案最小完整版 = **fire 方块**：可燃物烧成发光火焰而非 air，定时燃尽、水浇灭，**format 10**；粒子/蔓延超出最小范围【见 KNOWN_ISSUES 火 v1 条】）：**`fire` 块**（cross + 火焰纹理 atlas 空位 [10,0] 免重锚、`light_emission 15` = MC、瞬破、**无 drops**、非 flamable = 燃烧终态）+ **燃烧目标改 fire**（点燃器 P60 + 熔岩燃烧 pass P58；registry 无 fire 块回落 air = 单测 fixture 语义）+ **`Game::m_fires` 剩余寿命 map**（setBlock 收口创建/删除 = 炉子范式；real-dt 倒计时；熔岩 pass 走 changed 列表补注册）+ **水浇灭**（6-邻水 → 瞬灭；水胜火；熔岩不浇灭）+ **config `fire.lifetime_seconds`**（默认 4.0，>0 硬校验）+ **init 交叉校验**（有 flamable 块 ⇒ 必须有 fire 块，点名拒绝）+ **format 10 `fires`**（{pos, remaining} 位置序、remaining > 0、与块编辑双向交叉校验 = 炉子键纪律、format 9 迁移 = 空 + 手编旧文件补满寿命）；**无 worldgen 变更**；1034/1034（+11）、`-Werror` 0 警告、base18 **第 37 次结构性免疫**【fire 永不生成 + fresh 零熔岩 = 燃烧 pass 惰性 + 默认背包无点燃器 + atlas 空位无 UV 移位】；E2E 全绿（S2 点燃器 → **fire 活体终态**【MOV 300 = P57 模式，remaining ∈ (250,300]，×2 savenorm 剥 fires remaining】/ S3 燃尽【场景 remaining 4.0 + 600f = 10..26.1 s > 4.0 = fps 无关终态 air，×2 RAW】/ S4 水浇灭【**高架水** = 石台 L8 + 四侧石封边 + 火正上方——首轮裸 L8 水源横向漫灌高原【**水 flow 7 vs 熔岩 1，PITS】**+ (23,45,−5) 生成装饰格非 air，×2 RAW】/ S5 夜间渲染+光【**600f**——60f `--save` 跑可视 chunk 流送未竟 = 地面缺失 worker 竞态【PITS】，A≠B + 火焰像素 + ×2 各 + pair 重试【8px WM 黑条环境性】】/ S2ctl 无输入 ×2 RAW / S6 init 失败 ×2【删 fire.json → 交叉校验点名 / lifetime 0 → config 校验】；**PITS +2**）；P63 = flamable 内容扩展：MC 天然可燃三块（用户"先建议2,再建议1" = 建议 1，代理定案最小完整版 = 纯数据 `flamable` 扩展，**零核心代码**；火视觉/引燃 Mob 超出范围【见 KNOWN_ISSUES】）：**wood/leaves/grass** 加 `flamable: true`（与 P58 的 wheat_1..7 合 = 自然世界可燃面；点燃器 P60 与熔岩燃烧 pass P58 两个消费者立即生效）；可燃集**恰 10 块**精确集断言钉死（羊毛仍不燃 = 文档化偏离）；P58 测试负清单契约演进（wood/leaves 移出）；**无 format 变更**（format 9）；1023/1023（+1）、`-Werror` 0 警告、base18 **第 36 次结构性免疫**【JSON 属性无视觉 + fresh 零熔岩 = 燃烧 pass 惰性 + 默认背包无点燃器】；E2E 全绿（S2 点燃器烧 wood【(22,46,−5) 单次右键 → air、无掉落，420f ×2 savenorm】/ S3 熔岩烧 wood【P58 池几何 + **9 石衬砌**——池在草高原凹穴，P63 不衬砌则池底烧穿落层【第一轮】/ L7 草邻野火【第二轮】，PITS +2；240f ×2 RAW：wood → air + L7、池保 L8、零野火】/ S2ctl 无输入 ×2 RAW / S6 init 失败 ×2【flamable "yes"/1 → must be a boolean】）；P62 = 石头工具线 v1：石镐（用户"先建议2,再建议1" = 建议 2，代理定案最小完整版 = 镐路线中档 **stone_pickaxe**，**零核心代码**；项目无斧类工具 → "石斧" = 新工具型超出最小范围【文档化偏离】）：**`stone_pickaxe`**（tool、max_stack 1、`break_speed 3.0` / `attack_damage 2.5` = 项目压缩刻度**恰居中**于木镐 2.0/2 与铁镐 4.0/3；图标 = 共用 pickaxe() 锻灰色调）+ 配方（shaped **CCC/.S./.S.**，C = cobblestone = MC 配方 = **cobblestone 首个工具用途**：挖石→锻石→石镐→更快挖铁 = P61 材料链闭环）；**无 format 变更**（format 9）；1022/1022（+2）、`-Werror` 0 警告、base18 **第 35 次结构性免疫**【工具物品不在默认背包 + 配方只在 UI = 零世界状态】；E2E 全绿（S2 工具实战【stone 放 (22,46,−5) 石镐按住 1.0 s【1.5×1.5/3 = 0.75 s】→ 掉 cobblestone×1，420f ×2 savenorm 字节一致 + save EXACT】/ S2ctl 无输入 ×2 RAW / S6 init 失败 ×2【break_speed "fast" → must be a number / 配方 key typo → "key entry 'C' uses unknown item"】）；P61 = stone→cobblestone 材料链（用户"下两个 phase，请继续建议1 和建议2" = 建议 2，代理定案最小完整版 = cobblestone 块 + stone 自掉变更【**内容契约变更**】+ 4 锻石配方，MC 材料循环，**零核心代码**）：**`cobblestone`**（hardness 2.0 = MC、自掉、纹理 = stone 底 + 深色裂纹缝）+ **stone.json `drops: stone → cobblestone`**（挖天然石得锻石；stone 只能经合成回来）+ 配方 `stone`（shapeless 4 cobblestone → 1 stone = MC 1.14+）+ **atlas 空位 [9,0]**（65→66/128、grid 不变 = 无 UV 移位 = **无重锚**）；cobblestone 不能熔炼回 stone【block item 无 item 数据，文档化偏离】；契约变更安全 = 调研审计（无配方以 stone 为原料、只有 stone.json 掉 stone）；**无 format 变更**（format 9）；1020/1020（+2）、`-Werror` 0 警告、base18 **第 34 次结构性免疫**【空位 tile 无 UV 移位 + drops 变更只在破坏时生效】；E2E 全绿（S2 掉落变更【stone 放 (22,46,−5) 铁镐按住 1.0 s → 掉 **cobblestone×1 而非 stone×1**【(22.5,45,−4.5) 落地 = P59 证明位置】，420f ×2 savenorm 字节一致 + save EXACT】/ S2ctl 无输入 ×2 RAW / S6 init 失败 ×2【cobblestone.json 删除 → "block 'stone' drops unknown item" = 契约依赖方向钉死 / 配方 typo → "uses unknown ingredient item"】）；P60 = flint 用途：箭头 + 燧石与钢（用户"下两个 phase，请继续建议1 和建议2" = 建议 1，代理定案最小完整版 = MC 1.14+ 箭头配方【**替换** P24 两木棍配方 = 内容契约变更】+ flint_and_steel + 点燃交互，消费 P59 遗留"flint v1 无用途"）：**箭头配方替换**（shapeless flint+stick+feather → 4 箭 = MC 1.14+；flint 成真材料：gravel→flint、chicken→feather、树→stick = MC 闭环）+ **`flint_and_steel`**（material、64 叠、图标 = 钢杆 + 燧石尖）+ 配方（flint+iron_ingot）+ **物品字段 `ignites_flamable`**（严格 bool、缺省 false、非 bool 点名拒绝；与**五个右键通道互斥** food/drink/launcher/dye/fluid bucket = 通道所有权纪律；init 白名单 = P58 教训）+ **右键链点燃分支**（interact/bucket 之后、placeBlock 之前：目标 `flamable` 块 → setBlock(air) **无掉落**【P58 熔岩烧语义】、**不消耗**【MC：燧石与钢用不坏】；炉子/工作台仍开面板；非 flamable fall through）+ **`ignite` 声音**；**无 format 变更**（format 9）；1018/1018（+8）、`-Werror` 0 警告、base18 **第 33 次结构性免疫**【无 block / 无 worldgen / 默认场景纹理零改动 = P59 重锚后首次过门、**无重锚**】；E2E 全绿（S2 点燃【wheat_3 放 (22,46,−5) P52 几何、瞄准锚定存档 = 零鼠标移动、单次右键 → 块烧成生成 air【diff = {}】**零掉落**【entities = []，破坏会掉 wheat_1 种子 = 可区分】，420f ×2 savenorm 字节一致 + save EXACT；下方草 ≠ farmland = 生长门失败 = 场景全静态】/ S2b 非可燃隔离【同格 stone、同物品、同点击 → stone 保留】/ S2ctl 无输入 ×2 RAW / S6 init 失败 ×2）；P59 = 多掉落内容再扩展：gravel + flint（用户"按你的建议继续下两个phase" = 建议 2，代理定案最小完整版 = gravel worldgen 矿脉 + flint 物品 + gravel 10% 概率掉落 + 内容钉死单测，**零核心代码**）：**`gravel`**（hardness 0.6、自掉 + `chance_drop: {flint, 1, 0.1}` = MC 值）+ **`flint` 物品**（material、64 叠、图标 = 3 片碎块）+ worldgen `ores.veins` + gravel（probability 0.25 / vein 4 / max_y 80 = 常见石替换脉）+ **atlas 8×8 → 16×8**【64/64 已满、gravel = 第 65 tile → 所有既有 UV 移位 → base18 重锚（P41 先例）】；flint v1 无用途、排除 stone→cobblestone 链（文档化偏离）；**无 format 变更**（format 9）；1010/1010（+3）、`-Werror` 0 警告、base18 **重锚**【atlas 扩容移位所有 UV，第 33 次过门 = Phase 60】；E2E 全绿（S2 flint 掉落【roll-PASS 格 (21,46,−1)、yaw=π/4 瞄准锚定存档 = 零鼠标、场景 diff 清 3 角点格【45° 射线沿格网角点穿过 → DDA tie-break 命中对角固块，PITS】：gravel×1 + flint×1，×2 savenorm 字节一致】/ S2b 无 flint 对照【roll-FAIL 格 (22,46,−5)：只掉 gravel = per-cell 确定性 roll 隔离】/ S2ctl 无输入 ×2 RAW / S6 init 失败 ×2）；P58 = 火系统 v2：熔岩烧 Mob 与作物（用户"按你的建议继续下两个phase" = 建议 1，代理定案最小完整版 = Mob 点燃 + 熔岩烧作物，**全量消费** P54 遗留"熔岩不烧 Mob 与作物"）：**块属性 `flamable`**（可选 bool 缺省 false，非 bool 点名拒绝；内容 = wheat_1..7 全 7 阶）+ **FluidSim 燃烧 pass**（扫所有 LAVA 格 6-邻 flammable BLOCK 格 → 收集排序后 setBlock(air)，**无掉落**，快照确定性，注册表无 flamable 块 = fast-exit）+ **Mob 点燃**（生成时 `armFire` 从 ignites_on_lava 效果 + config `fire_duration_seconds` 取参；**整数帧燃烧格**【0.2 s tick 的 double 累加实测相位 off-by-one，改 int 帧计数】：1 HP/0.2 s = 5 HP/s 可致死、熔岩内每帧重置 duration【相位保留】、离熔岩烧 8 s 残留、水瞬时熄灭且先于 tick 【水胜熔岩】）+ **init 交叉校验**（ignites 效果 tick_interval×60 必须整帧数，容差 1e-4 帧——float 0.2f×60=12.00000018 过、0.33 s 拒） + `Entity::overlapsFluid` 自 player.cpp 上移 + 顺手修复 P56 `grants_effects` 白名单一行漏列；**无 format 变更**（format 9）；1007/1007（+18）、`-Werror` 0 警告、base18 **第 32 次结构性免疫** 【fresh 世界零熔岩（P54）、零 flammable 块（wheat 只玩家种）、60f 内零 mob = 两条燃烧路径皆惰性】；E2E 全绿（S2 熔岩膜【3×3 level-1 膜铺地面：走过即燃，10 只被动 mob 烧死 = 18 item 实体，膜光 15 挡 hostile 光门 → 玩家 20 HP 恒定，2400f ×2 savenorm 字节一致】/ S2ctl 水膜对照【hostile 杀玩家 health 0、零点燃 = 隔离熔岩】/ S4 作物燃烧【wheat_3 贴池 → air、池 +1 L7、×2 RAW 字节】/ S6 init 失败 ×2）；P57 = 脱离熔岩残留火：火系统 v1
   （用户"按你的建议，继续下一phase" = 建议 1，代理定案最小完整版 =
   玩家点燃 + 残留燃烧 + 水灭火，消费 P54 遗留"脱离熔岩无残留燃烧"）：
   **效果两数据标志**（`ignites_on_lava` 至多一个注册效果可带【init
   交叉校验】+ `extinguished_by_water`）+ **`fire` 效果**（1 HP/0.2 s
   = MC 燃烧节奏、可致死、`negative: false` = 牛奶不灭火）+
   **`Game::updateEffects` 点燃/灭火**（在 count==0 早退前每帧执行：
   inLava 每帧重施加 = 熔岩内持续燃烧 + 离熔岩 countdown 8 s 残留；
   inWater 瞬时灭火且先于 tick = 水内零伤害，水胜熔岩）+ config
   `player.fire_duration_seconds`（默认 8 = MC Normal）+ effects.png
   64→80（火焰 icon 4，UV 动态零代码）+ 熔岩内 P54 直伤与火 tick
   叠加 ~9 HP/s（MC 同款）；**无 format 变更**（format 9，读档续烧）；
   989/989（+7）、`-Werror` 0 警告、base18 **第 31 次结构性免疫**
   【fresh 世界零熔岩（P54）零效果 = 点火源永不触发】；E2E 全绿（S2
   复合烧死 / S2b 纯点燃【mov config 熔岩直伤 300 s = 唯一伤害源 =
   火，无 P57 则活着 → DEAD 终态 = 点燃证明】/ S3 水灭火零伤害 / S4
   干地 14 tick【到期帧不 tick】/ S5 P54 游泳场景原样 + 纯火死于贴墙
   终态【几何钳制 fps 无关】/ S6 init 失败 ×3）；P56 = 金苹果后续：吸收层（用户
   "金苹果后续，为下一阶段。"，代理定案最小完整版 = 吸收层 HP 概念 +
   `grants_effects` 列表 + 金苹果再生+吸收，消费 P49 遗留"金苹果只有
   再生（无吸收）"）：**效果 `absorb_hp`**（整数 [1,64]，独立于
   tick/melee_penalty，no-op 规则 = 三者皆无才拒）+ **有效 max health**
   （`base + Σ层`；施加层 = health 提升增量【层显示为满】，层到期 =
   超出 base 的 health **无声掉下**【无红闪/伤害计时】；heal 可填进层；
   读档 = 层先于 health 生效【applySave 4d/4e 重排】+ 不重复提升）+
   **`Game::syncAbsorption` 六收口点**（进食/mob 命中/每帧到期/牛奶/
   死亡/读档）+ **物品 `grants_effects`**（严格配对数组，与
   `grants_effect` 互斥【P52 模式】，仅食物，init 逐条目校验）+ 内容：
   `absorption`（4 HP = MC 吸收 II、正面、icon 3）+ 金苹果改
   `[regeneration 10 s, absorption 5 s]` + effects.png 48→64（红心+
   白十字）+ **HUD 吸收红心**（正常心带左侧 `ceil(层/2)` 颗红心，无层
   零绘制 = 结构性免疫）；**无 format 变更**（format 9；health 24 + 层
   读档合法）；982/982（+15）、`-Werror` 0 警告、base18 **第 30 次结构
   性免疫**【无输入零效果 = 红心带零绘制】；E2E 全绿（S2 金苹果吃
   1200f = fps 无关终态【19.4..46 s > 再生到期 14.5 s】slot0 空 + 饥饿
   16 + health 20 + 效果 []；S3 层激活 health 24 合法读档 + 红心带像素
   存在；S4 到期钳制 24→20 字节 ×2；S5 不过扣 18→18 字节 ×2；S6 init
   失败 ×3；savenorm 新增剥 saturation = 真实 dt 字段）；
   P55 = 多掉落内容扩展（用户"熔岩湖
   不着急，先记录下来，留到很久的未来。下一步先进行建议2的扩展。" =
   建议 2，代理定案最小完整版 = 3 方块 MC 语义内容扩展、**零代码**，
   消费 P52 遗留"多掉落内容扩展"；worldgen 熔岩湖 = 用户明示"留到很久
   的未来"）：**`leaves`**（单条目 `chance_drop` →
   `chance_drops: [stick×1 @ 0.02, apple×1 @ 1/128]` = MC 1.14+ 树叶掉
   木棍 2% + 苹果；无 sapling = 文档化偏离，苹果保留 P49 项目值）+
   **`lapis_ore` / `glowstone`**（`drops_count: 2` + 两个 50% 条目 =
   2..4 均匀 = MC 同款数量分布）+ **3 个内容钉死单测**（加载真实
   `assets/data/blocks` 断言字段精确值——防概率笔误，如 0.2 vs 0.02）；
   **无 format 变更**（format 9）；967/967（+3）、`-Werror` 0 警告、
   base18 **第 29 次结构性免疫**【掉落只作用于破坏，fresh 世界无输入
   零破坏 = 无输入零活动】；E2E 复用 P52 已验证瞄准几何（p55scan 真实
   raycast 复验 3 目标 HIT + 真实 roll 预计算：lapis×3 / glowstone×3 /
   leaves×1+stick×1；铁镐按住破坏；归一化存档 ×2 + 精确掉落断言 +
   无输入对照 ×2 全过）；P54 = 熔岩后续（用户"好的.按建议
   继续。" = 建议 1，代理定案最小完整版 = 熔岩游泳状态 + 灼烧伤害 +
   下沉减速 + 熔岩不湿润耕地，消费 P51 遗留"熔岩无燃烧/减速" + "湿润
   判定把熔岩也算水"，worldgen 熔岩湖/脱离残留火/熔岩烧 Mob 与作物
   留后续）：**熔岩 = 同款游泳状态 + 熔岩节奏 + 灼烧**（`isInLava`
   AABB 扫描，水/熔岩并存时熔岩优先；`lava_swim_speed` 1.9 / 上浮 1.5 /
   重力 4 / 终端 2，config 可覆盖，比值对齐 MC 运动系数比
   0.014/0.0231）+ **灼烧伤害**（真实 dt 生存计时器：每
   `lava_damage_interval_seconds` 1.0 s 受 `lava_damage` 4 HP = MC
   4 HP/s，走正常伤害路径；离熔岩重置未到期 tick = v1 无残留火）+ 落入
   熔岩取消坠落伤害 + 熔岩内抑制疾跑/下蹲 + **`cropIsWet` 只算水**
   （熔岩旁耕地 = 干节奏）；6 个新 `player.*` config 键（默认 = MC 值，
   `lava_damage` ≤ `max_health` 交叉校验）；**无 format 变更**（format
   9）；964/964（+10：物理 7 + config 2 + growth 1）、`-Werror` 0 警告、
   base18 **第 28 次结构性免疫**【熔岩只存在于场景（桶/放置），fresh
   世界零熔岩零作物 = 无输入零活动】；E2E 场景池 = seed-9 天然 2×1 空穴
   + 3 石块编辑围墙【存档禁止同格 block+fluid 编辑，PITS 已收录】；
   P53 = 作物放置限制（用户"按你建议
   继续：候选1"，代理定案最小完整版 = `plant_on` 数据驱动放置土壤门，
   消费 P48/P50 遗留"放置不受限"，熔岩后续/多掉落内容扩展留后续）：
   **`plant_on` 纯数据**（block 可选**数组**（方块名）——放置时目标格
   **正下方**须为列表之一；缺省 = 不限 = 通用 placeBlock 规则；空数组
   no-op 拒绝【ammo 纪律】、逐条目点名 `plant_on[i]` 拒绝）+ **门 = 纯
   函数 `canPlantAt`**（`world/raycast`，`canPlaceAt` 同款可单测；下方
   越界读 air → 拒）+ `placeBlock` 在 canPlaceAt 后加门（土壤不符 =
   静默 no-op，不消耗物品）+ **init 每条目存在性校验**（grows_on 纪律）
   + **`plant_on`（放置门）与 `grows_on`（生长门）相互独立**（本项目同值
   farmland = Minecraft 耕地约束落地）+ 内容：`wheat_1..6` 加
   `plant_on: ["farmland"]`。**无 format 变更**（format 9）；954/954
   （+12：block 解析 6 + canPlantAt 6）、`-Werror` 0 警告、base18 **第
   27 次结构性免疫**【plant_on 只影响放置，fresh 世界零放置 = 无输入零
   活动】；E2E 场景选位 = p53scan 探针调真实 `cropGrowthSteps` 预计算
   【选 [0,240) 无生长通过帧的格 = fps 无关断言，见 PITS P53 条】；
   P52 = 加权多掉落列表（用户"按候选顺序
   继续后面两个 phase" = 候选 2，代理定案最小完整版 = `chance_drops`
   多条目独立 roll + grass 改 MC 语义，放置限制/熔岩后续留后续）：
   **`chance_drops` 纯数据**（block 可选数组，元素 = 与 P49 单条目
   `chance_drop` **相同的严格三元组** `{item,count,chance}`；与
   `chance_drop` **互斥**【同时写 = 解析拒绝】；空数组/非对象元素/未知键/
   缺三元组/越界逐条目点名 `chance_drops[i]` 拒绝）+ **每条目独立
   roll**（`rollChanceDropEntry(seed,x,y,z,entryIndex,chance)` 纯函数：
   列表专用 base salt + 条目索引 golden-ratio 推进 + splitmix64 finalize，
   同格各条目互不追踪、也不追踪单条目 roll；per-cell 确定性 = P49 同款
   刻意代价）+ breakBlock 循环条目、每条目至多 1 掉落实体 + init 每条目
   复用 drops 纪律（物品存在 + count ≤ max_stack，点名 `chance_drops[i]`）
   + 内容：`grass` 改 **MC 语义**（`drops: wheat_1` → `drops: dirt` +
   `chance_drops: [wheat_1 ×1 @ 0.125]` = 草掉泥土 + 偶尔种子，0.125 =
   MC 基础概率；种子来源 = 草概率掉 + 未熟作物掉落）；`leaves` 保留单条目
   `chance_drop` = 两字段并存示范。**无 format 变更**（format 9，掉落 =
   实体走既有 `entities`）；942/942（+21：block 解析 13 + entry roll 8）、
   `-Werror` 0 警告、base18 **第 26 次结构性免疫**【grass drops 变数组不改
   worldgen/atlas = 无输入零活动】；
   P51 = 熔岩流体系统（用户"按候选顺序继续后面两个 phase" = 候选 1，
   代理定案最小完整版 = 熔岩第二流体种类 + 水/熔岩反应 + 熔岩桶 + format
   9，加权多掉落留 P52）：chunk 流体字节双种类（0=干 / 1..8=水 / 9..16=
   熔岩）+ FluidSim 反应 pass（水/熔岩 6-邻 → obsidian/stone）+ 桶双种类
   化 + format 9 + lava/obsidian/lava_bucket 内容；921/921（+12）、第 25 次
   结构性免疫；
   P50 = 耕地·水·光照生长条件（用户"按建议继续下两个阶段" = 建议 2，
   代理定案最小完整版 = `farmland` 方块【dirt 合成】+ 生长引擎三道数据
   驱动环境门，加权多掉落/熔岩/放置限制留后续）：**作物侧三个可选环境
   字段**（缺省 = 无要求 = 旧行为）
   ——`grows_on`（作物**下方一格**须为该土壤，空 = 无要求，init 硬校验
   指向存在方块 = grows_to 纪律）/ `growth_light`（作物格
   max(天空光,方块光) 最低值 [0,15]，0 = 无要求）/ `dry_slowdown`
   （无水时 roll 模数倍数 [1,64]，1 = 无减速）+ 湿润判定（土壤格同 y
   切比雪夫半径 4 内任一流体 level ≥1；模数 = `growth_ticks` × (湿 1 /
   干 `dry_slowdown`)，干时通过帧 = 湿时严格子集）+ 内容：`farmland`
   （solid cube、hardness 0.5、drops 回 `dirt`、顶面犁地垄纹）+ 配方
   `farmland`（shapeless 1 dirt）+ `wheat_1..6` 加 farmland/9/2（MC 值）；
   **无 format 变更**（format 8）；
   P49 = 金苹果（概率掉落 + 吃时给效果；
   用户"按建议继续下两个阶段" = 建议 1，代理定案最小完整版 = 概率次级
   掉落系统 + 苹果 + 金苹果，耕地/多掉落留后续）：**概率次级掉落**
   （block `chance_drop` `{item,count,chance}` 严格三元组，与 `drops`
   独立【一次破坏最多两个掉落实体】；是否掉出 = `rollChanceDrop(seed,
   x,y,z,chance)` 确定性纯函数 `block/chance_drop`——位置哈希 + 专用 salt，
   同世界同格每次破坏同果）+ **吃时给效果**（item `grants_effect`
   `{name,duration}` 严格配对，**仅食物**【可饮用清效果、通道互斥】；
   进食通道完整吃下时经 P46 `StatusEffects::apply` 施加，与 mob
   `on_hit_effect` 同路径）+ init 双硬校验（chance_drop 物品存在 +
   max_stack / grants_effect 效果名注册）+ 内容：`leaves` 1/128 掉
   `apple` + `apple`（`restore_hunger` 4 = MC）+ `golden_apple`
   （`restore_hunger` 4 + 吃给 regeneration 10 s）+ 配方
   `golden_apple`（shaped GGG/GAG/GGG，MC 镶金）+ 2 图标；**无 format
   变更**（format 8：实体走既有 `entities`、效果走既有 `player.effects`）；
   P48 = 面包（小麦种植系统 v1；用户
   "继续建议1" = 第一候选，代理定案最小完整版 = 作物 + 生长引擎 + 收获 +
   面包配方/食物，金苹果/熔岩留后续）：**生长纯数据**（block `grows_to`
   下一阶 + `growth_ticks` 平均 tick 进一阶，严格配对 + init 硬校验指向
   存在方块；**生长状态 = 方块 id 本身**，无 per-instance 状态/存档字段）
   + 生长引擎（`world/growth.{hpp,cpp}` 纯函数 + `Game::m_crops` map
   位置序，setBlock 收口维护 + 读档后 `rescanCrops` 全量重建；每帧 1/60
   固定步长确定性哈希 roll）+ 内容：7 方块 `wheat_1`→…→`wheat_7`
   （cross，未熟掉种子可再种 / 成熟掉 `wheat`×2）+ `wheat`/`bread`
   （`restore_hunger` 5）+ 配方 `bread`（shapeless 3 wheat，同物品合并
   count）+ 7 atlas tile（空位无 UV 移位）+ 2 图标 + 7 cross 纹理 +
   **grass drops 改 `wheat_1`**（v1 种子来源）；**无 format 变更**
   （format 8，作物 = 方块编辑）；P47 = 水桶（流体桶 v1；用户"按排序
  继续"= 第一候选，代理定案最小完整版 = 水，熔岩/面包/金苹果留后续）：
  **桶对纯数据**（item `collects_fluid`+`collects_becomes` 空桶侧 /
  `pours_fluid`+`pours_becomes` 满桶侧；流体名 v1 词汇表 = 仅 "water"；
  两侧互斥 + 与食物/可饮用互斥；**init 对偶对称硬校验** = 伴侣存在且
  反向指回）+ `raycastFluid`（桶目标射线：首个「非 air 方块或带流体」
  格，**墙壁遮挡** = MC；与 raycastBlock 共享 DDA 骨架）+
  `tryFluidBucket`（舀 = 命中 **level 8 源** → 移除源 + 换物品 +
  "splash"；倒 = 放置目标格 air 无流体不重叠玩家 → 放 L8 源 + 换物品；
  右键顺序在 interact 之后 = 手持桶对炉子仍开面板；倒下的源走正常
  FluidSim）+ 内容：`water_bucket` + 蓝水面桶图标 + `splash.wav`；
  **无 format 变更**（format 8）；P46 = 状态效果系统（代理自定案范围：
  poison/weakness/regeneration 数据驱动效果 + **spider** 毒源 + 效果图标
  HUD + 牛奶清负面补全 + 存档 format 8）：**效果纯数据**（`assets/data/
  effects/*.json` 严格解析 + no-op 拒绝；真实 dt 倒计时 + 跨 interval
  tick、min_health 地板、melee_penalty、negative 标志）+ **spider**
  （首个 `on_hit_effect` mob：近战命中施加 poison 15 s、只在黑暗、
  无掉落）+ 效果图标行（心行左对齐 x0=434、仅 locked && !panelOpen、
  无效果不绘制 = 结构性免疫）+ 牛奶 = 清红闪 + **清全部负面效果**
  （正面 regen 保留 = MC 语义）+ format 8（`player.effects` 名字序、
  tick 相位不保存、`parseSave` 加 `EffectRegistry` 第 4 参）；P45 =
  染色补全 + 十字植物渲染（P41/P12/
  P20/P23 遗留"无蓝/绿/紫来源"）：**颜色词汇表纯数据化**（dye item
  `dye_color` 命名，加颜色**零代码**改动）+ 补 6 色 blue/green/purple/
  magenta/light_blue/cyan（项目 silver 位 = MC light_gray → **15 染料全**）
  + `BlockModel::Shape::Cross`（2 交叉竖直面片，透明 pass 双面、无邻居
  剔除、自身格光、面 shade 1.0）+ biome `decorations` 表面装饰（树的格网
  + 专用 salt + 每(格,条目)独立 roll，落表面格正上方、洞穴啃掉不放、
  树干让位）+ 内容全 JSON：5 方块（`lapis_ore`/`cactus`/`allium`/`tulip`/
  `cornflower`）+ `lapis` 物品 + 6 染料 + 6 wool 变体 + 6 配方（青←lapis+绿
  MC 1.14+）+ 11 atlas tile（空位，无 UV 移位）+ lapis 矿脉；P44 =
  per-block 炉子状态（P40 遗留"单个全局 FurnaceState"）：`Game` 每炉子块
  独立 `FurnaceState`（`std::map<BlockPos, FurnaceState>` 位置序）+
  setBlock 收口创建/删除 + 面板绑定所点方块 + 存档 format 7（`furnaces`
  双向交叉校验 + format 6 迁移：sole 归属/其余丢弃）；P40 = 炉子/熔炼
  （P28 遗留"熟肉"）、P41 = wool 染色框架（P12/P20 遗留"wool 无用途/无
  染色"；atlas 4×8→8×8 → base18 于 P41 重锚）、P42 = 弓后续 2、P43 =
  牛奶桶最小项（饮用通道 + 挤奶）。
- 测试 **1109/1109**，`-Wall -Wextra -Wpedantic -Werror` 恒 0 警告。
- 无输入基线门：`/tmp/opencode/base18.png`（字节一致；P41 重锚【atlas
  8×8】、P59 重锚【atlas 16×8】，P42/P43/P44/P45/P46/P47/P48/P49/P50/P51/P52/P53/P54/P55/P56
  /P57/P58 过门 = 第 16/17/18/19/20/21/22/23/24/25/26/27/28/29/30/31/
  32 次结构性免疫、无重锚；P59 重锚；P60/P61/P62/P63 过门 = 第
  33/34/35/36 次结构性免疫、无重锚，第 37/38/39/40/41/42/43 次过门 =
  Phase 64/65/66/67/68/69/70、第 44 次过门 = Phase 71、第 45 次过门 = Phase 72、第 46 次过门 = Phase 73、第 47 次过门 = Phase 74、第 48 次过门 = Phase 75、第 49 次过门 = Phase 76、第 50 次过门 = Phase 77、第 51 次过门 = Phase 78）。
- 活跃已知问题：`docs/KNOWN_ISSUES.md`；下一阶段候选：`SESSION_CONTEXT.md`「下一步」。

## 命令

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build  # 构建（0 警告）
./build/bin/tokenmine_tests                                                 # 测试（964）
./build/bin/tokenmine                                                       # 运行（ESC 退出）
./build/bin/tokenmine --frames N [--screenshot /abs.png] [--save slot] [--new] [--config path]
```

游戏必须从仓库 cwd 启动（资源相对路径）；独立测试程序链接
`build/lib/libmc_core.a -lSDL3 -lGLEW -lGL -lz -lpthread`。

## 硬性规则 Top 10（违反即 bug；全文 docs/PROTOCOL.md）

1. **坐标**：world/chunk/local 严格分离，换算只在 `coordinates.hpp`；负坐标 `floorDiv`。
2. **渲染隔离**：游戏逻辑不碰 GL/SDL（renderer/ 与 window/input 边界除外）。
3. **数据驱动**：核心不 switch 方块 id（读 properties）；air 是代码哨兵（无 air.json）。
4. **错误与代码质量**：`Result<T>` + `MC_TRY` 显式传播；**无 TODO/伪代码/魔法数字/
   硬编码方块检查/删功能过构建/禁用测试**。
5. **相机**：yaw 0 = 朝 -Z、正 = 左转；pitch 正 = 上、clamp ±~89°；**鼠标转向仅在
   指针锁定时生效**（无输入运行字节复现的保证）。
6. **玩家物理**：position = AABB 底面中心；重叠判定**严格不等式**（无 epsilon）；
   防穿透 = 子步化（每子步 ≤0.4 块、≤64 子步）+ 终端速度 38。
7. **chunk streaming**：worker job 只在 **post→flush 窗口**运行（窗口内主线程只读
   World）；**光照 job 每帧至多 1 个**（相邻列区域重叠 ±16）。
8. **确定性**：昼夜/Mob/箭/粒子 = 固定 1/60 帧步长；生存计时器（饥饿/进食/破坏/
   坠落/实体 age）= 真实 dt；截图只在帧边界字节一致。
9. **面表绕序**有回归测试守护（`FaceWindingIsCounterClockwiseFromOutside`），勿改
   顶点序；光照 = `max(sky,block)/15 × faceShade`（顶 1.0/±X 0.8/±Z 0.6/底 0.5）。
10. **出生点**：4×4 chunk 演示区列，按 `kSpawnCandidates` 自动找干燥陆地
    （h > seaLevel 且出生 AABB 内无 solid）。

## E2E 协议（摘要；全文 docs/PROTOCOL.md「E2E 协议」）

- **零并发 :0 活动**（用户手离开鼠标）+ 每拍截图前 `unmap_covers`。
- **无输入运行**：×2 全帧字节一致 + skygate（FRESH 模式色）+ base 门
  `/tmp/opencode/base18.png` 字节一致（新系统需论证"无输入下零活动"= 结构性免疫）。
- **输入驱动运行（xdotool 脚本）**：**存档状态（含 yaw/pitch）是确定性权威证据**——
  全帧字节一致受 SDL 指针锁定 grab delta 环境限制（一次性相机旋转 ±0.01 rad = 4 px；
  缓解 = warp 实际原点 + (632,352)、同批同 warp、手不动；存档 yaw/pitch = 漂移探测器）。
- 输入锚定窗口发现时刻，D ∈ [0.1, 0.5] s；tick 类断言需 ≥1.6 s 裕量（1/60 双累加
  边界：36 s 饥饿 tick = 帧 2161，20 s 饥饿伤害 = 帧 1200，1.6 s 进食通道完成 = 帧 97）。
- **HUD 红心检查**（心带 y[646,661] x[551,729]）：无输入运行 = 0（>50 = 污染废拍）；
  输入运行 >50（输入生效）。

## 阶段流程（每个 phase 按此顺序）

1. 用户确认方向 → 2. 实现（数据驱动、无 TODO）→ 3. 构建 + 全量测试（`-Werror`）→
4. E2E（base 门 + 回归对 + 场景专属验证，按上面协议）→ 5. 文档（ROADMAP 行、
KNOWN_ISSUES、SESSION_CONTEXT）→ 6. **显式路径列表** git commit（永不 `git add -A`；
永不提交 `SESSION_CONTEXT.md`（本地））→
7. 中文阶段报告（标准结构：实现内容/变更文件/测试/构建与验证结果/已知问题/下一步建议）。

## 坑位 Top 8（全文 docs/PITS.md，188 条）

1. **SDL grab delta**（P28）：输入 E2E 全帧字节一致不保证——存档 yaw/pitch 为漂移
   探测器；驱动 warp 用实际窗口原点（实测 (4,64)）+ (632,352)。
2. **物理光标**（P5）：指针未锁定时 SDL 仍投递 mouse motion → 无输入 E2E 必须零
   :0 活动，红心检查 = 污染哨兵。
3. **粒子视觉验证视角**（P29）：直下视角可视锥不含破坏粒子——必须斜视角（pitch
   −1.02 级）；帧差分析先排除已知 UI 变量（饥饿/HP 图标也在 diff 里）。
4. **splitmix64 是纯 finalizer**（P29）：不推进 state——推进式流必须
   `state += 0x9E3779B97F4A7C15ULL` 后再 mix（同 burst 同值缺陷根因）。
5. **`std::string::replace(pos,len,const char*,n)` 截断**（P28）：只取替换串前 n 字符。
6. **`--save slot` 第二次运行会加载第一次写的存档**（P15）：E2E 对跑每拍从源副本
   `cp` 新鲜存档。
7. **按住破坏会自动续破下一块**（P15）：输入脚本的释放时机 = 破坏完成 + 余量，
   余量内的按住对下一块产生确定性裂纹进度。
8. **像素验证**：`glReadPixels` 必须在 `swapBuffers()` 后（前缓冲）；
   `magick ... txt:` 输出 `(r,g,b,a)` 开头 `(` 破坏数值转换，awk 取 `$2`。

## 禁止事项

- `config/config.json` 是**已提交文件**（P32 起：用户调值 view_distance 80 /
  day_length 600 + `player` 默认节）——**不还原用户的调值**；内容改动仅在用户
  要求时做，提交用显式路径。
- **永不 `git add -A`**——提交用显式路径列表。
- `SESSION_CONTEXT.md` 本地专用（`.git/info/exclude`），永不提交。
- 无 TODO/伪代码/硬编码检查/魔法数字；新内容一律走数据文件 + 注册表严格校验。

## 文档地图

| 文档 | 内容 | 何时读 |
|---|---|---|
| [docs/GAMEPLAY.md](docs/GAMEPLAY.md) | 玩法指南（操作/生存循环/战斗与食物/工具路线/昼夜） | 玩法问题 |
| [docs/MECHANICS.md](docs/MECHANICS.md) | 机制详解（截至 P28：生存/Mob/弓/合成/UI/世界生成） | 改玩法逻辑前 |
| [docs/CONTENT_GUIDE.md](docs/CONTENT_GUIDE.md) | 数据驱动内容添加（biome/方块/配方/Mob/hotbar）+ 存档使用 + worldgen 参数 | 加内容/调世界 |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | 目录结构 + 核心架构（模块/线程池/存档格式/帧管线） | 改架构前 |
| [docs/PROTOCOL.md](docs/PROTOCOL.md) | 开发协议：命令/技术栈/硬性约定/测试/**E2E 协议** | 写代码与验证前 |
| [docs/PITS.md](docs/PITS.md) | 坑位合集（188 条，都踩过） | 遇到怪问题 |
| [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md) | 已知问题清单（活跃）+ 已修复存档索引 | 阶段验收前后 |
| [docs/ROADMAP.md](docs/ROADMAP.md) | Phase 0–29 规划 + 细节存档索引 | 了解历史 |
| [docs/archive/phase-NN.md](docs/archive/) | 单阶段细节存档（阶段报告 + 会话存档） | 深挖某阶段 |
| `SESSION_CONTEXT.md`（本地，不入库） | 会话交接：最新状态/提交列表/下一步 | 任何"继续"会话 |
