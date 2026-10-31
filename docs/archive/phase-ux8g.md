# Phase UX-8g — 第三人称瞄准修复（v1.47.0，门 167）

> 消费用户 v1.46.0 实机报障：「按 C 切换视角后，我发现十字光标在
> 我自己身上，我就无法选中方块，右键，敲打、挖、砍树这些动作都
> 失效了。」**UX-8 体验细化行收官后首个实机报障修复**（v1.34.0
> 插队修复同款：不挂 tag，UX-8 系统 release 补记）。

## 根因（UX-4 门 158 记档误判）

UX-4 定案记档"交互/瞄准/箭/声音/云**零改动**（全读 m_camera：
First 不变，ThirdBack 自相机 = 屏幕中心与准星一致 = MC 同款）"——
**MC 同款是误判**：Minecraft 的第三人称瞄准射线从**眼位**出发
（玩家自身模型永不遮挡目标，屏幕中心 = 眼位视线）。本项目照搬
"全读 m_camera" 后：

- 第三人称 `m_camera.position` = 眼位后上方球面 **4.0 块**
  （`kThirdPersonDistance`），`m_camera.forward()` = 玩家朝向
  （m_camera.yaw/pitch 帧始同步玩家值）→ 瞄准射线 = 与"相机过眼位
  屏幕中心线"**共线但原点在后 4 块**
- `kReach = 5.0` → **前方有效射程 = 5.0 − 4.0 = 1.0 块**（正常
  2-4 块距离的方块/树/mob 全够不着 = "挖/砍/敲打失效"）
- 射线**穿过玩家自身头部区**：最近的可达命中落在贴身块/脚下块
  （裂纹高亮与选择框对着自己 = "十字光标在我自己身上"）；开敞地
  带连 1 块内都无目标 = 选择框消失 + 所有动作 no-op
- 同族受害（同 `m_camera.position` 原点）：`targetMob` 攻击
  （:447）/ `tryFluidBucket` 舀与倒双射线（:1232/:1306）/
  `fireBow` 箭生成点（:609，箭从身后 4 块冒出）

## 修复（5 处原点，全在 game_interaction.cpp）

**瞄准/交互射线原点两模式统一 `m_player->eyePosition()`**——
第一人称 `m_camera.position ≡ eyePosition`（game_frame.cpp:413）=
**位级恒等零回归**；第三人称 = MC 语义（眼位射线与屏幕中心线共线，
仅原点前移 4 块 → 射程恢复 5.0 全预算 + 首格 = 眼格不被测 = 玩家
自身永不遮挡）。方向 `m_camera.forward()` 零改动（两模式皆玩家
朝向）。

1. `targetBlock()`：挖掘/放置/交互/裂纹高亮全链收口
2. `targetMob()`：与 targetBlock 的 bestT 同原点 = 遮挡比较一致
3. `fireBow()` 箭生成：`eye + f·kArrowSpawnOffset`（箭从身前发射）
4. `tryFluidBucket()` collect 侧 raycastFluid
5. `tryFluidBucket()` pour 侧 raycastFluid

**保持相机驱动（m_camera.position 消费点 13 处逐一复核）**：
声音 listener（听者 = 视点）/ 视图矩阵与雾 eye / 云层平移 /
流体覆盖层判定（相机格浸没 = 视觉）/ 相机遮挡射线（third_person
resolve 设计本体）/ 帧始相机同步 / 读档相机复位。

**新测** `EyeAimRayCoincidesWithTheViewAxis`（test_third_person）：
无遮挡 resolve 相机位 = 眼位射线 t=−4.0 精确共线（6 yaw/pitch
用例）——targetBlock 是 Game 方法不可纯函数单测，此几何论证
证明"眼位射线 = 屏幕中心线" = 修复后准星语义与视图一致。

## 测试

2054/2054（+1）、`-Werror` 0 警告、banner v1.47.0。
零 format/config/内容/音效。

## 门 167（编排者执行，old 侧 = v1.46.0 = d1cb11d = tm_v147_base）

- **S1 = 第 167 次结构性免疫 + base18 锚连续 168140 B 免重锚**：
  fresh 60f 四方 ×2 全字节一致 + **跨侧 RAW 字节一致**（无输入无
  C = First 模式，eye ≡ camera 原点位级恒等）+ 跨版 savenorm EXACT。
- **S2 第三人称挖矿 A/B PASS**（门 164 实证几何：wood row
  (0,55,−4..−6)，W 0.8s → (0.5,54,−2.7)，yaw 0 pitch −0.12，
  9.5s L-hold = 3.0s/木）：
  - **NEW-3P（修复后第三人称）**：破 **3** 木 + 3 散落掉落 +
    教程 {planks,0} = 用户报障直接修复
  - **NEW-1P（第一人称对照）**：同终态（破 3 + 3 掉落 +
    {planks,0}）+ **NEW-3P ≡ NEW-1P savenorm EXACT = 修复后
    第三人称行为与第一人称完全等价（最强形实证）**
  - **OLD-3P（修复前）**：只破 **1** 木（最近格 ≈4.3 块 < 5.0，
    次近 ≈5.3 > 5.0 够不着 = 根因预测逐值复现）+ 1 掉落 +
    {chop,1}
  - **互斥判别**：NEW-3P vs OLD-3P 段级分解 = **仅** wood 2 格
    （−5/−6 未破）+ entities 3/1 + tutorial（玩家 pos 逐位同、
    fluids/inventory 零 diff、本跑湖 churn 零）
  - ×2 RAW 三对全字节一致 + 同侧 savenorm EXACT
- **S3** = 2054/2054。**无 S6**（零 parse 面——纯原点替换，零
  新数据字段）。
- **性能 A/B v1.46.0 vs v1.47.0（发布前硬门槛）**【待收】。

## 偏离/记档

- UX-4 门 158 记档"瞄准全读 m_camera = MC 同款"废止 = 本修复
  （AGENTS UX-4 条目的该半句以本档为准）；UX-4 其余面（相机
  位/遮挡收敛/零状态/手持隐藏）不变。
- 第三人称手持 viewmodel 仍隐藏（UX-4 定案不变）；箭生成点在
  眼位前方 kArrowSpawnOffset（MC 从弓位发射的精确复刻 = 矩阵外）。
