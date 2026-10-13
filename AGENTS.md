# AGENTS.md — 代理快速接收

> Phase 30 文档整理产物。本文件是代理接手本项目的**第一入口**：读完本页即可开工，
> 细节按「文档地图」按需加载。所有规则原文见 `docs/PROTOCOL.md`。

## 项目一句话

可运行、可扩展的 Minecraft 风格 3D 体素沙盒：C++20 / SDL3 / OpenGL 3.3 core，
自研 `Result`/JSON/PNG，**内容全数据驱动**（加方块/物品/配方/Mob = JSON + 纹理，
不改核心代码），**确定性是一级要求**（无输入运行逐帧字节可复现）。

## 现状（Phase 65 完成时）

- 功能阶段 P0–P65 全部完成并提交；P65 = 火把内容：torch 方块（用户"按建议顺序，继续完成下两个" = 建议 2，代理定案最小完整版 = **torch 方块**：可放置光源，光 14 = MC、cross、瞬破、掉自己；coal+stick 配方 = coal 首个合成用途；campfire/火蔓延超出最小内容范围【见 KNOWN_ISSUES 火把 v1 条】）：**`torch` 块**（cross + 火把纹理 atlas 空位 [11,0] 免重锚、`light_emission 14` = MC、瞬破、**drops torch** = 可复用、非 flamable = 光源不是火源）+ **配方 shapeless coal+stick → 4**（MC，coal 首个合成用途）+ **bug 修复：破坏 cross 块崩溃**（P45 潜在：`drawParticles` 粒子纹理查 `cubeFaceTexture`（top/side/bottom 槽）对 cross 模型 = 空名 → `AtlasLayout::uvMin` 抛 `unordered_map::at`；火把破坏 E2E 首次暴露 + gdb 定位；修复 = cross 分支，粒子立方体六面全用 `cross` 纹理，与 item_mesh/chunk_mesher 两分支同款）；**无 format 变更**（format 10，torch = 块编辑）；1036/1036（+2）、`-Werror` 0 警告、base18 **第 38 次结构性免疫**【torch 永不生成 + 只经合成、默认背包无 + atlas 空位无 UV 移位】；E2E 全绿（S2 放置【石块锚点顶面瞄准，torch 立顶面 + 物品消耗 + fires []【火把非 fire 块：无寿命条目】，×2 RAW】/ S3 夜间渲染+光【午夜 600f，A≠B + 火焰像素 + ×2 各】/ S4 破坏 → 掉 torch 物品【**左键按住** = 破坏、hardness 0 首帧即破、掉落物停锚点上，×2 savenorm】/ S2ctl 无输入 ×2 RAW / S6 init 失败 ×2【light_emission 16 越界点名 / 配方原料未知点名】；**PITS +3**【瞄准 pitch 正 = 朝上 / 破坏 = 左键按住、右键 = 放置交互链 / awesomeWM float toggle 是 TOGGLE + 60f 跑 ~1.85 s 预算】）；P64 = 火视觉 v1：fire 方块（用户"按建议顺序，继续完成下两个" = 建议 1，代理定案最小完整版 = **fire 方块**：可燃物烧成发光火焰而非 air，定时燃尽、水浇灭，**format 10**；粒子/蔓延超出最小范围【见 KNOWN_ISSUES 火 v1 条】）：**`fire` 块**（cross + 火焰纹理 atlas 空位 [10,0] 免重锚、`light_emission 15` = MC、瞬破、**无 drops**、非 flamable = 燃烧终态）+ **燃烧目标改 fire**（点燃器 P60 + 熔岩燃烧 pass P58；registry 无 fire 块回落 air = 单测 fixture 语义）+ **`Game::m_fires` 剩余寿命 map**（setBlock 收口创建/删除 = 炉子范式；real-dt 倒计时；熔岩 pass 走 changed 列表补注册）+ **水浇灭**（6-邻水 → 瞬灭；水胜火；熔岩不浇灭）+ **config `fire.lifetime_seconds`**（默认 4.0，>0 硬校验）+ **init 交叉校验**（有 flamable 块 ⇒ 必须有 fire 块，点名拒绝）+ **format 10 `fires`**（{pos, remaining} 位置序、remaining > 0、与块编辑双向交叉校验 = 炉子键纪律、format 9 迁移 = 空 + 手编旧文件补满寿命）；**无 worldgen 变更**；1034/1034（+11）、`-Werror` 0 警告、base18 **第 37 次结构性免疫**【fire 永不生成 + fresh 零熔岩 = 燃烧 pass 惰性 + 默认背包无点燃器 + atlas 空位无 UV 移位】；E2E 全绿（S2 点燃器 → **fire 活体终态**【MOV 300 = P57 模式，remaining ∈ (250,300]，×2 savenorm 剥 fires remaining】/ S3 燃尽【场景 remaining 4.0 + 600f = 10..26.1 s > 4.0 = fps 无关终态 air，×2 RAW】/ S4 水浇灭【**高架水** = 石台 L8 + 四侧石封边 + 火正上方——首轮裸 L8 水源横向漫灌高原【**水 flow 7 vs 熔岩 1，PITS】**+ (23,45,−5) 生成装饰格非 air，×2 RAW】/ S5 夜间渲染+光【**600f**——60f `--save` 跑可视 chunk 流送未竟 = 地面缺失 worker 竞态【PITS】，A≠B + 火焰像素 + ×2 各 + pair 重试【8px WM 黑条环境性】】/ S2ctl 无输入 ×2 RAW / S6 init 失败 ×2【删 fire.json → 交叉校验点名 / lifetime 0 → config 校验】；**PITS +2**）；P63 = flamable 内容扩展：MC 天然可燃三块（用户"先建议2,再建议1" = 建议 1，代理定案最小完整版 = 纯数据 `flamable` 扩展，**零核心代码**；火视觉/引燃 Mob 超出范围【见 KNOWN_ISSUES】）：**wood/leaves/grass** 加 `flamable: true`（与 P58 的 wheat_1..7 合 = 自然世界可燃面；点燃器 P60 与熔岩燃烧 pass P58 两个消费者立即生效）；可燃集**恰 10 块**精确集断言钉死（羊毛仍不燃 = 文档化偏离）；P58 测试负清单契约演进（wood/leaves 移出）；**无 format 变更**（format 9）；1023/1023（+1）、`-Werror` 0 警告、base18 **第 36 次结构性免疫**【JSON 属性无视觉 + fresh 零熔岩 = 燃烧 pass 惰性 + 默认背包无点燃器】；E2E 全绿（S2 点燃器烧 wood【(22,46,−5) 单次右键 → air、无掉落，420f ×2 savenorm】/ S3 熔岩烧 wood【P58 池几何 + **9 石衬砌**——池在草高原凹穴，P63 不衬砌则池底烧穿落层【第一轮】/ L7 草邻野火【第二轮】，PITS +2；240f ×2 RAW：wood → air + L7、池保 L8、零野火】/ S2ctl 无输入 ×2 RAW / S6 init 失败 ×2【flamable "yes"/1 → must be a boolean】）；P62 = 石头工具线 v1：石镐（用户"先建议2,再建议1" = 建议 2，代理定案最小完整版 = 镐路线中档 **stone_pickaxe**，**零核心代码**；项目无斧类工具 → "石斧" = 新工具型超出最小范围【文档化偏离】）：**`stone_pickaxe`**（tool、max_stack 1、`break_speed 3.0` / `attack_damage 2.5` = 项目压缩刻度**恰居中**于木镐 2.0/2 与铁镐 4.0/3；图标 = 共用 pickaxe() 锻灰色调）+ 配方（shaped **CCC/.S./.S.**，C = cobblestone = MC 配方 = **cobblestone 首个工具用途**：挖石→锻石→石镐→更快挖铁 = P61 材料链闭环）；**无 format 变更**（format 9）；1022/1022（+2）、`-Werror` 0 警告、base18 **第 35 次结构性免疫**【工具物品不在默认背包 + 配方只在 UI = 零世界状态】；E2E 全绿（S2 工具实战【stone 放 (22,46,−5) 石镐按住 1.0 s【1.5×1.5/3 = 0.75 s】→ 掉 cobblestone×1，420f ×2 savenorm 字节一致 + save EXACT】/ S2ctl 无输入 ×2 RAW / S6 init 失败 ×2【break_speed "fast" → must be a number / 配方 key typo → "key entry 'C' uses unknown item"】）；P61 = stone→cobblestone 材料链（用户"下两个 phase，请继续建议1 和建议2" = 建议 2，代理定案最小完整版 = cobblestone 块 + stone 自掉变更【**内容契约变更**】+ 4 锻石配方，MC 材料循环，**零核心代码**）：**`cobblestone`**（hardness 2.0 = MC、自掉、纹理 = stone 底 + 深色裂纹缝）+ **stone.json `drops: stone → cobblestone`**（挖天然石得锻石；stone 只能经合成回来）+ 配方 `stone`（shapeless 4 cobblestone → 1 stone = MC 1.14+）+ **atlas 空位 [9,0]**（65→66/128、grid 不变 = 无 UV 移位 = **无重锚**）；cobblestone 不能熔炼回 stone【block item 无 item 数据，文档化偏离】；契约变更安全 = 调研审计（无配方以 stone 为原料、只有 stone.json 掉 stone）；**无 format 变更**（format 9）；1020/1020（+2）、`-Werror` 0 警告、base18 **第 34 次结构性免疫**【空位 tile 无 UV 移位 + drops 变更只在破坏时生效】；E2E 全绿（S2 掉落变更【stone 放 (22,46,−5) 铁镐按住 1.0 s → 掉 **cobblestone×1 而非 stone×1**【(22.5,45,−4.5) 落地 = P59 证明位置】，420f ×2 savenorm 字节一致 + save EXACT】/ S2ctl 无输入 ×2 RAW / S6 init 失败 ×2【cobblestone.json 删除 → "block 'stone' drops unknown item" = 契约依赖方向钉死 / 配方 typo → "uses unknown ingredient item"】）；P60 = flint 用途：箭头 + 燧石与钢（用户"下两个 phase，请继续建议1 和建议2" = 建议 1，代理定案最小完整版 = MC 1.14+ 箭头配方【**替换** P24 两木棍配方 = 内容契约变更】+ flint_and_steel + 点燃交互，消费 P59 遗留"flint v1 无用途"）：**箭头配方替换**（shapeless flint+stick+feather → 4 箭 = MC 1.14+；flint 成真材料：gravel→flint、chicken→feather、树→stick = MC 闭环）+ **`flint_and_steel`**（material、64 叠、图标 = 钢杆 + 燧石尖）+ 配方（flint+iron_ingot）+ **物品字段 `ignites_flamable`**（严格 bool、缺省 false、非 bool 点名拒绝；与**五个右键通道互斥** food/drink/launcher/dye/fluid bucket = 通道所有权纪律；init 白名单 = P58 教训）+ **右键链点燃分支**（interact/bucket 之后、placeBlock 之前：目标 `flamable` 块 → setBlock(air) **无掉落**【P58 熔岩烧语义】、**不消耗**【MC：燧石与钢用不坏】；炉子/工作台仍开面板；非 flamable fall through）+ **`ignite` 声音**；**无 format 变更**（format 9）；1018/1018（+8）、`-Werror` 0 警告、base18 **第 33 次结构性免疫**【无 block / 无 worldgen / 默认场景纹理零改动 = P59 重锚后首次过门、**无重锚**】；E2E 全绿（S2 点燃【wheat_3 放 (22,46,−5) P52 几何、瞄准锚定存档 = 零鼠标移动、单次右键 → 块烧成生成 air【diff = {}】**零掉落**【entities = []，破坏会掉 wheat_1 种子 = 可区分】，420f ×2 savenorm 字节一致 + save EXACT；下方草 ≠ farmland = 生长门失败 = 场景全静态】/ S2b 非可燃隔离【同格 stone、同物品、同点击 → stone 保留】/ S2ctl 无输入 ×2 RAW / S6 init 失败 ×2）；P59 = 多掉落内容再扩展：gravel + flint（用户"按你的建议继续下两个phase" = 建议 2，代理定案最小完整版 = gravel worldgen 矿脉 + flint 物品 + gravel 10% 概率掉落 + 内容钉死单测，**零核心代码**）：**`gravel`**（hardness 0.6、自掉 + `chance_drop: {flint, 1, 0.1}` = MC 值）+ **`flint` 物品**（material、64 叠、图标 = 3 片碎块）+ worldgen `ores.veins` + gravel（probability 0.25 / vein 4 / max_y 80 = 常见石替换脉）+ **atlas 8×8 → 16×8**【64/64 已满、gravel = 第 65 tile → 所有既有 UV 移位 → base18 重锚（P41 先例）】；flint v1 无用途、排除 stone→cobblestone 链（文档化偏离）；**无 format 变更**（format 9）；1010/1010（+3）、`-Werror` 0 警告、base18 **重锚**【atlas 扩容移位所有 UV，第 33 次过门 = Phase 60】；E2E 全绿（S2 flint 掉落【roll-PASS 格 (21,46,−1)、yaw=π/4 瞄准锚定存档 = 零鼠标、场景 diff 清 3 角点格【45° 射线沿格网角点穿过 → DDA tie-break 命中对角固块，PITS】：gravel×1 + flint×1，×2 savenorm 字节一致】/ S2b 无 flint 对照【roll-FAIL 格 (22,46,−5)：只掉 gravel = per-cell 确定性 roll 隔离】/ S2ctl 无输入 ×2 RAW / S6 init 失败 ×2）；P58 = 火系统 v2：熔岩烧 Mob 与作物（用户"按你的建议继续下两个phase" = 建议 1，代理定案最小完整版 = Mob 点燃 + 熔岩烧作物，**全量消费** P54 遗留"熔岩不烧 Mob 与作物"）：**块属性 `flamable`**（可选 bool 缺省 false，非 bool 点名拒绝；内容 = wheat_1..7 全 7 阶）+ **FluidSim 燃烧 pass**（扫所有 LAVA 格 6-邻 flammable BLOCK 格 → 收集排序后 setBlock(air)，**无掉落**，快照确定性，注册表无 flamable 块 = fast-exit）+ **Mob 点燃**（生成时 `armFire` 从 ignites_on_lava 效果 + config `fire_duration_seconds` 取参；**整数帧燃烧格**【0.2 s tick 的 double 累加实测相位 off-by-one，改 int 帧计数】：1 HP/0.2 s = 5 HP/s 可致死、熔岩内每帧重置 duration【相位保留】、离熔岩烧 8 s 残留、水瞬时熄灭且先于 tick 【水胜熔岩】）+ **init 交叉校验**（ignites 效果 tick_interval×60 必须整帧数，容差 1e-4 帧——float 0.2f×60=12.00000018 过、0.33 s 拒） + `Entity::overlapsFluid` 自 player.cpp 上移 + 顺手修复 P56 `grants_effects` 白名单一行漏列；**无 format 变更**（format 9）；1007/1007（+18）、`-Werror` 0 警告、base18 **第 32 次结构性免疫** 【fresh 世界零熔岩（P54）、零 flammable 块（wheat 只玩家种）、60f 内零 mob = 两条燃烧路径皆惰性】；E2E 全绿（S2 熔岩膜【3×3 level-1 膜铺地面：走过即燃，10 只被动 mob 烧死 = 18 item 实体，膜光 15 挡 hostile 光门 → 玩家 20 HP 恒定，2400f ×2 savenorm 字节一致】/ S2ctl 水膜对照【hostile 杀玩家 health 0、零点燃 = 隔离熔岩】/ S4 作物燃烧【wheat_3 贴池 → air、池 +1 L7、×2 RAW 字节】/ S6 init 失败 ×2）；P57 = 脱离熔岩残留火：火系统 v1
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
- 测试 **1036/1036**，`-Wall -Wextra -Wpedantic -Werror` 恒 0 警告。
- 无输入基线门：`/tmp/opencode/base18.png`（字节一致；P41 重锚【atlas
  8×8】、P59 重锚【atlas 16×8】，P42/P43/P44/P45/P46/P47/P48/P49/P50/P51/P52/P53/P54/P55/P56
  /P57/P58 过门 = 第 16/17/18/19/20/21/22/23/24/25/26/27/28/29/30/31/
  32 次结构性免疫、无重锚；P59 重锚；P60/P61/P62/P63 过门 = 第
  33/34/35/36 次结构性免疫、无重锚，第 37/38 次过门 = Phase 64/65）。
- 活跃已知问题：`docs/KNOWN_ISSUES.md`；下一阶段候选：`SESSION_CONTEXT.md`「下一步」。

## 命令

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build  # 构建（0 警告）
./build/bin/minecraft_tests                                                 # 测试（964）
./build/bin/minecraft                                                       # 运行（ESC 退出）
./build/bin/minecraft --frames N [--screenshot /abs.png] [--save slot] [--new] [--config path]
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

## 坑位 Top 8（全文 docs/PITS.md，~108 条）

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
| [docs/PITS.md](docs/PITS.md) | 坑位合集（~108 条，都踩过） | 遇到怪问题 |
| [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md) | 已知问题清单（活跃）+ 已修复存档索引 | 阶段验收前后 |
| [docs/ROADMAP.md](docs/ROADMAP.md) | Phase 0–29 规划 + 细节存档索引 | 了解历史 |
| [docs/archive/phase-NN.md](docs/archive/) | 单阶段细节存档（阶段报告 + 会话存档） | 深挖某阶段 |
| `SESSION_CONTEXT.md`（本地，不入库） | 会话交接：最新状态/提交列表/下一步 | 任何"继续"会话 |
