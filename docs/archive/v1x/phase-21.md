# Phase 21 细节存档

> Phase 30 文档整理：由 `SESSION_CONTEXT.md`「Phase 21 已完成（细节存档）」节、`KNOWN_ISSUES.md`「已修复（归档）」条目 合并而成（未改写原文）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/...` 驱动脚本）指向当时状态；当前文档布局
> 见 `README.md`「文档地图」。

## 阶段报告（原 KNOWN_ISSUES「已修复（归档）」）

- **`[2026-10-05, Phase 21]` Projectile 无游戏内发射器、不渲染（P10）/ --save 运行遇存活瞬态实体保存失败（P13 守卫 × P20 遗留）** — Phase 21 实现弓 + Projectile 游戏内发射器 + 箭渲染：**数据** `shoot_speed`（`(0,100]` 缺席=非发射器；显式 0/≤0/>100/非数拒绝）+ `shoot_damage`（`[0,100]` 默认 0）物品字段（`ItemRegistry::parseShoot`，未知字段白名单同步）+ bow 物品（material，max_stack 1，图标 `tools/generate_item_icons.py` 弓形：棕 C 弧 + 浅色弦）+ shaped 配方 `bow`（` S `/`S P`/` S `，3 stick + 1 planks，C 形开向右）+ "shoot" 音效（`tools/generate_sounds.py` 下行 whoosh 0.12s）→ 11 WAV。**代码** `arrow_mesh`（`makeArrowGeometry`：共享六面表各向异性缩放 0.55 长 ×0.05 粗、本地 −Z = 飞行轴、满天空光烘焙、CCW 绕序有测；`arrowOrientation` 纯函数：yaw = atan2(−vx,−vz)、pitch = atan2(vy,|水平|)，水平分量近零（垂直飞行）时 yaw 钉 0——IEEE 符号零 atan2(−0,−0)=−π 坑）/`Projectile` +`damage()`（构造参数，发射时从物品数据带入）+`kill()`（非方块来源消耗，同 shouldDespawn 移除路径）/Game：`selectedShootSpeed/Damage`（0=非发射器）、`updateBowShooting`（按住每 `kSwingInterval` 0.25s 一支，松键复位）、`shootArrow`（眼前 `kArrowSpawnOffset`=0.3 格沿 `camera.forward`，播放 "shoot"）；`handleInteraction` 左键分支：选中发射器 → 射箭且**不**挖掘/挥击（Minecraft 弓语义）；`updateEntities`：箭走**固定帧步长** `kProjectileStepPerFrame=1/60`（运动实体可复现约定，同 `kMobStepPerFrame`）+ 每帧**扫掠命中**（步前位置捕获，`raycastAabb(前位, 本步位移, mob AABB)` t∈[0,1] 命中——首命中 mob（实体索引序）受 `arrow->damage()` + 沿箭水平速度近战式击退（`kKnockbackSpeed`/`kKnockbackLift`），箭 `kill()`；穿过玩家/其他实体、不伤玩家）；`drawArrows`（opaque pass 在 mob 后；共享单 mesh 懒构建；模型 = `translate(盒中心) × rotateY(yaw) × rotateX(pitch)`（相机同款约定，−Z 面 = 飞行方向）；**纯色路径** `u_useTexture=0` + `u_color`（基本着色器既有分支，无 atlas 箭纹理）——**GL 陷阱**：`Renderer::draw` 每次绘制后 `program.unbind()`，opaque pass 末次绘制后程序处于解绑态，直接 setUniform = `GL_INVALID_OPERATION`(0x502) 每帧报错（无箭时也触发，因 uniform 切换无条件执行）；修复 = `drawArrows` 内 bind → 切 uniform → 绘制 → 再 bind → 恢复 `u_useTexture=1` → unbind（uniform 驻留程序对象，下 pass 每次绘制自行 bind））/**存档守卫修正**：`writeSave` 的"只存 ItemEntity、其他一律失败"守卫（P13 原文"游戏只生成 ItemEntity"在 P19/P20 起已不成立）改为跳过已知瞬态种类（`MobEntity`/`Projectile`，按设计不入档）、其他未知种类仍 `MC_FAIL`——修复 P20 遗留：`--save` 运行退出时若有存活 zombie 保存失败（P20 E2E 均不带 --save 故从未触发）。**测试** 600/600（+12：ItemRegistry shoot 字段 解析/默认 0/`shoot_speed` 0 与 −1 与 101 与非数拒绝/`shoot_damage` −1 与 101 与非数拒绝；ArrowGeometryTest ×4 计数/边界含极值达界/CCW 绕序（轴对齐法线恰为单位轴且背离中心）/满天空光；ArrowOrientationTest ×3 六方向 −Z 对齐（`Mat4::transformDirection` 断言，含 +Z 反向 yaw=π 与未归一化方向）/垂直飞行 yaw 钉 0 pitch=π/2；ProjectileTest `kill` 消耗 + 伤害携带）。**E2E**（`/tmp/opencode/p21_e2e.zsh`，首用 xact `B`/`U` 按住/松开按钮命令）：base 60f ×2 与 base18 逐字节一致（默认热栏无弓，发射路径惰性）；**bow 运行**（手写 format-3 存档 `bowtest`：seed 123456789、热栏 0 = bow、玩家出生位/朝向——绕开"工作台合成弓"的长输入序列；存档在 `data/saves/`，git 忽略，驱动每次运行前重拷 pristine 主档，因干净退出会写回）90f：锁指针 → `R 0 −280`（pitch −0.25→+0.45 朝天空）→ 按住左键 0.62s（GAP 113ms 校准，3 发 @~44/59/74 帧）→ 松键 → 帧 89 截图：×3 运行**逐字节一致**（运动实体确定性）；**ctrl 对照**（同存档同相机姿态、不按住）×3 逐字节一致；bow vs ctrl = **616 px 差**，bbox x[632,647] y[338,489]（屏幕中央竖直带 = 沿 −Z 直飞的 2 支飞行箭；第 3 支在截图前 ~1 帧撞地消亡），93% 差像素为箭棕色（R>G>B ∧ R<140 ∧ R−B≥15），UI 带/心带零差。**驱动/环境**：:0 平铺 WM 规则继承（resizable:false 锁 1280×720，输入运行不碰窗口）；xact 增 `B <btn>`（仅按下）/`U <btn>`（仅松开）（链接需 `-lXtst`）。

## 会话存档（原 SESSION_CONTEXT）

- **目标**：候选 1 —— Projectile 游戏内发射器 + 箭渲染（弓）。用户"开始 phase 21"
  即确认此候选。
- **设计决策**：
  - **数据**：物品可选字段 `shoot_speed`（`(0,100]`，缺席=非发射器；**显式 0 拒绝**
    ——缺席才是默认，显式 0 = 笔误）+ `shoot_damage`（`[0,100]` 默认 0）。
    `shoot_speed > 0` 即发射器（不引入新 ItemType，material 即可）。bow =
    material 物品（max_stack 1、图标）+ shaped 配方 ` S ` / `S P` / ` S `
    （3 stick + 1 planks，C 形开向右，planks 当弦——v1 无 string 物品，避免
    不可获取的死内容）。
  - **发射语义**：选中发射器时**左键 = 射箭**（按住每 `kSwingInterval` 0.25s 一支，
    松键复位 `m_bowCooldown`），**不再**挖掘/挥击（Minecraft 弓语义；弓近战 1 点
    伤害未实现，记 KNOWN_ISSUES）。箭从 `camera.position + forward×0.3`
    （`kArrowSpawnOffset`）沿 `camera.forward`（单位）以 `shoot_speed`（bow = 15 =
    P10 `kLaunchSpeed` MC 满威力）发射，播放 "shoot"。
  - **箭 = P10 `Projectile`**：构造 +`damage` 参数（`damage()` 访问器）；新增
    `kill()`（非方块来源消耗——mob 命中，同 `shouldDespawn` 移除路径，无插墙语义；
    **撞 solid 仍即亡** = P10 已测行为，插墙残箭是 P22 候选，不偷改已测语义）。
  - **命中**：Game 每帧（updateEntities 内，实体更新后）：步前捕获每支箭位置
    （`std::vector<std::pair<Projectile*, Vec3>>`）→ 更新循环加第三分支（箭走
    `kProjectileStepPerFrame` 1/60 固定步长）→ 扫掠测试 `raycastAabb(前位, 本步位移,
    mob->aabbMin(), mob->aabbMax())` t∈[0,1]（t=0 = 起点已在 mob 内也算）；
    首命中（实体索引序）`mob->damage(arrow->damage())` + 沿箭水平速度归一 ×
    `kKnockbackSpeed` + 抬升 `kKnockbackLift` 击退 + "hurt" 音，箭 `kill()`。
    穿过玩家/其他实体（`collidesWithEntities=false`），不伤玩家（单人生存 v1）。
  - **渲染**：`arrow_mesh` 新模块 —— `makeArrowGeometry()`（共享 `kCubeFaces` 六面表
    各向异性缩放：x/y ×0.05、z ×0.55（`kArrowLength`/`kArrowThickness`），中心在原点，
    本地 −Z = 飞行轴，满天空光烘焙，UV (0,0) 不用）；`arrowOrientation(v)` 纯函数
    → (yaw, pitch)：yaw = atan2(−vx,−vz)、pitch = atan2(vy, √(vx²+vz²))；
    **|水平| < 1e-6（垂直飞行）时 yaw 钉 0**（atan2(−0,−0) = −π 的 IEEE 符号零坑，
    否则箭会绕竖直轴无意义转 π）。模型 = `translate(碰撞盒中心) × rotateY(yaw) ×
    rotateX(pitch)`（相机同款约定：−Z 面 = 飞行方向；推导与单测用
    `Mat4::transformDirection((0,0,−1)) ≈ v̂` 断言）。Game 单 mesh 懒构建
    （`m_arrowMesh`，`arrowMesh()`）；`drawArrows` 在 `drawMobs` 后（opaque pass）。
    **纯色渲染**：基本着色器既有 `u_useTexture=0` + `u_color`（vec3）分支（无 atlas
    箭纹理、无新工具产物）；`u_color` 用 `setUniform3f`（注意 UI 着色器的 u_color
    是 vec4，别混）。
  - **存档守卫修正（P13 × P20 遗留 bug）**：`writeSave` 原"只存 ItemEntity、其他
    `MC_FAIL`"守卫基于 P13 假设（"游戏只生成 ItemEntity"）——P19/P20 起不成立：
    `--save` 运行退出时若有存活 zombie（或现在：箭）→ 保存失败（ERROR 日志，退出码
    0；P20 E2E 均不带 --save 故从未触发）。改为：**跳过已知瞬态种类**
    （`MobEntity`/`Projectile`，按设计不入档）、**其他未知种类仍 `MC_FAIL`**
    （保留"新实体类型未接存档"的响亮失败）。
  - **v1 简化（KNOWN_ISSUES「弓 / 箭」节）**：无蓄力 / 无箭物品·弹药（无限箭）/
    箭不入存档 / 无插墙残箭 / 不伤玩家 / 无箭头·尾羽·纹理 / 选中弓时左键无近战。
- **GL 陷阱（本阶段最大坑）**：`Renderer::draw` 每次绘制**前后** bind/unbind 程序 →
  opaque pass 末次 chunk 绘制后程序处于**解绑态**；`drawArrows` 里直接
  `setUniform1i/3f` = `GL_INVALID_OPERATION` (0x502) 每帧报错——**无箭也触发**
  （uniform 切换在实体循环外无条件执行）。修复 = `drawArrows` 内
  `bind → 切 u_useTexture=0/u_color → 绘制循环（每次 draw 自行 bind/unbind）→ 再
  bind → 恢复 u_useTexture=1 → unbind`（uniform 驻留程序对象，下 pass 不受影响）。
  排查教训：bisect 时 sed 注释了调用点，后续"函数内二分"全在注释态下跑 → 得出
  错误结论；**每个 bisect 步必须确认目标代码真的在跑**（本阶段因此多花了一轮）。
- **物品解析 bug（本阶段第二坑）**：`parseShoot` 首版把 `shoot_speed` 范围校验写在
  `if (def.find(...))` **块外** → 缺席默认 0 被 `<= 0` 拒 → 所有无 shoot 字段的
  物品注册全失败（102 测崩：fixture 临时物品/配方全链路）。教训：**可选字段 + 默认
  值 0 时，范围校验必须在"字段存在"分支内**（`parseAttackDamage` 默认 1.0 所以
  同模式无碍）。
- **测试** 600/600（+12）：
  - test_item.cpp +4：ParsesShootFields（15/5）、ShootFieldsDefaultToZero、
    RejectsBadShootSpeed（0 显式/−1/101/非数）、RejectsBadShootDamage（−1/101/非数）。
  - test_arrow.cpp（新，7）：ArrowGeometryTest 六面 24 顶点 36 索引 / 盒边界（含
    极值达界）/ CCW 绕序（轴对齐面法线恰为单位轴 + 背离中心）/ 满天空光；
    ArrowOrientationTest 六方向 −Z 对齐（含 +Z 反向 yaw=π、陡峭下行）/ 未归一化方向 /
    垂直飞行 yaw 钉 0 + pitch=π/2。
  - test_entity.cpp +1：ProjectileTest KillConsumesItForDespawn（kill → dead +
    shouldDespawn；damage 携带）。
- **E2E**（`/tmp/opencode/p21_e2e.zsh`；xact 新增 `B <btn>` 仅按下 / `U <btn>` 仅松开
  ——按住支持；**链接要 `-lXtst`** 不是 `-lXext`）：
  - base 60f ×2 与 `/tmp/opencode/base18.png` 逐字节一致（默认热栏 slot 0 = stone，
    弓不在默认热栏 → 发射路径惰性；新物品/配方/图标/音效不触 base 渲染）。
  - **bow 运行**：手写 format-3 存档（`/tmp/opencode/bowtest.json` 主档，驱动每次
    运行前 `cp` 到 `data/saves/bowtest.json` —— 干净退出会写回，必须重拷 pristine；
    seed 123456789、time 0.25、玩家 [0.5, 54.0, 0.5] yaw 0 pitch −0.25 hp 20、
    热栏 0 = bow×1 + stone×64、inv/table grid 9 空、entities/blocks/fluids 空）。
    选手写存档而非"工作台合成弓"：后者是放置桌子→开面板→放材料→取产物→Tab 的长
    输入序列，脆弱且慢；手写存档同时验证了严格存档加载路径。90f 序列：M 窗口中心
    → `C 1`（锁）→ `R 0 −280`（pitch −0.25 → +0.45 朝天空——高对比 + 箭滞空久：
    pitch +0.45 时箭滞空 ~0.85s）→ `S $GAP`（**GAP=113ms**，校准自 P19 时间标记：
    xact 起点 ≈ scene build + 0.371s）→ `B 1` 按住 → `S 620` → `U 1`。3 发落点
    ≈ 帧 44/59/74（按住 37 帧 = 3 发，第 4 发 45 帧超窗）；截图帧 89 时 2 支飞行
    （第 3 发 0.75s 贴近 0.85s 滞空极限，截图前 ~1 帧撞地消亡——稳定 2 支）。
  - **ctrl 对照**：同存档同 `C 1` + `R 0 −280`（**相机姿态必须相同**，否则 diff =
    地形位移而非箭）、不按住。
  - 结果：bow ×3 **逐字节一致**（运动实体固定步长确定性成立）、ctrl ×3 逐字节一致；
    bow vs ctrl = **616 px 差**，bbox x[632,647] y[338,489]（屏幕中央竖直带——沿 −Z
    直飞的箭 x 恒 640；2 簇：近箭 35px 长 532px + 远箭 20px 84px），93% 差像素箭棕
    （R>G>B ∧ R<140 ∧ R−B≥15；其余 7% = 抗锯齿混合边），UI 带（y≥660 或 x≥1170）
    与心带零差。
- **变更文件**：`src/item/item.{hpp}`（+shootSpeed/shootDamage）、
  `src/item/item_registry.{hpp,cpp}`（parseShoot + 调用点 + 未知字段白名单）、
  `src/entity/arrow_mesh.{hpp,cpp}`（新）、`src/entity/projectile.{hpp,cpp}`
  （构造 damage + damage()/kill() + 头注释）、`src/core/game.{hpp,cpp}`（常量
  kProjectileStepPerFrame/kArrowSpawnOffset、左键发射分支、selectedShoot*、
  updateBowShooting/shootArrow、updateEntities 箭分支 + 扫掠命中、drawArrows/
  arrowMesh、存档守卫跳过瞬态种类）、`CMakeLists.txt`（arrow_mesh.cpp +
  test_arrow.cpp）、`assets/data/items/bow.json`（新）、`assets/data/recipes/bow.json`
  （新）、`assets/textures/items/bow.png`（新，工具生成）、`assets/sounds/shoot.wav`
  （新，工具生成）、`tools/generate_item_icons.py`（+bow 图标）、
  `tools/generate_sounds.py`（+shoot 音效 → 11 WAV）、`tests/test_arrow.cpp`（新）、
  `tests/test_item.cpp`（+4）、`tests/test_entity.cpp`（+1）、README（可玩内容 P21
  节 / 物品数 23 / 物品 JSON 文档 +2 字段 / 存档瞬态说明 / WAV 数 11 / roadmap）、
  KNOWN_ISSUES（P10 发射器行移入归档 + 新「弓 / 箭」节 7 条 + 存档 format 1–3 行
  修正 + P21 归档条）、SESSION_CONTEXT（本节）。config/config.json（用户手改
  view_distance 80 / day_length 600）不入档、不回退。
- **提交**：`1583499`（代码 + 数据 + 文档，显式路径列表；config/config.json 不入档）。
