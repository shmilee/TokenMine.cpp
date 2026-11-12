# Phase v3.48.0 — V3-21 用户实机诉求批 2：全景接缝修复 + 菜单 demo 返回教学菜单（门 238）

> 11-13 用户实机 2 条反馈（v3.47.0 首用）：①全景背景从右往左平移时
> 右侧和左侧衔接处有明显接缝 ②菜单进入的教程 demo，Q 退出应该回到
> 教学菜单（与命令行 --demo 的 Q = 进程退出区分）。**V3-21**
> （11-13 用户实机诉求入表，V3-18/19/20 先例）。

## 实现

### ① 全景接缝 = 45° 视场角 tile 精确拼接
- **根因**：游戏 FOV = 70°（kFovDegrees），8 向拼接步长 45° =
  每帧重叠 25°——环带实际横跨 560° 角度内容，pan 在 360° 处 wrap
  = **200° 跳变**（用户可见的硬接缝）+ 相邻帧 25° 回卷重复带。
- **修**：`kPanoramaFovDegrees = 360/8 = 45°`（具名）——8 向恰好
  铺满 360°（零重叠零间隙）→ view7→view0 闭环连续 = GL_REPEAT
  wrap 无缝。实现 = render 的 FOV 覆写面：`m_panoramaFovDegrees`
  成员（-1 = 游戏 70° 位级红线）+ `effectiveRenderFov()`（render
  投影 + drawSkyObjects 像素-世界换算双消费点）；renderPanorama
  捕获期置 45°、结束恢复。
- 资产再生：`--panorama` ×2 字节恒等 + ≡ committed（新 384 KB，
  45° 视场 = 每向更宽的地平线视野）；门臂 = 环带 8 个 tile 边界 +
  wrap 列的 mean-abs-diff 带（≤40 = 亚像素边缘位移域；拼接错误
  显示 100+）。

### ② 菜单 demo Q = 返回教学菜单（进程干净重启形）
- **用户诉求**：菜单进教程 → 看完 Q → 回教学菜单（≠ CLI --demo
  的 Q = 进程退出）。
- **架构裁决**：回放需要场景初始态；项目无「就地重建世界」先例
  （buildScene 3300+ 行、Worker 线程/GL 网格/三维度实体——重建 =
  大重构超出诉求批）。**返回 = 进程干净 re-exec**：teardown（池
  drain + 音频 + 窗口/SDL）+ `execv(/proc/self/exe, --config <cfg>
  --menu-open tutorial [--save <slot>])`——**同 pid 换新镜像**（窗
  口关闭重开 = 重载面，~2s），每个 demo 恒得 fresh 场景，菜单深
  链回教学页。诚实形：不假装无缝（MC 的 in-process level reload
  同款代价，只是短）。
- **`--menu-open <page>` CLI seam**（V3-21 新面）：封闭词表
  title/login/multiplayer/settings/tutorial（Join 子页 = 多人页内部
  态，永不 boot 页）；纯函数 `menuOpenPage(word)`（test 钉 5 + 5
  拒收面）；**五形 bypass 旗全部互斥拒收**（--demo/--record/--
  demo-author/--listen/--connect/--script/--new/--panorama/--list-
  demos × --menu-open = 命名错误）；init 菜单分支消费（页状态初值）。
- **区分面**：`m_menuDemoReturn`（run() menuExit 2 置位）+
  `m_demoQuitByAudienceKey`（runDemoReplay 的 **Q** 键分支置位——
  窗口关闭 = 进程退出形不变，应用的退出是应用的退出）；run() 回放
  后检查双旗 → execRestartToMenu()。**CLI --demo 的 Q 行为零变化**
  （m_menuDemoReturn 缺省 false = 位级红线）。
- 演示 pacing 注意：demo 的 dt = 录制机节奏（本批机器 ~20fps 录制
  → guide_nether 1× ≈ 9 min；2× wheel ≈ 4.5 min——门臂用 2× +
  finished 日志行轮询定时 Q）。

## 门结论（门 238 全 PASS）

- **3023/3023**（+1 MenuOpenTest 10 面【5 词 + 5 拒收】+ banner re-pin 1）；
  `-Wall -Wextra -Wpedantic -Werror` 恒 0 警告。format 33 不变 / sim 2 不变 /
  存档与网络协议零变化（--menu-open = 纯菜单面，WELCOME/HELLO 不动 = 混版
  互拒不受影响）。
- **S1 重锚 base20 = v3.48.0 全景标题 47961 B**【f60 无输入 ×2 RAW 逐字节
  + defined mouse (10,10) + 槽不创建断言 PASS——菜单退出世界未建；
  旧锚（v3.47.0）diff 188762 px 37.4% = 全背景替换，论证 = 45° tile 精确
  环带 = 用户接缝诉求的视觉变更面】。
- **全景接缝臂**【--panorama ×2 字节恒等 + ≡ committed 资产（384 KB 新
  45° 视场）】：环带 8 tile 边界列 mean-abs-diff
  [4.8, 11.5, 4.1, 10.8, 28.1, 1.9, 2.1, 4.7] + wrap 4.7 →
  **SEAM BELT PASS（max 28.1 ≤ 40 = 亚像素边缘位移域；拼接错误显示
  100+）**。28.1 = 4→5 边界雾-地形高对比边界的 1-px yaw 步移梯度（目检
  留证 boundary45.png：零跳变）。
- **menu-open 臂**【--menu-open tutorial f60 ×2 RAW EXACT（deep-link 确定性）
  + 截图目检 = 教学页 5 章 + 全景 ✓】+ **S6 两面 rc 2 命名拒收**【--menu-open
  bogus = "invalid --menu-open: 'bogus' (title/login/multiplayer/settings/
  tutorial)" / --menu-open × --demo = "menu-bypass flags" 互斥】。
- **demo 返回臂 ×2**【菜单 → 教学 → 维度远征 → guide_nether → 2× wheel →
  finished 日志行轮询 → Q → re-exec】：两跑 **日志 banner 计数 ×2**（同 pid
  重载判据，X 窗 id 复用不作数）+ "Demo replay finished (10865 frames)" +
  "Demo replay quit (audience)" + "menu: the demo's return — the process's
  re-exec to the menu's tutorial's page" + 重启后 "menu: tutorial — the
  demo's launch" 链路完整；重启菜单 import 活捕 ×2 留证（**活捕 ≠ 帧锁定
  截图**：pan 相位随墙钟，PNG 头区字节差 = 捕获方式工件——状态确定性由
  menu-open 臂帧锁定截图执法，流程证据由 banner+日志执法，非产品缺陷）。
  CLI --demo 的 Q = 进程退出形不变（m_menuDemoReturn 缺省 false = 位级
  红线，canary 臂实证）。
- **canary 4×2 新≡旧（tm_v347 v3.47.0）**：树内 RAW r1==r2 EXACT ×4 +
  savenorm 跨树 EXACT ×4 + **跨树 RAW EXACT ×4**【v3.48 零字体 = v-UV 不
  变，PITS 425④ 一次性门面终结——门 237 的文本边缘白名单不复发】。
- **性能 A/B 同档**【OW 900f --new：new upd med 0.372/0.385/0.358 vs old
  0.352/0.368/0.562 median 同档 + render 0.535/0.477/0.428 vs
  0.428/0.523/0.680 同档 + swap 15.6-15.7 同档 + changesApplied 9/3/1
  逐位 ×2 + **终档跨树 savenorm EXACT** = 全景 FOV 覆写缺省 -1 位级 +
  re-exec 面无输入全冷 = 结构性零成本】。
- **门内捕获 PITS 430 三坑**【①闭环环带 tile 精确（70°×8 = 560° wrap
  200° 跳变 = 用户接缝根因）②demo pacing = 录制机 dt（门臂 2× wheel +
  finished 轮询定时纪律）③re-exec 门面（banner 计数判据 + teardown 序 +
  /proc/self/exe；活捕 import ≠ 帧锁定截图 = 流程证据面非 EXACT 断言面）】。
