# Phase 20 细节存档

> Phase 30 文档整理：由 `SESSION_CONTEXT.md`「Phase 20 已完成（细节存档）」节、`KNOWN_ISSUES.md`「已修复（归档）」条目 合并而成（未改写原文）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/...` 驱动脚本）指向当时状态；当前文档布局
> 见 `README.md`「文档地图」。

## 阶段报告（原 KNOWN_ISSUES「已修复（归档）」）

- **`[2026-10-05, Phase 20]` 无 mob / 无 AI / 无玩家近战（P10 实体层之上的空白）** — Phase 20 实现数据驱动 mob 系统（v1 数据仅 zombie）：**数据** `data/mobs/<id>.json`（properties：health/speed/detect_radius/attack_range/attack_damage/attack_cooldown/wander_interval/drops(+count)；model：width/height AABB + box 列表，每箱 pos/size/一 atlas tile，box 须在 AABB 内；严格校验 + init 交叉校验：纹理在 atlas、drops 在 ItemRegistry、spawning 列表已注册）+ `data/mob_spawning.json`（check_every_frames/first_spawn_frame/max_mobs/min·max_radius/despawn_distance/mobs 名单）+ zombie 掉 `rotten_flesh`（新 material 物品 + 图标）+ 工具 `attack_damage`（木镐 2/铁镐 3，数据字段默认 1.0）+ atlas 2 tile（`textures/mobs/` 并入，`tools/generate_mob_textures.py` 确定性绿皮肤/青上衣，atlas 19→21 tile，既有 tile 位置零移动）。**代码** `MobRegistry`（严格解析）/`MobSpawningConfig::parse` + `findMobSpawnCell`（纯函数：玩家列周围固定扫描序 dz 外 dx 内、欧氏环 [min,max]、首合法格 = 固体地板 + ceil(h−ε) 头顶空余（air 且无流体）+ 地板高度带 ±2 + 不压玩家 AABB + mob 水平间距 ≥1）/`makeMobModelGeometry`（共享六面表、单 tile 六面、满天空光烘焙、底心原点）/`MobEntity`（AI：Wander 种子随机决策[1/3 站定、余半速随机向]→Chase[全速、面向 atan2(−dx,−dz)]→Attack[停、冷却打击经 hit 回调→`Player::damage`，走 P16 红闪/音效/死亡轮询]；击退窗口 0.3s 抑制 AI + exp(−6h) 衰减；integrate 无地面摩擦——AI 每帧设精确数据速度；splitmix64 状态推进流（state += gamma））/`raycastAabb`（slab 法，世界射线模块）/Game 集成（生成节奏 `frameCount ≥ first && frameCount % every == 0`、类型选择与 per-mob 种子 = 世界种子 ⊕ 0x9E37…×(frame|spawnCounter)、死亡掉落 + 距离消散入 despawn pass[rebuild 后 push，扫描中 push 会丢]、`targetMob`[方块距离 vs AABB 命中取近者——方块遮挡]、`updateAttacking`[0.25s 挥击、物品 attack_damage、击退 4 水平 + 2.5 抬升]、`drawMobs`[opaque pass、translate×rotateY(yaw 约定)]、config `mobs.enabled` 默认 true）。**确定性关键修复**：mob 运动实体首版用真实 dt → E2E ×2 出现 18px 亚像素漂移（机器负载 dt 抖动积分到运动体末位）；改为**固定帧步长** `kMobStepPerFrame=1/60`（日循环/物品动画同款约定）后 ×2 逐字节一致。**测试** 新增 40：MobRegistryTest ×7（解析/缺 id/非正 health/空 boxes/box 越界/drops_count 越界/重复 id）、MobSpawningConfigTest ×4、MobSpawnCellTest ×8（扫描序首格/3 格墙挡头顶[单格只会成为新地板——测试设计要点]/流体/环边界/无地板 nullopt/间距/玩家重叠/地板高度带）、MobMeshTest ×4（计数/AABB 内/绕序 CCW/UV 在 tile 内）、MobAiTest ×7（追击精确速度+朝向[±π 用 cos/sin 判]/超距不追/攻击冷却[61 帧二次命中、±1 ulp 容差]/垂距不打/死亡玩家忽略/同种子确定性/击退抑制后恢复）、MobCombatTest ×3、MobRaycastTest ×4（entry t/miss/原点内=0/平行出 slab）、MobItemsTest ×3（attack_damage 解析/默认 1.0/非正拒绝）→ **579/579**（P19 539 + 40）。**E2E**（:0，`/tmp/opencode/p20_e2e.zsh`，无输入场景无需 xact/窗口操作）：base 60f ×2 与 `base18` 逐字节一致（first_spawn_frame=60 > 最后更新帧 59，无 mob）；mobs 240f ×2 逐字节一致；mobs_off 对照（`config_xep_nomobs.json` 仅加 `mobs.enabled=false`）×2 逐字节一致；mobs vs 对照 = 54245 px 差全在世界区 x[273,678] y[242,485]、8413 px 绿皮肤（G≥R+40∧G≥B+60∧G>120）、UI 区（y≥660 或 x≥1170）零差、心区零差（无伤害，4 只 zombie 自 4–6 块外以 0.8 b/s 追、240 帧内无人进入 1.6 攻击半径）、无红闪 tint。启动日志零 warning（atlas 21 tile 双向交叉校验过）。**顺带发现**：`particle.cpp` 的 `uniform01` 不推进 splitmix64 state（同 burst 粒子全同值）→ 记入「UI / 音效 / 粒子（Phase 15）」节（修复留后续）。新简化（不入档/无光生成/无动画/无 step-up/无游泳/固定帧步长等）记入「Mob（Phase 20）」节。

## 会话存档（原 SESSION_CONTEXT）

- **目标**：mob 与 AI（KNOWN_ISSUES 优先级首位；P19 通用实体间碰撞已铺好碰撞基础）。
  v1 数据仅 zombie：生成 + AI（wander/chase/attack）+ 玩家近战 + 死亡掉落 + box 模型
  渲染。全部数据驱动（新 mob = JSON + 纹理，不改代码）。
- **设计要点**：
  - 数据：`data/mobs/<id>.json`（严格校验：id 非空唯一 / 数值 >0 / drops_count 1..64 /
    box 须在 width×height AABB 内 / 缺纹理名=错；未知字段警告）+ `data/mob_spawning.json`
    （节奏/环/上限/消散距离/名单；min<max、despawn>max_radius 校验）+ init 交叉校验
    （spawning 名单在 MobRegistry、box 纹理在 atlas、drops 在 ItemRegistry 且 count ≤
    maxStack——同 block drops 纪律）。
  - AI（确定性）：种子 = 世界种子 ⊕ 0x9E3779B97F4A7C15 × spawnCounter（per-mob
    splitmix64 **状态推进流**：state += gamma 后 finalizer——注意 particle.cpp 的
    uniform01 是纯 finalizer 不推进 state，同 burst 同值缺陷，P20 发现已记录）。
    状态机：Wander（每 wander_interval 决策：r%3==0 站定，否则 53-bit uniform 角度
    半速走）→ Chase（distH ≤ detect_radius 且 |dy| ≤ 2：全速、yaw=atan2(−dx,−dz)）→
    Attack（distH ≤ attack_range 且 |dy| ≤ 1：停、面向、cooldown 到点经
    std::function hit 回调→Game lambda→Player::damage——红闪/音效/死亡全走 P16 既有
    m_prevHealth 轮询，mob 不直接碰 UI/音效）。玩家死亡（isDead）→ 不追不打。
  - 生成（纯函数 findMobSpawnCell）：玩家列 (px,pz) + 玩家地板 floorY（脚底列最高
    solid）为锚；扫描 dz=−R..R 外、dx=−R..R 内（R=ceil(max_radius)），欧氏环
    [min,max]；首合法格 = 固体地板（**带内最高** solid，y ∈ [floorY−2, floorY+2]，
    上限 kWorldHeight−airCells−1）+ ceil(height−1e-6) 头顶格 air 且无流体 + spawn AABB
    不压玩家 AABB（严格测试同碰撞约定）+ 与其他 mob 底心水平距 ≥1。
  - 攻击：左键按住 → targetMob()（targetBlock 距离 vs 每 mob raycastAabb slab 命中
    取近者——**方块遮挡**：方块更近=打方块；origin 在盒内=0）→ updateAttacking
    （0.25s 挥击 cooldown；伤害 = 选中物品 attack_damage 数据[徒手 1.0]；击退 = 瞄准
    水平归一 ×4 + y 2.5 → addKnockback：impulse + 0.3s AI 抑制窗口，窗口内 integrate
    水平 exp(−6h) 衰减、窗口后 AI 恢复）。瞄准离开 mob → swingCooldown=0 + 恢复挖掘
    （resetBreaking 对称）。
  - 死亡：damage() 致死置 dead（health 钳 0）；despawn pass 里收集掉落（**rebuild
    kept 之后** push ItemEntity——在扫描循环里 push 会被重建丢掉）+ 播 "death"。
    despawn_distance（40）3D 距离消散（仅活 mob）。mob 不入存档（MC 式瞬态）。
  - 渲染：makeMobModelGeometry（每 box 六面走 kCubeFaces 共享表、单 tile 六面、满天空光
    烘焙同掉落物约定、底心原点）→ mobMesh(MobID) 懒缓存 → drawMobs 在 opaque pass
    （drawItemEntities(false) 之后、粒子之前）：model = translate(position) ×
    rotateY(facingYaw)。**yaw 约定**：Mat4::rotateY(θ) 把局部 −Z 映到世界 (−sinθ, 0,
    −cosθ) = 相机 forward(θ)（pitch 0），所以箱体 −Z 面（zombie 脸）朝向 facing 方向。
  - **确定性关键修复（本阶段最重要教训）**：运动实体的截图可复现要求位置是帧号的纯
    函数。首版 mob 走真实 dt → 两次 240f 运行差 18px（zombie 剪影边缘亚像素级：负载
    抖动 dt 积分 ~3s 运动）。修复 = mob 固定帧步长 `kMobStepPerFrame = 1/60`（Game
    updateEntities 里 mob 用固定步、其余实体用真实 dt）——日循环 kDayStepPerFrame /
    物品动画帧计数器同款约定。此前 P10–P19 的 E2E 实体（掉落物）全是**静止位**（精确
    整数边界，dt 历史无关），所以未暴露。运动实体（mob/未来弓的箭）一律走固定步。
- **测试**（test_mob.cpp 新文件 37 + 见下）：**579/579**（P19 539 + 40）：
  - MobRegistryTest 7：ParsesZombie（全字段）/缺 id（全有效 JSON 去掉 id 字段）/
    非正 health/空 boxes/box 越界（head 顶 1.75+0.25>1.8）/drops_count 65/重复 id。
  - MobSpawningConfigTest 4：valid/环反/空名单/despawn≤max_radius。
  - MobSpawnCellTest 8（fixture：石地 y=0 面高 1、x,z ∈ [−16,16)）：扫描序首格
    (0,0,−6)[dz=−6 时 dx=0 先于 ±1：hypot(0,6)=6.0 入环、hypot(1,6)=6.08 出环]/
    **3 格墙**挡头顶（关键：单格只会成为新地板——floor 扫描取带内最高 solid，
    头顶检查只在"最高 solid 之上还有 solid"时触发）/流体格/环内无地板 nullopt
    （fixture 地板角 (−16,−16) 在 22.63 处——min_radius 必须 > 22.63！21.5 会命中
    角落）/间距（已有 mob 在候选点→下一格 (−3,0,−5)）/玩家 AABB 重叠（宽玩家
    z 起 −0.3：候选 (0,−1) 的 spawn z 跨 [−0.8,−0.2] 重叠→拒；下一候选 (−1,0,0) 的
    x 跨 [−0.8,−0.2] 不重叠→收）/地板高度带（玩家 floorY=10 → 全带无地板 nullopt）。
  - MobMeshTest 4：48 顶点 72 索引（2 box）/顶点在模型 AABB 内/六面绕序 CCW（cross
    点法向 >0，同 chunk mesher 不变式）/UV 在各 box 的 tile 内。
  - MobAiTest 7：追击（精确速度 0.8、**朝向 ±π 用 cos/sin 判**——atan2(−0.0,−3)=−π
    符号依赖 −0.0，直接比 π 会挂）/超距不追（wander 速度 ≤ 0.4）/攻击冷却（首帧
    命中、第 60 帧累计 1.0s 仍不命中[先减后判]、第 61±1 帧二次命中——**±1 ulp 容差
    写法**：while hits<2 && extra<4）/垂距 1.5 不攻击（Chase 不打）/死亡玩家忽略/
    同种子 300 帧轨迹逐位相等/击退（窗口内速度保持 −z 衰减、0.4s 后恢复 +0.8 追击）。
  - MobCombatTest 3：致死返回 true + health 钳 0 + 死后 damage 仍报 dead +
    shouldDespawn / 部分伤害 / 击退 impulse 叠加。
  - MobRaycastTest 4：entry t=4.7 精确 / 后方 miss / origin 内=0 / 平行出 slab miss。
  - MobItemsTest 3：tool attack_damage=3 解析 / 默认 1.0（material + block item）/
    非正拒绝。
  - 测试文件坑：`EXPECT_EQ(*cell, BlockPos{x,y,z})` 宏内逗号 → 须加括号
    `EXPECT_EQ(*cell, (BlockPos {x,y,z}))`；`Result<Vec3>` 返回须 `return Vec3{...}`
    （花括号直接初始化 Result 歧义）；`const Mob& def = makeMobRegistry().at(...)`
    悬垂（临时对象）→ 注册表须存活到 def 使用完。
- **E2E**（:0，`/tmp/opencode/p20_e2e.zsh` <base|mobs|mobs_off> out.png [config]；
  无输入场景 → 无需 xact/窗口移动/光标等待，直接跑 + 等退出 + 校验截图）：
  - base 60f ×2 与 `base18` **逐字节一致**（first_spawn_frame=60 > 最后更新帧 59，
    无 mob；atlas 加 tile 不影响既有 UV/渲染）。
  - mobs 240f ×2 **逐字节一致**（固定帧步长修复后）；mobs_off 对照（config 仅加
    `"mobs": {"enabled": false}`，`/tmp/opencode/config_xep_nomobs.json`）×2 一致。
  - mobs vs 对照：54245 px 差**全在世界区** x[273,678] y[242,485]（4 只 zombie 自
    4–6 块外以 0.8 b/s 追、240 帧内无人进 1.6 攻击半径）；8413 px 绿皮肤判据
    （G≥R+40 ∧ G≥B+60 ∧ G>120——注意草地 (92,163,62) 也能过 G 判据，所以判据只在
    **diff 像素内**数，不能全场数）；UI 区（y≥660 或 x≥1170）零差；心区零差
    （无伤害）；无红闪 tint（角落像素一致）。
  - 启动日志零 warning：atlas "loaded 19 from blocks" + "loaded 21 from mobs"
    （**累计**计数——loadDirectory 日志打的是 m_tiles.size()，非本次加载数）+
    "uploaded 64x128 (21 tiles)"（双向交叉校验）。
  - zombie 像素位置参考：mobs run 帧 239，zombie 剪影 bbox x[273,678] y[242,485]
    （玩家 (0.5,55.62,0.5) 眼、pitch −0.25 默认）。
- **数据/资产清单**：`assets/data/mobs/zombie.json`、`assets/data/mob_spawning.json`、
  `assets/data/items/rotten_flesh.json`、`assets/textures/mobs/{zombie_head,
  zombie_body}.png`（tools/generate_mob_textures.py）、`assets/textures/items/
  rotten_flesh.png`（generate_item_icons.py +rotten_flesh()）、block_atlas.json +2
  tile、`data/items/{wooden,iron}_pickaxe.json` +attack_damage、CMakeLists +4 源
  +1 测试。
- **提交**：代码 + 数据 + 文档（README 可玩内容/数据文档/roadmap、KNOWN_ISSUES
  Mob 节 + P16 伤害来源行更新 + 粒子缺陷行 + P20 归档条、SESSION_CONTEXT 本节）。
  config/config.json（用户手改 view_distance 80 / day_length 600）不入档、不回退。
- **P21 备忘**：弓 = Projectile（P10 已有实体+单测）+ 发射器（左键 vs 右键取舍、
  挥击节奏可复用）+ 渲染（箭几何）；Projectile 也是运动实体 → 走 kMobStepPerFrame
  同款固定步（或独立 kProjectileStepPerFrame）。更多 mob = 纯数据。粒子同 burst
  同值缺陷 = 一行修复（uniform01 推进 state），无基线风险（无截图覆盖粒子）。
