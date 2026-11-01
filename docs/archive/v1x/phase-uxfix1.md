# UXFIX-1 用户实机反馈渲染修复（v0.31.0，门 114）

10-26 用户实机报障两条（v0.30.0 release），复现 + 根因 + 修复：

1. **白盒**："右键后，物品栏中很多物品都是半透明白盒子，e 键之后的背包里
   也有白色半透明盒子"
2. **黑块**："挖方块的时候，方块显示全黑（裂纹白线在，但整个块面纯黑）"

## 根因（同一泄漏家族：PRE-4 云引入的 per-draw 状态泄漏，v0.28.0 起潜伏）

basic shader 的 per-draw uniform（`u_useTexture` / `u_color` / `u_alpha`）
与 GL blend 状态都是**跨 draw 持久**的——每次 draw 只覆写自己用的子集，
其余继承上次 basic draw 的遗留值。云（PRE-4）与光束（P83）是纯色 draw：

- **bug 1（白盒）= uniform 泄漏**：`drawClouds` 设 `u_useTexture=0` +
  `u_color=(1,1,1)` + `u_alpha=0.55` 后，帧内再无任何 basic draw 覆写这三者
  → 热栏/面板的**块立方图标**（`renderUi` 末尾，唯一不设自身
  u_useTexture/u_color/u_alpha 的 basic draw）继承云配方 → 半透明白盒
  （u_color 白 × u_alpha 0.55 叠暗槽底 = (150..182) 平灰）。
  **CUBE PROBE 实证**（临时 glGetUniformiv，已删）：立方 draw 时继承
  `u_useTexture=0 u_color=(1.0,1.0,1.0)`。旧帧槽区实测 (182,182,183) 平灰
  方差 ≈0 = 白盒本体。
- **bug 2（黑块）= blend 泄漏**：`drawBeaconBeams` / `drawClouds` 尾部各自
  `disableBlending()` → 泄漏进 `drawWorldOverlay`（裂纹立方，cracks.png
  白线 + 透明底）→ 无混合绘制 = 透明底 (0,0,0,0) 以**不透明**写入
  → 纯黑盒覆盖被挖块面。

两者都只在 **locked + 有云绘制**的路径暴露：无输入运行指针不锁定（热栏/
面板不画、overlay 门 playing 才画）→ 113 次无输入门全盲 = 结构性免疫
从未见过。用户实机（锁定 + 挖掘/开面板）首触发。

## 修复（4 处，全部 `src/core/game.cpp`，零新文件/零 config/零内容）

1. `drawBeaconBeams`：删内部 enable/disableBlending（blend 是调用方
   混合段的状态，per-draw 函数不拥有）
2. `drawClouds`：同款
3. `drawWorldOverlay`：**自持混合**——playing 门后 `enableBlending()` +
   函数尾 `disableBlending()`（裂纹/轮廓本就是 alpha 合成，语义自洽）
4. `renderUi` 立方循环（受害者侧防御）：显式 `bind + u_useTexture=1 +
   u_color(1,1,1) + u_alpha=1.0` = 纹理配方，不再继承任何世界 draw 遗留

无输入 = 位级恒等（S1 实证：new≡old 字节一致；锁定零事件 → 立方/overlay
路径零运行，云/beacon 的 blend 改动被混合段自身状态覆盖 = 净零）。

## 门 114

- **S1 PASS = 第 114 次过门，结构性免疫**：fresh 60f 无输入
  s1_na/fix1/c1-c5 七帧（新旧二进制）md5 全同 `ca639e50`。
  **base18 锚刷新** = 同帧——旧锚（15:4x/17:2x 拍，含 20235 云像素）
  与 19:4x 起帧的差**全部**在顶带云环 = PITS 265 机器态（vd80 流送 60f
  时是否加载 d~140 高原剪影取决于 debug/info 日志的 fps 拖累），与本次
  变更无关。纪律更新：锚对云带只做连续性参考，核心断言 = 同批同机
  new≡old A/B。
- **S2a 挖掘 A/B PASS**（stone@(0,54,-3)，手挖，L 按住至 run 末）：
  全分辨率块区 x[560..800] y[300..560] old **纯黑 38314 px**（bug 2
  本体，裂纹立方 (0,0,0,0) 不透明覆盖）vs new **石色 40194 px**
  （裂纹白线正常合成）；步 2 采样 9578 黑 / 10035 石。
- **S2b 面板 A/B PASS**（E 开面板 + 热栏行同帧双区）：槽区 old 平灰
  白盒 (182,182,183) 方差≈0 主导（面板槽 420 白盒/77 纹理 比 5.5；
  热栏槽 787/76 比 10.4）vs new 纹理立方（面板 225/219、热栏 277/419，
  石 (103,103,103) / dirt 棕 (69,48,33)）；帧 diff 5061 px 全部限定
  槽区 + 顶带云（PITS 265 环境差，非修复产物）。
- **S3 PASS**：1672/1672（探针删后重跑）、`-Werror` 0 警告。
- **S6 无**：零 parse 面（config/内容/格式全不动）。
- **性能 A/B PASS**（fall 900f，tm_v0310_src vs tm_v0300_src 插桩对，
  双侧 timing-clean 接受）：三窗 fluidCells **123879/167781/236573 双侧
  逐位一致**（同世界）+ upd med 45.97/50.57/50.23 vs 47.43/49.78/49.98、
  render med 0.364/0.569/0.757 vs 0.371/0.547/0.734、swap/flush/stream
  全同档（±6% 内，win2/3 render +3% = 0.02 ms 级环境噪声——无输入跑
  立方/overlay 零运行 = 本修复结构惰性）= **成本可忽略无回归**。

## 输入协议定案（10-26 实证链，后续输入跑范式）

- 首帧 GL 编译期 L/R 同 pre-lock poll = R 边沿被 !locked 分支丢弃 →
  **L 点击后轮询 debug 日志 "Pointer captured" 才发后续动作**
  （点击重试至 captured，≤12 次）
- **"Pointer captured" 是 MC_LOG_DEBUG**——`--log-level info` 跑输入
  协议 = 盲重试（16 点击零信号实证）
- **键送达 = `windowfocus --sync` + 裸 `xdotool key E`（XTEST）**；
  `key --window $WID` = XSendEvent 合成事件 **SDL 忽略**（PITS 266）
- 面板签名断言 = 中心像素 (681,372) ≈ (27,27,33) kPanel 暗底 + 槽区
  内容；**勿用大面积暗区**（世界林荫阴影 48k px 假阳性实证）

## 偏离

- 云/光束不再自管 blend = 混合段所有权上移（段首 enable/段尾 disable
  语义不变）；`render.clouds=false` 且无信标时混合段状态由段自身维持
  = 行为位级不变（S1 七帧实证）
- 手持/箭头恒白光照 = PRE-6 既有偏离，不受本修复影响

## 变更文件

- `src/core/game.cpp`（4 处，+19/−5 行）
- 本存档 + `docs/PITS.md`（266）+ `AGENTS.md`（现状行/坑计数）
