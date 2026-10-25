# Phase SURV-5 — mob 效果（v1.0 任务表 21/32，门 102）

> 状态效果系统从"仅玩家"扩展到"玩家 + mob 双载体"（MC wither 骷髅箭 /
> 毒箭语义落地），全部数据驱动。SURV 行 5/5 收尾。

## 实现内容

1. **效果侧**（`status_effect.{hpp,cpp}`）：3 新字段
   - `affects_mobs`（严格 bool 缺省 false）——效果可施加到 mob 的门
   - `min_health_mob`（整数 0/1，缺省 = 跟随 min_health）——mob 侧地板
     （poison 0 = mob 可毒死；玩家 min_health 1.0 不动 = 存量零回归）
   - `undead_exempt`（严格 bool 缺省 false）×mob `undead`（缺省 false）
     组合门 = MC 不死族免疫 poison
2. **内容**：
   - `wither.json`（新）：1.0/4.0 s（MC wither I）、**无 min_health**
     （双目标可杀）、negative、affects_mobs、icon 10
   - `poison.json`：+min_health_mob 0 / affects_mobs / undead_exempt
   - skeleton.json：+`arrow_effect {wither, 10}`（MC wither 骷髅箭值）+
     `undead: true`；zombie.json：+`undead: true`
   - `poison_bow.json`（新）：bow 同款 + `arrow_effect {poison, 60}`
     （MC arrow of poison 1:00）+ durability 384 弓族；配方 shapeless
     bow + potion_poison → 1（药水作材料消耗、不还瓶 = 记档偏离，给
     SURV-4 毒药水第二用途）
   - effects.png 160→176 尾部 append（tile 10 wither 深紫骷髅，既有 10
     tile 逐像素零变化 + 77 件 item icon 逐字节零变化 + 两生成器重跑
     幂等）
3. **mob 载体**（`mob_entity.{hpp,cpp}`）：MobEntity 持 StatusEffects
   纯类 + `applyEffect(id,duration,registry)` 双门（affects_mobs +
   undead_exempt×undead，dead 拒）+ `updateStatusEffects(dt,registry)`
   （Game 在 `mob->update` 后同帧调用 = 帧锁定；onTick Damage 按 mob
   地板判定走既有 damage 击杀路径；**Heal = no-op 记档偏离**（mob 无
   回血）；count==0 fast-exit = 无效果 mob 位级不变）
4. **投射物**（`projectile.{hpp,cpp}`）：构造尾部
   `int effectId = -1, double effectDuration = 0.0`（默认 = 全部既有
   调用位级不变，9 参 legacy 形同款钉死）
5. **双源数据字段 + init 交叉校验**：item `arrow_effect`（要求
   shoot_speed > 0 = 非发射器拒）/ mob `arrow_effect`（要求
   shoot_range > 0 = 近战拒）+ **bows 额外要求效果 affects_mobs**（弓
   箭只打 mob，点名）/ mob 侧不要求（skeleton 箭打玩家）
6. **命中接线**（game.cpp）：fireBow + skeleton shoot 回调射箭时
   解析 id 传投射物；arrow→player 走 `applyPlayerEffect`（SURV-4 单
   收口复用）+ syncAbsorption；arrow→mob 走 `applyEffect`
   （`!mob->dead()` 门 = 箭击杀的不施）

**零 format（18 不变）/ 零 config / 零新音效**（箭命中复用 "hurt"）。

## 变更文件（26）

核心 12：status_effect.{hpp,cpp} / mob_registry.{hpp,cpp} /
mob_entity.{hpp,cpp} / projectile.{hpp,cpp} / item.hpp /
item_registry.{hpp,cpp} / game.cpp；内容 6：wither.json（新）/
poison.json / skeleton.json / zombie.json / poison_bow.json（新）/
recipes/poison_bow.json（新）；纹理 4：effects.png（176 宽）/
poison_bow.png（新）/ generate_effect_icons.py /
generate_item_icons.py；测试 4：test_status_effect（+9）/ test_item
（+3）/ test_mob（+11）/ test_entity（+2）。

## 测试（1519/1519，+25 新）

效果字段解析/严格性/坏值 ×5 / 内容钉死（wither/poison/skeleton/zombie
真实文件 + 4 非不死族对照）/ mob 载体行为 ×6（affects_mobs 门 /
undead 门 + wither 伤不死族 / poison 12 tick 毒杀 10HP cow 边界留裕量 /
min_health_mob 1 地板恒 1 活 / heal no-op / count==0 fast-exit + dead
拒）/ 投射物 effectId 缺省位级 ×2。`-Werror` 0 警告。

## 门 102 E2E（10-25 19:2x-19:5x）

- **S6 init 失败 3/3 PASS**（scratch 资产点名全命中）：
  bow `arrow_effect {speed,60}` → *"affects_mobs false — a bow's arrow
  only hits mobs"* / skeleton `arrow_effect {witherz,10}` → *"references
  the unregistered effect 'witherz'"* / poison `min_health_mob 0.5` →
  *"must be an integer (0 or 1)"*
- **S1 PASS = 第 102 次结构性免疫**（19:50 att 3）：fresh 60f ×2 RAW
  + vs base18 字节一致，无需重锚（wither 只在箭命中施加 / poison_bow
  不在 starter+hotbar / effects.png 尾部 append / first_spawn_frame 60
  = 60f 无 mob / mob 载体 count==0 fast-exit）
- **S2 探针 PASS（19:43，行为层权威）**：skeleton-only 场景（玩家
  (334.5,45,16.5) 夜 0.75 easy，mob_spawning 两侧资产均限
  skeleton+max_mobs 1 = 隔离变量）420f → **effects 含 wither
  remaining 4.2 + hp 8 存活**；接线实证 = skeleton 箭 → applyPlayer
  Effect 全链（探针迭代史 = PITS 255-258：场景 time 存档权威 / 保护圈
  锚世界出生点 / scratch 资产漏同步 / real-dt 时标放大）
- **S2 截图对 = 补跑中**（门内 8 次黑相未得窗；gate102_retry.zsh
  持久化累积环运行中：预探针门控 + 单拍成功保留 + ×2 RAW + 字段断言
  门控 + 心带区 diff）
- S3/S4（poison_bow 毒杀 cow / zombie 不死族免疫）= 输入跑补跑清单
  （行为层 = test_mob 载体单测已证：poison 12 tick 毒杀 cow 边界 /
  undead 门 + wither 伤不死族）

## 性能（插桩 A/B v0.18.0，发布前必做，10-25 20:0x）

- **fall 900f 三窗**（mobs off = 载体 fast-exit 基线）：**fluidCells
  123879/167781/236573 双侧逐位一致（同世界）**；upd med 47.2/50.8/48.1
  vs old 46.7/49.7/48.5、render 0.326/0.483/0.642 vs 0.307/0.476/0.622、
  swap 0.458/0.498/0.479 vs 0.453/0.481/0.470、stream 1900/2184/2440 ms
  vs 1833/2151/2433 = 全同档
- **skel 场景 1200f 四窗**（mobs on = 载体实际负载：skeleton 射箭 +
  wither 施加/tick）：w1/w2 fluidCells 逐位一致（110495/225506）；
  ents 段 new 2.39/2.83/2.95/4.40 vs old 5.43/8.35/7.80/9.75 ms/300f 窗
  （**new 更低 = 玩家被 wither 更早死亡 → 后期战斗活动减少**，非代码
  成本差）；upd/render/swap 前 2 窗同档（44-50 / 0.43-0.70 / 0.57-0.62）；
  w3/w4 = 双侧玩家死亡态（player 段 → 0.02-0.75 ms）= 时标失步副产物
  （real-dt 死亡时刻差 → 流送中心漂移 → 后 2 窗 fluidCells 差）
- **结论：SURV-5 成本可忽略，无回归**（载体 count==0 fast-exit + 只在
  命中/效果活跃时激活；无每帧新增处理——fall 场景零 mob 全同档实证）

## 记档偏离

- mob Heal tick = no-op（mob 无回血机制；wither/poison 皆 damage 型）
- poison_bow 配方不还瓶（药水作材料消耗；MC 保留瓶子 = 记档偏离）
- 箭击杀 mob 时不施加效果（despawn 同帧移除，施加无意义）
- bow 侧 arrow_effect 要求 affects_mobs（弓箭只打 mob）；mob 侧不要求
  （skeleton 箭打玩家）= 双向语义非对称，init 点名
