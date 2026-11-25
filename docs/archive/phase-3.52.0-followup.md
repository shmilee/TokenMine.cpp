# v3.52.0 后续批（门 243）— 4.0 预发布实机反馈：demo 固定第三人称近轨 / 主菜单 UI ×4 / 字体 +2 / ESC 暂停面板

> 2026-11-24/25 用户实机 6 条报障。4 代码批 + 1 文档批提交（36b9dc4 / f21f147 /
> 294f3e1 / d8d9aef / 本批）。banner 维持 3.52.0（无版本 bump = S1 版本行与
> banner test 双不动）。批量 9 支 demo 重拍 = 本批提交后的下一管线。

## ① demo 固定第三人称 + 2.5m 近轨 + 录制 C no-op（用户 11-24 定案）

用户否决第一人称默认（「demo 视角要固定第三人称」）+「轨道距离 4.0m 太远
画面空旷，贴脸遮挡可接受」。

- `runDemoReplay` 初始化 `m_cameraMode = CameraMode::ThirdBack` +
  `m_thirdPersonDistance = kDemoOrbitDistance`（third_person.hpp 新常量 2.5f；
  `kThirdPersonDistance` 4.0f 默认不动）。
- **录制 C beats = no-op**：handleUiKeys 的 C 分支 `m_demoMode !=
  DemoMode::Replay && m_window.relativeMouseMode()`（原 `m_demoPlayer ? true :`
  形替换）——观众 C 移除是 v3.52.0 门 242 的 PITS 434 碰撞面（观众 C 与录制 C
  共写 m_cameraMode + aimForward 前视翻转 = 注入交互射线被翻转 180°），录制
  的 C 现在同样无害化（固定 ThirdBack 下 C 无处可去）。Record 保留 C（作者
  实况预览）。
- 后视角 aim = 玩家基视角原文（前视翻转仅 ThirdFront——Replay 已不可达 = 死码
  保留不动）。观众滚轮 = 速度梯（v3.52.0），永非缩放 → 轨道距离恒。
- **位级论证**：仅渲染相机动（第一人称眼位 → 第三人称 2.5m 轨道），录制的
  编排（注入输入序列 + sim）位级重放 = canary 4 ×2 RAW + **savenorm ≡
  第一人称时代存档 EXACT**（表现层证明，门 242 同款纪律）。唯一正面段例外
  = guide_night f10899（Front 段 mouse_down），批量重拍夜战时按第三人称口径
  重做。

## ② 主菜单 UI ×4（用户 11-24 实机报障）

1. **教学条目宽窗居中**：行 rect 固定 `kRowX=60, kRowW=333` —— 60 恰是
   453 锚定宽的 `(w−333+1)/2`，宽窗口就偏左。改 `(w − kRowW + 1) / 2` 任意
   窗宽居中（锚定几何值恒等 = 布局测试原位全绿 + 853 宽回归钉 x=260）。
   行标签 `centerX`（原 `r.x+10` 是全菜单唯一左对齐）。
2. **设置页返回钮**：六页唯一无可见返回（只靠 ESC）。+`MenuButton back`
   （0.92h，保存下方，70px 同款，控件 id `kMenuSettingsBack=33`）+
   menuSettingsControl/Step 双通路（点击 = ESC 孪生）。
3. **子页操作按钮字号统一 1.5**：用户报「保存和返回字号不一致」——根因 =
   返回钮抄了 `kMenuItemScale` 2.0（标题页大项面），同页保存 = 1.5。口径 =
   标题页五项保持 2.0（主菜单设计），所有子页操作按钮（开始/保存/返回/创建
   游戏/加入游戏）统一 1.5（顺带收了教学页返回钮）。
4. **首页全景平移减半**：`m_menuFrame % width` → `(m_menuFrame/2) % width`
   （85s/圈 → 171s/圈；帧锁定纯函数 ×2 RAW 恒等保持）。S1 重锚 base20 =
   247963 B（半速 pan 面，菜单期逐版重锚先例 PITS 426②）。

## ③ 字体 +2 停/暂（暂停面板字形预备）

- charset 469→471：停 0x505C（cell 177）/ 暂 0x6682（cell 342）代码点序
  插入 + 后缀 re-cell（≥0x505C 的 +1 / ≥0x6682 的 +2）。
- atlas 256×2272 **不扩**（NEW_CJK_MAX_CELL 563→565；566..567 尾 2 格
  headroom 留证）。top-512 红线恒等；生成器幂等双跑字节恒等（md5
  f3336dd3b427bc949bf07b28b0fbfb1f）。
- 字形安全措辞核证：已暂停/继续/保存退出全在 471 表；并/菜/休/息/憩缺 =
  「保存退出」（非「保存并退出」）措辞规避。
- **FNV 重钉 = PITS 439**：tests 的 fnv1a64Hex 用 basis
  1469598103934665603（标准 FNV-1a 64 少一位的历史变体；pathfind.cpp 同款，
  net assetFNV 用标准 basis 0xcbf29ce484222325 = 两套并存）。一切历史 font
  pin 皆出该函数——标准算法手算新值 a29b076df0526872 = 假 FAIL（门内实证），
  正确 pin = 函数实际输出 **f5c00ef99dad4594**。不修函数（修 = 全部历史 pin
  作废）。
- test_font 全重钉：count 471 / last-index 470 / 成员 cell 5 钉（利锋限魔猪
  +1/+2 移位）/ max-cell belt 565 / 扩展表 62 条目移位 + 2 新成员 /
  GlyphUv 9 锚移位 + 停/暂 2 新锚。

## ④ 游戏内 ESC 暂停面板（用户 11-25 报障：「ESC 直接退出没有确认」）

- **触发**（runGameplayLoop，ESC 检查重构）：窗口关闭 = quitRequested 独立
  先行（暂停面不能困它）。`escFree` = 指针自由 ∧ 面板关 ∧ 存活（原前置
  原样）→ wasPressed(Escape)：**solo ∧ !netActive() = 开/关暂停 toggle**
  （双击形：首 ESC 释镜在 update 原文位级，次 ESC 开面板在 loop）；
  **netActive() = 遗留直接退出**（锁步不能停，偏离记档）。m_gamePaused 会话
  态零 format（demo 暂停同款偏离）。
- **暂停帧**：dt 块**先行**（lastFrame 保鲜 = 恢复无 dt 跳）→ skip
  update/net 钩子/record append/m_frameCount++（模拟全冻结，PITS 391⑥
  demo 暂停 dwell 孪生）→ render + swap（世界 + 帷 + 面板）。
- **面板**：0.55 暗帷 + 已暂停 @2.0 + 继续/保存退出 @1.5（自含 UiBatch，
  demo 面板同位 flush = 主 2D + cube icons 后/tooltip 前；hover = 菜单
  kHoverBar 面白条 0.30 + 3px 橙左缘）。词表单源 kMenuPauseTitle/Resume-
  Label/SaveQuitLabel + menuPauseLayout/menuPauseControl/menuPauseStep 纯
  函数（test 孪生 3 面：布局门几何+宽窗钉 / step 4 面 / 词表 3 面）。
- **动作**：继续 = ESC 或点击【点击恢复**立即 setRelativeMouseMode(true)**
  = MC 回游形，免二次点击锁镜】；保存退出 = 原退出路径（loop 尾 writeSave
  单收口，rc 0 + 干净退出）；点击空白 = None（帷吞击）。

## 门 243 证据

- **3031/3031**（+3 MenuPauseTest）；-Werror 恒 0 警告。
- **冻结证明**（TDD 形双判别）：暂停中相隔 2s 墙钟两帧活捕 **RAW 逐字节
  恒等**（sim 冻结权威——云/时钟/粒子全停）+ 对照（未暂停）运行同墙钟
  偏移活捕全帧 diff（mean-abs 138，A 冻 ~f130 vs B 跑 ~f255）。
- **保存退出臂**：rc 0 + 存档 766 B（暂停时 sim ≈f60 的世界态，无推进）+
  日志「Exit requested (the pause's panel's 保存退出)」。
- **继续臂**：恢复日志 + 准星实证（重捕获后第一人称锁定面）+ --frames 300
  干净退出 + 存档 1057 B > 766 B（恢复后模拟推进）+ 零 relative-mouse-mode
  失败 WARN。
- **net 客户端臂**：ESC = 遗留「Exit requested, shutting down」rc 0，零
  「Game paused」（结构不可达实证）。
- **S1** ≡ base20 247963 B ×2 RAW（字体 re-cell 像素惰性免重锚——atlas
  尺寸不变 = 归一化 v-UV 不变，PITS 425④ 白名单不复发）。
- **canary 4** ×2 RAW + savenorm ≡ 第三人称时代参照（pb_r1 ≡ tp_r1 EXACT
  ×4——字体/暂停零 sim 影响）。
- 宽窗/锚定窗面板截图（用户实机验收面）。

## 门内捕获 PITS 438（门臂输入双坑）

1. **xdotool ESC tap 伪重复**：无头 X 下 `key Escape` tap 除物理按下外 ~0.6s
   后再来一个 KEYDOWN（keyup 在 SDL grab 态被吞 = 键「按住」→ X11
   auto-repeat ~660ms 延迟再按）→ 每 tap 可能 toggle 两次（首臂实证：
   pause@12.539 → 幽灵 resume@12.805）。门臂协议：**不得假设 tap 数→状态
   映射**；每 tap 后 drain 1.6s + 读日志尾态（Game paused / Game resumed
   的最后一个）+ 不对齐再 tap 重对齐（6 次上限）。
2. **click --window 合成事件 SDL 不收**：xdotool `click --window` 走
   XSendEvent（send_event=True）= 按钮事件到不了 SDL——指针位可被
   getmouselocation 钉在按钮正中，点击却静默落空（P3 首跑 15 分钟挂死 =
   此坑）。改 `mousedown 1` / `mouseup 1` **无 --window**（XTest
   硬件形，落指针所在窗）= 一发入魂。
