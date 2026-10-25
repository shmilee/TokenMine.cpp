# Phase GEN-6 — 树放置半区缺失（v1.0 任务表 21/32 执行序第 32 项，门 103）

> 交付中上报的 Phase 4 起潜伏 bug（GEN-5 发现，10-25 用户定案立项 = 矩阵
> 第 32 项，执行序 SURV-5 后 = v0.20.0）：placeTrees/placeDecorations 的
> wz 循环上界误用 X 基 limit → cz ≥ cx+1 对角半区整块不种树/不放装饰。

## 根因

`src/world/world_generator.cpp`：`placeTrees`/`placeDecorations` 的
`const int limit = ox + kChunkSize [+ margin];` 被 wx 循环（正确）与内层
wz 循环（**错误**）共用：chunk 的 oz ≥ ox+19（树）/ ox+16（装饰）时，本
chunk 的 z 列不被迭代 → trunk 单列只由本列 chunk 写入 = **约世界一半
（cz ≥ cx+1 对角半区）永久无树无花**；装饰在 cz = cx+1 层同样整块缺失。
Phase 4（ae19893）起潜伏，101 道门未发现 = 基线视图朝 -Z（该侧 chunk 恒在
种树侧 cz ≤ cx）+ 生成器单测只断言局部区域。GEN-5 shipped 扫描（320×320
树生长侧盒）首次量化：修复侧 346 树列正常。

## 实现内容

1. **wz 上界改 Z 基（2 处）**：
   - `placeTrees`：`const int zlimit = oz + kChunkSize + margin;`（margin 对
     wz 无意义时 zlimit == chunk 自身 z 跨度），wz 循环 `wz < limit` →
     `wz < zlimit`
   - `placeDecorations`：`const int zlimit = oz + kChunkSize;` 同款
   - **wx 循环 / margin / 格网 / roll / GEN-5 守卫逻辑零改动**（diff 仅 2 处
     边界 + 注释，src +11/−2）
2. **跨 chunk 无缝契约保持**：trunk 单写者（属主 chunk）+ 冠 margin 区多写
   幂等 = 既有范式不变；修复只是让属主 chunk 真正迭代到自己的 z 列
3. **零 format / 零 config / 零 content**（纯核心修复）。世界生成演进 =
   受影响半区大量 chunk 新增树/装饰（旧存档新地形先例：P86/88/93 +
   GEN-4/5 同款）

## 变更文件

- `src/world/world_generator.cpp`（2 处 wz 上界，+11/−2）
- `tests/test_world_generator.cpp`（+342：GEN-6 节 = 2 判别性测试 + 5 helper）
- 探针 `/tmp/opencode/g103_count.cpp`（不入套件，GEN-4 cave_probe 范式：
  真实资产 shipped seed 123456789 A/B 计数，old 计数 = 精确子集论证
  【trunk 单写者 + 判定纯函数不变 ⇒ old ⇔ wz ≤ 16cx+16（树）/ cz ≤ cx
  （装饰）】，×2 运行输出字节一致）

## 测试（1521/1521，+2 新，无重钉）

1. `Gen6AffectedSideTreeColumnGrows` — 探针式搜索受影响侧（cz ≥ cx+1 且 wz
   过 pre-fix 上界）过 roll 树列（trunk 在西缘 wx%16==0 使冠跨 chunk seam）
   → 断言 ① 属主 chunk **单独**生成出连续 trunk（单写者；修复前此 chunk
   从不迭代该列）② 盒内冠模板全格无洞 + seam 叶（wx−1 西邻 chunk 独有——
   修复前该邻 chunk 零 z 列迭代）③ 西邻 chunk standalone 生成与盒逐格一致
   （无重复/无丢失）④ seam 格在属主 chunk 内为 air
2. `Gen6AffectedSideDecorationPlaced` — 受影响侧（cz ≥ cx+1，pre-fix 整
   chunk 零 z 列迭代）过装饰 roll 且无同列 trunk 的格 → 属主 chunk 单独
   放置 + 盒一致，装饰恰在 biome 表面格上方 1
- **判别性实证**：`git stash` 还原修复 → 2 新测**均 FAIL**（400 seed 全搜无
  case = pre-fix 该 case 不存在）→ 恢复 → PASS（1.1s/0.7s）
- **既有契约零重钉**：seed 42 fixture 区新增 1 树 + 13 装饰后既有断言全为
  下界 GT（wood>10 / leaves>30 / flowers>0 / cactus>0）或逐格不变式
  （trunk 连续 / 装饰恰 h+1 / 在表面上）自动成立；跨 chunk 无缝测试
  （x=16 边界，cz≤cx 种树侧）与 GEN-5 测试组（扫描区全 cz≤cx）、
  RealWorldgenPlacesNoStatefulBlock 均不受影响 = 全量 1521 实证

## 探针量化（shipped seed 123456789，真实资产）

- **门场景盒 x[0,128) z[16,128)**（与 g103_cols 预计算同盒）：树列
  **13→33（+20，2.54×）**、wood 70→186（+116）、leaves 1667（新）、装饰格
  **5→20（+15）**；20 gained 列全清单与 g103_cols 23 列**完全自洽**
  （20 gained + 1 old【cz=cx+1 且 wz=16cx+16，pre-fix 边界恰迭代】+ 2
  GEN-5 guard 跳过【(12,120)/(24,124) 表面被洞吃，修复后仍跳过 = 天然
  对照】）
- **演示盒 |chunk|≤2**：树列 24→30（+6）、wood 138→171、装饰格 4→6（+2）；
  gained = (−32,−8) h57 / (−32,16) h50 / (−28,12) h53 / (−24,4) h58 /
  (−8,4) h52 / (−4,20) h51 + 装饰 (−20,59,−4)/(−8,44,28)
- **S1 视锥判定**（spawn (0,0) h53、眼 (0.5,55.62,0.5)、yaw 0 朝 −Z、
  pitch −0.25、水平半 FOV = atan(tan35°×16/9) = **51.2°**）：60f fresh 无
  输入视图 = 演示区 chunk −2..1（idle 不流送，PIPE-1 论证同款）——区内
  gained 5 特征**全部在视锥外**（4 个在相机后方 z≥4、2 个前瞻者方位角
  75.4°/77.6° > 51.2°）→ **初判结构性免疫免重锚**（像素 A/B 终裁）
- **前瞻半区**（cz<0，解析枚举不生成 chunk）：全图 gained 树列 13505，其中
  **215 列在基线视锥内**（距离 156–1011，最小 3 列 [150,200)：(−120,−100)
  h35 / (−132,−124) h37 / (−152,−128) h51），全部分布视锥**左缘走廊**
  （方位 45°–51.2° = cz≥cx+1 受影响带在前瞻半区的交线；右半区
  cz<0<cx+1 恒不受影响）——不在 60f fresh 视图内（idle 不流送），属游戏内
  视野 + 性能 A/B 输入

## 门 103

- **S1 PASS = 第 103 次结构性免疫**（10-25 21:0x，fresh 60f ×2 RAW 字节一致
  + vs base18 **字节一致** = 无需重锚；探针视锥论证与像素终裁互证）
- **S2 树群判别场景 PASS（补跑收 10-25 23:2x，retry v3 att 28）**：
  场景 = 玩家 (10.5,42,30.5)（(10,30) h41 站立）、yaw 2.356 斜视 wz 半区
  新增树群（(8,32) h40 t7 = 视轴上 2.8 块处，old 无树/new 有树）、pitch
  −0.15、600f 无输入；old = frozen v0.19.0 / new = frozen v0.20.0，
  **×2 RAW 双侧字节一致** + **A/B diff 476,835 world px**（质心 (923,438)
  视轴树投影区；颜色定案 = NEW 侧 **叶绿 44% + 干褐 55%**【树干 + 树冠】
  vs OLD 侧草绿/天空/地褐 = **树添加像素定案**，GEN-5 S2 树移除定案的
  镜像）。提交时首轮窗几何混合 PITS 249（perf stabilize_window 改平铺态
  → 1362/679 跨几何 RAW 对失效）→ retry v3 = frozen 二进制 + good()
  尺寸门 + 四帧几何一致校验
- **S3** = 套件 1521/1521（+2 新测含跨 chunk seam 一致性）PASS
- **性能 A/B v0.19.0（发布前必做，10-25 23:0x，timing-clean）**：
  - **fall 900f 三窗**（mobs off 标准场景）：**fluidCells
    123879/167781/236573 双侧逐位一致（同世界）**；upd med 74.3/74.6/85.2
    vs old 62.8/73.3/73.4、render 0.281/0.411/0.548 vs 0.321/0.411/0.561、
    swap 0.356/0.454/0.519 vs 0.434/0.386/0.490 = 全同档
  - **tree 场景 900f 三窗**（S2 同款站位 (10.5,42,30.5)，wz 半区新增树群
    实际负载）：**fluidCells 119826/162475/250678 双侧逐位一致**；upd
    med 65.2/73.2/68.6 vs old 70.7/73.0/51.8、render 0.300/0.442/0.607
    vs 0.274/0.410/0.622、swap 0.425/0.505/0.481 vs 0.381/0.438/0.474、
    ents 0.35/0.47/0.36 vs 0.35/0.36/0.32 = 全同档
  - **结论：GEN-6 成本可忽略，无回归**（2 行纯 worldgen 变更零每帧代码；
    世界演进树密度 2.54× 的渲染/流送成本在环境波动带内不可见）；黑相
    3/4 run 截图黑但 swap med 干净 = timing-clean 协议（gate98 同款先例）

## 偏离 / 残余

- 前瞻半区 215 列锥内树（游戏内视野）= 世界生成演进预期结果，非缺陷
- 旧存档（format 18）加载后 = 新洞形/新树先例：已加载 chunk 保持旧状
  （生成时快照），未加载区域按新代码生成 = 既有 P86/88/93 语义，无迁移
