# FLUID-3 mob 涉水修正（v0.32.0，门 115）

10-26 用户实机报障（bug 3）："牛羊小鸡走入水中，或走过悬崖边有时候
有问题，他会悬在空中，或站在水上，这不合理。"

## 根因（explorer 调研定案：共享流体模型三缺陷 + 踏阶双门）

ENT-4 把玩家 FLUID-2 水面模型上提 `Entity` 共享给 mob/item 时，删掉了
玩家的全部输入逃逸通道（跳跃/潜行潜水）又加了 `!m_inFluid` step-up 门
——mob 陷入玩家可以自由进出的水面状态机且**无出口**：

- **B 站在水上**：浮漂终态 `y = W'`（脚面=水面、**整个身体干**、
  onGround=false、vy=0）是封闭不动点。被动上浮分支**无 inFluid 门**
  （玩家孪生有 m_inWater 门）→ 击退上抛/岸沿走入的干身也被推上浮 →
  snap 钳回 → 终态；下沉分支要 vy<0 但终态 vy 恒 0；无跳跃；step-up
  双门（onGround && !m_inFluid）全禁 → **永久踩水皮游走**。
- **A 悬在空中**：① 悬崖坠水时下沉钳制 −4 在**空中 0.4 atSurface 带内
  就生效**（脚面离水面 0.4 的身体还在空气里）= 自由落体被"降落伞"化
  （玩家孪生干带 keep vy）；② 浮漂终态 + 岸壁 ≥1 格 = 崖壁根部永久
  困锁（同上无出口）。深水沉底 resting 也困（resting 门压死上浮 +
  岸壁 >1 踏不过）。

两者都是**无输入 AI 自主路径**（zombie 追逐/wander 入水即可触发）：
历次无输入门的结构性免疫论证（"fresh 60f 无 mob 入水"）只保 60f 窗内
惰性，实机长跑必现——= 与 UXFIX-1（locked 门全盲）同族的"实机首触发"
模式，但本项无输入跑即可复现。

## 修复（定案：MC 式游泳——浅水涉水 / 深水游泳 / 可出池，4 处）

全部在 `src/entity/entity.cpp`（共享模型）+ `src/entity/mob_entity.cpp`
（踏阶门）；**玩家孪生（player.cpp）位级不变** = test_player.cpp
零改动红线；item 随共享路径变（半浸没终态，记档）；零 format（19）/
零 config/零内容/帧锁定。

1. **下沉分支加 `m_inFluid` 门**：水面以上干带 keep vy = 全速过带
   （玩家孪生 "keep vy" 行对齐）。修 A2 降落伞。
2. **上浮分支：inFluid 门 + 游泳位目标 + Q2 深度门**：`m_inFluid &&
   (!m_restingOnSolid || m_fluidDepth ≥ kRiseMinPoolDepth 2.0) &&
   vy ≥ 0 && feet < 游泳位`。干身不上浮（修 B1 制造链）；1 格浅池
   resting = 站底涉水不变（E4 主线保留）；≥2 深池 resting 上浮出底
   （修 B3/A1 深水困锁）；到位即停。Q2（phase-fluid2.md:236-238 开放
   问题"加面下深度门？"）= 本次消费，玩家侧 Q2 偏离不动。
3. **snap 改游泳位**：`y* = W' − h/2`（身体中轴=水面，半浸没 = MC 游泳
   观感），门 `!onGround && inFluid && vy ≥ 0 && feet ∈ [y*−eps, W']`
   （**inFluid 非 atSurface** = 干带无 snap：击退 mob 可溅出水面再全速
   落回，MC 同款；终态 = 封闭不动点保留，fps 无关）。
4. **mob step-up 删 `!m_inFluid`**（`m_onGround && lengthSq(preMove)>0`
   = 玩家对齐）：1 格浅池涉水 mob 踏岸壁出池（修"走入水出不来"）。
   游泳位 mob（onGround=false）仍无 step-up = **记档偏离：无 swim-up
   爬岸**（矩阵外残余；观感 = 水面游泳，不悬空不踩水皮）。

**W' 语义演进（配套）**：浸没体 W' = 中心列窗内（feet ±32）**最高
L8 +1 = 真水面**（原"脚下第一个 L8"对站池底体 = 底部水格顶，游泳位/
深度门都会算错）；干身体 = 脚下第一个 L8 即最高（上扫 inFluid 门控，
**干身路径 World 读次数逐位不变** = 结构免疫论证支点）。

## 测试（1672 → 1677，+5 新 / 重钉 6 / 反转 1）

- **反转 1**：`NoStepUpInWater` → **`WadingMobStepsOutOfWater`**
  （900f 涉水 mob 踏 1 格岸壁出池 x≈12.3/y=1.0 vs pre-fix 卡 9.7 互斥）
  = ENT-4 语义修订（门 94 S3 st1 历史 E2E 期望翻转，活契约 = 本测）
- **重钉 6**：mob 浮漂终态 y 3.0→**2.1 exact**（+inFluid=true）×3
  （SubmergedMobRisesAndFloatsAtSurface / FallingMobSinksToPoolFloor
  改 minY=1.0 潜水证明 + 终态游泳位 / SubmergedRiseIsFrameLocked
  Deterministic）+ item 终态 3.0→**2.875 exact**（W'−0.25/2 半浸没）×3
  （FloatsAtWaterSurface 含滑行 / SinksToPoolFloorWithEntryMomentum /
  FloatsInLavaSymmetric）
- **新增 5**（判别性，stash pre-fix 11 FAIL / 1 守卫 OK）：
  AirBandFallKeepsFullVy（干带 |vy|=5.0 exact 过带、水下钳 −4 = 降落伞
  回归钉）/ OneDeepPoolNoRiseWhenResting（1800f 站底不浮 = E4 主线
  守卫，pre-fix 亦过）/ TwoDeepPoolRisesWhenResting（池底 → 2.1 exact
  = 深度门）/ KnockbackSplashThenSwim（+3 击退 body 顶穿 W' 溅水、
  feet 恒 ≤2.1+0.025、终 2.1 = B1 全链）/ DryBodyAboveWaterNoRise
  （干带上浮门断 + 干带重力落全速 = ②①联合钉）
- **零改动全过**：test_player.cpp 全套（位级红线）/ WadesSlowerInWater
  ThanOnLand / OnLandMobIsFluidInert / DetectsFootBodyLavaAndDry /
  **MobFireTest 熔岩池全套**（2 深熔岩池 resting mob 现上浮至游泳位
  2.1，AABB 仍重叠顶层熔岩格 = P58 再点燃节拍不变）/ 投射物全套
- `-Werror` 0 警告

## 门 115

- **S1 PASS = 第 115 次过门，结构性免疫**：fresh 60f 无输入 new×2 RAW
  字节一致 + **new ≡ old（v0.31.0 二进制）字节一致**（9c64b0f8 四帧
  同 md5）；存档差 = 仅 fires remaining real-dt（PITS 244 非缺陷）。
  免疫论证 = 基线视图零水面（海在相机后方，PRE-3 先例）+ 60f 内零
  mob 入水（生成排除流体 headroom，门 94 先例）+ fresh 零掉落 →
  共享流体新分支（inFluid/depth/游泳位）对全部在场体结构惰性 + 干身
  W' 扫描读次数逐位不变。base18 锚未刷新（PITS 265 云带机器态当值
  = 可见态，锚仅作连续性参考，核心断言 = 同批同机 A/B）。
- **S2 判别场景 PASS**（zombie-only easy 夜 0.75，tm_scene_check v2 预校验，
  无输入 + MOBPROBE 帧探针【临时行已删净】，两场景 × 两侧 ×2 逐值一致）：
  - **A 深池 2（底 41/W'=43）+ 平崖 1 步下池**：old 终态 y=41.0 exact
    **池底涉水**（og=1 inf=1，depth 门缺 = 2 深与 1 深同样站底）vs new
    y=42.0999985 = **43−1.8×0.5 exact 游泳位**（og=0 inf=1 半浸没）——
    y 差 1.1 + og 互斥 + 存档侧证（old 玩家 5 击 hp 18.5 / new dy=1.1 超
    攻击垂直门零击 hp 20.0）。入池轨迹两侧同（301 首 inFluid、398 落底
    −4 钳）→ 分歧恰在 rise 门 = 修复点隔离。
  - **B 浅池 1（底 45/W'=46）+ 1 格岸壁**：old 涉水终态 y=45.0 exact
    恒卡池内（global 141-899 共 **759 帧零违规** x∈[14,18) = ENT-4
    `!m_inFluid` 踏阶禁令实证 = 用户"走入水出不来"本体）vs new **global
    545 踏壁顶**（y=46.0 exact og=1 inf=0）→ **global 566 x<14 达玩家侧**
    出池——FLUID-3 踏阶解禁实证。old/new 全程互斥。
  - **A2 降落伞段场景不可达（记档）**：spawn floor band 使崖缘顶恒 ≤
    水面，行走入池 vy=0 起步、无 y>W' 干带穿越（探针双侧 n(y>43)=0）——
    该修复由单测 AirBandFallKeepsFullVy 钉死（干带 |vy|=5.0 exact 过带 +
    水下钳 −4）+ 代码级论证（下沉 inFluid 门 = 玩家孪生 keep-vy 行对齐）；
    old 全程 max|vy|=4.0 亦实证水中钳制不变。
  - **B1 干身踩水面场景对行走 mob 不可达（记档）**：崖缘 1 步下池首 tick
    身体尾压崖缘 floor → fresh resting=TRUE → old rise（vy≥0 门）永不
    触发 = 行走 mob 物理到不了干身终态（仅击退上抛/预置可达，单测
    KnockbackSplashThenSwim 覆盖）。
  - 截图 4 张（a/b × old/new，1362×744 非黑验帧）：new 半浸没/壁顶出池
    vs old 池底涉水/卡壁。
- **S3 PASS**：1677/1677 + `-Werror` 0（独立重建复跑）。
- **S6 无**：零 parse 面（config/内容/格式全不动）。
- **性能 A/B PASS**（fall 900f，tm_v0320_src vs tm_v0310_src 插桩对，
  双侧 timing-clean）：三窗 fluidCells **123879/167781/236573 双侧逐位
  一致**（同世界）+ upd med 46.97/49.53/48.69 vs 46.89/49.04/50.60、
  render med 0.376/0.538/0.721 vs 0.363/0.540/0.705、swap/flush/stream/
  fluid 全同档（±8% 内，重跑噪声带）= **成本可忽略无回归**（fall 场景
  零流体交互实体 = FLUID-3 新分支结构惰性，玩家孪生位级不变）。

## 记档偏离

1. **item 半浸没**：掉落物浮面终态 ENT-4 干身 W' → 游泳位（W'−size/2，
   夹具 2.875），MC 同款观感。
2. **无 swim-up 爬岸**：游泳位 mob（onGround=false）无 step-up；只有
   站底/涉水体可踏 1 格岸壁出池。深水游泳出池 = 矩阵外残余。
3. **击退可溅出表面**：splash 是 **body 顶**穿 W'（zombie 游泳位头顶
   高出水面 0.9）；+3 冲量在流体内被 rise cap 1.5 截断，feet 永不过
   游泳位；干带 keep-vy 使落回体全速过带。
4. **ENT-4 语义修订**：门 94 S3 st1"卡水隙"旧语义翻转（历史 E2E 记录，
   活契约 = WadingMobStepsOutOfWater）。KNOWN_ISSUES:70 / BACKLOG:143
   同步修订。
5. **W' 语义演进**：浸没体 W' = 真水面（窗内最高 L8+1）取代瞬态面
   （脚下格顶）；干身体恒等不变；m_atSurface 带自此量自真水面。
6. **带内 hover 冻结**（preset 专属 artifact）：干身 atSurface 带 +
   vy=0 悬浮体被 keep-vy 冻结（无重力无浮力）——真实玩法不可达（带内
   到达必 vy<0 或自游泳位）；玩家孪生同 preset 会 snap 到脚面（接受
   的 Q2 偏离），mob 模型刻意不复制。
7. **窗深边界**：>32 格深列取窗内局部面（pre-FLUID-3 同边界）；
   shipped worldgen 熔岩湖 depth ≤16 校验，不受影响。
8. **游泳位与池底实体的垂直攻击门**（S2 A 实证）：2 深池游泳位 zombie
   （feet W'−0.9）对池底玩家（feet W'−2）dy=1.1 > 攻击垂直门 1.0 =
   **永不攻击**（旧行为 = 池底涉水互攻）。深水池底站立玩家对游泳 mob
   获得天然安全位——MC 无此效果（游泳 mob 可打池底），记档偏离，影响限
   深水池战斗边缘；浅池（≤1 深）行为不变（双方站底互攻）。

## 性能面

每实体每帧新增 = 游泳位 1 次减法 + 池深 1 次减法 + 深度门 1 次比较；
World 读：干身路径逐位零新增（上扫 inFluid 门控），浸没路径 ≤32 次同列
窗读（实际 1-2 次到面即止）；无新状态入档、无新 config、无 format 变更。

## 变更文件

- `src/entity/entity.{hpp,cpp}`（游泳位/深度门/三分支门控/W' 上扫）
- `src/entity/mob_entity.{hpp,cpp}`（step-up 门删 `!m_inFluid` + 文档）
- `src/entity/item_entity.hpp`（文档：半浸没终态）
- `tests/test_mob.cpp`（+5 新 / 重钉 3 / 反转 1）
- `tests/test_entity.cpp`（item 重钉 3）
- 本存档 + `docs/KNOWN_ISSUES.md`（ENT-4 条修订）+ `docs/BACKLOG.md`
  （ENT-4 行修订注）+ `AGENTS.md`（现状行）
