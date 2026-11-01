# ENT-9 掉落可视化（v1.x 16/33，v1.16.0，门 131，ENT 行 5/5 收尾）

v1.x 任务矩阵第十六项。原始定义已佚 = 代理定案最小完整版：**全量掉落
可见（纯内容，零核心代码）**——CRAFT-2 后 63 件非 block 无 model 物品
的掉落不可见（含 mob 掉落 rotten_flesh/bone/arrow/feather/beef/mutton/
leather + 所有可 Q 丢弃物：食材/锭/矿/染料/盔甲/桶/药水/瓶），解法 =
给全部 63 件加 `model` 段（texture = 既有 icon PNG `<id>.png`，零新纹理
免重锚；CRAFT-2 渲染路径自动覆盖手持/世界掉落/进食三上下文）。与
PRE-12 边界：ENT-9 = 掉落物本体可见（内容）；PRE-12 = 通用
tooltip/F3 框架（UI，后续行）。

## 定案

- **model 集合 16 → 全 79 件非 block 物品**：assets/data/items 目录即
  全非 block 集（block 物品由块数据合成、无 item JSON，P61 先例）→
  契约 = "每个 item 文件都有 model"。
- **家族几何**（单位空间 0..1，centered ~0.5，1-3 box 剪影）：ingot
  1 扁板 / gem 1 块 / chunk 1 块 / dye 1 薄纸 / sheet 1 / bread 1 /
  meat 1 / wheat 1 竖束 / stick 1 细杆 / bone 3（横杆 + 两端骨节）/
  rotten 2 团 / feather 1 薄片 / arrow 3（杆 + 头 + 羽）/ bucket 1 /
  bottle 2（瓶身 + 颈）/ potion 2 / flint_and_steel 2 / armor 4 槽位
  各 1（helmet/chest/legs/boots 剪影）/ **master_bow = bow.json 3 元素
  结构复用**（texture 换 "master_bow"）。
- **texture 纪律 = 既有 icon**：`model.texture` 恒 = 物品自身 icon 名
  （init 交叉校验文件存在 = CRAFT-2 grows_to 纪律）；零新 PNG、atlas
  零触碰、免重锚。
- **共面 Z-fight 修正**（首轮套件揪出，CRAFT-2
  ShippedModelsHaveNoFaceCoincidentBoxes 守护）：bottle/potion 的颈
  from-y 与瓶身 to-y 共面 + 面积重叠 = 同面双绘 → 颈底埋入瓶身
  （0.62→0.50）；arrow 头 from-y 共面 → 埋入杆（0.80→0.72）；
  rotten 两团共 z 面 → 二团收边（0.70→0.62）。相交（apple 柄先例）
  合法，共面非法。
- **零核心代码**：src/ 零变更（ENT-8 后代码位级同 = 性能 A/B 最强形）；
  零 format（19）/config/音效/新纹理。

## 测试（1803/1803，-Werror 0 警告）

- `ItemModelTest.ModelItemSetIsExactly16` 契约演进 →
  **`AllShippedItemsHaveModels`**（目录全 79 件都有 model +
  texture==自身 icon + 元素非空，集合逐名钉死）+ 新
  **`Ent9FamilyGeometrySpotChecks`**（12 家族元素数抽查：arrow 3 /
  bone 3 / master_bow 3 / potion 2 / bottle 2 / fands 2 / rotten 2 /
  dye 1 / helmet 1 / ingot 1 / stick 1 / wheat 1）。
- `ItemModelMeshTest.ShippedModelsHaveNoFaceCoincidentBoxes` 契约演进：
  modelCount 16 → 79（Z-fight 守护对新 63 件全量生效）。

## E2E 门 131

场景 s131_room = s129 已验证平台（x22-33 z-35..0 地板 y44 + air carve
y45-48）围合成密封房（x21/x34/z-36/z1 石墙 y44-48 + 房顶 432
glowstone【air carve 占位改写，首轮去重坑】）+ 10 件预置 item 实体
（5×2 网格，全 ENT-9 增益件：rotten_flesh/beef/coal/iron_ingot/
dye_red + wheat/bone/arrow/potion_healing/glass_bottle）+ slot0
iron_ingot【手持上下文 = 仅指针锁定时渲染（UXFIX-1 纪律），无输入跑
  结构不可见 = 偏离记档：手持/进食的 model 分支 = CRAFT-2 既有机制
  同款（16 件先例），本相由套件 + 该机制先例覆盖】+ 玩家 (27.5,45,-17)
yaw0 pitch-0.3 + 600f 无输入。

- **S1 = 第 131 次结构性免疫免重锚**：fresh 60f 四方 ×2（new v1.16.0
  / old v1.15.0 = c1f9d31，g118_nomobs）387726 B 全字节一致
  （new ≡ old ≡ base18）+ savenorm new×2 / old×2 EXACT。论证 =
  手持 slot0 = stone（block 立方路径位级不变）+ 增益件全在隐藏
  starter 槽 17-26（不渲染）+ fresh 零掉落 + 模型网格惰性
  build-once 缓存（未用 = 未建）。
- **S2 密封房 A/B PASS**：×2 RAW/侧（new 271760 / old 264611）+
  savenorm ×2/侧 + **跨侧 savenorm EXACT**（实体物理位级一致 =
  纯渲染实证，CRAFT-2 先例）+ A/B 像素 diff = **684 px 全限世界带
  y[308..380]（10 掉落 box；冒烟 300f/600f 双帧定 672/684 = 帧锁定
  确定性）**+ 心带 y[646,661]×x[551,729] 零 + 热栏带
  y[660,720]×x[440,840] 零（HUD 2D 图标零变化纪律）。
- **S3 = 套件 1803**。无 S6（零 parse 面，CRAFT-2 同款先例）。
- 场景坑：**PITS 305**（air carve 已占位的房顶格被 add() 去重跳过
  = glowstone 静默丢失、房间恒暗、A/B diff≈0——builder 须支持
  overwrite + 落盘后重读断言）。

## 性能 A/B（发布前必做）

tm_v115_perf（v1.15.0 插桩）vs tm_v116_perf（v1.16.0-dev 插桩 =
**同插桩树 + ENT-9 变更文件**——ENT-9 零 src 变更 ⇒ 两树 src 位级同，
差异仅 assets/测试/版本串 = 最强性能形），同 seed（perf_ab17 剥
fluids → gperf_ab22.json）fall 900f 三窗：模型网格惰性（fall 场景
零掉落 = 零 model build）⇒ fluidCells 应逐位一致 + 全指标同档。

**PASS = 成本可忽略（零代码）**：fluidCells win1 51926 逐位 +
win2/3 29803/29800 vs 29703/29700（**±100 = PITS 283 rim churn 相
位类，非回归**——裁定链比门 130 更强：① 两性能树 src **位级一致**
（ENT-9 零 src 变更，仅 assets/测试/版本串差）⇒ 相位差绝不来自代码
行为 ② **同二进制 v115perf 自分歧**（门 130 两跑 29703/29700、门
131 跑 29803/29800）③ 终档流体差 3 格全在程序化熔岩湖 rim 簇
(81..84,36,73..77)，fires 50 = 50）+ PIPE3 changesApplied
1404 = 1404 + fluidCellsSeen 43048/43999（churn 类）+ 全指标同档
= 结构性惰性实证。

## 发布

v1.16.0 = 新组 1.16-1.20 首版（组内不打 tag；组末 v1.20.0 → tag +
GitHub release，notes 覆盖 v1.16-1.20）。正确性 A/B 旧侧 = v1.15.0 =
c1f9d31（tm_v115_src）。
