# Phase 63 细节存档

> 范围（用户"先建议2,再建议1" = 建议 1，代理定案为最小完整版：**flamable
> 内容扩展 = MC 天然可燃三块纯数据**；火视觉/引燃 Mob 超出本 phase
> 范围【见 KNOWN_ISSUES 燧石与钢 v1 范围条目】）：
> ① `wood` / `leaves` / `grass` 三块加 `flamable: true`（与 P58 的
>    wheat_1..7 合起来 = 自然世界可燃面）；
> ② 两个消费者立即生效（**零核心代码**）：点燃器（P60 右键点燃）与
>    熔岩燃烧 pass（P58 FluidSim 快照扫描）都只读该属性。
> **无 format 变更**（format 9）。

## 阶段报告

- **`[2026-10-14, Phase 63]`** flamable 内容扩展：MC 天然可燃三块
  — 1023/1023（+1），`-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 36 次
  结构性免疫【无重锚】+ S2 点燃器烧 wood + S3 熔岩烧 wood【9 石衬砌
  场景，×2 RAW】+ S2ctl 无输入【×2 RAW】+ S6 init 失败 ×2）。

### 实现内容

**内容（零核心代码）**：
- `assets/data/blocks/wood.json` / `leaves.json` / `grass.json`：
  properties 各加一行 `"flamable": true`（indent 2 手工 edit，无
  json.dump 重排）。
- 可燃集 = **wheat_1..7 + wood/leaves/grass = 恰 10 块**（单测精确
  集断言钉死；仍不燃的 MC 可燃物：羊毛【文档化偏离】/高草/火把
  【未实现】，见 KNOWN_ISSUES）。

**测试（+1，1023/1023）**：
- `test_block`：`Phase63FlamableContent`——真实 assets 断言三块
  flamable = true + **全注册表可燃集恰 10 块**（排序后逐名精确
  断言：grass/leaves/wheat_1..7/wood——防"顺手多标一块"悄悄扩大
  燃烧面；ASCII 序坑：wheat_* 排在 wood 前）。
- **P58 测试契约演进**：`Phase58WheatCropFlamableContent` 的负清单
  原含 wood/leaves（"v1 不燃"）——P63 有意变更该契约，wood/leaves
  移出负清单（保留 stone/wool/sand），注释指向 P63 测试。

**无 format 变更**（format 9）。

### 关键决策

- **范围 = 纯数据三块，不含火视觉**：建议原文"flamable 内容扩展 +
  火视觉（较大）"——火视觉（火焰块/粒子）是独立大项，v1 烧 = 直接
  移除（P58/P60 既有语义），视觉后补不改变本 phase 的语义完整性。
- **结构性免疫论证（第 36 次，无重锚）**：flamable = JSON 属性
  （无视觉变化）+ fresh 世界**零熔岩**（P54）= 燃烧 pass 扫零格
  惰性 + 默认背包无点燃器 → 无输入零活动。
- **S3 场景 = 石质衬砌（本 phase 最大的坑，PITS 收录）**：P58 S4
  的熔岩池场景原样复用**两次失败**——池位于草高原上的 2×1 凹穴，
  整个周边环 + 池底全是草（P58 时不燃）：① 第一轮烧穿池底 → 池落层
  进 y44 洞穴、烧穿整层草、玩家烧死；② 7 石修补后 L7 的草 6-邻
  被点燃 = **高原野火**（熔岩前沿 + 烧穿草互相推进）。终版 = **9 石
  衬砌**（3 缘 + 池底 2 + 墙 2 + L7 的草 6-邻 2），终态恰
  {池 2×L8 + 烧穿格 L7}、零野火。教训：熔岩场景复用必须枚举**每个
  可能出现的熔岩格**（含烧穿后的流体级）的 6-邻可燃格；且"某跑未烧
  某格"的推断只对该跑实际出现的熔岩格有效。

### E2E 证据链

- **S1 第 36 次结构性免疫**（**无重锚**）：fresh 60f ×2 + == base18
  字节一致（mismatch = FAIL）。
- **S2 点燃器烧 wood**（420f ×2 savenorm 字节一致 + save EXACT）：
  wood 放 (22,46,−5)（P52 几何、瞄准锚定存档 = 零鼠标移动）、slot0 =
  flint_and_steel、t_ml+2.9 s 单次 `xdotool click 3` → 终态 diff =
  {}（格回生成 air）、**entities = []**（无掉落 = 烧语义；破坏会掉
  wood 物品 = 可区分）、health 20 / pos / yaw-pitch 不变。
- **S3 熔岩烧 wood**（240f ×2 RAW 字节一致 + save EXACT，**无输入**）：
  P58 池几何 + 9 石衬砌 + wood 放 (23,45,−5)（池 6-邻）→ 第 1 帧燃烧
  pass 烧掉 wood（**无掉落**）、空位收 L7、池保 L8（石底不掉落层）、
  9 衬砌全完好、**零其他格变化**（无野火）、玩家 20 HP 不变。
- **S2ctl 无输入**（240f ×2 RAW）：wood 保留、零实体。
- **S6 init 失败 ×2**（scratch assets）：wood.json `flamable: "yes"`
  与 leaves.json `flamable: 1` → "block property 'flamable' must be a
  boolean"（P58 严格 bool 检查在扩展块上逐文件点名）。

### 会话存档

- 脚本：`/tmp/opencode/p63_e2e.zsh`（全批：S1/S2/S3/S2ctl/S6；助手 =
  p59_helpers.zsh 前缀改 p63_）；日志 `/tmp/opencode/p63e2e/p63_e2e.log`。
- **PITS +2**：熔岩场景跨 flamable 契约复用（含"未烧 ≠ 不燃"推断
  边界）+ 单引号 save_exact 体撇号坑（`zsh -n` 预检）。
- **后续候选**：火视觉（火焰块/粒子 = 较大）/ 火把·营火方块（光源 +
  可放置 + 蔓延 = 较大）/ 吸收层双分量模型（大改）/ 斧类工具（新
  工具型）；worldgen 熔岩湖 = 用户明示"留到很久的未来"。
