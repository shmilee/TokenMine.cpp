# Phase SURV-2 存档（氧气窒息）

## 定案

水下氧气条 + 窒息（溺水）伤害，v1.0 任务表 SURV 行第 2 项（16/29）。
MC 语义查证（minecraft.wiki Damage 页伤害调整表 + Drowning 小节）：
**溺水/窒息对护甲无效（Armor: No effect）、对 Resistance 有效
（Applies）**——SURV-1 定案"届时按 MC 豁免规则单独立项"在此落地。
代理定案最小完整版：帧锁定氧气系统 + 眼位单格流体判定（水/熔岩同款）
+ damage 护甲豁免默认参 + format 17 + HUD 气泡排（替换饥饿排）；
无 config 键（MC 值 = 命名常量）、无气泡声（无新 wav）、mob 不溺水
（MC mob 不溺水）、无呼吸 enchantment/水肺/难度豁免（项目无这些系统）。

## 实现

- **数值（帧锁定，60fps = MC tick × 3）**：空气 150（`kOxygenMax`），
  头没入时每 6 帧（`kOxygenDecayFrames`）−1 = 900 帧 15.0 s；耗尽后每
  150 帧（`kDrownDamageFrames` = 2.5 s）`kDrownDamage` 2.0 伤；出水
  立即满（MC 换气）。时间线钉死：满空气帧 894 耗尽、帧 1044 首伤、
  此后每 150 帧；预置 0 → 伤害帧 149,299,…,1499（10 次 = 死）。
- **纯函数 `player/drown.{hpp,cpp}`**（campfire 范式）：
  `drownStep(DrownState&, headInFluid, frame) -> 本帧伤害量`
  （换气→扣空气 %6 全局相位→耗尽累加 150 帧发 2.0，顺序即语义）+
  `headInFluid(Player&, World&)` = 眼位单格（`eyePosition()` 含 crouch
  缩减，与相机同款）`World::getFluidKind` 任意流体种类；出界/未创建 =
  air。Game 每帧 pass（player update 后、帧锁定上下文、传
  m_frameCount；dmg>0 → `damage(dmg, bypassArmor=true)`）。
- **`Player::damage(float, bool bypassArmor = false)`**：bypass 跳过
  SURV-1 floor 减伤步（维基表实证）；resistance 与吸收池照常；默认
  false = 全部既有调用点位级不变（存量伤害测试零改动背书）。
- **format 17 `player.oxygen`**：int [0,150] 恒写（armor {} 恒写同款）；
  键纪律 17 必需 / ≤16 带键 = 版本谎报点名 / 非整数、越界点名 /
  player 白名单 +oxygen；15/16→17 迁移满空气 150；**drownTicks 不存**
  （读档归零 = 记档偏离，MC 只存 airSupply 同款语义）；死亡过渡
  （respawn 收口）`DrownState{}` 复位。
- **HUD 气泡排**：`locked && !panelOpen() && oxygen < 150` 时**替换
  饥饿排原位**（gx0, hy；MC 空气条显示时饥饿条隐藏）；10 气泡
  （`kOxygenBarIconCount`/`kOxygenPerIcon` 15），填充 = ceil(oxygen/15)
  单 tile 无半态；新纹理 `oxygen.png` 16×16（
  `tools/generate_oxygen_icons.py`，salt 105 = 物品图标 104 后下一空闲
  值；双跑 md5 一致 + 全部既有 PNG 重跑零变化 = 确定性证明）；
  基线免疫：dry 出生 + 基线视图零水体 → headInFluid 恒 false →
  oxygen 恒 150 → 零绘制。
- **F3 调试行** `oxygen N`（debug_overlay 12→13 行，test 断言行号同步）。

## 变更文件

（fixer 交付，待主线提交；实现 17 文件：新增 drown.{hpp,cpp} /
test_drown.cpp（18 条）/ generate_oxygen_icons.py / oxygen.png；
改 game.{hpp,cpp}（drown pass + HUD 泡排 + save/apply + F3 + 纹理
加载）/ save.{hpp,cpp}（format 17）/ player.{hpp,cpp}（bypassArmor）/
debug_overlay.{hpp,cpp} / CMakeLists / test_debug_overlay / test_save /
test_campfire（format 钉 17 + 降级点剥离）/ KNOWN_ISSUES）

## 测试

1448/1448（+18）、`-Werror` 0 警告、存量 1430 零回归。
- drownStep 8：换气满/扣空气 %6 相位/耗尽帧 894/首伤 1044/节奏 150
  帧/预置 0 首伤 149/出水即满 ticks 归零/死亡 inert
- headInFluid 4：水/熔岩/浅池眼上/出界 air
- damage bypass 2：满钻甲（20 点）2.0 豁免 = hp 18.0 exact（默认路径
  19.6 = floor 0.4 互斥）/ resistance 0.2 → 1.6 且池先扣（池 4→2.4，
  health 20 不动，三击后池空 health 19.2）
- save format 17 4：RoundTrip 中段值 / 16 带键谎报点名 / 15,16→17
  迁移满空气 / 越界与非整数点名

## E2E（门 96）

场景 = g94 池几何复用（4×8 石池 3 层水 y36-38，玩家池底 (22.5,36,1.5)
静止 = ENT-4 池底站立门不上浮）：深池眼 (22,37,1) 在水 = 持续没入 /
浅池（删上 2 层水）眼 (22,38,1) 在空气 = 换气。
- **S1 base（结构性免疫）**：历史 gate88 锚（679x744）与当前窗态
  （away 重平铺 1362x744）尺寸失配弃用 → 改**新旧 A/B 同窗态**：
  NEW fresh spawn 60f vs **OLD v0.13.0** 60f = **全帧字节一致
  ✓**（SURV-2 对基线视图零变化，比历史锚更强的证明）。
- **S2 溺水死**（deep，oxygen=0 预置，1500f no-input）：伤害帧
  149,299,…,1499 共 10 次 ×2.0 = **hp 0 死亡**（effects 清 = 死亡清
  ✓），×2 存档 savenorm3 EXACT【剥 fires remaining/hunger/age =
  real-dt 字段，PITS 244 同款非缺陷】。
- **S2b 满空气节奏**（deepfull，oxygen=150，1500f）：**hp 12.0 exact +
  oxygen 0** = 帧 894 耗尽 + 伤害 1044/1194/1344/1494 共 4 次——扣空气
  与溺水双节奏帧精确实证，×2 savenorm3 EXACT。
- **S2c 护甲豁免互斥**（deeparmor，oxygen=0 + 满钻甲 20 点，1500f）：
  **hp 0 死亡**（10×2.0 全吃）——若误走 SURV-1 全源减伤 = 10×
  floor(2×0.2)/100 = 4 → hp 16 存活；死/活互斥 = **MC 豁免规则
  （Armor: No effect）进入 E2E**，×2 savenorm3 EXACT。
- **S3 换气对照**（shallow 浅池眼上水面，1500f）：**oxygen 150 恒定 +
  hp 20 恒定**（不扣不伤，池底不上浮 = pos 钉死），×2 savenorm3 EXACT
  ——与 S2 族互斥。
- **S4 气泡排像素隔离**：首轮协议（hud o2=60 vs shallow o2=150 捕获
  对）**10-25 14:2x 采得双干净帧但判读弃用**——两场景几何不同
  （deep 眼在水里 = 水下覆盖层 + 水体视差 vs shallow 眼上水面）→
  全帧 diff 1,011,812 px = 覆盖层差异，非排隔离（教训：气泡排对照
  必须**同几何只改 oxygen**）。**修正协议已定 + 状态证据已得**
  （10-25 14:3x）：同 deep 几何 600f，s4a o2=150 → 终态 **o2 50 =
  4 泡** / s4b o2=135 → **o2 35 = 3 泡**（互斥，6 帧/氧 ×600f = 耗
  100 精确；hp 20/20 双未窒息 = 心行同形）。**状态级隔离已证**：
  s4a vs s4b 存档全量 diff = 仅 22 处 fires[i].remaining（real-dt
  剥离项，PITS 244 非缺陷）+ player.oxygen 50/35；blocks 924 /
  fluids 31162 / entities 0 双同 = savenorm 剥离后**唯一差异即泡数
  来源**。像素对 = 采集 2 后台补跑中（14:3x-14:5x GLX 黑相 6 次全
  黑帧 → 留补跑清单，状态证据已足以支撑门内行为结论）。用户实机验证 = 潜水看
  饥饿排变气泡排（不变条款）。气泡排代码路径 = 既有 drawIcon 复用
  （无新绘制原语）+ 单 tile 纹理，渲染风险低。
- **S5 存档 format 17**：S2 600f 版 hp 12.0（伤害 149/299/449/599 = 4
  次）+ oxygen 0 在档 + fmt 17，×2 savenorm3 EXACT。
- **S6 init 失败 ×3**：`save 'player.oxygen' requires format 17 or newer
  (got 16)`（版本谎报点名）/ `must be in [0, 150], got 151.000000`
  （越界点名）/ `must be an integer in [0, 150]`（类型点名）。
- **环境注（10-25 晨）**：GLX 黑相波 07:15 起 3h+（干净窗仅 2-5 s，
  整跑曝光 5-25 s 难落入；away 重平铺改窗尺寸 = 跨时段 RAW 对失效）——
  存档级确定性（savenorm3 EXACT ×5）+ 新旧 A/B 字节一致（S1'）为门内
  已证核心；场景族 RAW ×2 对与 S4 像素隔离 = 短曝光重试中/晨间补跑
  清单。

## 性能（插桩 A/B v0.13.0，发布前测试）

- **underwater 900f 三 300f 窗**（深池 o2=0，drown pass 活动路径）：
  render med 0.355/0.518/0.693（old 0.366/0.524/0.677 同档）、upd med
  45.5-48.7 ms（old 44.9-49.2 同档）、swap med 0.48-0.50（old 同款）、
  player 1.85-1.92 ms（old 1.74-1.83 同档）、ents 0.25-0.26（old
  0.26-0.30）、stream 1.82/2.13/2.48 s（old 1.81/2.10/2.41 s）
- **vd16 fresh 流送填充 600f**：render med 0.773/0.751（old
  0.743/0.741）、upd med 0.217/0.281（old 0.230/0.277）、stream
  46.1/31.2 ms（old 43.6/26.0）——同档
- **结论：SURV-2 成本可忽略，无回归**（drown pass = 每帧一次纯函数 +
  一次眼格流体读；HUD 泡排仅指针捕获 + 水下时绘制 = 无输入零活动）

## 偏离

- 窒息豁免护甲减伤 = **MC 规则实证**（维基伤害调整表 Drowning/
  Suffocation 行 Armor: No effect）——SURV-1"全源减伤"偏离的豁免项
  落地；resistance 与吸收池对溺水仍生效（表：Resistance Applies）
- drownTicks 不存档（读档归零 = 溺水节奏可能提前/延后 ≤2.5 s，MC
  只存 airSupply 同款）
- 无气泡声/无喝水声（无新 wav；MC 气泡声随机 = 非确定）
- mob 不溺水（MC mob 不溺水；任务条 = 玩家氧气条）
- 无呼吸 enchantment/水肺/和平难度豁免（项目无对应系统）
- 无 config 键（MC 值 = 命名常量，YAGNI）
- 泡排空位不绘暗态（MC 空气条 10 位恒定显示暗泡——简化记档）
