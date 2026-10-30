# 插队修复 ×3：saturation 白名单 / 插桩器入 tools/ / 炉子配方（v1.34.0，门 149）

## 定案（用户 10-30 定案"3、4、炉子可，先插队在这里完成"）

v1.x 矩阵 33/33 收官后的排队项 ③④ + GAMEPLAY 刷新时发现的内容缺口，
矩阵 UX 行开工前清掉：

1. **④ 物品严格解析白名单补 `saturation`**（门 141 内发现，KNOW
   ISSUES 收录"基础设施轮"积压）：`item_registry.cpp` 顶层
   unknown-field 白名单加一行 `key != "saturation"`——10 个带
   `saturation` 的食物 JSON（SURV-9 内容）每次 init 打 10 行
   `unknown field 'saturation' ignored` WARN（解析路径本就正确消费
   该字段，纯日志噪音）。零行为变更。
2. **③ 性能插桩器入 `tools/`**：`tools/g139_instrument.py`（g139 族
   门脚本的 perf 树补丁器：27 标记 PERF-WIN + PIPE3-S0 段计时，两侧
   同打 = A/B 可比）。锚点结构性断言（count==1）= game.cpp 结构变化
   时 loud fail（PITS 317 纪律：perf 树须可从 repo+commit+本工具重建，
   /tmp 重启后不丢）。
3. **炉子配方（内容缺口修复）**：Phase 40 起炉子系统完整可用（熔炼/
   燃料/面板/ding/火星）但 **shipped 65 配方零 furnace + 初始库存无
   = 正常游玩拿不到炉子**（10-30 GAMEPLAY.md 刷新时发现并如实写入）
   → **shaped 配方 `8 cobblestone 围圈 → 1 furnace`**（MC 1.14+
   摆法，纯内容零核心代码；锻石 = Phase 61 挖石头掉落 = 石头→锻石→
   炉子闭环）。隐藏 starter 槽不给（配方即获取途径，MC 同款进度）。
   配方 65→66（shaped 33→34）。

## 测试

**1913/1913（+1：FurnaceRecipeContent 内容钉死——真实资产加载 +
字段钉死 + 真实匹配器正例【满围圈经 matchGridRecipe = furnace 命中，
即工作台 UI 同一入口】+ 负例【缺 1 石 matchesShaped 不中】）、
banner pin v1.34.0**，`-Werror` 0 警告。

**双二进制 CPU harness（/tmp/opencode/harness149）**：同 19 点击序列
经真实 InventoryUI + 双侧真实注册表重放——
- NEW（v1.34.0 库/资产）：terminal = **slot0 furnace ×1**，grid 空
  （shaped 围圈命中 = 首输出点击合成消耗 8，次点击无匹配）
- OLD（v1.33.0 库/资产）：terminal = **slot0 stone ×2**，grid 空
  （无 furnace 配方 = 仅 shapeless 4 锻石→1 石头可匹配，两输出点击
  各合成 1 石头）
= 逻辑层终态互斥的二进制级实证。

## 门 149（全 PASS）

矩阵态 = 浮动窗强制 1362×744（PITS 340 协议）。

- **S1 = 第 149 次结构性免疫 + base18 锚连续 168140 B 免重锚**（fresh
  60f 四方 ×2 全字节一致 new v1.34.0 ≡ old v1.33.0(b41e768) + 跨版
  savenorm EXACT；新配方仅在注册表 + 无工作台/无面板/无合成 = 无输入
  零执行；saturation 白名单 = 纯日志面）
- **S2b = init WARN 消除实证**：v1.33.0 init 日志恰 10 行
  `unknown field 'saturation'`（复现）/ v1.34.0 = 0 行
- **S2 = 炉子合成 A/B PASS（6/6，4 跑全首轮 OK 零漂移 cam=(0,−0.25)）**：
  场景 = 工作台 (0,54,−2)（视线命中 t=2.506 < kReach 5 实证）+ slots
  0-7 = 锻石 ×1；协议 = 捕获 → 右键开台 → 8×(拾取 slot c + 放 grid
  ring c【ring = 0,1,2,3,5,6,7,8 跳过中心 4】) → 输出点击 ×2 →
  slot0 落位
  - **NEW 侧（full 19 点击）= E2E 全链实证**：×2 RAW + ×2 savenorm
    EXACT + 终态 slot0 = **furnace ×1**、grid 空、0 锻石残留（shaped
    围圈命中 = 首输出点击消耗 8 合成入 carry，次点击无匹配 no-op）
  - **OLD 侧（minimal 7 点击，竞态无关断言）**：×2 RAW + ×2 savenorm
    EXACT + 终态 = 无 furnace / 无 stone / 8 锻石守恒（inv + grid +
    entities）——v1.33.0 注册表无 furnace 配方 = 同输入不可能出炉子；
    minimal 化原因 = v1.33.0 二进制对 19 点击序列系统性坏帧相位
    （3 跑同散位终态，重试不变；门内发现未编号，见 PITS 节）
  - **双二进制 harness = 逻辑层终态互斥**：NEW slot0 furnace ×1 vs
    OLD slot0 stone ×2（OLD 满围圈也只可能 shapeless 石头双合成）
- **S3 = 1913/1913 PASS**
- **性能 A/B（tm_v134_perf_old v1.33.0 vs tm_v134_perf_new v1.34.0，
  同 tools/g139_instrument.py 插桩 = 入 tools/ 即实战首验）PASS**：
  banner 双侧实证 + PERF-WIN 全指标同档（upd med 0.388-0.400 vs
  0.399-0.422 / render med 0.62-0.63 vs 0.65-0.68 / swap ~15.4 双侧 /
  fluid·fires·spread·ents·stream 同档）+ PIPE3-S0 changesApplied
  9/3/1 三窗逐位 + **终档 fluids 18/18 diff 0 + blocks 55/55 diff 0 =
  同世界同成本**（结构惰性最强形：无输入无工作台/面板/合成 = 新配方
  仅注册表层）

## PITS

- **PITS 338**（面板点击 warp-press 竞态：双 warp 不够，落点错位可
  污染状态 = 双击收集合并堆 → 落位验证协议）
- **PITS 339**（场景注入 per-block 状态容器块必须携对应状态段：
  crafting_table 无 tables 条目 = init 拒档，症状伪装成 GEOM-
  UNSTABLE【search 命中孤儿窗】）
- **PITS 340**（长输入序列须先浮动窗 + 强制几何：WM 数秒级重排 tile
  窗 = 已验证光标落窗外/标题栏 quit）
- **门内发现（未编号）**：v1.33.0 二进制对 19 点击序列存在系统性坏
  帧相位（3 跑 = 同散位终态，重试不变）——OLD 侧改最小 7 点击 +
  竞态无关断言（无 furnace/无 stone/8 锻石守恒）= 判别力不减；NEW
  侧相位良（2/2 完美）。同输入同序列跨二进制帧相位敏感 = 跨版
  A/B 输入跑须按侧设计断言强度。
