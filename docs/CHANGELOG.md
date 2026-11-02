# 版本历史（CHANGELOG）

> 本文件只留 **v2.x** 逐版明细（v2.0.0 = 1.x 收官里程碑 / V2-1 前视角 /
> V2-2 Steve 皮肤）。v1.x（v1.50.0→v1.0）→ `docs/archive/v1x/CHANGELOG.md`；
> v0.x（v0.12.0→v0.1.0）→ `docs/archive/v0x/CHANGELOG.md`。逐版细节
> （机制/证据/偏离/性能 A/B）见 `docs/archive/v1x/phase-*.md` 阶段存档与
> GitHub Releases。README 只保留里程碑版本（v0.1.0 首个公开版 / v1.0 大版本）。

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
