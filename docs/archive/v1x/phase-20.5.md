# Phase 20.5 细节存档

> Phase 30 文档整理：由 `SESSION_CONTEXT.md`「Phase 20.5 已完成（细节存档）」节、`KNOWN_ISSUES.md`「已修复（归档）」条目 合并而成（未改写原文）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/...` 驱动脚本）指向当时状态；当前文档布局
> 见 `README.md`「文档地图」。

## 阶段报告（原 KNOWN_ISSUES「已修复（归档）」）

- **`[2026-10-05, Phase 20.5]` 用户反馈两项：白天生成 zombie 被攻击 + 窗口缩放场景不跟随** — **① 昼夜生成规则（光照门）**：mob 数据新字段 `spawn_max_light`（0..15 整数，默认 0 = Minecraft 敌对 mob 的"只在黑暗生成"约定；严格校验：非整数/越界拒绝）；`findMobSpawnCell` 新参数 (dayFactor, spawnMaxLight) + 纯函数 `currentSpawnLight(world, x, y, z, dayFactor)` = floor(存储天空光 × dayFactor) 与方块光取较大者——本引擎昼夜是渲染层缩放（天空光静态存储白昼值，见 day_cycle.hpp），故用渲染同款 dayFactor（正午 1/深夜 0/晨昏 smoothstep）模拟 MC 的"夜间天空光=0"：夜晚开阔地有效光 0 → 可生成，白天 15 → 不可，萤石（方块光，不随昼夜）任何时候都挡，树冠/洞穴永久暗格（存储天空光 0）白天也可生成（与 MC 一致）。检查格 = 生成格脚位（floorY+1）。`Game::attemptMobSpawn` 传 `m_dayCycle.dayFactor()` + `def.properties.spawnMaxLight`；`zombie.json` 显式 `"spawn_max_light": 0`。**② 窗口缩放**：根因 = 窗口无 `SDL_WINDOW_RESIZABLE` 且 `Window::width/height` 返回 init 时缓存尺寸（WM 改窗后视口/纵横比/UI 全停在旧尺寸）。修复 = 窗口模式默认可缩放（新 config `window.resizable`，默认 true；false 锁定配置尺寸）+ `width/height` 每帧实时 `SDL_GetWindowSizeInPixels`（SDL3 无 `SDL_GL_GetDrawableSize`，此版本用像素尺寸 API）+ 最小尺寸 320×200（命名常量，防 aspect 除零与 UI 退化）。视口（`Renderer::beginFrame`）、投影 aspect、屏幕 UI、截图全部每帧读实时尺寸 → 零事件处理即跟随。**E2E 环境要点（新）**：本机 :0 是平铺 WM（2560×1600）——**可缩放窗口会被 WM 强制平铺为 1272×1552**（新建/收到 resize 请求即触发），不可缩放窗口 WM 尊重配置尺寸。因此：基线/昼夜运行用 `window.resizable=false` 锁 1280×720（base18 门不变）；resize 测试用可缩放窗口 + xdotool 请求 1000×600 → WM 平铺 1272×1552，断言 = 截图尺寸 == 窗口实际最终几何（运行中读，窗口销毁后读不到）且 ≠1280×720 + 原视口外区域（y=1000 行）是真实世界内容（草地纹理 16 色，非 clear 色）。Xephyr :99 尝试过：几何 1280×720 ✓ 但像素 649 px 差（GLX 路径不同）→ 不能用于字节基线。**夜晚 E2E 时刻选择**：光照门 要求 floor(15×dayFactor) ≤ 0 且场景非全黑（dayFactor > 0）→ 黄昏 ramp（t∈[0.45,0.55)，dayFactor = 1−smoothstep((t−0.45)/0.1)）的末段；配置 day_length 1200s + start_time_of_day 0.536 → 生成帧（60/90/120/150）dayFactor ≈ 0.039–0.048（有效光 floor 0 ✓）、截图帧（239）dayFactor ≈ 0.032 （>0 可见，且 0.539 < 0.55 未入深夜全黑平台）。**测试结果** 588/588（+9：SpawnMaxLight 默认 0/解析 7/16 与 −1 拒绝/非整数 1.5 拒绝 + 生成格夜间暗格可生/白天亮格拒→下格/同亮格夜间 dayFactor 0 可生/夜间方块光 1 仍拒/spawn_max_light 15 白天可生）。**E2E**（`/tmp/opencode/p205_e2e.zsh`）：base 60f ×2 与 base18 逐字节一致；**白天 mobs-on 240f ×2 与 mobs-off ×2 逐字节一致（白天不生成的字节级证明）**；夜晚 mobs ×2 与对照 ×2 逐字节一致、mobs vs 对照 = 54177 px 差、bbox x[273,678] y[242,485] 与 P20 白天 **完全相同**（同生成格同 AI 轨迹，光照门只改"能否生成"不改"在哪/怎么走"）、UI 区零差、心区零差（无伤害）、zombie 剪影遮挡黄昏天空（mobs (7,10,5) vs 对照天空 (134,65,39)）；resize 运行：PNG 1272×1552 == FINAL GEO 1272x1552，原 1280×720 视口外为草地纹理。教训：Python `dict(x)` 是浅拷贝——嵌套 dict （`cfg['world']`）被后续修改污染，配置生成脚本必须 `copy.deepcopy`。

## 会话存档（原 SESSION_CONTEXT）

- **目标**：修复用户反馈的两项问题：① 进入游戏（白天）就被 zombie 攻击——白天不应
  有 zombie；② 窗口调整大小时场景不跟随（窗口可被 WM 缩放但 GL 视口停在初始尺寸）。
- **① 昼夜生成规则（光照门）**：
  - 数据：mob properties 新字段 `spawn_max_light`（0..15 整数，默认 0 = Minecraft
    敌对 mob 的"只在黑暗生成"约定；未来 passive mob 可设 15 = 随时可生）。校验：
    非数字/非整数（1.5）/越界（16、−1）= 明确报错。`zombie.json` 显式
    `"spawn_max_light": 0`。
  - 代码：`findMobSpawnCell` 新参数 `(float dayFactor, int spawnMaxLight)`（放在
    height 之后、playerMin 之前）+ 新纯函数 `currentSpawnLight(world, x, y, z,
    dayFactor)`（mob_spawning.{hpp,cpp}）= `floor(存储天空光 × dayFactor)` 与方块光
    取较大者。**语义依据**：本引擎昼夜是渲染层缩放（day_cycle.hpp：天空光静态存储
    白昼 0..15 值，渲染器 `light = max(v_skyLight × u_dayFactor, v_blockLight)`），
    故用渲染同款 dayFactor（正午 1 / 深夜 0 / 晨昏 smoothstep ramp）模拟 MC 的
    "夜间天空光=0"：夜晚开阔地有效光 0 → 可生成；白天 15 → 不可；方块光（萤石）
    不随昼夜 → 任何时候都挡；树冠/洞穴永久暗格（存储天空光 0）白天也可生成（与 MC
    一致）。检查格 = 生成格**脚位**（floorY+1，mob 站立的格子）。
  - Game：`attemptMobSpawn` 传 `m_dayCycle.dayFactor()` +
    `def.properties.spawnMaxLight`。
- **② 窗口缩放**：
  - 根因：窗口未设 `SDL_WINDOW_RESIZABLE`（flags 仅 OPENGL+FULLSCREEN），且
    `Window::width()/height()` 返回 **init 时缓存的** m_width/m_height——WM 改窗后
    每帧 `beginFrame(缓存值)` 的视口、投影 aspect、UI 布局、截图全部停在旧尺寸
    （用户看到"场景固定大小"）。
  - 修复：`window.resizable` 配置（默认 true；false = 锁定配置尺寸）+ 窗口模式
    非全屏时加 `SDL_WINDOW_RESIZABLE` + `width()/height()` 每帧实时查询
    `SDL_GetWindowSizeInPixels`（**注意：此 SDL3 版本没有 `SDL_GL_GetDrawableSize`**，
    像素尺寸 API 在 SDL_video.h）+ `SDL_SetWindowMinimumSize(320, 200)`（命名常量
    kMinWindow*，防 aspect 除零与 UI 退化；仅 resizable 时设）。零事件处理——视口
    （`Renderer::beginFrame`）、投影（render() 每帧 `Mat4::perspective(fov, w/h)`）、
    屏幕 UI（drawUi 每帧取 w/h）、截图（takeScreenshot glReadPixels w×h）全部每帧
    读实时值，天然跟随。
- **E2E 环境要点（重要，P21 起沿用）**：
  - 本机 :0 是**平铺 WM**（屏 2560×1600）：**可缩放窗口会被 WM 强制平铺为
    1272×1552**（新建即触发，收到 resize 请求也触发；位置 4,44）；**不可缩放窗口
    WM 尊重配置尺寸**（1280×720）。因此 E2E 配置 `config_xep*.json` 全部加
    `"window": {"resizable": false}` 锁 1280×720 → base18 门不变；resize 测试用
    独立 `config_xep_resize.json`（resizable true）+ xdotool windowsize 请求 1000×600
    → WM 平铺 1272×1552，断言 = 截图尺寸 == 窗口实际最终几何（**运行中读**——窗口
    销毁后 getwindowgeometry 为空，120 帧运行 ~4s 结束，sleep 1.0 后 resize 再
    0.4s×N 轮询读）且 ≠ 1280×720 + 原视口外区域（y=1000 行）是真实世界内容（草地
    纹理 16 种颜色，非 clear 天空色——平原地带颜色会重复，勿用"distinct > 20"判据）。
  - **Xephyr :99 不可用于字节基线**：几何正确（1280×720，无内部 WM 不平铺）但像素
    649 px 差（GLX 路径不同）；仅作逻辑回退（同 P17 结论）。
  - **夜晚 E2E 时刻选择（光照门下的可行窗口）**：需 floor(15×dayFactor) ≤ 0（可生）
    且 dayFactor > 0（场景非全黑，否则 zombie 与地形同为纯黑、diff=0）。黄昏 ramp
    t∈[0.45,0.55)：dayFactor = 1 − smoothstep((t−0.45)/0.1)。配置
    **day_length 1200s + start_time_of_day 0.536**：生成帧（60/90/120/150，每帧推进
    1/(60×1200) 的 t）dayFactor ≈ 0.039–0.048（有效光 floor(≤0.71)=0 ✓）、截图帧
    （239）t≈0.5393（< 0.55 未入深夜全黑平台 ✓）dayFactor ≈ 0.032（>0 可见 ✓）。
    夜晚 zombie 剪影遮挡**黄昏橙天空**（mobs (7,10,5) vs 对照 (134,65,39)），diff
    判据用总量/bbox/UI/心区（绿皮肤颜色判据 G≥R+40 在低亮度下不适用，弃用）。
- **测试** 588/588（+9）：MobRegistryTest：SpawnMaxLightDefaultsToZero（kZombieDef
  无该字段 → 0）/ParsesSpawnMaxLight（7）/RejectsSpawnMaxLightOutOfRange（16、−1）/
  RejectsNonIntegerSpawnMaxLight（1.5）；MobSpawnCellTest：SpawnsInDarknessAtNight
  （fixture 新 chunk 光 0/0 即暗，df 0 + maxLight 0 → 首候选）/SkipsCellThatIsTooBrightByDay
  （首候选脚位写天空光 15，df 1 → 拒，下格 (−3,0,−5)）/NightScalesSkyLightToZero（同亮格
  df 0 → 可生）/BlockLightStillBlocksSpawnAtNight（方块光 1，df 0 → 拒）/
  PermissiveMobSpawnsInLight（df 1 + maxLight 15 → 可生）。**fixture 要点**：新 chunk
  光 0/0（= 暗），既有 8 个 cell 测加 (0.0f, 0) 参数后行为不变；写光 =
  `world.getOrCreateChunk({0,0,−1}).setSkyLightAt(0,1,10,15)`（世界 (0,1,−6) = 该
  chunk 局部 (0,1,10)）。既有 8 个 cell 测调用点插 `0.0f, 0,` 参数（python 批量替换）。
- **E2E**（:0，`/tmp/opencode/p205_e2e.zsh`，无输入；配置 config_xep*.json 均
  resizable=false）：base 60f ×2 与 base18 逐字节一致；**白天 mobs-on 240f ×2 与
  mobs-off ×2 逐字节一致（白天不生成的字节级证明——用户 bug ① 修复）**；夜晚
  （day_length 1200 + start 0.536）mobs ×2 与对照 ×2 逐字节一致、mobs vs 对照 =
  **54177 px 差**、bbox x[273,678] y[242,485] 与 P20 白天**完全相同**（同生成格、同 AI
  轨迹——光照门只改"能否生成"不改"在哪/怎么走"）、UI 区零差、心区零差（4s 内无人被
  打到）；resize 120f：PNG 1272×1552 == FINAL GEO 1272x1552 ≠ 1280×720，y=1000 行
  为草地纹理（16 色）——场景跟随窗口（用户 bug ② 修复）。
- **教训**：① Python `dict(x)` 是浅拷贝——`night['world'][k]=v` 会污染原 dict，配置
  生成脚本必须 `copy.deepcopy`（本次首版把 day 对照配置写成夜晚参数，导致一轮无效
  diff）；② E2E 驱动里 `pkill -f "Xephyr :99"` 会匹配到**当前 shell 自身命令行**
  （复合命令里含该字样）→ 自杀；用 `pkill -x Xephyr` 或独立步骤；③ 驱动内 `cd` 到
  仓库后相对路径的截图会落在仓库根 → 永远传绝对路径。
- **变更文件**：`src/core/window.{hpp,cpp}`（RESIZABLE + 实时尺寸 + 最小尺寸 +
  window.resizable）、`src/entity/mob_registry.{hpp,cpp}`（spawnMaxLight 字段 +
  解析校验 + <cmath>）、`src/entity/mob_spawning.{hpp,cpp}`（currentSpawnLight +
  findMobSpawnCell 参数与检查）、`src/core/game.cpp`（attemptMobSpawn 传参）、
  `assets/data/mobs/zombie.json`（"spawn_max_light": 0）、`tests/test_mob.cpp`
  （+9 测、8 处调用点加参）、README（窗口节/生成+光照门/新增 Mob 文档/roadmap）、
  KNOWN_ISSUES（Mob 节"无光照/昼夜生成规则"行更新 + P20.5 归档条）、SESSION_CONTEXT
  （本节）。config/config.json（用户手改 view_distance 80 / day_length 600）不入档、
  不回退。
- **提交**：`fb43923`（代码 + 数据 + 文档，显式路径列表；config/config.json 不入档）。
