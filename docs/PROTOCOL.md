# 开发协议

> Phase 30 文档整理：合并 `SESSION_CONTEXT.md` 的「命令」「技术栈与约束」「硬性
> 约定」、`README.md` 的「测试」（均未改写），并新增「E2E 协议」节（汇总 P25 起
> 逐步定型、P28 完善的截图/确定性验证协议）。

## 命令

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build  # 构建（0 警告要求）
ctest --test-dir build --output-on-failure                                  # 测试
./build/bin/tokenmine --frames 60 --screenshot /tmp/opencode/shot.png       # 自动化验证（退出码 0）
./build/bin/tokenmine --seed 999 ...                                        # 换种子
magick shot.png -format "%[pixel:p{x,y}]" info:                             # 像素采样
```

截图验证法：`glReadPixels` 必须在 `swapBuffers()` 后（前缓冲；后缓冲全 0）。
像素位置/颜色估算写小程序用真实 `Mat4`/`Camera`（心算投影不可靠）。
`magick ... txt:` 输出 `(r,g,b,a)`，awk 取 `$2`（开头 `(` 破坏数值转换）。
## 技术栈与约束

C++20、CMake+Ninja、SDL3、OpenGL 3.3 core（GLEW）、GTest、zlib。**无网络下载**。
`-Wall -Wextra -Wpedantic -Werror` 恒 0 警告。GCC 16.2.1 / X11 `:0` / Mesa 26.2.3 / 18 核。
内置 `Result<T>` + `MC_TRY`/`MC_FAIL`（**MC_FAIL 不含 return**，宏内 return）；
内置 JSON（`core/json.hpp`：find/isString/asInt.../members()）；内置 PNG（`core/png.hpp`）。
独立测试程序链接：`build/lib/libmc_core.a -lSDL3 -lGLEW -lGL -lz -lpthread`，
`glReadPixels` 需 `#include <GL/glew.h>`。
## 硬性约定（违反即 bug）

1. world/chunk/local 坐标严格分离，换算只在 `coordinates.hpp`；负坐标必须 floorDiv。
2. 渲染隔离：游戏逻辑不碰 GL/SDL（renderer/ 与 window/input 边界除外）。
3. 数据驱动：加方块/biome/矿脉 = JSON+纹理，不改核心；核心不 switch 方块 id（读 properties）。
4. air 是代码哨兵非数据方块（无 air.json）；drops 为空 = 不掉落。
5. 错误显式传播（Result），无静默失败；无 TODO/伪代码；无魔法数字（命名常量）；
   无硬编码方块检查；不删功能过构建；不禁用测试。
6. 面表绕序（有回归测试 `FaceWindingIsCounterClockwiseFromOutside` 守护）：
   +Y 角点 `{(0,1,0),(0,1,1),(1,1,1),(1,1,0)}`，-Y `{(0,0,1),(0,0,0),(1,0,0),(1,0,1)}`。
7. 相机约定：yaw 0 = 朝 -Z，正=左转；pitch 正=上，clamp ±~89°。
   `forward()={-sin(yaw)cos(pitch), sin(pitch), -cos(yaw)cos(pitch)}`。
8. 光照（Phase 7 正式；LGT-2/3、PRE-2/5/6、LGT-4 演进）：光值 0-15 存
   chunk（sky/block 两数组），查询走 `World::getSkyLight/getBlockLight`
   （虚空=sky15/block0）。呈现层 frag 公式
   `light = max(v_sky − (8/15)×(1−u_dayFactor), v_block)`，
   `brightness = (kAmbient 0.03 + (1−kAmbient)×light×faceShade)`，faceShade
   顶 1.0/±X 0.8/±Z 0.6/底 0.5（顶点着色器按法线算）。**LGT-3 仿射
   撤退**：存的是源等级 15 的 noon 场；夜间源等级 S = 7 + 8×df（kNightSkyLevel
   7 = MC 月光地板），天空项 = max(存值−(15−S), 0)——零重传播、逐格精确
   （confluence 单测）；block 光不随昼夜变（萤石夜亮）。**LGT-2**：光场
   进入成本由 `light_opacity`（0-15，缺省 opaque 派生 = 内容零变化）。
   **PRE-2**：距离雾（u_fog* 4 uniform，深度走独立表达式，gl_Position 树
   位级红线）。**PRE-5**：cutout 桶 discard（u_discardEnabled 门控）。
   **PRE-6/LGT-4**：per-draw 光覆盖 `u_useLightOverride`+sky/block 3
   uniform（掉落物 itemDropLight / 信标光束 beamLightSample = 信标块上方
   空气格 (x,y+1,z)），覆盖 draw 后强制 bind+复位 0（UXFIX-1 纪律）。
   按面采样（面后方一格），块状光照感。截图识面：顶面最亮、-Z 面最暗的
   侧面。
9. `Chunk&` 引用被 map rehash 失效（world.hpp 头部已文档化）。
10. 演示区：4×4 chunk 列（世界 x,z ∈ [-32,32)）；玩家按 `kSpawnCandidates`
    自动找干燥陆地（game.cpp）：候选列需 h > seaLevel 且 `Player::hasRoomAt`
    （出生 AABB 内无 solid 方块，避开树干/树冠）。
11. 玩家物理：position = AABB **底面中心**（MC 约定）；重叠判定**严格不等式**
    `min < b+1 && max > b`（块边界是整数、float 精确，贴面不判重叠 → 无 epsilon、
    无抖动）；被挡轴精确贴块面+该轴速度清零；Y 向下被挡 = onGround；
    防穿透靠子步化（每子步 ≤0.4 块，≤64 子步）+ 终端速度 38；
    固体判定只读 `properties.solid`（水可穿过；游泳见 P9）。
12. 相机是纯视点（position/yaw/pitch/viewMatrix/forward/right）；
    Game::update 每帧 `player.update(dt,input,lookEnabled)` 后把眼位+yaw/pitch 抄给相机。
    **鼠标转向仅在指针锁定（relativeMouseMode）时生效**（Phase 5 修复：未锁定光标
    的移动会破坏截图逐字节复现；标准 FPS 惯例=捕获后才能转视角）。
13. chunk streaming 帧管线（Phase 14，并发正确性靠它）：worker job 只运行在
    **post→flush 窗口**（update 末尾 post，下一帧 update 开头 flush；此窗口内主线程
    只读 World——render 只读 chunkPositions+GPU mesh，截图只读 framebuffer）。
    生成 job 不碰 World（纯 generator 写 job 自己的 scratch chunk）；光照 job
    （updateRegion）写光数组+dirty，主线程在只读窗口不写光数组（meshing 只读光、
    烘焙进顶点）；**光照 job：流送与流体追赶两类各至多 2 个/帧，且同一窗口内
    所有光照 job 两两位级独立**（99.10：判据 = 两列 region 位级独立——写 reach =
    列 box ±kLightUpdateRadius = ±1 列，block 光 BFS 边界种子环再 +1 列读 reach，
    `kLightParallelColumnGap = 2·(reach+1) = 4`：任轴 gap≥4 则写/写与写/读双向
    零共享 chunk；依赖对（双轴 gap≤3）或 3+ 列回退单 job 顺序执行 = pre-99.10
    逐位不变，含 LGT-1 定案不修的 overlap clobber 语义。confluence 证明：独立
    region 的最终光是插入前世界的纯函数，完成顺序无关 = 并行安全。99.11：流体
    追赶 job（`kFluidLightColumnsPerFrame = 2` 列/帧预算，set 序取列，
    `planFluidLightJobs`）与流送 job 跨类同窗仅当两两位级独立——
    postFluidLightJobs 对失败候选跳过（留待下帧），4 个光照 job 同窗 = 填充帧
    flush 下限 ≈14.4 ms（thread_pool.hpp 预算分析）。并行 job 的净变 chunk 走
    job-local 收集（dirtyOut），flush 后主线程统一 markChunkDirty——共享 dirty
    set 无并发写；流体列的 chunk 由此获新光值重烘 = 99.4 lit-pop-in 纪律）。
    主线程写 World（集成/交互/流体）只发生在 flush 之后、post 之前。
 14. **确定性双轨**（AGENTS.md Top 10 #8 的权威源）：**帧锁定**（固定 1/60
     帧步长）= 昼夜/Mob/箭/粒子/云/环境音/流体/光照/合成/烤制/生长/摆动
     相位——无输入运行逐帧字节可复现；**真实 dt** = 生存计时器（饥饿/
     进食/破坏进度/坠落/实体 age/火寿命/状态效果倒计时/自动存档）。截图
     只在帧边界字节一致；real-dt 字段与音频不入帧状态（截图/存档零影响）。
     长墙钟跑的 savenorm 按 real-dt 类剥离（saturation/hunger/
     effects[].remaining/fires[].remaining/entities.age，PITS 244/286）。
## 测试

```bash
ctest --test-dir build --output-on-failure
# 或直接运行
./build/bin/tokenmine_tests
```
## E2E 协议（截图/确定性验证）

> Phase 30 新增：此前散落在各阶段存档与坑位合集（P5/P15/P25/P27/P28/P29），此处汇总。
> 细节与教训见 `PITS.md` 对应条目。

- **零并发 :0 活动**：整批运行期间用户手离开鼠标（物理光标是验证的不可控变量——
  指针未锁定时 SDL 仍投递 mouse motion 事件，P5 条）；每拍截图前 `unmap_covers`
  解映射覆盖游戏区（(0,60) 1280×720 邻域）的已映射窗口（P27 用户鼠标污染教训）。
- **无输入运行**：×2 全帧字节一致 + skygate（FRESH = 天空模式色正午 (120,181,242) /
  黄昏 (130,63,38)，采样中心区像素众数）+ **base 门** `/tmp/opencode/base18.png`
  字节一致（无输入基线；新系统需论证"无输入下零活动 = 结构性免疫"，P26 起惯例）。
  **notblack 判别必须用 PNG 字节尺寸**（PITS 286）：环境窗口态把截图打成
  均色时文件 ~22 KB（有效帧 300-430 KB），`fx:mean` 探针不可信（均色值不
  固定，假亮放行过整批黑帧）——每跑后 `stat -c%s` 尺寸门 + 重试。
  **场景存档无输入运行例外（P54 起）**：玩家置于场景、视野含流送边界列时，
  远列的 pop-in 帧与墙钟相关（P14：光照 job 每帧至多 1 个、worker 吞吐按
  墙钟——240f 两跑差 10 ms 即可让地平线树冠碎片在终帧有无翻转）→
  **截图 ×2 字节一致对场景跑不是有效断言**；权威 = **存档 ×2 字节一致**
  （全量世界状态）+ notblack + skygate，与输入跑同款纪律（P53 ctl 先例）。
- **输入驱动运行（xdotool 脚本）**：**存档状态（含 yaw/pitch）是确定性权威证据**；
  全帧字节一致受 SDL 指针锁定 grab delta 环境限制（P28 条：xdotool XWarpPointer 只移
  虚拟光标，进相对模式时 SDL 报一次性 motion delta ≈ warp 位置 − SDL 计算的窗口中心
  + 物理鼠标微漂移 → 锁定时一次相机旋转，0.0025 rad/px，4 px = ±0.01 rad；同批首拍
  常干净、后续拍 4 px 可复现）。缓解：warp 到实际窗口原点 + (632,352)（SDL 中心；
  实测原点 (4,64) 而非名义 (0,60)）、同批同 warp、手保持不动；**存档 yaw/pitch 作
  漂移探测器**（干净 = 0.0000/初始值，漂移 = ±0.0100）。
- **窗口发现延迟 D**：输入锚定在窗口发现（`xdotool search` 首次命中）时刻，D ∈
  [0.1, 0.5] s；tick 类断言需 ≥1.6 s 裕量（P28 进食对 5 连验证模式：
  1/60 双累加边界精确推帧——1.6s 进食通道完成帧 97、36s 衰减 tick 帧 2161、
  20s 饥饿伤害帧 1200、18s 疾跑 tick 帧 1080，Phase 31 起）。
- **HUD 红心检查**（心带 y[646,661] x[551,729] 红像素计数）：无输入运行必须 = 0
  （>50 = 有人点了鼠标 = 污染，废拍重跑）；输入运行必须 >50（= 指针已锁、输入生效）。
- **场景门变体**：直下视角无天空 → 唯一色计数（>50 色 = 场景存在）；帧差分析先排除
  已知 UI 变量（饥饿/HP 图标状态变化也在 diff 里，P29 条）。
- **驱动模式**（`/tmp/opencode/p29_e2e.zsh`，P28 起沿用）：每拍 `shot()` = 启动游戏 →
  轮询窗口（≤4s）→ `windowmove 0 60` → 读实际原点 → [可选]输入子 shell（sleep 锚定
  窗口发现时刻）→ wait → 存档 dump → 场景门 → HUD 检。
