# Phase GEN-3 — 矿脉形态（v1.0 任务表 9/29）

> 设计案 = ora-2（oracle）定稿 + 代理开放问题拍板（用户夜间授权"按建议的来"）。
> 随机游走脉 → MC 3×2×1 旋转椭球脉（GEN 行末项）。

## 一句话定案

**随机游走 → MC 3×2×1 旋转椭球单脉**（`planOreVein` 纯函数：6 轴置换
取向 × 整数椭圆谓词 `4u²+9v²+36w²≤36` = **21 格/椭球**，新盐 PITS #4
推进纪律；`vein_size` 语义演进为**椭球簇数 K ∈ [1,8]**（kMaxOreVeinClusters，
shipped 全 K=1）；placeOres 其余骨架【lattice 8 格网 roll / 概率门 / 格内
均匀起点 / deep-only 替换 / maxY 逐格丢弃 / 跨 chunk seam 契约】**逐位零改**）
+ **gravel 同改** + **一轮密度标定**（探针新旧格数比，预测 coal ~2.3× /
diamond ~4.2× / gravel ~5.2× 偏密 → 概率降档 re-tune 一次定稿，目标比值
∈ [0.8,1.25]）；**caves 先挖 ores 后换石顺序与 deep-only 不动 = GEN-1 洞壁
暴露契约保持**；**门 87 = 结构性免疫不重锚**；新增单测 ~15 条，E2E 今晚
10 min 无输入（S1 门 / S2 洞壁矿 A/B / S6 vein_size 越界）。

## 开放问题拍板（设计 §9，按推荐通过）

- **Q1 gravel = 同改椭球**（单一形状函数覆盖 veins 全列表；"石替换"是替换
  目标属性非形状属性，MC gravel 亦块状；否决 per-entry shape flag 配置面）。
- **Q2 shipped 簇数 K = 全 1**（单 re-tune 轴；21 格脉已比旧 9 格游走
  chunky 得多；K>1 机制保留测试覆盖，未来内容调档零核心改动）。
- **Q3 vein_size 范围 = [1,8]**（kMaxOreVeinClusters 命名常量；8 簇 = 168 格
  饱和上限；旧 [1,32] 游走格数语义已废）。

## 现状锚点（设计核实，file:line）

- placeOres（world_generator.cpp:358-415）：lattice s=8 格网 + margin =
  maxVeinSize+s；逐矿盐 `(seed ^ kSaltOre) ^ (t+1)*kFeatureStep` → hash3 →
  概率门 → 胞内均匀起点 → 26 邻随机游走 vein_size 步（splitmix64 推进）。
- placeOre（417-430）：y ∈ [1,maxY] + insideChunk + **只替换 m_deep**
  （cave air/表层/流体/bedrock 全跳过 = 洞壁矿暴露自然机制）。
- shipped（worldgen.json ores.veins）：coal 0.4/9/80、iron 0.25/8/56、
  gold 0.12/7/36、diamond 0.05/5/20、lapis 0.08/6/32、gravel 0.25/4/80。
- pass 顺序（GEN-1 后）：fill → carveCaves → placeOres → placeTrees →
  decorations → lakes。**矿在洞之后 = 洞壁矿暴露契约**。
- 跨 chunk seam 契约：世界坐标格网独立 per-chunk 求值 + per-cell 写。

## In / Out 清单

### In

1. **src/world/ore_vein.{hpp,cpp}（新）**：`planOreVein(seed, origin, clusters)
   → vector<BlockPos>`（世界坐标偏移格集纯函数）+ constexpr 6 取向表 +
   整数椭圆谓词 + 命名常量 kOreVeinCellCount=21 / kOreVeinMaxAxis=3 /
   kOreVeinClusterSpread=2。
2. **placeOres 重接**：游走循环 → planOreVein（同既有盐流）+ 逐格 placeOre
   （417 函数零改动）；margin 公式 → `s + 3 + 2*(maxClusters-1)`（K=1
   shipped → 11 < 现 17 = 扫描略降）；概率门/起点/盐流逐位保留。
3. **vein_size 语义演进**：游走格数 → 椭球簇数 K ∈ [1,8]；worldgen.json 6
   条 re-tune（K=1 + 概率降档，标定后定稿）；maxY/lattice/probability 语义
   不变。
4. **gravel 同改**（Q1）。
5. **测试 ~15 + 文档**（MECHANICS 矿脉节 / CONTENT_GUIDE ore 段 /
   worldgen.json 注释）。

### Out

- 多簇 shipped（机制 + 测试有，shipped K=1；coal/iron 大簇 = 未来内容调档）。
- 新矿种 / 深度分层重排 / 巨型脉事件。
- gravel 独立形状 flag（YAGNI）。
- caves 顺序 / deep-only / maxY 语义（零改动）。
- 存档 / 光照 / mesh（矿 = 生成地形既有机制）。

## planOreVein 定义

整数 3×2×1 椭球（MC Java 矿石形状）：半轴 (3,2,1) 按 6 轴置换定向；格集 =
{(u,v,w) : 4u²+9v²+36w²≤36} = **恰 21 格**（w=0 面 19 + 端帽 (0,0,±1) 2；
边界钉死：(3,0,0)∈ (0,2,0)∈ (0,0,1)∈ / (3,1,0)∉ (0,2,1)∉）。中心对称 →
取向 = 6 态无独立镜像。确定性流：state = hash3(origin, seed)；每次取数
state += kFeatureStep 后 splitmix64（computeProceduralLakes 同款，PITS #4）；
取数 1 = orientation v%6；簇 k≥2 中心偏移 = 前簇 + (v%5−2)³ ∈ [−2,2]³。
输出世界坐标格集，簇序 + 固定格表序，重叠去重保首现。

### 标定（step-0，参数定稿前）

固定 seed 123456789 + 采样区（列 4..11 × 0..7 = 1024 列全高）：旧基线
（HEAD worktree）vs 新同参数 → 比值表 → re-tune 一轮（probability_new =
old/比值 3 位整洁值）→ 复测 per-ore 比值 ∈ [0.8,1.25]（残留 >1.25 记档
接受，单轮纪律）。附暴露率（cave air 6-邻占比，S2 选点依据）+ 定稿常量
（测试钉死）。预测定稿：coal ~0.17 / iron ~0.10 / gold ~0.04 / diamond
~0.012 / lapis ~0.023 / gravel ~0.05（全 K=1，maxY 不变）。

## 测试计划（~15 新，存量零改动）

- planOreVein 纯函数（新 test_ore_vein.cpp）：21 格 + 边界逐个钉死 / 6 取向
  全现 + 轴置换像 / 确定性 / 跨 seed 不同 / K=2 界 21..42 / 偏移 ≤ margin。
- 生成器集成（test_world_generator）：定稿 per-ore 格数钉死（四矿）/ 密度带
  [0.75,1.3] 软守卫 / maxY 丢弃 / deep-only 回归 / 跨 chunk seam 一致 /
  gravel 椭球形态。
- 解析（test_worldgen_config）：vein_size 0/9 拒、1/8 合法 / shipped 6 条
  定稿值精确钉死（P55 范式）。

## E2E 计划（今晚 10 min 无输入）

- **S1 门 87（免疫）**（~2 min）：fresh 60f ×2 + 与门 86 锚字节一致 + 红心 0
  + skygate。论证 = 视图 ⊆ 演示区 + 演示区无洞（clear 区）+ 矿只替换 deep
  层 = 矿形变化零可见路径。
- **S2 洞壁矿场景**（~5 min）：scratch config（vd 10 / day 3600 / mobs off）
  + 探针选点（cave air 6-邻有矿 + 视线路径无遮挡）→ 600f ×2 RAW（确定性）
  + **新 vs 旧二进制 A/B**（同存档各生成各脉 → diff 限定世界区 = 脉形变化
  视觉实证）+ 场景内矿格计数探针。
- **S6 init 失败**（~1 min）：vein_size 9 → 点名拒（[1,8] 新范围 E2E 背书）。
- **晨间可选（输入）**：玩家挖暴露矿脉（×2 savenorm + EXACT，回归性质）。

## 风险清单

1. 标定残留：单轮后个别矿越带 → 记档接受 + 软带测试防漂移。
2. 矿感变化：脉更大更稀（21 格/脉 vs 9 格游走，概率 ~÷2.3）= MC 1.18+ 矿感
   文档化；diamond 单次收益大。
3. 旧存档：未编辑 deep 石按新脉形生成（GEN-1 同款 worldgen 演进条）。
4. maxY 边界丢弃：椭球顶格丢弃（placeOre 零改动）；探针按有效格数计。
5. seam：plan 必须世界坐标起点（现状已是）——测试 11 钉死。
6. -Werror：整数谓词 + constexpr 表。
7. GEN-2 并行零文件交集（已核：GEN-3 = ore_vein 新文件 + placeOres 段 +
   worldgen.json ores 段 + worldgen_config ore 校验）。
8. margin 17→11：微 perf 收益（step-0 顺带报告 gen ms/列）。

## 构建与验证结果（E2E）

- **实测**：1297/1297（存量 1285 零回归 + 12 新 + 2 演进【FullOverride 新域 /
  Phase59GravelVeinContent 定稿值】），干净重建 0 警告。14 文件
  （+490/−55 + 3 新文件）。
- **标定（单轮定稿）**：探针（seed 123456789、列 4..11×0..7、64 列全高、
  真实 assets）；旧基线 v0.4.0 随机游走：coal 3206 / iron 1908 / gold 755 /
  diamond 142 / lapis 347 / gravel 1007（总 7365）。新形同参 pass1 比值
  2.6-5.6× → 概率降档定稿（0.154/0.0855/0.0376/0.0101/0.0208/0.0445，全
  K=1，maxY 不变）→ pass2 复测：coal 3397 (1.060×) / iron 1670 (0.875×) /
  gold 760 (1.007×) / gravel 1080 (1.073×) ✓ 带内；**diamond 91 (0.641×) /
  lapis 254 (0.732×) 残留记档**（低概率下格网通过集换血 + maxY/cave 截断
  非线性；单轮纪律不再调，软带测试按钉死值防漂移）。
- **S1 base18 门 87 = 结构性免疫（不重锚）**：fresh 60f ×2 全帧字节一致 +
  **与门 84 锚（s1_l2c_r1.png）字节一致**（门 85/86/87 连锁）+ 红心 0 +
  skygate 75,348 严格。论证兑现：视图 ⊆ 演示区 + 演示区无洞（clear 区）+
  矿只替换 deep 层 = 矿形变化零可见路径。
- **S2 洞壁矿 A/B**：gen3_s2scan 探针选点（暴露矿格 + 水平视廊 + 洞底落地 +
  视线全 air）= coal (106,17,108)、相机 (109.5,17,108.5) yaw π/2 pitch
  −0.3573；**场景火把 (108,18,108) 照明**（GEN-1 深洞矿格光 ≈0 不可见——
  探针加 light ≥9 过滤后 64 列区零候选 = 洞穴无天光可达矿，火把 = 同场景
  新旧公平照明）→ 新 ×2 RAW 字节一致（静态）+ **新 vs 旧（v0.3.0 随机
  游走，TM_REPO v0.3.0 资产 scratch 跑）全帧 diff 6,703 px（中央 4,659）=
  脉形变化视觉实证**（同洞穴几何、同火把、矿布局不同）。
- **S6 init 失败**：scratch worldgen `veins[0].vein_size=9` → **rc=1 + 点名**
  `'vein_size' must be in [1, 8]`（新域 E2E 背书）。
- **PITS +1（#209）**：旧二进制 A/B 必须跑旧版资产 scratch + TM_REPO 覆盖
  （内容契约演进单向——新 sapling JSON 被旧 registry 拒 = 3 连 init 失败）；
  次坑 = 洞穴场景相机 pitch 须按真眼高（feet+1.62）算视线（探针 +0.62 误判
  → 矿被石檐挡、全帧石墙 diff=0）。
- **晨间可选（输入）**：玩家挖暴露矿脉（×2 savenorm + EXACT，回归性质）。
