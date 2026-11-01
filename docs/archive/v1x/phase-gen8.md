# GEN-8 地形互感（v1.7.0 候选，门 122）

v1.x 任务矩阵第七行（docs/BACKLOG.md，GEN 行 2/3）。原始定义已佚（v1.0
砍掉清单 63 项只剩名称"地形互感"），代理定案最小完整版 = **山峰互感
（mountain passes）**：MC 地形互感的标志性机制——相邻山峰在低频山脊
掩膜驱动下合并成山脉，掩膜凹陷处留鞍部（"pass"）。

## 定案与废弃路线

- **首版局部峰设计废弃**（10-27 门 122 预备实证）：局部峰 = baseHeightAt
  严格 3×3 局部最大值 + 绝对高度门。shipped 高度噪声 scale 0.02 非常
  平滑，lround 整数舍入把穹顶顶拍平成 5-10 格宽**平顶**，严格局部最大
  恒不成立——probe 三 seed（123456789 64×64 零峰 / 9 一峰 / 42 零峰），
  5×5 平顶 apex 规则（ring-1 全 ≥ + 非严格时 ring-2 全 >）仍救不回
  （平顶宽 5-6 = ring-2 内仍有等值格）。
- **coarse ridge mask（定稿）**：独立低频 fbm2 场（`noise_scale`
  0.008 = 特征周期 ~125 块、3 octaves、kSaltPasses 专用 salt）标记
  山脉带；`mask > threshold` 时 `bonus = floor(amplitude × (mask −
  threshold)/(1 − threshold))`，0..amplitude **只升不降**；
  `heightAt = min(baseHeightAt + bonus, kWorldHeight − 16)`（baseHeightAt
  = 旧 heightAt 体逐位 = 代码级 off 免疫保证）。纯函数、零 chunk 数据
  = 跨 chunk 一致构造性成立（GEN-5 守卫同款纪律）；每列 1 次 fbm2（3
  octaves）= 生成成本可忽略。
- **shipped 参数**（探针标定，单轮零迭代定案）：{enabled true,
  noise_scale 0.008, threshold 0.55, amplitude 6}——mask 分布实测
  [0.07, 0.88]（p50 0.53 / p90 0.64 / p99 0.71），threshold 0.72 首选
  全世零 bonus（死参数）→ 0.55：bonus>0 要求 m > 0.55 + (1−t)/amp
  = 0.625，demo 区（出生 4×4 chunk）242 列 +1（全在 z≥0 = 基线视图
  后方 → S1 免重锚，见 E2E 节）、全世 ~10% 列 bonus≥2、最大 bonus 4
  （1200×1200 实测）。曾评估"高 threshold 保 demo 全零"路线（0.66）
  = 全世最大 +2 效果过弱弃用。
- **缺省 disabled = 位级 legacy**（段缺失 = enabled false；config 结构
  默认 = shipped 值）。
- **零 format**（世界生成演进 = 旧存档新山脉先例 P86/88/93/GEN-4/5/7）；
  零 config 键（worldgen.json 数据面）；零音效/内容/纹理。

## 实现

4 文件（worldgen_config.{hpp,cpp} + world_generator.{hpp,cpp} +
worldgen.json）+ 11 新单测 + 4 契约演进重钉（GEN-3 矿计数 3 值 +
GEN-4 洞穴探针 3 测）。

## 测试（1733/1733 = 1722 + 11，-Werror 0 警告）

- config 7：段缺失 = disabled+默认值 / 全字段解析 / noise_scale 0·0.06
  点名 / threshold 0·1·1.2 点名 / amplitude 65 点名 / enabled 非 bool
  与段非对象点名 / shipped 内容钉死（0.008/0.55/6）。
- 生成 4：disabled = bonus 0 + base 逐位 legacy + enable 只升
  （4096 列）/ mask 超阈区抬升 + bonus ≤ amplitude + heightAt =
  base+bonus 精确（128×128 整掩膜周期）/ 跨生成器确定性 + 种子敏感 /
  shipped 钉死（5 点 top-clamp 恒等 + 3 点 base/surface 精确值 +
  (62,36) demo 唯一 bonus=1 点 = 抬升本体钉死）。
- **契约演进重钉**（方向契约全保持）：GEN-3 矿计数（coal 3255→3337 /
  iron 1616→1653 / gravel 1003→1020 / diamond 96 不变——抬升列扩深石
  带 = 矿目标集微增）+ GEN-4 洞穴探针 3 测（96×96 三 seed 逐值表 +
  聚合 1045910/50155/61670 + 32×32 聚合 105219/4255/2——**final 胜
  legacy 的方向 pin 逐条全过**，4-6% 密度带 final 4.80%/32 样本 4.04%
  仍在带内）。

## E2E 门 122

- **S1 PASS = 第 122 次结构性免疫免重锚**：fresh 60f 三方 ×2（NEW
  仓库 / OLD v1.6.0 干净树 git archive / OFF passes-disabled scratch
  资产树）六跑 RAW 387726 B 全两两字节一致——**new ≡ base18 ≡
  old ≡ off**（savenorm EXACT）+ **off≡old 字节一致 = 代码级免疫**
  （passes off = 位级 legacy 的 E2E 证明）。免疫论证（探针定量）：
  演示区（出生 4×4 chunk，x,z ∈ [-64,64)）恰 **242 列 bonus>0，全部
  z≥0（x[48,64)×z[32,64) 两 16×16 格 = 93+149），z<0 侧 = 0**——基线
  视图 yaw 0 朝 -Z，变化列全在相机后方（GEN-6"基线视图朝 -Z 恒在
  种树侧"同款先例）。
- **S2 山脊判别场景**（玩家 (-304.5,35,560.5) 西望 x[-420,-370) z
  [552,640) 强脊簇——该处 mask bonus 2-4、全世最强山带（1200×1200
  实测 max 4 / 10.5% 列 bonus≥2）；player 格 bonus 0 = 两侧同地面
  同站位；600f 无输入 mobs off；场景含前景海盆 32926 流体编辑 =
  世界生成演进实证）：
  **PASS**：三方 ×2 RAW（old/off 353007 B / new 158892 B）+
  **off≡old 字节一致 = 代码级免疫**（passes off = 位级 legacy 的
  E2E 证明）+ **new vs old diff 525,506 px（51.9%）**：顶天带 y<60
  零 diff + diff 区 old 侧 30.5% 水域蓝 → new 侧 **0% 水域 + 100%
  地形** = 盆地抬升排水判别（新世山脉带把旧世水盆抬成陆地 = 特征
  本体）+ 双对 FORMAL PASS（g122_s2.py）。player 态两侧 EXACT
  （pos/yaw/pitch/health；hunger 17 vs 19 = real-dt 类 PITS 244 非
  特征差异）；世界 blocks/fluids 差异（37→38 / 36479→32926）= 世界
  演进（排水 + 反应位置漂移），off≡old 已证归因纯净。**坑**：首轮
  黑帧 = 双因叠加【Firefox 窗口标题含 "TokenMine" 与游戏窗同 tag
  平铺 → 游戏窗被分半 679×744（PITS 249/286 家族）+ 真黑窗相位
  22106 B/12037 B 交替】；修 = 跑前最小化浏览器窗 + 窗口识别按
  进程 cmdline（_NET_WM_PID 对 firefox 同名窗失效）+ 尺寸门
  (1362,744)+mean>40。
- **S3**：1733/1733。**S6**：无新增 parse 面（7 config 点名单测覆盖）。
- **性能 A/B（收）**：tm_v160_perf vs tm_v170_perf **同 seed 起跑**
  （perf_ab15 冻结态失效 = 其流体格在新世抬升后处实心【init
  "cannot apply save fluid edit at (-374,33,129)"】→ 剥离 fluids 重
  生成 seed，双侧各 900f）：fluidCells 51352/29700/29700 →
  51922/29803/29800（+1.1%/+0.35%/+0.34% = 世界演进）+ render med
  1.00/1.05/1.05 → 1.02/1.06/1.06 + swap 7.3/9.6/10.0 →
  2.8/8.5/9.4 + stream 1091/967/965 → 1015/938/986 ms（同档或更
  优）+ 每流体格成本同档（423/29700 = 14.2µs → 415/29800 =
  13.9µs）+ PIPE3 负载同档（visitedChunks 254→268 / changesApplied
  1383→1404）= **代码成本可忽略**（passes = 每列生成期 1 fbm2；
  fires/spread 大漂移 = 世界演进【熔岩湖相对新地形位置变】如实
  记档，GEN-4 先例）。

## 偏离/记档

- mask 是独立噪声场（非高度场的函数）= "山脉在哪"由第二信号决定，
  与 MC mountain pass 的"mountain noise 驱动"同构。
- 只升不降（pass 不挖谷）= 无河流/峡谷类互感（矩阵外）。
- 首版局部峰路线废弃记档（平顶教训 = PITS 候选 290：平滑噪声 +
  整数舍入下严格局部最大近乎不存在，特征检测须用独立低频场或宽容
  apex 规则；探针先行）。
- GEN-4 32×32 样本 final 密度 4.04% = 4-6% 带下缘（GEN-8 抬升扩 band
  所致；方向 pin 不受影响）。

## 坑位（PITS 追加）

- PITS 290（候选）：局部峰特征检测在平滑噪声场失效（lround 平顶）。
- raw string 吞 TEST 宏：`R"(json)))` 缺 `)"` 闭合 → 未终止 raw string
  吞并后续整行（含下一个 TEST( 宏）直到远处 `)"` 才终止 = 测试函数
  静默消失 + 上一个测试断言对垃圾 JSON 跑（门 122 预备实证：
  RejectsPassesEnabledWrongType 整体被吞，--gtest_list_tests 才发现）。
