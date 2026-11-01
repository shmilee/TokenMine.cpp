# Phase 17 细节存档

> Phase 30 文档整理：由 `SESSION_CONTEXT.md`「Phase 17 已完成（细节存档）」节 移入（未改写原文）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/...` 驱动脚本）指向当时状态；当前文档布局
> 见 `README.md`「文档地图」。

## 会话存档（原 SESSION_CONTEXT）

- 提交 `6e1e841`（2026-10-05）。测试 523/523（+49）。E2E 全绿（:0 Intel Arc，最终二进制）：
  - **base 60f 无输入**：与 `base3.png` **逐字节一致**（门控规则继续成立，含 atlas 改 4×8）。
  - **inv 180f**（锁→E 开面板→光标回中）：**×2 字节一致** + 像素判据（planks 配方行图标）。
  - **table 400f**（完整合成脚本，83 动作）：**×2 字节一致** + 5 项像素判据（网格 g00 空/
    输出空/slot10 planks/slot11 sticks/slot8 pickaxe）。终态：hotbar slot4 wood 64→61、
    slot8=pickaxe（橙框）；storage 9=pickaxe×1、10=planks×3、11=sticks×2；双网格空；
    表格块 (0,54,−2)；玩家 (0.5,54,0.5) pitch −0.68；表格面板开；光标停 (640,360)。
  - 合成脚本：E 面板 craft 12 planks→craft 工作台（2×2 shaped）→交换进 slot8→
    stash 8 planks → 关 E → 重锁 → **Tab×8 选中 slot8** → 瞄准 pitch −0.68（R 0 172）→
    RMB 放置表格 (0,54,−2) → RMB 开表格面板 → 3×3 摆 PPP/.S./.S.（先 4 sticks 再 pickaxe）
    → 取 pickaxe 入 slot8 → 停光标。
- **E2E 环境坑位（本阶段全部实锤，勿再踩）**：
  - **没有外部"窗口杀手"**——之前 :99 运行里观察到的"窗口被毁"是游戏自己在帧帽到时
    干净退出（SDL_DestroyWindow）被轮询脚本的偏移误读。
  - **xdotool 本机每次调用 ~150 ms**（不是 ~10 ms）：68 动作的序列跑 ~11 s，追不上帧帽
    截图 → 截图落在输入中途（所有运行在动作 ~67-76 "整齐截断"的真相）。**长序列禁用
    逐动作 xdotool**。
  - **libX11 请求缓冲**：单进程 XTest 的无应答请求（XTestFake*）会被 libX11 攒在输出缓冲，
    成 ~1 s 一批投递（37 点击+9 键 1 ms 内到齐）；`wasPressed` 边沿逻辑把同帧多次按下
    折叠成一次 → 突发输入只剩 ~2 次有效点击。**XTest E2E 客户端每个动作后必须
    `XFlush(dpy)`**（对照实验 evlog：不加=成批到、加了=±1 ms 准点）。
  - **真实鼠标污染**：:0 运行时 UI 窗口被 unmap（游戏窗口必须在点击处最上层），此时用户
    的真实鼠标 motion/click 直接进游戏窗口；指针锁定期间的真实 motion = 相机旋转。
    对策：运行前**等光标静止 ~400 ms**（100 ms 轮询，上限 15 s）；且不要 warp 光标到
    远离用户手的位置（手在旧位置不动 = 光标 warp 后任何手抖都变成事件）。
  - **瞄准容差**：放置命中格由地面命中点决定，pitch −0.80 时上沿容差仅 ~9 px（下沿 87 px）；
    驱动用 **pitch −0.68（R 0 172，自初始 −0.25）≈ 上下各 49 px**。
  - **Tab 丢失级联**：少 1 次 Tab → 选中格错（glowstone 而非表格）→ 放置放错块 →
    开面板 RMB 打到非 interact 块 → 面板不开 → **指针 grab 持续** → 之后所有绝对 warp
    被 X 当作相对位移（grab 内 XWarpPointer=相对）→ 光标钳到窗口角落 (1279,719) →
    后续点击全打在世界角落。**诊断特征：motion 日志出现 (996,719)/(1279,719) 爬行 +
    之后 warp 不再产生 motion 事件** = 面板没开、grab 还在。本阶段实际根因是驱动
    heredoc **只写了 7 个 Tab（应为 8）**——重写驱动时数 Tab 次数！
  - **E 阶段 LMB "双 down" 是良性**：SDL3/X11 在指针自由态对 XTEST 点击双投递（core+XI2
    嫌疑），同毫秒同帧 → `wasPressed` 折叠成一次点击，状态机不受影响；勿为此改代码。
  - **:99 Xephyr 仅逻辑回退**：:0 光标扫过 Xephyr 的 :0 窗口时会被转发进 :99（用户光标
    曾 (2555,1552/1595) = Xephyr 窗内 → 污染）；llvmpipe 渲染不能用于 base 字节基线。
- **工具（/tmp/opencode/，未入库）**：
  - `xact.c`（`gcc -O2 -o xact xact.c -lX11 -lXtst`）：长驻 XTest 动作播放器，读脚本文件
    （`M x y` 绝对 / `R dx dy` 相对（screen=-1）/ `C 1|3` / `K e|tab|w|a|s|d` / `S ms`），
    **每动作后 XFlush**，stderr 打发送时间线（调试用）。XTest 在 **libXtst** 不在 libXext；
    `XTestQueryExtension` 5 参数；头文件用 `KeySym`；相对 motion = `XTestFakeMotionEvent(d,-1,dx,dy,0)`。
  - `p17_e2e.zsh`（:0 驱动）：unmap UI 窗口（29361071，**勿动用户浏览器 29360131**）→
    等光标静止 → `xdotool mousemove 100 100` → 起游戏（config `/tmp/opencode/config_xep.json`
    = 仓库 config + `render.fps_cap 60`；frames base 60/inv 180/table 400）→ 找窗、
    `windowmove 0 60`、读回 origin（awesome 4 px 框偏移 → 实 (4,64)）、drift 检查 →
    生成绝对坐标脚本（origin 烘焙）→ `xact` 播放 → 帧帽截图 → 内置 PIL 像素判据
    （失败 exit 3）→ trap 恢复（mouseup/keyup/UI remap/kill xact）。
  - `p17_e2e_xep.zsh`（:99 回退驱动）：Xephyr 窗口 park+lower、watchdog。
  - `evlog.c`：XTest 投递时序对照实验（证明 XFlush 修复）。
- **时序规则**：输入结束须早于截图 ≥2.5 s（table：脚本 ~4.6 s + 起始 ~0.7 s ≈ 5.3 s 结束，
  截图 ~8.0 s）；帧帽 60 fps 使帧数↔墙钟映射（:99 必须开 fps_cap，:0 开=无操作已验证）。
- **C++ 坑位（本阶段新踩）**：`Json::size()` 对 OBJECT 返回 0（用 `members().empty()`）；
  JSON 字面量用 raw string 易打错（测试里 `tiles` 字符串已含花括号）；嵌套 enum 须 public；
  `short` 是保留字；GCC 16 `-Werror=maybe-uninitialized` 对循环里 `Json` move 误报（改拷贝）。
