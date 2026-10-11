# Phase 42 细节存档

> 用户确认方向（"按建议顺序连续执行两阶段"，P42 = 弓后续 2）。
> 范围 = P21/P24 遗留：3D 手持手部（view model）+ 插墙箭 100s 寿命
> （MC 值）+ 方块变更释放箭 + 插墙箭拾取 + 箭击退随蓄力。
> 无存档格式变更（format 6；箭不入存档 = 瞬态实体）。

## 阶段报告

- **`[2026-10-12, Phase 42]` 弓后续 2：3D 手部 / 插墙箭寿命 / 释放 / 拾取 / 击退随蓄力**
  — 769/769（+7），`-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 16 次结构性
  免疫免重锚 + S2 跨构建存档一致 + S3/S4 输入运行存档精确 + 像素 0 bad）。

### 实现内容

**Projectile（`entity/projectile.{hpp,cpp}`）**
- 新字段 `power`（构造参数，默认 1.0）：发射蓄力，缩放 mob 命中击退。
- 新字段 `ammoItem`（构造参数，默认空）：发射器弹药物品名；非空 = 插墙后
  可被拾取（返还 1 个该物品）。
- `kStuckMaxAge = 100.0`：插墙箭独立寿命（Minecraft 值）；飞行箭仍
  `kMaxAge = 30`。`shouldDespawn()` 按**当前**状态取上限（age 累计不重置）
  → 插墙 >30s 后释放的箭立即消散（MC 分歧，文档化）。
- `release()`：stuck 清除 + 速度清零（位置不动）→ 箭落回飞行物理
  （重力 + 碰撞 + 可再命中 mob）。飞行箭上 no-op。

**击退随蓄力（`core/bow.hpp`）**
- 纯函数 `arrowKnockback(velocity, power)`：沿箭**水平**速度方向
  `kArrowKnockbackSpeed = 4.0 × power` 水平冲量 + `kArrowKnockbackLift =
  2.5 × power` 抬升；零水平速度 = 仅抬升（1e-4 守卫同 P21）。
- power = 1 与旧常数**逐位相同**（skeleton/满蓄力零行为变化）；
  game.cpp 的 `kKnockbackSpeed/Lift` 保留但**仅近战**用。

**方块变更释放箭（`Game::setBlock`，`core/game.cpp/.hpp`）**
- 私有 `Game::setBlock(x, y, z, id)` = `m_world.setBlock` + 释放卡住格内的
  插墙箭；**破坏 / 放置 / 染色换块 3 个调用点全走它**（单收口）。
- 判定（关键坑）：碰撞解算器把箭**精确 clamp 到面**（盒最大坐标 = 面
  平面，不进入块）→ 严格重叠判定永不触发；纯非严格"触任意面"有假阳性
  （往箭盒所在的空气格放块也会释放）。正确 = 每轴 `inAxis`（盒区间 ∩ 格
  闭区间 ≠ ∅）**且**至少一轴 `touchAxis`（盒从**外侧**触面：中心严格在
  面外）。两个 `[half]` 捕获 lambda，循环内逐箭判定。

**插墙箭拾取（`Game::updateEntities`，物品拾取环之后、同 `gone[]` 机制）**
- 规则：**stuck 箭**（飞行箭不可拾取）+ `ammoItem` 非空 + 距**玩家底心**
  （`position()`，非 AABB 中心）≤ `kArrowPickupRadius = 1.0f` →
  `m_inventory.add(aid, 1, m_items) ≥ 0` → gone + "pickup" 音效；背包满 =
  箭留墙内、**每帧重试**。不加 playerAlive 守卫（与物品拾取一致）。

**mob `ammo` 字段（`entity/mob_registry.{hpp,cpp}`）**
- `MobProperties::ammoItem`：JSON `ammo` = 非空字符串（显式空串/非字符串
  拒绝）；**`shoot_range > 0`（远攻）必填**；近战 mob 不给 = 无远攻。
- init 交叉校验弹药物品存在（同 launcher `ammo` 模式）；
  `assets/data/mobs/skeleton.json` 加 `"ammo": "arrow"`。
- fireBow 传 `power, item.ammo`；skeleton 射击捕获 `arrowAmmo`、传
  `power = 1.0f`（骨架满威力，与 P27 逐位同）。

**3D 手持手部（`Game::drawHeldHand` + `handIconMesh()`，`game.cpp/.hpp`）**
- render() 中 **drawArrows 之后、不透明 drawParticles 之前**调用；
  门 = `m_window.relativeMouseMode() && !panelOpen()`（无输入永不锁定 →
  基线结构性免疫）。
- 矩阵：相机→世界 `C = T(pos)·Ry(yaw)·Rx(pitch)`（C·(0,0,−1)=forward 已
  验证）；`model = C·T(kHandOffset)·Rx(spinX)·Ry(kHandSpinY)·S(scale)`。
  常数（game.cpp）：`kHandOffset {0.42, −0.38, −0.85}`（相机系：x 右、
  y 上、z 眼后）、`kHandCubeScale 0.28`、`kHandIconScale 0.5`、
  `kHandSpinX −0.35`、`kHandSpinY 0.6`。
- **块物品** = `itemMesh`（掉落物同款 3D 方块体，atlas + 世界 dayFactor
  照明，夜晚变暗）；**图标物品** = 共享单位 quad（顶边 y=+0.5 ↔ v=0 与 2D
  UI 图标同向；CCW from +Z 索引 `{0,3,2, 0,2,1}`）+ skyLight = blockLight =
  1（`max(sky·dayF, block) = 1` 恒亮）。
- 四个渲染坑（全踩全修，PITS 有条目）：
  1. `Renderer::draw` 每次 draw **解绑程序** → setUniform 前必须
     `m_basicShader.bind()`（残留 GL 1282 毒化懒建 mesh 上传的 glGetError，
     每帧重复、无崩溃、纯静默）；
  2. 不透明段 **blending 关** → 图标透明像素 (0,0,0,0) 渲成不透明黑方 →
     draw 前后 `enableBlending()/disableBlending()`；
  3. **faceShade 是顶点着色器从模型旋转后的法线分桶的**（+Y→1.0 /
     −Y→0.5 / |X|→0.8 / 其余 0.6，MeshVertex 无 faceShade 字段）→ 图标
     quad 几何 +Z 法线经自旋落 0.6 桶暗 61% → **顶点法线取 +Y**（所有自旋
     态 n.y > 0.5 恒亮）；
  4. 懒建 mesh 上传失败 = 每帧重试（valid() 门），日志是唯一的诊断入口。
- 动画：**拉弓** `offset.z += 0.3·p` + `spinX += 0.9·p`（后拉 + 弓体后仰）；
  **进食** `offset.y += 0.35·progress` + `offset.z += 0.35·progress`（抬向
  面部）。**2D 手持图标拉弓位移删除**（`kBowDrawPullPx` 常数删；2D 图标
  固定，3D 手部为唯一拉弓/进食指示器）。

### 变更文件

- `src/core/game.cpp` / `src/core/game.hpp`（setBlock 收口 + 3 调用点 /
  箭拾取环 / 手部常数 + handIconMesh + drawHeldHand + render 接入 /
  fireBow 与 skeleton 射击传参 / init ammo 交叉校验 / 2D 拉移删除）
- `src/entity/projectile.hpp` / `.cpp`（kStuckMaxAge / power / ammoItem /
  release / shouldDespawn 分状态上限）
- `src/core/bow.hpp`（arrowKnockback + 常数）
- `src/entity/mob_registry.hpp` / `.cpp`（ammoItem 解析 + 远攻必填）
- `assets/data/mobs/skeleton.json`（`"ammo": "arrow"`）
- `tests/test_entity.cpp`（既有插墙寿命测试改 100s 语义 + 3 新测试）
- `tests/test_bow.cpp`（arrowKnockback 4 测试）
- `tests/test_mob.cpp`（kSkeletonDef + ammo 断言 + 3 拒绝用例并入既有表）
- 文档：KNOWN_ISSUES / MECHANICS / GAMEPLAY / CONTENT_GUIDE / ROADMAP /
  PITS / AGENTS

### 测试

769/769（+7）：
- test_entity：`StuckArrowDoesNotMoveAndAgesOut` 改语义（过 30s 飞行上限仍
  存、100s 消散）+ `ReleasedStuckArrowFallsAndRejoinsFlight`（release →
  零速下落 → 地面重插）+ `ReleaseIsANoopWhenNotStuck` +
  `PowerAndAmmoItemRideWithTheArrow`（默认 1.0/空 + 自定义保持）。
- test_bow：`arrowKnockback` ×4（满威力 = 4.0/2.5 逐位【16.0 保持归一化
  二进制精确】/ 0.5 半 / 水平方向 (3,4)→(2.4,3.2) 分解 + −x 反向 /
  垂直仅抬升）。
- test_mob：kSkeletonDef 加 `"ammo": "arrow"`（远攻必填，不加则既有
  ParsesSkeleton 红）+ 断言 + 拒绝用例（空串 / 非字符串 / 远攻缺 ammo；
  近战无 ammo 合法 = zombie）。

### 构建与验证结果

- `cmake --build build`：0 警告（`-Wall -Wextra -Wpedantic -Werror`）。
- 无输入基线门 `/tmp/opencode/base18.png` **字节一致（免重锚）= 第 16 次
  结构性免疫**（论证：3D 手部仅锁定时渲染；击退/寿命/释放/拾取均需输入
  或 mob 箭；skeleton ammo 数据无箭时休眠；P41 参考构建 @ec23b59 同资产
  加载验证——P41 解析器对未知 property 键静默忽略）。

### E2E（批跑 `/tmp/opencode/p42_e2e.zsh`，全绿）

- **S1 base 门**（新世界 60f 无输入 ×3）：P42 ×2 + P41 参考构建
  （worktree @ec23b59）**三方字节一致** + 与 base18 字节一致 + skygate
  （120,181,242）×3。
- **S2 回归 A/B**（p27far 夜 360f seed 9）：P42 ×2 存档字节一致（确定性
  权威）+ **P41 ref 存档跨构建字节一致**（无 format 升格；脚本对 P42 合法
  的"+1 arrow 拾取差"留了断言分支，本次运行未触发）+ 截图 P42 ×2 字节
  一致 / vs ref veil 容差（real-dt 红纱边界同 P27 签名）+ 出生序列 +
  health 8 钉值。
- **S3 输入运行"发射 & 插墙不拾取"**（远墙 z=−2 双 wool【(22,45,−2)+
  (22,46,−2)】、slot0 bow + slot1 arrow×3、yaw/pitch 0 平射——箭落点
  y≈46.52 命中顶块、插于 (22.5, 46.52, −1.125) 距底心 2.23 > 1.0；锁 →
  右键 1.8s 满蓄力 → 释放，300f）：存档**精确**（箭 3→2 净消耗 = 发射且
  未拾取、墙未动、pos/yaw/pitch 零漂移、20/20）+ 像素 vs 无输入对照：
  26,343 diff = 箭棕 1,448（kArrowColor (0.36,0.27,0.16) ×faceShade 0.6/
  0.8/1.0 三色族 ±8）+ 准星 40 + **3D 手部 1,640**（bow 图标 quad，中心
  约 (894,590)）+ 暗轮廓 1,431（目标块 outline = 锁定 UI）+ HUD 21,784 +
  **0 bad**。
- **S4 输入运行"释放 & 拾取"**（近墙 z=−1 双 wool、同库存；箭插顶块
  (22,46,−1) 于 (22.5, 46.61, 0.125) 距底心 1.65 不即时拾取 → **Tab×2 换
  空槽 2**（发射器选中左键永不挖掘——仅近战）→ 左键按住 1.4s 破顶块
  （wool 硬度 0.8 → 1.2s；余量 0.2s 对下方块仅 17% 裂纹、松手重置）→
  `Game::setBlock` 释放箭（日志 (22.5, 46.614, 0.125)）→ 自由落体 1.55 块
  ≈0.39s → 落地 (22.5, 45.0, 0.125) 距底心 0.38 < 1.0 → **落地帧即拾取**
  （日志 (22.5, 45.0, 0.125)；无行走 = pos 零漂移），360f）：存档**精确**
  （箭 3→2→3、块 diff 只剩底 wool【顶块已破】、pos **精确** [22.5,45,0.5]、
  selected 2、yaw/pitch 零漂移、20/20）+ 像素 vs 无输入对照：739,541 diff
  = wool 类 616,626（近墙占满屏中央：破块露天空——wool 顶面 ×1.0 + 正面
  ×0.612 逐通道噪声族）+ HUD 122,915 + **0 bad**（准星/手部像素被 wool 类
  吸收——此场景锁定 UI 全画在 wool 面上、wool 判定先行；3D 手部渲染证据
  由 S3 承担。另：近墙场景 3D 手部世界坐标在顶块内 = 被块面深度全遮挡，
  0 像素属预期，PITS 有条目）。

### 已知问题（新增/变更，docs/KNOWN_ISSUES.md）

- 归档 2 条：~~插墙箭寿命 = 30s~~、~~拉弓 = 2D 手持图标位移~~（均 P42
  修复）。
- 新增 4 条：非块 3D 手部 = 平面图标 billboard（无 per-item 3D 模型/手部
  皮肤/弓弦）；插墙箭拾取 = 返还发射器弹药（背包满每帧重试）；释放箭共享
  累计寿命（上限按当前状态切换——插墙 >30s 释放即消亡，MC 分歧）；2D
  手持图标不再拉（3D 手部为唯一指示器）。

### PITS +8

1. Renderer::draw 解绑程序 → setUniform 前必须 bind（残留 GL 1282 毒化
   懒建上传，每帧静默失败）。
2. 不透明段 blending 关 → 带透明像素的纹理 quad 渲成不透明黑方。
3. faceShade 由顶点着色器从模型旋转后的法线分桶（图标 quad 法线取 +Y 恒亮）。
4. 插墙箭盒只触及卡住格面、不重叠（release 判定 = inAxis ∧ touchAxis）。
5. 发射器选中左键永不挖掘 + 热栏无数字键（Tab 唯一）+ wasPressed 每帧
   （xdotool key 间隔 >1 帧）。
6. 3D 手部可在 solid 块内（近墙场景全遮挡 0 像素无报错）。
7. gtest EXPECT_EQ 不接受花括号初始化列表（具名变量）。
8. 纹理噪声逐通道独立——像素颜色分类器必须逐通道独立偏移。
