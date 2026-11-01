# Phase 8 细节存档

> Phase 30 文档整理：由 `SESSION_CONTEXT.md`「Phase 8 已完成（细节存档）」节、`KNOWN_ISSUES.md`「已修复（归档）」条目 合并而成（未改写原文）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/...` 驱动脚本）指向当时状态；当前文档布局
> 见 `README.md`「文档地图」。

## 阶段报告（原 KNOWN_ISSUES「已修复（归档）」）

- **`[2026-10-03, Phase 8]` 静态天空色/无昼夜** — Phase 8 实现昼夜循环：`DayCycle`（`src/world/day_cycle.{hpp,cpp}`，确定性游戏内时间）；shader 天空光 × `u_dayFactor`（黎明/黄昏 smoothstep 渐变，夜晚天空光=0，方块光不受影响）；天空背景色关键帧插值（夜蓝→黎明橙→昼蓝→黄昏橙→夜蓝，正午段与 Phase 1-7 天空色逐位一致——保证基线截图不变）；太阳/月亮/300 颗星为像素级圆形 billboard quad（`assets/shaders/sky.{vert,frag}`，Renderer::drawSky，无 u_model）。新增 CLI `--time <0..1>` 与配置 `world.day_length_seconds`（240）/`world.start_time_of_day`（0.25）。

## 会话存档（原 SESSION_CONTEXT）

- `src/world/day_cycle.{hpp,cpp}`（+17 个 DayCycleTest）：见核心架构节。
- 渲染：`assets/shaders/sky.{vert,frag}`（unlit billboard quad，u_view/u_proj/
  u_offset/u_sizePixels/u_pxToWorld/u_color；**无 u_model**）；
  Renderer::drawSky（索引、不设 u_model）；basic.frag 加 `u_dayFactor`：
  `light = max(v_skyLight × u_dayFactor, v_blockLight)`（block 光不受昼夜影响）。
- Game：`m_dayCycle`（config `world.day_length_seconds` 240 / `world.start_time_of_day`
  0.25）；update 每帧 `update(1.0/60.0)`（固定步长，逐字节可复现）；render：
  clear(skyColor)、u_dayFactor、opaque pass 与 transparent pass 之间
  `drawSkyObjects(view, proj, h)`（stars（starAlpha>0）→ sun（40px，(1.0,0.93,0.78)）
  → moon（28px，(0.85,0.88,1.0)）；地平线淡出 `clamp(elevation/0.1, 0, 1)`；
  天空对象在 r=800（< kFar 1000））。CLI `--time <0..1>`（init 里写 config 覆盖）。
- 常量（game.cpp 匿名命名空间）：kSkyDistance=800、kStarCount=300、
  kStarSeed=0x5EED、kStarSpriteSize=2、kSunSpriteSize=40、kMoonSpriteSize=28、
  kHorizonFade=0.1、kDayStepPerFrame=1/60。
- 验证：277/277 测试；**60 帧正午基线与 P7 逐字节一致（含 clean rebuild）**；
  夜晚（--time 0.75）：天空 (5,8,20)、地面近黑（~(3,5,2)）、27 个星像素、
  两次运行逐字节一致；黎明 E2E（--time 0.05 + xdotool 右移 450px/上移 371px）：
  40px 圆形太阳（(255,237,199)）在屏幕中心附近、天空 (155,83,51) 黎明橙；
  月亮 E2E（--time 0.55，同瞄准）：28px 月亮（(217,224,255)）、天空 (67,35,29)
  黄昏→夜插值精确命中。
- E2E 瞄准配方：`mousemove 1284 804; click 1`（锁定）→ `sleep 0.3` →
  `mousemove_relative -- 450 -371`（对准 t≈0.07/0.57 的日/月，含视差）；
  300 帧窗口（5s）内完成，截图取最后帧。
