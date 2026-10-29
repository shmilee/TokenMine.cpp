# PRE-11 HUD 细化（v1.x 矩阵 32/33，v1.32.0，门 147，PRE 行 5/6）

## 定案

原始定义已佚（矩阵短标签"HUD 细化"）= 定案 **心行窄窗回退布局**（消费
KNOWN_ISSUES UI 节 Phase-23 条"饥饿 HUD 在极窄窗口会被裁剪——UI 是像素
正交、越界即裁，无回退布局"）：心行 = [吸收带][心][6px 间距][饥饿|氧气
排] 居中排布，窗口窄时右排（饥饿/氧气 = 更紧急的读数）越右缘被裁。

修复 = **纯函数 `heartRowLayout` 窄窗回退**（`core/ui_layout.{hpp,cpp}`
新文件，campfire 范式单测拥有）：整行超出右缘时把行左移直至**右排右缘
贴屏**——吸收带（行最左）最先被裁（记档 last resort），低于
hearts+gap+right 最小行宽时心本身也裁（无布局能救 sub-row 窗口，记档）；
**宽窗返回 legacy 坐标位级精确**（无输入基线 + 所有 E2E 窗零触碰 = 结构
免疫根）。顺带把 UI 图标度量常量（kUiIconPx 16 / kUiIconGap 2 /
kUiStripGap 6）收口为 ui_layout.hpp 单一真源（drawUI 局部常量改别名）。
右排宽度按**本帧实际渲染的排**预留（氧气 < 150 时氧气 10 图标替换饥饿，
SURV-2 原位语义）。零 format/config/内容/音效/帧状态。

## 实现

- `src/core/ui_layout.{hpp,cpp}`（新）：`HeartRowLayout{heartsX, rightX,
  rowLeft}` + `heartRowLayout(width, heartIcons, absIcons, rightIcons)`
  纯函数 + 度量常量单一真源
- `src/core/game.cpp`：drawUI 心行段改用纯函数（absIcons/oxygen 计算提前
  + 去重）；PRE-10 效果行锚 `effX0` 改 = `heartRow.rowLeft`（宽窗同值）
- `CMakeLists.txt`：mc_core +tests 注册、1.31.0 → 1.32.0

## 测试

**1905/1905（+8：test_ui_layout.cpp 新——宽窗 legacy 位级 / 池锚 rowLeft /
恰好贴合不移动边界 / 1px 过界移 1 / 窄窗右排贴屏心全留 / 窄窗带池吸收先裁
/ sub-row 心也裁 / 零宽右排守卫）**、`-Werror` 0、banner v1.32.0。

## 门 147

- **S1 = 第 147 次结构性免疫 + base18 锚刷新 387726→168140 B**（fresh 60f
  四方 ×2 = new×2 / old(v1.31.0)×2 全字节一致 + new≡old + 跨版 savenorm
  EXACT；锚刷新 = WM 平铺态 1362×744→679×744，PITS 265/315 类环境态，
  四方 clean 后采用）
- **S2 = 宽窗 capture A/B PASS（10/10，矩阵态 1362×744）**：N（裸 HUD）/
  M（5 效果 + 皮甲 4 + 吸收池 4）× 新（v1.32.0）/ 旧（v1.31.0）×2 = 8
  capture 跑【原子中心点击 + "Pointer captured" 轮询 + 尺寸门 + 漂移门
  <0.001 rad（PITS 331/332/334），全跑 cam=(0.0, −0.25) 零漂移】：存档
  证据 4/4【N：eff [] / armor {} / abs 0；M：eff 5 全点名 / armor
  chest·feet·head·legs / abs 4 / hp 20】+ x2 RAW 4 对字节同 +
  **new_N≡old_N 字节同 + new_M≡old_M 字节同 = 代码级免疫**（宽窗布局
  legacy 位级精确的 E2E 实证，三行 HUD 全覆盖）；**半态判别 = 单测钉死**
  （窄窗回退在虚拟 453 宽激活，WideWindowIsLegacyCentered /
  NarrowWindowKeepsRightStripOnScreen / NarrowWindowWithPoolClipsAbsorptionFirst
  等 8 测位级；本门矩阵恰落宽态 = 记录，半态 capture 若 WM 再现可补跑）
- **S3 = 1905/1905 PASS**
- **性能 A/B（tm_v131_perf vs tm_v132_perf，同 g139 插桩，fresh --new
  --seed 123456789 900f nomobs 无 capture）：PASS**（banner 双侧实证
  + PERF-WIN 全指标同档 + changesApplied max 9/3/1 三窗逐位 + **终档
  fluids 18/18 + blocks 55/55 集合恒等 = 同世界同成本**【结构惰性最强
  形：无 capture = drawUI 心行段零执行】）

## PITS

- **PITS 333**（窗口态探测跑的帧数须覆盖读延迟：probe `--frames 30` 在
  60fps quiet 场景 0.5 s 退出 = 窗口在读几何前消失，28 轮全空误判"无
  窗口"）
- **PITS 334**（capture 跑读到的窗口几何 ≠ 帧 200 截图的 framebuffer
  尺寸：WM 运行期重排 → 矩阵态锚定实际 PNG 尺寸 + 点击前几何两次连读
  稳定窗 + 判别带按 H/s 高度自适应）
- **PITS 335**（`( cmd ) &` 起的游戏 `kill $gpid` 只杀子壳 → timeout+
  游戏孤儿存活（9 FPS 时 200 帧 ≈ 22 s）→ 孤儿窗污染
  `xdotool search --class tokenmine | head -1`（假"稳定几何"）+ 孤儿写
  共享 log 的 "Pointer captured"（假捕获）+ 共用存档槽互覆 → killtag
  按 save 槽精确杀本尊）
- **PITS 336**（高负载取景的 capture 跑间歇性纯黑读回：N 场景相机在地底
  熔岩湖旁（流体 churn 13803 格/200f）→ glReadPixels 全帧 unique=1 纯
  黑（Mesa i965 上下文类故障，同场景 60f 正常/70f 黑/70f 又正常 = 负载
  触发非时段；--new 地表取景 7 跑全正常）→ 场景相机移地表开放区 +
  清空 blocks/fluids 编辑（HUD 断言全屏幕空间与取景无关），×4 跑全
  正常；probe 低负载"黑帧 clear"不能预言高负载场景黑）
