# 版本历史（CHANGELOG）

> 本文件只留 **3.x 时代**（「世界扩展」期，v3.0.0 起）逐版明细。
> 2.x 全史（v2.19.0→v2.0.0，两批 20 版）→ `docs/archive/v2x/
> CHANGELOG.md`；v1.x（v1.50.0→v1.0）→ `docs/archive/v1x/CHANGELOG.md`；
> v0.x → `docs/archive/v0x/CHANGELOG.md`。逐版细节（机制/证据/偏离/
> 性能 A/B）见对应时代 `docs/archive/*/phase-*.md` 阶段存档。
> README 只保留里程碑版本（v0.1.0 首个公开版 / v1.0.0 首个正式版 /
> v2.0.0 1.x 收官 / v3.0.0 3.x 开启）。
> 版本命名注：3.x「世界扩展」期 11-04 曾命名 4.x（2.x 第二批收官日
> 晋升定案），11-04 同日用户重编号定案改 3.x（2.x 已吸收原 3.x「系
> 统扩展」八项，回归自然序号）——任务号 V3-1~V3-7。

## v3.45.0（2026-11-12）— V3-17c 主菜单：登陆页 + 教学两级（门 235 全 PASS）

**V3-17 相位 C/4**（BACKLOG 规格 ③ 登陆页【本地名字——net HELLO
名字载体消费，singleplayer 可选】④ 教学页【V3-16 demo 族宿主：分类
→ demo 两级 + mode 上下文过滤 +「需联机」徽章】；**零字体**——17c
词表全在 457 表，8 缺字靠措辞规避）：标题→单人 = **Login 页**（名
字标题 + 300×40 输入框 +「（可选）」hint + 开始/返回）；**文本输入
生命周期** = m_textInputOn 页入开/页出关（SDL_StartTextInput 仅登
陆期开——创造搜索/聊天同款先例）+ frameText/frameBackspace +
latinOnly 净化 + 12 上限（net HELLO name 载体域）+ m_menuName 默认
"player" 会话态零 format【名字不入存档】；**教学两级** = tier 1
5 章 +「· N 个」计数（init 缓存 m_demoIndex 零额外 IO）+ tier 2 章
内 demo「name · mm:ss」【.dmo 剥除 + scale 1.0 防溢出——门内截图冒
烟捕获】+ coop 行「需联机」badge + **coop 拒收**（mode 检查 = 需联
机消息 + 日志点名，net×demo 互斥同款）；**菜单驱动 demo 启动 =
startDemoFromMenu**【main.cpp --demo headless parse 复刻：
loadDemoFile 严格永不 force + world spec→args + **applyDemoArgs
（init --demo 块原文提取共用单源）**】→ menuExit 2 → buildScene →
runDemoReplay = CLI --demo 位级孪生【门臂 savenorm EXACT 实证】；
mm:ss = --list-demos 逐字公式（total/3600, (total%3600)/60 非
ceil——初稿 ceil 不符门内捕获）；render 三新面 hover 从 uiMouse 逐
帧派生【m_menuHovered 成员移除——循环/渲染零共享帧态】。门 235：
3017/3017（+5）+ S1 重锚 base20 = v3.45.0 标题【×2 逐字节 + 旧锚
diff 101 px 单行版本行有界 + 槽不创建】+ **登陆臂**【6×BackSpace
清默认 + type "gate"：日志 `name 'gate'` = frameText + frameBack-
space 双通路权威 + 存档 ≡ --new savenorm EXACT】+ **教学臂**【菜
单 ×2 savenorm+RAW EXACT + **菜单 ≡ CLI --demo savenorm EXACT =
位级孪生最强形**】+ coop 空章截图 + canary 4×2 新≡旧（tm_v344）
**跨树 RAW EXACT ×4**【零字体门面延续】+ 性能 A/B 同档【median 交
错 + changesApplied 9/3/1 vs 9/2/1【win2 = real-dt 域先例】+ 终档
跨树 savenorm EXACT】；门内捕获 PITS 427 四坑【背景构建竞态 +
旧日志假完成信号 / tier-2 长文件名溢出 / mm:ss 逐字公式 / 清默认名
计数 6 非 4】；详见 `docs/archive/phase-3.45.0.md`

## v3.44.0（2026-11-12）— V3-17b 主菜单：设置页（门 234 全 PASS）

**V3-17 相位 B/4**（BACKLOG 规格 = config 表单化编辑 + 严格读 + 写回
config.json，config 纪律不变）：设置页 = 8 行表单（视图距离 4..64
±4 / 白天长度 60..86400s ±60 / 天气 toggle / 声音 0..10/10 ±1 / 界面
缩放 1.0..4.0x ±0.5 / 全屏 toggle / 窗口宽 640..2560 ±64 / 窗口高
480..1440 ±64）+ 保存钮 + 「部分项重启后生效」hint——**零文本输入**
（数值全 stepper/toggle，SDL 文本面不碰）+ ESC 返回 / Enter 保存
便利；纯布局 + 状态机 = menu_layout.hpp test 孪生（gate xdotool 点击
同源公式）；**唯一运行时生效面 = ui.scale**（m_uiScale 每帧 uiSize()
读——菜单布局实时 reflow = 活体证明；世界启动 buildScene 重读 config
同值；其余行 = 世界启动/下次启动生效——streamRadius/帧锁定步长/
Window::init only）；**写回 = 既存未用的 Config::save**（保存钮
confirm 后整档 dump——Json::Object = std::map → 键序字母归一 + 2-
space 缩进 = 记录偏离，值全保：未动键原样存活；整值出整数 80 非
80.0）；**字体 +3**（屏宽项——设置页词表核证缺口）= 代码点序插入 +
后缀 +1..+3 重格，**atlas 2208 不增长**（69 行 headroom 承接，max
cell 551 = row 68）= 归一化 v-UV 不变 → 门 233 的跨树文本边缘白名单
**不复发**（canary 跨树 RAW 恢复 EXACT ×4 实证，PITS 425④ 一次性门
面验证）；FNV re-pin 6b4fde4b271b467e。门 234：3012/3012（+2
MenuSettingsTest）+ S1 base20 重锚【×2 逐字节 + 旧锚 diff 95 px 单行
= 版本行 v3.43.0→v3.44.0 有界断言——PITS 426②：菜单期 S1 逐版重锚】+
**设置臂**【xdotool 设置→天气关→缩放 2.0（LIVE reflow，点击序 = 布局
稳定形）→保存 @2.0 几何：写回断言 PASS（2 键落盘 + view_distance/
day_length/seed/window/player 全存活）+ a3 臂修改后 config 标题 ×2
RAW EXACT】+ canary 4×2 新≡旧（tm_v343）树内 RAW + savenorm + **跨树
RAW EXACT**×4 + 性能 A/B 同档【median 交错 + changesApplied 9/3/1
逐位 + 终档跨树 savenorm EXACT】；门内捕获 PITS 426 两坑【value 列
中心叠字 / 标题版本串 = S1 逐版重锚】；详见
`docs/archive/phase-3.44.0.md`

## v3.43.0（2026-11-12）— V3-17a 主菜单系统：标题页 + 教学入口 + 字体 +21（门 233 全 PASS）

**11-07 用户实机诉求首相位**（「demo 要分组展示；游戏打开后有设置/
登陆/教学页面」；V3-17 拆 4 相位 17a-d，本相 = 菜单架构 + 标题页 +
3 stub）：交互 boot 进**主菜单**（第 4 循环 runMenuLoop——window+
renderer+UI 资产后、buildScene 前；renderMenu = clear + UiBatch only
禁 render/renderUi【世界未建】；调色板/原语 = renderUi 文件局部
lambda 的拷贝，renderUi 原文零触碰）；标题页 = logo「TokenMine.cpp」
+ 版本行 + 单人/多人/设置/教学/退出 5 项（hover 白条 + click/Enter
激活 + ESC 返回/退出，零新 GameKey）；**单人 = 直接 fresh world-
start（--new 形：config 种子、存档不触碰——m_frameCount 自世界首帧
0 起 = 位级孪生）**；多人/设置 = stub「the later face」（17b/17d）；
**教学 stub = kDemoCategories 5 章标题**（两级 demo 列表 + mode 过滤
+ 需联机 badge = 17c）；**menuEnabled 门** = demo/net/script/--new
绕开（结构免疫根：canary 位级形 + 管理旗 main 即退）；其余世界塑造
旗与菜单组合（种子覆写先入 m_config）；**buildUiAssets = buildScene
头提取幂等**（ui shader + font 纹理 + 双 scale——菜单 init 建一次，
单人启动 buildScene 重入跳过）；菜单退出（无世界）= save 写跳过
（--save 槽不创建）；**字体 +21**（人设教学返主距气声缩窗联创继第大
维远征历协）= 代码点序插入 + 后缀 +1..+21 重格（v3.15.0 于先例；
侦察「尾部追加」否决——glyphUv lower_bound 有序不变量）+ atlas
2112→2208（region V3-10d 起 528/528 满）+ **top-512 红线 131072 px
逐字节恒等**；**S1 重锚 base20**（f60 无输入 = 标题 21900 B——菜单
无输入渲染纯函数【17a 全静态无墙钟入像素】×2 逐字节 + defined mouse
face (10,10) no-hover warp 纪律 + 槽不创建断言）；门内捕获 PITS 425
四坑【beginFrame drawable 双坐标系 / hover 鼠标位置依赖面 / 字体尾部
追加否决 / atlas 长度变 = v-UV 重缩放文本边缘差】；3010/3010（+5
菜单族）+ canary 4×2 新≡旧（tm_v342）树内 RAW + savenorm EXACT +
跨树 RAW = 文本边缘白名单【92-118 px = 0.01%，逐 px 审计 100% 文本
边缘 + 零世界像素，17b 起恢复 EXACT】+ click 臂 单人≡--new
savenorm EXACT + 性能 A/B 同档【changesApplied 9/3/1 逐位 ×2 + 终档
跨树 savenorm EXACT】；详见 `docs/archive/phase-3.43.0.md`

## v3.42.0（2026-11-12）— V3-18 存档扩展名（门 232 全 PASS）

**11-08 用户诉求**（「.json 扩展名体现不出压缩后的 json，与没压缩的
混淆」）；**零 format**（容器 = V3-7 确定性 gzip 不变）/ **零像素**
（纯 IO）/ 零 config：写路径单源 = **`slotWritePath`/`slotReadPath`
路径族**（save_slot.hpp）——新写形 `<slot>.json.gz`（gzip 容器可见
扩展名），读 = **两级探测**（.json.gz 先、legacy .json 后 = **零迁移**
——legacy 槽可读，下次写自动换名温和迁移；V3-7 魔数探测同款先例）；
Game::saveWritePath/saveReadPath 委托（writeSave/autosave/exit 共享
写恒新形，tryLoadSave + demo save-spec 检查走探测）；**SAVE-3 管理
面双扩展名**（list 两形枚举去重 .json.gz 赢 / copy = 探测读 + 新形
写【复制即迁移】/ rename + delete = 消费全槽删孪生）；**demo scene
资产随改**（assets/demos/scenes/ 11 个 committed 资产 git mv →
.json.gz + mk_scene.py 输出新形 + demoSceneReadPath(name, dir) 两级
探测参数化单源 + init 交叉检走探测【门内捕获：初版硬编码 .json 后缀
= 全索引假拒 PITS 424①】）；文案 --help + CONTENT_GUIDE_CONFIG /
ARCHITECTURE / README 存档槽行；gate_compare.py 已魔数探测零改动。
**3005/3005**（+6：SaveSlotTest 5【写形/两级探测/双形枚举去重/复制
重命名迁移含全槽消费/删除全槽】+ V316aDemo ScenePathTwoStageFind +
2 既有断言 re-pin）+ S1 第 113 次【f60 ×2 ≡ base19——纯 IO 零像素】
+ canary 4×2 新≡旧二进制（tm_v341 v3.41.0）RAW+savenorm EXACT + 迁移
臂【新写 .json.gz + legacy 名可读 + 写回自动换名 + list 双形去重 +
copy/delete 全槽】+ scene 臂【guide_mining 11387f 自 .json.gz scene
回放 + --list-demos 交叉检 OK】+ 性能 A/B 同档；详见
`docs/archive/phase-3.42.0.md`

## v3.41.0（2026-11-12）— V3-19 死亡屏计时（门 231 全 PASS）

**11-08 用户报障修复**（「死亡界面存活时间不停增加」；表现层 3 点修，
零 format / 零 config / 零新资产）：根因 = 死亡屏「存活」行活读
m_frameCount——死亡后世界模拟照跑帧继续推进，数字一直涨；其余四行
（采挖/放置/击杀 = 会话计数器、行走 = 物理冻结）天然静止唯此行脱缰。
修 = ① PlayerSession + `int deathFrame = -1`（会话态不存档，V3-13
会话统计同款偏离）② alive→dead 转移块（deathCount++ 旁）
`s.deathFrame = m_frameCount` ③ 死亡屏读改快照
`Player::survivalStatsLine(deathFrame, frameCount)`【**纯函数** =
deathCauseLine 的孪生——cause 行读死亡的 DAMAGE INFO，此行读死亡的
FRAME；-1 回落 = 旧活读位级红线】；测试 = 快照帧独立 + 回落活读 +
整式边界（3600f = 1 分 0 秒）。**门 231**：死亡屏两帧臂【8×8 平台
y84 悬空坠死 @ f≈97 无输入确定性 + day_length 1e7 冻日：新树
f254 vs f434 存活行 crop **AE=0 冻结** vs 旧树（tm_v340）同臂 **AE
43/42 px 爬升复现**【「4 秒→7 秒→10 秒」秒位字形差 = 修复前行为实
证，×2 RAW 同帧协议抓不到跨帧漂移本臂专治】】；2999/2999（+1）+
S1 第 112 次【f60 ×2 ≡ base19——死亡屏只在死时画锚零像素】+ canary
4×2 新≡旧 RAW+savenorm EXACT + 性能 A/B 同档；门内捕获 PITS 423 两
坑【①行 y 名义位 ≠ 渲染 bbox（scale-2.0 标题行步下推，crop 须实测）
②两帧臂日冻结 = day_length 1e7】；详见 `docs/archive/phase-3.41.0.md`

## v3.40.0（2026-11-12）— V3-16h② 扩展指导 demo 族：② 打末影龙（门 230 全 PASS）

**V3-16 相位 H/7 收官支 = 10 主题全完成**（16h 拆两版：① 下界门 =
v3.39.0 / ② 末影龙 = v3.40.0；**零内核**——末地嵌眼/travelEnd/龙
boss/水晶/珍珠/爆炸/netherite 甲全既有 V3-10a-d + V3-10b + SURV-1
基建，16a 的 scene/panel_click/索引全复用，内容 + 工具链增量）：
**guide_dragon**（15214f 04:13，176 events：scene = 12×12 平台正午
SURVIVAL **mobs ON** + 预置 ender_eye 12 (h0) + ender_pearl 1 (h1) +
**netherite 甲 4 件 (h2-h5)** + end_portal_frame 5×5 环 12 格 center
(6,6) 未嵌眼【frame survival 不可得 = 预置】；tour = P0 → **装备**
【netherite RMB 即穿 20 AP 80% 减伤 6→1.2/击】→ **嵌 12 眼**【GOOD
cell (6,4) 末位 PITS 421 → "End portal activated at (6,54,6)"】→
站入 60-f 传送【"Traveled to end at (40,53,0)" 落 entry platform】→
**dragon --dragon-damage 200 同帧杀**【f1442 + egg (0,54,0) + 23 orb
200XP + 240-f ascension 仪式】→ **珍珠 28° 弧西掷 40-m**【flat 仅
12-m；"pearl teleport to (1.89,54.69,-4.30)"】→ 入 center【step-up/
down 出口门环 = 1-block 实心墙】→ **破 2 出口水晶**【f1960/f2019
bare-hand hardness-0 瞬破 explode r3 爆坑 28 格；tower 晶 ±50
off-route CUT 偏离】→ 观蛋【**egg 完好**——center 站位眼位离 egg box
≥1-m PITS 422④】→ 站 center 等 900-f 冷却 → **返程**【"Traveled to
overworld (end exit)" 落 (0.5,54,0.5)】→ 观点亮门收束；终态钉 hp20 /
**xp 92**【11/23 orb 收集；12 早落 orb 穿未载 chunk 沉基岩洋 y5 持久
= streaming 时序产物记档偏离】/ netherite 甲 4 件各 6.5 损 / inv 空 /
lit portal 9/9 + eyed 12/12 / time 0.6726；**--dragon-damage 200 =
必带 CLI seam**【龙战不可手编，10d1 同款记档】）；**门内捕获 PITS
422 八坑**【①传送后视图 re-anchor yaw+位置 pitch 勿覆写 ②珍珠射程
仰角主导 ③珍珠穿晶 = shatter_on_arrow 全 projectile 吞珍珠 ④egg box
= LMB ray t0 陷阱 ⑤chunk 载竞 = 珍珠穿未载地形 + orb 沉基岩洋
⑥step 事件行走漂移 0.5-m ⑦rim 大 yaw 多 look 模型漂 = 破晶全 miss
⑧出口门环 = 1-block 实心墙】；index **dimension seq 2**【tags
end/dragon/portal/pearl】；2998/2998（banner re-pin 1）+ S1 第 111
次【f60 ×2 ≡ base19】+ canary 4×2 新≡旧二进制（tm_v339 v3.39.0）
RAW+savenorm EXACT + guide_dragon ×2 RAW/savenorm EXACT + 性能 A/B 同
档【upd med 0.523/0.576/0.565 vs 0.527/0.583/0.557 + swap 同档 +
终档跨树 savenorm EXACT = 结构性零成本】；详见
`docs/archive/phase-3.40.0.md`

## v3.39.0（2026-11-12）— V3-16h① 扩展指导 demo 族：① 搭建下界传送门 + 去下界（门 229 全 PASS）

**V3-16 相位 H/7 = dimension 章节首支**（16h 拆两版：① 下界门 = v3.39.0 /
② 末影龙 = v3.40.0，16d 拆分先例；**零内核**——下界传送门/8:1 映射/下界
worldgen/flint_and_steel 点火全既有 V3-1 Phase B 基建，16a 的
scene/panel_click/索引全复用，内容 + 工具链增量）：**guide_nether**
（10865f 03:01，98 actions：scene = 12×11 平台 x0..11 z-8..3 顶 y54 正
午 SURVIVAL + 预置 flint_and_steel h0 + **obsidian 4×5 传送门框**
【14 ring 格平面 z=-4，含角，未点燃——obsidian 挖掘 15-s/格 = 预置】；
tour = P0 开场 → **点火**【flint_and_steel aim 底行顶面 (5.5,55,-3.5) →
同帧 "Portal activated at (5,54,-4) (4x5)"】→ **站入 + 传送**【walk 入
portal 60-f 倒计时 → "Traveled to nether at (1,55,-1) from (5,55,-4)"，
8:1 映射落生成帧底中心 (1,55,-0.5)】→ **下 frame** 到 nether 地面 y53
【2-m 落无坠伤】→ **观下界**【walk 回路 + C 第三人称 sweep ×3：玄武岩柱
+ 熔岩海 + 无天穹红天 + 生成 portal】→ 收束（player 下界 (0.97,53,2.87)
hp20）；**返程不做**【下界生成 portal 返程 = 900-f 冷却 + 2-m 上台帧难
+ auto-re-travel 乒乓，② 走 end exit portal 更自然，本支聚焦"去下界"】）；
**门内捕获 PITS 420 四坑**【①点火 aim 底行顶面朝内面 ②传送后 author
track 失效须手动 re-anchor ③返程 auto-re-travel 乒乓 = 下 frame 规避
④下界 frame 悬空 2-m】；index **dimension seq 1**【tags nether/portal/
travel】；2998/2998（banner re-pin 1）+ S1 第 110 次【f60 ×2 ≡ base19】
+ canary 4×2 新≡旧二进制（tm_v338 v3.38.0）RAW+savenorm EXACT +
guide_nether ×2 RAW/savenorm EXACT + 性能 A/B 同档；详见
`docs/archive/phase-3.39.0.md`

## v3.38.0（2026-11-12）— V3-16g 扩展指导 demo 族：⑩ 完整黑夜历险（门 228 全 PASS）

**V3-16 相位 G/7**（survival 章节首支；**零内核**——弓/箭/火把/knockback/
mob 生成/磁吸/自然回血全既有，16a 的 scene/panel_click/索引全复用，
内容 + 工具链增量）：**guide_night**（18301f 05:05，454 actions：
scene = 40×26 平台 x0..39 z-25..-1 顶 y54 **time 0.55 入夜** SURVIVAL
**seed 7**【seedhunt 8 粒：首批 4 = sheep/chicken/chicken/**zombie
f150 唯一 hostile**，无 skeleton【wither 箭站桩 9s 死】/无 spider【1.0
b/s 打不过 kite 账本】】+ 预置 planks 9/coal 1/flint 2/feather 2；
tour = P0 开场 → **kite NE + 造 table** → **SE 停扎最小 combat kit**
【table place (38,54,-2) + sticks + bow（P = **g5** 非 g4）+ arrows
×1】→ **弓箭 combat**【N 走到 (38.5,-14) 7-m，4 箭 full-draw 站射
击杀 zombie **f3524**——弓 = 短程 v²/g 11.25-m，箭 **release 发射**
按 release 帧位置 aim + **hold=100** 保满力 + 弹道 lead + **knockback
定点 5 轮迭代**】→ 回 SE 造余 kit【sticks + arrows + torch，**select
非弓**再开容器】→ **火把环 ×4**【先走到点再放】→ 黑夜巡视【C 第三人
称 + E 面板展示】→ **黎明** face E（t0.058）；终态钉 player
(19.57,54,-16.57) **hp20 存活** / stick 2 + bow 1 (dur 380) + arrow 4
/ 4 火把 + table / entities 0【掉落 120s 消散】/ time 0.058 dawn）；
**工具链增量** = gen_guide_night.py 场景 + build_guide_night.py beat
脚本 + **arrow_sim.py / aim_from_log.py 弓箭弹道 aim pin 探针族**；
**门内捕获 PITS 419 八坑**【①弓短程 11.25-m 19-m 全 miss ②release
发射 aim→release 1.6-m 推进 ③hold96 欠拉 99-power 0.2-hp 存活 ④
knockback 定点迭代 ⑤持弓 RMB = 拉弓非开容器 ⑥火把先走到点再放 ⑦
zombie 自 spawn 即追 s2 受 6 伤 ⑧inv_grid = 制作网格非背包 + panel
tap ≥25-f + bow P = g5】；index **survival seq 1**【tags night/
zombie/bow/torch】；2998/2998 + banner re-pin 1 + 结构免疫 + canary
4×2 + S1 base19 + 性能 A/B 同档；详见 `docs/archive/phase-3.38.0.md`

## v3.37.0（2026-11-11）— V3-16f 扩展指导 demo 族：⑨ 河流（门 227 全 PASS）

**V3-16 相位 F/7**（build 章节第三支；**零内核**——水桶/流体/营火/
火把/游泳全既有，16a 的 scene/panel_click/索引全复用，内容 + 工具链
增量）：**guide_river**（19841f 05:30，443 actions：scene = 24×15 平
台 x0..23 z-15..-1 顶 y54 正午 stone 垫 y50-52 + **dirt 表层 y53**
【裸手 0.75-s 挖掘面】+ **1-deep 密封源水池** x18..21 z-13..-10 16
源【2-deep 弃用 = 顶层源 resting-on-fluid 无水平传播，舀痕永久——
1-deep 痕 = 邻源 1 tick 补 level-7 稳，probe B 实证】+ **campfire
(10,53,-7) LIT** + SURVIVAL 预置 bucket h0 + torch h1；tour = P0 开
场 → 池巡视 → **挖河 15**【x2 顶面自 bank + x3..x9 沟内侧面带 +
x10 过营火 + x11 顶面挖 + x12..x17；磁吸随进随收】→ **舀水 1**
【池远角源 (19,53,-13)——level-7 痕不可舀，第二舀必换格】→ **倒水
W** (2,53,-7)【站位 y53 穿越落格盘算 + re-anchor pin】→ 观西梯度
【8..1 铺 8 格，营火邻格 x9 湿 = T0+21 tick，**灭 = T0+22 帧**】→
舀水 2 (20,53,-13) → 倒水 E (17,53,-7) → 观东梯度【2..8】→ **火把
置沟北岸 (14,54,-5)**【水火对比：营火灭 vs 火把亮——MC 偏离记档】
→ **游泳横渡**【step-in 15-f + 14-m 252-f + space 25-f 升 + step-out
30-f——probe v3 pin，实测落点 (0.65,54,-6.16)，游后诚实 re-anchor】
→ E 面板展示 15 泥土 → C 第三人称双线巡视【终点 21.5 = 东缘 2.3-m
margin】→ 收束；终态钉 对称 V 沟形 + 池 14 源 2 痕 + campfire_
extinguished 存续 + dirt 15 + bucket 空 + 零实体 + 终姿
(1.36,54,-4.76) hp 20）；**工具链增量** = mk_scene fluids/campfires
两 strict 面【幂等 10232 B】+ 分段前缀验证 14 段【≤30 ✓】；**门内
捕获 PITS 418 十六坑**【mid-coast 1-m 横向 jog = 漂移放大器两实例
——s7 倒水三轮 + 游泳入口 4-m 东腿 coast 的 at(20) settle 不足 =
入口 x +1.77-m 东冲 → 252-f 泳 pin 短停河心 x6.25 → 20-m 巡视东腿
冲过平台缘 y38 hp13【修 = 入口列走 z 末腿同轴，coast 仅 z 向】/ 池
格非实心 = 干路选列须验整列身下实心【x18.5 列穿池落水停池心】/
**demo finished = 模拟冻结**【cascade 须 replay 内完成，dwell ≥240-f】
/ campfire 格内侧面挖邻格 = 必中 cross 块【x11 顶面挖修】/ 倒水 y54
穿越点落格纪律 / 游泳腿显式 hold + 相对坐标 + pos 断言须带 y + 平台
域】；2998/2998【内容相位零新增 + banner re-pin 1】；S1 第 108 次
【f60 ×2 ≡ base19 + banner 零像素】+ canary 4×2 新≡旧二进制
（tm_v336）RAW+savenorm EXACT + guide_river ×2 RAW/savenorm EXACT +
性能 A/B 同档【OW 900-f 无输入三窗 upd med 0.555/0.609/0.583 vs
0.599/0.626/0.625 + render 0.563/0.708/0.642 vs 0.620/0.706/0.683 +
swap 15.32/15.23/15.27 vs 15.23/15.18/15.18 + changesApplied 9/3/1
逐位 + 终档跨树 savenorm EXACT = 结构性零成本】；详见
`docs/archive/phase-3.37.0.md`。

## v3.36.0（2026-11-11）— V3-16e 扩展指导 demo 族：⑧ 材料（门 226 全 PASS）

**V3-16 相位 E/7**（build 章节第二支；内核 = 1 行×10 的 demo 恒锁定 +
工具链的 entry-aware 向量行走模型——16a 的 scene/panel_click/索引全复用）：
**guide_materials**（25383f 07:03：scene = 30×26 平台 x0..29 z-25..0
顶 y54 正午 + SURVIVAL + **预置 wooden_pickaxe h2** + 8580 cells；
**7 biome 床 33 标本** = B1 森林【橡木 3 + 草凸块 2】/ B2 沙漠【仙人
掌 2 + 砂凸块 4】/ B3 针叶林【云杉 2 + 雪凸块 3】/ B4 矿脉【石出露
4×2 depth-1 + 煤 2 铁 1】/ B5 花田【葱绒/矢车菊/郁金香 各 2，
non-solid】/ B6 羊毛【羊毛 3 + 荧石 2】；tour = P0 扫视 → B5 六花
1f tap → B4 **ore 先 stone 后**【单拍链破 3 格修复形，铁 1 煤 2 短拍
1.25s → 石 3 ×2.3s 尾 2.2% 部分破零过挖】→ B6 → B2 → B1 → B3 →
E 面板 1020f 展示 → C 第三人称四面巡视 → 收束；终态钉 33 全破 +
保留格（出露 x18 双层 + 树干 y55/56 层）+ 地板零挖 + 零实体 +
库存精确【wood 3/dirt 2/snow 3/spruce 2/cactus 2/sand 4/coal 2/iron 1/
cobble 3/wool 3/glowstone 6 每块 3/花 6 + pickaxe 1】+ 终姿
(15.5,-1.5)）；**22 轮根因迭代（v3.1-v3.22）= PITS 417 二十二坑**（guide authoring
的步行校准 + 收集 + 间距全域）：ENT-7 镜像模型【指数逼近 4.3 + 松键
9f 滑行，k 8.0/12.0 与 player.cpp 358-394 孪生，7×33f 链 15.4917 vs
实测 15.4922】→ leg 类 {1.0,2.0} only【0.5/1.5 未校准类 = 11% 过走
死源，walk_to 响亮断言】→ **entry-aware 修正**【稳态链表对 rest-entry
腿短走 ~0.19m = 0.2-m 线裕量吃光 = B3 脱轨真根】→ aim = 面点【4 格
角点 = DDA 跳格】→ **收集红线**【磁吸 = 速度 6(1-d/2.5)² + 前 0.5s
不吸不拾 + ready 后 d≤1.0 当帧即拾——每 drop 必有轨迹帧 ≤1.05m，
stand d≤1.0 或 detour 路过点，33/33 审计】→ **间距规则**【走线与完
好 y54/55 实体 ≥0.8m 首选，0.2m 平面部类保留（12/12 实证 + 校准腿
±0.03m = 0.17m 结构裕量）；自家树干 y55 层拦体带 = 同柱线须绕】→
雪块/仙人掌搬迁【(9,-8) 与 z-8.5 线 0.2m 擦碰 = 角接触 float-ulp pin
死点】→ **分段前缀验证定案**【用户定案 ≤30 段 = 11 段：trim 跨 F
hold 响亮拒 + wheel-0 延长按；段检查 = pos ±0.5m 漂移金丝雀 / 累计
broken / 累计库存 / 实体 0——前缀 = 全跑逐帧恒等，段 PASS 可传递】；
**内核 1 行×10 = demo 恒锁定**【初 3 处 = game_frame inputLocked 镜像
+ game_interaction mine 门 + game_ui UI 组：m_demoPlayer ? true :
relativeMouseMode()——物理锁窗 flap（失焦/OS 释放）吞注入旋转/挖掘
= 5/5 复现的"随机脱轨"红鲱鱼源，demo 确定性环境无关化；canary RAW
diff 再捕获 7 处【新 vs 旧二进制截图 byte 35/36 + savenorm EXACT =
像素面分叉 sim 位级恒等：C 相机 toggle 会话态【同一 C 事件一 run 处理
一 run 丢弃 → First↔ThirdBack 分叉】+ T 教程/HUD/进食/ESC 退出/手臂/
准星——12 处 relativeMouseMode() 读取审计全表，2 处有意不门控】】；
format 33/sim 2 不变（canary 4 EXACT 执法）；2998/2998（banner re-pin 1，内容
相位零新增单测）；S1 第 107 次（f60 ×2 ≡ base19 + banner 3.36.0 零
像素）+ canary 4×2 新≡旧二进制 RAW+savenorm EXACT【锁定修冷路径位级证明】+
guide_materials ×2 RAW/savenorm EXACT + 性能 A/B 同档【终档跨树
savenorm EXACT = 结构性零成本】；详见 `docs/archive/phase-3.36.0.md`。

## v3.35.0（2026-11-09）— V3-16d 扩展指导 demo 族：⑦ 皇宫（门 225 全 PASS）

**V3-16 相位 D/7**（build 章节首支 = 建造与大地 ⑦⑨⑧ 的 ⑦；16d 拆
两版用户定案：本支 = ⑦皇宫，⑧材料 → 16e v3.36.0；内核零变更 = 16a
的 scene/panel_click/索引全复用，本相位 = 内容 + 工具链增量）：
**guide_palace**（19536f 05:25：scene = 21×18 扩台平台 x0..20 z-13..4
顶 y54 正午 + creative flag + 8301 edits；**定案结构（v10 简化）** = 开场
第三人称（建造全程观众视角，aim 射线自眼位不变）→ 双层围墙 92
【t1 stone_bricks + t2 terracotta_yellow，门洞 x8/x9 双层开，地面行走
绝对格 aim】→ **四角塔 8 = 垂直-only 角循环**【B 单按 fly ON → 升 45f = 3.0m
（脚 57.0 眼 58.62，v7 的 65f = 58.33 眼 59.95 时 8 aim 全 4.12-4.15-m
超 kReach 4.09 = 0/8 门内捕获反解）→ aim 目标 t2/t3 顶面【aim 点在面
平面 = 命中与眼高无关，8 aim 逐一射线核证 2.2-3.0-m】→ B 单按 fly
OFF → 地面走到下一角（9/16/9-m，0.1-m 裕量墙线 = 92/92 实证域）×4
（SW→NW→NE→SE；NW→NE 走线 z-12.0 外圈北绕行【v9 门内捕获：z-10.5
线在北墙 z 带内 = 16-m 墙隧道卡死；v10 门内捕获同族：z-9.0 线撞东
墙 (15,54,-9) 卡 x14.6——内圈 z 线必撞东墙，环外唯一解，逐线三轴
核 PITS 416⑫】）+
航拍展示 2（SW 墙成 / SE 塔成 360° 渐进环绕）→ 结尾四面巡视【SE
塔落地即出】；终态钉 100 块 + 零 stray + 平台零挖 + 零实体 +
终姿 (8.5,3.5)）；**门内捕获 PITS 416 十三坑**【**creative scene = 入场飞行**【applySave
setFlying(true) V3-11a：v6 墙段 airwalk 7.12 = +0.89m/leg 15m 漂移
stray 网格实证】+ 双击墙钟窗 120s autosave stall flake【v4 入口侥幸
入窗 / 塔段 flake = 塔 0/8 地板 0/38】→ **gamepad B 单按定案**【入口
B fly OFF + 4 处飞行 toggle，无窗无 jump grant sim 帧确定；timeline
词表拒 gamepad = dmo 后贴 demo_fly_patch.py：pinned gzip 字节 parity +
sorted JSON + no-op 自校每次断言 + 文件序保持】/ 飞行 7.12 实测 vs
6.88 名义【自锚定无 margin】/ 脚手架 step-up 死路【S2 AABB 恒重叠
静默拒收 + 角落 tie + creative 30f 即挖连锁 2 深坑穿平台】→ 飞行悬
停放 / 帧停诊断法 + autosave 中途覆写陷阱 / scene inventory 严格对
象形 / demo 玩家通道纯注入【用户鼠标 = audience orbit 仅渲染，唯一
例外 = 帧节奏】/ checker stray = 名称比较非存在性】；工具链增量
【demo_author FLY 7.12 + walk_leg(fly) + sweep_gradual + fly_toggle
side-car + demo_fly_patch.py 后贴器】；2998/2998（内容相位零新增——
init 交叉检查 5 条目全量 boot 复验）；S1 第 106 次（f60 ×2 ≡ base19
+ banner 3.35.0 零像素）+ canary 4 EXACT + guide_palace ×2
RAW/savenorm + 性能 A/B 同档（结构性零成本）；详见
`docs/archive/phase-3.35.0.md`。

## v3.34.0（2026-11-09）— V3-16c 扩展指导 demo 族：③ 完整房子+床（门 224 全 PASS）

**V3-16 相位 C/7**（day1 章节 ③ 一支 = day1 ④⑤⑥③ 全 4 支完成；
内核零变更 = 16a 的 scene/panel_click/索引全复用，本相位 = 内容 +
工具链增量）：**guide_house**（18875f 05:14：scene = 12×12 平台 +
工作台 (1,54,-1) + 夜 0.75 + h0 planks 36/h1 wool 3；**定案结构
= 床预制先做**【夜窗时序面：sleep f5794 ≪ 10800】→ 一段墙 9 + 二
段墙 9【地面顶面 aim】→ 脚手架 S1/S2 + 唯一 step-up 链【S2 顶】→
middle x2 + north x2【S2 顶 elevated 眼】→ 屋顶 walk 北行 x3-5【二
段北顶面】→ 南行 x5/x4【二段南顶面】+ middle x5【链 +X 面，aim
陡化 = 捕获⑤】→ 4-hops 下行【W 提前释放 = 无坠漂，捕获③】→ 落点
原位脚手架双挖【净 0 块】→ 室内 middle x3/x4【链 +X 面】→ 门缺口
列南 x2/x3【middle +Z 仰面——南 x3 二段门缺口无顶面 = 唯一可行
面，捕获②】→ 床 place_ahead (4,54,-5) + **Slept at the bed**【夜跳
dawn】→ 黎明巡视【四面 walk-around + 日出】；终态钉 house 30
planks y54 9/y55 9/y56 12 零 stray + 床 + respawn (4.5,54.5,-4.5) +
h0 3 + 零实体 + hp 20）；**门内捕获 PITS 415 七坑**【出生点与表格
格重叠 = 静默顶 y55 全链死（表格移至 (1,-1)）/ 地面眼 y55.62 < y56
= 二层侧底面截断 = 屋顶可行面谱定案 / C4 坠 W-hold 漂 0.5-0.8m /
长 walk leg 损耗 0.4-0.5m 过补偿 / 屋顶 walk z stop 0-0.5m = aim
边距 1cm 生死 / 场景 journal 容器态三处同步 / gzip 资产 Python 读
0x8b】；工具链增量【demo_author walk_leg y/hold 尾参（既有 demo
零影响）+ mk_scene tables/furnaces 透传】；2998/2998（内容相位零
新增——init 交叉检查 4 条目全量 boot 复验）；S1 第 105 次（f60 ×2
≡ base19 + banner 3.34.0 零像素复验）+ canary 4 EXACT + guide_house
×2 RAW/savenorm + 性能 A/B 同档（结构性零成本）；详见
`docs/archive/phase-3.34.0.md`。

## v3.33.0（2026-11-08）— V3-16b 扩展指导 demo 族：⑤补充体力 + ⑥修复回血（门 223 全 PASS）

**V3-16 相位 B/7**（day1 章节 ⑤⑥ 两支；内核零变更 = 16a 的
scene/panel_click/索引全复用，本相位 = 内容 + 工具链增量）：**⑤
guide_food**（18262f 05:04：6×4 麦田【farmland 挖洞铺 + wheat_7×8
可收 + 年轻梯度 ×16 demo 期间帧锁定生长视觉长熟】→ 逐格收割 1f tap
→ E 面板面包 ×5 → 进食 ×2【1.6s 通道 96f】hunger 10→20 满；终态钉
h0 麦 1/h1 面包 3/hunger 17/sat 8/田 16 株全熟/零实体）+ **⑥
guide_regen**（18344f 05:05：5 格石柱 5 落差台 → 走下台 hp 20→18
→ 进食 → **10s 门 + 2×4s 自然回血**【每半心耗 1 饥饿】hp 18→20
满；终态钉 hp 20/hunger 13/sat 2/面包 1 余/柱完好）+ 工具链增量
【capture 2f 化 = 零 locked-mining 帧 / 旧分段 walk_polyline 死码清
除】；**门内捕获 PITS 414 四坑**【f0 捕获 hold 尾部瞬破 0 硬度作物
×3 / scene journal 同格重复点名 / 1f vs 2f tap 的硬度 0 连锁 +
年轻麦 f3000 全熟变硬度 0 / perf A/B cwd 陷阱】；2998/2998（内容相
位零新增）；S1 第 104 次（f60 ×2 ≡ base19 + banner 零像素）+ canary
4 EXACT + ⑤⑥ ×2 RAW/savenorm + 性能 A/B 同档（结构性零成本）；详见
`docs/archive/phase-3.33.0.md`。

## v3.32.0（2026-11-08）— V3-16a 扩展指导 demo 族：数据层 + scene + panel_click + pilot ④（门 222 全 PASS）

**V3-16 相位 A/7**（10 主题 3-10 分钟级教学 demo 族——本相位落内核 +
工具链 + pilot ④ 挖矿打造工具，16b~16g 按章节扩）：**scene 世界
spec 第三形**【`{type:"scene",name}` → committed 确定性存档资产
`assets/demos/scenes/<name>.json`，只读 = 观察红线结构根；`--save`
可组合（退出写回放结果）】+ **panel_click 事件**【几何无关格地址：
panel id + slot id + button；resolver = 开面板检查 + layout rect 中心
（当前窗口几何孪生）+ 真实 Input closure 合成点击；author 词表面
严格拒收；net `--script` 拒收（40B 无 panel 字段边界）；**sim 2 不
bump**——canary 4 EXACT 执法】+ **索引数据层**【`index.json` 四轴
schema {category,seq,mode,tags,file,length_frames} + 5 章封闭词表
【day1/build/dimension/survival/coop】+ init 每次 boot 全量交叉检查
【文件存在 + strict load + SIM 门 + length + scene 存在】+
`--list-demos` 独立无头旗】+ **pilot ④**（11387f = 03:09：砍树→
木板→工作台→木镐→逐格挖矿 11→熔炉环→熔砂×2 玻璃；鲁棒性定案 =
轴对齐行走 + 相对放置瞄准 + 逐格贴线挖矿 + 磁吸 1m 收集带，全
PITS 413）；`tools/mk_scene.py`（场景手术幂等）+ `tools/demo_author.py`
（Guide DSL）+ `tools/scenes/`（beat 脚本）；2998/2998（+11）；
S1 结构免疫第 103 次（f60 ×2 ≡ base19 + 旧树三方闭环——**base19 锚
= f60 非 f900 门内定案**）+ canary 4 EXACT + pilot ×2 RAW/savenorm
+ S6 五面点名 + 性能 A/B 同档（结构性零成本）；详见
`docs/archive/phase-3.32.0.md`。

## v3.31.0（2026-11-08）— V3-12c2 断线恢复 REJOIN + format 33 players[]（门 221 全 PASS，V3-12 全 4 相位收官）

**V3-12 多人收官相位 c2**（**format 32→33**【players[] 迁移 + 末地
orb 持久化】/ **sim 2 不 bump**【canary 4 EXACT 执法】/ 全 sim 覆盖）：
**format 33 players[]** = root `player` + root `inventory`/`inv_grid`
迁入 per-player entry【entry 0 = primary 镜像，applySave 主路径零改；
entries 1.. = 远程玩家】+ 三面 version lie【≤32 携 players / ≥33 缺
players / ≥33 携 root player/inventory/inv_grid】+ 序列化 fallback
【save.players 空 = 从 legacy 主字段派生单条——fixture 兼容 + belt】
+ kSavePlayerCap 8；**末地 XP orb 持久化**（10d1 后移复议兑现：
`end_xp_entities` 恒写，overworld xp entry 形 verbatim，parseXpEntry
单源）；**REJOIN 快照** = save 形 WELCOME 携主机当前文档【磁盘档 +
会话计数族 {weather 5 字段含双 splitmix 流 / spawnCounter×3 /
particleBreakCounter} + 活粒子 + per-player 会话计时器族
{prevHealth/hurtFlash/lastSpace/pearl/drownTicks/flying}——全
presence-gated，on-disk 格式不变 S1 锚免重锚】+ 重连流程【主机持久
listener + 中会话非阻塞 accept + WELCOME startFrame 采纳 + 重连者 =
新体 + 起始库存 D25【原体物品 100% 掉落仍在 entities 可捡回】】；
**Join/Renumber CTRL 广播**【N 增长/移除的 slot 重编号——行随玩家
移、输入历史跟走、无冲洗；removal 的 compact erase 破旧客户端 ring
越界的门内捕获修】；**net 主机抑制幽灵**【载多玩家档 = stale 幽灵的
slot/m_nets 错乱 segfault 修——重启后其他玩家连接已死，玩家集 =
{自身}+{新连接}，与 solo 档 host 等价】；feedJournal 改哈希
**live-diff**【journal ∪ retained 对生成参考的 diff，load 不变面——
rejoin client 回填 journal ≠ host live journal 的原始 cell 集合的门内
捕获修】；respawn **精确 float**（≤32 int cell 对非 cell-center 值
丢 0.5-y 的门内捕获修）；**mob-quiet 合同**【活 mob 在场 = 点名拒收
REJOIN，全 mob 快照传输后移 D24——门臂 mobs-off 实证】。门 221：S1
结构免疫第 102 次（≡ base19 逐字节 ×2 + savenorm EXACT + on-disk 无
net 段）+ canary 4 EXACT（sim 2 不 bump）+ 迁移臂（v3.30.0 format 32
档 → v3.31.0 载入 → 重存 33，值保真 pos/yaw/34 blockEdits/
inventory + root player→players 迁移正确）+ **REJOIN 臂**【host
--listen --save + C1 初始窗 + C2 中会话重连 slot 2 of 3 @ frame 366
+ 三向哈希 f600 ad509c664ccdeb4b host≡C1≡C2 逐字 + 零 desync + 双
rc=0；C1 移除的 Renumber 广播 + 双端 100% drop 同步】+ S6【listen×
save 转正（旧「save-form WELCOME is the 12c's face」拒收移除）+ WELCOME
payload 损坏 → client 点名 + mob 在场拒收命名实证】+ 性能 A/B 同档
【旧 = d316cfc v3.30.0 + g139 双树：OW 900f 无输入 PERF-WIN 三窗 upd
med 旧 0.640/0.638/0.650（复跑带 0.648/0.702/0.696）vs 新
0.707/0.701/0.642（复跑 0.717/0.722/0.721）同档【win2 在旧树带内、
win3 低于带、win1 略高于带 = warmup 窗噪声】+ render/swap 同档 +
changesApplied 9/3/1 逐位【新树复跑 win2 单 cell 9/2/1 = real-dt 流体
沉降抖动 PITS 385 域】= format 33/net 面全冷 solo 路径结构性零成本；
net 自成本（新树 mobs-off 600f）：solo upd med 0.696/0.727 vs 2P 主机
0.773/0.783 = +~0.07ms/帧（per-player sim pass + per-remote 轮询/屏障
+ socket，sub-ms 同档）+ 零 desync】；2987/2987（+13：SaveFormat33
9 项【双玩家 round-trip/32→33 迁移/lie 五面/嵌套必需/end_xp
round-trip+lie/cap/空数组/fallback】+ NetSnapshot 4 项【net 段
round-trip/≤32 lie/absent 默认/shape 拒收】+ NetProto codec 扩展
【startFrame + Renumber/Join + name 16-cap】）。PITS 412（八坑：
Json(bool) 指针陷阱 / legacyShape-format 矛盾 / respawn 0.5-y / slot
重编号 ring 越界 / barrier 停滞 accept 节拍 / client renumber 无 drop /
net 主机幽灵 segfault / feedJournal 原始 cell）。

## v3.30.0（2026-11-07）— V3-12c1 大厅/聊天/N 泛化/心跳（门 220 全 PASS）

**V3-12 多人收官相位 c1**（**format 32 不变**【大厅/聊天/名牌全表
现层】/ **sim 2 不 bump**【canary 4 EXACT 执法】/ kNetMaxPlayers
2→8 协议形）：**N 泛化** = accept 窗 N 定形【首客 10s 阻塞 + 3s 窗
250ms 截止重试【单次静默 break = 门内捕获坑】+ WELCOME 窗后统一
flush 同 N】+ 客户端全槽体【每机模拟每玩家——netAddPlayer(1) 硬编
码 slot 2 segfault 门内捕获修】；**大厅** = 第三人称名牌【world→
screen 投影常量三件套 game_local.hpp 共享，本地玩家无牌】+ F3 第
22 行 players【Latin 零新字形，test re-pin】（MOTD 并入 V3-17 主
菜单项）；**聊天** = T 切换【net 下 T = 聊天，教程跳过 solo only
D12 延伸】+ frameText 活 pump 本地面 + CHAT 包 type 7【slot/u16
len/≤256B 客户端→主机重广播】+ 8 行环浮层 300f 淡出 + Latin only
零字形 + **打字抑制**【typed 'e' 开面板截断消息的门内捕获修——
Input 可打印域 owner 机捕获前本地吞，中继态干净零】+ 表现层结构
不入哈希；**心跳** = 屏障 stall **min 水印** 10s 点名移除最滞后
【anyProgress 健康同伴遮死者的死锁形门内捕获修】；**E 面板中继
修** = 边沿全玩家处理 + 窗口/声音动作 localOwner 自门【isLocal 门
= 主机玩家面板 desync f570 门内捕获根】。门 220：S1 第 101 次（≡
base19 ×2 + EXACT）+ canary 4 EXACT + mobs-on EXACT / **3P 锁步臂
×4 批次哈希跨批逐字恒等 + 主机档跨批 EXACT + 零 desync**【截图
RAW ×2 各进程至少一批全干净；Mesa 无头前缓冲读回竞态纯彩块 = PITS
环境面】/ 大厅截图臂【--script 确定性视角 yaw -88.8°：名牌 + F3
players 行 + ThirdBack OCR 实证】/ 聊天臂【xdotool T+type+Return
双进程日志 "net chat[host]: hello gate" 逐字 + 浮层 OCR】/ S6 八面
复验 + accept 窗 2 新面【2 客→N=3 / 窗 3.01s 超时→N=2】/ 性能 A/B
同档【旧 = 1f2e10f v3.29.0 + g139 双树：banner 实证 v3.29.0/v3.30.0 + OW 900f 无输入：PERF-WIN 三窗 upd med 0.426/0.444/0.426 vs 0.399/0.402/0.497 同档 + render 0.495/0.480/0.433 vs 0.464/0.456/0.507 同档 + swap 15.6 同档 + changesApplied 9/3/1 逐位 + 终档 savenorm EXACT 零 format 最强形 = 大厅/聊天/N 泛化/打字抑制的 solo 路径（净面全冷 + 抑制旗 solo 恒 false）结构性零成本；净自成本（新树 mobs-off 600f）：solo upd med 0.472/0.424 vs 2P 主机 0.687/0.729 vs 3P 主机 0.736/0.402 = +~0.2-0.26ms/帧（per-player sim pass + per-remote 轮询/屏障 + socket，sub-ms 同档），render 0.452/0.525/0.561 同档 + swap med 15.5/14.9/14.6 同档【net 跑 swap max ~30ms = 屏障等待尖峰预期】+ 零 desync】/ 2974/2974。PITS 411（九坑：accept 窗单
次 break / ms-s 混用 / 客户端全槽体 / E isLocal 门 / typed 'e' 截
断 / Mesa 读回竞态 / WM 平铺几何 / 尾帧 broken pipe / 屏障 min 水
印）。详见 `docs/archive/phase-3.30.0.md`。

## v3.29.0（2026-11-07）— V3-12b 2 人多人 PoC（门 219 全 PASS，矩阵收官前最后一版）

**矩阵最后项 V3-12 多人相位 12b = 2 人局域网 lockstep PoC**（设计稿
§4 全采纳；**format 32 零 bump**【主机单存档 D13】/ **sim 2 不 bump**
【canary 4 EXACT 执法——solo 路径位级 = 结构免疫根】/ 零新依赖
POSIX TCP 无 net 线程 / N ≤ 8 协议形 12b 恒 2）：

**net/ 新目录 ~1200 行**：小端固定头协议 {magic, type, len} /
HELLO 四指纹握手 {TM_VERSION, sim, assetFNV（assets 树排序 FNV）,
simConfigHash（sim 键固定表——seed 豁免【WELCOME 权威】/ render 豁免
【dt 广播归一】）} / WELCOME（fresh = 仅 seed 零字节世界传输）/
INPUT{frame, FrameState 40B, hash} / FRAME{frame, dtBits raw f64,
states×N, hash} / CTRL / REJECT 点名；FrameState = keys u64×2 +
buttons + mouseDx/Dy raw f32 零量化 + Ax/Ay（R8 面板落格）+ wheel +
locked（R18）；**状态哈希字段清单**（§2.3：frame/day/weather/每玩家
全态 + 会话计时器 + 36 格库存 + 效果 / 每维实体插入序含 mob rng /
每维 journal 全集 / spawnCounter×3 / 粒子 / fires）每 30 帧交换双向
检测。**player 多人化重构（双别名最小方案）**：m_players vector +
m_localSlot + m_player raw pointer primary 别名（354 处零改动）/
PlayerSession 吸收 10 成员族 + 14 引用别名绑 session 0（~430 处零
改动）/ pass 归属二分（primary 锚不动：mob 目标/教程/龙/portal；×N
循环化：物理/生存/效果/骑乘/珍珠/交互 17 函数/拾取磁吸三拾取/眼接触/
箭扫掠/面板/教程 toast/渲染 local 化）/ per-player aim（solo 相机
路径位级，net 第一人称形 D17）/ R2 simTime 2 处 / R10 xpSeed 盐族 /
R9 出生点 i。**streaming 2 处修正**：回退门 pool.pendingCount()（本机
吞吐 = desync 根）→ streamPending.size() 共享账本（盘锚经别名自动
primary）。**host/client 双循环**：单 runGameplayLoop + net 钩子
（主机 poll + **lockstep 屏障**【等客户端 INPUT[N−D】；客户端等包
阻塞模拟）+ 哈希交换 + **断线移除**（EOF/RST 同形：库存 100% 掉落
+ 落乘 + 会话 solo 续跑；客户端见主机 EOF = 全体退 D2）+ CLI
`--listen/--connect/--script`（demo 时间线第四消费方 = 确定性输入，
× demo 族互斥，--connect×--save 点名 R7）。**12b 偏离 D16-D20**：净
世界 fresh only / net aim 第一人称形 / 远程体静态姿态 / window parity
= config 几何 / client 退出 = RST。

**门 219**：S1 结构免疫第 100 次（≡ base19 逐字节 ×2 + EXACT）/
**S2 双进程 headless 锁步臂**【无输入 600f ×2 run：两进程 net hash
f0/f300 逐字同 + 主机档 savenorm 跨 run EXACT + 各进程截图 RAW ×2；
输入注入臂 ×2：哈希逐字同 + **client 挖掘经中继落主机 journal【跨
进程世界收敛铁证】**；断线臂 ×2：removed f302 + 起始库存 20 件 100%
掉落 + 主机档跨 run EXACT】/ S6 八面点名（sim/magic/asset/config/
window/connect×save/listen×demo/端口占用 全 rc 1）/ canary 4 EXACT【
sim 2 不 bump】/ 性能 A/B 同档【旧 = a8259c8 v3.28.0 + g139 双树：banner 实证 v3.28.0/v3.29.0 + OW 900f 无输入：PERF-WIN 三窗 upd med 0.524/0.560/0.581 vs 0.544/0.582/0.586 同档 + render 0.556/0.674/0.637 vs 0.564/0.666/0.656 同档 + swap 15.3 同档 + changesApplied 9/3/1 逐位 + 终档 savenorm EXACT 零 format 最强形 = 多人化重构的 solo 路径（净面全冷 m_net null + size 1）结构性零成本；净自成本（新树 mobs-off 600f）：solo upd med 0.574/0.587 vs 2P 主机 0.642/0.638 = +~0.06ms/帧（player-2 sim pass + 环 + socket，sub-100µs 同档），render/swap 同档】/ 2974/2974（+10
NetProto/NetSession 族）。PITS 410（九坑：client 环槽偏 1【f150 哈希
点名】/ 主机快跑屏障 / RST≠FIN / WM 平铺竞态 / HELLO uiScale 时序 /
seed 豁免 parity / 无火种子扫描 / --new 净主机 / 起始库存 per-
session）。详见 `docs/archive/phase-3.29.0.md`。

## v3.28.0（2026-11-07）— V3-12a 多人设计定案（门 218 全 PASS，纯文档版）

**矩阵最后项 V3-12 多人（唯一「超大」架构级）相位 12a = 立项设计
稿，无代码**（v3.0.0 开启版同款：纯文档 + banner 3.28.0）：设计权
威入仓 `docs/V3-12-MULTIPLAYER-DESIGN.md`（390 行，代码引用文件:
行实证）——**方案 1 主机权威 lockstep 定案**（项目确定性纪律 =
锁步免费午餐：demo 内核 = 客户端循环形状 + 输入中继 = Input
handle* 第二消费方 + 世界传输零【worldgen 纯函数同 seed 位级】）/
**时间源 = 主机广播 clamped wall dt（raw f64 位）**（real-dt 系统
结构性零 desync；wall 直读仅 2 处 → simTime 化 solo 位级）/ 网络 =
POSIX TCP 无 net 线程 + 协议 schema 定案（HELLO 四指纹握手 /
FrameState ~40B / D=2 统一延迟无预测 / 30 帧状态哈希字段清单）/
**Player 多人化 = 双别名最小方案**（m_player = primaryPlayer() 保
留 354 处零改动 + 10 成员迁移 + pass 归属二分表）/ streaming 跨机
2 处修正（primary 锚 + streamPending 共享账本）/ 12b = format 32
零 bump + sim 2 不 bump + 全 sim 覆盖。风险 R1-R20 / 偏离 D1-D15
全文。编排者裁定：12b = v3.29.0 门 219（侦察门号顺延修正）。门
218：S1 第 99 次（≡ base19 逐字节 ×2，banner = F3 文本零像素面）/
canary 4 EXACT / 性能 A/B 同档【旧 = 3f9ae83 v3.27.0 + g139，二进制差异仅 F3 文本串：PERF-WIN 三窗 upd med 0.563/0.663/0.649 vs 0.520/0.610/0.580 同档 + render 0.562/0.744/0.726 vs 0.543/0.682/0.602 + swap 15.3 同档 + changesApplied 9/3-2/1【win2 = streaming 边界 real-dt 域，门 207 先例带】+ 终档 savenorm EXACT】/ 2964/2964。详见
`docs/archive/phase-3.28.0.md`。

## v3.27.0（2026-11-07）— V3-10d2 召唤与复杀（门 217 全 PASS）

**矩阵唯一真大项收官相位 D2/2**（召唤序列 + 复杀闭环；format 32 不
变【召唤/蛋计时/败亡旗全会话态，蛋 = 世界态既有面】/ **sim 2 不
bump**（canary 4 全 OW EXACT）/ 零 config / 块 135 不变【crystal_
base = 既有 4 块新旗非新块】/ 字体零新增【F3 龙态行全英文，
528/528 满区不碰】）。

**召唤**：`crystal_base` 块旗（strict bool，obsidian/bedrock/出口
双帧 4 块置 true）+ `requires_crystal_base`（end_crystal 独占）=
放置门（placeBlock 数据门零物种 switch——end_crystal 生存可放置，
10d1 配方闭环）；**4 出口面晶全在 ∧ 无蛋 ∧ 无龙**（面晶词表：±X
对 = arena 出口晶算数，±Z 对 worldgen 缺席只能玩家放置）→ 60f 倒
计（任一损失取消）→ 完成时 4 晶引爆【相邻晶爆波及 = MC 4 晶同爆
读】+ 蛋计时清除 + 缺位塔晶回写（已载 only）+ 显式 60f 复生。**败
亡旗**（首杀置位不清除，重载复位）：10d1 生成门「无蛋 → 生成」=
首访读——败亡后蛋缺席 = 破蛋非生成信号【门内捕获：破蛋 60f 后无
召唤复生】。**蛋会话重生** = 玩家破蛋 → 3600f 回写（重载复位计时
= D7）；**蛋 blast_resistance 3**（晶爆 r3 局域 1.0 < 3 存活——MC
蛋近不可晶爆毁读）。**复杀 XP = 8**（单 8 值 orb——MC 500/12000
比例 1/24 × 200 的 D5 读；首杀 200 burst 不变）。--dragon-damage
改每生成施加（单次生成行为不变，同跑可走全循环）。F3 +第 21 行
dragon（none/spawn/orbit/strafe/perch/ascend/summon/egg/egg
respawn）。

**门 217**：S1 结构免疫第 98 次（≡ base19 逐字节 ×2 + EXACT）/
**S2 召唤+复杀臂 = authored demo ×2 RAW + 逐字**（f59 杀1 + 200
XP burst 23 orb 逐位 / f321 破晶【蛋 br3 存活】/ f322 破蛋 →
f3922 调度 / f340-373 三面晶放置 / f373 summon armed / f433
sequence complete【2 shatter + 2 爆波及】/ **f493 杀2 + 8 XP burst
单 orb value 8 f558** + 蛋2）/ S2a 无输入末地回归（summon 行 0 =
门惰性）/ 10d1 三臂回归全同（kill f59 + 200XP / 破晶 f151→strafe
f152 / 重载蛋在无复生）/ S6 十六面点名 / canary 4 ×2 RAW +
EXACT【sim 2 不 bump】/ **2964/2964**（+5：DragonSummon 4 +
DragonLine 1）/ 性能 A/B 同档【旧 = 94dc3d9 v3.26.0 + g139：banner 实证 v3.26.0/v3.27.0 + 双场景——OW 900f 无输入：PERF-WIN 三窗 upd med 0.561/0.562/0.571 vs 0.556/0.571/0.573 同档 + render 0.619/0.656/0.661 vs 0.630/0.697/0.638 + swap 15.3 同档 + changesApplied 9/3/1 全逐位 + 终档 savenorm EXACT 零 format 最强形；末地战斗 600f（mobs-on 变体，双侧龙在场）：upd med 0.870/1.017 vs 0.841/1.023 同档 + render 0.502/0.584 vs 0.503/0.569 + swap 15.2 同档 + ents 0.39/0.46 vs 0.36/0.47 同档 = 10d2 增量（放置门/蛋/召唤/F3 全冷）无可测自成本】。PITS 409（五
坑：负 cell 中心 -N+0.5 非 -N-0.5【10d1 词表负侧偏 1 格静默】/
无蛋 ≠ 首访 / demo 同帧 down+up 死点击 + hold 续帧重新取目标 /
demo 旋转 dy 逐目标累计 / 门脚本 /tmp 路径重写）。详见
`docs/archive/phase-3.27.0.md`。

## v3.26.0（2026-11-07）— V3-10d1 末影龙核心 + 首杀闭环（门 216 全 PASS）

**矩阵唯一真大项的首相位**（末影龙 boss 核心 + 首杀闭环；侦察全采纳
+ 门内微调 ×2；format 32 不变 = **零 bump 最强形：龙会话态 + 击败态
= 蛋块世界态** / **sim 2 不 bump**（canary 4 全 OW EXACT）/ 零 config /
块 133→135 / mob 15→16 / atlas 174→179 零移位 / 声音 71→75 / 效果
11→12 / 配方 113→114 / **字体 430→433 = 扩展区 528/528 恰好满**）。

**龙 = 第 16 mob 纯数据**（核心零物种 switch）：200 HP / hostile /
flying + spawns_in_air + `dragon_flight` / `boss` / contact 5 / 火·
效果·击退三免疫旗 / breath 8s-3s-r2 + fireball 2.5s-12b/s +
crystal_break_damage 10 + detect_vertical 8 / 8×4 八盒双翼。**AI =
runAi flying 分派最顶纯新分支**（ghast/bat 位级红线）三态复用
MobState：盘旋（定心半径 30【门内裁决：40+pull 恒过入场点 → 定心移
pull】）/ 侧移 3.5s（火球 2.5s 冷却）/ 驻坐出口锚（咆哮 1.25s → 玩家
脚位吐息云；起飞 = 连吐 4 / 离 150 / 累计受击 >50）。**流契约**：每
帧 ambient 1 抽 + 跨圈帧**恰 2 抽**（u1 侧移 0.25 / u2 驻坐
1/(3+活晶)，strafe 命中亦消费 u2 = 门内重构纯函数
rollDragonLapDecision）。**水晶 = 新块非实体**（6 固定槽 = 4 塔顶 + 2
出口，D3 非 MC 10；break explode_radius 3 = 块自身引信，**无连锁 =
结构性质**；shatter_on_arrow 箭扫掠；治疗源晶碎反伤 10；晶上永久火
D9；普查卸载无关【门内捕获：raw getBlock 0→6 漂 = 假碎晶】）。**龙息
云** = 零物理新实体（r0+0.15·age；吐息 3s/r2 脚位 + 火球撞 5s/r2.5，
leavesCloud 尾参缺省 false 位级红线）→ dragon_breath 效果 3HP/s
玩家-only。**首杀闭环**：m_dead 帧蛋 (0,54,0)【R14 0 帧差；门内捕获
同帧 spawn+kill 漏账死循环】+ 升天 60f 飞 + 180f 悬停 + **23 orb =
22×9+2 = 200 整**（D5）+ wail + 240f 延迟门移除；重载 = 蛋在无复生。
**血条** = renderUi 段（186×8 十段 + 「末影龙」+3 字形 + 2s 淡出；
margin 8→24 门内裁名修）。**--dragon-damage seam**（--explode 孪生——
龙战斗 demo 不可手编 = 耦合系统定案）。

**门 216**：S1 结构免疫第 97 次（≡ base19 逐字节 ×2 + EXACT）/ S2a
无输入末地臂 600f ×2 RAW + EXACT / **S2b 破晶 demo 臂**（f151 shatter
→ f152 strafe + 火球 f152/f303 冷却逐位 + f363 回轨，×2 RAW + 逐字；
demo 存档红线 = 截图 + DEBUG 行权威）/ S2c 击杀仪式臂（--dragon-
damage 200：f59 死 + 蛋 + 23 orb f124..f234 逐位 + ×2 RAW + EXACT +
终档恰 6 键）/ 重载臂（蛋在无复生 ×2 RAW）/ S6 十四面点名 / canary 4
×2 RAW + EXACT【sim 2 不 bump】/ **2959/2959**（+29：dragon 族 26 +
塔晶 + 无连锁 + 字体面）/ 性能 A/B 同档【旧 = 23c4e7c v3.25.0 + g139：banner 实证 v3.25.0/v3.26.0 + 双场景——OW 900f 无输入：PERF-WIN 三窗 upd med 0.594/0.548/0.586 vs 0.576/0.574/0.583 同档 + render 0.637/0.544/0.651 vs 0.617/0.640/0.654 + swap 15.3 同档 + changesApplied 9/3/1 全逐位 + 终档 savenorm EXACT 零 format 最强形；末地战斗 600f（mobs-on 变体）：旧（无龙仅 enderman）upd med 0.525/0.687 vs 新（龙 + enderman）0.905/1.012 = boss 自成本 ~+0.4 ms upd + ~0.1 ms render（beam/云/血条），swap 15.2 同档 + changesApplied 0/0 逐位】。PITS 408
（八坑：init 交叉检顺序 / 数值累加器泄漏 / demo 存档红线 / 流送依赖
锚点 / -1 哨兵 delta / 同帧 spawn+kill / 血条 margin / CJK 边界）。
详见 `docs/archive/phase-3.26.0.md`。

## v3.25.0（2026-11-07）— V3-10c 末影人（End 首个 mob，门 215 全 PASS）

**矩阵大项 V3-10 末地相位 C/4**（End 首个 mob = neutral + 瞬移 + 携
格；format 32 不变 / **sim 2 不 bump**（canary 4 EXACT）/ 零 config /
块 133 不变 / atlas 172→174 零移位 / item 101 不变 / 声音 70→71）。

**末影人**（纯数据 + 数据旗，核心零物种 switch）：40 HP / speed 0.8 /
detect 16 / attack 6 / 0.6×2.9 六盒四 swing / light 0（出口门光 11 圈
排除）。**注视激怒** = `eye_contact_range` 48（=kEndFogEnd「看见它 =
激怒它」定案）+ 5 连续帧（零流计数，眨眼单帧复位）+ game pass（瞄准
射线全 AABB + 瞄准块 t 严格遮挡）→ m_angry + 30s 窗复用。**瞬移** =
`teleport_interval_min/max` 2/6 严格配对（存在性 = 能力门）+ 动作非状
态（MobState 零新增）+ 受击 100%（damage 只置旗，update 固定槽消费 =
帧序钉）+ 流契约 8 候选 × 3 抽 + 1 重掷 = 25 抽/尝试（全败亦消耗）+
平静 ±16/±8 自位锚 / 激怒 ±8/±4 朝玩家 min(12,d) 点锚（MC 64³/
16h17v 项目缩放偏离记档）+ 执行 = setTeleportCallback game 缝
（catchUpDisk → 候选须空旷 ∧ 16 格内有地面 → teleportTo + fall 复
锚 → 落点音 + DEBUG；全败 = swallowed 行）。**携格** = mob
`carries_blocks` + 块 `carriable` 双严格旗（缺省冷，stateful
carriable = init 点名拒）+ 10c-lite 集 {endstone, chorus_plant} + 摸
每帧 4×3×4（dy 含脚下）x→y→z 定序 + 首格遮挡 + 放 6000 帧全局长拍
（1/2000 tick 读）+ 死亡 100% 掉所携 + 0.9× 头顶立方渲染（itemMesh
复用，Opaque/Cutout 双 pass）。**生成** = mob_spawning_end.json 单种
族池（nether 孪生 fail-fast）= 10a 两相位槽消费；**掉落** = pearl
0-1 + eye 0-1（**生存唯一来源，10a/10b 记档闭合**）+ xp 5；**声音** =
enderman_teleport 单件（仅落点音；ambient/scream/stare 后移 10d）。后
移：定身/endermite（R11）/主世界·下界池 = 10d 复议。

**门 215**：S1 结构免疫第 96 次（≡ base19 逐字节 ×2 + savenorm EXACT +
终档恰白名单 3 键）/ S2 无输入末地臂 600f（spawn 4 + provoked ×2 +
teleported ×8 + carried ×4 = end 段恰 4 air 格 + ×2 RAW + 逐字）/
**火杀 demo 臂**（chorus 点火 → 燃烧链 → death f1016
(37.695915, 53.306664, 3.944854) + 所携 endstone x1 掉落逐字 +
pearl/eye 0-1 roll 0-0 合法 + ×2 RAW + 事件行逐字 + 掉落物 crop
目检）/ S6 七面点名 / canary 4 ×2 RAW + savenorm EXACT【sim 2 不
bump 实证】/ **2930/2930**（+31：enderman 族 26 / end 池 5）/ 性能
A/B 同档【旧 = e2e2071 v3.24.0 + g139：banner 实证 v3.24.0/v3.25.0 +
PERF-WIN 三窗 upd med 0.307/0.352/0.315 vs 0.366/0.349/0.334 + render
0.387/0.407/0.371 vs 0.455/0.433/0.379 + swap 15.8 同档 + changes-
Applied 9/3/1 全逐位 + 终档 savenorm EXACT 零 format 最强形】。PITS
407（demo 槽不回写 / 火脚级重
叠 / 携格带含脚下 / 遮挡首格语义 / 下探穿固体虚空 / 截图 679×744 /
按住 LMB 挖掘）。详见 `docs/archive/phase-3.25.0.md`。

## v3.24.0（2026-11-07）— V3-10b 末地内容族（基岩洋/岛群/chorus + 末影珍珠，门 214 全 PASS）

**矩阵大项 V3-10 末地相位 B/4**（10a 最小 fill 换真内容 + **项目首
个投掷物**；format 32 不变 / sim 2 不 bump（canary 4 EXACT）/ 零
config / 块 131→133 / atlas 170→172 零移位 / item 99→101 / 配方
112→113 / 声音 68→70）。

**末地内容**：基岩洋 = 全维 y[0,4] 实心带（下界地板镜像，非流体海
零 churn；跌落死因 Void→Fall + 珍珠逃生面）+ **岛群 pass**
（lattice 16 p0.15 独立盐，半径 5..12 顶 y 44..56，顶 2 层 + 下锥
2 层悬浮岛；air-only 谓词 + 中央足迹护栏保出口/平台；跨 chunk 走格
外扩 rMax）+ chorus 岛顶装饰（独立盐 p0.6，cross stateless +
chance_drops ≈ MC 比例，生长不做记偏离）+ biomes_end 单群系池
（10d 缝 + S6 面）+ end_stone_bricks（2×2 配方 count 4 MC 精确）。

**末影珍珠全链**：`throws_teleport` strict bool + 双音配对 +
one-right-click 8 面互斥 + material-only + init 交叉检；挂点 = 潜
行旋转后 placeBlock 前（MC 可交互块优先）；发射 = 眼位前 0.3 +
**投掷者速度并入**（MC 1.9+）+ **30 b/s（MC 1.5 b/t 绝对值）** +
冷却 60f 会话态；Projectile 尾参 teleports（箭/火球位级）+ stuck
复用 → **pearlLanding**：catchUpDisk + **2.5 球面均匀偏移**（counter
流帧锁定）+ hasRoomAt 上探 3 + fall 复锚（落地消坠伤）+ 落乘 +
probe 全败吞没不瞬移（偏离）+ **无自伤**（MC 5HP 偏离）+ 0.625×
青色发光立方渲染 + 2 wav。

**门 214**：S1 结构免疫第 95 次（≡ base19 + 恰白名单 3 键——10b
内容 = 生成参考零 diff 最强形）/ S2a 去程臂（10a demo 复跑 ×2
RAW + 逐字 + 末地新内容雾内可见）+ S2b 珍珠臂（`threw a
'ender_pearl'` + `pearl teleport to (1.395792, 54.893940,
-1.207714)` ×2 逐字 + 冷却窗内第二次静默 no-op 实证 + 心行全满 +
热栏 16→15 crop）/ S6 六面点名 / canary 4 EXACT / 2899/2899（+14）/ 性能 A/B 同档【banner 实证 v3.23.0/v3.24.0 +
PERF-WIN 三窗 upd med 0.575/0.585/0.594 vs 0.569/0.587/0.597 +
changesApplied 9/2/1 全逐位 + 终档 savenorm EXACT 零 format 最强
形】。PITS 406（lattice 位切 int 截断负值 = 模运算先于转换 + JS
三元误写 + stale assets 再踩）。详见 `docs/archive/phase-3.24.0.md`。

## v3.23.0（2026-11-07）— V3-10a 末地维度框架 + 真传送（项目第三维度，门 213 全 PASS）

**矩阵大项 V3-10 末地首相位**（V3-1 下界维度基建的**第二次消费**：
worldgen 纯函数 / 流送 / 光照 / 传送管线 / 存档 format 末地段；规模
≈ V3-1 Phase B 门 194；**format 31→32** / **sim 不 bump**（canary 4
EXACT 实证）/ 零 config / 块 127→131 / atlas 166→170 零移位 / item
98→99 / 声音 67→68）。

**入口 = A-lite**：`end_portal_frame`（空）/`end_portal_frame_eyed`
（嵌眼）= 两个 stateless 块（项目无 per-state 纹理机制）+ 数据谓词
`end_frame`/`end_frame_eyed`/`end_portal` + **5×5 水平环纯函数族**
（scan/激活 plan/8 候选中心反解/孤儿谓词）+ `ender_eye` 物品 +
**嵌眼交互**（持眼右键 frame：吞 1 眼 + 嵌眼态 + 环扫描；12 全 eyed
+ 中心 3×3 全 air → 中心激活 end_portal；frame 不可破坏 = 无去激活
路记偏离）。**最小末地** = 120×120 末地石台地 y[48,52] + 原点出口
门（worldgen 常活）+ 虚空无地板 + 入场平台 5×5 obsidian @(40,52,0)
**每次传送写入**（偏离 MC (100,49,0)）+ 落点朝西 +π/2。**传送管线
= 新函数**（travelPortal 原文零触碰位级红线）：1:1 独立原点无 8:1 /
返程 = 主世界出生点（床校验）/ 60f 倒计时 + 900f 冷却复用 / 死亡回
OW 重生 D8 三值化。**渲染 6 点**：暗紫黑 clear 0.05/0.03/0.06 / 雾
8..48 / 日月星云雨 inOverworld 门 / F3 +end / ambience 第 5 上下文
+ambience_end.wav / **u_ambient + u_caveAmbient 双 floor 0.25 按维
度**（门内捕获：shader 无光分支只读 u_caveAmbient，单设 = 全黑）。

**门 213**：S1 结构免疫第 94 次（≡ base19 ×2 + 终档恰白名单 3 键
{format,end,end_entities}）/ S2 去程 authored demo（12 嵌眼 →
`End portal activated at (3, 54, 3)` → `Traveled to end at (40, 53,
0)` ×2 RAW + 逐字恒等 + 热栏 13→1 眼）+ 返程零输入臂（`Traveled to
overworld (end exit)` ×2 + pos 出生点精确）+ 末地持久化臂（end 段 =
恰 25 obsidian 平台格集合精确）/ S6 五面点名 / canary 4 EXACT /
2885/2885（+29）/ 性能 A/B 同档【banner 实证 v3.22.0/v3.23.0 +
PERF-WIN 三窗 upd med 0.327/0.323/0.324 vs 0.533/0.573/0.575【旧树
同二进制复跑 2× spread = 环境漂移带，新在带内】+ render/swap 同档 +
changesApplied 9/3/1 全逐位 + 终档 savenorm 恰白名单 3 键
{format 31→32, end, end_entities} format bump 最强形】。PITS 405
（场景手术先探地形挖深坑 + shader 无光分支双 floor + GCC-16 假阳性
结构隔离 + 存档伪造只剥对应后键 + 场景派生同步 fluids）。记档偏离 10 项（A-lite 双块 /
平台位置 / 60f 倒计时 / 出口常活 / 无去激活 / 声音复用 / 水不蒸发 /
虚空 / 生成池空槽 / frame 生存不可得）。详见
`docs/archive/phase-3.23.0.md`。

## v3.22.0（2026-11-06）— V3-14d strider 骑乘（项目首个骑乘面，门 212 全 PASS）

**矩阵 ⬜ 项小→大推进**（V3-14 下界 mob 深化相位 D/4——骑乘机制
新基建收官 V3-14；**承载 = 会话态零 format**（31 不变）【唯一自洽
解：mob 不入档，骑乘关系无从持久，先例 = portal 冷却/飞行】/
**sim 不 bump**（canary 4 demo EXACT 实证）/ 零 config / 零新
wav / 零新纹理）。

**机制族**：strider.json +`rideable: true`（strict bool，walks_
on_lava 孪生）+ **Player `m_carried` 承载旁路**（setFlying 孪生：
wish 锁 + move/stepUp 旁路 + integrate no-op + fall 复锚每帧 = 落
乘 1.05 落差永不坠伤；保留 look/survival/swing——落乘进熔岩会烧 =
MC 语义）+ **MobEntity `m_ridden` + riderSteer**（runAi 顶部纯新
分支：WASD 直控——数据 speed 沿航向 + kRideDecel 0.9 漂停（停帧
= release+38 钉）+ ambient 流契约保留（1800f = 13 hiss 钉）+
**collidesWithEntities opt-out**【分离 pass 不顶坐骑——骑手盒设计
性重叠】）+ **Game ride pass 单收口**（落乘条件：坐骑死/玩家死/
飞行/面板/入水【熔岩面保持骑乘 = MC 定案】/portal arm + 承载 snap
背位 1.05【kRideCarryHeight，ride_height 不数据化 = YAGNI】+ steer
推送）+ **tryRide**（RMB 链 tryBarter 后，tryMilk 范式：rideable
数据谓词 + 已骑吞点击/异坐骑穿透 + 飞行拒骑）+ **Space/
LeftControl 落乘**（Space 挂双击飞行窗记录之前 = 落乘 tap 不武装
飞行）。渲染零改动（相机/第三人称/站姿体全自动——承载 snap 后）。
**MC 语义偏离 6 记档**：无 saddle 直骑 / WASD 直控（无 fungus
stick）/ 水伤下沉不做（下界无水 = 单测臂）/ 原地落乘 / 无脚步音
族 / 站姿背位（无坐姿 pose）。

**门 212**：S1 结构免疫第 93 次（≡ base19 ×2）/ S2 骑乘臂 =
authored demo（lava 沟场景【5 列贯通沟 + 4 桥格 + glowstone 灯排
y38——spawn 光门验算】+ strider 独占池变体：f60 strider 生
(40.5,32,34.5)【(40,34) 唯一合法列】→ f120 `rode the 'strider'` →
180° 转向（1257px 单帧）→ W hold 120f 沿桥 2.0 块 → f320
`dismounted the 'strider'` + 1.05 落桥零伤；×2 RAW 恒等 + 逐字
恒等 + 第三人称骑乘视觉；**终态权威 = 截图 + DEBUG 行【demo save
红线不回写 PITS 403①】**）+ 无输入臂 1800f ×2 RAW + savenorm
EXACT 7/7 / S6 点名（rideable 非 bool rc 1）/ canary 4 EXACT /
2856/2856（+9）/ 性能 A/B 同档【banner 实证 v3.21.0/v3.22.0 +
PERF-WIN 三窗 upd med 0.589/0.762/0.702 vs 0.690/0.679/0.709 +
changesApplied 9/3/1 全逐位 + 终档 savenorm EXACT 零 format 最强
形】。PITS 404（git checkout
还原 = 未提交功能回退两连踩 + 骑乘场景视线三件【沟/桥/灯】+ demo
瞄准换算【体心 + 0.0025 px/rad】+ Space 双击窗绕过 + 分离 pass
opt-out 首消费）。详见 `docs/archive/phase-3.22.0.md`。

## v3.21.0（2026-11-06）— V3-14c piglin 交易（barter 物物交换，门 211 全 PASS）

**矩阵 ⬜ 项小→大推进**（V3-14 下界 mob 深化相位 C/4——piglin
neutral 类上叠加交易面；零 format 31 不变 / **sim 不 bump**（canary
4 demo EXACT 实证）/ 零 config / 零新纹理）。

**数据驱动 barter 面**：`barter_input: "gold_ingot"`（单输入——
现行 MC 仅 gold ingot，无三档表 = 立项假设证伪弃做）+
`barter_outputs` 12 项加权表（MC 比例项目近似 Σ=249：gravel
80×8-16 / nether_brick 40 / obsidian 40 / string 20×3-9 / leather
20×2-4 / soul_sand 10×2-8 / quartz 10×5-12 / ghast_membrane 10 /
hoglin_meat 10 / water_bottle 6 / golden_apple 2 / **netherite_scrap
1 头奖**；数量区间 [1,64]——drops 的 0 概率式拒收 = 付金无货是
笔误）；解析纪律 = milk 配对 + wool_weights 字段族 + drops 两形式
互斥；**init 交叉检 = 纯函数 `checkBarter`**（checkWoolDrops 范
式：input/output 存在 + countMax ≤ maxStack，首个失败 mob 点名，
S6 实机 rc 1 直证）。

**交互 + 不激怒**：`tryBarter`（tryMilk 范式挂点 tryDyeMob 后：
targetMob kReach + 数据谓词 barterInput == 手持名；错手持 = 吞
点击 failed-milk 规则；非 barter mob = 穿透位级）+ `barterItem()`
无状态 roll（rollBarterOutput 单 uniform01 累计权重 + rollDropCount
单 nextMix，固定数量零流消耗）；**不激怒 = 结构性质**（激怒单收
口 damage()，barter 路径零调用——挤奶先例同款证明，无豁免字
段）；产出走地面 ItemEntity + V2-6 磁力（MC 抛出孪生，非直接入
包）；**barter.wav**（0x6A11 定种双音 chime 880→1318.5 Hz，
66→67 幂等既有 66 件字节恒等）。

**门 211**：S1 结构免疫第 92 次（≡ base19 ×2）/ S2 交易臂 =
**authored demo**（猪灵独占池变体 run 目录 + 1×3 视线沟【1×1 洞
被 rim 遮挡不可见】+ 12 拒生沟【scan 序钉生成 cell】+ 临时插桩
校准准星至 kReach 界内：f60 猪灵生沟 + f120 RMB = `bartered with
'piglin' (gold_ingot -> obsidian x1)` ×2 逐字恒等 + 终帧截图 RAW
恒等 + 热栏 crop 目检 gold 3→2 + 心满 HP 20）+ 无输入臂 1800f ×2
RAW + savenorm EXACT 6/6【**demo 引用存档槽 = 观察红线不回写终
档**——终态权威 = 截图 + DEBUG 行 PITS 403①】/ S6 点名 / canary
4 ×2 RAW + EXACT / 2847/2847（+14）/ 性能 A/B 同档【banner 实证 +
PERF-WIN 三窗 upd med 0.561/0.584/0.571 vs 0.552/0.567/0.567 +
changesApplied 9/3-2/1【win2 streaming 边界 real-dt 域】+ 终档
savenorm EXACT 零 format 最强形】。PITS 403（demo 观察红线 + run
目录 stale assets 两连探踩坑 + kReach 边界瞄准 + 生成环视线几何 +
ambient 每帧耗流 = roll 断言必 authored demo）。记档偏离：即时交易（MC 6s 检视）+ 表 = 项目
近似（golden_apple/hoglin_meat/netherite_scrap 特色项）。详见
`docs/archive/phase-3.21.0.md`。

## v3.20.0（2026-11-06）— V3-14b ghast（飞行爆炸射手 + 火球，门 210 全 PASS）

**矩阵 ⬜ 项小→大推进**（V3-14 下界 mob 深化相位 B/4——ghast =
bat 飞行数据旗 + skeleton 射击数据融合，首个爆炸射手，V3-14a 爆
炸机制首个游戏内消费方；零 format 31 不变 / **sim 不 bump**（canary
4 demo EXACT 实证）/ 零 config 键）。

**机制族**：`explode_radius` mob 数据属性（strict 非负；
>0 须 shoot_range>0；**ammo 规则豁免爆炸射手**——火球撞击即消
耗永不钉住 = pickup 不可达）+ `shoot_sound` 数据字段（默认
"shoot" 位级；ghast = "ghast_roar" 发射前咆哮；init 交叉检 wav
存在 = ambient_sound 孪生）+ **flyShoot 分支**（飞行 dispatch
内 `flying && shootRange>0` 数据门：seesPlayer【alive + detect +
kDetectVertical 2.0 + shootRange】→ hover 保持（水平速度归零）+
面向 + 4.0s 冷却发射；否则 bat cruise 原文——bat shootRange 0
位级红线，MobFlyShootTest 4 测钉）+ **火球 = Projectile 复用**
（damage 0 / hurtPlayer / 无 ammo 无 effect / explodeRadius +
explodeSource="ghast" 尾参缺省 0 = 旧调用位级）+ 爆炸接线三处
（stuck→explodeAt+kill 钉住即爆 / 玩家接触→explodeAt / mob 扫描
跳过爆炸弹 = 火球穿 mob MC 孪生）+ **火球渲染**（单色发光立方
0.4，色 1.0/0.55/0.15 无旋转，drawArrows 分支 + 惰性 mesh = 箭
三段孪生）。

**内容族**：ghast 数据（health 10 / speed 0.7 / detect 16 /
shoot 14@10 / cooldown 4.0 / explode_radius 3 / spawn_max_light
15 / hostile / flying / spawns_in_air / ambient ghast 8.0 /
drops ghast_membrane 0-1 / xp 5；model 1.5×1.75 五盒）+
ghast.png 16×16 自绘（atlas [9,10] 空格零移位 165→166；**物品
图标 = 独立 32×32 RGBA 透明底 PNG 非 atlas tile**）+ 声音 2 件
（ghast 环境嘶鸣 0x6600 1.4s 0.5 ambient class / ghast_roar 发射
咆哮 0x6E0B 0.5s SFX class；64→66 幂等）+ 下界池 +ghast weight
2（总 33，尾追加既有 5 条目位级）。

**门内捕获（字体修复并入）**：V3-14a 的「第二爆炸字」= 爽
U+723D（笔误，本意 炸 U+70B8）——charset/表/PNG/单测四处一致
地错，门 210 死因屏 crop 首渲染 `被 ghast 的爆炸杀死` 显「爆?
杀死」。修复 = charset 换入 0x70B8（0x70B9..0x7206 九 glyph 重格
+1，0x7248 起净零）+ 生成器重跑（TTF pin / legacy 33 位级 /
top-512 红线 131072 px 恒等）+ font.png FNV 重钉
94cbd43cfb750568→0fce6a0766e78125 + test_font cell 锚重钉——
**修复后 S1 ≡ base19 字节恒等**（锚场景无消费方文本）。教训 =
一致性 pin 抓不到码点语义错，E2E 目检 = 语义门（PITS 402④）。

**门 210**：S1 结构免疫第 91 次（≡ base19 ×2 + savenorm EXACT）
/ S2 下界决定性场景 v5（1800f ×2 截图 RAW + savenorm EXACT +
终档 9/9：架 4/4 破【玩家接触爆破——爆点 = 火球盒底位】/ 平台 +
顶盖 obsidian 全存【br 3 = r 3 恰界】/ 玩家 4 接触死 deathCount 1
/ fires [] / nether_entities []【mob 不入档 = format 30 设计】）
+ 决定性时间线（f240 ghast#1 生 + 4s 冷却发射 + 顶盖卡壳自爆自伤
5 + f870/f1110 同确定性 cell 新生 = 非重生【mob 无死亡日志 + XP
orb 120s 寿命反证】）/ S2 视觉（492f 火球发光立方 + ghast 五盒
同框 + 死因屏 `被 ghast 的爆炸杀死` 全行正确）/ S6（shoot_sound
缺 wav init rc 1 点名）/ canary 4 demo ×2 RAW + savenorm EXACT /
2833/2833（+12）/ 性能 A/B 同档【banner 实证 + PERF-WIN 三窗
upd med 0.578/0.604/0.640 vs 0.549/0.530/0.557 + changesApplied
9/2-3/1【win2 streaming 边界 real-dt 域】+ 终档 savenorm EXACT
零 format 最强形】。记档偏离：火球 = 箭弹道（近程弧线补偿朝上，
MC 火球近直线）+ 射手自伤（MC 免疫自身火球）+ 无击退/破碎无掉
落/点火 100%（V3-14a 继承）。详见 `docs/archive/phase-3.20.0.md`。

## v3.19.0（2026-11-06）— V3-14a 爆炸机制（衰减伤害/破块/点火 + --explode seam，门 209 全 PASS）

**矩阵 ⬜ 项小→大推进**（V3-14 下界 mob 深化相位 A/4——爆炸机制
族核心，ghast 前置；零 format 31 不变 / sim 不 bump / 零 config 键）。

**纯核心 `world/explosion`**（fire_spread 纯函数模型，单测可测）：
`scanExplosion` = 包围盒 (2r+1)³ 确定 x/y/z 序扫描（格中心 +0.5 距
离），`broken` = in-range ∧ solid ∧ destroyable ∧ **`blast_resistance`
< 局域强度**（radius − dist：中心满强度、边界 0——**obsidian 数据
3.0 在 radius-3 blast 全域存活** = 门的 a2 臂对照实证）；`ignited`
= in-range ∧ 未破 ∧ `flamable`（fire spread 同款谓词；100% 确定点
火，MC creeper 25% roll = 记档偏离）。`explosionDamage` =
`ceil((1 − dist/radius) × 12)` 整读（中心 12、界 0）。破碎**无掉
落**（MC 孪生）/ **流体不触碰**（记档）/ **无击退**（项目无击退面，
V3-15 先例）。

**接线 `Game::explodeAt`**：setBlock 收口（破碎→air / 点燃→fire +
relight）+ 碎片 spawnBreak（共享计数器黄金比种子契约）+ 中心
spawnFire 余烬 + **boom 音**（生成器 0xB007 定种，音效 63→64，
peak pin 32574）+ 衰减伤害（玩家 damage 单收口【创造门自动免
疫】+ mobs MobEntity::damage）+ 死因「死于爆炸」/「被 <name> 的爆
炸杀死」（**字体 +2 字形 爆/炸 428→430**：扩展区 re-cell +2、
top-512 红线位级、test_font 全锚重钉含 PNG FNV 重钉）。

**块属性 `blast_resistance`**（数据驱动，hard rule 3）：strict 非负
数（absent = 0.0 = 任意 blast 可破；负值/非数 init 点名——S6 实机
rc 1 实证）；**obsidian = 3**（传送门黑曜石在爆炸中存活 = 数据而
非代码）。

**触发 = `--explode x y z [r]` CLI seam**（--weather 孪生：帧 0 应
用 = 整场回放可见 + 严格有限数解析 + r 缺省 3 + 管理旗/
demo-author/demo-replay 三重互斥）——**游戏内暂无消费方**（V3-14b
ghast 火球 = 首个真触发，调同一 explodeAt，sourceName 命名死因）。

**门 209**：S1 ≡ base19 逐字节 ×2 + savenorm EXACT（**结构免疫第
89 次**——爆炸无 seam 全冷 + obsidian 数据仅 init 解析 + boom 音频
不入帧态）；S2 密封石室双臂【场景 = fresh 60f 去火 + 14×14×7 石
室填充 + 玩家空气口袋【森林群系到处可燃 = 石室封死火域，PITS
401①】：a1 = blast (0.5,56.5,0.5) r3 → **health 18**（dist 2.5 →
dmg 2 衰减整读）+ (1,56,0)/(2,56,0) 破【journal 回生成态 = 条目消
失 PITS 401②】+ **(3,56,0) 界上存活**（local 强度 0 硬界 pin）+
(5,56,5) 房外石墙残留对照 + entities=0 无掉落 / a2 = 石室 + obsidian
柱 (2,56,0)/(2,57,0) → **obsidian 双双存活** vs 邻格 (1,56,0) 破
（数据属性对照）；双臂截图 ×2 RAW + savenorm EXACT×2【fires[] = 地
下生成火簇回落 real-dt 域 PITS 401③，savenorm 权威】】；S2b canary
4 demo ×2 RAW + savenorm EXACT（sim 不 bump 实证）；S6 点名【init
负 blast_resistance → rc 1「'blast_resistance' must be >= 0」】；全
套 **2821/2821**（+10：ExplosionTest ×5 / BlastResistance ×4 /
SoundExplosionTest ×1 + 死因行 2 断言并入既有测试），`-Werror` 0 警
告；性能 A/B 同档【旧 = b71cb5e v3.18.0（git archive）+ g139 vs 新 +
g139 banner 实证，PERF-WIN 三窗 upd med 0.393/0.550/0.593 vs 0.571/0.549/0.588 + render 0.494/0.661/0.675 vs 0.651/0.678/0.675 + swap med 15.571/15.269/15.215 vs 15.212/15.282/15.249（旧侧 win1-3 swap max ~32 ms = 一次性 OS 停顿 med 正常）+ fluid·fires·spread·ents·stream 同档 + **changesApplied 9/3-2/1**【win2 ±1 = streaming 边界 real-dt 域，v3.14.0 先例】+ **终档 savenorm EXACT**（零 format = 最强形——爆炸全冷路径结构性零成本）】。PITS 401（爆炸门
场景三纪律：石室封火域 + journal 回生成态消失断言法 + 地下火簇
savenorm 域）。详见 `docs/archive/phase-3.19.0.md`。

## v3.18.0（2026-11-06）— V3-11b 创造物品栏（E 面板双 tab + 前缀搜索 + 点击授予，门 208 全 PASS）

**矩阵 ⬜ 项小→大推进 4/6，V3-11 创造模式全项完成**（零 format 31
不变 / 零 config 键 / sim 不 bump【canary 4 demo 回放 EXACT 实证】）。

**入口 = 创造下 E 面板双 tab**（MC 同款整合）：标题行「物品栏/背包」
按钮（激活橙框）；**物品栏 tab** = 前缀搜索框 + 224 物品 name 升序
过滤 + 6×4 网格滚轮翻页（1 tick = 1 项，双向夹紧；搜索缩表重夹）+
左键满组/右键单个**授予选中热栏位**（同物合并清耐久附魔 / 异物替换
/ 满同物 no-op / pickup 音）；**背包 tab** = 既有 E 面板内容位级。
**生存 E 面板零变化**（门 208 旧≡新二进制字节恒等实证）。

**文本输入 = 项目首个打字面**：`SDL_StartTextInput` 仅「创造 +
Inventory 面板」期开（SDL3 默认关 = 无输入运行零 TEXT_INPUT = 结构
性免疫根）；Input 帧态 frameText/frameBackspace（wheelDelta 孪生，
frameBegin 重置，256 raw 界）；词表 [a-z0-9_] 小写归一 + 24 上限 +
光标帧锁定闪烁；**不入 tap 词表**（demo 录制 schema 8 型不变，录制
的 demo 永不打字）；IME 组合不处理（记档偏离）。**搜索焦点数字语
义**：物品栏 tab 开时数字键 = 文本入搜索不切热栏（MC 搜索焦点；槽
位须在开面板前选——门内 xdotool 实证捕获：数字键双投递 KEYDOWN+
TEXT_INPUT = "app2" 无匹配假象，PITS 400③）。

**demo author 面扩展**：timeline `mouse_move` 可选 x/y 绝对位（文件
schema 既有，author 面补齐 = 面板点击坐标；缺省 0 = 4 canary 位级
恒等）+ 键词表 +g（Python 镜像同步）。

**门 208**：S1 ≡ base19 逐字节 ×2 + savenorm EXACT（**结构免疫第
88 次**）；S2 面板臂【全 headless demo，捕获点击先行 = PITS 400①：
a0 vs a1 = 55 360 px（面板在）/ a1 vs a3 = tab 视图差 + tab 行 107
px / a2 授予臂 fresh + 退出写 = **存档 slot0 = allium×64** + 背包视
图 slot0 渲染 / **a7 生存 E 旧≡新字节恒等** / 全臂截图 + 存档 ×2
RAW】；**S2c v3.17.0 补证臂**【门 207 空转断言的真 E2E（PITS
400①②）：a4b = fresh + 捕获 + G + 低头 + 单帧 LMB → 退出写档
**creative=True / (0,53,0)=air 即挖 / entities=0 无掉落 / pos 飞行
不坠** + a5 生存对照 = 同窗口未破】；**S2d 搜索 xdotool 臂 ×2**【项
目首个文本输入 E2E：捕获 + G + 2（开面板前选槽）+ E + 打字 "app" +
cell0 点击 → 退出写档 **slot1 = apple×64** + 退出截图 cell0 = 苹果
图标；调试日志实证 frameText 逐字符送达】；S2b canary 4 demo ×2
RAW + savenorm EXACT；S6 无（零新解析面）；全套 **2811/2811**（+10：
CreativeLayout ×2 / CreativeTabUI ×6 / Input frame ×2）；性能 A/B 同
档【旧 = 57c417e v3.17.0（git archive）+ g139 vs 新 + g139 banner 实
证，900f 无输入独占：PERF-WIN 三窗 upd med 0.635/0.624/0.753 vs 0.712/0.586/0.605 + render 0.665/0.712/0.841 vs 0.714/0.584/0.649 + swap med 15.173/15.049/14.820 vs 14.927/15.240/15.177（新侧 win2 swap max 208 ms = 一次性 OS 停顿 med 正常）+ fluid·fires·spread·ents·stream 同档 + **changesApplied 9/3/1 逐位恒等** + **终档 savenorm EXACT**（零 format = 最强形——面板/文本输入无输入全冷路径结构性零成本）】。PITS 400（demo 无初始指针锁定 +
save-spec 红线禁 F5 + xdotool 数字双投递 + X grab 阻塞外来屏捕 +
门机窗口 679×744 口径）。详见 `docs/archive/phase-3.18.0.md`。

## v3.17.0（2026-11-06）— V3-11a 创造基础（G 模式切换 + 不死 + 飞行 + 即挖，门 207 全 PASS）

**矩阵 ⬜ 项小→大推进 3/6**（V3-11 创造模式相位 A/2；B = 创造物品栏
v3.18.0）。

**设计定案**（11-06 编排者定案）：① 创造 = **存档 flag**（format 31
`player.creative` strict bool **恒写**，dimension 恒写先例；pre-31 载
迁 false；format 30 文件携键 = version lie 点名）= MC per-player
/gamemode 语义，**非**会话态；② 切换键 **G**（GameKey::G 插在
FlyToggle 前，kKeyCount 尾钉不变；locked pointer + 仅存活 = C 键范式）；
③ 不死 = **全源伤害门**（Player::damage 单收口——lava/fire/fall/mob/
arrow/starve/void 全走这里，一处门控免疫）；④ 饥饿条**静态满**
（setCreative 入场填饱 + 衰减/饥饿 tick 用 `if (!m_creative)` 包裹
——**非** early return，保护下方 lava/kill-plane 段）；⑤ **即挖** =
creative 强制 duration 0（复用既有 data-driven instant-break 路径）；
⑥ **无掉落/无 XP/无挖掘耐久**（breakBlock gate 只包 4 段 drops/chance
drops/XP——粒子与音效保留 = 创造挖掘手感；声明前移重构 = gate 尾段
可见性）；⑦ **放置不消耗**（3 点：cloth/顶部 slab 合并/常规放置）；⑧
创造开/关 = setFlying(true/false)（模式拥有飞行过渡，双击 Space 保留
作模式内 toggle）；⑨ **sim 不 bump**（创造 = 存档态增量功能，survival
路径位级不变；canary 4 demo 回放 savenorm EXACT = 门内实证）。

**门 207**：S1 ≡ base19 逐字节 ×2 + savenorm EXACT（**结构免疫第 87
次**——creative 默认 off = 零存档键差零像素）；S2 creative demo 臂【场景 = 60f fresh 去火 + format 31 creative
true；截图 ×2 RAW + 装载态断言；⚠ 门 208 复盘：F5 结果捕获被
save-spec 红线禁用 + demo 无初始指针锁定 = "已挖/无掉落" 断言空
转（PITS 400①②），真 E2E 补证 = 门 208 S2c a4b 臂】；S2b **canary 4 demo ×2 RAW +
savenorm EXACT**（sim 不 bump 实证）；S6 点名【实机 format-30 文件携
creative → rc 1 点名 + 单元 ThreeWay 三面：30 携键/31 缺键/非 bool】；
全套 **2801/2801**（+6：SaveFormat31Test ×3 + PlayerTest.Creative
×3）；`-Werror` 0 警告；性能 A/B 同档【旧 = 6b031c5 v3.16.0 + g139 vs
新 + g139 banner 实证，900f 无输入：PERF-WIN 三窗 upd med
0.578/0.667/0.650 vs 0.690/0.596/0.652 + render/swap 三窗同档（新旧各
一窗 swap max ~32 ms = 一次性 OS 停顿 med 正常）+ fluid·fires·
spread·ents·stream 同档 + changesApplied 9/2-3/1【win2 ±1 = streaming
边界 real-dt 域，v3.14.0 先例】+ **终档 = 恰白名单 2 键**（format
30→31 + player.creative missing-in-old，零其它漂移——format 跳变版
v3.4.0「恰白名单 4 键」同款）】。PITS 399（format bump 测试面 = 4 类
降级点：strip helper ×5 文件本地复制 / 手动 withoutKey 链 / inline
rebuild / gzip identity rebase——首跑全套 61 失败全此族；demo save-spec
结果捕获 F5 机制）。详见 `docs/archive/phase-3.17.0.md`。

## v3.16.0（2026-11-06）— V3-15 生存/表现小批（netherite 火免疫 + portal UV 动画 + 蒸汽粒子，门 206 全 PASS）

**矩阵 ⬜ 项小→大推进 2/6**（三小项打包 1 版，零 format / 零新
config 键依赖【portal.anim / portal.steam = 严格 bool 默认 true，
off = 位级 pre-v3.16.0】）。

① **netherite 火·熔岩免疫**（V3-4 记档后续兑现）：物品可选属性
`fire_resistant`（strict bool，armor-only = parseArmor 守卫，shears
tool-only 孪生；4 件 netherite 护甲 = true）——任一件穿上（setArmorSlot
单收口 ANY 派生）：熔岩 burn tick 不落地（accumulator 重置 = 脱件无
余伤）+ 火/熔岩接触不再点燃（Phase-57 重应用门控）+ 既有火效果伤害
tick 抑制（视觉保留 = MC fire-resistance 语义）；**熔岩中死亡原因行
覆写**（fire 效果致死一击且 inLava → 「死于熔岩」= MC 元素归因，门内
实证：首版死因 = 效果 tick 报「死于燃烧」）。**击退抗性弃做**（项目
无玩家击退面 = 数据字段无消费方，记档）。
② **portal UV 动画**：纹理水平 2 周期重写（生成器 x%8，左半 ≡ 右半，
其余 164 张位级不变）+ basic.frag tile-rect 分支（`u_portalAnim` 门控
+ `u_portalPhase = m_frameCount % 120 / 120` frame-locked，tiled /
legacy 双路径统一于 sampleUv；off = 分支不执行位级恒等）+ init
fail-fast（atlas 缺 tile 点名）。
③ **蒸汽粒子**：`updatePortalSteam` 每 6 帧玩家脚下 8×8×9 盒（y 非
对称 feet-4..+5 = portal 顶框行，门内首版盒 y+3 漏顶行 = 场景实证
捕获）扫描 portal 格，1/10 确定性 roll（fire 种子契约 + 专属盐）发
射 1 粒（portal 纹理 0.12 半透立方体走 blend pass，-0.4 m/s² 慢升，
0.8-1.4 s 寿命）。

**门 206**：结构免疫第 86 次【S1 无输入 ≡ base19 逐字节 ×2 + savenorm
EXACT——shader 分支 / 蒸汽 / 免疫全为死亡态·场景态渲染面】；S2a
**portal 场景**（场景手术 4×5 框 + 2×3 内域 + states 6 条目【stateful
必带 PITS 383】+ 地板平台 + **去火**【种子 fresh 档自带 3 团地下火 =
real-dt 污染 PITS 398①】）：存档 ×2 RAW 恒等【无火域】+ **动画关
控制臂 ×2 RAW 恒等**【管线确定性】+ 动画开单跑目检【漩涡流动 + 蒸汽
点】+ f400/f460 diff 3560 px【scroll + steam 在动】——截图 ×2 恒等
对动画内容非有效断言（退出帧 off-by-one × 高频纹理 = PITS 398③）；
S2b **熔岩池双臂**（挖掘池 + 石环 + 流体日志 [x,y,z,7,'lava']
【PITS 398②：fluids 数组 level 前置于 name / armor = 命名 dict 非
数组】）：免疫臂 health 20 ×2 + 存档 ×2 RAW【免疫 = 确定性】/ 对照臂
health 0 ×2 + 死亡屏「死于熔岩」目检【real-dt 场景域 = health 断言
权威】；S3 全套 2795/2795（+3：FireResistantArmorField /
SteamIsDeterministicAndRises / NetheriteArmorSuppressesLavaBurn）；
性能 A/B 同档【旧 = 55da85d v3.15.0 + g139 vs 新 + g139 banner 实证，
900f 无输入：PERF-WIN upd med 0.541/0.648/0.648 vs 0.650/0.657/0.583
+ render/swap 三窗同档（新侧 win1 swap max 31 ms = 一次性停顿 med 正
常）+ fluid·fires·spread·ents·stream 同档 + **changesApplied 9/3/1
逐位恒等** + **终档 savenorm EXACT**——蒸汽/免疫/scroll 无输入场景全
惰性 = 结构性零成本】。PITS 398（场景手术三坑 + 动画内容门协议 +
MC_LOG 单参）。详见 `docs/archive/phase-3.16.0.md`。

## v3.15.0（2026-11-06）— V3-13 死亡统计屏（死因行 + 会话统计 5 行 + 字体 +1「于」，门 205 全 PASS）

**矩阵 ⬜ 项执行（小→大序第 1 项）**：死亡统计屏 / Game Over 屏。
HP 归零 → 冻结画面死亡屏从 2 行（你死了！/ 按 R 重生）升级为**统计
屏**：① **致死原因行** = `Player::damage` 新增 `DamageInfo` 重载
（2 参旧签名保留 = 委托 None 源，pre-V3-13 位级路径）——**仅致死一
击**的来源记录：Fall「死于高处掉落」/ Void「死于世界之外」/ Starve
「饿死」/ Lava「死于熔岩」/ Drown「死于落水」/ Arrow「被箭射死」/
Mob「被 <name> 杀死」（hit callback init-capture killerName）/
Effect「死于燃烧（fire）/死于毒（poison）/死于药水（其余）」——全部
经 5 个 damage 调用点接入（箭/mob 近战/溺水/效果 tick/内部
fall·starve·lava·void）。② **会话统计 5 行**：采挖/放置/击杀 =
Game::SessionStats（breakBlock 单收口 / placeBlock 双 setBlock 点
含 slab 合并 / 玩家致死近战 + 箭；**不存档** = 记档偏离，MC
scoreboard 持久）+ 行走 = Player 水平位移累计（传送阈值守卫）+ 存活
= 帧锁定 m_frameCount/60。③ **Game Over 屏 = 死亡屏本身**（无
hardcore，R 重生既有 = 记档偏离 MC 双屏）。④ **字体 +1 字形「于」**
（CJK_CHARSET 427→428，扩展区重排格：0x4E8E 之后全部 +1，top-512
红线位级恒等，atlas 尺寸不变 256×2112；死因字符串全字体表内——
摔/掘/外缺字教训 = 逐字查表定措辞）。固定物理偏移行距（fScale 仅
font_scale 不含 ui.scale = PITS 397）。

**门 205**：结构免疫第 85 次【S1 无输入 60f ×2 ≡ base19 逐字节 +
savenorm EXACT——死亡屏不渲染 + 字体扩展区重排格无输入零像素（教程 =
locked pointer 才显）+ 双跑复核】；S2 **坠落死亡场景臂**【场景手术
挖 24 格竖井（种子 123456789 spawn 列）+ 无输入 ×2：**RAW 恒等 +
savenorm EXACT**（fall = 帧锁定物理 = 死亡帧确定；saturation = 已知
real-dt 字段 savenorm 域）+ 死亡屏裁剪目检（「死于高处掉落」+ 5 统
计行全渲染）】；S3 全套 **2792/2792**（+5：DeathCauseKillingBlow
Wins / DeathCauseMobCarriesKillerName / DeathCauseLineFontSafe【全
9 死因字符串钉】/ RespawnClearsDeathCause / StatsDistanceWalkAnd
TeleportGuard）+ test_font 4 锚重钉（PngBytes FNV / TablePinned /
ExtendedCellsPinned【含 vector 末元素无尾逗号 = 正则必漏单查】/
UvCellPositions）；S6 无（零新解析面：DamageInfo = 内存态不序列化，
SessionStats 不存档）；性能 A/B 同档【旧 = d61a757 v3.14.0 + g139 vs
新 + g139 banner 实证，900f 无输入：PERF-WIN upd med
0.588/0.634/0.629 vs 0.655/0.624/0.575 + render/swap 三窗同档 +
fluid·fires·spread·ents·stream 同档 + **changesApplied 9/3/1 逐位恒
等** + **终档 savenorm EXACT**（死亡屏/统计 = 死亡态渲染面，无输入
零触碰）。PITS 397（fScale 不含 ui.scale + 加字工作流 + 重排格零像
素论证）。详见 `docs/archive/phase-3.15.0.md`。

## v3.14.0（2026-11-06）— V3-8b 手柄按键映射定案（L3 蹲伏 / RT 疾跑 / B 飞行专用钮 + 首个 sim bump 1→2，门 204 全 PASS）

**用户 11-06 报障后续定案**（V3-8a 连接修复后）：「按键习惯问题需
进一步确认（可能有多套常见映射表?）」→ 调研 + 三项习惯定案。**调研
结论**：SDL3 GameController 按手柄数据库把 **Xbox / PlayStation /
Switch 三大布局归一化到同一逻辑位**（PS 的 ✕ = SOUTH = Xbox A 同角
色）= **现行单表已结构性覆盖三大布局，无需多表**（山湾克隆 = 数据库
已识别，V3-8a 实机 open 铁证）。**用户三项定案**（config 全自定义 =
定案**不做**）：① **L3 = 蹲伏**（补键盘 LeftControl 的手柄缺口，
MC Java 惯例；`SDL_GAMEPAD_BUTTON_LEFT_STICK` → 虚拟 Ctrl 既有源位
掩码模式）② **RT 扳机 = 疾跑**（`RIGHT_TRIGGER` 轴 0.2 死区数字化
按住 = Shift，左摇杆移动同款；B 位解放）③ **B = 飞行 toggle 专用
钮**（新 `GameKey::FlyToggle` **仅手柄键** = `mapKey` 永不返回【黑盒
256 键扫描单测钉】，单击直接 `setFlying(!)` 无双击窗；键盘双击 Space
不变，A 双击仍生效 = MC 同款冗余记偏离）。LT / R3 / GUIDE / Dpad
上下左保持未映射（记档）。

**首个实际 sim bump（1 → 2）**（v3.13.0 定案的 bump 纪律「输入路由」
条款首实例）：录制的 gamepad 事件按**当前解释**重放——sim-1 demo
携 L3/RT/B 事件将静默变行为 = 必须 bump。`tools/make_example_demo.py`
的 Python 镜像常量 `SIM_VERSION` 漏 bump = **门 204 S2 首跑捕获**
（example.dmo 静默写成 sim 1 + recorded_with 3.14.0 自相矛盾 → sim
门点名拒收，金丝雀先于用户发现，PITS 396①）。4 个 canary demo 重新
author（sim 2，字节可复现）。

**门 204**：结构免疫第 84 次【S1 无输入 60f ×2 ≡ base19（committed
config ui.scale 1.5 口径）逐字节 + savenorm EXACT——手柄映射 = 无手柄
冷路径 + GameKey 尾扩零像素零存档面】；S2 sim-2 canary 4 demo ×2 RAW
+ savenorm EXACT；**S2b 首个 bump 实证**【git 0705139 的 sim-1 canary
= rc 1 点名「demo sim 1 != this build sim 2 (recorded with 3.13.0)」
+ `--demo-force` rc 0 WARN + **force 回放终档 savenorm ≡ sim-2 canary
= 纯键鼠录制行为恒等最强形**】；S3 全套 **2787/2787**（+3：
RTTriggerSprintDigitize【0.2 死区边界 6553/6554 同款钉 + LT 恒零】/
L3CrouchHold / FlyToggleKeyIsGamepadOnly【256 键黑盒无孪生 + B 边
缘精确】+ TriggersUnmapped re-pin 为 LeftTriggerUnmapped）；性能 A/B
同档【旧 = d9c8c0a v3.13.0 + g139 vs 新 + g139，900f 无输入：PERF-WIN
upd 0.665/0.647/0.705 vs 0.671/0.628/0.693 + render/swap 三窗同档（旧
侧 win1 swap max 15139 ms = 一次性 OS 停顿 med 正常）+ fluid·fires·
spread·ents·stream 同档 + changesApplied 9/2/1 vs 9/3/1【win2 差 =
real-dt 追赶抖动 PITS 385 域】+ **终档 savenorm EXACT**】。PITS 396
（Python 镜像常量漏 bump 门内捕获 + `kKeyCount` 尾钉 = GameKey 加成员
须同步【漏 = 新键索引越出 array 边界 -Werror 零报警静默 OOB】）。
**实机验收归用户**（门强度记档同款 V3-8：本环境全合成事件单测，山湾
克隆实机手感 = 你终审）。详见 `docs/archive/phase-3.14.0.md`。

## v3.13.0（2026-11-06）— demo 回放独立 sim 版本（旧 .dmo 零迁移直放，门 203 全 PASS）

**用户报障**：`--demo assets/demos/example.dmo` 被拒「demo recorded
with v3.11.0, this is v3.12.0 - incompatible」——旧设计把**构建版本串
当模拟语义的代理**，v3.12.0 这种纯渲染常量版也误杀全部已录 demo。**
用户定案设计**：demo 有独立版本号，游戏版本变化时按兼容性判断。

**实现**：① `kDemoSimVersion = 1`（`src/core/demo.hpp` 新常量）= 回放
准入**唯一判定轴**，独立于 TM_VERSION；**bump 纪律**：仅帧锁定模拟
语义变更（update 循环/物理/AI/流体/生成/输入路由/demo pacing）才 +1
——纯渲染/UI/音频/内容数据/文档版本**不 bump**（v3.12.0 阴影缩尺 =
不 bump 实例）。② `recorded_with`（录制时 TM_VERSION 串）**仅信息展
示**（回放日志行 + 结束面板新「recorded with」行，4 行 → 5 行布局）
永不拒播。③ **legacy 映射零迁移**：旧 schema（仅 `version` 键）→
sim := 1 + recorded_with := version（旧严格门已证 record-build ==
replay-build = sim 同期系，用户手头全部 .dmo 继续可放）；新 schema =
{sim int ≥1, recorded_with 非空串, world, dts, events}；`version` 与
sim/recorded_with 同现 = 混用 schema 点名拒。④ sim 不匹配 = rc 1 点
名「demo sim X != this build sim Y (recorded with Z) - the simulation
semantics differ」+ `--demo-force` 逃生门（仅 bypass sim 门，schema
严格性保留，WARN 点名；无 `--demo` 带 force = rc 2 点名）。⑤ **canary
纪律**（PROTOCOL.md E2E 新条）：4 个 shipped demo = 模拟语义金丝雀，
每版门内回放 savenorm EXACT——核心改动破坏 canary = 必须 bump + 重新
author（时间线 JSON 字节可复现）。

**门 203**：结构免疫第 83 次【S1 无输入 60f ×2 ≡ base19（committed
config ui.scale 1.5 口径）逐字节 + savenorm EXACT；用户工作树
ui.scale 2.0 口径 ×2 自洽——S1 首跑假失败 = 用户 config 调值（锚在
1.5 时代捕获，时钟/XP 文本 4/3× = 2.0/1.5 精确比），PITS 394】；S2
四 demo（含重新 author 的现行 schema 294 B example）回放 ×2 RAW 恒等
+ savenorm EXACT；**S2b 用户报障场景实证**【git 4531c42 旧 example.
dmo（legacy `version` 3.11.0）回放成功 + 终档 savenorm ≡ 现行 schema
回放 = 映射行为恒等】；S2c sim-2 拒 rc 1 点名 + force 旁路 rc 0 WARN；
S6 force 无 --demo rc 2 点名；性能 A/B 同档【旧 = 648b80d v3.12.0 +
g139 vs 新 = v3.13.0 + g139，900f 无输入：PERF-WIN upd/render/swap 三
窗同档（新侧 win1 swap max 14955 ms = 一次性 OS 停顿 med 正常）+
fluid·fires·spread·ents·stream 同档 + **终档 savenorm EXACT**（changes-
Applied 9/2/1 vs 9/3/1 窗口差 = real-dt 追赶抖动 PITS 385 域，savenorm
权威恒等）】。2784/2784（2777 基线 + V3-8a 手柄 open 修复 1【用户
11-06 并行提交 d5a5ea6，本提交前已入树】+ 本版 +6：SimPinnedToOne /
ParseLegacyMapping / ParseCurrentSchema / ParseRejectMixedSchemas /
ParseRejectCurrentSchemaFieldShapes / LoadLegacyFileAccepted +
LoadVersionMismatch 重钉为 LoadSimMismatch）。PITS 394（S1 锚假失败 = 用户 config 调值 + 时钟
dawn 对齐映射误读 start 0.25 = 10:48 非 6:00；`day_cycle.hpp`
startTimeOfDay 默认注释「// noon」随版更正）。4 个 demo 资产重新
author（现行 schema，字节可复现 md5 恒等）。详见
`docs/archive/phase-3.13.0.md`。

## v3.12.0（2026-11-06）— 实机报障：mob 阴影缩尺（1.5× → 1.0×，物理不动，门 202 全 PASS）

**用户实机报障**：mob 阴影方块比 mob 大，落体瞬间阴影角还压在高处
方块上 = 看似"根据阴影判定落地"。**核实定案**（一次性探针后撤，
PITS 383 纪律）：物理 = **AABB 质心规则**（探针实测失接地 = 身体
中心恰好越过边缘 0.3000 = 身体半宽，**Minecraft 同款**——MC 亦
允许半身悬空，质心过边即落）；`blobShadow*` 仅渲染消费（代码核
证），阴影零参与判定。所见 = 1.5× 阴影（半宽 0.45）> 身体半宽
0.3 的落体瞬间视觉状态。**用户定案处置 = 缩小阴影视觉**：
`kMobShadowSideScale 1.5f → 1.0f`（= 身体同宽足迹，消除压边误导；
玩家第三人称阴影同常量一并收拢）。1 常量 + 3 测试重钉 + 版本三件
套；2777/2777 不变。门 202 = **结构免疫第 82 次免重锚（对
base19）**【S1 无输入 ≡ base19 逐字节——锚无 mob 无玩家阴影 = 阴
影从不渲染】+ S2 阴影视觉臂【mobs-on 场景 ×2 RAW 恒等 + **前后
对比 2896 px = 3 簇地面阴影带，单调变亮审计 0 违例（阴影只缩
小）+ 天空带零 diff + crop 3× 目检**（同场景同羊同位，其余像素零
变化）】+ 性能 A/B 同档【upd 均值 0.528 vs 0.503 噪声带 +
changesApplied 9/1/3 逐位 + 终档 savenorm **EXACT**】。PITS 392
（DEBUG 级事件行 grep 假阴性——门跑 --log-level debug 或 crop 目检
互补）。详见 `docs/archive/phase-3.12.0.md`。

## v3.11.0（2026-11-05）— V3-9 demo 回放（注入内核 + 编写/录制/回放三入口 + 观众只观察路由，门 201 全 PASS，**3.x 矩阵收官**）

**用户 11-05 BACKLOG 定案条目执行**——① **demo 文件格式**（V3-7
确定性 gzip 容器 10B 头全钉 + 严格 schema：version 指纹 TM_VERSION
【不匹配拒播点名】/ world fresh{seed}|save{slot}【双键·缺键·槽字符
集拒】/ dts【负·>0.25 拒】/ events【乱序·悬空帧·未知 type 带 8 词
表·payload 面全严格】；**零 format** = 独立文件族）② **注入内核**
（core/demo 新：DemoPlayer 注入 = Input 公开 handle* 入口【与
xdotool 跑的游戏内路径同形，游戏逻辑零触碰】/ dtForFrame = **demo
dts 唯一时间源** / rendersPerFrame【0.5→2·1→1·2→1】/ nextSpeed
0.5→1→2 循环 纯函数）③ **双 Input 路由 = 架构边界**（回放：观众
Input 实例 = SDL 队列单消费者 + 玩家 m_input 零 pump 纯 demo 注入——
**改世界操作在观众通道无消费者 = 白名单非黑名单结构性零遗漏**；
录制：现状 pump + handle* 边界 tap 先记后注【录制零副作用单测钉】；
普通运行 tap 恒 null = 位级恒等）④ **三 CLI 旗互斥**（--demo 回放
/ --demo-author 头less 编写【timeline JSON 展开：hold 展开 + dts =
1/60 定步全帧锁定】/ --demo-record 游玩捕获 O(1)/帧【quit 帧未模拟
尾部排除】）⑤ **回放循环**（墙钟只 pacing int64µs 防漂移，慢机掉
渲染帧模拟不追；倍率切点 = demo 帧边界不切半帧；**0.5× = render-
only 保持**【第二渲染帧零 update 零 ++m_frameCount = 帧锁定系统不
双推】；**--frames 按模拟帧计** = 任意倍率同终态）⑥ **观众只观察**
（ESC = 暂停菜单**非退出**【Q 退出】/ C = 观察视角循环 / mouse +
摇杆 = 转镜头 **orbit render-only**【aimForward() = 回放模式基视角
无 orbit——观众转镜头不偷 demo 玩家瞄准，7 处世界向射线机械替换】/
手柄 Start=暂停 LB/RB=倍率 左摇杆=数字化步进【V3-8 已先行 = 定案
立项加】）⑦ **观察红线**（save-spec 回放引用槽永不回写【md5 前后
逐字节不变实证】/ --demo + --new/--seed 点名拒 = world spec 权威）
⑧ shipped 示例 demo（assets/demos/example.dmo 285 B 266 帧 14 事件：
W 走 + 挖掘 2 块 dirt + E 背包 + C 三后 + zoom；生成脚本重跑字节
恒等）+ data/demos/ gitignore（录制默认 rec_<ts>.dmo）。**版本三
件套** 3.10.0→3.11.0。2777/2777（+43：schema 拒收 14 / 加载级 4 /
gzip 容器恒等 3 / 注入内核 5 / 录制零副作用 3 / 编写展开 8 / 观众
路由 2 / 标签 3）。门 201 = **结构免疫第 81 次免重锚（对 base19）**
【S1 三旗 absent ≡ base19 逐字节 + savenorm EXACT vs v3.10.0 旧侧】
+ S2 六臂【(a) shipped 示例回放 ×2 RAW + savenorm EXACT = 定案"首
放固化" / (b) **录制→回放闭环**（rec.dmo 2810 B 10B 钉头 → ×2
RAW + EXACT）/ (c) **编写臂**（timeline → ×2 EXACT + 玩家位移
4.121 块 = xdotool 替代面实证）/ (d) **观众路由**（转镜头终档姿态
≡ 无转镜头逐位恒等 = orbit render-only）/ (e) **变速双向**（wheel
up/down 终档 savenorm EXACT vs 参照 + 像素 diff 76 px 全在 HUD
角标 = 任意倍率同模拟实证）/ (f) **结束面板**（放完态退出截图
patch lum 32.0 < 120 暗帷在场 + ×2 恒等）】+ S6 点名 7 面【版本
拒播 / 乱序 / 未知 type 带词表 / 负 dt / 超 0.25 / world 双键 /
gzip 损坏 全 rc=1 带路径】+ 性能 A/B 同档【主对旗 absent：upd 均值
0.501 vs 0.579 窗口噪声带 + changesApplied 逐位 + 终档 EXACT；回放
自成本 = sub-ms 零可测增量】。**门内缺陷修复**（fixer 二次交付）：
观众 UI 不可见 = demo UI 块位于主批冲刷循环之后【顶点永不被消费】
+ 项目截图 = swap 后读 back 落后一帧既有语义 → 自包含独立批 +
headless 放完双渲染 + badge 移时钟下（S1 锚修复后回归 PASS）。记档
偏离 8 条（英文 UI 串 + 独立批 / LB/RB 不分源 / quit 尾排除 / world
spec 权威 + 观察红线 / 简版示例 / 放完态退出 / 左摇杆数字化 / E2E
实证门内补齐）。**3.x「世界扩展」矩阵 11 项编号任务全 ✅（V3-1~
V3-9 + V3-8/V3-9 UX 行）**；方向项（死亡统计屏 / Game Over 屏 /
末地 / 创造 / 多人）矩阵外立项需用户定案。详见
`docs/archive/phase-3.11.0.md`。

## v3.10.0（2026-11-05）— V3-6 世界边缘雾（填充感知 fog end，base19 重锚，门 200 全 PASS）

**流送盘边缘不再露截面**——消费「世界边缘即虚空（无雾）」Phase
3/14 时代记档。编排者实证定案：无输入跑 worker = 启动 16 列（4×4
演示区 cheb 半径 2）后零扩展（PIPE-1 结构免疫纪律）→ 锚图"浮空
截面" = 演示区边界 32-45 块，远在雾 start 750 内完全可见（fog off ≡
on 逐字节实证：可见世界全 <750）；现有 PRE-2 雾按**目标盘**（min
1000/1024 = 1000）算 = 对未填充盘缘无效。修复 = **候选 B 标量化：
fog end = min(静态 end, 实际填充盘 chebyshev 半径 ×16)**：无输入/
填充中盘面雾吞截面（end 32 / start 24 = "浮空岛淡出"）；稳态行走
盘满 64 → min(1000, 1024) = 1000 **位级恒等**（单测含 65 滞回变体
钉）；**下界同款 clamp**（首传 3×3 catch-up 半径 1 = end 16 同款
隐藏）。纯代码 9 文件：`filledChebyshevRadius` 纯函数（流送已载列
max Chebyshev，填充 rim 语义 = 下闭保守性论证）+ `fillClampedEnd` /
`fogFilledRange` 纯函数（start = 0.75 × clamp 后 end——ramp 总止于
填充 rim）+ render 雾块（列 set→vector 快照共享零新树走查）；**零
配置新键 / 零 format / 零新 worker 活动**（无输入日志逐行恒等实证）。
2734/2734（+10：FogFilledTest 6【满盘位级恒等含 65 滞回 / 2→32 / 0
→ degenerate off / clamp 只降不升 / nether 16·32·96】+
FilledChebyshevRadius 4【演示区 4 可达列全 = 2 / 满盘 64·65 / 部分
填充】）。门 200 = **base19 重锚（首重锚，225272 B）**【diff 审计
20940 px 全在盘缘带：上天空 0 diff + 前景/HUD 0 diff + 逐通道单调
趋天空 0 违例 + 单雾因子 100% ≤2 LSB 舍入签名 + 留证五图；存档面
savenorm vs v3.9.0 旧侧 EXACT = 雾纯渲染】+ S1 ≡ base19 ×2 + S2
**fog-off 控制臂 ≡ base18 逐字节**（三方闭环 = 唯一像素面实证）+
性能 A/B 同档【无输入对 = 16 列扫描零成本 + changesApplied 逐位 +
终档 savenorm EXACT】。详见 `docs/archive/phase-3.10.0.md`。

## v3.9.0（2026-11-05）— V3-5 建筑内容族（黏土链/石砖/苔石/15 色陶瓦染色族/水底 sediment 生成，门 199 全 PASS）

**建筑的多样性**——① 黏土链（MC 完整形态）：水底 sediment 生成
（世界生成新 pass，worldgen.json 数据驱动 `sediment` 节：
clay_probability 0.06 / mossy_cobblestone 0.008，per-column 纯函数
新盐零噪声，dry 列零触碰，熔岩湖双谓词拒收，顶 1 层）→ 破 clay 掉
**clay_ball ×4** → 熔炼 brick → 2×2 合成 bricks → 熔炼 **terracotta**
② 石砖族：stone_bricks（2×2 cobblestone）/ mossy_cobblestone（水底
稀有生成，记档偏离 = MC 废墟替代）/ mossy_stone_bricks（shapeless
石砖×3 + 苔石×1）③ **陶瓦染色族**（Phase 41 机制首个新族）：
terracotta white 基块 + **14 染色变体**（项目染料词表 15 色含 white
无 lime——设计 16 色按项目词表收敛，偏离 1；init 交叉检缺一色点名）
④ 纹理 20 tile（14 染色 = 新确定性混合公式 (base×3+dye×2)//5，
零新盐）+ 2 icon + atlas 145→165 空格零移位（无幂二增长）。
**核心 = 仅 worldgen 4 文件**（Sediment 解析 + pass + 3 纯谓词 +
fail-fast；absent = disabled 位级 legacy；[0,1] 严格）。2724/2724
（+27：内容钉 13 / 生成 pass 8【含发行世界 census 1024²：eligible
3683 / clay 211 / mossy 26 沙漠沙洋底 only + 样本列 ground truth】/
config 6）。门 199 = **结构免疫第 80 次免重锚**（S1 ≡ 锚 224468 +
savenorm **EXACT** + 白名单 4 键——生成内容不入 diff + 锚图零水域 +
opaque 派生 15 同不透明度）+ S2 三臂【全手工密封水室（围堰 48 格，
PITS 390 场景几何教训）：(a) 展示行 ×2 RAW 恒等【6 件新纹理水下
可见实证 + 池格 32 精确】/ (b) **破 clay → 4× clay_ball 入库存**
【磁力拾取 + 编辑回退参考】/ (c) 熔炼臂【"Furnace smelt complete:
brick" + fuel_substeps 4201 验算】】+ S6 4/4 点名【概率越界 ×2 /
字符串 / **染色族缺一色交叉检**】+ 性能 A/B 同世界同成本（PERF-WIN 三窗同档：upd med 0.550/0.539/0.574 vs 0.607/0.574/0.612
  ms = 窗口间机器噪声带，均值 0.554 vs 0.598——sediment pass =
  生成链一次性列成本零每帧开销；render 均值 0.573 vs 0.579 同档；
  swap vsync 同档；fluid·fires·spread·ents·stream 同档）+
  changesApplied 9/1/3 逐位 + 终档 savenorm **EXACT**（生成内容不入
  diff 零 format 完全恒等最强形）。
记档偏离 6 条。详见 `docs/archive/phase-3.9.0.md`。

## v3.8.0（2026-11-05）— V3-3 下界 mob（5 种 + neutral/walks_on_lava 旗 + 下界生成池，门 198 全 PASS）

**下界有了居民**——① **5 mob 纯数据**：zombified_piglin（hostile，
zombie 数据孪生，**undead false**）/ **piglin·hoglin = neutral 新类**
（闲逛；受击追击——piglin 16HP/6 伤，hoglin 40HP/慢速 1.0/drops
hoglin_meat 1-2 + leather）/ magma_cube（hostile 16HP 慢速，**不分裂
不跳** blob 3 盒）/ **strider**（passive + **walks_on_lava**，熔岩面
行走）② **M1 neutral 旗**（与 hostile 互斥点名拒）：`damage()` 单
收口任意来源激怒（玩家/箭/火/摔落/毒，MC piglin 同款）+ 30 s real-dt
脱激 + 再击刷新 + 玩家死亡即时脱激；chase 谓词单式第二支
`(p.neutral ? m_angry : true)` = 非 neutral 位级恒等 ③ **M2
walks_on_lava 旗**：熔岩点火豁免（火块仍点燃 = MC 同款）+ 熔岩面
**feet-on-plane walk 档**（snap 不动点 fps 无关 + 深潜浮回 + 免溅
落）+ 生成面 = 熔岩源格顶面（findMobSpawnCell 尾参 flag 门控）④
**M3 下界生成池零新基建**（Phase B 预留槽兑现）：
mob_spawning_nether.json 权重 10/8/7/5/1 + fail-fast init 点名 +
单一共享类型表 + 策略继承主世界会话值；维度选池/只 tick 活跃维/非
活跃维冻结/mob 不入存档 = 既有语义零代码 ⑤ **strider
spawn_max_light 7→15**（门内实证机制缺口：熔岩自发光 15，7 门自毁
= 偏离 1）⑥ 食物链 hoglin_meat +4 / 熟 +8（pork 先例）+ 3 地面
ambient（hoglin grunt/piglin whisper/strider hiss——地面 roll 新
runAi 分支 = bat flyCruise 先例推广，数据门控零流抽取 = 偏离 3）+
纹理 9（atlas 136→145 空格零移位）。**版本三件套** 3.7.0→3.8.0。
2697/2697（+39：旗解析互斥 4 / 内容钉 6 / neutral 激怒态 7 / 熔岩
行走 7 / 生成谓词 6 / 池 5 / 音效 3 / 食物链 1 + census 重钉）。门
198 = **结构免疫第 79 次免重锚**（S1 ≡ 锚 224468 + EXACT + 白名单
4 键）+ S2 三臂【(a) 下界高原 ×2 生成流恒等（piglin f60/magma f90/
zpig f120/piglin f150 位置逐字节）+ 截图 RAW 恒等 + 终档 census /
(a2) **strider 决定性臂**（环全清 4704 air 编辑 = cap 永不饱和——
每次尝试新抽：**`strider f660 at (13.5, 32.0, −5.5)` ×2 恒等，y =
32.0 熔岩海顶面精确 + 540f 零 burn 点火豁免 E2E**）/ (b) 池隔离
（OW 生成 4 + 下界物种 0）】+ S6 6/6 点名【互斥 / 双旗非 bool /
池缺失 / 池未知 mob / ambient 单向严格对】+ 性能 A/B 同世界同成本
【PERF-WIN 三窗同档 + changesApplied 9/1/3 逐位 + **终档 savenorm
EXACT**【nether-pool only 零 format 最强形】】。记档偏离 7 条
（光门 15 / 异种流体 / 地面 ambient 分支 / 共享类型表 / 策略继承 /
下界原点保护圈 / ghast 延后）。PITS 389（门配置 mobs-off 纪律 / sed
派生 OUT 目录 / spawn 行正则负号 / cap 吞尝试 / 落点光场压制）。详见
`docs/archive/phase-3.8.0.md`。

## v3.7.0（2026-11-05）— V3-2 Part 2 下界深化（水蒸发/音效族 11 件/magma 光/ambience 第 4 上下文/soul sand 数据驱动减速，门 197 全 PASS）

**下界的行为补全**——收口 v3.4.0/v3.5.0 的 4 条记档偏离，全
nether-scoped（**overworld 位级不变**单测 + 锚双保险），零渲染/零
format（30 不变）/零 config。① **T1 水在下界蒸发**（v3.4.0 偏离 8
no-op → MC 蒸发语义）：下界泼水桶 = 桶变空桶 + `evaporate` 音 + info
日志，**零格触碰**（桶独占事件）+ 点击恒消耗；下界熔岩桶正常放置
（MC 同款）；主世界泼水 = 同一语句 false 分支位级恒等。② **T2 下界
音效族**（v3.5.0 偏离 3/4 stone·grass 复用收口）：新 `nether`/
`nylium`/`stem` 3 族 9 件（固定种子确定性合成）+ soul 族 → 既有
`sand` / ore·basalt·nether_brick → 既有 `stone` 重指派（sound_material
字段数据驱动零核心）——13 下界块音族全钉；SoundBank 49→60（11 新
wav 含 evaporate + ambience_nether，既有 49 件 md5 逐一不变幂等实证）。
③ **T3 magma_block light_emission 3**（v3.5.0 偏离 5 收口，MC 精确，
数据一行）。④ **T4 下界 ambience 第 4 上下文**（优先级 nether > cave
> day/night，MC 同款"下界洞窟也是下界"= 偏离 5）：
`ambienceContext` 加**默认参 inNether=false**（既有调用点位级不变，
overworld 30 组输入面恒等单测钉）+ `ambience_nether` 48 Hz 隆隆 2.0 s
新 wav；调度常量复用（300f 窗 × p 0.5 不动）。⑤ **T5 soul sand 减速**
（v3.5.0 偏离 D6 收口，数据驱动）：块属性 **`walk_speed_factor`** 严格
解析 number (0,1]（absent=1.0，越界/非数字 init 点名；**仅
soul_sand 0.4** MC 精确）+ player 步行消费 = **双 feet 格取最小因子**
（支撑行 × X 跨双端点严格重叠 × 中心 Z 列；飞行/流体/潜行豁免，疾跑
= 步行受减速 MC 同款偏离 3）；`speed × 1.0f ≡ speed` 位级恒等 = 主
世界无携带块时的结构免疫根。**版本三件套** 3.6.0→3.7.0。2658/2658
（+18：WalkSpeedFactorParses 6 形 / WalkSpeedFactorTest 7【纯函数
1.0 位级 / 2 s 位移比 0.4±0.005 / 边界双格 / 疾跑亦减速 / 潜行·飞行
·涉水豁免轨迹恒等】/ AmbienceNetherTest 5【优先级四形 / overworld
位级恒等钉 / 发行 seed roll 首 FALSE = 帧 2100 边界 + day 流去相关 /
wav 钉】/ SoundNetherFamilyTest 4【PITS 382 守卫】/ census 60 ×2 /
T1 谓词四形）。门 197 = **结构免疫第 78 次免重锚**（S1 无输入 ×2 ≡
锚 224468 + savenorm EXACT + 白名单 4 键）+ S2 下界场景三臂【(a)
shot ×2 RAW 恒等 + **ambience_nether 事件 a=2 b=2 逐一对**（帧锁定
确定性）+ nether 段 16 块编辑持久 / (b) **soul sand 步行比值
0.403**（全 soul 行 vs 全 neth 行 3.0 s W-hold：dx 4.321 vs 10.712，
T5 端到端）/ (c) **桶蒸发**：日志 + slot0 空桶 + nether 段零 water +
(c2) 主世界对照**水正常放置**零蒸发】+ S6 3/3 点名【1.5 / 0 /
"0.4" 字符串全带 "walk_speed_factor" 路径 rc=1】+ 性能 A/B 同世界同
成本【PERF-WIN 三窗同档（均值 0.568 vs 0.557 ms 新侧更低）+
changesApplied 9/1/3 逐位 + **终档 savenorm EXACT**】。记档偏离 6 条
（无蒸汽粒子 / portal UV 动画延后 = 渲染器基建 / 疾跑亦减速 / 双 feet
格取定形 / nether>cave 优先 / 11 wav = 设计计数笔误 T1 规格胜）。详见
`docs/archive/phase-3.7.0.md`。

## v3.6.0（2026-11-05）— V3-4 netherite 材料链（第四材料档，**零核心代码纯数据**，门 196 全 PASS）

**ancient_debris → scrap → ingot → netherite 工具/护甲**（完整 MC 链的
项目形态）——① 熔炼链：ancient_debris 块 `+smeltable_to: netherite_
scrap`（CRAFT-6 块物品继承，netherrack → nether_brick 先例）→ scrap
material 物品 ② **netherite_ingot** = shapeless 4× scrap + 4×
gold_ingot（记档偏离 1 = MC 用 gold nugget，项目无 nugget）③
**netherite 工具 3 件**（项目无钻石工具 = 直接 crafting 克隆铁形状，
记档偏离 2）：**durability 2031**（MC 精确）/ pick break_speed 4.0（
项目铁刻度）/ attack_damage 4·5·4.5（铁 +1）/ enchantability 14 /
**fast_against 2.0 × 21 块**（铁石族 10 + nether 台地 11 = 正确工具
语义）/ repair netherite_ingot ④ **netherite 护甲 4 件** = **shapeless
升级 ingot + diamond_<slot>**（项目无锻造台，记档偏离 3）：**durability
545/792/743/644**（= 钻石 ×1.5 MC 精确）/ armor_points 3/8/6/3 /
enchantability 10（钻石同款刻度）⑤ icon 9 件（暗红铜色板 = 下界世界观
一致，生成脚本确定性，既有 84 张 md5 不变）+ model 9 件（工具克隆铁系
/ 护甲克隆钻石系换纹理）。**零核心代码**（git status src/ = 空——
"加材料 = 数据"承诺的第四档验证）：smeltable_to/shapeless/shaped/
repair_with/fast_against/model 全既有机制。2640/2640（+23：内容钉族 /
熔炼数据 + tick 两段 / 合成行为 3 族（ingot/升级/工具网格）/ 护甲
×1.5 刻度验算 / icon·recipe census / **SmeltThenCraftEndToEnd** / S6
拒收 4 面）。门 196 = **结构免疫第 77 次免重锚**（S1 ≡ 锚 224468 +
白名单 4 键——新物品不在默认热栏 + 无世界生成消费 = 纯内容零像素）+
S6 点名 5/5【durability 0 / enchant_class 未知词带词表 / strict pair /
未知材料物品 / fast_against 未知块引用——全 init 带路径】+ 性能 A/B
同世界同成本【PERF-WIN 三窗全同档 + changesApplied 9/1/3 逐位 + 终档
savenorm EXACT 完全恒等】+ 熔炼臂实证（fixer 冒烟：600f 无输入
debris 入炉 → "Furnace smelt complete: netherite_scrap" + 炉内 output
×1 + fuel_substeps 4201 精确验算 + **xp orb ×1 = smelt_xp 发放面实证**
）。记档偏离 4 条（nugget/直接 crafting/无锻造台/暗红色板）。详见
`docs/archive/phase-3.6.0.md`。

## v3.5.0（2026-11-05）— 下界内容族 Part 1（V3-2 合并执行：群系 +
块族 + 矿脉 + 装饰 + **下界带修订为可航行空腔**，门 195 全 PASS）

**下界变成 MC 的下界**（第一步）——① **D9 带修订（编排者布局裁决，
实现后实证 v3.4.0 实心岩板下界不可航行）**：MC 下界废土形态 =
基岩底 0-4 / 熔岩海 5-31 全源（岩板下挖穿可见）/ **netherrack 台地
32-52**（群系表面 = 台地顶）/ **空气空间 53-122**（70 格可航行空腔）/
天花板 glowstone y=122（贴基岩顶底面 MC 精确）/ 基岩顶 123-127——
传送落点 Y=55 → 空腔内（首访观感实证：暗红天穹 + 远地平线雾吞 +
portal 平台）；世界生成演进先例（未编辑列按当前生成器重建，既有
portal 框持久）② **5 群系**（biomes_nether/ 独立池独立 registry，
MC 比例权重 36/22/17/16/9；netherBiomeAt = biomeIndexAt 克隆 + 独立
盐；装饰门控 = 数据派生旗 fill 零 name switch）③ **12 新块**
（nether_brick 2.0 / magma 1.5 / quartz_ore 3.0 / nether_gold_ore 3.0
drops gold_ingot / ancient_debris **10** = MC 50 的 obsidian /5 缩放
先例 / soul_sand·soil 0.5 / 双 nylium 0.4 grass 先例 / basalt 1.2 /
双 stem 0.4——全 stateless；纹理 12 件确定性生成，**atlas 16×8→16×16
幂二增长既有 124 格零移位 + 像素行恒等 = base 锚免重锚实证**）④ **3
矿脉**（lattice 8 per-cell 纯函数，台地 only：quartz p 0.0376 /
nether gold p 0.0208 / ancient debris p 0.0026）⑤ **5 装饰**（stem
柱 4..9 / basalt 柱 5..30 自 y53 可见 / soul_soil·magma per-cell 带
48-52 / 天花板 glowstone 复用既有块）⑥ quartz 物品 + nether brick 熔
炼/2×2 配方 2 件⑦ **init fail-fast**（门内 S6 实证发现：surface 存在
性/stateful 拒收从首传惰性前移 buildScene，主世界同款）。2617/2617
（+35）。门 195 = **结构免疫第 76 次免重锚**（S1 ≡ 锚逐字节 + 白名单
4 键）+ S2 内容双臂【(a) 传送空腔截图 ×2 RAW 恒等 + 观感实证 / (b)
**下界原生采矿**：quartz_ore 破 → quartz ×1 入库存【磁力 1.08<2.5】+
按住续破 soul_sand【PITS 7】+ 掉落 2.5 边缘留地【PITS 377】+ OW 段
26 块原样】+ S6 5/5 init 点名【weight 0 / 重复 id / 未知 surface /
空池 / stateful surface】+ 性能 A/B 同世界同成本【主对 900f 无输入：PERF-WIN 三窗全同档（upd med 0.578/0.615/0.634 vs 0.556/0.563/0.584 ms / render 0.597/0.651/0.596 vs 0.600/0.598/0.544）+ changesApplied 9/1/3 逐位 + 终档 savenorm **EXACT**【零 format 完全恒等】+ 下界含内容稳态 ≤ OW 基线（render med 0.365/0.361/0.362 vs 0.60 + fluid/fires 0.000 熔岩海零 churn）】。
详见 `docs/archive/phase-3.5.0.md`。

## v3.4.0（2026-11-05）— V3-1 Phase B 下界世界 + 真传送（门 194 全 PASS）

V3-1 三段拆分第二段（**下界维度本体**；Phase A 门框 v3.2.0 已发，
Phase C 内容族 = v3.5.x 系列）——**站进主世界传送门，真的去下界了**：
① **下界维度**（D1 增量并行成员：overworld 代码路径零 diff；D2 **惰性
构造**——首传前零 chunk/零 mesh/零流送/零内存；D3 **仅 Player 换绑**
（Entity setWorld 单咽喉点，m_world 引用→指针 17 点机械适配），
inactive 维 = 时间胶囊【world-pass 冻结，DayCycle/Weather 全局恒推进
= MC 时间连续】）② **下界 worldgen = 专属纯函数**（坐标的纯函数 =
零噪声构造性确定）：基岩底 0-4 / **熔岩海 5-31 全源静态**（流体零
churn）/ netherrack 32-122 / 基岩顶 123-127；派生 seed `worldSeed ^
kSaltNether` 不落盘（单源真值）；netherrack 块（hardness 0.4 MC 精确 /
drops 自身 / 纹理 atlas [13,7] 零移位 / 音族复用 stone 记档）③
**skyless 光照 2 参**（void/顶 sky 读 0 + rescan 顶源 0，shader 零
改动 = 无天穹下界）④ **真传送管线**（单帧帧锁定无 loading screen）：
60 帧倒计时 → 8:1 floorDiv 双向映射 → 目标维 ±16 搜活跃 portal（无 →
**生成 4×5 含角框 + 2×3 基岩平台**走咽喉点）→ 落点 hitbox 安全 +
step-up → 3×3 列盘同步 catch-up → 换绑 + restorePosition + 相机 →
流送自动 engage → portal_trigger/travel 2 件新音效（47→49）；**900
帧冷却**（15 s 防振荡，会话态）；**正交 portal yaw −π/2**（MC JE 顺
时针 90°）⑤ **渲染 5 点门控**（暗红 clear / 日月星云雨不画 / 雾
10→96 块短程 + F3 dimension 行 19→20）⑥ **format 30**：player.
dimension 严格词表恒写 + nether 段 {blocks,fluids,states} 恒写 +
nether_entities（item）恒写；29 携新键 = version lie 点名 / 30 缺键
点名 / 未知块名点名 / nether 内 stateful 块须 nether.states 条目
（PITS 383① 孪生）；两维 portal 载入 rescan；gzip 编码零改动；**下界
死亡 = 主世界重生**（MC 精确）⑦ 下界行为收口（7 per-block state 族
主世界 only / 床睡眠 no-op / 容器面板主世界 only / 下界火不烧尽 /
水桶 no-op = 8 条记档偏离）。2582/2582（+27：nether_gen 14【层带/熔
岩顶边/chunk 连续/负坐标/纯净性/派生 seed/框搜索族/hitbox 上探/正交
yaw】+ portal 映射往返 2 + format 30 族 7 + setWorld 钉 + F3 行 +
块内容钉）。门 194 = **结构免疫第 75 次免重锚最强形**（下界惰性未建：
S1 ≡ 锚 224468 逐字节 + 终档**恰增白名单 4 键**）+ **S2 双跳 arm
（全 headless）**：传送（"Traveled to nether at (1,55,−1) from
(0,55,−2)" 映射验算 + 下界截图 ×2 RAW 恒等【netherrack 岩洞 + portal
光 11 紫晕】+ 落点 [1,55,−0.5] 精确 + nether 段 26 块 = 生成框 +
平台）+ 返程（±16 搜中原 portal + 落回原 portal 内域底中心 + nether
段持久化 + time 恰 +120f 帧锁定验算）+ S6 点名 6/6 + 性能 A/B 同世界
同成本（PERF-WIN 三窗全同档 + changesApplied 9/1/3 逐位 + 终档白名单
4 键 + **下界稳态 ≤ OW 基线：render 0.349 vs 0.510 med、fluid/fires
= 0.000【全源静态熔岩海零 churn = recon 风险面 ③ 按设计消解实证】**）。
PITS 387（g139 插桩器锚点随 DimCtx 调用点形态演进 = 双形态锚 + 每树
用自己 commit 的插桩器）。详见 `docs/archive/phase-3.4.0.md`。

## v3.3.0（2026-11-05）— V3-8 手柄支持（门 193 全 PASS）

11-05 用户实机诉求定案（BACKLOG UX 行，v3.2.0 收口后紧接执行）：**SDL3
GameController 一阶 API，实现面 = 输入边界层**（世界/渲染/存档/物理
零触碰）——① **源位掩码 OR 语义**：`m_keyDown`/`m_buttonDown` bool →
每源一 bit（bit0 键鼠 / bit1 手柄），`isDown` = 任一源按下，每源独立
rising edge，`wasReleased`（弓）仅本源 falling 且掩码清 0 才置 =
**键鼠手柄天然双连共存**（同一动作的并集，任一侧按住不松）+ **无手柄
= bit1 永不置位 = 位级恒等**（legacy fixture 单测钉）；② **v1 映射**
（Xbox 布局，9 钮 + 2 摇杆）：左摇杆移动（死区 0.2 外数字化，四方向
全量重算幂等回中清键）/ 右摇杆视角（死区外重映射 × 100 px 当量合成
mouseDelta：杆右 = xrel+，杆上 = yrel−）/ A = Space【双击飞行自动
继承】/ B = LeftShift 疾跑 / X = E 背包 / Y = Q 丢弃 / LB = 挖掘 / RB =
使用 / Back = C 相机 / Start = ESC / D-pad 右 = Tab；LT/RT/L3/R3/Guide/
D-pad 上下左/paddles = v1 不映射记档；③ 单手柄模型（第二手柄忽略）+
连断日志（`SDL_GetGamepadNameForID`，无 = "unknown"）+ F3 末行连接态
（18→19 行）+ 断连清全部手柄位**不置释放边**（弓 stale-up 取消蓄力而
非悬挂）；④ `SDL_Init(VIDEO | SDL_INIT_GAMEPAD)`（无手柄环境惰性零成
本）；无 config 键（映射 = 命名常量，`kGamepadLookPixelsPerUnit` = 未来
config 化锚点）。2555/2555（+19：test_gamepad 18【映射纯函数钉 / 9 钮
按压释放含 RB 弓边 / 17 不映射钮零变化 / 死区边界 6225·6553·6554 /
四方向+对角+回中+切换幂等 / 右摇杆符号+增益精确 + 累积 + frameBegin
复位 / 重映射公式钉 / trigger 零变化 / 键·钮 OR 弓语义双序 / 断连清位
+ 再连 + 陈旧 no-op / 单手柄模型 / 未连接 no-op / **legacy 位级恒等
fixture**】+ DebugOverlay GamepadLine 四形）。门 193 = **结构免疫第 74
次免重锚**（本环境无物理手柄 = 零 SDL 手柄事件：S1 无输入 ×2 ≡ 锚
224468 逐字节 + gamepad 日志行 = 0）+ S3 全套 + S6 跳过记档（无
config/内容面）+ 性能 A/B 同世界同成本（PERF-WIN 三窗全同档 +
changesApplied 9/1/3 逐位 + **终档 savenorm EXACT 零 format 最强形**）；
**门强度记档**：全部 handler 逻辑单测覆盖（handle 入口 = handleKey 同
款模式）+ 实机验收 = 用户（release 提示）。详见
`docs/archive/phase-3.3.0.md`。

## v3.2.0（2026-11-05）— V3-1 Phase A 下界传送门框（门 192 全 PASS）

V3-1「下界维度·传送门与下界」三段拆分第一段（**门框**；Phase B 下界
世界 + 真传送 / Phase C 下界内容族另立项）——① **nether_portal 块**
（纯数据 + 紫罗兰漩涡纹理 atlas [13,6] 零移位 + portal_ambient 确定性
合成音效 46→47）：full-cell 半透盒（lava 先例）/ **destroyable false**
/ **light_emission 11**（MC 精确）/ 无 drops / **state_count 2 axis**
（ENT-2 机制）；② **core/portal 纯函数族**：MC 框语义（4×5..23×23
外廓、**角可选**、竖直平面、高度恒 ≥5）有界扫描（45×45×2 平面 +
前缀和 O(1) 候选判定，事件 <1 ms 级）+ 激活/去激活决策（obsidian 与
fire 双向事件检查 = "火须最后放" quirk 偏离记档；**非角破坏去激活 /
角破坏保留 = MC 精确**；**fire 熄灭 ≠ 去激活 = portal 自持**；逐框
确定性应用）+ 8:1 映射纯函数（floorDiv 负坐标安全）+ 站入 60 帧
（1 s）帧锁定 step + ambient roll；③ **setBlock 咽喉点钩子**（furnaces
簿记族克隆：obsidian/fire 事件 → 扫描 → 内 air+fire → portal
state=axis【fire 格被替换 = MC "one of them replacing the fire"】+
bbox 一次重光 + 日志）+ **点火分支 (c)**（命中环格**面法向相邻内 air
格**点火——框内 air 与环共面射线不可直达，slab 放置同款几何；磨损 +
音孪生）；④ **站入倒计时**（AABB 重叠 arms 60 帧 / 走出取消 / 无冷却
重计 / **GUI 互斥：portal 内开面板自动关**【MC 传送门规则，closePanel
既有语义 = click 重捕获】）；Phase A 传送 = 显式 no-op 桩
（`kPortalTravelEnabled = false` + static_assert 构建门，禁 TODO；
完成帧日志 "Nether ready at (mapped 8:1)" = Phase B 输入）；⑤ **载入
rescan**（孤立 portal 块 → air，PITS 383① 双向交叉检纪律 + 追踪集
重建 + 一次性重光）；⑥ **存档零 format**（29 不动；portal = 普通
stateful 块走 blockEdits + states 段）。2536/2536（+33：portal 纯
函数族 31【扫描域/角可选/3×4·4×4·24×23 拒/水平拒/共享框/transition
全形/entry step/映射/ambient roll/S6 内容拒收 3】+ 块内容钉 +
ambient wav 守卫）。门 192 = **结构免疫第 73 次免重锚**（≡ 锚 224468：
worldgen 零 obsidian + 打火石不在默认热栏）：S1 无输入 ×2 ≡ 锚 +
savenorm EXACT；**S2 输入 E2E 六臂**（激活【终档 6 portal state 0 +
fires 0 + flint damage 1 + 日志】/ 去激活【obsidian 硬度 10 17.5 s
hold】/ **角保留 MC 精确** / 站入【60 帧恰 "Nether ready at (0,55,−1)"
验算 + 零位移 + 每秒重计语义】/ **GUI 互斥决定性六跑**【d6 站入+E+
重捕获 ≡ d3 对照 RAW 逐字节 + d4/d5 框外面板留开 = 条件化】/ 旧侧
形状互斥）；S6 4/4 点名；性能 A/B 同世界同成本（PERF-WIN 三窗全同档
+ changesApplied 9/1/3 逐位 + **终档 savenorm EXACT**【零 format 最
强形】+ portal 自成本无可测 per-frame 增量）。PITS 386（门内三坑：
面法向点火方向 / cell 索引 vs 格中心差 0.5 / V3-7 起门脚本读档过 gzip
魔数）。详见 `docs/archive/phase-3.2.0.md`。

## v3.1.0（2026-11-04）— V3-7 存档 gzip 压缩（3.x 首功能版，门 191 全 PASS）

11-04 用户实机诉求定案（data/saves 体积大）先执项：**整档 gzip(JSON)
落盘，默认开**——① **确定性 gzip 容器**（手写 = 字节可钉：固定 10B
头 `1f 8b 08 00 00 00 00 00 00 03`【FLG=0/MTIME=0/XFL=0/OS=3】+ raw
deflate level 6 + CRC32/ISIZE 尾；zlib 自带 gzip 封装的 MTIME/XFL/OS
随版本漂移 = 排除，PITS 385①）：同世界存档逐字节可复现；② **加载
魔数 `1f 8b` 自动探测**（唯一编码判据，扩展名恒 .json）：旧明文档
**零迁移直读**（format 28 及更早明文档新构建原样加载）；解码严格
（头 10B 逐字节全钉 / 截断 / 半体 / CRC·ISIZE 不符全点名带路径，
系统 gzip CLI 产物点名拒收不静默容忍）；③ **config 键
`save.compression: "gzip"|"none"` 严格读**（`save.auto_save_seconds` +
`render.ao` 双先例）：absent = **gzip（代码默认 = 用户定案**；
committed config.json 无 save 节 = 零 config 改动）/ "none" = 明文
（旧行为位级恒等：同状态文件与 v3.0.0 差异 = 恰 root.format 单键）；
非 string / 空串 / 未知词 init 点名拒；④ **format 29**（schema 与 28
完全一致——编码与 format 号正交，29 = "gzip 时代"标记；per-format
规则全 `>= N` 下界检查 → 解析侧代码零改动）；⑤ **集成 = 写侧单
choke point**（`Game::writeSave` exit/F5/autosave 共享：payload
`dump(2)+'\n'` 一字不动 → 按 config gzip 或明文 → `writeAtomicFile`
原子性不变）**/ 读侧两个消费点**（`Game::readSaveFile` +
`--list-saves` 摘要，corrupt 槽 problem 命名不隐藏文件）；
save copy/rename/delete = 纯字节操作零改动；⑥ **`tools/
gate_compare.py` savenorm 加解压步**（魔数 → gzip.decompress）；
⑦ **save/load 日志加耗时 ms + 体积变换**（real-dt 性能证据，不入
字节门）。**zlib 零新依赖**（自研 PNG 已链接）。门 191 = **结构免疫
第 72 次免重锚**（存档 = 渲染无关路径，零像素）：S1 无输入 60f ×2
≡ 锚 224468 逐字节 + 头 10B 实证 + 存档 savenorm EXACT（RAW diff =
fires real-dt 三键 = PITS 385③ 既有口径）+ **S2 四臂**（gzip
roundtrip 6.25× + pos/time 帧锁定保持 / **遗留零迁移最强形式 =
savenorm 新旧 fresh-60f 恰 `.format: 28 vs 29` 单键** + faithful load
逐计数 / none 明文 + savenorm 恒等 / corrupt 512B 截断 exit=1 点名
不崩溃）+ S6 点名 3/3 + 性能 A/B 同世界同成本（IO 增量口径：save
写 gzip 1.2 ms / load inflate 0.3 ms @ 4 KB 档，门档 2.7 MB 级外推
≈ 0.5 MB 落盘）。实测压缩率：门档 4237 B → 677 B = **6.3×**（立项
前大档 5.4× = 4 万块 journal 1.74 MB → 0.32 MB）。2503/2503（+21：
test_save_gzip 19 + save_slot 2；既有 12 处 format 字面量钉 28→29
有意识更新，逻辑零改动）。PITS 385（① zlib 封装字节漂移 = 手写容器
② 本机 zlib 1.3.2 缺 Z_DEFAULT_MEM_LEVEL 宏 = 命名常量 8 ③ 双跑 RAW
恒等适用域 = 无火场景）。详见 `docs/archive/phase-3.1.0.md`。

## v3.0.0（2026-11-04）— 3.x「世界扩展」期开启（原 4.x 命名重编号；纯文档 + banner，零玩法变更）

2.x 全史收官（v2.19.0，两批 20 版）后的时代开启版：**① 重编号定案
（11-04 同日用户定案）**——「世界扩展」期任务表由原 4.x 命名改
**3.x**（2.x 已吸收原 3.x「系统扩展」八项 = V2-11~V2-18，「世界扩
展」回归自然序号）：任务号 V4-1~V4-7 → **V3-1~V3-7**（V3-1 下界
维度锚 / V3-2 下界内容族 / V3-3 下界 mob / V3-4 netherite 材料链 /
V3-5 建筑内容族 / V3-6 世界边缘雾 / V3-7 存档 gzip 压缩【11-04 用户
实机诉求，默认开 = format 29，先执 = **v3.1.0**】），执行权威 =
`docs/BACKLOG.md`（3.x 任务表）；**② README「版本与发布」精简为四里
程碑**（v0.1.0 / v1.0.0 / v2.0.0 / v3.0.0，逐版明细指向 CHANGELOG
时代分档）；**③ 活文档重编号刷新**（AGENTS/BACKLOG/CHANGELOG 时代
切换）。**零玩法/零 format/零像素变更**：banner v2.19.0 → v3.0.0
（PITS 355 三件套：CMakeLists PROJECT_VERSION 单点 + test_debug_
overlay F3 钉 + banner 实证）+ 全套测试复跑 + 无输入 base18 结构免
疫（banner 仅 F3 调试覆盖层 + 启动日志消费，截图零触碰）。下一版
**v3.1.0 = V3-7 存档 gzip 压缩**（用户开新会话后执行），其后 V3-1
下界维度拆分立项。
