# Phase V2-4 — 天气（雨/雷）（v2.4.0，门 175 全 PASS）

> 2.x 第四项（V2-4 执行序）。范围权威 = docs/BACKLOG.md V2-4 条 + 编排者
> 11-03 设计定案（夜间自主推进授权，定案文件已并入本档 §定案常量）。
> 帧锁定粒子雨（静态 VBO + 顶点 shader 逐滴纯函数位置，cloud program 第四
> 先例）+ 雷声（新 thunder.wav 固定种子）+ `render.weather` config +
> `--weather` CLI；交互全走既有系统：雨灭火/灭营火/灭燃烧效果（水浇灭
> 规则 ∨raining 扩展）、雨润耕地（`soilIsWet` 雨水源）。**零 format**
> （25 不动——天气 = session 派生，云漂移先例类）。

## 定案（11-03 编排者）

### 状态机（core/weather.{hpp,cpp} 纯函数，campfire 范式）

- 流 = splitmix64 **推进式**（PITS 4：`state += 0x9E3779B97F4A7C15ULL`
  后再 mix）；主流 seed = 世界 seed（跨 run 同世界同序列）；雷 = 独立
  salt 流（`kSaltWeatherThunder` 0x7E42B1D9，碰撞核对全仓 kSalt* 32 位域
  + 零扩展 64 位域）。
- 周期（命名常量，帧）：CLEAR ∈ [kClearMin 9000, kClearMax 18000)
  （150-300 s）/ RAIN ∈ [kRainMin 3000, kRainMax 6000)（50-100 s）——
  MC 比例压缩到 day_length 600 s 量级；雷：RAIN 内每 [kThunderMin 600,
  kThunderMax 1800) f 一次。
- 交替 CLEAR→RAIN→CLEAR；**首段恒 CLEAR**（初始 state；时长 ≥ kClearMin
  = 9000 f ≥ 60 f 基线窗 = 无输入 60f 内永不进雨，结构性免疫根）。
- **--weather CLI**（main.cpp，--time 同款范式：nextValue + 词校验
  "clear"|"rain"，未知值点名 `invalid --weather: '<w>' (use clear|rain)`）：
  仅覆盖**初始 state**（rain = 初始进雨态，framesLeft 取 **kRainMin 固定**
  （定案：初始雨时长可复现），雷 gap 首卷自独立流；主流零消费——其后走
  流 = 无 override 序列前移一段，10203/4436 钉实证）；init 日志逐字
  `Weather override: rain|clear`。
- 定案日志（E2E 证据逐字）：进雨 `Weather: rain started` / 出雨
  `Weather: rain stopped` / 雷 `Thunder`（info，帧锁定序列；--weather
  rain 的 init 同发 `Weather: rain started`——帧 0 即雨，编排者冒烟
  点名行）。

### 呈现（renderer 边界内）

- 雨滴 = 独立 shader program（`assets/shaders/rain.{vert,frag}`，cloud
  第四先例）：静态 VBO 每滴参数（出生格偏移 + 相位，`kSaltRainDrop`
  0xD93A5E17 专用 salt 推进流，N = kRainDropCount 300，init 一次上传）+
  顶点 shader 逐滴纯函数位置 `y = mod(offset.y + fallOffset + phase×16,
  16)`（fallOffset = (frame % 16) × 1.0 f/帧 有界项 = 无缝回绕 +
  float32 精度恒界；boxOrigin = 玩家格心 16³ 盒 uniform）= per-frame
  零上传（cloud quad 同款纪律）；CPU oracle `rainDropPosition`
  （单测钉，hexfloat 精确值）。
- 雨滴几何 = 细竖条 quad 0.02×0.5（X-Y 面，双面关裁剪，blend pass，
  alpha 0.35 白蓝 (200,210,235)——**无新纹理**定案）。
- 绘制点：game_render.cpp 云层之后、HUD 之前（深度测试开、混合 pass）。
- 天空：`weatherDayFactor()` = **dayFactor 消费点单收口**——RAIN ×
  kRainSkyDim 0.85（命名常量）+ 雷 thunderPulse 叠加；u_dayFactor
  uniform（render() 顶 + drawBeaconBeams per-draw 两处消费）与同帧 sky
  clear / 雾色（u_fogColor = 同帧 sky）全走该收口；shader 表达式树零
  改动（雾自动继承）。off/clear = IEEE 恒等（x×1.0 == x，x+0.0 == x）
  = v2.3.0 位级。
- 雷闪光：thunderPulse(frame, eventFrame) = 5 帧线性衰减（+0.15 起，
  0.15/0.12/0.09/0.06/0.03，第 6 帧归 0），与 dayFactor 同通道叠加
  （clear 色 + u_dayFactor 两处同加）。

### 交互（全帧锁定，消费既有系统）

- 雨灭火块：`extinguishedByWaterOrRain`（world/fire_spread 新纯谓词 =
  6-邻水 ∨ raining）→ updateFires 收口（1 f 即灭，死亡爆发粒子走既有
  putOut 路径）。
- 雨灭营火：Phase 75 水浇灭条件扩同一谓词（烤制状态保留续烤同款语义；
  无复燃 = 既有记档不变）。
- 雨灭燃烧效果：玩家 updateEffects `inWater ∨ raining`（re-ignite 先行 =
  火块上雨中不灭，水胜火序）；mob setRaining per-frame feed（setPlayer
  同款）`inWater ∨ raining`。**不做屋檐遮蔽检查 = 记档偏离**。
- 雨润耕地：`soilIsWet(world, pos, raining=false)` / `cropIsWet` /
  `cropGrowthSteps` 扩参（默认 false = 旧语义位级不变——mesher 湿润视觉
  / 湿度 sweep / 放置 seed 三消费者不传 = 视觉零变，仅生长节奏消费活
  天气）→ 雨中生长 2×（drySlowdown 门解除，111/200→200/200 钉）。
- 流体不注水（MC 同款，记档）。雷无声外效果（无 MC 闪电落点伤害，记档）。

### config / F3 / 音频

- `render.weather` 代码默认 **true**（**strict read**——SAVE-1 纪律：
  缺键 = true，非 bool = init 点名拒 `config render.weather must be a
  boolean (true/false), got a non-boolean`；设计 §4 "getBool 通道"与
  §6 "config 严格读（非 bool 点名）"并读 = 严格读为准，render.clouds
  先例仅指代码默认 true + config.json 零改动两点，偏离点名见 §偏离）。
  off = 状态机不跑 + 零雨滴 + 零交互 = v2.3.0 **位级恒等**。
- F3 行 `weather clear|rain`（第 17 行，tutorial 行后；unit-test 空值
  = "weather (none)"；off = 机器不跑 → "clear"）。
- 雷新 wav：generate_sounds.py + "thunder"（60→40 Hz 滑频 + 固定种子
  0x70DE 双 lowpass LCG 滚雷体，3.5 s 慢起音长衰减）；**既有 39 wav
  md5 零变化实证**（39 行 diff 仅 +thunder 一行）+ 幂等重跑（二次 md5
  diff 空）。2D 中心播放（远雷，不空间化 = 定案）。**无雨声循环**（记档
  偏离：项目音效集无循环音范式）。
- **零 format**（25 不动）：天气 = session 派生（云漂移先例类）；MC
  持久天气 = 记档偏离。版本号 CMakeLists 单点 2.3.0→2.4.0 +
  test_debug_overlay F3 钉 v2.4.0 + banner 实证。

## 实现（变更文件）

- `src/core/weather.{hpp,cpp}`（新）：状态机纯函数
  （weatherInitialState/weatherStep/thunderPulse）+ 呈现常量（kRainSkyDim
  / kThunderPulseFrames / kRainDropCount / kRainBoxSize / kRainDrop* /
  kSalt* 两枚）+ makeRainDrops/rainDropPosition oracle + parseWeather +
  weatherEnabled strict read
- `assets/shaders/rain.{vert,frag}`（新）：逐滴位置顶点 shader（有界
  fallOffset + boxOrigin）+ 无纹理白蓝 fragment
- `src/world/fire_spread.{hpp,cpp}`：`extinguishedByWaterOrRain` 纯谓词
  （6-邻水 ∨ raining；kExtinguishNeighbors 收编自 game_frame 匿名
  kFireNeighbors）
- `src/world/growth.{hpp,cpp}`：soilIsWet/cropIsWet/cropGrowthSteps
  raining 扩参（默认 false = 位级不变）
- `src/entity/mob_entity.{hpp,cpp}`：setRaining feed + 灭火 `inWater ∨
  m_raining`（re-ignite 先行保水胜火序）
- `src/core/game.hpp`：GameArgs.weather（optional<bool>）+ 天气 family
  成员（m_weather/m_weatherEnabled/m_weatherStream/m_weatherThunderStream
  /m_weatherThunderFrame/m_rainMesh/m_rainShader，cloud family 后追加）+
  updateWeather/drawRain/weatherDayFactor 声明
- `src/core/main.cpp`：--weather CLI（词校验点名 + save-management 独占
  检查纳入 + usage）
- `src/core/game_frame.cpp`：update() 顶部 updateWeather()（day cycle
  后、所有 raining 消费者前）+ 营火/灭火路径换谓词 + cropGrowthSteps
  传活天气 + updateWeather 实现（逐字日志 + thunder 2D 音 + 脉冲帧）
- `src/core/game_render.cpp`：render() 天空收口（sky × dim + pulse，
  clear/雾/u_dayFactor 同源）+ drawBeaconBeams u_dayFactor 换收口 +
  drawRain（云后 HUD 前，深度开混合开裁剪关）
- `src/core/game_init.cpp`：weatherEnabled strict read + 双流 seed +
  --weather override（init 逐字日志）+ 雨 program/mesh 构建（生成器后）
- `src/core/game_ui.cpp`：玩家灭火 `inWater ∨ raining` + F3 weather 填充
- `src/core/game_entities.cpp`：mob setRaining feed
- `src/ui/debug_overlay.{hpp,cpp}`：weather 行（第 17 行）
- `tests/test_weather.cpp`（新，17 例）/ test_debug_overlay（v2.4.0 钉 +
  17 行 + WeatherLine）/ test_mob（ExtinguishedInRain）
- `tools/generate_sounds.py` + `assets/sounds/thunder.wav`（新，308744 B）
- `CMakeLists.txt`（2.4.0 单点 + 两源文件）

## 门 175（编排者执行；old 侧 = v2.3.0 = 9b8305b 树）— 全 PASS

- **S1 结构免疫 = 三方恒等最强形**：60f fresh 六跑（new-on n1/n2 /
  old v2.3.0 o1/o2 / new-off x1/x2）全 179116 B，**n1 ≡ o1 ≡ x1 ≡
  base18.png 锚逐字节**（off ≡ old ≡ new-on 三方恒等；首段 CLEAR
  ≥9000f = 60f 窗零雨 + off 位级恒等双根）+ 跨版 savenorm **EXACT**
  （零 format，无 V2-3 的 series 字段差）。
- **S2 雨判别全 PASS**（全段无输入 headless）：
  - a：`--weather rain --frames 600` ×2 RAW 恒等（180317 B，确定性）
  - b：clear ×2 恒等（179116 B）+ rain vs clear 像素 diff 存在（雨
    可见）+ 120f savenorm 精确判别：**rain 侧恒无火**（fires 条目清空 +
    无 fire 块）/ clear 侧 f120 活火数 = real-dt 类（1-2 条，火寿命
    倒数自 worldgen 起算先于帧 0——只记录不门控）/ blocks+fluids 差 =
    熔岩湖反应前沿 churn 类（in-lake 断言）/ **非世界键全 EXACT**
  - c：营火场景 300f 互斥终态（rain → campfire_extinguished / clear →
    campfire）
  - d：火块场景 120f 互斥（rain → 格回生成态 air + fires 条目消失 /
    clear → 活火 remaining 2.02s 记录）
  - e：雷+停雨帧锁定：4 事件 ×2 名序恒等、间隔 [12.812, 23.133,
    14.513] vs [12.891, 23.134, 14.512] s（±0.08s 容差内 = 整帧锁定）
    + 雨窗 50.5 s ≈ kRainMin 3000f 50 s（--weather rain 固定开段实证）
- **S3** 2128/2128（编排者独立复跑）。
- **S6 点名 2/2 实证**：`render.weather: "yes"` → 「config
  render.weather must be a boolean (true/false), got a non-boolean」/
  `--weather fog` → 「Argument error: invalid --weather: 'fog' (use
  clear|rain)」；雷 salt 钉 / rain 初始态 kRainMin 钉 = 单测。
- **性能 A/B PASS 同世界同成本**（用户硬要求）：v2.3.0（tm_v240_perf
  = 9b8305b + g139 27 标记）vs v2.4.0（tm_v240_perf_new = 最终工作树 +
  g139），fall 900f 无输入 seed 123456789，banner 双侧实证：
  - PERF-WIN 三窗全指标同档（upd med 0.69/0.68/0.70 vs 0.70/0.70/0.67；
    p95 24.2/11.6/28.5 vs 22.5/28.7/22.5 窗间摆动无系统偏移；render
    med 0.63/0.67/0.67 vs 0.63/0.69/0.65；swap 15.1 vs 15.1 vsync 主导
    同值；fluid/fires/spread/ents/stream 全同档）
  - **PIPE3-S0 changesApplied 三窗 max 9/3/1 双侧逐位**
  - **终档 fluids 18/18 + blocks 55/55 集合恒等**（同世界最强形）
  - **rain 自成本预算**（v2.4.0 同二进制 --weather rain vs clear
    900f）：render med 0.42/0.35/0.34（≤ clear 0.63/0.67/0.67）、upd
    med 0.66/0.66/0.61 同档、swap 15.5 同值 = 300 滴静态 VBO + 1 draw
    成本在噪声带内（fires 0.009→0.0005 = 雨灭火块的行为差，非成本）

## 测试与构建

- **2128/2128**（+19：weather 17 + F3 WeatherLine + mob ExtinguishedInRain；
  设计 §6 八组全覆盖：流序列钉 seed 123456789 前 5 周期 29 事件逐帧 +
  双流终态 hex 钉 / 首段 CLEAR ≥9000f 五种子 / --weather rain 初始态
  （kRainMin 固定 + 主流零消费 + 其后走流 10203→4436 钉）/ 雨滴纯函数
  （字段 ×2 + 边界 + 4 滴 16 值 hexfloat 钉 + 位置 ×2 + 周期 16 恒等 +
  5 帧 y 走钉）/ 灭火三例（谓词 fire+campfire 共用 + off 免疫 5 断言 +
  mob 效果实体路）/ soilIsWet raining 变体 + 节奏 2×（111/200→200/200）/
  config 严格读（非 bool 点名 ×2）/ CLI 词校验（fog 点名）+ 附加：
  thunderPulse 5 帧衰减钉 / thunder wav 资产钉）。
- `-Wall -Wextra -Wpedantic -Werror` 0 警告（全量重建实证）。
- 冒烟（本地实证）：`--frames 60 --new --save v24_smoke` banner v2.4.0 +
  零 Weather 日志；`--weather rain --frames 5` 两行 override 逐字；
  `--weather fog` 点名拒；`--list-saves --weather rain` 独占拒。

## 偏离（记档）

1. **初始 CLEAR 不持久**（零 format 定案的直接后果）：重载/新 run 天气
   一律从首段 CLEAR 重启（--weather rain 除外）——MC 持久天气 = 记档
   偏离（云漂移同款 session 派生先例类）。
2. **屋檐遮蔽无**：雨中 fire 块/营火/燃烧效果一律灭（MC 有遮蔽检查）=
   设计定案记档偏离。
3. **雨声循环无**：项目音效集无循环音范式 = 设计定案记档（雷 = 一次性
   远雷 2D 中心）。
4. **雷无伤害/落点**：无声外效果 = 设计定案记档（MC 闪电可落点伤害）。
5. **流体不注水**：雨不产生流体（MC 同款，设计记档）。
6. **render.weather = strict read**（非 getBool 静默回落）：设计 §4
   "getBool 通道"（render.clouds 先例 = 代码默认 true + config 零改动）
   与 §6 "config 严格读（非 bool 点名）"并读，取严格读 = SAVE-1 纪律
   （tutorialEnabled 同款）——S6 "render.weather 非 bool" 点名门依赖之。
