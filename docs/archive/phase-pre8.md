# PRE-8 mob 火焰（v1.x 矩阵 29/33，v1.29.0，门 144，PRE 行 2/6）

## 定案

原始定义已佚 = 定案 **燃烧 mob 的余烬火焰**（消费 P73/P74 燃烧系统的表现层
缺口：mob 有火 tick（P57/P73 点火 + 灼烧）但无火视觉；玩家燃烧表现 = 既有
0.2 s tick 红纱已覆盖，不入本项）——CRAFT-5 炉口火焰同款范式：

- **每燃烧 mob 一批上升余烬**（`MobEntity::burning()` = P57/P73 火系统状态：
  在熔岩/严格重叠火块格每步重点燃，离源残留 8 s 倒计时）
- **周期 36 帧**（`kMobFlameEveryNFrames`，CRAFT-5 值：>2× 余烬 0.5 s 最大
  寿命 = 任意 ≤60 fps 下截图帧只携 age-0 批，前批已死；帧锁定硬规则 8）
- **发射器复用 P66 `spawnFire`**（2 粒/批、负重力上升、0.3-0.5 s 寿命）
- **专用盐 `kMobFlameRollSalt`**（P66/CRAFT-5 纪律：mob 流不追踪炉口/火块余烬流）
- **自发光**：余烬 block 光 = 1.0（燃烧本体照亮，深夜也亮 = MC 观感）+ 中心格
  天光作环境项；与 CRAFT-5 炉口余烬（在炉上方空气格、环境光）的记档差异 =
  "炉口火是探出，mob 火是裹身"
- **中心格 = `entityCenterCell` 纯函数**（AABB 中心 = position + height/2，
  逐轴 floor 负坐标安全 = 硬规则 1；从 PRE-6 `itemDropLight` 提取共享）
- **`render.mob_flames` getBool 代码默认 true**（config.json 零改动，
  render.furnace_flames 先例）；off = 零发射 = 对 v1.28.0 位级恒等 = 结构免疫根
- 零 format（22）/零 config.json/零内容/零音效/零世界状态

## 实现

- `game.hpp`：`updateMobFlames()` 声明 + `m_mobFlames` 成员
- `game.cpp`：常量（`kMobFlameEveryNFrames`/`kMobFlameRollSalt`）+ config 读取 +
  update() 调用点（updateFurnaceFlames 之后）+ `updateMobFlames()` 函数体
- `item_light.{hpp,cpp}`：提取 `entityCenterCell(position, height)` 纯函数，
  `itemDropLight` 改调（行为位级不变）
- 无资产变更（火块/余烬粒子/纹理全既有）

## 测试

**1894/1894（+6）**：`EntityCenterCellTest`（本体格/负坐标 floor/边界 floor
向下/短体跨界 4 例）+ `MobFlameConfigTest`（缺省 on/显式 off 2 例），
`-Werror` 0 警告，banner v1.29.0。

## 门 144

**S1 PASS = 第 144 次结构性免疫 + base18 锚连续**（387726 B 免重锚）：quiet
config（nomobs+spread0）fresh 60f 无输入四方 ×2（new v1.29.0-dev ×2 / old
v1.28.0 tm_v128_src ×2）RAW 全 EXACT + new≡old + 同侧 savenorm EXACT ×2 +
跨版 savenorm EXACT（零 format 变更，两侧恒写 22）。

**S2 PASS = 燃烧 mob 活粒子协议（CRAFT-5/P66 同款；场景 = 膜区抬高到平坦
y39 + 8 格火膜环 + 2 格石柱 + 僵尸追踪【dy=2 看得见够不着，玩家零伤】+
`fire.particles=false` 隔离 P66 膜余烬）**：
- OFF×2（new, mob_flames=false）RAW **字节一致**（无余烬静态）
- OLD×2（v1.28.0，无此特性）RAW **字节一致**
- **OFF≡OLD RAW 字节一致 = 代码级免疫**（off 管线 = 旧管线）
- **ON×2（new, mob_flames=true, **N=540 = 36 倍数**）diff = 371 px 限
  x[492,518] y[293,322] 画面中心膜区小簇 = 活余烬（real-dt 位移，PITS 327：
  N 非 36 倍数 → 末帧非发射帧 → age-0 批缺席 → 余烬已死 → diff=0 假阴性）**
- savenorm OFF≡ON≡OLD：仅 1 格 fluid @ (81,73)（z 在熔岩湖带 25..93 内 =
  视图外 real-dt churn，PITS 244/283 类非回归）
- 玩家 hp 20 恒定（零受伤零红闪 = 判别纯净）

**S3 PASS**：套件 1894/1894。

**性能 A/B PASS**（真 v1.28.0 tm_v128_perf vs v1.29.0-dev tm_v129_perf，同款
g139 插桩两侧同打，banner 实证，fresh --new --seed 123456789 900f 三窗 + 终档
集合）：PERF-WIN 全指标同档（upd med 0.37-0.44 vs 0.34-0.40 / render med
0.55-0.67 vs 0.55-0.58 / swap 15.4-15.6 vsync 锁定 / fluid 0.046-0.056 vs
0.042-0.060 / fires 0.008-0.010 / spread / ents 0.0033-0.0042 vs 0.0032-0.0035 /
stream 0.013-0.016 vs 0.014-0.015）+ PIPE3-S0 changesApplied max 9/3/1 三窗
逐位 + **终档 fluids 18/18 + blocks 55/55 集合恒等（diff 0）= 同世界同成本**
【结构惰性最强形：nomobs 无输入跑零燃烧 mob = 零余烬发射】= 成本可忽略。

## PITS

- **PITS 324（夜间场景须用敌对 mob——被动 mob 夜间不生成）**——被动 mob
  （cow/sheep/chicken）生成需光照 ≥9，夜间月光地板仅 7（LGT-3）→ 夜间火膜
  场景零被动 mob（v9 实证：膜可见但无 mob 踏膜，ON-vs-OFF diff=0）。夜间
  燃烧 mob 场景必须用敌对 mob（zombie，spawn_max_light 7 ≤ 月光 7）。
- **PITS 325（敌对生成保护圈 = 场景须离世界出生点 > 半径）**——Normal 难度
  保护圈 12 块（`kSpawnProtectionNormal`，锚世界出生点 (0,0)）。玩家放出生
  点 → 环 5-6 全在保护圈内 → 零敌对生成（v6 实证：场景静止、无僵尸）。燃烧
  mob 场景须整体 >12 块离出生点（本场景膜在 (20..22,-9..-7) ≈ 23 块）。
- **PITS 326（玩家抬高的 dy 双重约束：安全须 >1、追踪须 ≤2；周边高地形破
  安全）**——玩家 2 格石柱顶（y40，膜 y38）：膜上僵尸 dy=2（追踪✓够不着✓）
  但西侧坡地 y39 的僵尸 dy=1 → 攻击（v7/v11 玩家 hp0 实证）。修 = **把膜区
  抬到与周边高地形同平（y39）**（置 stone@y38），僵尸恒在 y39（dy=2），
  玩家 y41，安全+追踪同时满足（v12 实证 hp20 恒定 + 60-92 次发射）。
- **PITS 327（余烬截图帧数须为发射周期 36 的倍数，否则活余烬缺席）**——
  发射门 `(m_frameCount+1)%36==0` + 余烬寿命 0.3-0.5s real-dt：N=500（非
  36 倍数）末帧 499 非发射帧、最近发射 467（32 帧前 = 低帧率 1.6-3.2s >
  寿命）→ 截图无活余烬 → ON×2 diff=0 假阴性（CRAFT-5 PITS 316 边界教训第 2
  次）。修 = N 取 36 倍数（540 → 末帧 539，(539+1)%36=0 = age-0 批在帧）→
  ON×2 diff=371px 判别成立。凡活粒子余烬 A/B，帧数须对齐发射周期。
