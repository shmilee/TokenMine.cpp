# BACKLOG.md — 3.x 任务表（执行权威）

> **11-04 二批收官定案晋升**（由 `docs/BACKLOG_4x.md` 方向底稿同款晋升，
> 先例：1.x→2.x / 2.x 第一批→第二批）。11-03 用户定案：（原命名
> 4.x）= **「世界扩展」期**（维度与终局）**保持大版本**（条目皆大任
> 务）；**锚项 = 下界维度（超大，拆分立项）**。**11-04 逐项用户定案：
> 6 项全保留 + 用户实机诉求 V3-7 存档 gzip 压缩（默认开）先执**
> （V2-19 提前项同款先例），其后 V3-1 拆分立项（门框 → 下界世界 →
> 内容三段），死亡统计屏/Game Over 屏归 UX 行续编号。**11-04 同日用
> 户重编号定案：原 4.x 命名改 3.x**（2.x 已吸收原 3.x「系统扩展」八
> 项，「世界扩展」期回归自然序号）——V4-1~V4-7 → **V3-1~V3-7**，版
> 本 v4.0.0 → **v3.0.0**（3.x 开启版，11-04 已发布：重编号 + README
> 里程碑精简，纯文档 + banner，零玩法变更）。V3-N = 执行编号（版本
> 3.x；首功能版 v3.1.0 候选 = V3-7 存档 gzip）。2.x 全任务表（两批
> 完成）= `docs/archive/v2x/BACKLOG.md`。
>
> 纪律同款：任务只从执行权威表取、不另列菜单；增删格子需用户确认。

## 3.x 任务矩阵（**11-05 原 11 项全完成 + 11-06 用户定案 7 项入表
（V3-8b + 方向项 V3-10~V3-15）+ 11-07 用户实机诉求 2 项入表（V3-16 扩
展指导 demo 族 + V3-17 主菜单系统）+ 11-08 用户诉求 2 项入表（V3-18 存
档扩展名 + V3-19 死亡屏计时）= 23 项矩阵（21 ✅【含 V3-12 全 4 相位 a/b/c1/c2 + V3-16 10 主题全
✅ + V3-19 死亡屏计时 v3.41.0 + V3-18 存档扩展名 v3.42.0】/ V3-17 主菜单全 4 相位 ✅（17a v3.43.0 + 17b v3.44.0 + 17c v3.45.0 + 17d v3.46.0）= **3.x 矩阵收官**）**，
11-05 v3.11.0；11-04 定案：6 项全保留 + V3-7 先执 + 11-05 用户诉求 V3-8 手柄
入 UX 行 + 11-05 用户定案 V3-9 demo 回放入 UX 行；**表格只留简写，详情见「完
成项明细」节 +「待执行项明细」节（表外）**）

| 系统 | ① | ② | ③ | ④ | ⑤ | ⑥ | ⑦ | ⑧ |
|---|---|---|---|---|---|---|---|---|
| STRM 流送与内存 | （方向：双世界流送/内存设计，依赖 V3-1） | | | | | | | |
| PIPE 帧管线与并发 | （方向：传送过渡帧管线，依赖 V3-1） | ✅ V3-12 多人【全 4 相位】：✅ a 设计（v3.28.0）+ ✅ b 2 人 PoC（v3.29.0）+ ✅ c1 大厅/聊天/N/心跳（v3.30.0）+ ✅ c2 断线恢复 + format 33（v3.31.0，矩阵收官项） | | | | | | |
| LGT 光照 | | | | | | | | |
| GEN 世界与生成 | ✅ V3-1 下界维度·门与下界（三段） | ✅ V3-2 下界内容族（Part 1+2） | ✅ V3-5 建筑内容族 | ✅ V3-10 末地维度：✅ V3-10a 框架（v3.23.0）+ ✅ V3-10b 内容族（v3.24.0）+ ✅ V3-10c 末影人（v3.25.0）+ ✅ V3-10d1 龙核心（v3.26.0）+ ✅ V3-10d2 召唤复杀（v3.27.0）【V3-10 全 4 相位完成】 | | | | |
| FLUID 流体 | | | | | | | | |
| ENT 实体/Mob/移动 | ✅ V3-3 下界 mob | ✅ V3-14d strider 骑乘（v3.22.0）【V3-14 全 4 相位完成】 | | | | | | |
| SURV 生存/战斗/效果 | ✅ V3-4 netherite 材料链 | ✅ V3-11a 创造基础（v3.17.0） | ✅ V3-11b 创造物品栏（v3.18.0） | ✅ V3-15 生存/表现批（v3.16.0） | | | | |
| CRAFT 物品/合成/面板 | | | | | | | | |
| SAVE 存档 | ✅ V3-7 存档 gzip | （方向：双世界存档 format，依赖 V3-1） | ✅ V3-18 存档扩展名（v3.42.0） | | | | | |
| PRE 表现层 | ✅ V3-6 世界边缘雾 | ✅ V3-17 主菜单系统（11-07 用户实机；17a ✅ v3.43.0 + 17b ✅ v3.44.0 + 17c ✅ v3.45.0 + 17d ✅ v3.46.0 = 全 4 相位） | | | | | | |
| UX 实机报障/体验细化 | ✅ V3-8 手柄支持 | ✅ V3-9 demo 回放 | ✅ 阴影缩尺报障（v3.12.0） | ✅ demo sim 版本（v3.13.0） | ✅ V3-13 死亡统计屏/Game Over（v3.15.0） | ✅ V3-8b 手柄映射定案（v3.14.0） | ✅ V3-16 扩展指导 demo 族（11-07 用户实机，16a-h 10 主题全 ✅） | ✅ V3-19 死亡屏计时（v3.41.0） |

### 完成项明细（表外；逐版细节 = `docs/archive/phase-3.x.0.md` +
### CHANGELOG 逐版条）

- **V3-1 下界维度·传送门与下界**（三段拆分）：Phase A 门框 = v3.2.0
  门 192 / Phase B 下界世界 + 真传送 = v3.4.0 门 194【format 30】/
  Phase C Part 1 内容族 = v3.5.0 门 195【5 群系 + 12 块 + 3 矿脉 +
  5 装饰 + D9 空腔带修订 = 下界可航行】
- **V3-2 下界内容族**：Part 1 = v3.5.0 门 195（与 V3-1 Phase C 合并
  执行）/ Part 2 下界深化 = v3.7.0 门 197【水蒸发 / 音效族 11 件 /
  magma 光 3 / ambience 第 4 上下文 / soul sand walk_speed_factor
  数据驱动减速】= 全收口
- **V3-3 下界 mob** = v3.8.0 门 198：5 种（zombified piglin / piglin /
  hoglin / magma cube / strider）+ neutral·walks_on_lava 数据旗 +
  下界生成池（Phase B 预留槽零新基建）；ghast 延后（爆炸机制）
- **V3-4 netherite 材料链** = v3.6.0 门 196：debris→scrap→ingot→工具
  2031 耐久 / 护甲 ×1.5 刻度，**零核心代码纯数据**；火免疫/击退抗 =
  后续候选
- **V3-5 建筑内容族** = v3.9.0 门 199：黏土链（水底 sediment 生成 →
  clay_ball×4 → brick → bricks → terracotta）/ 石砖族 / 苔石 / 15 色
  陶瓦染色族；核心仅 worldgen 4 文件
- **V3-6 世界边缘雾** = v3.10.0 门 200：填充感知 fog end = min(静态,
  实际填充盘 cheb 半径 ×16)——流送盘边缘截面雾化淡出，稳态满盘位级
  恒等；**base19 重锚（首重锚）**
- **V3-7 存档 gzip** = v3.1.0 门 191：整档 gzip(JSON) 默认开 + 魔数
  1f 8b 探测零迁移 + 确定性容器 10B 头全钉 + save.compression 严格读
  + format 29；实测 6.3× 门档 / 5.4× 大档
- **V3-8 手柄支持** = v3.3.0 门 193：源位掩码 OR 语义键鼠手柄双连 +
  v1 Xbox 映射 9 钮 2 摇杆 + 无手柄位级恒等；门强度记档（本环境无
  物理手柄 = 全逻辑单测 + 实机验收归用户）
- **阴影缩尺报障**（11-06 用户实机报障）= v3.12.0 门 202：mob 阴
  影 1.5× → 1.0×（= 身体同宽足迹）——核实 = 物理 AABB 质心规则
  （探针实测失接地 = 中心过边缘恰 0.3000，MC 同款）阴影零参与，
  所见 = 落体瞬间 1.5× 阴影角压高处方块的视觉误导；用户定案物理
  不动只缩阴影（玩家第三人称阴影同收拢）。
- **demo sim 版本**（11-06 用户报障 `--demo example.dmo` 被拒"recorded
  with v3.11.0, this is v3.12.0 - incompatible"）= v3.13.0 门 203：
  回放准入 = **独立 sim 版本** `kDemoSimVersion`（仅帧锁定模拟语义
  变更才 +1；纯渲染/UI/音频/内容/文档版不 bump）+ `recorded_with`
  构建串仅信息展示永不拒播 + legacy `version` 指纹 schema **零迁移
  映射 sim 1**（用户手头全部 .dmo 继续可放）+ 混用 schema 点名拒 +
  `--demo-force` 逃生门（仅 bypass sim 门）+ **canary 纪律**（4 个
  shipped demo 每版门内回放 savenorm EXACT = 模拟语义金丝雀，破坏 =
  必须 bump + 重新 author）。
- **V3-9 demo 回放** = v3.11.0 门 201（11-05 用户问答定案，
  矩阵尾部默认执行、可一句话提前）：双入口共享一个注入内核（与 E2E
  输入跑同款输入路径，游戏逻辑零触碰）——**编写** = JSON 动作时间线
  （展开事件流、首放 ×2 字节一致固化，可复用替代 xdotool 门脚本）+
  **录制** = 游玩捕获（帧 dt 序列 + 输入事件流，O(1)/帧零帧率影响）；
  demo 文件 = {版本指纹 TM_VERSION【不匹配拒播】+ 世界起点【fresh
  种子 | 存档槽引用】+ 帧 dt 序列 + 输入事件流} gzip（V3-7 确定性
  容器先例）→ `data/demos/`，**零 format**，~0.2-0.5 MB/小时。
  （v3.13.0 修订：版本指纹 → 独立 sim 版本 + recorded_with 信息串，
  legacy 零迁移，详见「demo sim 版本」条）**回放
  交互模型（定案）**：只观察——允许 = 转镜头（渲染相机偏移 = 过肩
  观察）/ 切换视角（一/三/前视）/ 0.5×·1×·2× 变速（demo dt 序列 =
  唯一时间源，0.5× = 帧保持）/ 暂停（ESC = 暂停菜单非退出）；禁止 =
  一切改变世界的操作（挖掘/放置/攻击/物品使用/背包/移动键/交互）——
  **架构边界 = 输入路由**：游戏输入通道 100% 由 demo 文件供给、观众
  输入只进「相机 + 播放控制」层（白名单非黑名单 = 结构性零遗漏）；
  「转镜头 vs 录制前进方向」分歧问题由此结构性消解（录制玩家移动用
  录制朝向）。v1 偏离记档：无 4×（渲染可持续上限）/ 无即时跳转（帧
  锁定态 = 帧号纯函数 + dt 前缀和，留后）/ 游戏手柄通道 = V3-8 先行
  则立项时加（输入捕获收口点同款）

### 待执行项明细（表外；11-06 用户定案入表 + 大项相位拆分）

- **V3-8b 手柄按键映射定案** = **v3.14.0 门 204（11-06 完成）**：调研 = SDL3 手柄数据库归一化 Xbox/PS/Switch 三大布局到同一逻辑位（单表覆盖，无需多表；山湾克隆 = 数据库已识别 V3-8a 实机铁证）+ 用户三项习惯定案：**L3 = 蹲伏**（补 LeftControl 缺口 MC Java 惯例）/ **RT 扳机 = 疾跑**（0.2 死区数字化，B 解放）/ **B = 飞行 toggle 专用钮**（新 GameKey::FlyToggle 仅手柄键，单击 setFlying(!)，键盘双击 Space 不变，A 双击冗余记偏离）；config 全自定义 = 定案不做；LT/R3/GUIDE/Dpad 上下左保持未映射（记档）；**首个实际 sim bump 1 → 2**（录制的 gamepad 事件按当前解释重放 = v3.13.0 定案「输入路由」条款首实例，4 canary 重新 author，sim-1 旧档 = 点名拒 + --demo-force【纯键鼠录制 force 行为恒等门内实证】）；2787/2787（+3）；门强度记档 = 实机手感验收归用户。
- **V3-10 末地维度**（大项 = 4 相位，11-06 定案拆分；依赖 V3-1
  维度框架先例，执行序 a→b→c→d）：
  - **V3-10a 维度框架 + 真传送 = v3.23.0 门 213（11-07 完成）**（第三
    维度：下界维度基建**第二次消费** = worldgen 纯函数/流送/光照/传
    送管线/存档 format 末地段；规模 ≈ V3-1 Phase B）：入口 A-lite（双
    stateless frame 块 + 数据谓词 end_frame/end_frame_eyed/end_portal
    + 5×5 水平环纯函数族 + ender_eye 物品 + 嵌眼交互：12 全 eyed +
    中心 3×3 air → 激活；frame 不可破坏 = 无去激活路记偏离）+ 最小末
    地（120×120 末地石台地 y[48,52] + 原点出口门 worldgen 常活 + 虚
    空无地板 + 入场平台 5×5 obsidian @(40,52,0) 每次传送写入【偏离
    MC (100,49,0)】+ 落点朝西 +π/2）+ 传送新函数（1:1 无 8:1 / 返程
    = 主世界出生点床校验 / 60f 倒计时 + 900f 冷却复用 / travelPortal
    原文零触碰）+ 渲染 6 点（暗紫黑 clear / 雾 8..48 / inOverworld
    门族 / F3 +end / ambience 第 5 上下文 +ambience_end / u_ambient
    + u_caveAmbient 双 floor 0.25 按维度【门内捕获 shader 无光分
    支】）+ **format 32**（dimension 词表按 format 分词：30/31 携
    "end" = version lie / end + end_entities 段恒写 / endSeed 不落盘
    / PITS 399 四类降级点同 commit 盘点）；块 127→131 / atlas
    166→170 零移位 / item 98→99 / 声音 67→68；门 213：S1 第 94 次
    （≡ base19 + 恰白名单 3 键）+ S2 去程 authored demo（12 嵌眼 →
    activated → Traveled to end ×2 RAW 逐字）+ 返程零输入臂 + 末地
    持久化臂（end 段恰 25 obsidian 平台格）+ S6 五面点名 + canary 4
    EXACT / 2885/2885（+29）/ 性能 A/B 同档；sim 不 bump；PITS 405
    （场景手术先探地形 + shader 无光分支双 floor + GCC-16 假阳性结
    构隔离 + 存档伪造只剥对应后键 + 场景派生同步 fluids）
  - **V3-10b 内容族 = v3.24.0 门 214（11-07 完成）**（末地石原野/基
    岩洋/末地岛 + 末影珍珠投掷——纯数据 + 生成；≈ V3-5 建筑内容族）：
    基岩洋全维 y[0,4]（下界地板镜像，跌落死因 Void→Fall + 珍珠逃
    生面）+ 岛群 pass（lattice 16 p0.15 独立盐，r 5..12 顶 44..56，
    顶 2 层 + 下锥 2 层悬浮岛；air-only 谓词 + 中央足迹护栏保出口/
    平台；跨 chunk 走格外扩 rMax）+ chorus 岛顶装饰（独立盐 p0.6，
    cross stateless + chance_drops ≈ MC；生长不做记偏离）+ biomes_end
    单群系池（10d 缝）+ end_stone_bricks（2×2 配方 count 4 MC 精确）
    + **末影珍珠全链 = 项目首个投掷物**（throws_teleport strict bool
    + 双音配对 + one-right-click 8 面互斥 + material-only + init 交叉
    检；挂点潜行旋转后 placeBlock 前；发射 = 投掷者速度并入 + 30
    b/s MC 绝对值 + 冷却 60f；Projectile 尾参 teleports + stuck 复用
    → pearlLanding：catchUpDisk + 2.5 球面均匀偏移（counter 流）+
    hasRoomAt 上探 3 + fall 复锚消坠伤 + 落乘 + probe 全败吞没 + 无
    自伤【MC 5HP 偏离】+ 0.625× 青色发光立方渲染 + 2 wav 68→70）；
    format 32/sim 2 不变；门 214：S1 第 95 次（≡ base19 + 恰白名单 3
    键零 diff 最强形）+ S2a 去程复跑 + S2b 珍珠臂（threw + pearl
    teleport ×2 逐字 + 冷却窗 no-op + 心满 + 16→15 crop）+ S6 六面 /
    canary 4 EXACT / 2899/2899（+14）/ 性能 A/B 同档；PITS 406
    （lattice 位切 int 截断负值 = 模先于转换 + JS 三元误写 + stale
    assets 再踩）
  - **V3-10c 末影人 = v3.25.0 门 215（11-07 完成）**（End 首个
    mob = neutral + 瞬移 + 携格；≈ V3-3 单 mob，纯数据 + 数据
    旗）：末影人 40 HP/speed 0.8/detect 16/attack 6/0.6×2.9 六盒
    四 swing/light 0（出口门光 11 圈排除）+ **注视激怒**
    （eye_contact_range 48 = kEndFogEnd「看见 = 激怒」定案 + 5 连
    续帧零流计数眨眼复位 + game pass 全 AABB 瞄准射线 + 瞄准块 t
    严格遮挡 → m_angry + 30s 复用）+ **瞬移**（interval 2/6 严格
    配对存在性 = 能力门；动作非状态 MobState 零新增；受击 100%
    damage 只置旗 + update 固定槽帧序钉；流契约 8 候选 × 3 抽 + 1
    重掷 = 25 抽/尝试全败亦消耗；平静 ±16/±8 自位锚 / 激怒 ±8/±4
    朝玩家 min(12,d) 点锚 = MC 项目缩放偏离；executeMobTeleport
    game 缝：catchUpDisk → 候选空旷 ∧ 16 格内有地面【下探穿固体
    虚空 PITS 407⑤】→ teleportTo + fall 复锚 + 落点音 + DEBUG；全
    败 swallowed 行）+ **携格 10c-lite**（carries_blocks +
    carriable 双严格旗缺省冷 + stateful carriable init 点名拒；集
    = {endstone, chorus_plant}；摸每帧 4×3×4 dy 含脚下 x→y→z 定
    序 + 首格遮挡 PITS 407④；放 6000 帧全局长拍；死亡 100% 掉所
    携；0.9× 头顶立方 itemMesh 复用 Opaque/Cutout 双 pass）+ **生
    成** = mob_spawning_end.json 单种族池（nether 孪生
    fail-fast = 10a 两相位槽消费）+ **掉落** = pearl 0-1 + eye 0-1
    【生存唯一来源，10a/10b 记档闭合】+ xp 5 + 声音仅
    enderman_teleport（落点音，70→71；ambient/scream/stare 后移
    10d）；后移：定身/endermite（R11）/主世界·下界池 = 10d 复议；
    format 32 不变 / sim 2 不 bump / 块 133 不变 / atlas 172→174
    零移位 / item 101 不变；门 215：S1 第 96 次（≡ base19 + 恰白
    名单 3 键）+ 无输入末地臂 600f（×2 RAW + 逐字 + end 段恰 4
    air 格）+ 火杀 demo 臂（death f1016 逐字 + 所携 endstone x1
    掉落 + ×2 RAW + crop 目检）+ S6 七面点名 / canary 4 EXACT /
    2930/2930（+31）/ 性能 A/B 同档；PITS 407（demo 槽不回写 /
    火脚级 / 携格带含脚下 / 遮挡首格 / 下探虚空 / 截图 679×744 /
    按住 LMB 挖掘）
  - **V3-10d 末影龙 boss**（血条 UI/AI 阶段/死亡仪式/龙蛋——**唯一的
    真大项** 2 相位，侦察定案拆分 11-07）
  - **V3-10d1 龙核心 + 首杀闭环 = v3.26.0 门 216（11-07 完成）**（≈
    10c×2-3，纯数据 + 数据旗，核心零物种 switch）：龙 = 第 16 mob
    （200 HP/hostile/flying + spawns_in_air + dragon_flight/boss/
    contact 5/火·效果·击退三免疫/breath 8s-3s-r2 + fireball
    2.5s-12b/s + crystal_break_damage 10 + detect_vertical 8/8×4 八
    盒双翼）+ **AI = runAi flying 分派最顶纯新分支**（ghast/bat 位级
    红线）三态复用 MobState（盘旋定心 30【门内裁决 40+pull → 30 移
    pull】/侧移 3.5s + 火球 2.5s 冷却/驻坐出口锚 + 咆哮 1.25s → 脚位
    吐息云 + 三起飞条件 连吐 4/离 150/累计伤 >50）+ **流契约**（每
    帧 ambient 1 抽 + 跨圈帧恰 2 抽 strafe 命中亦消费 u2 = 门内重构
    rollDragonLapDecision 纯函数）+ **水晶新块非实体**（6 固定槽 = 4
    塔顶 + 2 出口 D3；break explode_radius 3 块自身引信 + 无连锁结构
    性质 + shatter_on_arrow 箭扫掠 + 治疗源晶碎反伤 10 + 晶上永久火
    D9 + 普查卸载无关【门内捕获 raw getBlock 0→6 漂】）+ **龙息云零
    物理新实体**（r0+0.15·age；leavesCloud 尾参缺省 false 位级红线；
    dragon_breath 3HP/s 玩家-only 效果 11→12）+ **首杀闭环**（m_dead
    帧蛋 (0,54,0) R14 0 帧差【门内捕获同帧 spawn+kill 漏账死循
    环】+ 升天 60f 飞 + 180f 悬停 + 23 orb = 22×9+2 = 200 整 D5 +
    wail + 240f 延迟门移除；**format 32 零 bump 最强形：击败态 = 蛋
    块世界态**，重载 = 蛋在无复生；末地 orb/云存档静默跳过 = 偏离记
    档 format 33 后移 d2 复议）+ **血条**（renderUi 段 186×8 十段 +
    「末影龙」字体 +3 = 433【扩展区 528/528 满】+ margin 24 门内裁
    名修 + 2s 淡出）+ **--dragon-damage seam**（--explode 孪生——龙
    战斗 demo 不可手编 = 耦合系统定案）+ 生成门（playerInEnd ∧ mobs ∧
    无龙 ∧ 非蛋 → 60f → (0.5,58,0.5)，kSaltDragon）；块 133→135 /
    mob 15→16 / atlas 174→179 零移位 / 声音 71→75 / 配方 113→114；
    门 216：S1 第 97 次（≡ base19 逐字节 ×2 + EXACT）+ S2a 无输入末
    地臂 600f ×2 RAW + EXACT + S2b 破晶 demo 臂（shatter f151 →
    strafe f152 + 火球冷却逐位 + ×2 RAW + 逐字）+ S2c 击杀仪式臂
    （--dragon-damage 200：23 orb f124..f234 逐位 + 终档恰 6 键 +
    ×2 RAW + EXACT）+ 重载臂（蛋在无复生 ×2 RAW）+ S6 十四面点名 /
    canary 4 EXACT【sim 2 不 bump】/ 2959/2959（+29）/ 性能 A/B 同
    档；PITS 408（八坑：init 交叉检顺序 / 数值累加器泄漏 / demo 存档
    红线 / 流送依赖锚点 / -1 哨兵 delta / 同帧 spawn+kill / 血条
    margin / CJK 边界）
  - **V3-10d2 召唤与复杀 = v3.27.0 门 217（11-07 完成）**（小项
    ≈10c×1，V3-10 全 4 相位收官）：crystal_base 块旗 4 块（obsidian/
    bedrock/出口双帧）+ requires_crystal_base（end_crystal 独占）=
    placeBlock 数据门放置规则（零物种 switch，end_crystal 生存可
    放置）/ **4 出口面晶 ∧ 无蛋 ∧ 无龙 → 60f 倒计时**（面晶词表
    ±X = arena 晶算数 + ±Z worldgen 缺席；任一损失取消）→ 4 晶引爆
    【相邻爆波及 = MC 同爆读】+ 蛋计时清除 + 缺位塔晶回写（已载
    only）+ 显式 60f 复生 / **败亡旗**（首杀置位，重载复位——无蛋
    ≠ 首访【门内捕获】）/ **蛋 60s 会话重生**（玩家破蛋 → 3600f
    回写，重载复位 = D7）/ **蛋 blast_resistance 3**（晶爆存活，
    MC 近不可晶爆毁读）/ **复杀 XP = 8**（单 8 值 orb，D5 比例
    1/24 × 200；首杀 200 burst 不变）/ --dragon-damage 每生成施
    加 / **F3 第 21 行 dragon**（全英文零新字形——528/528 满区不
    碰）；format 32 不变（全会话态）/ sim 2 不 bump（canary 4
    EXACT）/ 门 217：S1 第 98 次（≡ base19 ×2）+ 召唤复杀 demo
    臂 ×2 RAW + 逐字【f59 杀1 200XP / f321 破晶蛋存活 / f322 破蛋
    f3922 调度 / f373 summon armed / f433 complete / f493 杀2 + 8XP
    单 orb f558 + 蛋2】+ S2a/10d1 三臂回归同值 + S6 十六面点名 /
    canary 4 EXACT / 2964/2964（+5）/ 性能 A/B 同档；PITS 409（负
    cell 中心 -N+0.5【10d1 词表负侧偏格静默修】/ 无蛋 ≠ 首访 /
    demo 同帧点击死 + hold 续取目标 / 旋转 dy 累计 / /tmp 路径重
    写）
  - **末地 orb 持久化复议**（✅ v3.31.0 兑现，非矩阵编号）：format 33
    end_xp_entities 恒写（10d1 的「120-s 引信限界损失」偏离闭合）
- **V3-11 创造模式**（2 相位，11-06 定案拆分；与 V3-13 无依赖可并
  行，执行序 a→b）：
  - **V3-11a 模式切换 + 不死 + 飞行 + 即挖 = v3.17.0 门 207（11-06
    完成）**：G 切换（locked pointer + 仅存活，C 键范式）+ 创造 = 存档
    flag【format 31 `player.creative` strict bool 恒写，pre-31 迁
    false，30 携键 = version lie】+ 全源伤害门 + 饥饿静态满 + 即挖
    （duration 0）+ 无掉落/无 XP/无耐久 + 放置 3 点免消耗 + 开关 =
    setFlying；sim 不 bump（canary 4 demo EXACT 实证）
  - **V3-11b 创造物品栏 = v3.18.0 门 208（11-06 完成）**：E 面板
    创造双 tab（物品栏/背包，标题行 tab 按钮，生存位级不变）+ 前缀
    搜索（SDL 文本输入新面：StartTextInput 仅创造面板期开 + 帧态
    frameText + [a-z0-9_] 词表 + 24 上限 + 光标闪烁）+ 6×4 网格
    滚轮翻页 + 左键满组/右键单个授予选中热栏位 + 搜索焦点数字语义
    （tab 开时数字 = 文本不切热栏）+ demo author 面扩展（timeline
    mouse_move 可选 x/y 绝对位 + 键词表 +g）；零 format（面板 = 会
    话态）/ sim 不 bump（UI + 授予经既有 API）/ 零 config 键
- **V3-12 多人**（**唯一「超大」= 架构级**，11-06 定案拆分，V3-1
  超大同款处理 = 设计先行，执行序 a→b→c）：
  - **V3-12a 设计定案 = v3.28.0 门 218（11-07 完成，纯文档版）**：
    设计权威入仓 `docs/V3-12-MULTIPLAYER-DESIGN.md`（390 行实证）——
    **方案 1 主机权威 lockstep**（demo 内核 = 客户端循环形状 + 输
    入中继 = Input handle* 第二消费方 + 世界传输零）/ 时间源 = 主
    机广播 clamped wall dt raw f64 位（real-dt 结构性零 desync；
    wall 直读 2 处 simTime 化）/ POSIX TCP 无 net 线程 + 协议
    schema（HELLO 四指纹 / FrameState ~40B / D=2 无预测 / 30 帧哈希
    字段清单）/ Player 双别名最小方案（354 处零改动 + 10 成员迁
    移 + pass 二分表）/ streaming 2 处修正 / 12b = format 32 零
    bump + sim 2 不 bump；R1-R20 / D1-D15
  - **V3-12b 2 人 PoC = v3.29.0 门 219（11-07 完成）**：主机权威
    lockstep 全实现——net/ 协议 + POSIX TCP（HELLO 四指纹 / FrameState
    40B / D=2 无预测 / 30 帧哈希双向交换 / 流重组 R16）+ player 多人
    化重构（双别名 + PlayerSession 14 引用别名 + pass 二分 ×N 全 sim
    覆盖）+ streaming 回退门共享账本 + R2 simTime 2 处 + R10 xpSeed
    盐 + R9 出生点 i + host/client 双循环（lockstep 屏障 + 等包阻塞）
    + 断线移除（100% 掉落 solo 续跑 / 客户端见主机 EOF 全退 D2）+
    CLI --listen/--connect/--script（demo 时间线第四消费方）；
    format 32 零 bump（D13 主机单存档）/ sim 2 不 bump（canary 4
    EXACT）/ 偏离 D1-D20；门 219：S1 第 100 次（≡ base19 ×2）+
    S2 双进程锁步三臂【无输入 ×2 run 哈希逐字 + 主机档跨 run EXACT /
    输入注入 ×2 run client 挖掘落主机 journal / 断线 ×2 run 20 件
    掉落 + 主机续跑 EXACT】+ S6 八面点名 + canary 4 EXACT / 2974/
    2974（+10）/ 性能 A/B 同档；PITS 410 九坑（client 环槽偏 1 /
    主机快跑屏障 / RST≠FIN / WM 平铺 / uiScale 时序 / seed 豁免 /
    无火种子 / --new 净主机 / 起始库存 per-session）
  - **V3-12c1 大厅/聊天/N 泛化/心跳 = v3.30.0 门 220（11-07 完
    成）**：kNetMaxPlayers 2→8（协议形，12c1 门臂 3P 实证）/ accept
    窗 N 定形（首客 10s 阻塞 + 3s 窗 250ms 截止重试 + WELCOME 窗后
    统一 flush = 所有客户端同 N）/ 聊天（T/Enter + frameText 累积 +
    CHAT 包 type 7 {slot,u16 len,≤256B} 客户端→主机重广播 + 8 行环
    浮层 300f 淡出 + Latin only 零字形 + **打字抑制**【Input 可打印
    域 0x20..0x7e owner 机捕获前本地吞——typed 'e' 开面板截断消息
    的门内捕获修】+ 表现层结构不入哈希）/ 名牌（第三人称 world→
    screen 投影，本地玩家无牌）/ F3 第 22 行 players（Latin 零新
    字形）/ 心跳（stall 10s **min 水印**点名移除最滞后——健康同伴不
    遮死者）/ **E 面板中继修**【isLocal 门 = 主机玩家面板 desync
    根，改全玩家处理 + 窗口动作 localOwner 自门】/ 客户端全槽体
    【netAddPlayer 1..n-1——slot 2 缺体 segfault + f0 哈希点名】；
    format 32 不变 / sim 2 不 bump（canary 4 EXACT）/ 大厅 MOTD 意
    念并入 V3-17 主菜单项；门 220：S1 第 101 次 + 3P 锁步臂【4 批
    次哈希跨批逐字恒等 5894…/5b4f… + 主机档跨批 EXACT】+ 大厅截图
    臂【F3 players 行 + 名牌 + 聊天浮层 OCR 实证】+ 聊天臂（xdotool
    T+type+Return 双进程日志逐字）+ S6 八面复验 + accept 窗 2 新面
    【2 客→N=3 / 窗 3.01s 超时→N=2 续跑】；PITS 411（九坑）
  - **V3-12c2 断线恢复 + format 33 = v3.31.0 门 221（11-08 完成，
    矩阵收官项，全 PASS）**：format 33 players[]（root player +
    inventory/inv_grid 迁入 per-player entry，entry 0 = primary 镜像，
    三面 version lie）+ **末地 XP orb 持久化**（end_xp_entities 恒写，
    10d1 后移复议兑现）+ **REJOIN 快照**（save 形 WELCOME = 磁盘档 +
    会话计数族 {weather 5 字段含双 splitmix 流 / spawnCounter×3 /
    particleBreakCounter} + 活粒子 + per-player 会话计时器族
    {prevHealth/hurtFlash/lastSpace/pearl/drownTicks/flying}——全
    presence-gated，on-disk 格式不变 S1 锚免重锚）+ 重连流程（主机
    持久 listener + 中会话非阻塞 accept + startFrame 采纳 + 重连者 =
    新体 D25）+ **Join/Renumber CTRL 广播**（N 增长/移除的 slot 重编
    号，行随玩家移）+ **net 主机抑制幽灵**（载多玩家档 = stale 幽灵
    的 slot/m_nets 错乱 segfault 修）+ feedJournal 改哈希 live-diff
    （load 不变面）+ respawn 精确 float（≤32 int cell 的 0.5-y 丢失
    修）；**mob-quiet 合同**（活 mob 在场 = 点名拒收，全 mob 快照后
    移 D24）；门内捕获八坑全 PITS 412（Json(bool) 指针陷阱 /
    legacyShape-format 矛盾 / respawn 0.5-y / slot 重编号 ring 越界 /
    barrier 停滞 accept 节拍 / client renumber 无 drop / net 主机幽灵
    segfault / feedJournal 原始 cell）；2987/2987（+13：SaveFormat33
    9 + NetSnapshot 4 + codec 扩展）；S1 结构免疫第 102 次（≡ base19
    逐字节 ×2 + savenorm EXACT + on-disk 无 net 段）+ canary 4 EXACT
    （sim 2 不 bump）+ 迁移臂（32→33 值保真 pos/yaw/34 blockEdits/
    inventory）+ REJOIN 臂（slot 2 of 3 @ frame 366 + 三向哈希 f600
    ad509c… 逐字 + 零 desync）+ S6（listen×save 转正 + mob 拒收命名）
    + 性能 A/B 同档（OW 900f upd 在旧树噪声带内 + changesApplied
    9/3/1 + net 自成本 +~0.07ms/帧 sub-ms + 结构零成本 solo）；后移
    候选复议定案：per-player 维度 D9 / 最近玩家目标 D10 / 玩家碰撞 D7
    / 协作挖掘 D6 / 物品交换 D5 / creative 多人 D11 / 本地预测 D3 /
    swing 体 D18 / aim 相机形 D17 全后移（12c2 只做断线恢复 + format 33，
    不做即记档）+ **全 mob 快照传输后移 D24**（mob-quiet 合同，门臂
    mobs-off 实证）
- **V3-16 扩展指导 demo 族**（**11-07 用户实机诉求**——「现有 demo
  都太简单，几十秒就结束」；**相位拆分 11-08 编排者定案：✅ 16a =
  v3.32.0 门 222【内核 + 工具链 + pilot ④ 挖矿打造工具 11387f】→ ✅
  16b = v3.33.0 门 223【⑤体力 18262f + ⑥回血 18344f】→ ✅ 16c =
  v3.34.0 门 224【③房子+床 18875f，day1 ④⑤⑥③ 全 4 支完成】→ ✅
  16d = v3.35.0 门 225【⑦皇宫 19536f，build 章首支，100 块 = 92 墙
  + 8 四角塔，内圈地板 38 块 CUT 记偏离；16d 拆两版 = 11-09 用户
  定案（⑧材料 = 16e v3.36.0）】→ ✅ 16e = v3.36.0 门 226【⑧材料
  25383f，7 biome 床 33 标本收集 + 内核 10 处 demo 恒锁定 + 向量行走
  模型，PITS 417 二十二坑】→ ✅ 16f = v3.37.0 门 227【⑨河流 19841f，挖河 15 + 双舀双倒对称 V 梯度 + 营火水灭 + 火把对比 + 游泳横渡 + E 面板 + 第三人称巡视；1-deep 密封池 + level-7 舀痕钉 + mk_scene fluids/campfires 两 strict 面 + 列/行 2-m 分块末腿同轴纪律，PITS 418 十六坑】→ ✅ 16g = v3.38.0 门 228【⑩黑夜历险 18301f，造火把 + 弓箭 + 打败 zombie + 活到天亮；弓 = 短程 v²/g 11.25-m 弹道 aim pin + release 帧 aim + hold=100 满力 + knockback 定点 5 轮迭代 + 火把环 + seed 7 唯一 hostile，PITS 419 八坑；index survival seq 1】→ ✅ 16h① = v3.39.0 门 229【①下界门 10865f，obsidian 4×5 帧预置 + flint_and_steel 点火（aim 底行顶面朝内面）+ 站入 60-f 传送（8:1 映射落生成帧）+ 下 frame 观下界（玄武岩柱/熔岩海/红天）；返程不做【auto-re-travel 乒乓规避 = 下 frame，② 走 end exit 更自然】，PITS 420 四坑；index dimension seq 1】→ ✅ 16h② = v3.40.0 门 230【②末影龙 15214f，嵌 12 眼【GOOD cell 末位 PITS 421】+ travelEnd + dragon --dragon-damage 200 同帧杀 + 仪式【egg + 23 orb】+ 珍珠 28° 弧 40-m 一掷 + 破 2 出口水晶 + egg 完好 + 返程 exit portal；enderman 威胁 = netherite 甲 80% 减伤 + 珍珠速离 + center 站位 PITS 422 八坑；index dimension seq 2 = V3-16 10 主题全完成】→ coop 占位**）：10 主题玩法指导，
  每支 3-10 分钟级（现行 300-600 帧 = 5-10s 的 10-100×）：① 搭建
  下界传送门 + 去下界 ② 打末影龙（去末地 + 破晶 + 战斗 + 仪式）
  ③ 完整建造一栋房子 + 床 ④ 挖矿打造工具（木→镐→矿石）⑤ 补充体
  力（食物链 + 进食）⑥ 修复回血（回血手段族）⑦ 搭建皇宫（大型创
  造建造）⑧ 收集各种材料（多群系巡游）⑨ 挖出河流（大规模地形改
  造）⑩ 一次完整的黑夜历险（造火把 + 弓箭 + 打败怪物 + 活到天
  亮）；管线 = V3-9 authored timeline（--demo-author + 键鼠词表扩
  充）或录制；**分组定案（11-07 用户裁定 = 两级导航）**：demo =
  教学页内内容（标题页只留「教学」入口，无 demo 列表）+ **数据层
  四轴 schema**（demo 索引 JSON：category / seq = 类内学习顺序 /
  mode = solo|coop 硬约束 / tags = 系统·场景软标签 v1 只存不画 /
  file / length_frames，严格解析）+ **导航 = 分类 → demo 两级**
  （mode 不做导航级：标题页单人/多人上游入口已分上下文，教学页按
  上下文过滤 + coop 条目「需联机」徽章；三级方案 = 用户裁定否，
  理由：多人子树早期为空 + 与上游裁决重复）+ **章节草案 4+1**：
  第一天基础（④⑤⑥③）/ 建造与大地（⑦⑨⑧）/ 维度远征（①②）/
  生存历险（⑩）/ 多人协作（coop，V3-12 联机后填）；确定性纪律
  （×2 RAW + savenorm + 无火种子域）；sim 版本 bump 按纪律（输入
  路由/时长 pacing 变更 = 评估面——**panel_click 实证 = 增量事件不
  bump，canary 4 EXACT 执法**）。**16a 内核（v3.32.0）**：scene
  世界 spec 第三形【committed 确定性存档资产只读】+ panel_click
  几何无关格地址【resolver 三开态 + FurnaceLayout 共享格】+ 索引
  数据层【四轴 schema + init 交叉检查 + --list-demos】+ 工具链
  【mk_scene.py 场景手术幂等 + demo_author.py Guide DSL】+ pilot ④
  鲁棒性定案【轴对齐行走 + 相对放置瞄准 + 逐格贴线挖矿 + 磁吸
  1m 收集带——PITS 413 全 19 坑】
- **V3-17 主菜单系统** = **17a ✅ v3.43.0 门 233（11-12：第 4 循环 runMenuLoop + 标题页 + 3 stub + 字体 +21 代码点序插入重格 2112→2208 + S1 重锚 base20 + menuEnabled 五形绕开【demo/net/script/--new/管理旗】+ buildUiAssets 幂等提取 + 菜单退出 save 写跳过；click 臂 单人≡--new savenorm EXACT；PITS 425 四坑）** + **17b ✅ v3.44.0 门 234（11-12：设置页 8 行表单 stepper/toggle 零文本输入 + ui.scale 唯一运行时生效面【菜单 reflow 活体证明】+ 写回 = 既存 Config::save 整档 dump【值全保，键序归一记录偏离】+ 字体 +3 屏宽项【headroom 承接不增 atlas = v-UV 不变，canary 跨树 RAW 恢复 EXACT ×4】；设置臂写回断言 + a3 scale2.0 ×2；PITS 426 两坑）** + **17c ✅ v3.45.0 门 235（11-12：登陆页【文本输入生命周期 m_textInputOn 页入开/页出关 + frameText/frameBackspace + latinOnly + 12 cap + m_menuName 默认 player 会话态零 format】+ 教学两级【5 章 + 计数 / 章内 demo name·mm:ss + coop 需联机拒收】+ **startDemoFromMenu = CLI --demo 位级孪生**【applyDemoArgs init 块提取共用单源——菜单 ≡ CLI savenorm EXACT 实证】；零字体跨树 RAW EXACT ×4；PITS 427 四坑）** + **17d ✅ v3.46.0 门 236（11-12：多人页【名字共享 m_menuName + 创建游戏/加入游戏】+ 加入子页【host:port + menuParseHostPort 纯函数 7 合法/10 拒收】+ kNetDefaultPort 5555 具名 + netClientStart/netPostScene init 块原文提取【CLI 位级，菜单 menuExit 3/4 共用】+ WELCOME +hostName +motd【混版 HELLO 先拒 = 兼容性免费】+ MOTD = config net.motd 默认 TokenMine.cpp → 客户端首聊行【12c1 大厅意念最小形】+ R7 菜单面写档守卫【门内定案捕获】；网络臂 A/B/D 三形×2 全绿；PITS 428 四坑）** = V3-17 全 4 相位收官**，**11-07 用户实机诉求**——
  「demo 要分组展示；游戏打开后有设置/登陆/教学页面，多人模式开
  发好后也少不了联机界面」）：启动 = 主菜单（取代直进游戏形）：
  ① 标题页【游戏 logo + 单人 / 多人 / 设置 / 教学 / 退出——无 demo
  列表，demo = 教学页内内容（11-07 定案）】② 设置页（config 表单化编辑：view_dist-
  ance/day_length/天气/声音/ui.scale/窗口——严格读 + 写回 config.
  json，config 纪律不变）③ 登陆页（本地名字——net HELLO 名字载体
  消费，singleplayer 可选）④ 教学页（V3-16 demo 族宿主：分类 → demo
  两级 + mode 上下文过滤 +「需联机」徽章，schema/章节草案见 V3-16 定案）
  ⑤ 多人页（主机 --listen / 加入 --connect host:port + 名字；
  12c1 大厅 MOTD 意念并入此项）；架构面 = 游戏 init/loop 前的新
  状态层（表现层零模拟——确定性纪律：菜单期无输入 = 字节可复
  现）；字体面评估（中文菜单字形——现有 528/528 满区 + 扩展区先
  例）；相位拆分（✅ 17a 标题 + 教学入口 = v3.43.0 门 233 + ✅ 17b 设置 = v3.44.0 门 234 → ✅ 17c 登陆 + 教学页两级 = v3.45.0 门 235 →
  ✅ 17d 多人页 + MOTD = v3.46.0 门 236）【V3-17 全 4 相位完成】
- **V3-18 存档扩展名** = **v3.42.0 门 232（11-12 完成，11-08 用户诉求**——「.json 扩展
  名体现不出压缩后的 json，与没压缩的混淆」）：现状 = V3-7 确定性
  gzip 容器（10B 头全钉 + 魔数 1f 8b 探测零迁移）但落盘名仍
  `data/saves/<slot>.json`（game_init.cpp:2468 单构造点）= 压缩态不可
  见。**扩展名调研**：**`.json.gz` = 事实标准**（gzip CLI 约定
  `gzip x.json → x.json.gz`；数据生态通用「内容类型 + 压缩」两段式
  扩展名，tar.gz 同族；`.jsonz` = 非标准、无工具生态，不推荐）→
  定案方向：新档写 `<slot>.json.gz` + 读路径两级探测（.json.gz 先，
  legacy .json 后 = 零迁移，V3-7 魔数探测同款先例）。影响面 = 路径
  构造点 + --help 文案 / `--list-saves` 双扩展名枚举 / **demo scene
  资产**（assets/demos/scenes/*.json 同为 gzip，V3-16a 形，随改 +
  init 交叉检路径）/ gate_compare.py（已魔数探测，零改动）/
  CONTENT_GUIDE_CONFIG 存档使用节；零 format（容器不变）/ 零像素
  （纯 IO）
- **V3-19 死亡屏计时** = **v3.41.0 门 231（11-12 完成，11-08 用户报障**——「死亡界面
  存活时间不停增加」）：根因 = 死亡屏存活行活读 `m_frameCount`
  （game_ui.cpp:2024）——死亡后世界模拟照跑帧继续推进，数一直涨；
  其余四行（采挖/放置/击杀 = 会话计数器、行走 = 物理冻结）天然静止，
  唯此行脱缰。修 = 死亡瞬间快照（问答会话已核证定位，规格即贴）：①
  PlayerSession + `int deathFrame = -1`（game.hpp）② updateSurvival
  存活→死亡转移块 SURV-7 deathCount++ 旁 `s.deathFrame = m_frameCount`
  ③ 死亡屏读改快照（-1 回落 = 旧读法，不可达）。零 format / 零
  config / 表现层（会话态）；门 = V3-13 死亡屏臂加一断言（死亡屏两个
  不同帧采样，「存活」行文本逐字相同——既有 ×2 RAW 同帧协议抓不到
  跨帧漂移）→ **实证：新树两帧 crop AE=0 冻结 vs 旧树（tm_v340）同臂
  AE 43/42 px 爬升复现【TDD 前置：「4 秒→7 秒→10 秒」秒位字形差】+
  survivalStatsLine 纯函数 2999（+1）+ PITS 423 两坑**
- **V3-13 死亡统计屏 / Game Over 屏** = **v3.15.0 门 205（11-06
  完成）**：死亡屏 2 行 → 统计屏（致死原因行 = damage DamageInfo
  重载 9 死因措辞全字体表内【+1 字形「于」428】+ 会话统计 5 行
  采挖/放置/击杀/行走/存活 = SessionStats 不存档记档偏离 + 行走
  传送阈值守卫）；Game Over 屏 = 死亡屏本身（无 hardcore = 记档偏
  离）；2792/2792（+5）；门 205 S2 = 场景手术 24 格竖井坠落 ×2 RAW
  + savenorm EXACT。
- **V3-14 内容遗留·下界 mob 深化**（4 相位，11-06 定案拆分；执行
  序 a→b，c/d 可与 b 后并行）：
  - **V3-14a 爆炸机制 = v3.19.0 门 209（11-06 完成）**（新机制族核
    心：伤害衰减/破块/点火——ghast 前置，单独立项）：纯核心
    `scanExplosion`（包围盒 x/y/z 序确定扫描 + 格中心距离 + 局域强度
    radius−dist）+ `explosionDamage`（线性衰减 ceil 整读，界 = 0）+
    `Game::explodeAt` 接线（setBlock 收口 + 碎片种子契约 + boom 音 +
    中心余烬 + 玩家/mob 衰减伤害）+ 块属性 `blast_resistance`（obsidian
    = 3 数据存活）+ `--explode x y z [r]` CLI seam（--weather 孪生，
    帧 0 应用；demo 互斥）+ 死因「死于爆炸/被 <name> 的爆炸杀死」
    【字体 +2 字形 爆/炸 428→430】+ boom.wav【63→64】；零 format
    31 不变 / sim 不 bump / 无击退（项目无玩家击退面 = 记档偏离，
    V3-15 先例）/ 破碎无掉落（MC 孪生）/ 点火 100% 确定（creeper 25%
    roll = 偏离）/ 流体不触碰
  - **V3-14b ghast = v3.20.0 门 210（11-06 完成）**（飞行 + 咆哮
    + 爆炸召唤，消费 14a）：flyShoot 分支（flying && shootRange>0
    数据门，bat cruise 位级红线）+ explode_radius/shoot_sound 数
    据属性（ammo 规则豁免爆炸射手 + init wav 交叉检）+ 火球 =
    Projectile 复用（damage 0/hurtPlayer/explodeSource="ghast"
    尾参缺省位级）+ 爆炸三接线（stuck 即爆/玩家接触爆/mob 扫描
    跳过）+ 火球发光立方渲染 + 内容族（ghast.json/ghast.png
    16×16 atlas [9,10] 165→166/ghast_membrane 物品 + 32×32 独立
    图标/声音 2 件 64→66/池 +ghast 2 总 33）；**门内捕获 = V3-14a
    字体码点笔误修复**（爽 U+723D → 炸 U+70B8，九 glyph 重格
    +1，top-512 红线 + S1 ≡ base19 字节恒等）；零 format 31 不变 /
    sim 不 bump（canary 4 EXACT）；2833/2833（+12）；记档偏离：
    火球 = 箭弹道（近程弧线补偿朝上）+ 射手自伤（MC 免疫自身火
    球）
  - **V3-14c piglin 交易 = v3.21.0 门 211（11-06 完成）**（barter
    物物交换 + 激怒联动既有 neutral 类）：barter_input
    gold_ingot 单输入 + barter_outputs 12 项加权表（MC 比例项目
    近似 Σ=249，netherite_scrap 1 头奖；数量 [1,64] drops 0 概率
    式拒收）+ checkBarter 纯函数 init 交叉检（checkWoolDrops 范
    式）+ tryBarter（tryMilk 范式挂点）+ barterItem() 无状态
    roll（单 uniform01 + 单 nextMix）+ **不激怒 = 结构性质**（零
    damage 路径）+ barter.wav 66→67；零 format / sim 不 bump
    （canary 4 EXACT）/ 2847/2847（+14）；门 211 S2 交易臂 =
    authored demo（视线沟 + 12 拒生沟 + kReach 边界校准准星）；
    PITS 403（demo 观察红线 = 无退出写 + run 目录 stale assets +
    ambient 每帧耗流 = roll 断言必帧锁定）
  - **V3-14d strider 骑乘 = v3.22.0 门 212（11-06 完成）**（骑乘机制
    = 新基建，**V3-14 全 4 相位收官**）：strider.json +rideable
    strict bool + Player m_carried 承载旁路（wish 锁 + move/stepUp
    旁路 + integrate no-op + fall 复锚 = 落乘 1.05 落差永不坠伤）+
    MobEntity m_ridden + riderSteer（WASD 直控 + kRideDecel 0.9 漂
    停 + ambient 流契约保留 + collidesWithEntities opt-out 分离
    pass 不顶坐骑）+ Game ride pass 单收口（落乘条件 6 项 + 承载
    snap 背位 1.05 + steer 推送）+ tryRide（tryMilk 范式）+ Space/
    LeftControl 落乘（Space 绕双击飞行窗）；**承载 = 会话态零
    format**（mob 不入档 = 唯一自洽解）；MC 语义偏离 6 记档（无
    saddle/WASD 直控/无水伤下沉/原地落乘/无脚步音/站姿背位）；
    门 212 S1 第 93 次 + S2 骑乘臂 authored demo（rode/dismounted
    ×2 逐字 + RAW 恒等 + 第三人称视觉）+ 无输入臂 7/7 + canary 4
    EXACT / 2856/2856（+9）/ 性能 A/B 同档；PITS 404（git
    checkout 还原两连踩 + 视线三件 + 瞄准换算 + 双击窗绕过 +
    分离 opt-out 首消费）
- **V3-15 内容遗留·生存/表现小批** = **v3.16.0 门 206（11-06 完
  成）**：netherite 火·熔岩免疫（fire_resistant armor-only 属性 + 熔岩
  tick/点燃/火 tick 三抑制 + 熔岩死因覆写「死于熔岩」；**击退抗性弃
  做** = 无玩家击退面无消费方记档）/ portal UV 动画（2 周期纹理 +
  fragment tile-rect 滚动 phase 120f frame-locked，portal.anim off =
  位级）/ 蒸汽粒子（8×8×9 盒 1/10 roll 确定性发射，portal.steam 门
  控；营火既有火焰粒子 = 覆盖）。

> **条目说明（11-03 用户审议；11-04 定案：6 项全保留 + 用户诉求 V3-7
> 先执）**：
> - **V3-7 存档 gzip 压缩**（11-04 用户实机诉求【data/saves 体积大】，
>   先于 V3-1 执行 = v3.1.0；V2-19 提前项同款先例）：整档
>   gzip(JSON) 落盘——加载魔数 `1f 8b` 自动探测（旧明文档零迁移直
>   读）+ **确定性 gzip**（mtime=0 / 无文件名 / 固定 GZIP 头 = 同世
>   界字节可复现，一级纪律）+ 新 config 键 `save.compression:
>   "gzip"|"none"` 严格读（render.ao 同款纪律，**默认 gzip = 用户定
>   案**，none = 旧行为位级恒等）+ format 29；zlib 已是链接依赖（自
>   研 PNG 在用）= 零新依赖。改动面 = save.cpp 写侧（gzip 后写）+
>   读侧（魔数探测解压）+ `tools/gate_compare.py` savenorm 加解
>   压步 + 单测族（roundtrip / 遗留明文 / 非 JSON 拒 / 确定性钉 /
>   config 严格读 ×3）；门 = 结构免疫（存档 = 渲染无关路径，零像素
>   变更）+ S6 点名 + 性能 A/B（用户硬要求，IO 增量实测口径）。
>   实测压缩率（11-04）：4 万块 journal 档 1.74 MB → 0.32 MB =
>   **5.4×**（−82%）/ 小档 84% / 2.7 MB 门档外推 ≈ 0.5 MB。
> - **V3-1 下界维度·传送门与下界**（超大，11-04 定案拆分立项先行）：
>   obsidian 传送门框（激活条件/交互）+ 第二世界空间（独立 worldgen /
>   流送 / 光照）+ 过渡帧管线。项目最大架构项，立项先做设计定案 + 拆分
>   方案（门框 → 下界世界 → 内容三段）。黑曜石来源既有（水-熔岩反应）。
> - **V3-2 下界内容族**（依赖 V3-1）：netherrack 族方块 / 下界矿脉
>   worldgen / 装饰——纯数据驱动扩展（GEN-3 矿脉机制既有）。
> - **V3-3 下界 mob**（方向，依赖 V3-1/V3-2）：数据驱动 + V2-2 per-face
>   消费。
> - **V3-4 netherite 材料链**（方向，依赖 V3-1）：第四材料档（工具/护
>   甲）；SURV-3 耐久 + UX-5 修复机制既有，= 内容 + 配方。
> - **V3-5 建筑内容族**：clay / 苔石 / 砖等建筑多样性（纯内容，1.x
>   染色/配方机制既有）。
> - **V3-6 世界边缘雾/遮挡**：消费「世界边缘即虚空（无雾）」记档（流送
>   边缘悬空截面）。
> - **方向项（11-06 编号入表）**：死亡统计屏/Game Over = V3-13 /
>   末地 = V3-10 / 创造 = V3-11 / 多人 = V3-12 / 内容遗留 =
>   V3-14 + V3-15 / 手柄映射 = V3-8b——用户 11-06 定案全部入 3.x
>   矩阵（范围仍立项时定案，执行顺序待用户定）。

## 执行顺序

1. V3-N = 执行序号（**V3-7 存档 gzip = v3.1.0 已完成**【门 191 全
   PASS，11-04】→ **V3-1 Phase A 门框 = v3.2.0 已完成**【门 192 全
   PASS，11-05】→ **V3-8 手柄支持 = v3.3.0 已完成**【门 193 全
   PASS，11-05】→ **V3-1 Phase B 下界世界 + 真传送 = v3.4.0 已完
   成**【门 194 全 PASS，11-05，format 30】→ **Phase C 下界内容族
   = v3.5.x 系列（与 V3-2 下界内容族合并执行）→ V3-3 下界 mob →
   V3-4 netherite 材料链 → V3-5 建筑内容族 → V3-6 世界边缘雾**
   → V3-9 demo 回放【矩阵尾部】 → **V3-9
   demo 回放 = 矩阵尾部默认**【11-05 定案，用户可一句话提前】→
   **11-08 用户诉求 2 项：V3-18 存档扩展名（SAVE 行 ③，行序先于
   V3-17）+ V3-19 死亡屏计时（UX 行 ⑧，行序 V3-16 残余后）【均可
   一句话提前，皆为小项】；立项
   调研 `/tmp/opencode/v31_recon.md`，Phase A 设计稿
   `/tmp/opencode/v320_design.md`；实机报障项入 UX 行续编号）。
2. 一任务一 phase，走标准阶段流程（详见 AGENTS.md）。
3. 版本号逐版 bump（v3.0.0 = 3.x 开启版已发【纯文档 + banner】；
   v3.1.0 = V3-7 首功能版；PITS 355 三件套）。
4. **每版发布前必做性能 A/B**（用户硬要求），旧侧 = 上一 release tag 或
   指定 commit。
5. tag/release 政策（**用户定案：3.x 不打逐版 tag/release，只留节点**）：
   11-05：v3.1.0–v3.10.0 只留 **v3.9.0** 代表（9 release + 9 tag 删除，
   正文归档 /tmp/opencode/v3x_releases_archive/ 留底）；v3.0.0 开启版保留。
   11-13：v3.12.0–v3.46.0 只留 **v3.31.0（1f36bf2）/ v3.40.0（0575203）**
   两节点 tag（用户字面定案；33 tag 删除全无 release，SHA 清单见
   变更记录 + /tmp/opencode/v3x_del33_shas.txt 留底）；**两节点 tag 各发
   节点 release**（v3.31.0 = 多人联机收官 / v3.40.0 = notes 覆盖
   v3.12.0–v3.40.0 29 版，v3.40.0 = GitHub Latest——同日用户修正：
   v3.31.0 补 release）。v3.x 终态 = 5 tag（v3.0.0 / v3.9.0 /
   v3.11.0 / v3.31.0 / v3.40.0）4 release（v3.0.0 / v3.11.0 / v3.31.0 /
   v3.40.0）。
   2.x 历史 = v2.0.0 / v2.10.0 release + v2.19.0 收官 tag（无 release，
   历史遗留）。
6. **11-06 矩阵扩容 + 大项相位拆分**：6 个 ⬜ 项（V3-10 / V3-11 /
   V3-12 / V3-13 / V3-14 / V3-15）执行顺序待用户定案——**大项按相位
   取**（V3-8b = v3.14.0 已完成）——建议序 = V3-13（小·UX 收尾）→
   V3-15（小批·纯数据）→ V3-11a → V3-11b → V3-14a（爆炸机制）→
   V3-14b（ghast）→ V3-14c/d → V3-10a → V3-10b/c → V3-10d（末影龙）
   → V3-12a（多人设计稿）→ V3-12b（v3.29.0）→ V3-12c1（v3.30.0，
   11-07）→ **V3-12c2（矩阵收官项）→ V3-16 扩展指导 demo 族 →
   V3-17 主菜单系统**（11-07 用户实机诉求入表，小→大序：demo 族 =
   内容先行，菜单 = 展示容器后行）。

## 变更记录（3.x）

- **11-13（GitHub tag 精简，用户要求）**：v3.12.0–v3.46.0 逐版 tag 35 个
  （≈ 一 commit 一 tag）→ 只留 **v3.31.0（1f36bf2）/ v3.40.0（0575203）**
  两节点 tag（用户字面定案）；**33 tag 删除**（全无 release，SHA 留档：
  648b80d / 0705139 / d61a757 / 55da85d / 6b031c5 / 57c417e / b71cb5e /
  e1320b4 / 113ed11 / 11c1840 / bc26bed / d0c0435 / e2e2071 / 23c4e7c /
  94dc3d9 / 3f9ae83 / a8259c8 / 1f2e10f / d316cfc / 75266af / 477082c /
  7e4a1a2 / 00cb62a / 4139b4e / 0b45caf / 5f9e1d4 / 001f696 / b65f32b /
  f4b7aab / 2409b26 / 1b8ebc0 / 3cdb7b9 / aac5fa7，完整 tag→SHA 表
  /tmp/opencode/v3x_del33_shas.txt）；**v3.40.0 新增单一代表 release**
  （title "TokenMine.cpp v3.40.0"，notes 覆盖 v3.12.0–v3.40.0 29 版 =
  末地维度 / 多人联机 / 教学 demo 族 10 主题 / 下界战斗链 / 创造模式 /
  打磨）；**同日用户修正：v3.31.0 亦发节点 release**（多人联机收官：
  REJOIN + format 33 players[]，title "TokenMine.cpp v3.31.0"）→
  **GitHub Latest = v3.40.0**（`gh release edit --latest` 显式定标，
  v3.11.0 摘 Latest）。v3.x 终态 = 5 tag 4 release（v3.0.0 / v3.11.0 /
  v3.31.0 / v3.40.0）+ 孤儿 tag v2.19.0 / v3.9.0（历史遗留无 release，
  留待用户定夺）。执行顺序 #5 已刷新。
- **11-06（V3-15 生存/表现小批完成）**：**v3.16.0 已提交**（门 206 全
  PASS：结构免疫第 86 次【S1 ≡ base19】+ S2a portal 场景【存档 ×2 RAW
  + 动画关控制臂 ×2 RAW + 单跑目检漩涡流动/蒸汽 + f400/f460 diff 3560
  px；场景去火 PITS 398① + 动画内容门协议 PITS 398③】+ S2b 熔岩池
  双臂【免疫 health 20 ×2 存档 RAW / 对照 health 0 ×2 + 「死于熔岩」
  目检】+ 2795/2795 +3 + 性能 A/B 同档；netherite fire_resistant
  免疫三抑制 + 熔岩死因覆写 + portal 2 周期纹理 UV 滚动 + 蒸汽粒子 8
  ×8×9 盒；击退抗性弃做 = 无玩家击退面记档。详见
  `docs/archive/phase-3.16.0.md`）。
- **11-06（V3-13 死亡统计屏完成）**：**v3.15.0 已提交**（门 205 全
  PASS：结构免疫第 85 次【S1 ≡ base19 逐字节 + 字体扩展区重排格零像
  素复核】+ S2 场景手术 24 格竖井坠落死亡 ×2 RAW + savenorm EXACT +
  死亡屏目检【「死于高处掉落」+ 5 统计行】+ 2792/2792 +5 + 性能 A/B
  同档【changesApplied 9/3/1 逐位 + 终档 savenorm EXACT】；致死原因
  = damage DamageInfo 重载 9 措辞全字体表内【+1 字形「于」= 字体
  428 扩展区重排格，top-512 红线位级恒等】；会话统计 = SessionStats
  不存档记档偏离；Game Over 屏 = 死亡屏本身记档偏离；PITS 397【fScale
  不含 ui.scale + 加字工作流 + vector 末元素无尾逗号正则必漏】。详见
  `docs/archive/phase-3.15.0.md`）。Release 政策延续 = tag + 提交无新
  release。
- **11-06（V3-8b 手柄映射定案完成）**：**v3.14.0 已提交**（门 204 全
  PASS：结构免疫第 84 次【S1 ≡ base19 committed config 口径逐字节】+
  S2 sim-2 canary 4 demo ×2 RAW + savenorm EXACT + **S2b 首个实际 sim
  bump 实证**【sim-1 旧 canary 点名拒 + --demo-force + force 终档
  savenorm ≡ sim-2 = 纯键鼠行为恒等】+ 性能 A/B 同档【终档 savenorm
  EXACT】；2787/2787 +3；L3 = 蹲伏 / RT = 疾跑 / B = 飞行专用钮（用户
  三项定案，config 自定义不做；SDL 三大布局归一化 = 单表无需多表）；
  PITS 396【Python 镜像常量 SIM_VERSION 漏 bump 门内捕获 + kKeyCount
  尾钉】；实机手感验收归用户。详见
  `docs/archive/phase-3.14.0.md`）。Release 政策延续 = tag + 提交无新
  release。
- **11-06（矩阵扩容：方向项/内容遗留入表）**：用户定案 = 方向项
  （末地/创造/多人/死亡统计屏）+ 内容遗留（ghast/piglin 交易/strider
  骑乘/netherite 火免疫/portal UV/蒸汽粒子）+ 手柄映射定案**全部入
  3.x 矩阵** = V3-8b + V3-10~V3-15（7 项 ⬜，矩阵 11 → 18 项）；
  范围仍立项时定案，执行顺序建议见「执行顺序」节。纯文档（BACKLOG
  矩阵 + 待执行项明细 + GAMEPLAY 下一步节 + AGENTS 现状行最小同步）。
- **11-06（demo sim 版本）**：**v3.13.0 已提交**（门 203 全 PASS：
  结构免疫第 83 次【S1 ≡ base19（committed config ui.scale 1.5 口径）
  逐字节 + 用户 2.0 口径 ×2 自洽】+ S2 四 demo 回放 ×2 RAW + savenorm
  EXACT + **S2b 用户报障场景实证**（git 4531c42 旧 example.dmo legacy
  回放成功且终档 ≡ 现行 schema 回放 = 映射行为恒等）+ S2c sim-2 拒
  点名 + force 旁路 + S6 force 无 --demo rc 2 + 性能 A/B 同档；2784/
  2784（= 2777 + V3-8a 手柄 1【d5a5ea6 并行提交】+ 本版 6）；详见 `docs/archive/phase-3.13.0.md`）。Release 政策 =
  11-05 定案延续 → v3.13.0 只留 tag + 提交，无新 release。
- **11-06（实机报障：阴影缩尺）**：**v3.12.0 已提交**（门 202 全
  PASS：结构免疫第 82 次【对 base19】+ 前后对比 2896 px 三簇阴影带
  单调变亮 0 违例 + 性能 A/B 同档【终档 savenorm EXACT】；2777/
  2777；物理 = AABB 质心规则探针定案不动，阴影 1.5× → 1.0×；详见
  `docs/archive/phase-3.12.0.md`）。Release 政策 = 11-05 定案（逐版
  release 撤销，v3.11.0 = 期合并 release）→ v3.12.0 只留 tag + 提交。
- **11-05（V3-9 完成 = 3.x 矩阵收官）**：**v3.11.0 已提交**（门
  201 全 PASS：结构免疫第 81 次【对 base19】+ S2 六臂【shipped 示例
  回放 ×2 首放固化 / 录制→回放闭环 2810 B / 编写臂位移 4.121 块 /
  观众路由姿态恒等 / 变速双向 EXACT+76px 角标 / 结束面板暗帷 32.0】+
  S6 7 面点名 + 性能 A/B 同档【回放自成本 sub-ms】；2777/2777；门内
  捕获并修复 demo UI 绘制序缺陷；详见
  `docs/archive/phase-3.11.0.md`）。**3.x「世界扩展」矩阵 11 项编号
  任务全 ✅**（v3.1.0–v3.11.0，门 191–201）；方向项（死亡统计屏 /
  Game Over 屏 / 末地 / 创造 / 多人）= 矩阵外，立项需用户定案。
  **Release 政策按 11-05 用户定案执行**：逐版 release 撤销（v3.1.0–
  v3.8.0 + v3.10.0 九 release/tag 已删，v3.9.0 保留为期间代表），
  **矩阵收官 v3.11.0 = 单一 3.x 期合并 release**。
- **11-05（GitHub release 精简，用户要求）**：3.1.0–3.10.0 逐版 release
  太频繁 → 只留 **v3.9.0**（用户字面定案；实际最新版 v3.10.0 一并删除，
  Latest 现指 v3.9.0），9 release 删除（v3.1.0–v3.8.0 + v3.10.0；正文归档
  /tmp/opencode/v3x_releases_archive/ 留底）+ 9 tag 删除（cc3dbb4 /
  c9bf08d / 0824f6f / 951628d / 27f374e / 9d78d6d / 44cfa95 / ae1fd92 /
  68c2bbb）；v3.0.0 开启版 + v1.x / v2.x release 不动。tag = 1:1 release
  （24 tag = 23 release + v2.19.0 历史遗留无 release）。执行顺序 #5 已
  刷新；后续 v3.11.0 收官单一 release。
- **11-05（V3-6 完成）**：**v3.10.0 已发布**（门 200 全 PASS：
  **base19 重锚首重锚**【diff 20940 px 全盘缘带 + 单调趋天空 0 违例
  + 存档 EXACT】+ fog-off ≡ base18 三方闭环 + 性能 A/B 同档；2734/
  2734；详见 `docs/archive/phase-3.10.0.md`）。下一项 = **V3-9 demo
  回放 = v3.11.0【矩阵收官】**（设计定案 /tmp/opencode/v3110_design.
  md：注入内核 = Input handle* 收口 + 双 Input 路由边界 + 0.5× render-
  only 帧保持 + V3-7 gzip 容器）。
- **11-05（V3-5 完成）**：**v3.9.0 已发布**（门 199 全 PASS：结构
  免疫第 80 次【savenorm EXACT——生成内容不入 diff + 锚图零水域】+
  S2 三臂【密封水室展示行 ×2 / 破 clay → 4× clay_ball / 熔炼 → brick
  fuel 4201 验算】+ S6 4/4【含染色族缺一色交叉检】+ 性能 A/B；
  2724/2724；发行世界 census clay 211/mossy 26；详见
  `docs/archive/phase-3.9.0.md`）。下一项 = **V3-6 世界边缘雾**
  （v3.10.0：备忘 /tmp/opencode/v310_recon.md——PRE-2 雾已存
  end=min(far, 流送半径)；实证锚图碎片 = 60f 填充中期盘缘，稳态盘缘
  1024 > 雾 end 1000；候选 B 动态雾 end = min(现有, 实际盘缘距离)，
  预期重锚）→ V3-9 demo 回放【设计定案 /tmp/opencode/v3110_design.md】。
- **11-05（V3-3 完成）**：**v3.8.0 已发布**（门 198 全 PASS：结构
  免疫第 79 次 + S2【下界生成流 ×2 恒等 / **strider 熔岩面 f660 y=32.0
  ×2 + 零 burn** / 池隔离】+ S6 6/6 + 性能 A/B 同档【终档 savenorm
  EXACT】；2697/2697；详见 `docs/archive/phase-3.8.0.md`）。下一项 =
  **V3-5 建筑内容族**（v3.9.0：22 块含 16 色陶瓦染色族 / 2 物品 /
  19 配方 / 水底 sediment 生成 pass——设计定案
  /tmp/opencode/v390_design.md，锚图零水域 = 结构免疫第 80 次口径）
  → V3-6 世界边缘雾 → V3-9 demo 回放。
- **11-05（V3-2 Part 2 完成）**：**v3.7.0 已发布**（门 197 全 PASS：
  结构免疫第 78 次 + S2 三臂【下界截图 ×2 RAW + ambience_nether 事件
  逐一对 / **soul sand 步行比值 0.403** / 桶蒸发 + 主世界对照水正常
  放置】+ S6 3/3 + 性能 A/B 同档【终档 savenorm EXACT】；2658/2658；
  收口 v3.4.0/v3.5.0 四条偏离——详见 `docs/archive/phase-3.7.0.md`）。
  下一项 = **V3-3 下界 mob**（zombified piglin/piglin/hoglin/magma
  cube/strider + neutral·walks_on_lava 旗 + 下界生成池——Phase B 已留
  维度就绪架构零新基建）→ V3-5 建筑内容族 → V3-6 世界边缘雾 → V3-9
  demo 回放。
- **11-05（V3-4 完成）**：**v3.6.0 已发布**（门 196 全 PASS：结构
  免疫第 77 次 + S6 5/5 + 性能 A/B 同档【终档 savenorm EXACT】+ 熔炼
  臂实证；2640/2640；**零核心代码 = 纯数据**——ancient_debris →
  scrap → ingot（4+4 gold）→ 工具 3【2031 耐久/fast_against 21 块】+
  护甲 4【×1.5 刻度升级配方】；详见 `docs/archive/phase-3.6.0.md`）。
  下一项 = **Part 2 下界深化**（流体规则/音效族/ambience/视觉打磨）
  → V3-3 下界 mob → V3-5 建筑内容族 → V3-6 世界边缘雾 → V3-9 demo
  回放。
- **11-05（V3-2 Part 1 / Phase C 第一段完成）**：**v3.5.0 已发布**
  （门 195 全 PASS：结构免疫第 76 次【S1 ≡ 锚 + 白名单 4 键 + atlas
  幂二增长像素行恒等实证】+ S2 内容双臂【空腔观感 + 下界采矿 quartz
  入库存】+ S6 5/5【门内发现并修复校验时序 fail-fast】+ 性能 A/B；
  2617/2617；**D9 带修订 = 下界可航行空腔**（编排者布局裁决）；详见
  `docs/archive/phase-3.5.0.md`）。下一项 = **V3-4 netherite 材料链 +
  Part 2 下界深化**（ancient_debris → netherite 冶炼链 / 群系深化 /
  流体规则 / ambience）→ V3-3 下界 mob → V3-5 建筑内容族 → V3-6
  世界边缘雾 → V3-9 demo 回放。
- **11-05（V3-1 Phase B 完成）**：**v3.4.0 已发布**（门 194 全
  PASS：结构免疫第 75 次最强形【终档恰增白名单 4 键】+ S2 双跳
  headless arm（传送/返程映射验算 + 落点精确 + nether 段持久化）+
  S6 6/6 + 性能 A/B 同档【下界稳态 ≤ OW，熔岩海零 churn】；
  2582/2582；format 29→30；PITS 387；详见
  `docs/archive/phase-3.4.0.md`）。下一项 = **Phase C 下界内容族
  （v3.5.x 系列，与 V3-2 合并）**：群系族 / netherrack 族 / 矿脉 /
  流体规则 / ambience / 视觉打磨。
- **11-05（V3-8 完成）**：**V3-8 手柄支持 = v3.3.0 已发布**（门 193
  全 PASS：结构免疫第 74 次免重锚【无物理手柄 = 零事件】+ S3 2555/2555
  + 性能 A/B 同档【终档 savenorm EXACT 零 format】；源位掩码 OR 语义 +
  v1 Xbox 映射 + 无手柄位级恒等；门强度记档 = 实机验收归用户；详见
  `docs/archive/phase-3.3.0.md`）。下一项 = **V3-1 Phase B 下界世界 +
  真传送（v3.4.0 候选，format 30）**。
- **11-05（V3-9 demo 回放入表 + 回放交互模型定案）**：用户问答定案
  （回放 demo = 编写好的 + 自己玩的过程两入口），入 UX 行 ② 续编号 =
  **V3-9**，执行 = 矩阵尾部默认（用户可一句话提前）。定案要点：① 双
  入口共享一个注入内核（与 E2E 输入跑走同款输入路径 = 项目确定性一
  级要求的直接消费，游戏逻辑零触碰）② demo 文件 = {版本指纹 TM_
  VERSION【不匹配点名拒播】+ 世界起点【fresh 种子 = 世界本体零存储 /
  存档槽引用 = 展示老基地】+ 帧 dt 序列 + 输入事件流【只记变化】}
  gzip（V3-7 确定性容器先例）→ `data/demos/`（与 saves 平级，**零
  format bump**）；体积 ~0.2-0.5 MB/小时游玩，录制 O(1)/帧零帧率影
  响 ③ **回放 = 只观察**：允许转镜头 / 切换视角（一/三/前视）/
  0.5×·1×·2× 变速 / 暂停（ESC = 暂停菜单非退出）；禁止一切改变世界
  的操作（挖掘/放置/攻击/物品使用/背包/移动键/交互）④ 架构边界 =
  **输入路由**（非黑名单）：游戏输入通道 100% 由 demo 文件供给，观众
  输入只进「相机 + 播放控制」层 = 结构性零遗漏；用户指出的「转镜头后
  前进方向与录制不一致」分歧问题由此消解——录制玩家移动恒用**录制朝
  向**（demo 驱动），观众转镜头 = 渲染相机偏移（过肩观察），模拟与
  渲染分道 ⑤ 变速正确性 = demo dt 序列是唯一时间源（real-dt 系统消费
  录制 dt 原值；掉帧尖峰原样记录、注入后物理钳制自然生效；0.5× = 帧
  保持不插值）⑥ v1 偏离记档：无 4×（渲染可持续上限）/ 无即时跳转
  （帧锁定态 = 帧号纯函数 + dt 前缀和可寻址，留后）/ 手柄通道 =
  V3-8 先行落地则立项时加（输入捕获收口点同款）。详见矩阵 UX 行条目。
- **11-05（V3-1 Phase A 完成 + V3-8 入表）**：① **V3-1 Phase A 门框
  = v3.2.0 已发布**（门 192 全 PASS：结构免疫第 73 次免重锚 + S2 六臂
  【含 GUI 互斥决定性六跑 d6≡d3 RAW】+ S6 4/4 + 性能 A/B 同档【终档
  savenorm EXACT 零 format 最强形】；2536/2536；PITS 386；详见
  `docs/archive/phase-3.2.0.md`）② **用户实机诉求 V3-8 手柄支持入
  UX 行**（11-05 定案：SDL3 GameController API，输入边界层零世界/渲染
  /存档触碰，键鼠优先双连纪律；排期 = v3.2.0 收口后紧接执行 =
  **v3.3.0**，其后 V3-1 Phase B）。
- **11-04（V3-7 完成）**：**V3-7 存档 gzip 压缩 = v3.1.0 已发布**
  （门 191 全 PASS：结构免疫第 72 次免重锚 + S2 四臂【遗留零迁移
  最强形式 = savenorm 恰 root.format 单键】+ S6 3/3 + 性能 A/B；
  2503/2503；format 28→29【schema 零变更 = gzip 时代标记】；PITS
  385；详见 `docs/archive/phase-3.1.0.md`）。下一项 = **V3-1 下界
  维度拆分立项**（门框 → 下界世界 → 内容三段，立项调研在
  `/tmp/opencode/v31_recon.md`）。
- **11-04（同日二次修订）用户重编号定案：原 4.x → 3.x**（2.x 已吸收
  原 3.x「系统扩展」八项，「世界扩展」期回归自然序号）：任务号
  V4-1~V4-7 → V3-1~V3-7，版本 v4.0.0 → v3.0.0；**v3.0.0 = 3.x 开启
  版已发布**（tag + GitHub release：重编号 + README「版本与发布」精
  简为四里程碑，纯文档 + banner bump，零玩法/零 format 变更）；首
  功能版 = v3.1.0（V3-7 存档 gzip，用户开新会话后执行）。
- **11-04（二批收官日）**：① 晋升执行权威（`docs/BACKLOG_4x.md` 方向
  底稿 → 本文件，同款先例；2.x 完成表沉 `docs/archive/v2x/
  BACKLOG.md`）② 逐项用户定案：6 项全保留，V3-1 拆分立项（门框 →
  下界世界 → 内容三段）③ **用户实机诉求提前项 V3-7 存档 gzip 压缩
  入表**（data/saves 体积大；定案 = 默认开【整档 gzip(JSON) + 魔数
  `1f 8b` 探测零迁移 + 确定性 gzip + `save.compression` 严格读 +
  format 29】；实测 5.4×【4 万块 journal 1.74 MB → 0.32 MB】；
  先执，用户开新会话后执行）④ release 政策定案 = 逐版 tag + GitHub
  release。

## 表外

- 继承 1.x / 2.x（第一批 + 第二批）表外（各 BACKLOG 表外节 + archive）。
- 下界之后的大方向（末地 / 创造模式 / 多人）= 11-06 用户定案已编号入
  矩阵（V3-10 / V3-11 / V3-12，范围立项时定案）。
