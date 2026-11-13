# Phase v3.50.0 — V3-22 用户实机诉求批 3：全景定格布景（周期性 + 全内容）+ 菜单键盘操作（门 240）

> 11-13 用户复验 v3.49.0 报障 + 新诉求：①「接缝还在，不只是树对不上，
> 草地也对不上——拼接的地方两张图的场景不一致，存在割裂。我要的背景
> 图是周期性的，拼接后场景连续，两边看不出接口，像无穷无尽的场景。
> 背景图草地、树木、天空、云朵都齐全，有动物、主人公，还有末影龙。
> 可以离得远一点拍，容得下更多东西」②「再次强调：背景图主要是周期
> 性问题」③「菜单界面需支持键盘操作，如方向键、WASD、以及 Enter」。
> **V3-22**（11-13 用户实机诉求入表，V3-18/19/20/21 先例）。

## 实现

### ① 周期性 = 45° 水平 FOV 的真正 tile 精确（v3.48 修错了轴）
- **根因（门内实证）**：`Mat4::perspective` 首参 = **垂直** FOV
  （mat4.hpp 明写）。v3.48 的「tile 精确」把 45° 喂给了垂直参——
  640×360 tile 的实际**水平**覆盖 = 2·atan(tan22.5°·640/360) =
  **72.7°**，8 向跨 581.8° = **每向重叠 27.7°**：环带每个拼接处
  场景回跳 27.7°（树、草全部对不上 = 用户所见「割裂」）。门 238/
  239 的接缝带只量相邻列差（27.7° 回跳落在雾区/同质地形时量级可
  混过 ≤40 带）= 门盲点。
- **修**：`kPanoramaHfovDegrees = 360/8 = 45°`（水平语义具名）+
  `effectiveRenderVFovDegrees()` = 2·atan(tan(h/2)·H/W)（垂直孪生，
  640×360 下 = 26.24°）+ `m_panoramaHfovDegrees` 覆写面（-1 = 70°
  垂直位级红线）双消费点（投影 + drawSkyObjects 像素换算）不变形
  式。8 向 = 360° 无重叠无间隙，wrap 闭合 = 周期性。
- 帧几何（新俯瞰）：+30 高度、pitch −0.08 rad（−4.6°）——帧顶
  +8.5°（天空带：云 + 龙）、帧底 −17.7°（近地：草/树/动物/Steve）。

### ② 定格布景 = committed scene + 帧 0 程序布景（全确定性）
- **scene 资产** `assets/demos/scenes/panorama.json.gz`（mk_scene.py
  + **ring 面**：chebyshev 方盘每列 1 条 air 编辑 = 强制生成 ±48
  chunk 列（9409 列）——雾端 = min(1000, 48×16) = 768 块的远景地
  平线；`center: "spawn"` 自引用 + radius = **chunk 列**单位
  【门内捕获：初稿按块 = 48 块仅 3 chunk，全景只有小贴片】）。
- **--panorama-scene <name>** seam（GameArgs + 解析 + 互斥【单独
  = 命名拒收】+ init 存在性拒收 + buildScene 前 m_demoSceneName
  置位）；缺省 = fresh config 种子世界位级不变。
- **同步列烘焙** `meshLoadedColumns()`（--panorama 一次性面）：
  流送网格级联只在玩家贴边时 engage（静止瞭望永不扩盘——5000 帧
  预热 6 分钟的门内捕获）+ `loadedColumns` 镜像 ≠ 世界 chunk map
  【scene ring 列 49-of-9400 门内捕获】而 draw pass 恰遍历镜像 →
  世界权威枚举 + 镜像回填 + MeshJob worker 本体逐字同步化（3×3
  快照 + 原 ChunkMesher + integrate 落地 + updateChunkMeshes 单
  遍上传）。
- **程序布景**（renderPanorama，帧 0 确定性，进程一次性——永不
  入档、游戏 boot 从不走）：
  - **Steve（主人公）** = 第二玩家（netAddPlayer(1) 直调——solo
    安全实证：不碰 net 成员；local body 第一人称 skip = 非 local
    才绘）立于相机东偏 110 块的生成器表面上（帧底 −11° 带）。
  - **动物群** `spawnPanoramaMobs` = sheep/cow/pig/chicken 4 只
    帧 0 布景（池生成锚定 primary 玩家 = 瞭望正下方 −90° 出帧的
    门内捕获 → 可见中距须程序布景；每 index 黄金比例盐隔离流；
    立于 surfaceHeight+1 无坠落）。
  - **末影龙** `spawnPanoramaDragon` = spawnDragon 的 overworld
    孪生（**无回调静默飞行**：shoot/breath/growl 回调 = End 玩法
    接线，全景不设 = AI 守卫 no-op，流契约每帧 ambient 1 抽零
    变化；轨道锚定**相机**【初稿锚 feet = 龙在相机下 15 块出帧的
    门内捕获】：东偏 150 + 高 15 → 仰角 4.8–7.1° = 天空带内；
    300f 预热 = 轨道弧确定性位移）。
  - 300f 布景预热（帧锁定 fixed-dt = 纯函数）。
- **资产** 2814425 B（×2 字节恒等，--panorama-scene panorama）。

### ③ 菜单键盘操作 = 统一 m_menuSel 状态机
- **新增 4 GameKey**（Up/Down/Left/Right，G 后插入，FlyToggle 保
  枚举尾；mapKey SDLK_UP… 映射；**玩法零消费方** = demo/net/sim
  位级红线的结构根；authorKeycode 词表 +4 名保封闭集不变式）。
- **统一选中态** `m_menuSel`（-1 = 未选中 = 无键盘操作前零像素，
  S1 锚腰带）：鼠标 hover 同步 sel（每帧含测，17c 派生面改为状
  态面）+ 键盘移动（menuMoveSel 纯函数：wrap + 首按进入 + clamp，
  test 钉）；页/tier 切换重置 -1。渲染 = sel 驱动白条（六页），
  click 面 = hover 原样（鼠标路径位级不变）。
- **各页语义**：Title/多/加 = 项选择 + Enter 激活 sel；Login =
  {开始,返回} 条移动（Enter 恒 = 开始 = 页主动作 17c 不变）；
  Settings = 行选择（Up/Down 行移 + **Left/Right = Dec/Inc/Toggle**
  + Enter 恒 = 保存 17b 不变）；Tutorial = tier1 Enter/Right 进章
  / tier2 Left 回章 + Enter/Right 启动（coop 拒收带不变）。
- **文本输入门控**：WASD 移动仅 `!m_textInputOn`（打字的 'w' 不
  移动选中）；**方向键恒可用**（不产 TEXT_INPUT，与打字零冲突）。

## 门结论（门 240 全 PASS）

- **3028/3028**（+5 MenuKeyboardTest【MoveSel 10 面 / Title Enter 激活
  sel / Settings LeftRight 行 / Tutorial 键盘 / Multiplayer+Join
  Enter】+ banner re-pin 1）；`-Wall -Wextra -Wpedantic -Werror` 恒
  0 警告。format 33 不变 / sim 2 不变 / 存档与网络协议零变化（4 方向
  键 GameKey = 菜单零消费方——FrameState u64 掩码宽度不变、demo 编
  解码词表 +4 名仅 author 面、sim 面从不 press）。
- **S1 重锚 base20 = v3.50.0 布景标题 247705 B**【f60 无输入 ×2 RAW
  逐字节 + defined mouse (10,10)【sel = -1 零像素腰带】+ 槽不创建断
  言 PASS；旧锚 diff 367500 px 72.7% = 全背景布景场景替换（天空/
  云/森林/动物/龙）= 用户内容诉求论证 + 版本行】。
- **周期性臂（新断言，TDD 前置复现 + 修复后 PASS）**：边界梯度 max
  15.1 vs tile 内参考 8.7（**1.7x ≤ 2x PASS**——27.7° 回跳面 = 5x+
  的执法带；v3.48/49 的 ≤40 绝对带双漏的门盲点转正为比值带）+ wrap
  8.0 无异常 + 覆盖臂（相邻 tile 差全 > 0.01 + 无 readback 滞后
  WARN——431 回归面）+ 内容带（顶行 RGB [157,197,236] 天空蓝 / 底行
  [69,101,60] 绿 + **龙 24×11 px 剪影扫描实证**（x4152-4175 y12-22，
  天空带唯一暗簇）+ 动物群小点簇 + Steve 立于表面）+ ×2 字节恒等 +
  ≡ committed（2814425 B 资产）。
- **键盘臂 ×4 ×2 RAW EXACT（帧锁定 = 选中态纯函数执法）**：a) 无输入
  标题【sel=-1 零像素】/ b) Down×2+Enter = 多人页【导航 + 激活】/
  c) Down×3+Enter+Right = 设置页行调整【Left/Right = Dec/Inc 面】/
  d) **s s Enter = b 像素恒等**【W/S ≡ 方向键的最强孪生形】；各臂
  与标题页 diff 非零（导航真跑）。
- **menu-open 臂回归**【tutorial f60 ×2 RAW EXACT】+ **S6 三面 rc 2
  命名拒收**【--menu-open bogus / --menu-open × --demo /
  --panorama-scene 单独 = "--panorama <out> is required"】。
- **canary 4×2 新≡旧（tm_v349 v3.49.0）**：树内 RAW r1==r2 EXACT ×4
  + savenorm 跨树 EXACT ×4 + **跨树 RAW EXACT ×4**【demo 路径位级
  ——方向键玩法零消费 + 全景面 = --panorama 一次性结构免疫】。
- **性能 A/B 同档**【OW 900f --new 双侧 g139：new upd med
  0.392/0.579/0.598 vs old 0.622/0.644/0.653 median 同档【old win1
  swap max 15322ms = 一次性 OS 停顿先例】+ render/swap 同档 +
  changesApplied 9/2/1 逐位 ×2 + **终档跨树 savenorm EXACT** = 全景
  布景 --panorama 一次性面 + 键盘菜单面 = 玩法路径全冷结构零成
  本】。
- **门内捕获 PITS 432 五坑**【①perspective 首参 = 垂直 FOV（v3.48
  tile 精确喂错轴 = 每向重叠 27.7° 回跳割裂——绝对值接缝带双漏）
  ②静止相机不扩流送（5000 帧预热 6 分钟零网格 = 同步烘焙面）
  ③loadedColumns 镜像 ≠ 世界 chunk map（49-of-9400 = draw pass 恰
  遍历镜像——世界权威枚举 + 镜像回填）④ring 半径单位 = chunk 列
  （初稿按块 = ±3 chunk 小贴片全帧纯天空）⑤布景锚定 = 相机相对
  （feet 相对初稿：龙在相机下 15 块出帧 + Steve/池动物 −90° 出帧
  + 池锚 primary → 可见中距须帧 0 布景）】。
