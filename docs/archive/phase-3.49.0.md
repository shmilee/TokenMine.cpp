# Phase v3.49.0 — V3-21 续：全景读回滞后修复（门 239）

> 11-13 用户复验 v3.48.0 报障：全景接缝仍在，且诊断直指「背景两边的
> 树木被截断展示」。复测实证 = **门 238 捕获的环带角度覆盖缺陷**：
> 8 个 tile 中 tile 1 与 tile 0 全 640×360 像素恒等（mean abs diff
> 0.0）——环带实际内容 = [v0][v0][v1]…[v6]，**v7 的 45° 场景缺失**，
> wrap 处两端的「截断树木」= 缺失窗内的同一棵树被劈成两半且不闭合。

## 根因

- `readFrontBuffer` 从**默认读缓冲** glReadPixels（全项目零
  `glReadBuffer` 调用）——双缓冲上下文的默认读缓冲 = **GL_BACK**。
- renderPanorama 的旧序 = `render → swapBuffers → read`：swap 之后
  后缓冲持有的是**上一帧**的内容（刚呈现到前缓冲的那帧被换下来）。
  于是每次读回都滞后一帧：tile 0 ← 预热帧（恰好 = view 0 相机）、
  tile 1 ← view 0、…、tile 7 ← view 6——**view 7（yaw 292.5°–
  337.5°）从未入带，view 0 重复一次**。
- 门 238 未捕获的原因 = 接缝带断言只查相邻列差（滞后产生的仍是
  「合法场景」环带，列间差与正常边界同量级）+ ×2 字节恒等（Mesa
  行为确定性——两跑同滞后）。**新门盲点族**：环带类资产须断言
  **角度覆盖**（相邻 tile 必异 + wrap 对连续），不止列差。
- v3.47 门 237 的 ×2 恒等/≡ committed/目检全绿同因失效——该环带
  一直带着 45° 缺口发布（用户复验抓获）。

## 修

- **读回时序 = `render → read → swapBuffers`**：swap 之前读后缓冲
  = 刚画完的帧本身，呈现竞态彻底出捕获面（确定性一级）。take-
  Screenshot 的「swap 后读 = 落后一帧」既有语义不动（PITS 391②
  纪律，游戏截图面独立）。
- `readFrontBuffer` 头注释如实化（默认读缓冲 GL_BACK + 两种时序
  语义分面）。
- **in-code 滞后回归腰带**：拼接后相邻 tile 全等 → WARN 点名
  （防御纵深；门臂 = 本种子 ground truth 断言相邻 tile 必异，
  mean abs < 0.01 判 FAIL——路径性平坦世界不误报，因为 WARN
  只针对像素恒等）。
- 资产再生：`--panorama` ×2 字节恒等 + 覆盖臂 + 边界带，cp 进
  `assets/textures/panorama.png` 提交。

## 门结论（门 239 全 PASS）

- **3023/3023**（banner re-pin 1，读回修复零新增测试）；`-Wall -Wextra
  -Wpedantic -Werror` 恒 0 警告。format 33 不变 / sim 2 不变 / 存档与
  网络协议零变化（改面仅 --panorama 捕获序，纯表现资产）。
- **覆盖臂（新门臂，TDD 前置复现 + 修复后 PASS）**：旧带 tile 1 == tile 0
  全 640×360 像素恒等（mean abs 0.0）+ 新 tile5/6 == 旧 tile6/7 逐位 0.0
  = 逐帧滞后实证；新带相邻 tile 差 8.06/13.50/15.59/9.82/3.68/2.04/3.23
  全 > 0.01 **TILE-DIFF ARM PASS** + 无 in-code WARN + wrap 对 4.64 =
  正常场景梯度 + 边界带 [11.5, 4.1, 10.8, 28.0, 1.9, 2.1, 3.4, 4.6]
  （28.0 = 雾-地形高对比边界，目检零跳变）+ ×2 字节恒等 + ≡ committed
  （401 KB，tile 0 位级不变 0.00，tiles 1–7 前移一格）+ wrap 区目检
  两半互补、截断树消失。
- **S1 重锚 base20 = v3.49.0 全景标题 47967 B**【f60 无输入 ×2 RAW
  逐字节 + defined mouse (10,10) + 槽不创建断言 PASS；旧锚 diff 仅
  73 px 0.0% = 版本行 v3.48.0→v3.49.0 有界（PITS 426② 菜单期逐版重锚
  先例——cover-fit 可见窗未含 v7 区 = 背景面近乎恒等）】。
- **menu-open 臂回归**【--menu-open tutorial f60 ×2 RAW EXACT + bogus
  rc 2 命名拒收 + ×--demo rc 2 互斥拒收——v3.48 面位级不变】。
- **canary 4×2 新≡旧（tm_v348 v3.48.0）**：树内 RAW r1==r2 EXACT ×4 +
  savenorm 跨树 EXACT ×4 + **跨树 RAW EXACT ×4**【改面仅 --panorama
  捕获序 = demo 路径全冷位级实证——读回时序修复不进游戏帧路径】。
- **性能 A/B 同档**【OW 900f --new：new upd med 0.652/0.604/0.596 vs
  old 0.576/0.596/0.592 median 交错入带【old win1 render max 9.99ms +
  swap max 14638ms = 一次性 OS 停顿先例】+ render/swap 同档 +
  changesApplied 9/3/1 逐位 ×2 + **终档跨树 savenorm EXACT** = 零世界
  面结构零成本】。
- **门内捕获 PITS 431 两坑**【①双缓冲默认读缓冲 = GL_BACK：swap 后读
  = 上一帧——8 tile 逐帧滞后 = view 7 缺失 45°（×2 恒等 + 列差接缝带
  双双抓不到的门盲点族）②门的视觉验证须到内容完整性——用户实机诊断
  「树木截断」= 内容级缺口非像素级跳变；环带类资产门臂必含角度覆盖
  断言（相邻 tile 必异 + wrap 对连续）】。
