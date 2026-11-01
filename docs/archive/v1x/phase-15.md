# Phase 15 细节存档

> Phase 30 文档整理：由 `SESSION_CONTEXT.md`「Phase 15 已完成（细节存档）」节、`KNOWN_ISSUES.md`「已修复（归档）」条目 合并而成（未改写原文）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/...` 驱动脚本）指向当时状态；当前文档布局
> 见 `README.md`「文档地图」。

## 阶段报告（原 KNOWN_ISSUES「已修复（归档）」）

- **`[2026-10-04, Phase 15]` 无准星/目标高亮（P6）/ 无裂纹动画（P6）/ 无库存 UI（P11）/ 工具数据部分不可获得（P11）/ 无游戏内合成 UI·触发（P12）/ 音频系统未引入** — Phase 15 实现 UI/音效/粒子/性能统计：专用无光照 UI 着色器（`assets/shaders/ui.{vert,frag}`，u_useTexture/u_color，ortho 屏幕空间与 perspective 世界空间通用）+ 批量化绘制（`UiBatch`，同色同纹理合并为一个 mesh+draw）；准星/目标方块线框描边（`MeshMode::Lines`）/hotbar（选中橙框+数量）/手持物品；`E` 背包+合成面板（纯逻辑 `InventoryLayout`+点击状态机 `InventoryUI`：左键取/放/交换/并入、右键移 1、点击外丢弃、悬停物品名、点击配方行 `craft()`）；`F3` 调试 overlay（8 行：FPS 整数/chunk·draw·顶点/坐标/seed/昼夜/流式/背包·粒子）；`F5` 立即存档（`--save` 激活时）。破坏反馈：5 级累积裂纹条（`tools/generate_cracks.py`，进度→stage 钳制）+ 破坏粒子（`ParticleSystem`，确定性 splitmix64 爆发、半隐式 Euler、上限 256、spawn 烘焙光照——渲染只读窗口不读光数组）。音效：`SoundBank`（SDL3 `SDL_OpenAudioDeviceStream` 队列式，NULL 回调，`SDL_PutAudioStreamData` 排队；init 失败优雅降级静音+WARN）+ 8 个程序化 WAV（`tools/generate_sounds.py`，44.1kHz 16-bit 单声道），固定事件集（dig 每 0.3s tick/break/place/pickup/step 每 0.4s/ui_open·close/craft），音量 `audio.volume`（默认 0.7）。物品图标：block item = 带光照倾斜 3D cube（复用掉落物 mesh，3D 图标在 2D 批之后绘制防被槽背景盖住），工具/材料 = `Item.icon` 数据字段 → `assets/textures/items/<icon>.png`（`tools/generate_item_icons.py`，init 交叉校验文件存在）。字体：程序化 8×8 字形（`tools/generate_font.py` → `font.png` 128×256，`src/ui/font` UV/顶点纯逻辑）。**门控规则**：所有 UI 只在按键（F3/E）或指针锁定或事件（裂纹/粒子/音效）时渲染 → 无输入 60 帧运行零新增绘制，`base3.png` 基线逐字节不变（E2E 验证）。新增 40 个单测（font UV/文本宽度/顶点；debug overlay 行格式；InventoryUI 布局/左键全流程/右键/点击外丢弃/合成行；Particle 确定性爆发/更新/上限/死亡；Sound parseWav/applyGain）+ E2E（无输入基线逐字节；锁定态准星+hotbar+手持+描边；F3 overlay；背包面板+悬停；背包左键取整堆移格；点击 planks 配方行合成 wood 64→63+planks×4；按住破坏 0.45s 裂纹 stage 2；破坏后粒子消亡定格——各用例两次运行截图逐字节一致；8 次运行日志 "Audio ready: 8 samples, volume 0.700000"）。新简化记入「UI / 音效 / 粒子（Phase 15）」节。

## 会话存档（原 SESSION_CONTEXT）

- **UI 管线**（`src/core/game.{hpp,cpp}` + `assets/shaders/ui.{vert,frag}`）：
  - ui 着色器 = 无光照；`u_useTexture`（0=纯色 `u_color`；1=纹理 `vec4(texel.rgb,
    u_color.a*texel.a)`）；`gl_Position = u_projection * u_view * u_model * pos`——
    ortho 屏幕空间与 perspective 世界空间通用（同一 shader 画 2D 批 + 3D 图标 + 裂纹/描边）。
  - 批量：`UiBatch {color, textured, texture*, vertices}`，`batchFor(color,textured,tex)`
    同键合并（一个 batch = 一次 upload + 一次 draw）。UI pass 状态：深度关 + 剔除关 +
    混合开（`Renderer::setDepthTest/setCulling` 新增；结束后恢复）。
  - `Mesh` 加 `MeshMode {Triangles, Lines}`（Lines=非索引、偶数顶点、`glDrawArrays(GL_LINES)`）
    ——目标方块描边 24 顶点线框立方体（1.004 scale，黑色 α0.85，深度开，ui 着色器）。
  - 门控规则（硬性，基线逐字节的前提）：准星/目标描边/hotbar/手持 = 指针锁定且背包关闭；
    背包面板 = `m_inventoryOpen`；F3 = `m_debugOverlay`；裂纹/粒子/音效 = 事件驱动。
    无输入 60 帧运行 renderUi 零 batch、drawParticles 零 draw → `base3.png` 字节不变。
  - `uiProjection(w,h) = Mat4::ortho(0,w,h,0,-128,128)`——**near/far 必须 ±128**：
    3D cube 图标倾斜后 |z| 最大 ~66px（76px 手持图标），±1 会把整个 cube 裁掉
    （2D quad 在 z=0 不受影响，深度测试关所以宽范围无害）。
  - **3D 图标绘制顺序**：block item 图标 = 带光照倾斜 cube（`itemMesh(block)` 缓存 +
    基本着色器，model = translate·rotateY(0.65)·rotateX(-0.35)·scale(sizePx/0.25)），
    在 renderUi 里**先收集 `UiIconDraw`、2D 批全部画完后最后画**（否则被槽背景盖住；
    且画前重绑 atlas——批绘制绑过 font/icon 纹理到 unit 0）。
- **背包/合成 UI**（`src/ui/inventory_ui.{hpp,cpp}`，纯逻辑可单测）：
  - `InventoryLayout::compute(w,h,recipeCount)`：panel 828×238 @ (226,241)（1280×720/4 配方）；
    槽 52px gap 4；grid @(238,283)；存储行 y 283（9..17）/339（18..26）；hotbar 行 y 395
    （0..8）；craft 列 x 762 宽 280，行高 46（header y 253 h 30）。`slotAt(x,y)->int`（-1=外）。
  - `InventoryUI(Inventory&, const ItemRegistry&)` 点击状态机：左键=空→拿起整堆/同物→
    并入（满则交换）/异物→交换（carry 带出被占格）；右键=移 1（同物有空间或空格，
    异物格/面板外=无效）；左键点面板外=丢弃 carry。carry 经 `carry()` 暴露给渲染。
  - 打开 `E`（仅指针锁定时）→ 释放指针；关闭 `E`/`ESC`（指针保持自由，用户再点回捕获）。
    打开时暂停玩家物理+方块交互，世界/昼夜/流体/实体/流式照常 tick。
    ESC 退出检查（run()）要求 `!m_inventoryOpen`。
- **音效**（`src/core/sound.{hpp,cpp}`，`SoundBank`）：
  - SDL3 3.4.16 特性（坑）：`SDL_AudioDeviceID SDL_OpenAudioDeviceStream(id, spec,
    callback, userdata)` **返回 stream**；传 NULL 回调 = 应用侧 `SDL_PutAudioStreamData`
    排队播放；`SDL_DestroyAudioStream` 同时关设备。无 `SDL_QueueAudio`。
  - 优雅降级：init 失败 → WARN + `enabled=false`，`play()` no-op（静音不崩）。
  - 8 个 WAV（`tools/generate_sounds.py`：44.1kHz 16-bit 单声道，包络+噪声/正弦合成）：
    dig/place/break/pickup/step/ui_open/ui_close/craft。事件映射：破坏 tick 每 0.3s
    （`m_lastDigProgress`）、break 完成、place、pickup（含 Q 丢弃）、脚步每 0.4s
    （水平速度>0.5+贴地+锁定+背包关）、背包开/关、合成成功。音量 `audio.volume`（0.7）。
  - 纯函数可单测：`parseWav`（RIFF 解析）/`applyGain`。
- **粒子**（`src/entity/particle.{hpp,cpp}`，`ParticleSystem`）：
  - `Particle {pos, vel, age, life, block, skyLight, blockLight}`；上限 256、每爆发 16、
    life 0.45~0.75s、重力 16、**无碰撞**、半隐式 Euler（先 vel 后 pos——测试别用显式）。
  - 确定性：spawn seed = `worldSeed + (++m_particleBreakCounter) * 0x9E3779B97F4A7C15`
    （splitmix64）；运动用真实 dt → **截图必须在粒子死绝后**（break+50 帧，寿命 ≤0.75s）。
  - **光照 spawn 时烘焙**（`breakBlock` 在 `Lighting::updateLocal` 之后读
    `findChunk`+`skyLightAt/blockLightAt`——主线程写窗口安全；render 是只读窗口，
    **绝不能在 render 里读 World 光数组**，worker 光照 job 可能同时在跑）。
  - 渲染 = 每 pass 一个动态 indexed mesh（0.12 格 cube，逐面 atlas UV），按
    `block.properties.transparent` 分 opaque/transparent 两 pass。
- **裂纹**（`tools/generate_cracks.py` → `assets/textures/cracks.png` 80×16，5 级累积，
  splitmix64 随机游走白 alpha）：5 个预建整 cube mesh（1.006 scale，UV=stage 瓦片
  u∈[s/5,(s+1)/5]、v 全高），ui 着色器（无光照、混合、深度开）画在透明 pass；
  stage = clamp(floor(progress/duration × 5), 0, 4)。
- **字体/图标**：
  - 纹理 v 约定（**坑**）：上传不翻转 → v=0 = 图像**顶行**。font/crack/icon UV 都用
    图像空间 v（v_top < v_bottom）。
  - `tools/generate_font.py` → `font.png` 128×256（16px cell，cell (c%8, c/8)，glyph
    在 cell 内 +4px 偏移）；`src/ui/font.{hpp,cpp}`：`glyphUv(c)`（非 ASCII→'?'）、
    `textWidth = n*8s + (n-1)*s`、`makeTextVertices`（每字符 6 顶点、advance 9s、
    屏幕 y 向下）。
  - `tools/generate_item_icons.py` → 8 个 16×16 图标（planks/stick/coal/diamond/
    gold·iron ingot/wooden·iron pickaxe）；`Item.icon` 可选字段（数据驱动，
    unknown-field allowlist 已加，buildScene 交叉校验文件存在=缺失即 init 错误）；
    block item 的 icon 为空（画 3D cube）。
- **输入**（`src/core/input.{hpp,cpp}`）：新键 `GameKey::E/F3/F5`（`kKeyCount` 由
  LAST 枚举值算）；`handleMouseMotion(dx,dy,x,y)` 4 参（绝对 x/y 来自 SDL motion 事件，
  `mousePosition()` 供背包点击；**指针锁定时 x/y 是相对值——stale，但锁定态不用它**）。
- **其他**：`Inventory::set` 改名 `setSlot`（UI/初始化/读档全部更新）；`Mat4::scale`
  新增；`config audio.volume`（默认 0.7）。
- 测试 458/458（40 新增，见项目状态）；构建 0 警告（`-Wall -Wextra -Wpedantic -Werror`）。
- E2E（驱动 `/tmp/opencode/p15_e2e.zsh`；窗口 (644,444) 1280×720，中心 (1284,804)，
  每次运行前 `xdotool mousemove 100 100`；**截图是窗口内容=窗口坐标，像素检查别用
  屏幕坐标**（ImageMagick 会把超界点钳位到角落，造成假失败））：
  - base 60f 无输入：与 `base3.png` **逐字节一致**（门控规则证明）。
  - hud 240f（+0.7s 点击锁定）：准星(640,360)=白 251+hotbar+手持 stone+描边，×2 字节一致。
  - f3 240f（锁定+F3）：overlay 文字 (8,9)='M' 笔画 239，FPS 整数 60（vsync 稳），×2 一致。
  - inv 240f（E+悬停 slot 4 wood）：面板+tooltip，×2 一致。
  - invmove 240f（E+左键取 hotbar slot 1 dirt 64 → 移到存储 slot 10）：dirt cube 图标
    (320,309)=(67,47,38) 侧面着色，×2 一致。
  - craft 240f（E+点击 planks 配方行 abs(1546,796)=窗口(902,352)）：wood 64→63（数量
    文本显示）+planks×4 入存储 slot 9（图标 (264,309)=(168,121,74)），×2 一致。
  - breakcrack 96f（锁定+直视下方+1.3s 按住）：grass 0.9s 破坏，hold 0.45s → progress
    0.5 → 裂纹 stage 2（稳定区 hold 0.36~0.72s），×2 一致。
  - breakafter 240f（+2.8s 松手）：破坏~2.2s/落地~2.5s/拾取~2.7s/粒子死~2.95s →
    定格态，×2 一致。
  - 音频：8 次运行日志均 "Audio ready: 8 samples, volume 0.700000"（pipewire 正常）。
  - 配方行序 = 文件名排序：iron_pickaxe/planks/stick/wooden_pickaxe（planks=行 1）。
- 刻意简化（KNOWN_ISSUES「UI / 音效 / 粒子（Phase 15）」节）：固定事件集音效（无
  per-block 音色/空间化）；工具/材料=16×16 程序化图标（无 3D 手持模型）；背包右键
  只"移 1"（无取半/拖拽/shift）；背包不暂停世界模拟；F3 FPS 整数；粒子无碰撞逐帧
  动态 mesh；字体仅 ASCII 8×8；tooltip 只有名字。
- 归档（KNOWN_ISSUES「已修复」）：无准星/目标高亮（P6）/无裂纹动画（P6）/无库存 UI（P11）/
  工具数据部分不可获得（P11）/无游戏内合成 UI·触发（P12）/音频系统未引入（合并为 P15 归档
  条目）；非 block 物品掉落物 in-world 不可见（UI 部分已解决，world 部分保留）；
  会话中无自动存档（F5 手动保存已加，周期 autosave 保留）。
