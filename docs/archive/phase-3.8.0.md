# Phase V3-3 — 下界 mob（v3.8.0，门 198 全 PASS）

> 设计定案 = `/tmp/opencode/v380_design.md`（5 mob 数值表 / M1 neutral /
> M2 walks_on_lava / M3 下界生成池零新基建 / 资产 / 测试族 / 门论证）。
> 依赖 v3.7.0（下界深化：熔岩海/台地/空腔/音族）。**Phase B 预留槽
> 填充**（m_nether.mobs 空池 + currentEntities 维度隔离 + 只 tick
> 活跃维 = 既有架构，"Phase C fills it" 注释兑现）。

## 实现

### Mob 族（5，纯数据）
| mob | 类 | HP | 行为 | drops / xp |
|---|---|---|---|---|
| zombified_piglin | hostile | 20 | 追猎 3 伤（zombie 同刻度，**undead false**） | rotten_flesh 1 / 5 |
| piglin | **neutral** | 16 | 闲逛；受击 → 追击 6 伤 | 无 / 3 |
| hoglin | **neutral** | 40 | 闲逛；受击 → 追击 6 伤（慢速 1.0） | hoglin_meat 1-2 + leather 0-1 / 5 |
| magma_cube | hostile | 16 | 慢速追猎（0.8）3 伤，不分裂不跳（blob 3 盒无脸） | 无 / 0 |
| strider | passive + **walks_on_lava** | 20 | 熔岩面行走 | leather 0-1 / 3 |

**下界生成池** `mob_spawning_nether.json`：权重 10/8/7/5/1（MC
近似）+ cadence 主世界同款；**strider spawn_max_light 7→15**（门内
实证机制缺口：熔岩自发光 15，7 门对 strider 自毁——见偏离 1）。

### M1 neutral 旗（mob JSON 严格 bool，与 hostile 互斥点名拒）
- `damage()` 单收口钩子：**任意来源**受击（玩家/箭/火 tick/摔落/
  毒）→ m_angry + kNeutralAngryDurationSeconds 30 s（real-dt 生存类，
  硬规则 8）；再击刷新；致死不置态（dead data）；玩家死亡即时脱激
  （重生玩家不见 29 s 旧怨）。
- chase 谓词单式第二支 `(p.neutral ? m_angry : true)`——非 neutral
  = 常量 true = 改前位级恒等（无物种 switch）。

### M2 walks_on_lava 旗（严格 bool）
- 点火豁免：`inLava = overlapsFluid(Lava) && !walksOnLava`（火块
  接触仍点燃 = MC strider 踩火也烧）。
- 熔岩面行走：`onLavaSurface()` 单判定（旗 + 活熔岩平面 +
  atSurface/inFluid 并）→ integrate 走 `integrateLavaSurface`：feet-
  on-plane snap 不动点（fps 无关）+ 深潜浮回 + 落体免溅落；水下
  0.6× 速度门 + 流体峰值估计同旗豁免；strider 在水仍游泳档（下界
  蒸发使其实践不可达，记档边界）。
- 生成面：findMobSpawnCell `walksOnLava` 尾参（默认 false 位级根）
  = 熔岩源格顶面（顶向下扫 + 头顶 air + 下方异种流体拒【海柱收形】
  + 光门/间距/保护同款）。

### M3 下界生成池（零新基建）
- `loadNetherSpawnPool` 纯函数（checkWoolDrops 范式：文件缺失/坏
  schema/未知 mob 全点名，单测可测）；buildScene 内 NetherGen 缓存
  后解析 → m_nether.mobs.spawnConfig（**fail-fast init 点名**）+
  会话策略继承（enabled/difficulty/limbSwing ← 主世界同 config 值，
  偏离 5）。
- 类型表 = 游戏单一共享注册表 m_mobs.mobTypes（偏离 4：双注册表
  MobID 错位风险，渲染缓存键位空间一致）。
- 维度选池/只 tick 活跃维/非活跃维冻结/mob 不入存档 = 既有 Phase B
  语义（零代码，偏离 8）。

### 资产
- 纹理 9（per-face 脸 4 + 身体 4 + magma blob 1；salt 0x9A01-5；
  atlas 136→145 空格零移位）/ 音效 3（hoglin_grunt/piglin_whisper/
  strider_hiss，种子 0x9201-3，60→63；**地面 ambient roll 新
  runAi 分支** = bat flyCruise 先例的地面推广，数据字段门控零流
  抽取，偏离 3）/ 物品 2（hoglin_meat +4 / cooked_hoglin_meat +8 +
  smelting）+ icon 2。

## 门 198（结构免疫第 79 次；锚 224468 B 免重锚）

**结构免疫论证**：新旗主世界无数据消费 + 下界池只在下界消费 +
atlas 空格零移位 → 无输入主世界 ≡ 锚。
- **S1** 无输入 60f ×2 ≡ 锚逐字节 + savenorm EXACT + 白名单 4 键 +
  banner v3.8.0（注：g198_s1 首跑 sed 派生 OUT 目录未换 = PNG 落
  s197/ 假 DIFF，PITS 389②——正确位置复验 ≡ 锚）。
- **S2**（mobs-on 配置 g198_mobs.json——门基线 QUIET 配置
  mobs.enabled=false 是静默纪律，PITS 389①）：
  - (a) 下界高原场景 ×2（传送落点 = portal 光 11 + 原点保护圈压制
    位，PITS 389⑤）：生成流 **×2 恒等**（piglin f60 / magma f90 /
    zpig f120 / piglin f150，位置逐字节）+ 截图 RAW 恒等 + 终档
    census（dim nether / format 30 / nether_entities 0 = mob 不入
    存档既有）。
  - (a2) **strider 熔岩面决定性臂**（环全清 4704 air 编辑 → 陆地
    pick 全 no-valid-cell → cap 永不饱和 → 每次尝试 = 新抽，PITS
    389④）：探针 1 迭代命中——**`strider f660 at (13.5, 32.0, −5.5)`
    ×2 恒等，y = 32.0 = 熔岩海顶面精确**（5-31 源顶 31 + 1）+
    540f 零 burn（点火豁免 E2E）。
  - (b) 池隔离：主世界 600f mobs-on = OW 生成 4 只 + 下界物种
    spawn 行 0（"Registered mob" init 行 ≠ spawn 行，PITS 389③）。
- **S3** 全套 2697/2697（编排者独立复跑）。
- **S6 点名 6/6**（scratch 资产手术全 init 带路径 rc=1）：
  neutral∧hostile 互斥 / walks_on_lava 非 bool / neutral 非 bool /
  nether 池缺失（asset_manager 点名）/ 池未知 mob 'dragon' /
  ambient_interval 无 ambient_sound（单向严格对——单 ambient_sound
  合法 = 缺省 interval，首跑方向错已修正）。
- **性能 A/B（用户硬要求）**：旧 = v3.7.0（tm_v380_perf = 44cfa95 +
  g139，banner v3.7.0 实证），新 = v3.8.0（tm_v380_perf_new，banner
  v3.8.0 实证）。主对 900f 无输入（mobs-off 静默基线）：PERF-WIN
  三窗同档（upd med 0.427/0.547/0.578 vs 0.588/0.529/0.516 =
  窗口间机器噪声带；render 均值 0.524 vs 0.532 同档）+ **changes-
  Applied 9/1/3 逐位恒等** + **终档 savenorm EXACT**（nether-pool
  only 零 format 完全恒等最强形）。下界 mob 自成本 = S2 场景无
  性能窗（生成/tick 成本由 ents 窗同档 + 既有 mob 先例覆盖）。

## 单测族（2658 → 2697，+39）

NeutralRegistryTest 4（双旗解析/互斥/非 bool）/ NetherMobContent 6
（5 mob 数据钉 + 23 文件 FNV 资产钉）/ NeutralMobTest 7（受激帧钉/
30s real-dt 脱激/玩家死亡脱激/再击刷新/致死不置态/既有族位级）/
LavaWalkerTest 7（onLavaSurface 判定面/feet-on-plane 不动点/深潜
浮回/点火豁免/速度门豁免/strider 在水中仍泳档）/ MobSpawnCellTest
6（熔岩源顶面收/非熔岩拒/异种流体拒/海柱收形 4）/ NetherSpawnPool
5（缺失/坏 schema/未知 mob/权重词表/合法收）/ SoundNetherMobTest
3（3 wav PITS 382 纹理钉）/ V33HoglinMeatChainPinned 1（食物链）；
census 有意重钉（atlas 145 / 音效 63 ×2 / 物品 95 ×2 / F3 v3.8.0）。

## 偏离/记档（7 条）

1. **strider spawn_max_light 7→15**（门内实证：熔岩自发光 15，7 门
   自毁永不生成；15 = 熔岩面独占位，其余 4 mob 留 7）。
2. 下方 "second 流体" = 异种流体（熔岩海柱顶格下方即熔岩本身收）。
3. 地面 ambient roll 新 runAi 分支（bat 先例只在 flyCruise——机制
   缺口如实报告；数据字段门控，无 ambient 的既有 mob 零流抽取）。
4. M3 类型表 = 单一共享注册表（双注册表 MobID 错位风险）。
5. nether MobSystem 策略继承主世界会话值（enabled/difficulty/
   limbSwing 一线三赋；mobs.enabled off = 双维无 mob）。
6. 下界保护圈中心 = 下界原点 (0.5,0.5)（结构默认；MC 下界无保护）。
7. 既定不做：piglin 无交易 / magma 不分裂不跳 / strider 不可骑乘 /
   ghast 延后（爆炸机制）/ zpiglin undead=false / 任意来源激怒 /
   非活跃维冻结既有语义。

## 变更文件（显式路径，47 + config 除外）

核心 11（mob_registry×2 / mob_entity×2 / mob_spawning×2 /
nether_spawn_pool×2 新 / game_init / game_entities / game.hpp 注释 /
CMakeLists 3.8.0）+ 数据 8（5 mob / 池 / 2 物品 / block_atlas.json
145）+ 资产 14（9 纹理 / 3 wav / 2 icon）+ 工具 3（纹理/音效/icon
生成器）+ 测试 8（7 改 1 新）。

零改动红线核证：主世界 mob 数据零触碰（FNV 钉）/ 主世界池文件
零触碰 / 渲染器零新机制 / 存档零 format（30，mob 不入存档既有）/
config 零改动。
