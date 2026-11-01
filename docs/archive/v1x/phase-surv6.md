# SURV-6 效果档位（v1.x 17/33，v1.17.0，门 132，SURV 行 1/5）

v1.x 任务矩阵第十七项。原始定义已佚 = 代理定案最小完整版：**状态效果
档位化（amplifier levels I/II/III）= 加性修饰字段 per-tier 增量，全数
据驱动**（消费面 = P46 状态效果系统 / P84 修饰三字段 / SURV-4 药水
grants / SURV-5 mob 双载体）。与 PRE-12 边界：SURV-6 = 档位机制 +
存档 + 数据（行为）；PRE-12 = F3/tooltip 的档位"显示"（UI，后续行，
本相 F3/HUD 零触碰）。

## 定案

- **效果数据 2 新键**：`tier_max`（int [1,3]，缺省 1 = 单档位级复现旧
  行为；>3/非整数点名）+ `tier_steps`（对象：仅 5 个**加性修饰字段**
  可档位 `speed_multiplier`/`absorb_hp`/`melee_bonus`/`melee_penalty`/
  `damage_reduction`，每值 number ≥ 0；tick/instant 字段 = 单档记档偏
  离——档位改 tick 节奏会动帧网格，instant 无档位消费面）。三门纪律：
  **配对**（非零 step 须其 base 字段在场，fast_against 范式）/ **死字
  段**（tier_max 1 带 step 拒）/ **no-op**（tier_max > 1 全零 step 拒）。
- **施加点全链 tier 尾参**：`grants_effect(s)`（食物/饮品）/
  `arrow_effect`（物品弓 + mob 箭）/ `on_hit_effect`（mob 近战）各加
  可选 `tier`（int ≥ 0，缺省 0 = 位级旧行为；**范围 init 交叉校
  验**：tier < 所指效果 tier_max，点名——registry 解析期不可得，
  on_hit_effect 同款纪律）。`StatusEffects::apply(effect, duration,
  tier)`：RESET 语义扩到档位（再施放 = 换档位，不叠加 amplifier）。
- **聚合 tier 化**（`effectTieredValue(tier, base, step)` 纯函数）：
  speed 乘积 / absorption 和 / melee_bonus 和 / melee_penalty 和 /
  resistance 取最高——每实例先按档位取值再按既有组合律合成。tier 0 =
  全部聚合位级复现旧值（结构免疫根）。
- **format 19 → 20**：`player.effects` 条目可带 `tier`（**omit-when-
  0** = damage 纪律：tier-0 条目与 format-19 字节同形 ⇒ 19 文件原样
  读入 = 迁移 tier 0；19 及更早文件带 tier 键 = 版本谎报，格式依赖白
  名单自动点名；tier 非整数/≥ tier_max 点名，registry 在 parse 期可
  得 = 吸收池不变量同款）。
- **内容 = MC 档位值 5 效果**：speed 1.2+0.2（I/II/III = 1.2/1.4/
  1.6）/ absorption 4+2（4/6/8）/ resistance 0.2+0.2（0.2/0.4/0.6）/
  strength 2+2（2/4/6 项目压缩刻度）/ weakness 4+4（4/8/12 = MC 精
  确值）。其余 6 效果单档不变。
- **无档位 in-game 消费面（记档偏离）**：信标 `beacon_effects` = 纯
  名称列表（无 grant 对象，P91 形态）；药水无酿造系统（starter 隐藏
  槽 27 槽全满，PITS 300）→ 档位实例现仅经存档注入（E2E 范式，
  P84 先例）+ 未来内容扩展（信标 grant 对象化 / 酿造 = 矩阵外）。
- **零 format 之外的世界状态**：mob 效果 ephemeral 不入档（SURV-5
  既有）；F3/HUD 图标零触碰（档位显示 = PRE-12）。

## 测试（1820/1820，+17 净，-Werror 0 警告）

- test_status_effect +6：tier_max 范围 / tier_steps 五类拒绝（非档位
  字段/负 step/配对/死字段/no-op 双形）/ effectTieredValue 精确值 /
  五聚合 tier 化（0/1/2 档 = I/II/III 精确值）/ reapply 换档不叠加 /
  shipped 5 效果 tier_max 3 + step 精确钉死 + 6 单档对照。
- test_save +6：format 20 tier 往返 / tier-0 omit（字节同形 f19）/
  越界点名 / 非整数点名 / f19 带 tier = 版本谎报点名 / f19 迁移 tier 0。
- test_item +3：grants 单/列表 tier 解析 + 非法三形拒 / arrow tier 解
  析（缺省 0）。
- test_mob +2：on_hit tier 解析 + 非法三形拒。
- 7 处 format 钉 19→20 契约演进（test_save ×5 / test_campfire /
  test_durability）。

## E2E 门 132

场景 s132 = s131 密封房扩展坠落室（地板 y45 + 头顶 air carve 到 y56 +
石墙延到 y57 + glowstone 顶 y57；玩家 feet y53 → 8 格坠落 = 5 伤；
config 禁再生 = P84 范式；600f 无输入）。

- **S1 = 第 132 次结构性免疫免重锚**：fresh 60f 四方 ×2（new v1.17.0 /
  old v1.16.0 = f5a19b7）全字节一致 + base18（fresh 零效果 = count 0
  fast-exit，聚合零执行 + 存档 effects 空数组同形）。
- **S2 三值互斥（判别主体）**：s2a 无效果 **hp 15** / s2b resistance
  tier 0（5×0.8 = 4 伤）**hp 16** / s2c resistance tier 1（5×0.6 = 3
  伤）**hp 17**——各 ×2 RAW + savenorm（剥 effects remaining real-
  dt）EXACT + 三值精确断言 = 档位经 存档→apply→减伤管线 端到端实证。
- **S2d 迁移证明**：旧 v1.16.0 二进制 + s2b 的 format-19 孪生文件
  （tier-0 无键 = 字节同形）→ **hp 16**（pre-SURV-6 代码原样读取）。
- **S6 ×3 全点名**：(a) 旧二进制 + format-20 存档 = "unsupported
  save format 20 (this build reads formats 1-19)" / (b) scratch 资产
  speed tier_max 4 = "'tier_max' must be an integer in [1, 3], got
  4.000000" / (c) scratch 资产 potion_swiftness grants_effects[0]
  tier 3（≥ tier_max 3）= "grants 'speed' at tier 3, but the effect
  has tier_max 3 (tiers are 0-based, in [0, 2])"。
- 门脚本坑：S6c 首轮按单形 grants_effect 写 scratch，potion 实为
  grants_effects 列表形 = KeyError（先查物品实际 grant 形再改资产）。
- **S3 = 套件 1820**。

## 性能 A/B（发布前必做）

tm_v116_perf vs tm_v117_perf（= 插桩 v116 树 + SURV-6 src diff 重打
插桩 game.cpp【rsync 树会被 diff 覆盖丢 STEP0，须 git diff 补丁回打；
**且 cp -a 带的 build/ 须删掉重配**——陈旧 CMakeCache 目录失配 +
ninja mtime 假"无活" = 静默用旧二进制，PITS 306】），同 seed fall
900f（真 v1.16.0 vs v1.17.0 二进制 banner 实证）：零效果场景 = 聚合
fast-exit 结构惰性（**双侧终档 effects 全空实证**）⇒ fluidCells
win1 51927/51924（±3 fill 窗 real-dt 类）+ win2/3 29703/29700 vs
29803/29800（**±100 = PITS 283 rim churn 相位类，裁定非回归**：终档
流体差 4 格全在程序化熔岩湖 rim 簇 (81..84,36,74..77) + fires
50 = 50 + v1.16→v1.17 代码差 = 效果系统（本场景零执行））+ PIPE3
changesApplied 1404 逐位 + 全指标同档 = **成本可忽略（结构性惰
性实证）**。

## 发布

v1.17.0 = 新组 1.16-1.20 第 2 版（组内不打 tag；组末 v1.20.0 → tag +
GitHub release）。正确性 A/B 旧侧 = v1.16.0 = f5a19b7（tm_v116_src）。
