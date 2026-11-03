# 版本历史（CHANGELOG）

> 本文件只留 **v2.x** 逐版明细（v2.0.0 = 1.x 收官里程碑 / V2-1 前视角 /
> V2-2 Steve 皮肤 / V2-3 教程系列化 / V2-4 天气 / V2-5 猪）。
> v1.x（v1.50.0→v1.0）→ `docs/archive/v1x/CHANGELOG.md`；
> v0.x（v0.12.0→v0.1.0）→ `docs/archive/v0x/CHANGELOG.md`。逐版细节
> （机制/证据/偏离/性能 A/B）见 `docs/archive/v1x/phase-*.md` 阶段存档与
> GitHub Releases。README 只保留里程碑版本（v0.1.0 首个公开版 / v1.0 大版本）。

## v2.5.0（2026-11-03）— V2-5 新 mob 猪（门 176 全 PASS）

「加 mob = JSON + 纹理」数据驱动承诺的验证 = **零核心代码**：
**pig.json**（passive 10 HP / 0.6 speed / spawn_max_light 15 / drops
pork ×1-2 + leather ×0-1 随机区间既有机制）+ **宽体 6-box 模型**
（body + 4 粗短腿 + head 0.45³ 嵌入 body 前方 cow 范式；4 腿 0.4 rad
对角 swing；64 相位 clip 验收）+ **head `faces.front` = pig_face**
（**V2-2 per-face 纹理的首个 mob 消费**——此前仅玩家身体用过；显式
8×8 猪脸粉/深粉鼻口/2×2 眼 Steve 范式零 hash）+ **pig_body 纹理**
（MC 猪粉 (228,170,160)+noise，新 salt 0x5011；**既有 6 mob tile 176
文件全 cmp 零变化**）+ **pork/cooked_pork 食物链**（+3/1.8 sat →
smelt → +8/12.8 sat = beef 熟同款刻度；icon 生成器新 salt；无新配方
杀猪→熔炼 MC 同款）+ **生成权重** {"id":"pig","weight":1}（6→7 加权
抽取零代码）+ **图集 97→99**（pig_body [8,7]/pig_face [9,7] 进 row-7
空位，97 既有条目**零移位**，99 条全 census 逐格钉）+ **字体
422→423**（猪 U+732A 码点序，顶 512 行逐字节不变免重锚，424 预算剩
1）。2135/2135（+7 新 + 10 既有扩展）、-Werror 0、banner v2.5.0。
门 176 全 PASS：S1 **第 176 次结构性免疫**（60f 六跑 n1≡o1≡o2 跨版
RAW + n1≡m1【mobs-on ≡ off = first_spawn_frame 60 零 spawn 扫描
render-inert】+ m1≡m2 + 锚 179116 B + savenorm EXACT 零 format）+
S2 猪判别（a 生成序列 ×2 恒等 118 行【pig f60+f300 (0.5,-5.5) 外环
优先复现】+ 旧侧零 pig 互斥；b 杀猪链全存档权威：离线 f210 截图粉簇
反投影 + 走位自洽迭代解 KILL pose → 实跑零修正 3+ 命中 iron_axe →
**pork ×1 掉落区间钉** + leather ×0 → 两段接力【mobs-off save-posed
传送到掉落格】拾取/开炉/喂料/熔炼/取排/吃【selected 预置免数字键】
→ hunger 14→20）+ S3 2135 + S6 点名 2/2（unknown mob 'dragon' /
pig_face 图集缺条目）+ **性能 A/B 同世界同成本**（changesApplied
9/3/1 三窗逐位 + 终档 18/18 55/55 集合恒等 + mobs-on 猪自成本 ents
0.04-0.10 ms ≈ 既有 mob 同档无回归）。门内环境事故 3 条记 PITS
368-370（XTEST 键盘服务器级失效 + drawin 窗盖中央 + mob 不持久化）。
详见 docs/archive/phase-2.5.0.md。

## v2.4.0（2026-11-03）— V2-4 天气（雨/雷）（门 175 全 PASS）

帧锁定天气系统（设计定案 11-03 夜间自主推进）：**状态机 = core/weather 纯函数**（campfire 范式，splitmix64 推进式流 PITS 4——主流 seed = 世界 seed【同世界同序列】，雷 = 独立 salt 流 0x7E42B1D9；CLEAR [9000,18000)f / RAIN [3000,6000)f / 雷 [600,1800)f 命名常量；**首段恒 CLEAR** = 无输入 60f 基线永不进雨的结构免疫根）+ **呈现 = rain shader program**（cloud 第四先例：静态 VBO 300 滴出生偏移+相位【0xD93A5E17 专用 salt 流】init 一次上传 + 顶点 shader 逐滴纯函数位置【有界 fallOffset + 玩家 16³ 盒 boxOrigin，per-frame 零上传】+ 0.02×0.5 细竖条 alpha 0.35 白蓝 (200,210,235) 无纹理）+ **天空 dayFactor 消费点单收口**（RAIN ×0.85 + 雷 5 帧线性衰减 +0.15 脉冲——u_dayFactor/clear/雾同源，shader 表达式树零改动）+ **交互四件全帧锁定**（雨灭火块/灭营火 = 新纯谓词 extinguishedByWaterOrRain【6-邻水 ∨ raining，Phase 64/75 路径收口】/ 雨灭燃烧效果【玩家+mob inWater ∨ raining，re-ignite 先行保水胜火序，无屋檐检查 = 记档偏离】/ 雨润耕地【soilIsWet/cropIsWet/cropGrowthSteps 扩参默认 false = 旧语义位级不变，mesher 湿润视觉不传 = 视觉零变，雨中生长 2× = 111/200→200/200 钉】）+ **render.weather strict read 默认 true**（off = 状态机不跑+零雨滴+零交互 = v2.3.0 位级恒等）+ **--weather clear|rain CLI**（仅覆盖初始 state——rain = kRainMin 3000f 固定可复现，主流零消费其后走流；词校验点名）+ F3 `weather clear|rain` 行 + **雷新 wav**（generate_sounds 固定种子 0x70DE 60→40Hz 滚雷 3.5s，既有 39 wav md5 零变化+幂等实证；2D 中心不空间化；**无雨声循环** = 记档）+ **零 format**（25 不动，天气 = session 派生云漂移先例类）。2128/2128（+19：weather 17【设计 §6 八组 + 脉冲/wav 资产】+ F3 WeatherLine + mob ExtinguishedInRain）、-Werror 0、banner v2.4.0。门 175 全 PASS：S1 **三方恒等最强形**（60f 六跑 new-on ≡ old v2.3.0 ≡ new-off ≡ base18 锚逐字节 + savenorm EXACT 零 format）+ S2 雨判别全 PASS（rain ×2 恒等 / 雨可见 / 120f savenorm 精确 churn 判别【rain 侧恒无火 + 非世界键 EXACT】/ 营火+火块互斥终态 / 雷+停雨 4 事件 ×2 间隔 ±0.08s 帧锁定 + 雨窗 50.5 s ≈ kRainMin）+ S3 2128 + S6 点名 2/2 + **性能 A/B 同世界同成本**（changesApplied 9/3/1 三窗逐位 + 终档 18/18 55/55 集合恒等 + 全指标同档；rain 自成本 = 300 滴静态 VBO 在噪声带内）。详见 docs/archive/phase-2.4.0.md。

## v2.3.0（2026-11-03）— V2-3 教程系列化（农耕 + 厨房）（门 174 全 PASS）

用户 11-02 实机反馈「之前的新手指导比较单调，有没有多个任务系列可以添加」：**A 机制系列化**（`tutorials/` 多文件按名序 = 系列序 + `AssetManager::listDir` + 步 id 跨系列全局唯一 init 点名；`TutorialState` +seriesIndex 纯函数推广；**存档 format 24→25** `player.tutorial` 加 `series`——三向键纪律【25 缺 series 拒 / 23-24 携 = 版本谎报拒 / ≤24 载入缺省 0 = 值保真免迁】+ series 越界/id 不在该系列 = 重置上报不拒档；事件词表 +**pickup./smelt.** 两前缀均帧锁定【拾取收口 + 炉子 smelt-complete 消费点】；**T = 跳当前系列**；系列过渡无 toast【目标行换步即提示定案】；F3 行 `<s>/<total> <id> p/c`）+ **B 两个新系列**：② 农耕 7 步（2 种子→2 耕地→2 种→2 收 = 4 小麦→面包→吃，全链数据核实在位零新机制；**×2 修订** = wheat_7 掉 2 < bread 配方 3 的经济账死局修复）+ ③ 厨房 6 步（3 锻石→炉子→放→开→熔炼熟鸡→吃）；**beginner.json 字节零改动**；字体 401→422（+株 等，顶 512 行逐字节不变免重锚）。**门 174 内抓到并修复 UX-7 潜伏 bug**：E 面板 recipe 行左键合成成功漏发 tutorial craft 事件（玩家点配方行合成 = 永久卡教程；1.x 链从未经该路径 = 潜伏两版，PITS 360）。2109/2109（+25）、-Werror 0、banner v2.3.0。门 174：S1 **第 174 次结构性免疫免重锚**（四方 ×2 RAW 全字节一致 + 锚 179116 B 连续 + 跨版 savenorm 恰 2 项【format + series:0 值保真】）+ S2 输入链全 PASS（A 农耕 9 段【FAR 先放 = 累积态遮挡定案 + 生长轮询 + 走捡 + 逐 checkpoint 断言 ×8】/ B 厨房 2 段【开炉重捕获重试 + 燃料点火即耗物品判别】/ C T 三态 + 日志逐字 / D format 24 迁移值保真）+ S3 2109 + S6 精确点名 4/4 + 性能 A/B 同世界同成本【changesApplied 9/3/1 三窗逐位 + 终档 fluids 18/18 + blocks 55/55 集合恒等 + 全指标同档】。PITS 359→367（8 条：360 recipe 行漏 emit / 361 format 边界字面量 / 362 教程经济账 / 363 锁定态禁预 move / 364 面板 uiSize 空间 / 365 漂移须重捕获 / 366 燃料点火即耗 / 367 饥饿满 no-op）——详见 docs/archive/phase-2.3.0.md。

## v2.2.0（2026-11-02）— V2-2 玩家皮肤 Steve 化（门 173 全 PASS）

用户实机反馈「看到正脸了，没有眼睛，没有头发，皮肤也丑，请仿照史蒂夫更新」：根因 = 每盒六面全贴同一 tile + head tile 是四侧脸 2×2 拼格 → 五官亚像素。两部分修复：**A 模型 per-face 纹理**（MobBox 可选 `faces` 六面覆写，严格解析 + init 存在性校验点名 box+面；**无 faces = 位级不变**——既有 6 mob 零改动 + 全族测试零语义改动 = 红线证明；角序/绕序零动，摆动 UV 随行零改动）+ **B Steve 风格皮肤**（生成器确定性显式像素：8×8 脸 = 棕发双色调 + 2×2 眼 ×2【鼻侧白 + 外侧蓝虹膜】+ 鼻/嘴阴影行；青蓝衫/蓝裤/棕靴；7 个 per-face 独立 16×16 派生 tile，头 8×8→16×16 2× 精确放大 = 五官像素级；图集 94→97 row-5 空位，**90 遗留条目零移位免重锚**，旧 4 拼格退役）。2084/2084（+5）、-Werror 0、banner v2.2.0。门 173：S1 **第 173 次结构性免疫免重锚**（四方 ×2 + 锚 179116 B + savenorm EXACT）+ S2 Steve 脸 A/B（三模式 ×2 + savenorm 两两 EXACT + 头带 0.6 阴影换算色分类：**旧前视零五官（2840 旧肤）/ 新前视眼白 306 + 虹膜蓝 324 + 发 1421 / 新后视全发 5280**）+ S6 ×2 精确点名 + 性能同世界同成本【首轮 render 偏置重跑证伪 + changesApplied 9/3/1 逐位 + 终档 18/18 55/55 diff 0】——详见 docs/archive/phase-2.2.0.md。

## v2.1.0（2026-11-02）— V2-1 第三人称前视角（2.x 首个功能项，门 172 全 PASS）

用户 11-02 实机诉求「第三视角下能看到自己的前面」= MC F5 前视角。**核心 = 前视是后视同款数学在翻转视角角下执行**（yaw+π/−pitch ⇒ −forward 翻转 = 玩家前方；up_cam 翻转不变 ⇒ v2.0.0 的 0.4 轨道抬升同款继承 = 前视头也在准星下方 29-96px；遮挡收敛/穿流体/embedded 自动对称）——`resolveThirdPersonCamera` 等纯函数零改动。C 键三循环 First→后视→前视→First + 新纯函数 `thirdPersonFrontAngles`；**瞄准 UX-8g 眼位射线 origin/方向零动**（前视方向 = 相机向 = 屏幕中心向自动翻转）+ 新 `aimReach()`（前视 = D+kReach = 玩家身后 5 块有效带 MC 前视惯例，First/后视位级不变）4 消费点 + drawHeldHand 前视隐藏（`!= First`，First/后视逐位等价）+ 身体前视自动渲染（既有门控）。2079/2079（+4：翻转角钉/up_cam 不变/空世界恒等 bit-exact+后视镜像/墙钳止镜像）、-Werror 0、banner v2.1.0。门 172：S1 **第 172 次结构性免疫免重锚**（四方 ×2 + 锚连续 179116 B + savenorm EXACT）+ S2 三模式矩阵（三模式 savenorm 两两 EXACT 不入档 + 皮肤判别 First 0 / 后视 10,151 / **前视 10,125 全 0.6 阴影调 = 脸朝向相机** + 世界朝向 B≠C 10,674 diff 互斥 + 六跑零漂移）+ S2b 前视挖矿互斥（前视破身后 4.5 块靶 vs 后视破面前墙，pitch 翻转场景坑首轮实证）+ 性能 A/B 同世界同成本【首轮 win2 churn 偏置重跑证伪 9/3/1 逐位 + 终档 18/18 55/55 diff 0】——详见 docs/archive/phase-2.1.0.md。

## v2.0.0（2026-11-02）— 1.x 收官里程碑发布（扫尾三件事，门 171 全 PASS）

1.x 任务表 40/40 + UX-8g/UX-9 实机报障 4 项全完成后的里程碑发布（无新玩法），用户 11-02 定案三件扫尾：

1. **1.x 阶段文档归档**：`docs/archive/` 181 份 phase 文档 `git mv` → `docs/archive/v1x/`（保历史）+ 全仓 556 处引用零残留 + BACKLOG v1.x 表标记归档、顶部开 v2.x 任务表占位（待用户定范围）+ AGENTS「现状」节瘦身（1.x 巨段编年史 149k + 逐门记录抽出 → `docs/archive/v1x/HISTORY.md` 只读全史）+ 活文档刷新到 2.0 状态（MECHANICS 12+ 处 / KNOWN_ISSUES 8+12 处 / GAMEPLAY 8 处，全数字源码实证）+ 门协议工具入仓（`tools/gate_compare.py` / `perf_gate_config.json` / `xenv.sh` = PITS 317 重建纪律）
2. **第三人称准星不压头**（用户报障，B 方案 = MC 同款抬相机）：轨道点沿相机 up 轴抬高 `kThirdPersonOrbitHeight` 0.4 块（`Camera::up()` = cross(right, forward) 单一三角真源，头 top 降 29px / 底缘降 96px 低于屏幕中心 = 准星零重叠，任意俯仰角恒定）；瞄准射线两模式仍自眼位（UX-8g 红线零动）= 固定 TP 视差 MC 同款记偏离；First 分支字节不变
3. **发布**：tag v2.0.0 + GitHub release（2.x 迭代起点）

2075/2075（扫尾② +3）、`-Werror` 0 警告、banner v2.0.0。门 171：S1 **第 171 次结构性免疫 + 默认 First 位级恒等 + base18 锚连续 179116 B 免重锚**（四方 ×2 全字节一致 + 跨版 savenorm EXACT）+ S2 TP 准星 A/B 判别 PASS（OLD 中央带 160 皮肤 px = 压头实证 / NEW 中央带 0 + 下移带 720 = 头降 29-96px 解析吻合 + 中心像素 OLD 面部 vs NEW 草逐像素 + 遮挡回归 2390 皮肤 = 相机出墙保持 + 跨侧 savenorm EXACT 同世界最强形）+ S3 2075 + 性能 A/B v1.50.0 vs v2.0.0 同世界同成本（changesApplied 9/3/1 逐位 + 终档 18/18 55/55 diff 0）——详见 docs/archive/phase-2.0.0.md。
