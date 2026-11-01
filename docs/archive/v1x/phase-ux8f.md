# Phase UX-8f — 背包 3 行 36 槽（v1.46.0，门 166 = UX-8 系统收官）

> 消费用户 11-01 定案「背包可以增加更多行，毕竟按 E 之后，我看到
> 背包下面还有很多空间。行数多一些，与右侧的两列配方的高度相比，
> 更和谐一些。」**v1.46.0 = UX-8 体验细化系统收官版**（tag v1.46.0 +
> UX-8 系统 GitHub release，notes 覆盖 v1.42-1.46；10-31 政策 tag 1:1
> 对应 release）。

## 定案

MC 标准背包 = **存储行 2→3、总槽 27→36（27 存储 + 9 热栏）**：

1. `Inventory::kTotalSize` 27→36（inventory.hpp，构造默认/size() 全
   自适应）；`collectInventoryInto` 双击分区 / shift 快捷移动 / 取半 /
   数字键**全 size 驱动零代码改动**（仅注释 9..26→9..35；MC 分区
   语义不变：hotbar 0..8 / storage 9..35）。
2. **布局 4 行**：新派生常量 `kStorageRows =
   (kTotalSize−kHotbarSize)/kColumns`（static_assert 整行，零字面量
   行号）+ `kGridRows = kStorageRows + 1` = 4——E 面板 gridH 3→4 行
   + **炉子面板同款**（FurnaceLayout 原按 9×3 渲染全 27 库存，跟进
   9×4）；行主序映射 slot i → row (i−9)/9, col (i−9)%9（底行热栏
   0..8，上三存储行 27..35 / 18..26 / 9..17）。**shipped E 面板
   listH 872 恒主导 → 原生高 896 不变**（用户诉求 = 网格填满面板左
   区与配方列同高，而非面板变高）。
3. **存档 format 23→24**：写恒 36 槽；读 per-format 尺寸检查（24 须
   恰 36 / ≤23 须恰 27，错计数双向点名）+ **尾部补 9 空槽迁移**
   （原 27 槽逐槽值保真，27..35 恒空——SAVE-2 velocity-zeroed 迁移
   同款纪律）；`kPreUx8fInventorySlots = 27` 命名常量。
4. starter 范围 [9,26]→[9,35] **经 kTotalSize 派生零代码改动**
   （shipped starter 16-26 全在 legacy 区 = 内容零改动）。
5. 零 config/内容/资产/音效。

## 测试

2053/2053（净 +4 = SaveFormat24Test 套件）、`-Werror` 0 警告、
banner v1.46.0：

- SaveFormat24Test ×4：36 槽往返含新行堆 / f23 读补 36 值保真 +
  27..35 恒空 / pre-24 携 36 条点名 "27" / f24 携 27 条点名 "36"
- 契约演进：4 个 test 文件 42 处 format 降级位接入 `downgradeSlots`
  助手（27 条截断）+ 13 处 format 钉 23→24 + InventoryLayout 4 行
  几何 / FurnaceLayout 全重钉（冶炼区 4 矩形零变化）/ 双击分区加
  slot 31 新行同区 / StarterSlots 上界 35 合法 36 拒
- headless 独立驱动冒烟 10/10（真实注册表 f23 读→补 36→回写 f24 +
  版本谎报负对照）

## 门 166（编排者执行，old 侧 = v1.45.0 = ddf9935 = tm_v146_base）

- **S1 = 第 166 次结构性免疫 + base18 锚连续 168140 B 免重锚**：fresh
  60f 四方 ×2（new×2 + old×2）全字节一致 + **跨侧 RAW 字节一致**
  （无面板 = 零 layout 执行；+9 空槽零渲染；手持热栏 9 槽不变）+
  跨版 savenorm diff 恰 2 项 {format 24/23, slots len 36/27}——
  **占用 20 槽逐值恒等 + 尾部 9 恒空 = 迁移语义实证**。
- **S2 面板 A/B PASS**（E 键开面板 600f，×2 RAW 双侧
  【new 209841 B / old 211915 B 字节一致】+ 跨侧 savenorm 唯一 diff
  = 远端熔岩湖 fire/fluids churn【同二进制自分歧类 PITS 283，
  inventory 占用段恒等】+ **肉眼终裁**：NEW 网格 4 行（3 存储 +
  热栏底行）填满面板左区、与右侧两列配方列底部齐平 = 用户诉求
  「更和谐」直接兑现 vs OLD 3 行网格下留大片空区；**配方列两侧
  像素一致**（UX-8c 红线保持）；**像素 diff 14,166 px bbox
  [7,178,366,278] 全限网格行带**，面板外框两侧同位 (2,138,676,604)
  = 原生高 896 不变实证）。
- **S2b 迁移证明 PASS（三跑 headless）**：① NEW 读 f23 → 退出写
  f24（36 槽、占用值保真、27..35 恒空）② OLD v1.45.0 读 f24 =
  rc1 精确点名 **"unsupported save format 24 (this build reads
  formats 1-23)"** ③ NEW 再读 f24 = 往返 OK。
- **S6 版本谎报 PASS**：f24 携 27 条 slots → rc1 点名
  **"inventory.slots' must be an array of exactly 36 entries"**。
- **S3** = 2053/2053。
- **性能 A/B v1.45.0 vs v1.46.0（发布前硬门槛）PASS**：fall 900f
  串行零并行——PERF-WIN 三窗 upd med 0.367/0.355/0.381 vs
  0.387/0.402/0.421、render med 0.415/0.394/0.414 vs
  0.423/0.461/0.440、swap med 15.72/15.83/15.74 vs
  15.73/15.69/15.69 = 全指标同档；PIPE3-S0 changesApplied **9/3/1
  三窗逐位**；**终档 fluids 18/18 + blocks 55/55 集合恒等 diff 0 =
  同世界同成本** + 终档 format 23→24 / slots 27→36 = 迁移语义
  性能跑内再实证（无面板 = 4 行 layout 零执行 = 结构惰性最强形）。

## 门内发现（PITS 355）

- **PITS 355（sed 链式版本替换双移 = 门脚本生成坑，本会话第 2 次）**——
  用 `s/v1.43.0/v1.44.0/g; s/v1.44.0/v1.45.0/g` 这类**顺序链式** sed
  生成性能脚本时，第一次替换的输出被第二次再吃 → 全部版本标签推到
  新版（门 163 首次：标签全变 v1.43.0 纯展示错；门 166 复现：日志名
  互撞 abv145×2 + term 名互撞 = 脚本自毁）。教训：版本标签脚本 =
  整文件重写（write）或单替换 + 逐行核对，禁链式 sed；生成后
  `grep -n 'abv\|term_\|RC='` 核一遍再 bash -n。

## 偏离/记档

- 炉子面板同步 4 行（FurnaceLayout 渲染全玩家库存 = 27→36 跟进，
  冶炼区几何零变化 = 重钉测钉死）。
- 工作台面板（3×3 grid + 34 shaped 行）/ 配方两列（UX-8c）零改动
  = 位级保持。
- 36 槽 = MC 标准背包；再扩行 = 无 MC 对标（矩阵外）。
