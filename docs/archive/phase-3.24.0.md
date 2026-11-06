# Phase v3.24.0 — V3-10b 末地内容族（门 214 全 PASS，2026-11-07）

## 范围与定案

**末地 10a 框架的最小 fill 换成真内容**（基岩洋 + 岛群 + chorus 装饰 +
末地石砖 + **项目首个投掷物 = 末影珍珠**）；规模 ≈ V3-5 建筑内容族。
**format 32 不变**（珍珠 = 物品 + Projectile 不入档）/ **sim 2 不
bump**（canary 4 全 OW 无珍珠）/ 零 config。

- **基岩洋**：全维 y[0,4] 基岩实心带（下界地板逐字镜像，非流体海
  = 零 churn）——MC 末地基岩海 y-60 的单带近似（项目无负 y，偏离
  记档）；**副作用**：跌落死因 Void→Fall（措辞在字体表内零字形）
  + 珍珠可落洋面 = MC 逃生面孪生。
- **岛群 pass**：lattice 16 逐格 roll（p 0.15，独立盐 kSaltEnd-
  Island）→ 岛心 = 格原点 + 半径 5..12 + 顶面 y 44..56（中央岛
  视平线带，雾 48 视界内）；形状 = 顶 2 层实心 + 下锥 2 层（r-1,
  r-2）悬浮岛（MC 孪生）；**air-only 谓词**（台地/洋/出口/平台
  自保）+ **中央岛足迹护栏**（bbox 触 |x|,|z|≤60 即弃——出口门 +
  入场平台区零岛）；跨 chunk 契约 = 走格外扩 s+rMax+2（R1，双
  chunk 填恒等单测）。
- **chorus 装饰**：每岛二次 roll（kSaltEndChorus 独立盐，p 0.6）→
  岛顶中心 1×1 chorus_plant（cross stateless 块，chance_drops ≈
  MC 0-2 果 + 0-1 株；**生长不做**——项目生长 tick 仅主世界，跨
  维生长 = 超内容族后移 10c/10d 记偏离）。
- **biomes_end 池**：单群系 end_plains（surface endstone = 台地
  no-op 覆写；价值 = S6 fail-fast 面 + 10d 第二群系零核心缝；MC
  末地无群系 = 偏离记档）。
- **end_stone_bricks**：full_cube 块（hardness 1.25 MC / drops
  自身 / stone 音族）+ 2×2 配方 count 4（MC 精确）。
- **末影珍珠全链**（项目首个投掷物）：
  - 数据：`ender_pearl`（max_stack **16 MC 精确** + `throws_
    teleport` strict bool + throw_sound/teleport_sound 配对 + 单盒
    model）；`chorus_fruit`（restore_hunger 2 / saturation 0.0 MC）。
  - 谓词 = 数据 flag（硬规则 3，名字零消费）+ **one-right-click
    纪律 8 面互斥**（food/drink/launcher/ignite/dye/bucket/shears/
    armor 全点名）+ material-only（tool 携旗点名）+ init 交叉检
    （双音缺一 = 死字段点名 / wav 缺失点名，shoot_sound 先例）。
  - 挂点 = RMB 链**潜行旋转后、placeBlock 前**（MC 语义：可交互块
    优先于投掷）。
  - 发射 = fireBow 范式：眼位前 0.3 + **投掷者速度并入**（MC 1.9+
    孪生）+ kPearlLaunchSpeed **30 b/s（MC 1.5 b/t 绝对值精确**——
    按箭 15 比例 = 7.5 仅 3 块不可航行弃）+ 冷却 **60f 会话态**
    （MC 1s 帧锁定）。
  - 弹道 = Projectile 尾参 `teleports`（缺省 false = 箭/火球位级
    红线）+ stuck 复用零改动；**落地 = pearlLanding**：catchUpDisk
    先行 + 落点 **2.5 球面均匀偏移**（粒子 counter 流 2 抽：u1 极
    余弦/u2 方位，帧锁定确定）+ hasRoomAt 上探 3（portal 先例）+
    restorePosition **fall 复锚**（落地消坠伤 MC 孪生）+ 落乘
    （骑乘投掷 MC 1.10 孪生）+ teleport 音空间化落点 + DEBUG
    「pearl teleport to (x,y,z)」；**probe 全败 = 珍珠吞没不瞬移**
    （封闭洞罕见，偏离记档）；**无自伤**（零新 DamageSource/死因/
    字体，MC 5HP 偏离记档）；mob 扫掠跳过扩门（穿 mob MC 孪生）；
    渲染 = 火球发光立方复用 0.625× scale + 珍珠青（stuck 即瞬移
    从不渲染）。
  - 声音：ender_pearl_throw（320→980 Hz 上扬 whoop + swish，0.18s）
    + ender_pearl_teleport（1400→240 Hz 坠落 shimmer + 2.02× 微光
    + 碎裂爆头，0.34s）→ **68→70** 幂等既有 68 件字节恒等。
- 块 131→133 / atlas 170→172（[0..1,11] 零移位）/ item 99→101 /
  配方 112→113 / 声音 68→70 / cutout 集 +1（chorus cross）。

## 实现（代码 12 + 数据 12 + 工具 3 + 测试 4 + CMakeLists 1）

- `src/world/end_gen.{hpp,cpp}`：6 名 create（biomes_end 池 + 空池
  拒 + surface 未知/有状态拒）+ endBiomeAt/endIslandAt/endChorusAt
  纯函数 + 4 pass fill（洋 → 台地 → 岛群 → 出口）+ 盐族 3 常量 +
  参数 12 常量。
- `src/item/item.{hpp,registry.{hpp,cpp}}`：throwsTeleport +
  throwSound/teleportSound + parseThrows（strict/配对/8 面互斥/
  material-only）+ 未知键白名单 +3。
- `src/entity/projectile.{hpp,cpp}`：尾参 teleports + 谓词。
- `src/core/game.hpp`：m_pearlThrowCooldownFrames + tryThrowPearl +
  pearlLanding 声明。
- `src/core/game_local.hpp`：kPearlLaunchSpeed/Cooldown/Offset 3 常量。
- `src/core/game_interaction.cpp`：tryThrowPearl + 挂点。
- `src/core/game_frame.cpp`：pearlLanding（uniform01 局部 + pearl-
  Offset 纯函数）+ 冷却递减帧。
- `src/core/game_entities.cpp`：stuck→pearlLanding 分支 + mob 跳过
  扩门 + drawArrows 珍珠分支。
- `src/core/game_init.cpp`：biomes_end 加载 fail-fast + EndGen
  6 名 + 珍珠声音交叉检循环。
- `src/entity/arrow_mesh.hpp`：珍珠常量（0.625 scale + 青色）。
- assets：biomes_end/end_plains + 2 块 + 2 物品 + 1 配方 + 2 纹理
  入生成器（盐 1076..1080 / 184..187 幂等）+ atlas 172 + 2 wav。
- tools：generate_sounds.py（+2）/ generate_item_icons.py（+2）/
  generate_block_textures.py（+2）。
- tests：test_end_gen.cpp 10b 扩（14：create 6 名 ×4 / biome 单 /
  岛 roll 确定界 / 护栏 / chorus 独立流 / 洋带 / 4 chunk oracle /
  跨 chunk 恒等 / 中央区无岛 spot / 出口 / 常量 pin / shipped ×2）+
  test_ender_pearl.cpp（6：解析/strict/tool 拒/8 面互斥/legacy 冷/
  Projectile 尾参）+ census re-pin 12 文件。

## 门 214（全 PASS）

- **S1 结构免疫第 95 次**：60f 无输入 ×2 ≡ **base19 字节恒等** +
  savenorm EXACT + 终档恰白名单 3 键（10a 的 end/end_entities 空段
  原样——10b 内容 = 生成参考零 diff 最强形 + 珍珠物品冷）。
- **S2a 去程臂**（10a demo 复跑）：12 嵌眼 → activated →
  `Traveled to end at (40, 53, 0) from (3, 54, 3)` ×2 逐字恒等 +
  截图 RAW 恒等（末地新内容：洋底/岛群/chorus 在雾内可见）。
- **S2b 珍珠臂**（authored demo：fresh 档热栏 16 珍珠 + f30 LMB
  捕获 + f60 RMB 投 + **f70 RMB 冷却窗内第二次 = 静默 no-op 实证**）：
  `threw a 'ender_pearl'` + `pearl teleport to (1.395792, 54.893940,
  -1.207714)` ×2 逐字恒等 + 截图 RAW 恒等 + **心行全满无自伤** +
  热栏 **16→15** crop 实证 + 落点视角位移。
- **S6 六面点名**（temp 资产法 + 手工还原 + grep 回验）：① biomes_
  end 缺失 → `end world generation requires at least one biome` ②
  群系 surface 未知 → `end biome 'end_plains' names an unknown
  surface block` ③ chorus_plant 缺失 → `missing block 'chorus_
  plant'` ④ pearl 缺一音 → `requires BOTH 'throw_sound' and
  'teleport_sound'` ⑤ wav 缺失 → `is missing 'sounds/ender_pearl_
  throw.wav'` ⑥ endstone stateful → `per-instance state` ENT-2。
- **S2b canary 4 demo** ×2 RAW + savenorm EXACT（**sim 2 不 bump
  实证**）。
- **S3 全套 2899/2899**（+14：EndGen 10b +8 / Pearl +6 / census
  re-pin），`-Werror` 0 警告。
- **性能 A/B（用户硬要求）**：旧 = d0c0435 v3.23.0（git archive）
  + g139 vs 新 = 工作树 rsync + g139（三形态锚——10b 不改流送调用
  点形态，fillEndChunk 新 pass = 生成一次性成本），900f 无输入
  tools/perf_gate_config.json 独占——banner 实证 v3.23.0/v3.24.0 +
  PERF-WIN 三窗 upd med 0.575/0.585/0.594 vs 0.569/0.587/0.597 +
  render med 0.662/0.716/0.684 vs 0.591/0.656/0.641 + swap med
  15.24/15.22/15.22 vs 15.38/15.28/15.23（vsync 地板同档）+
  fluid·fires·spread·ents·stream 同档 + **changesApplied 9/2/1 vs
  9/2/1 全逐位** + **终档 savenorm EXACT**（零 format = 最强形——
  10b 内容 = 末地惰性生成参考 + 珍珠物品冷，无输入全冷路径结构性
  零成本）。

## 门内捕获（PITS 406）

1. **lattice 位切 int 截断负值**：`static_cast<int>(h >> 24) % span`
   的 `h >> 24` 最高 2^40——直接 int 截断可负 → % 负余数 → 半径出带
   （-2..12 实测）；纪律 = **模运算先于 int 转换**（`(h >> 24) %
   span` 后再 cast）。
2. **JS 三元误写进 Python 纹理函数**（`? :` 非 Python）——生成器
   语法错被 md5 幂等纪律捕获前必须先 py_compile。
3. **GCC-16 maybe-uninitialized 假阳性第二例**（10a 的 appendEnd-
   Sections 结构隔离先例）：本次无新触发（end 段序列化 10a 已隔离；
   10b 零序列化面）。
4. **run 目录 stale assets 再踩**（g213 run 无 biomes_end → init
   rc 1 点名 = 纪律按预期工作；rm -rf + cp -r 重建）。

## 记档偏离（MC 语义）

- **基岩洋单带 y[0,4]**（MC -31 洋底 + -60 床底双带；项目无负
  y）。
- **跌落死因 Void→Fall**（洋底终结虚空坠落；措辞字体表内零字形）。
- **末地单群系**（MC 无群系 = 中央/外围地形概念；池 = 10d 缝）。
- **珍珠**：重力复用 kGravity 20（MC 0.03 b/t²=12，偏离）/ **无 5HP
  落地伤**（零新 DamageSource/死因/字体）/ 无 endermite 5%（10c
  末影人相）/ 无熔岩免疫特例（洋底 = 无熔岩）/ 不跨维（飞越 portal
  不做）/ 虚空条款无消费方（洋底恒落实体）。
- **chorus 无生长**（生长 tick 仅主世界；跨维生长 = 10c/10d）。
- **珍珠可得性 = 存档手术/creative**（末影人 10c 后移）。
- ender chest / dragon egg = 10c/10d。

## 变更文件（显式路径，33）

见 git commit。
