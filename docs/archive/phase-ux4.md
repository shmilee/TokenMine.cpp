# Phase UX-4 — 第三人称/拉远相机（v1.x 任务表 37/40，门 158，UX 行 4/7）

> 消费 UX-3 建的 `CameraMode` 门：C 键切换第一/第三人称，第三人称 = 相机
> 眼位后上方球面偏移 + 遮挡收敛。第一人称（默认）位级恒等 = 结构性免疫
> 根。**tag v1.38.0 = UX-3+4 合并 tag**（UX 行政策兑现第 2 个）。

## 定案（10-30 kickoff 建议 + 门内修正）

- **相机 = 每帧派生，零状态**：`m_cameraMode`（UX-3 既有，默认 First）
  是唯一新面；ThirdBack 时相机 = `resolveThirdPersonCamera(world,
  eyePosition(), yaw, pitch)` 纯只读解析。**无新 Game 成员 = 零
  format（22）/ 零存档面 / 读档恒回 First（MC 同款，记档）**。
- **键 = C**（MC 现代相机切换键；kickoff 建议 F5 = 已被手动存档占用，
  **偏离记档**）。locked-pointer 门控（E 键范式：面板开/死亡时 C 无效）。
- **偏移数学 = 视图自己的球面**：offset = −Camera::forward(yaw,pitch) ×
  kThirdPersonDistance **4.0f**（直接调 Camera::forward = 与视图矩阵
  同一三角表达式同一运算序，位级同源；pitch 0 → 正后 / 上仰 → 下压，
  MC 同款）。轨道点 = 眼位（currentEyeHeight 1.62，crouch 随降 = 记档）。
- **遮挡收敛**（kCameraOcclusionMargin **0.1f**）：① ideal 格非 air
  （相机嵌墙：raycastBlock origin 格不测语义会漏）→ 解析穿出距离
  t_exit（DDA 首步，与 world/raycast.cpp tMax 初始化同款符号除法）→
  相机 = 穿出**再留 0.1 间隙在玩家侧**【**门 S2b 实证修正**：初版
  spec t−margin 在嵌入格 = 相机滞留墙内 0.1 块，B-C diff 81965 采样 px
  异常揪出】② raycastBlock 首非 air 块（穿流体 = 相机看水 MC 同款）→
  面前 0.1 停 ③ 无遮挡 = ideal 位级精确。
- **drawHeldHand（3D 视图手）第三人称隐藏**（身体可见 = UX-3 消费；
  手臂持物/弓拉姿态 = 记档偏离）。**2D 手持角标 = HUD 层照画**
  （kickoff 定案；renderUi 不动）。
- **交互/瞄准/箭/声音/云零改动**（全读 m_camera.position/forward：
  First 不变；ThirdBack 自相机 = MC 同款——**偏离 kickoff "瞄准仍自
  眼位" 记档**：相机原点 = 屏幕中心与准星一致，且交互代码零改动）。

## 实现

1. **`src/core/third_person.{hpp,cpp}`**（新，campfire 纯函数范式；
   独立文件 = camera.cpp 保持零 World 依赖的纯视图数学模块）：
   `thirdPersonCameraOffset`（位级同源 Camera::forward）+
   `resolveThirdPersonCamera`（三分支：嵌入穿出 / raycast 面前停 /
   ideal 位级）+ 2 命名常量。
2. **game_frame.cpp** 每帧相机同步：yaw/pitch 先行，
   `m_cameraMode == ThirdBack ? resolve : eyePosition()`（else = 原行
   位级不变）。game_init buildScene 相机初值不动（init 时恒 First）。
3. **input.hpp/.cpp**：GameKey 尾追加 `C`（既有枚举值零移位，kKeyCount
   同步）+ `case SDLK_C` 映射。
4. **game_ui.cpp** handleUiKeys 全局段（F5 块后）：C 切换（locked 门控）。
5. **game_render.cpp** drawHeldHand 门 +`m_cameraMode == ThirdBack`。
6. **版本** v1.37.0 → v1.38.0（CMakeLists 单点 + F3 钉 + banner 实证）。

**零 format（22）/ 零 config / 零内容 / 零音效 / 零新 Game 成员。**

## 变更文件

- `src/core/third_person.{hpp,cpp}`（新）
- `src/core/game_frame.cpp`（相机同步门控）
- `src/core/game_ui.cpp`（C 切换）
- `src/core/game_render.cpp`（drawHeldHand 门）
- `src/core/game.hpp`（CameraMode 注释演进，零行为）
- `src/core/input.{hpp,cpp}`（+C）
- `CMakeLists.txt`（1.38.0 + 2 源）
- `tests/test_third_person.cpp`（新，8 测）
- `tests/test_input.cpp`（+C 映射钉）
- `tests/test_debug_overlay.cpp`（F3 banner 钉）

## 测试

1946/1946（+8）、`-Werror` 0 警告、banner v1.38.0 实证：

- **ThirdPersonTest 8**：offset 轴钉（yaw0/p0 恰 (0,0,4) / p±90° /
  yaw+90° / 半径缩放）+ 中间角解析闭式 ×2 + 长度恒 4 + resolve 空世界
  位级 ideal / 墙前钳止 exact（raycast 分支 3.1）/ **嵌入穿出 exact
  3.9 = 门 S2b 回归钉死**（初版 4.1 = 墙内作废）/ 水 L8 穿流体位级
  ideal / 远处墙（t=4.5>D）位级 ideal + 与 Camera::forward 反平行一致
  ×6 角度（视图矩阵与相机球面同源守护）。

## 门 158（编排者执行，old 侧 = v1.37.0 = c788172 = tm_v138_base）

- **S1 = 第 158 次结构性免疫 + base18 锚连续 168140 B 免重锚**（fresh
  60f 四方 ×2 全字节一致 + 跨版 savenorm EXACT；默认 First = 相机
  else 分支位级 + C 未按下 + 无输入未锁定 = 双结构惰性；**最终二进制
  复验** = 修正后重跑四方全 EXACT）。
- **S2 = C 切换输入跑 12/12 PASS**（协议 = 捕获轮询 Pointer captured →
  windowfocus --sync + 裸 `xdotool key c`【PITS 266】→ 静止 1200f @
  ~70fps；窗口识别 = `pgrep -x tokenmine` 真实 PID【timeout 包装 PID
  无窗 = PITS 344】+ 几何点击时读取【PITS 340】）：
  - side A = 第一人称基线（墙场景无输入）/ side B = 第三人称 + 遮挡
    （墙场景：8 石墙 x∈{0,1} y∈{55..58} z=4，ideal 相机 (0.5,56.61,
    4.40) 嵌 z=4 格）/ side C = 第三人称无墙（fresh 场景）
  - **B×2 / C×2 RAW 字节一致**（第三人称静止态确定性）
  - **皮肤判别量 (224,178,132)±2**（头部顶面全亮色 = 身体世界唯一
    调色板项）：**A = 0 px / B = 1697 / C = 1394**——A 无身体（第一
    人称）+ B 相机出墙（遮挡收敛工作，嵌墙时 = 0）+ B/C 视差单调
    （B 相机 3.49 vs C 4.0 块，面积比 1.22 = 解析 1.31 同量级）
  - A-B diff 104898 采样 px（视角变化）/ B-C diff 83817（0.5 块视差，
    体素场景高对比 = 大 diff 正常量级，图像肉眼复核树/地位移）
  - **存档态**：A≡B savenorm EXACT（**相机模式不入档实证**）/ C vs A
    = 恰 8 墙格对称差 / 三侧 player pos/yaw/pitch/hp 全精确（零漂移，
    P28 探测器）
- **S3** = 1946/1946。**无 S6**（零 parse 面：无 config/JSON/存档字段）。
- **性能 A/B v1.37.0 vs v1.38.0（发布前硬门槛）PASS**：fall 900f 无
  输入三窗 upd med 0.382/0.368/0.413 vs 0.378/0.376/0.389、render med
  0.467/0.445/0.430 vs 0.460/0.438/0.458、swap med 15.62/15.75/15.70 vs
  15.70/15.78/15.67 = 全指标同档；PIPE3-S0 changesApplied **9/3/1 三窗
  逐位**；**终档 fluids 18/18 + blocks 55/55 集合恒等 diff 0 = 同世界
  同成本【结构惰性最强形：默认 First = resolve 零执行】**。

## 偏离/记档

- **C 键**（F5 = 手动存档占用；MC 现代惯例 = C）
- **第三人称瞄准自相机**（非 kickoff 眼位：屏幕中心与准星一致 + 交互
  代码零改动；MC 同款）
- **2D 手持角标第三人称照画**（HUD 层，kickoff "HUD 照画"；3D 视图手
  隐藏 = 手臂持物/弓拉姿态缺失 = 记档）
- **轨道点 = 眼位**（crouch 随降；MC 轨道点 1.62 同款值）
- **遮挡 = 任意非 air 块**（火把/栅栏等薄块也挡相机，MC 对薄块更宽容
  = 保守偏离；穿流体 = MC 同款）
- **相机模式不入档**（读档回 First = MC 同款；会话态）
- 嵌入格极端（t_exit + margin > distance = 贴墙）clamp 到 distance =
  防御界（相机不过轨道中心；实际 t_exit ≤ ~1.1 不可达，注释记档）

## PITS

- **PITS 344**（`xdotool search --pid` 须用真实进程 PID：后台
  `timeout N ./game &` 的 `$!` = timeout 包装 PID，其无 X 窗口 →
  search 恒空 = NO-WINDOW 假故障（门 158 S2 首轮整跑浪费实证）；修 =
  `pgrep -x tokenmine` 取真实 PID（零并发纪律保证唯一）。名搜索
  `^TokenMine` 备选但须 WM_CLASS 排 Firefox【用户浏览器标签同前缀】。
