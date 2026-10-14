# 坑位合集（都踩过，勿再踩）

> Phase 30 文档整理：从 `SESSION_CONTEXT.md` **原样**移入（未改写）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/*.zsh`）指向当时状态；当前文档布局见
> `README.md`「文档地图」。

- GTest：TEST 宏类继承 `testing::Test` → 局部 struct/变量**不能命名 `Setup`**（冲突其私有方法）；
  宏参数里 `(Vec3{...})` 要加括号；`ASSERT_*` 只用于 void 测试体。
- 原始字符串字面量：`R"({"id":")"` 的定界符变成 `{"id":` —— `(` 必须紧跟 `R"`。
  JSON 测试串多打一个 `)` 会在奇怪列号报 parse 错（逐字符数）。
- 类外定义成员函数时嵌套类型要限定：`const WorldGenerator::ResolvedBiome&`。
- `WorldGenerator` 默认构造私有（空实例危险）→ Game/测试里用 `std::optional<WorldGenerator>`。
- 未使用的局部变量触发 `-Werror=unused-but-set-variable`；结构化绑定未用不告警
  （与 block_registry 既有模式一致）。
- `findChunk` 是 const（要改 chunk 用 `getOrCreateChunk`）；`Result` 没有 `.into()`。
- 聚合初始化：结构体独立 `int dx,dy,dz` 成员不需要内层花括号。
- 测试数据临时目录模式：`std::filesystem::temp_directory_path()/("mc_xxx_" + ::getpid())`。
- **离散积分 ≠ 连续估算**：半隐式欧拉（先 v+=g·h 再 y+=v·h）的跳跃峰值是
  `1 + 72/60 = 2.2`（精确有理数），不是连续的 2.13——测试边界按离散值定。
- **float 字面量 vs 表达式**：`11.7f` ≠ `12.0f - 0.3f`（位级差 5e-6）——
  断言里必须用与代码相同的表达式（如 `10.0f - 0.3f`），别写 `9.7f`。
- **重生周期 ~102 tick**：虚空测试不要假设"N tick 后在出生点"，要扫描
  "y==spawn && v==0"（这两个条件同时成立只可能是刚重生）。
- `magick shot.png txt:`（整图）行格式 `x,y: (r,g,b) #hex srgba(...)`，
  `-F'[(,)]'` 下颜色在 **$3/$4/$5**（单像素查询 `%[pixel:p{x,y}]` 才是 $2）——
  分类脚本写错字段会得到 sky=0%/other=99% 的假结果。
- **物理光标是验证的不可控变量**：指针未锁定时 SDL 仍投递 mouse motion 事件
  （光标进入窗口还会合成一个大 xrel delta）→ 玩家视角变 → 截图不可复现。
  排查特征：全图微小色差（每通道 1~3）、非结构性。Phase 5 修复=只在锁定时转视角
  （`Player::update(dt,input,lookEnabled)`）；截图验证仍应把光标移出窗口
  （`xdotool mousemove 100 100`）。
- `Input` 测试入口：`handleKey(SDLK_W, true)`（SDL 键码在边界映射）；
  `isDown` 跨 frameBegin 保持，`handleMouseMotion(dx,dy)` 累加 mouseDelta。
- 台阶测试要防"跳上后走下台阶另一端"：台阶宽度 ≥10 格（1.5s 内走不完），
  且跳跃只按 1 tick（松 Space）避免落地后连跳把玩家弹下台阶。
- **DDA 起点 cell 不测试**（光线不打自身所在方块）：测试里目标方块别放在
  floor(origin) 的 cell 内（-4.5 的 floor 是 -5 不是 -4！）；命中距离是到
  **命中面平面**的距离，不是到方块中心（origin z=2.5 打 (0,0,0) → 1.5 不是 2.5）。
- 角落穿越（两个 tMax 相等）法线方向取决于浮点 tie-break——测试要避开精确
  角落（选非 1:1 斜率如 (-2,0,-3)/√13）。
- 默认玩家 AABB（宽 0.6 居中）不跨越任何块边界——测"侧面重叠"要手动构造
  跨边界的 AABB（如 min.x=0.9/max.x=1.5）。
- **输入状态机时序**：ESC 锁定时=释放、未锁定=退出；run() 的退出检查在
  update() 之前，用**上一帧**的锁定状态（语义正确：锁定期间按 ESC 不会退出）。
- xdotool E2E 序列（窗口在 644,444，1280x720）：`mousemove 1284 804; click 1`
  （锁定）→ `mousemove_relative 0 300`（pitch -0.25→-1.0 朝下看）→ `click 1`
  （破坏）→ `key Tab`（切 hotbar[1]）→ `click 3`（放置）。每次运行前
  `mousemove 100 100`（光标移出窗口，避免合成 motion delta）。
- E2E 截图 diff 解读：1 格方块在 1.6 块距离+pitch -70° 时占屏面很大（数百 kpx
  diff 正常）；base 与 break/place 的 pitch 不同 → 全图 diff 是设计使然，
  要看局部区域（ASCII 分类缩略图）判断内容。
- **E2E 准星位置 ≠ 直觉**：pitch -1.0（朝下 57°）时屏幕中心射线打的是前方
  ~2 格的地面，脚下 1 格处的洞在屏幕中心**下方 ~110px**。放置目标 = 命中面
  邻格，所以"对准洞"要多下移视角或移动位置；E2E 看到 glowstone 放在洞旁边
  的平地上是瞄准问题，不是放置 bug（先核对命中几何再怀疑代码）。
- **局部重传播的 dirty 必须按"净变化"判定**：sky 阶段先整列清 0（rescan）再 BFS
  回填，中间值 ≠ 最终值——若按"写过就标脏"，无净变化的更新也会触发全 chunk
  重网格。方案：更新前保存区域 chunk 的光值数组，更新后 memcmp，不同才 dirty
  （updateLocalDirtyTracking 测试守护）。
- **区域外必须留 shell 种子**：updateLocal 的区域（±16 列/盒）外的光值不变但
  是区域内传播的**边界源**——BFS 队列要额外种上区域外紧邻一圈（shell）的亮格，
  否则"盒外的远处光源照进盒边"会被清掉后无法恢复（UpdateLocalKeepsDistantSourceLight
  守护）。区域外格只读不写（inRegion=false 时 BFS 跳过写入/入队）。
- **天空光局部更新必须整列而非盒**：天空光垂直无衰减——拆掉 (c) 的天花板后
  整根柱子（包括 c.y-17 以下）都会变亮，±16 盒装不下。所以 sky 阶段重扫
  ±16 X/Z 足迹的**整列**（y 0..127）+ 整列 BFS；block 光阶段才用 ±16 盒。
- **光查询的虚空约定**：缺失 chunk→sky 15/block 0（世界边缘是开放天空，光会从
  边缘照进来，符合"边缘即虚空"）；y≥128→sky 15；y<0→全 0。BFS 写邻居前必须
  findChunkMut 判空（void 永不创建 chunk）。
- 纹理脚本 `tools/generate_block_textures.py` 是确定性的：重跑后既有 PNG 逐字节
  不变（zlib 压缩确定），git status 只应出现新增纹理。
- **Mesa ANV（Intel Arc）把 shader `gl_PointSize` 钳到 1px**（ALIASED_POINT_SIZE_RANGE
  报 [1,255] 但实际无效）——点精灵不可用。天空对象（太阳/月亮/星星）改用
  **像素级 billboard quad**：view-space 膨胀（1px = 2·tan(fovY/2)/H × 深度，两轴相同），
  frag 里 `dot(corner,corner)>0.25` discard 成圆形。quad 角点放在 MeshVertex 的
  normal 槽（VAO layout 不动，sky shader 读 location 1 当 a_corner）。
- **GCC 16 拒绝嵌套类 NSDMI 出现在外层类默认参数里**：`struct Params{double x=1;};`
  嵌套在 class 内、`explicit A(Params p = {})` 或 `= Params()` 都报
  "could not convert/required before the end of its enclosing class"。
  修复：Params 提升到**命名空间作用域**，类内留 `using Params = ...;` 别名。
- **xdotool 负坐标**：`mousemove_relative -- -505 -294`（`--` 前必须加，否则
  "unrecognized option"）。
- **鼠标转向符号**：`yaw -= dx·sens`（鼠标右移=视角右转=yaw 减小）、
  `pitch -= dy·sens`（鼠标上移=视角上抬）。瞄准太阳/月亮时 dx 的正负别按直觉——
  太阳在东（+X）在 -Z 视野的**右侧**，yaw 要转负 → dx 为**正**（右移）。
  另外瞄准计算必须含**视差**（相机在玩家眼位 ~55 格高，不在世界原点；
  用真实 Mat4/Camera 算屏幕位置，别只比 sunDir 和 forward）。
- **逐字节基线不变性的精度策略**（P8 保证 P7 基线截图不变）：正午落在 dayFactor
  常值平台（分支返回精确 1.0f，不走过表达式）+ skyColor 常值段短路（a==b 直接返回，
  避开 lerp 同端点的 1ulp 漂移）+ shader `×1.0f` 位级恒等。验证：60 帧正午截图与
  P7 基线 cmp 一致（含 clean rebuild）。
- **时间推进用固定步长不用墙钟**：`m_dayCycle.update(1.0/60.0)` 每帧——
  墙钟 dt 会让无输入运行截图不可复现。
- `make_unique<T>` 不能转发 brace-init-list（如 `Vec3{...}`）——先构造具名变量再传入。
- 成员上移基类的重构要逐个核对 ctor 初始化列表：P10 把 Player 成员移入 Entity 时漏了
  `m_spawnPos` → 虚空坠落重生在 (0,0,0)（既有测试捕获）。行为等价类重构必须靠既有测试
  + 字节基线双保险。
- P10 破坏 E2E 时序（120 帧 = 2.0s 墙钟，主循环 ~0.15s 后开始）：break click 必须落在
  帧限内（首次尝试因前置 sleep 1.0s 把它推过帧限而整 run 失败）；窗口未创建前的
  锁指针 click 会丢失；每次运行前 `mousemove 100 100`。
- 不同 xdotool 配方（A 组 vs C 组）的相机微差 = 全图 sub-pixel diff（不可消除）；
  组内自洽即可，组间不做字节对比。
- magick：`-compose difference -composite` 输出是 graya/alpha=0，txt: 解析用 graya 格式；
  裸 `%@` 不带 `-threshold` 对这些图不可靠；分区域 crop 必须存成单独文件
  （`magick A -crop G +repage a.png`）。

- **kKeyCount 从最后一个枚举算**（原 `GameKey::Escape + 1`，注释"Keep in sync"）——在枚举尾部
  加 Q 后数组越界（GCC -Werror=array-bounds 在 handleInteraction 的 std::array 下标处报出，
  报错位置离根因很远）；加键必须同步 kKeyCount（已改为 `GameKey::Q + 1`）。
- **GTest 宏参数里的花括号初始化**：`EXPECT_EQ(a, ItemStack {x, 1})` 的逗号被宏当参数分隔符
  →"宏传递了 3 个参数"——整个字面量要加括号 `(ItemStack {x, 1})`。**嵌套花括号**
  （`ItemStack {ItemID {1}, 3}`）用正则批量包括号会被 `[^}]*` 在第一个 `}` 截断产生残次品
  ——批量编辑后必须逐行核对。
- **E2E 脚本内 `cd` 会带走相对输出路径**：`p11_e2e.zsh A p11_a.png`（相对路径）+ 脚本内
  `cd repo` → 截图存进仓库目录（污染 git status）——E2E 输出路径一律传**绝对路径**。
- **倒计时类断言用二进制精确值**：`update(0.25)+update(0.25)` 恰好耗尽 0.5s（0.5-0.25-0.25
  位级为 0）；`N × (1/60)` 累减有 ~1e-15 残差，恰好耗尽的帧上断言会偶发失败。
- **引用改指针/值传参的连锁**：breakBlock 从 `std::optional<BlockHit>` 自取 hit 改为
  `const BlockHit&` 后，体内 `hit->` 全要改 `hit.`——编译器的 Vec3 构造错误是 `hit->` 的
  连锁噪音（参数类型烂了），先修根因再看连锁。
- **多行 JSON 测试串的行尾悬空引号**：`R"({...,"count":4},"` + 换行 + `"ingredients"...`
  的引号让换行落进 JSON 字符串内部（"unescaped control character in string"）。规则：
  行尾必须停在逗号上（`...,"` 的 `"` 属于下一行）。批量正则修复要覆盖下一行以
  **`{` 开头**（对象数组元素）的情况——只匹配下一行 `"` 开头的会漏（P12 实测漏 1 处）。
- **程序化编辑 raw string 会吃掉终止符**：Python `str.replace` 改 `R"({...})"` 尾部时
  若 old/new 只含到 `)"` 之前，容易把终止符的 `)` 丢掉 → "未终止的原始字符串"。
  改完必须 grep 核对每个 `R"(` 都有配对的 `)"`（或整体编译）。
- **craft 产出落"本操作刚清空的槽"**：add 的"第一个空槽"含消耗刚清空的低序槽——
  原料在 slot 1（清空后）时产出落 slot 1 而不是 slot 2。测试预期按"消耗后状态"推
  （P12 用探针 /tmp/opencode/probe_craft.cpp 实锤后修正预期，代码是对的）。
- **`Result<void>` 与 `Result<T>` 不能复用同一变量**：`auto r = buildX()`（void）后
  `r = registerY()`（ItemID）→ 类型不匹配编译错；不同 T 的 Result 用独立变量。
- **/tmp 脚本会丢执行权限**：`/tmp/opencode/*.zsh` 隔段时间 `chmod +x` 失效
  （exit=126 权限不够）——E2E 前 `chmod +x` 或 `zsh script` 显式解释器调用。

- **MC_TRY 是语句宏，不能当表达式用**：`return MC_TRY(x) ? A : B` 编译不过（do/while 块不是表达式）——
  `auto r = x; if (!r.ok()) { return r.error(); } return value;`（P13 collectSave 首版踩过）。
- **E2E 的 `--save slot` 第二次运行会加载第一次写的存档**：`--save` + 文件存在 = 加载路径生效——
  "全新"交互配方必须先 `rm -f data/saves/<slot>.json`（P13 实测：第二次 inter 在已修改世界上重放
  配方，破坏数 2→3、玩家再深一格、时间累加，存档当然不同——是加载路径在正常工作）。
- **按住破坏会自动续破下一块**：目标块破坏后按钮仍按住 → raycast 重新取目标并开始破下一块
  （进度只在松手/换目标时清零，resetBreaking）——想精确破 N 块的 E2E 配方：N×耗时+余量后松手，
  下一块已积的进度随松手丢弃（P13 inter2：1.7s 按住把 dirt+后面草都破了，掉落物塌到玩家 1.0 处被拾取；
  改 1.05s 只破 dirt）。
- **实体 age 按真实 dt 累积（墙钟），不是固定步长**：含实体的存档文件 age 字段跨运行差 ±1-2 帧，
  不做字节一致断言（归一化 age 后比较其余字段）；渲染不依赖 age（动画=帧计数器），截图仍确定。
  昼夜 time 是固定步长（位精确）——两者别混。
- **加载运行退出时会把存档写回（time 前进）**：连续两次 load 运行不是同一输入（第二次加载的是
  第一次写回的文件）。验"加载路径确定性"要快照存档文件、两次运行之间恢复（P13 驱动 cp/restore
  模式）；正午太阳在帧外时即使不 restore 截图也碰巧一致——不要依赖这个巧合。
- **拾取半径是 1.0（含边界）**：掉落物停在距玩家底心恰好 1.0 的位置**会被拾取**——E2E 想让掉落物
  留下时距离必须 >1.0（P13 用 1.414=√2 的对角距离）。

- **P14 流式触发只用"邻域"会退化成行走皮肤**：触发器只看玩家 ±1 邻域有无缺失列 →
  邻域填满即休眠，玩家继续走也只是"永远 1 列皮肤"，半径环永不填满（view_distance 名存
  实亡；seed 57 走 36 格只流了 14 列）。修：**迟滞**——激活=最近缺失列 ≤1 列；激活后
  **直到半径环充满**（nearestMissingDistance 返回 radius+1 哨兵）才解除。基线安全不变
  （静止中心玩家最近缺失=2 列，从未激活过 → 零 job）。
- **P14 光照 job 不能并发**：相邻新列的 updateRegion 区域各向 ±16 扩展会**重叠**，两个
  worker 并发写同一光数组=数据竞争（非原子字节写 + dirty 竞态）。修法：每帧至多 **1 个**
  光照 job，job 内按列 box 顺序 updateRegion（生成 job 纯函数可任意并发）。
- **P14 StreamJob 用 deque<unique_ptr>**：worker lambda 捕获 StreamJob 裸指针；
  vector push_back 扩容会移动元素 → 悬垂。deque 的 push_back/pop_front 不移动其他元素，
  且 job 本身堆分配永不移动。
- **P14 E2E 走位配方必须探针验证地形**：玩家无自动 step-up，任何 +1 上升都是停点。
  seed 123456789 的 -Z 线在 z=-28 有 4 格爬升（47→51）→ 玩家停在演示区内、流式休眠
  （只流 3 列）。解法：`--seed` 换种子 + 搜索探针（要求：出生 (0,0) 干燥+hasRoomAt、
  -Z 线无上升/无水/无悬崖直到越界、界外第一个 ≥1 上升=确定性停点）→ **seed 57**
  （53→35 一路 1-2 格下降、z=-32 界平、z=-36 一个 +1 台阶）。
- **P14 停点位置=单元格 +Z 面钳制**："z=-36 处上升"= solid 格 (·,·,-36) 跨 z∈[-36,-35)，
  其面向 +Z 的面在 **z=-35.0**——玩家前贴面钳在 -35.0、底心 z=-35.0+0.3=-34.7（不是
  -36.3！）。浮点 -35.0+0.3 逐位确定，两次运行一致。
- **P14 走位 E2E 最终加载集与时序无关**：玩家轨迹全程在停点列 8 列范围内 → 走位期间
  post 的列 ⊂ 停点列的半径 8 环；停点后环继续填满（273 列 ≪ 剩余帧数）→ 最终集 =
  演示区 ∪ 半径 8 环（精确 2312 chunk），与 dt 抖动无关 → 截图逐字节可复现。
  环填满后 nearestMissing=9>8 → 解除 → 状态冻结（实测截图前 14 s 静止）。
- **P14 昼夜是 240 s/天不是 60 帧/天**（day_length_seconds=240，start_time=0.25）：
  1200 帧只推进 1/12 天。walk 运行 t: 0.25→1/3、load 运行 1/3→5/12，**两者都在正午
  平台 [0.15,0.45)**（dayFactor 恒 1.0、天空色恒 kDaySky、太阳两个角度都在帧外）→
  load 截图与 walk 截图碰巧逐字节一致——是平台+太阳出画，不是 bug，也别依赖。
- **P14 nearestMissingDistance 哨兵 radius+1 与 trigger 比较要防碰撞**：radius=0 时哨兵=1，
  trigger=1 → `d<=trigger` 误判激活。streamingActive 必须 `d<=radius && d<=trigger`。
- **P14 读档点可能在未加载区**：存档玩家在演示区外时，读档世界没有他的列 → 触发器对
  "玩家自身列缺失"（距离 0）立即激活，~2-4 帧（4 ms 级）内流式补上地面，期间玩家下落
  ~0.03 格落在整数平面后逐位精确（无死亡风险，远小于 -32 杀线）。E2E 已验证。
- **P14 光照增量 == 全量等价测试的前提**：既有 chunk 不能有方块光源——否则新列的遮挡
  可能切断旧光路，而 updateRegion 的 block 光 BFS 只严格增大、不能跨区降低 → 不等价
  （封洞伪影，KNOWN_ISSUES 已记）。测试场景把 glowstone 放新列内、旧列全无光源。
- **P25 截图前缓冲陈旧 + 红闪伪装（E2E 判据可能整体作废，两个独立机制）**：
  `glReadPixels`（`swapBuffers` 后）读前缓冲。机制 A（真陈旧）：游戏窗口创建后
  有 **~3-5 s 的呈现冻结**（前缓冲停在早期帧；全屏遮挡——OpenCode UI 2552×1552 +
  其 awesome frame——会延长它）。3.0 s 的运行（180f）落在冻结窗口内 → 11/11 次
  陈旧（天空反解出 8~106 帧偏移）；6.0/10 s 的运行（360f/600f）→ 23/23 新鲜（含被红闪伪装初判陈旧的 8 次 600f）。
  机制 B（假陈旧）：**受伤红闪罩住天空**——红闪按真实 dt 衰减（P16 约定），最后
  一击落在截图帧前 ~17 个模拟帧时，60fps 下截图时刻 alpha≈0.15 → 天空
  `(123,60,37) → (143,51,31)`（= flash(0.15) 精确值），纯天空色门会误判"陈旧"；
  8 次运行 6 次逐字节一致（跨天、跨驱动复现 = 确定性冻结点/确定性模拟帧）。
  **模拟本身永远确定**（存档 time 恒为帧精确值：180f=0.5385 / 360f=0.541 /
  600f=0.5443…；t(N) = 0.536 + N/72000，day_length 1200）——陈旧的只是截图。
  正午平台场景两种机制都不可见（天空/dayFactor 恒定——P24"箭带 158~440px
  抖动"疑似机制 A 同源），黄昏 ramp 场景天空每帧差 ~0.03R 使其显形。
  **E2E 协议（P25 起，勿再省）**：① 批内零并发 :0 活动（UI 活动会重映射遮挡窗）；
  ② 每运行前 unmap 所有覆盖游戏区 (0,60)-(1280,780) 的已映射窗口（xwininfo 几何
  判定 X≤10、Y≤60、X+W≥1270、Y+H≥770、W<2560 排根窗——`p25_final_batch.zsh`
  `unmap_covers`）；③ 每截图过**天空色新鲜度门**（`skygate.py`：顶 120 行众数色
  vs 帧精确黄昏 lerp，严格 0 容差；**flash-aware**：同时接受红闪罩变体
  `flash(a)=(255a+R(1−a), G(1−a), B(1−a))`，a∈[0.10,0.25]，±1/通道 → FRESH-FLASH）；
  ④ **短窗口（<~5 s）运行不可信**——E2E 窗口取 ≥6 s（360f）；⑤ 陈旧弃跑重跑
  （每 case 备 3~8 次）。本批期望天空（t₀=0.536，{0.50 dusk}→{0.60 night}
  float32 线性）：180f=(135,66,39) / 360f=(130,63,38) / 600f=(123,60,37)。
  另：`pdiff.py` 已内建自测（每次调用先跑合成已知差对，覆盖 y≥1 行——P24
  slice 公式 bug 类别结构性防回归，`--selftest` 单独跑）。
- **P26 自写 PNG 解码器 RGB/RGBA 通道错位（假"周期性纹理错位 diff"）**：
  分析 dm vs dn 截图时临时手写的 PNG 解码器按 `stride = w*3`（RGB）假设，
  而游戏截图是 **8-bit RGBA 4 通道**（`takeScreenshot` 写 alpha=255；
  `file` 输出 `8-bit/color RGBA`）→ 每行错位 1 字节，y≥1 的"像素"全错 →
  假 diff = 7 个跨全宽 cluster（周期 ~426 px 的"纹理错位"图案）+ 大量
  R/G/B 恰 255 的饱和像素——极易误诊为 atlas UV 错位（本阶段刚改过
  `block_atlas.json`，嫌疑最大）。转折点 = 直接看截图肉眼可见 4 只 mob
  位置与 7 个 cluster 对不上 → `file` 查真实格式 → 改 PIL 解码后 diff =
  恰好 4 个 cluster、全在 mob 剪影内。**教训：E2E 像素分析一律用标准库
  解码（PIL），先对已知颜色像素（天空/地面）做 sanity check 再统计；
  截图格式以 `file` 输出为准，别假设。**
- **P27 预测表必须用仓库真实函数 + 一次日志校验**：本会话为 %5 新名单手写
  Python splitmix64 时误加了 `x += γ` 步（状态机式），而仓库
  `splitmix64(x)`（`world/noise.hpp`）是**纯 finalizer**（无加法步）→ %5 全表
  预测错误（f60 预测 chicken、实际 zombie）；%4 下恰好碰对 4 个值（P26 的
  4 抽全对）险些漏检。教训：**确定性预测表 = 逐字节对照仓库实现抄公式，
  写完先用一次真实运行的 debug 日志校验前几抽，再信任整表**（P27 用
  `--log-level debug` 的 mob spawn 日志校验后才定 E2E 预期）。
- **P27 投射物自命中 t=0（spawn 在发射者 AABB 内）**：箭 spawn 点 =
  射手头部 + 0.3 块瞄准偏移，仍在 0.6 宽身体 AABB 内；扫掠测试语义
  "t=0 = 起点在盒内即命中" → 每支箭在下一帧命中**自己的射手**（自杀自灭
  + 沿箭方向击退 → 射手向玩家漂移，E2E 实证 6 箭全灭、health 恒 20、
  skeleton 半速漂移三症状同时出现）。修复 = `Projectile::source()` +
  命中循环跳过发射者。**凡"实体发射影响自身的投射物"设计，必须显式排除
  发射者**（玩家的弓箭无此问题：玩家不是 MobEntity 且无 hurtPlayer 标志）。
- **P27 直线瞄 + 重力 = 距离硬上限**：箭重力 20 b/s²、速度 15 b/s，直线瞄
  玩家眼高只在 `d²/22.5 ≤ ~1.75`（d ≤ ~6.3 块）内命中；10 块外箭从玩家脚下
  飞过（E2E 实证：skeleton 在 ~8 块外射击、health 恒 20）。修复 = 瞄点加
  0.5·g·(d/speed)² 提前量（`Projectile::kGravity` 转 public 供瞄点复用同一
  常量，勿抄第二份数值）。**任何投射物射击 AI 都要做弧线补偿或降低重力。**
- **P27 E2E 外部输入污染（用户在 :0 的真实鼠标）**：UI 窗口 unmapped 期间
  用户的鼠标仍悬停在原坐标（≈UI 区 = 游戏窗区上方）；一次真实点击/移动
  落到游戏窗口 = 指针锁（HUD 出现）+ 视角转动 + 挖掘（热栏物品/掉落物/
  伤害）——"无输入"假设全破（dayc 对 393829 vs 364018 字节不匹配、截图
  见 HUD + 9 物品热栏 + 地面掉落）。**检测器**：① ×2 字节一致本身就是
  污染检测器（污染几乎必使两拍不同）；② 驱动内 HUD 红心像素检查（心带
  y[646,661] x[551,729] 红像素 >50 = 污染；无输入运行**永远不该有 HUD**——
  `locked && !panelOpen()` 门，见到 HUD = 见到输入）。**热栏 9 物品是
  `data/hotbar.json` 初始配置（每个新世界都有），不是污染特征**——污染
  特征是 HUD 可见 + 相机移动 + 掉落物。

- **P28 SDL 指针锁定 grab delta（输入驱动 E2E 的环境物理，本机 :0）**：xdotool
  `mousemove`（XWarpPointer）只移虚拟光标、不移物理鼠标；游戏进 relative mode
  时 SDL 把光标 warp 到其自算窗口中心，并把（warp 位置 − 当时虚拟光标位置）
  作为一次性 motion delta 上报 → 锁定时首帧相机旋转一次（0.0025 rad/px：
  4 px = ±0.01 rad）。**实测规律**：warp 位置 1:1 传导（warp +4 px → 漂移
  +4 px）；同一 warp 位 batch 内首次运行 0 px、其后稳定 4 px（**手静止可复现**
  = 物理鼠标微漂移，非随机抖动）；WM 实际窗口原点 (4,64)（`windowmove 0 60`
  不被平铺 WM 尊重）→ SDL 自算中心 = (636,416)，与 xdotool 名义中心 (644,424)
  差 8 px。**对策**：warp 光标到 SDL 实测中心（origin+(632,352)）+ 背靠背
  同 warp + 手静止；**存档 yaw/pitch 是漂移探测器**（干净 = 0.0000/−0.2500，
  漂移 = −0.0100/−0.2600）。**结论：输入驱动 E2E 的全帧字节一致受此环境物理
  限制——存档状态（含 yaw/pitch）是输入 E2E 的确定性权威证据，全帧字节一致
  用于无输入回归。**
- **P28 `std::string::replace` 4 参重载截断**：`replace(pos, len, const char* s,
  size_t n)` 的替换内容是 **s 的前 n 字符**（不是"替换 len 个字符后写入完整
  s"）——P28 测试代码误用（想插入长 JSON 片段、n 传小）只取到前几字符导致
  JSON 损坏。**字符串插入一律用 3 参 `replace(pos, len, std::string)` 版本。**

- **P32 Config getters 在类型不符时静默回退默认**：`getDouble("k", def)` 对
  **缺失键和非数值（如字符串 `"36"`）都返回 def**——调参类参数若直接走
  getter，用户写错类型会静默拿默认值、无从察觉。**需要校验的参数自己读
  `at(key)` + `isNumber()`**（`Player::Params::fromConfig` 模式：缺失 = 默认、
  非数值 = 启动报错、越界 = 启动报错）。
- **P29 E2E 视觉验证的视角选择（直下视角可视锥）**：直下视角（pitch −1.5，
  准星距脚 0.115 块）的可视范围只覆盖脚周 0.11–1.28 块环且只含较低部分（帧顶
  边 pitch −0.90 = 地平线以下 51.6°）——破坏粒子有向上偏置速度（+1.2~2.7 b/s）
  且水平飞 0.3–2.3 块，大多飞出锥外（太远或太高）→ **截图里 0 个粒子**
  （E2E 实证：P29 首拍视觉验证用直下视角，帧差 = 仅 29 px 饥饿图标，无粒子
  像素）。**粒子视觉验证必须选斜视角**（pitch −1.02 级别、准星在前方地面格
  面心：帧覆盖 pitch [−1.63, −0.41]，坑心在画面中，5–16 粒子可见）；帧差
  分析先排除已知 UI 变量（饥饿/HP 图标状态变化也在 diff 里——本对 29 px =
  饥饿图标 20→19，不是粒子）。
- **流式激活方向（迟滞）——E2E 场景设计**：流式**激活**的条件是玩家距缺失列
  ≤1 列（`nearestMissing > kStreamTriggerDistance(1)` 时提前返回 = 不激活），
  即玩家必须站在已加载区的**边缘**；在已加载区**内部**（nearestMissing ≥ 2）
  的玩家永不激活，**即使**半径方远未填满（P33 第一次尝试：玩家放演示区中心
  (0,0)，4×4 演示区内 nearestMissing=2 → 720 帧日志 0 条 "Streamed column"，
  场景白做）。**无输入 E2E 要让流式激活，玩家必须放在"演示边缘列"**（如
  (1,0)：已加载 + (2,0) 是距离 1 的缺失列）。激活后迟滞保持（半径环充满前不
  解除），死亡门控（P33）在激活判断之前提前返回、不触碰激活状态。
- **日志带 ANSI 色码（重定向到文件也是）**：logger 写 `\x1b[32mINFO \x1b[0m`
  形式，不是仅 tty 才着色——E2E 日志的正则解析必须先
  `re.sub(r'\x1b\[[0-9;]*m', '', line)`（P33 首版日志解析因此 0 命中，误报
  "log empty"）。
- **存档 `time` 是累加的、含源存档自身的偏移**：存档 `time` =
  start_time_of_day + 总帧数 × (1/60)/day_length（固定每帧步长）；E2E 的源
  存档若是 N 帧运行生成的，它的 time 已含 N 步（5f 源存档 → +5/14400）——
  不要假设起点是 config 的 start_time_of_day（P33 断言 0.5 差了恰 5/14400，
  实际 = 0.25 + 3605/14400）。
- **UI 文字颜色对 textured 批次无效（`ui.frag`）**：着色器 textured 分支输出
  `texel.rgb`（字体纹理 = 白字），`u_color` 只调制 alpha（着色器注释有记载的
  既有设计）——`addText(…, 红色)` 静默渲染为白字（死亡屏幕 "You died!"、
  合成 "needs:" 暗色、tooltip 颜色全部如此）。P16 起从未被像素级 E2E 验证，
  P33 才发现。像素断言文字时按**字体色（白）**检查，不要按代码里的颜色参数。
- **awesome WM 会平铺新窗口（用户会话重启后布局变了，P34 发现）**：P34 开始时
  游戏窗口映射成 1272×1552 的 tile，`xdotool windowmove/windowsize` 被忽略
  （P0–P33 的会话里窗口是浮动的 1280×720——同一台机器、同一 WM，会话状态
  不同）。E2E 恢复：`windowfocus <wid>` + `xdotool key Super+ctrl+space`
  （用户 awesome 配置的浮动切换绑定）→ 窗口变 1280×720 且 `windowmove` 生效
  （原点 (4,64)，同 P33）。**必须验证切换结果**（首按可能因焦点竞态被吞——
  循环"量宽 ≠1280 → 重新 focus+切换"，上限 3 次，仍不对就 FATAL）；
  无输入运行里这是纯 WM 层输入（游戏无焦点事件处理，零 SDL 事件，
  基线结构性不受影响）。`xdotool search --class minecraft` 找窗口（
  `--name "MineCraft"` 可能误配终端标题）。
- **E2E probe 配置必须每批重新生成并核对内容（P34 踩坑）**：
  `/tmp/opencode/config_p34ui2.json` 里残留着上一轮会话写的 `ui.scale: 1`
  （键名正确、值错误），两整轮 panel 运行全部按 scale 1 渲染、点击坐标
  （按 scale 2 计算）落空，浪费一轮调试。批处理脚本开头必须用
  config_xep 重新 `json.dump` probe 配置，且跑前 `grep` 一眼关键值。
- **`textWidth` 含 1px 字距，ink bbox ≠ n×8×scale（P34）**：
  `textWidth(text, scale) = n×8×scale + (n−1)×scale`（glyph 8px + 1px
  spacing，`adv=(kGlyphSize+1)×scale`）——"You died!"（9 字 scale 2）
  文本宽 160 不是 144，原点 (1280−160)/2 = 560；ink 实际宽 150（'!' 窄）。
  断言文字几何时按 `textWidth` 公式算，别用 n×8×scale 估。
- **加载即死的存档（pos 低于击杀平面 −32）的 pos.y 有 ~2e-5 真实 dt 抖动
  （P34）**：玩家加载在 y=−33，首帧物理用真实 dt 落一点点才触发
  `belowKillPlane` 死亡——两次运行的 `player.pos[1]` 差 ~2.3e-5
  （其余存档字段全部逐字节一致、截图逐字节一致）。虚空死亡场景的存档
  对比要豁免 pos.y（截图 + 其余字段才是权威证据）；这是"玩家物理 = 真实
  dt"既有约定的自然推论，非缺陷。
- **游戏对缺失的 config 文件自动补写种子配置（含 `ui.scale: 1`）（P35）**：
  E2E 批引用了一个批处理还没生成的 probe 配置文件名，游戏首启把缺失文件
  写成默认种子配置（`"ui": {"scale": 1}`）并按它运行——P35 冒烟的 5 次
  "scale 2"面板运行全部实际按 scale 1 跑（点击坐标按 scale 2 布局计算，
  槽 5 偏一格落到槽 4，存档结果对不上坐标推算）。发现靠 5 次重跑 + 临时
  PANELCLICK 日志（暴露 `scale=1.00`）。规则：批处理开头 `rm` 旧 probe
  文件、从源 config 重新生成、断言关键键值（`ui == {'scale': 2}` 等）；
  跑前确认文件存在且内容正确。P34 probe 坑（残留错值）的具体化案例：
  那次是文件残留错值，这次是文件根本不存在被游戏自动补成默认。
- **指针解锁后 WM 堆叠会翻转，键/点击投递丢失（P35）**：E 开面板释放指针
  后，光标下方可能是别的窗口（用户浏览器 + 停在上面的物理光标 +
  focus-follows-mouse）：P35 冒烟观察到 (a) `xdotool key E` 被吞（面板从未
  打开，解锁态点击 = 世界破坏尝试）与 (b) `xdotool getmouselocation` 的
  window 字段间歇切到浏览器窗口。对策：每个输入步之前
  `xdotool windowraise` 游戏窗口（纯 WM 层操作，零 SDL 事件）+ 每次运行后
  按**存档状态**验证重试（槽位结果 + `blocks`/`fluids` 为空，≤3 次）；
  存档是确定性权威——重试通过的运行其证据不受影响。
- **分数系数缩放布局：长度全程 double 运算、只在 rect 边界 lround（P35）**：
  逐段独立 lround 会让段宽之和超过窗口（s=0.638 时
  2×8+321+8+110+8+179=642 > 640，面板超窗 2px）；正确做法：所有长度保持
  double（slot=52s、gap=4s、pad=12s、…），panelW = native×s 精确、
  子 rect 从同一组 double 原点推导、只在边界取整——相邻 rect 互相一致
  （hit test 与渲染共用同一 rect 集合）。
- **C++ 循环变量遮蔽外层同名变量（P35）**：`InventoryLayout::compute` 的
  槽位循环 `for (int slot = 0; …)` 遮蔽了布局长度 `double slot`，全部槽位
  的坐标/尺寸按循环计数算出（slot.w=1、位置错乱）；`-Wall -Wextra
  -Wpedantic -Werror` 不查遮蔽（需 `-Wshadow`，未启用）。布局钉值单测
  （slot 宽 = kSlot）当场抓住。写循环别复用外层局部变量名。
- **手绘位图字体字形表极易出错（P35）**：95 个 16×16 手写字形第一稿有
  195 行宽度/行数错（生成器断言抓到），修完长度后大量字形形状也错
  （'a'/'b'/'&' 等畸形）。可靠路径：一次性从系统 TTF 提取（PIL：em16 +
  基线 row 12 + alpha>128 阈值）→ 静态表嵌入生成器（生成器保持纯
  stdlib、确定性，构建不需要 TTF）→ 合成预览 sheet（全部字形 + 样例句）
  目检迭代（counter 空洞/下伸部/细笔画）。
- **程序化图标 16×16 → 32×32 重采样：离散特征要逐一 ×2（P35）**：
  形状谓词（距离/椭圆/弧线）直接在 2× 坐标重评估，边缘自动获得 2× 细节；
  但离散单像素特征（羽毛翅膀 ±1、glint/斑点、骨端 knob）不会自动变粗——
  羽毛翅膀初稿保持 ±1（3px/32 = 9%，旧 3px/16 = 19%），放大目检才发现、
  修成 ±2。重采样后按旧 art 的"相对粗细"逐个核对离散特征。
- **E2E 参考资产放 /tmp 会被系统 tmp 清理整体抹掉（P36）**：P33→P36 间隔
  3 天，`base18.png`、`p28_daym_a.png`、`config_xep.json`、`skygate.py` 与全部
  批脚本消失。重建路径：probe 配置值 = 代码默认配置（批脚本内重新生成 +
  断言内容）；跨阶段基线门改用**上一阶段代码的参考构建 A/B**
  （`git worktree add <dir> <上一阶段 commit>` + 独立 build，同系统库版本，
  无输入运行逐字节对比——比旧参考图更严格，还顺带隔离了系统库升级影响）；
  基线图从验证过的运行重新生成、链条经 A/B 重锚。教训：关键参考资产要么
  入库、要么每批开头检查存在性（缺失 = 重建，不许"沿用"）。
- **zsh 的 `set -- $var` 不做词分割（P36）**：与 bash 不同，zsh 默认不分割
  未加引号的变量展开——`set -- $pair`（`pair="a b"`）只设 `$1="a b"`、`$2`
  未设，`set -u` 下直接致命错误（批死在第一个 pair 循环）。要分割用
  `set -- ${=pair}`。写批脚本的 pair 循环时逐处检查。
- **awesome WM 平铺新窗口是异步的，还会顺手改尺寸（P36）**：启动后一次性
  量窗口宽（1280 ✓）可能赶上平铺前的旧值，随后 WM 把窗口 resize 到平铺尺寸
  （实测 1272×1552）——游戏按窗口尺寸渲染，截图就是 1272×1552，整拍作废
  （P35 的"宽度检查"防不住这个竞态，P36 首拍 refbase1 就这么废了）。
  稳定化循环的完整教训（P36 连踩 4 轮才收敛）：
  ① **浮动 toggle 必须打对窗口**——OpenCode 的 Firefox 窗口覆盖游戏区域，
  不先 `windowraise`+`windowfocus`+把鼠标 warp 到窗口中心（focus-
  follows-mouse）就发 `Super+ctrl+space`，toggle 会翻转别的客户端，游戏
  窗口永远平铺；
  ② toggle 在 resize **之后**命中时，解除平铺会**还原客户端原始尺寸**
  （1272×1552 → 1280×720，无需 windowsize）但位置留在平铺位——之后
  `windowsize 1280 720` + `windowmove 0 60` 兜底；
  ③ **预算**：60 帧运行窗口存活仅 ~1.5s（init + 1.0s vsync 帧 + 截图），
  收敛必须一轮 ~1s 内完成（raise→检查→warp/focus→toggle→size/move→
  **同轮复查**）；循环里 `kill -0 $game` 先行——游戏已退出（窗口销毁、
  xwininfo 返回空）就 break，别空转；
  ④ **尺寸校验必须覆盖 panel()/f3run()**（不只有 shot()）：f3A2 的
  1272×1552 截图就是这么静默漏过、crop 断言才暴露的（运行后校验游戏日志
  "Screenshot saved: … (WxH)" = 1280x720，中途被平铺 = 作废）+ shot3/
  重试环（≤3 次）。preflight：批开头 `pgrep -f "[b]uild/bin/minecraft"`
  （方括号技巧防 pgrep 包装 shell 自匹配）——用户游戏/残留进程会抢
  xdotool 窗口搜索。
- **zsh 数字断言对 "n=10993" 形式的输出静默失败（P36）**：
  `n=$(... | awk '{print $NF}')` 取到的是 `n=10993`（不是数字），
  `[ "$n" -gt 10000 ]` 在 zsh 里不报错、只返回假 → 6 次 F3 运行明明
  墨值完美（10993 = P35 精确值）全部误 RETRY。取数值用
  `sed 's/.*n=//'` 并先验证是纯数字。
- **系统 SDL3 升级（0.4.16 → 0.4.18）让 CMake 缓存指向已消失的 .so
  （P36）**：`cmake --build build` 链接期报
  `'/usr/lib/libSDL3.so.0.4.16', needed by 'bin/minecraft', missing`。
  重跑 `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release` 重新解析即愈
  （0 警告保持）。跨天/跨周的会话接手后第一次构建先想到这个。
- **手工写进存档的流体编辑会下落扩散，断言"恰好 N 个编辑"必挂（P37）**：
  E2E 手工源存档在空气放一格流体 `(2,80,2)=3` → 流体模拟是**动态**的
  （下落 + 水平扩散，逐帧确定）：300 帧后水已落到地面扩散成 16 格，
  `(2,80,2)` 为空。"输出存档恰好含 3 个手工编辑"的断言全 MISMATCH
  （2 个方块编辑倒是精确的——方块是静态的）。对策：① A/B 字节一致性
  断言不受影响（两个构建演化出完全相同的世界——这本身就是证据）；
  ② 编辑内容断言改为**沉降终态精确钉值**（16 格逐格列出，取自串行
  构建的输出；跨构建/跨运行可复现 = 流体模拟确定性的又一验证）；
  ③ 需要"中间态 vs 末态"对比的场景（F5 验证）用**纯方块源存档**
  （blocks 静态、fluids=[]，两个时刻的世界完全相同）。
- **输入运行必须给 launch() 的稳定化留时间预算，否则按键落在末帧（P37）**：
  F5 验证脚本"launch → sleep 0.5 → 点击锁定 → F5"，但 launch() 在游戏
  **进程已启动之后**还要 0.3–1.2 s（窗口搜索 + float 稳定化），120f 的
  帧窗口总共 ~2.4 s → F5 实际落在末帧附近：F5 存档（mid）与退出存档的
  `time` 逐位相同（0.2586805555555555 = 末帧值）→ "mid vs exit"对比
  退化成同一状态比同一状态（表面 PASS、证据弱）。对策：① 输入运行的
  sleep 预算必须算上 launch 稳定化时长（frames 拉长、按键提前）；
  ② **两个存档对比的场景务必打印并目检 time 字段**——相同 = 两次写入
  几乎同刻 = 弱证据，必须重跑。重跑后 F5 落中段帧（time 0.2554 vs
  退出 0.2628），证据成立。P36 的 F3/panel 运行没踩到（只有截图 +
  退出存档断言，无"两存档对比"）。
- **按住类动作（挥击/挖掘）走 `isDown`，`xdotool click 1` 永远打不中（P38）**：
  `Game::handleInteraction` 的挥击分支条件是 `m_input.isDown(MouseButton::Left)`
  （按住时每 0.25 s 一击），而 `xdotool click 1` 的 press+release 在**同一帧的
  pump() 内**成对到达 → `m_buttonDown` 回到 false → 下一帧 `isDown` = false →
  一次 30 连点 = 0 次挥击（表象：blocks=0、无击杀、无 WARN，极难定位）。
  对策：按住类动作用 `xdotool mousedown 1` … `sleep N` … `xdotool mouseup 1`
  （P29 的破坏探针就是 press-hold，本坑是把它上升成规则）；`wasPressed` 类
  动作（锁定/开关面板）用 click 没问题。
- **指针锁定后 `xdotool mousemove`（绝对坐标）会变成巨型相对 delta（P38）**：
  相对模式下 X 的指针虚拟位置不在窗口中心（实测屏幕 (0,0)），XWarpPointer 到
  (700,820) → 游戏收到 (700,820) 的相对位移 → yaw −1.75 rad、pitch 夹到
  −89°（+60px 的"微调"变成整面墙的转向）。对策：**锁定后绝不发绝对
  mousemove**——需要转相机就把它写进存档的 yaw/pitch（P29 的 grab-delta
  免疫法），运行中只发按键/鼠标按键。
- **伤害红纱在 0.5 s 衰减边界会让截图 A/B 差 ±1 红量子（P38）**：
  `kHurtFlashSeconds = 0.5`，红纱按**真实 dt** 衰减——最后一箭命中时刻距截图
  帧 ~0.5 s 时（射击节奏 0.75 s/发的场景里几乎必然），残留 alpha 跨运行差
  <1/255 → 全屏幕红通道 ±1（100% 像素、每通道 ≤1、红占 ≥90% 变化量）。
  对策：截图 A/B 用**红纱容差比较器**（字节一致 OR 该精确签名：全屏 + 每通道
  ≤2 + 红主导），**存档 A/B 才是权威证据**（P27 的 veil-removal diff 先例）。
  场景设计上让最后一击远离末帧（>0.5 s 余量）可彻底规避。
- **`--frames N` 数的是模拟子步不是渲染帧（P38）**：固定步长累加器——
  渲染帧率掉到 40fps（xdotool 交互/WM 干扰）时每渲染帧补 1.5 个子步 →
  `--frames 720` 的墙钟 = 18 s（不是 12 s），存档 `time` 字段按子步推进
  （720 子步 = 12 s 游戏时间，与墙钟无关）。对策：输入运行的时间预算按
  **子步时间**（frames/60）估游戏内时刻、按**墙钟**（frames/fps，fps 可能
  <60）估 xdotool 序列长度——两者都要留足；断言别用墙钟反推帧号。
- **A/B 参考构建与数据 schema 迁移：参考侧必须从自己的 worktree cwd 运行（P39）**：
  本阶段迁移了 `mob_spawning.json` 的 schema（字符串列表 → 对象条目），仓库
  assets 已是新格式——P38 参考构建若像以前一样从**仓库 cwd** 启动，会加载
  新格式数据被旧解析器的迁移防护直接拒绝（`'mobs[0]' must be a non-empty
  string`，init 失败、窗口秒退，表象 = "game window not found"）。迁移防护
  本身在正常工作（这正是它该干的），但 A/B 的参考侧必须 `cd` 到 worktree
  目录（那里是旧 schema 的 assets + 自己的 `data/saves/`）再启动。
- **游戏启动辅助函数里 `cd <gdir>` 会让相对二进制路径失效（P39）**：
  P39 批脚本给 `launch()` 加了 scratch-cwd 能力（加权场景在临时目录跑，
  assets 是副本 + 改过的 `mob_spawning.json`），`(cd $gdir && exec "$bin")`
  之下 `BIN=./build/bin/minecraft` 相对路径指向 scratch 目录 → "没有那个
  文件或目录"（表象是 "game window not found"，误导）。对策：批脚本里
  二进制恒用绝对路径（`BIN=$PWD/build/bin/minecraft`，脚本顶部 `cd` 仓库
  后取值）。
- **`EXPECT_EQ(a, ItemStack{x, y})` 花括号内逗号被宏参数拆分（P40）**：
  C 预处理器的宏实参拆分**不认识花括号**——`EXPECT_EQ(s.fuel,
  ItemStack {id, 1})` 被看成 3 个实参，`-Werror` 下直接编译失败
  （"宏 'EXPECT_EQ' 传递了 3 个参数，但只需要 2 个"）。对策：宏实参里
  的 brace-init 包一层圆括号，或经 helper/局部变量传入（P40 测试用
  `stk(id, n)` helper 统一处理）。
- **右键顺序 tryEat → launcher → interact：手持食物右键永远开不了面板
  （P40）**：输入 E2E 里"右键命中炉子/工作台打开面板"的步骤，若玩家选中格
  是食物，点击先被进食通道吞掉（`tryEat()` 在最前，满饥饿也是 no-op 吞
  点击）→ 面板不会开。对策：这类场景的选中格放非食物物品（P40 S4 选中格
  放 coal、待移物品放邻格），或先 Tab 换格。
- **存档 format 升格后旧构建读不了新档：A/B 参考改用上一阶段的截图
  （P40）**：P40 把存档升到 format 6（`furnace` 字段）——跨 format 的
  存档 A/B 不可能（P39 参考构建对 `fmt > kSaveFormat` 严格拒绝，
  与 P39 的 schema 迁移坑同族但方向相反：那次是数据文件，这次是存档
  本身）。对策：格式升格阶段的回归 A/B 用**上一阶段 E2E 留下的截图**
  作参考（场景不得含新内容——P40 的 360f 夜场景无炉子，P40 构建与 P39
  参考截图可比），×2 无输入运行字节一致自证确定性。
- **70° 是垂直 fov，水平由 aspect 推导（P41）**：`kFovDegrees` 70 = **垂直**
  半高角；像素断言的投影 `sx` 必须用 `tan(35°)×(W/H)`（1280×720 = 1.247，
  水平 fov ≈102°）、`sy` 用 `tan(35°)`。P41 S3 断言初版两轴都用 tan(35°)
  → x 向压缩 9/16 → 四个面的投影 bbox 全错（面看起来"小一圈"，且远/近
  深度跨度对不上——几何上不可能，是模型错的信号）。对策：投影模型先拿
  实际截图里可数的角点（顶面 4 角、W/R 分界线）交叉验证再写断言。
- **shader 光公式有 3% 环境光下限：±Z 面是 ×0.612 不是 ×0.6（P41）**：
  `basic.frag` = `base × (0.03 + 0.97 × max(sky×dayFactor, block) ×
  faceShade)`——顶面（1.0）恰好 ×1.0，±Z 面（0.6）= 0.03+0.97×0.6 =
  **0.612**、±X 面（0.8）= 0.806、底面（0.5）= 0.515。像素颜色集必须按
  0.612 推导（texel×0.612 四舍五入 ±1 容差），按 0.6 会差 2-4 个灰度值
  把整面判成"坏像素"。
- **场景 diff 像素断言用颜色分类，不用投影 bbox（P41）**：两个**不同纹理**
  的相邻块之间的渲染共享边，实测偏离分析投影最远 ~20px（确定性、跨运行
  字节稳定——s3a==s3b 自证，但成因未明，疑似网格化接缝）。bbox+颜色断言
  必然假阳性。对策：断言"每个 diff 像素 ∈ 某颜色类（确定性纹理色集 ×
  光公式）或 ≤20/通道的小光级偏移"，bbox 只打印参考；S4 同款（白面变红
  = 红色类，裂纹 = 暗色 max<100，准星 = 中心 20×20 白）。
- **存档块编辑回放会在光照边界翻转个别 texel（P41）**：S3 场景（save 放
  wool 块）vs 无块对照：远处 2 个草像素光级差 ±1（编辑回放的局部光照重算
  边界效应，确定性）。场景 diff 断言允许少量 light-shift 像素（≤20/通道、
  总数 ≤16），超出 = 真异常。
- **准星只在指针锁定时渲染 → 输入 vs 无输入对照截图必差 ~40px（P41）**：
  准星 = 屏幕中心的白色加号（臂 ~20px、厚 2px，颜色 ~ (233,221,222)/
  (252,250,250)）；输入运行锁定、对照未锁定 → diff 里必有它。对策：断言
  给中心 20×20 框内的近白像素开例外（MECHANICS 已文档化"UI 只在按键/
  锁定时渲染"），或对照也锁指针。
- **物理按键污染：pos 是移动漂移探测器（P41）**：一次 S4 输入运行玩家
  凭空走了 0.575 块（= 4.3 块/s × ~8 帧 ≈ 134ms 的 W 键——脚本只发
  xdotool click，稳定化步骤的 `xdotool key Super+ctrl+space` 也验证过不
  产生移动，判别实验 ×2 复跑 |dz|=0）= 用户物理误触键盘。对策：输入 E2E
  的存档断言**必须含 pos 精确值**（同 yaw/pitch 的漂移探测器）；pos 漂移
  先怀疑物理污染，重跑一次再下结论（截图会整体错位，看起来像"相机模型
  错了"——P41 就是先被这个带偏了一整轮投影分析）。
- **染料物品名是 `dye_<color>` 不是 `<color>_dye`（P41）**：s4 源存档槽位
  手滑写成 `red_dye` → init 报 `save slot #0 holds unknown item 'red_dye'`
  （`ItemRegistry::idForName` 精确查名字，注册日志里明明是 id=40）。
  手工构造存档里的物品/方块名一律从注册日志或数据文件抄，不要凭约定猜。
- **Renderer::draw 每次 draw 后解绑程序 → setUniform 必须先 bind（P42）**：
  `drawHeldHand` 里在两次 draw 之间直接 `setUniform1i` → 程序未绑定 =
  GL_INVALID_OPERATION (1282)；该**残留错误**被懒建网格上传的 `glGetError`
  捞走 → "GL error during mesh upload" **每帧重复**（手部网格永远建不成、
  手永远不渲染、无崩溃、无其他报错——纯静默）。模式同 `drawArrows`：先
  `m_basicShader.bind()` 再设 uniform（uniform 持久在程序对象上，后续 draw
  重新绑定不影响）。
- **不透明段 blending 关 → 带透明像素的纹理 quad 渲成不透明黑方（P42）**：
  图标 PNG 的透明像素是 (0,0,0,0)，不透明段 GL_BLEND 关闭时 alpha 被丢弃、
  rgb 原样写出 = 黑方块垫底。3D 手部（图标 quad / 玻璃类方块体）必须在
  draw 前后 `enableBlending()/disableBlending()`（标准 SRC_ALPHA 混合，深度
  写入保持开着即可）。
- **faceShade 是顶点着色器从模型旋转后的法线推导的（P42）**：MeshVertex
  **没有** faceShade 字段——`basic.vert` 对 `mat3(u_model) * a_normal` 分桶：
  +Y→1.0、−Y→0.5、|X|>0.5→0.8、其余→0.6。手部图标 quad 的几何 +Z 法线
  经手部自旋（Rx·Ry）后落入 0.6 桶 = 图标暗到 61%。技巧：顶点法线取
  **+Y**（所有自旋态下 n.y 仍 >0.5 → 恒 1.0 桶、恒亮）——法线在 shader 里
  无其他用途，此 hack 安全。箭/方块体不受影响（各面法线随面正确旋转）。
- **插墙箭的碰撞盒只"触及"卡住格的面、不重叠（P42）**：碰撞解算器把箭
  **精确 clamp 到面上**（盒最大坐标 = 面平面，如 x=12.0 而非 12.0001）→
  严格重叠判定（`>`/`<`）的"方块变更释放箭"检查**永远不触发**；但纯非严格
  "触任意面"又有假阳性（玩家往箭盒所在的空气格放块也会释放）。正确判定 =
  每轴盒区间与格闭区间相交（inAxis）且**至少一轴**盒从**外侧**触面
  （touchAxis：中心严格在面外）——`Game::setBlock` 内两个 lambda。
- **发射器选中的左键永不挖掘（P42）**：选中 bow 时左键 = 仅近战（"弓是
  武器不是工具"设计）——E2E"持弓破方块"场景**静默无效**（存档方块纹丝
  不动、无报错）。破块前先 Tab 换到空/材料槽。附带两坑：① 热栏**没有
  数字键绑定**（`Input::mapKey` 只有 WASD/Space/Shift/Ctrl/Alt/Tab/Esc/
  Q/E/F3/F5/R），换槽只能 Tab 循环；② `wasPressed` 是**每帧**语义——
  xdotool 连发多个 Tab 若落在同一帧 = 只循环一次，脚本里相邻 key 事件
  间隔必须 >1 帧（≥50ms 稳）。
- **3D 手部可能整个在 solid 块内（P42）**：手部偏移 = 相机前方 0.85 格——
  墙面前缘距玩家 0.5 格的近墙场景里手部世界坐标落在块内，被块面深度完全
  遮挡（**0 像素、无报错**）。场景设计：手要可见需前面 ~1 格以上无障碍；
  近墙场景的像素断言不要包含手部（S4 近墙 = 无手部断言，手部证据由 S3
  远墙场景承担）。
- **gtest EXPECT_EQ 不接受花括号初始化列表（P42）**：
  `EXPECT_EQ(x, Vec3{0.0f, 0.0f, 0.0f})` 编译失败（"EXPECT_EQ not declared
  in this scope"——宏无法对 braced-init 临时对象推导类型/比较）。用具名
  变量：`const Vec3 zero = {0,0,0}; EXPECT_EQ(x, zero);`。
- **纹理噪声是逐通道独立的——像素颜色分类器必须逐通道独立偏移（P42）**：
  程序化纹理（wool 等）的 ±12 噪声对 r/g/b **各自独立**掷。分类器若用同一
  偏移 n 扫三通道（`for n: (r+n, g+n, b+n)`）会漏掉大部分噪声像素
  （(140,138,135) 对 base (142,141,137) 不在集内）→ S4 204k 像素误判
  bad。正确 = 三通道独立 dr/dg/db（S3 分类器的 `fam()` 一开始就是对的，
  S4 的 `with_noise` 抄歪了）。
- **存档 pos 是 float32——EXACT 断言只许用二进制精确值（P43）**：
  存档存 f32，z=-0.3 读回是 -0.30000001192092896 → 场景 pos 的
  `== [x,y,z]` 精确断言**静默失败**。用二进制精确值（…0.25/0.5/1.0，
  k/2^m 形式）。定场景 pos 前先跑 60f 无输入探查读回存档 pos 验证
  EXACT（探查存档记得删，别留在 data/saves/）。
- **mob 生成环锚定玩家列——探查必须用场景的真实玩家 pos（P43）**：
  生成格选择 = f(world, anchor=玩家列 (floor(px),floor(pz)), 环半径)——
  用规范出生点（z=0.5，列 (22,0)）探查得到的牛位（z=-5.5）**不等于**
  场景玩家 z=-0.25（列 (22,-1)）时的真实牛位（z=-6.5，差一整格，恰好
  把 t 从 4.65 推到 5.65 出 kReach）。探查与场景的**玩家 pos 必须一致**
  （同列即可，z 在列内移动不改锚）。附带：牛出生列与玩家列的几何关系
  意味着「站桩够不着」是常态——要么把玩家放进同列更靠牛处（-0.25→-1.0
  把 5.65 拉回 4.9），要么等后面生成的更近牛（f240 牛 t=3.9，且
  targetMob 按最近命中自动跳过超距牛）。
- **帧率≈40fps 下固定 sleep 的单次输入会错过确定性窗口——用点击序列（P43）**：
  本机实测 ~40fps（wall ≈ 1.5× 游戏秒），"lock 后睡 1.15s 点一次右键"
  的落帧随加载时长漂移 ±0.3s+，牛的可挤窗口（出生→首 wander = 180 帧）
  说错就错过且**无报错**（点击落空 → 无 mob 无块 → placeBlock no-op）。
  修法 = 0.35s 间隔的 N 次右键**序列**（25–60fps 全覆盖），前提是设计
  两个无害性：窗口前的点击必须安全空操作（本场景 = 无 mob 在射程 +
  射线无块 + 材料物品不可放置），动作后的点击必须无害（饮用通道
  1 帧 start+cancel，不消耗）。牛出生到首 wander 之间**完全静止**
  （wander_interval 3.0s = 180 帧）= 确定性瞄准窗口，挤/打被动 mob 的
  场景都按这个窗口设计。
- **isFull = "count-1 可加"探针（P43）**：`Inventory::isFull(item, items)`
  = 无空格 **且** 无未满同物堆（`empty() || (same && count<max)` 即
  false）——正是 "add(item,1) 必然成功" 的精确判据（同物可并入堆也算
  有空间）。饮用开始（返还容器放得下才许喝）与挤奶（满背包不产出但
  吞点击）都用它；不要自己写"有空格"版（漏掉并入堆的情况，奶桶
  max_stack 1 时无差、其他容器会错）。
- **跨构建 A/B 的 ref 不必然要自己 cwd（P43，P39 规则的边界）**：P42
  ref 二进制直接加载 P43 资产目录可正常 init——旧解析器对**未知** item
  字段（drink_time/returns_item）= 警告忽略、未知 mob property 键
  （milk_with/milk_gives）= 静默忽略、新配方（bucket）的输出物品
  "bucket" 因 bucket.json 被旧解析器当普通 material 注册而通过交叉
  校验。P39 的 own-cwd 硬错误只发生在**旧代码无法注册/解析**的新数据
  （如配方输出一个旧注册表结构装不下的物品）。跨构建 ref 跑批前先想
  一遍新数据文件在旧解析器下的命运，别默认要 worktree。
- **……但严格枚举值的新数据 = ref 必须 own worktree（P45，P43 条目的反面
  案例）**：P45 给方块模型加了 `shape: "cross"`——旧（P44）parseModel 把
  未知 shape 值当**硬错误**（`'model.shape' is unsupported`，启动失败），
  不像未知 JSON 字段那样容忍。ref 从新资产 cwd 启动 → init 失败 →
  "game window not found"。判别：新数据触发的是旧代码的"未知键容忍"
  路径还是"枚举/结构严格校验"路径，后者 ref 必须从自己 worktree 跑
  （`shot <gdir=worktree> ...`）。
- **红心检查的语义按运行类型分（P45）**：HUD（心/饥饿/hotbar/面板）只在
  **指针锁定**时渲染——无输入运行 0 红像素（>50 = 污染废拍）；**输入运行**
  锁指针后 HUD 才出现 = 满血 10 颗红心 670 红像素（>50 = 输入生效的
  **正向**信号）。对照（同存档无输入）截图**完全没有 HUD**——与输入运行
  做像素差时 HUD 区差异是预期的，别当回归。
- **format 7 场景存档必须带 `furnaces` 键（即使空数组）（P45）**：fmt 7
  解析严格——缺 `furnaces` = 启动报错（"save format 7 requires a
  'furnaces' array"）。从旧格式源（p27far = fmt 5）派生场景存档时手动
  升 format 7 的要同时补 `"furnaces": []`（场景无炉子块 = 空数组合法，
  双向交叉校验通过）。
- **纹理/图标工具的 salt 不能按调色板排序索引算（P45）**：
  `salt = 100 + sorted(PALETTE).index(name)` 在调色板**加项**时让既有
  颜色的 salt 移位 → 重新生成后**既有已提交 PNG 静默改变**（P45 首次
  重生成改了 7 个 wool + 8 个 dye 图标；基线门没抓到——出生点视野无
  wool/dye = 结构性免疫掩盖了回归；但旧存档里的 wool 块外观会变）。
  修复 = 显式 per-name salt 表（既有色保持原值、新色取后续空位）+
  `assert set(salt) == set(palette)`。教训：改数据驱动生成器后
  `git status` 必须**零既有文件修改**才算干净。
- **WM 会在运行中途重新 tile 窗口 → 前缓冲冻结 → 截图 = 过期帧（P46）**：
  launch 时的 stabilize 只保证**启动时** 1280x720；WM 可在运行后半程再次
  tiling/unmap（实测 1272x1552），unmapped 期间 SDL swap 停止 present →
  前缓冲停在最后一次 present 的帧。`--screenshot`（末次 swap 后
  glReadPixels）读到**过期帧**，两类症状：(a) 世界缺最后几秒整合的列
  （远景树冠剪影 27 px 条带，或更宽 ~500 px 的过期区）；(b) 过期帧上仍
  活跃的受击红纱（0.5 s）= **全帧均匀红色 tint**（实测 α≈0.22 全帧一致——
  真红纱不可能活到 6.1 s 帧窗的末帧：毒 tick 在 5.0 s，flash 5.5 s 结束，
  与截图 α 反解的 damage 时刻 5.88 s 矛盾 → 截图不是第 240 帧）。存档
  在循环结束后按真实状态写出，**不受影响 = 确定性权威**。判别/防护：
  **天空锚点像素**（正午场景 (100,100) = 固定 (120,181,242)——time 是
  固定 1/60 步长，颜色确定；带红纱的过期帧会偏 ~+30 R）+ 世界 diff
  上界（≤600 px，报告不致命）+ 存档断言（权威）。**流式场景的跨运行
  截图不要断言世界零 diff**（S5 对首拍即栽；S2 360f 未中招 = 远列早已
  整合 + 红纱早已结束，更稳但那是运气不是保证）。
- **带活跃效果的场景做存档 ×2 字节比对必须先剥 real-dt `remaining`
  （P46）**：`player.effects[*].remaining` 是真实 dt 倒计时（生存计时器
  类，硬规则 8），跨运行毫秒级漂移——同场景两次无输入运行直接 `cmp`
  **必然 FAIL**（本批 S3 首拍即栽）。权威 = 剥 `remaining` 后的深度比对
  （health/pos/yaw/pitch/time/inventory 必须逐位一致，实测剥离后全同）；
  `remaining` 本身单独做 range 断言。
- **受击中断饮用通道 → 中毒玩家"立刻喝"必被打断（P46）**：P28 规则
  `health < eatStartHealth` = **任何伤害**取消通道。带 poison（2.5 s
  tick）的场景若在 ~0.9 s 帧时开喝（1.6 s 通道），完成时刻 2.5 s 与首
  毒 tick 正面相撞 → 确定性取消（3/3 断：milk 未消耗、slot 不变——
  **机制**行为，MC 一致，非环境故障）。正确场景设计 = 在 tick 间隙喝：
  mousedown 于帧时 ~2.55..3.35 s（完成 4.15..4.95，在 5.0 s 第二 tick
  前），0.8 s 窗口；且最终 health 确定 = 20（tick1 20→19 在喝之前、
  regen 4.5 s 19→20、通道期无伤害）。**固定 sleep 打不中这个窗口**：
  launch→帧 0 偏移有 ~0.6 s 不确定度（窗口发现 + stabilize 耗时）——
  把 mousedown **锚定 `Entering main loop` 日志时间戳**（logger 逐行
  flush，可即时读；epoch 化后 sleep 到 t+2.9 s）。重试成功门 =
  `slot0 == bucket`（未完成 = 输入没落地/被打断 → 重试）。
- **Read 工具行号前缀含一个空格 → 文档缩进比看起来少 1（P46）**：
  Read 输出 `294: - **游泳**` 的内容是 `- **游泳**`（**无前导空格**），
  不是 ` - ...`。照"看到的"缩进写 edit oldString 会连续
  "Could not find oldString"（本会话连栽 4 次）；单行锚点（不含行首
  空白）必成。多行锚点先 `python3 -c "print(repr(段))"` 看精确字节再
  写。
- **流体桶 E2E 视线：源的扩散水永远挡在远处玩家与源之间（P47）**：
  搁置（resting）源格**每 tick** 向四向水平邻格补 level 7 水——玩家在
  3 格外瞄源时，桶目标射线（首个「非 air 方块或带流体」的格）先命中
  源头自己铺出来的 L7 水膜（≠8 → 不舀，**MC 正确行为**，不是 bug）。
  几何上也无法"从上方绕"：眼在源顶面上方仅 0.62 格（y46.62 vs y45
  顶面 y46），直线过不了 1 格宽的膜。正确场景 = **玩家站源旁 1 格**
  （脚下放石头平台格），pitch ≈ −0.6 斜视：DDA 先穿源的**上方格**
  （air）再降进源顶面——首个流体格 = L8 源。独立诊断程序
  （worldgen + 场景编辑 + raycastFluid 直跑）是这类瞄准问题的最快
  裁决工具（本坑用它在 1 分钟内证伪了两个错误几何假设）。
- **存档场景构造：同一格不能同时有 block edit + fluid edit（P47）**：
  `applyWorldEdits` 校验「a block displaces fluid」——同格
  `[x,y,z,"air"]` + `[x,y,z,8]` = **加载失败**（整档拒绝，干净报错）。
  E2E 场景要"保证目标格为 air + 放流体"时，air edit 不能覆盖流体格
  本身（依赖该格 terrain 本就 air；若有固体 = fluid 加载报
  "cell occupied" = 可检测）。路径格（射线穿过的其他格）照常用 air
  edit 强制。
- **天空锚点像素在俯视 + streaming 场景不稳定 → 用计数不用单点（P47）**：
  俯视场景（pitch ≤ −0.5）顶边像素在帧 60 是天空、帧 240 变成**远山**
  （远列在帧 60..240 之间才整合进来）——单像素锚点首拍即 FAIL（本批
  栽 2 次）。改用**顶 20 行条带内天空色 px 计数**（阈值 ≥300：实测
  最终帧 S2=518 / S3=5473，streaming 只会收缩不会染红；WM 过期红纱帧
  把天空染 ~+30 R → 计数塌到 ~0）——对 streaming 收缩稳健、对过期帧
  敏感。
- **chunk 是 16×16×16，不是 16×16×128——世界高度沿 ChunkPos.y 堆叠（P48）**：
  遍历 chunk 本地坐标时三个轴都在 [0, kChunkSize=16)；世界高度 128 =
  8 层 chunk（`ChunkPos.y` × 16）。误以为"一个 chunk 高 128"在单 chunk
  内遍历 `ly ∈ [0,128)` → `Chunk::get` 的 `assert(inBounds)` 在
  **Release（NDEBUG）下被裁掉** → 越界读 `m_blocks` → **segfault**
  （gdb 定位到遍历函数，栈无 assert）。正确范式见 `FluidSim::tick` 的
  origin 换算：`baseY = cp.y * kChunkSize`，本地 `ly ∈ [0,16)`，
  世界 y = `baseY + ly`。任何"全 chunk 扫描"（rescan/统计/光照）都照此。
- **配方同一物品重复项被拒绝——合并成一条 count（P48）**：
  `RecipeRegistry` 严格校验「duplicate ingredient item」——3 个小麦
  要写 `{"item":"wheat","count":3}`（一条），不是 3 条 `count:1`。
  shapeless 配方的 `ingredients` 是"物品→需求数"的 bag，同物品必须
  合并。单测用同一条 parse 路径能提前抓到（本坑正是单测先红）。
- **测试里拼 JSON 字符串：闭合引号/逗号最易漏，用显式拼接别用 R"()（P48）**：
  用 `R"({"id":"wheat_)" + std::to_string(n) + ...` 拼 JSON，数字后
  的闭合引号极易漏（`"wheat_1,` 而非 `"wheat_1",`）→ parse 在**奇怪
  列号**报 "expected ':' after object key"（逐字符数才定位到）。
  参数化的测试 def 用**显式 `+` 拼接**（每段引号/逗号都看得见）比
  跨多个 `R"()"` 段安全得多；写完先用 python `json.loads` 验一遍拼接
  结果再进 C++（1 分钟省 10 分钟）。
- **离线预计算确定性 roll：Python 任意精度 int ≠ C++ uint64 环绕（P49）**：
  用 Python 复刻 C++ 哈希链（hash3/splitmix64）预计算"哪些格会掉出"时，
  链中每一步 `h = (h + CONST) ^ X` 都必须 `& 0xFFFFFFFFFFFFFFFF`——
  Python int 不环绕，`h + CONST` 的溢出位留在 h 里，**污染后续 XOR 与
  splitmix64**（溢出位会经 `x >> 30` 混回低 64 位）。症状极隐蔽：
  部分格与 C++ 一致（该格链恰好没溢出）、部分不一致（本批 5 格对 3
  格），且**预计算"通过"的格在游戏里 fail**。对策：写一个几十行的小
  C++ 探针（含 `block/chance_drop.hpp` 调 `rollChanceDrop` 打印）与
  Python 全区域对拍，逐格一致才可信。
- **输入运行存档含真实 dt 字段——字节一致不可能，用归一化比较（P49）**：
  `entities[].age`（掉落物寿命计时）与 `player.effects[].remaining`
  （效果倒计时）都是**真实 dt**（生存计时器，协议明列）→ 同一场景两
  次输入运行的这两个字段必不同（实测差 ~3 ms）。输入运行的确定性
  权威 = **归一化后逐字段一致**：剥掉这两个字段，其余（blocks /
  inventory / 实体 item·count·pos / player / time）必须全等。无输入
  运行不受影响（无实体/效果时本就没有这些字段）仍走字节 cmp。
- **E2E 瞄准与掉落断言的三个物理事实（P49）**：① 破坏/交互射线
  `kReach = 5`——离线算好的目标格 t 必须 < 5（本批首版目标 t=5.9
  直接超距，射线打不到）；② 掉落物在**方块中心零速生成**后会受重力
  **落到下方表面**（悬浮树叶 y=46 的掉落在平台顶面 y=45 静止）——
  断言落位列区间（`44.9 < y < 46.6` 容生成点与落点），别断言生成点
  精确坐标；③ 同点双掉落物有**确定性 ±0.125 x 散布**（22.625 /
  22.375）——x 用 ±0.3 盒断言，精确值交给 ×2 归一化比较。
- **世界探针只生成 cy=0 的 chunk = 只看到 y 0..15（P50）**：
  `kWorldHeight = 128`，一列 = 8 个 chunk（cy 0..7）。探针/测试只
  `getOrCreateChunk({cx,0,cz})` 时，y≥16 的 `getBlock` 全部读 air
  （未生成 chunk = air），而 `surfaceHeight()`（纯噪声函数，不读
  chunk）仍报真实表面（如 44）——**两者矛盾**是症状（本批一度以为
  seed 9 演示区是 y=15 的海底，实际平台顶在 y=44；P49 场景的"平台"
  全部来自生成，与 p27far 存档的 0 编辑无关）。对策：探针对目标列
  生成整列（`for (cy : cy*16 < kWorldHeight)`）再断言。
- **生长帧计数器是 0-based：update 在 ++m_frameCount 之前（P50）**：
  主循环顺序 = `update(dt)`（含生长 roll，读 `m_frameCount`）→
  `swapBuffers` → `++m_frameCount`。所以 `--frames N` 的 N 帧里生长
  roll 看到的是 frame **0..N-1**（不是 1..N）。离线预计算通过帧必须
  用 0..N-1（差一帧在 mod 360/720 级别可能正好错过/多出一个通过点）。
- **FluidSim 水扩散到曼哈顿距离 7（level 1），大于湿润半径 4（P50）**：
  水从源沿 4 邻域 BFS 扩散（level = 8−MD，MD≤7 都有水）。E2E 的
  "干耕地"对照若只按切比雪夫半径 4 放远水，会被 MD 5..7 的远水膜
  污染（level 1-3 也算"任一流体"→ 判湿）。两个稳健对策：① 水离土壤
  MD ≥ 8；② **换 y 层**——水只水平扩散不爬升，土壤格抬高一层
  （如土壤 y=46、水全在 y=45）= 结构性干燥，免一切距离计算（P50 S2
  的 B 作物即用此法）。
- **C++ raw string `R"(...)"` 终止符 `)"`——JSON 尾部极易写错（P51）**：
  测试/探针里用 raw string 拼 JSON 块定义，内容以 `}` 结尾时，
  `...}}` + 终止符 `)"` 的字节序列极易写成 `}}})"`（多一个 `}` 被吞进
  字符串，初始化列表少一个元素）或把 `;` 落进列表内部（`...")";`），
  或 properties 未闭合（`"water_reaction":{...},` 后直接 `"model"` →
  字段全落顶层）。症状各异：`expected ',' or '}' in object`、
  `expected '}' before ';'`、**`unknown field 'X' ignored`**（字段落
  顶层 = properties 提前闭合的信号）。对策：拼接后用 Python
  逐字节重建 + `json.loads` 预验（P48"测试拼 JSON 串闭合引号最易漏"
  同款纪律的 raw-string 变体；本轮连踩 3 次）。
- **两种流体同 tick 正面抢同一空格 = apply 顺序决定（P51）**：
  FluidSim tick = 反应 pass → 水模拟（apply）→ 熔岩模拟（apply）；
  同一 tick 两种流体都提议同一 air 格时，水先落、熔岩后落 = **熔岩
  胜出**（setFluid 正 level 替换异种）。稳态由反应 pass 收敛（无
  6-邻接水-熔岩对，边界 = 石头/黑曜石墙），但瞬态会出现"熔岩抢水"。
  测试断言写**稳态不变量**（无 6-邻接对、墙位置），不要断言正面
  争夺的瞬态赢家（apply 顺序是实现细节）。
- **`git checkout -- <file>` 会连本阶段未提交的工作一起还原（P51）**：
  修 atlas JSON 格式问题（json.dump 重排 indent 污染 diff）时
  `git checkout --` 把文件还原到 HEAD（P50），本阶段未提交的
  lava/obsidian tile 随之丢失（atlas 回到 50 tile）。对策：还原前
  `git diff` 确认文件里哪些改动是未提交的；数据文件 JSON 修复用
  **定点文本替换**（保留原 indent，diff 最小），不要整文件 json.dump。
- **E2E 桶射线被同高墙挡 + 倒射线只认方块受击面（P51）**：1 格高墙
  （y=46，顶 y=47）挡 y=46 水平射线——眼高 47.62 的射线要么在
  x=25（墙列）处还在 y≥47（穿墙顶上空 = 没打到 y=46 的流体格），
  要么更早撞进墙格。解法 = 让射线**在墙列之前**下穿 y=47（pitch
  −0.38：x=25 处 y 47.2【air】→ x 24.95 处 y=47 → 命中 (24,46,−18)
  熔岩源）。**倒**（pour）走 raycastBlock：目标是"首个非 air 方块的
  受击面邻格"——孤立 air 格（四周无块）无法被倒中，须在目标旁放
  支撑块瞄其面（S3b 在 (27,46,−18) 旁放 (28,46,−18) 瞄西面）。
  舀（collect）走 raycastFluid：首个「非 air 方块或带流体」格，
  必须命中 level 8 源（扩散膜 ≠8 不舀，P47 同款）。
- **瞄方块面中心用错半格 = 射线擦着角边漏过（P51，浪费一整轮 E2E
  排查）**：方块格 (x,y,z) 占体积 [x,x+1)×[y,y+1)×[z,z+1)，其**西面
  面心 = (x, y+0.5, z+0.5)**。S3b 瞄准算成 (28, 46.5, −18.5)（把
  z=−18 当中心，实为格西-北竖直棱的中点）→ 射线恰好擦过石头西北角、
  从石头**北侧 0.5 格**穿过，reach 5.0 内不命中任何块 → pour 静默
  fall through（无 "poured" 日志、存档不变、渲染一切正常）。排查时
  渲染/相机/世界全对（截图轮廓反解 fit 误差仅 5px/点），最后用
  **真实 raycastBlock 探针**（p51ray：真实 seed-9 地形 + 场景块 +
  候选 yaw/pitch → 打印 hit/normal/pour cell）一锤定音。对策：场景
  瞄准**先探针验证 hit 与受击面**再跑输入 E2E（输入跑一次 ≈1 分钟
  ×3 重试 + 全套断言，错瞄 0.5 格 = 全部白跑）；面中心 z 分量
  = z+0.5，不是 z−0.5（同 S2 池中心 z=−17.5 非 −18.5 同款半格纪律）。
- **给"二选一"的两个字段加 init 校验：`continue` 条件必须覆盖两者皆空
  （P52）**：P49 的 chance_drop 校验循环用 `if (chanceDropItem.empty())
  continue;` 跳过无单条目的块。P52 加 `chance_drops` 列表后，若不改这个
  `continue`，"只有列表、无单条目"的块（= grass 本体）会被**整体跳过** →
  列表的 init 校验（物品存在 + max_stack）**静默失效**（解析期不查物品
  存在，只有 init 查）。对策：`continue` 条件改为"两者皆空"，且每个字段
  的校验各自独立成块（`if (!单条目.empty()) {...}` + `for (列表条目) {...}`），
  别共用一个入口。通用化：任何"同义/互斥多字段"的校验，跳过条件要覆盖
  **所有**字段的空判定。
- **E2E 输入跑里放下的作物会被生长引擎继续推进 = 断言"放下的块"失败
  （P53）**：放置发生在点击帧（= 2.9s × 实测 fps；fps 跑间漂移
  32..45——P52 场景跑 31.5fps、P53 45.3fps），240f 运行剩余 ~110 帧里
  生长引擎（P50）可能 roll 过一阶（干耕地模数 720 下 ~15%）→ 存档里是
  wheat_2 不是 wheat_1（×2 字节一致 = 确定性本身没坏，断言没算到）。
  对策 = **选位预计算**：C++ 探针调**真实 `cropGrowthSteps`** 扫候选
  场景位置（P53：z=−3/−4 各 1 个通过帧且与点击帧赛跑 = fps 敏感弃；
  z=−2/−1 在 [0,240) 零通过帧 → 选 z=−2 = 无论点击落哪帧作物都保持
  初阶，fps 无关断言）；或预计算通过帧 + 断言精确阶段。相关坑：纯探针
  世界（generateChunk 直建）调 `cropGrowthSteps` 前必须
  **`Lighting::propagateAll`**——生成世界无光照数据 = 光 0 →
  `growth_light` 门全假挡，roll 结果全错（P53 首版探针踩过：0 通过帧
  是假象，补 propagateAll 后 z=−3 现出 1 个）。
- **存档不能在同一格同时有 block 编辑与 fluid 编辑（P54）**：parseSave
  硬拒（"save has a block edit and a fluid edit at the same cell (a block
  displaces fluid)"，rc=1）。P54 首版场景"挖坑 + 灌熔岩"= 4 个 air 块
  编辑 + 4 个 fluid 编辑同格 → 启动即拒。对策 = **用天然空穴当池**：
  地形探针（p54terr 范式：逐列打印 top y + 格内 fluid）找"该层天然 air
  且同 y 四邻封闭"的格当池位，缺口邻格用**方块编辑放墙**（天然 air 格
  放石头 = 合法 block 编辑，与池格 fluid 编辑不冲突）。通用化：存档
  场景里"挖 + 填"不可一步完成，流体场景永远从天然流体层/天然空穴出发。
- **E2E 隔离控制参数必须盖住"整次运行"而非"机制窗口"（P54）**：S3 游泳
  场景用 `lava_damage_interval_seconds: 30` 隔离"移动不受灼烧影响"，但
  900f 场景跑实测 **26 fps = 34.3 s**（同批 S2 是 62 fps = 9.7 s——场景
  跑 fps 漂移 26..62 且与帧数无关）→ 玩家浸没 34 s > 30 s → 灼烧 tick
  照样打 1 次，health 断言 20 得 16（游泳断言全对 = 机制没错，是控制
  参数选短了）。对策 = 隔离用的时长参数取**最长可能运行时长**的数倍
  （300 s ≫ 900f @ 10fps = 90 s）；判断"跑多久"用日志首末时间戳实算，
  别用别的场景的 fps 外推。
- **场景无输入跑的截图 ×2 字节一致不是有效断言（P54）**：玩家置于
  场景、水平视野（pitch 0）时，视野里的远列 pop-in 完成时刻**与墙钟
  相关**——P14 已知行为"1 帧暗 pop-in"的帧锁定是指"该列集成后 1 帧"，
  但**哪个列在第 N 帧前集成完**取决于 worker 墙钟吞吐（光照 job 每帧
  至多 1 个、流送 job 在 post→flush 窗口按墙钟跑）：P54 的 S4 水塘场景
  240f 两跑（7.50 s vs 7.51 s，差 10 ms）让地平线一棵树的树冠碎片
  （跨 chunk 边界的 20×10 px，世界坐标 ≈(25,48,−16) = 第 2 环流送列
  (1,0,−2) 的一部分）在终帧有无翻转——**存档 ×2 字节一致**（世界状态
  全同）但截图 ×2 不同。base 门（S1，fresh 世界 60f）与**俯角场景**
  （P47–P53 放置场景都瞄向下方的土壤/目标，视野无地平线内容）不受
  影响。对策 = 场景跑（无论有无输入）**确定性权威 = 存档 ×2 字节一致**，
  截图只做 notblack + skygate 健全性（PROTOCOL.md 已加场景例外条款）；
  若确需场景截图 ×2，把 pitch 压到视野不含地平线流送边界列（≈ −0.8
  以内，视野顶缘 = pitch + 半 FOV 仍低于水平线）。

- **zsh 未引用变量不做词切分（P55）**：`FILES="$a.json $b.json"; for f in $FILES`
  在 zsh 里只循环**一次**（整个字符串当单个词）——P55 首版 E2E 的
  scene_check 把两个存档路径拼进一个字符串遍历，python 收到 "a.json b.json"
  当一个文件名 → FileNotFoundError。对策 = 用数组 `FILES=("$a" "$b")` +
  `for f in "${FILES[@]}"`（或 `${=FILES}` 强制切分）。bash 脚本没有
  这个坑，但本项目 E2E 脚本全是 zsh。
- **E2E 脚本必须显式 `export DISPLAY=:0`（P55）**：代理会话的 shell 不
  继承桌面会话的 DISPLAY——P55 首版批跑全部 run "SDL_Init(VIDEO) failed:
  No available video device"（窗口搜索 10 s 超时，日志在**游戏自己的**
  log 里而不是批跑 log，批跑只看到 "game window not found"）。P54/P52
  的脚本在当时的会话里恰好有 DISPLAY 才没暴露。对策 = 每个 E2E 脚本头
  `export DISPLAY=:0`；排查"窗口未找到"先看 `$T/<tag>.log`（游戏日志）
  的 ERROR 行。
- **同物品掉落会合并——断言掉落用"实体数 × count"（P55）**：掉落实体
  同物品且底中心距 < kMergeDistance（0.75）时每帧合并进先到的实体
  （count 相加，受 max_stack 限）。`lapis_ore` 破坏 = 主掉落 count 2 +
  roll 条目 count 1 **同物品同位置**（方块中心零速度生成）→ 合并成
  **1 个实体 count 3**，不是 3 个实体。E2E/单测断言掉落数量必须按
  "实体 count 之和"算；不同物品不合并（leaves×1 + stick×1 = 2 实体）。
- **读档：health 必须在吸收层之后设置，且读档不重复提升（P56）**：
  吸收激活时存档 health 可高于 base max（24 = 20 + 4 层）。若
  `applySave` 先 setHealth（此时层未应用，上限 20）→ 24 被钳成 20，
  层白读。正确顺序 = 4d 应用效果 → 4e `syncAbsorption(false)` →
  setHealth（对**有效**上限钳制）→ m_prevHealth。`healOnIncrease =
  false` 是读档专用模式——存档 health 已含层，若用 true 会在 24 上再
  抬 4 = 28（钳到 24 看似无害，但 22 会被抬到 24 = 读档白送血）。
  每个层变化点（进食/mob 命中/到期/牛奶/死亡/读档）都要 sync，漏一处
  = "bonus ≠ Σ层" 静默漂移。
- **saturation 是真实 dt 字段——场景存档比对必须剥（P56）**：隐藏饱度
  缓冲按 `hunger_decay_seconds`（36 s）**真实 dt** 衰减 tick 扣减
  （先扣饱度再扣饥饿）→ 同一场景两次跑（fps 不同 = 墙钟不同）跨的
  tick 数可能不同 → 存档 saturation 不同 → 字节 ×2 假失败。判定标准
  = 该字段是否随**真实 dt** 累积（实体 age、效果 remaining、saturation
  是；health/hunger 在"无 tick 跨过"窗口内稳定，但依赖场景设计）。
  P56 的 savenorm 剥 {age, remaining, saturation}；场景设计若能让
  终态无真实 dt 字段（如 P56 S4/S5 层在跑内到期 = 效果清空），直接用
  字节 ×2（更强）。
- **输入锚定时刻必须早于最早可能死亡时刻（P57）**：P57 S5 首版：W 在
  t_ml+2.9 s 按下，但玩家从 y47 落进熔岩池后以 9 HP/s（P54 直伤 4 +
  火 5）满血 20 HP ≈ 2.5 s 已死——**输入前玩家已死**，W 零效果，终态
  = 死亡处（池底中央 (22.5,45,−4.5)）而非设计的贴墙终态。存档仍字节
  ×2（确定性的死），但位置断言失败才暴露。规则 = 输入驱动死亡场景先
  算最早死亡时刻（伤害速率 × 血量 + 入熔岩时刻），输入锚定必须比它早
  且留裕量；"玩家会在输入前死掉"是静默的（无报错，只有终态位置不对）。
- **输入场景的确定性终态 = 持续顶着的几何钳制（P57）**：P54 S3 / P57
  S5 的可复现终态 = W 按住到结束、玩家**持续顶着墙**（rim 石）：rest
  位置由几何唯一决定（墙脸 − 半宽 0.3，×2 字节一致，P54 已验证）。
  反例（P57 S5 首版设计）：弹出熔岩→落到某处→停——落点时刻是真实 dt
  → rest 位置 = ±1 帧 × 速度 → 两跑不同 → 字节 ×2 必假失败。设计输入
  时让终态落在"持续推力 vs 静态边界"（墙/池底+墙）或"身体完全冻结"
  （死亡冻结位）上；未知地形（rim 外的自然墙）永远不可用（位置不可
  预知，断言写不出）。
- **到期帧不 tick（P46 语义，P57 首次算错）**：`StatusEffects::update`
  先 `remaining -= dt`，**只在 remaining > 0 时**推进 tick 累加器并结算
  （"an expiry this frame does not fire a tick"）→ 时长 D、interval I
  的效果 tick 数 = I 的**严格小于** D 的倍数个数；D 恰为 I 整数倍时 =
  D/I − 1 次，不是 D/I。fire 3.0 s / 0.2 s = **14** 次（health 20→6），
  不是 15 次（→5）。E2E/单测算 tick 数一律按此规则（P57 S4 首版按 15
  写断言，跑出来 health 6 才对）。
- **流体里 Space = 上浮、W = 水平漂移；松手即停（P57）**：水/熔岩游泳
  （`Player::integrate`）：`Space` 每子步把 vy **设为** swimUpSpeed（水
  4.5 / 熔岩 1.5）= 上浮；W/S/A/D = 水平（速度**瞬时设为** wish ×
  swimSpeed，熔岩 1.9）；**无 wish 时水平速度每 tick 清零**（含流体
  内）→ 松开 W 玩家立即停止漂移。空中 Space 才是跳（onGround）。
  设计熔岩池轨迹时"按住 W 上浮"是错的——P54 S3 的贴墙终态 = W 沿池底
  水平漂移顶墙（不是上浮）；要离池必须 Space 上浮到脚 y46（出流体，
  严格不等式）再走。
- **float tick 相位漂移 → 整数帧格（P58）**：Mob 燃烧首版用 double 累加
  0.2 s tick（P46 玩家效果同款）：float 0.2f 累加 + 1/60 double 步进
  实测**相位 off-by-one**（tick 帧跨边界漂移，致死帧/到期帧算不准，
  单测里暴露）。修正 = **int 帧计数**：`armFire` 算
  `m_fireIntervalFrames = llround(interval*60)`，tick k **精确**落在第
  12k 个燃烧步；remaining 仍用 double（到期判定），但 tick 只由帧计数
  推进。教训：凡是"tick 时刻必须是整帧的整数倍"的机制（init 交叉校验
  也依赖它），帧计数用 int、时长用 double 分开存，别用一个 double
  累加器同时扛两职。
- **1e-9 容差拒自带 fire（P58）**：init 整帧校验首版容差 1e-9：
  Effect::tickInterval 是 **float**，0.2f×60 = 12.00000018… → 自带的
  fire.json（0.2 s）也被拒，游戏启动失败。教训：float 字段换算成帧的
  容差按 float 误差量级取（1e-4 帧），不按"严格"取；容差职责 =
  吸收 float 误差 + 拒绝真离格值（0.33 s = 19.8 帧，偏 0.2 帧，1e-4
  照样拒）。
- **熔岩光门挡 hostile 生成（P58）**：熔岩方块光 15 **level 无关**
  （fluidEmission 取参考块的 light_emission，L1 薄膜也 15）+ hostile
  `spawn_max_light` 0 → 任何熔岩格 ~15 块内 hostile 永不生成（ring
  4-6 全被光门挡）。后果：无输入运行 hostile 永远进不了熔岩（池边连
  生成都不能，且不寻路）——E2E 的 mob 点燃场景只能靠**被动 mob 游荡
  入**；反向地，同一光门也保护了场景里的玩家（ring 内 hostile 不能
  生成 = 零攻击）。S2ctl 水膜对照（水光 0）= hostile 生成杀玩家 +
  零点燃，正好隔离出熔岩。
- **自然游走不可信横穿小目标 + 0.9 AABB 入不了 1 宽缝（P58）**：E2E
  首版 = 2×1 熔岩池等被动 mob 游荡掉入：3600f 零横穿（随机游走 +
  cap 4：rim 出生的 mob 游走后不再回来，cap 满后 rim 空了）；扩到
  5 格池仍零横穿（cow/chicken 就站在池边 rim 上 40 s 也不下水——游走
  朝向均匀随机，不保证朝向池）。即便真踏出边缘：0.9 宽 AABB 跨
  "solid + 熔岩"两格时**总有 solid 一半支撑** → 永不坠落；被 solid
  夹住的 1 格宽口袋（如 (21,−5)：北 (21,−6) solid、南 (21,−4)
  solid）结构上**不可坠**。终解 = **地面 level-1 膜**：无需下坠，
  走过即燃（AABB y [46,47.8] × 膜 [46,47) 重叠），目标 3×3、mob
  生成在膜边 → 2400f 内 10 只烧死。教训：E2E 要"mob 进入危险区"
  必须设计成**几何必然**（地面膜/独木桥），不能赌随机游走。
- **存档禁同格 block+fluid 编辑（P58）**：`Game::loadSave` 校验：
  同一格既有 block edit 又有 fluid edit = 启动失败（"a block
  displaces fluid"，语义不可表示）→ "挖洞 + 注水/熔岩"无法在单个
  存档表达；两跑存档也不行（第二份存档仍含同格双编辑，parse 即拒）。
  场景替代：用**天然腔体**（seed-9 y45 腔体：4 格 top 44）或
  **地面膜**（y46 air 格，纯 fluid 编辑，零 block 编辑）。
- **level-1 膜 = 稳定地面流体场景（P58）**：支撑在块上的流体格 L>1
  才扩散（**L1 = 终止膜，零扩散**）→ 3×3 L1 膜铺地面从第一 tick 起
  惰性，无需 rim（对比 L8 池必须石 rim 封邻格防扩散）。副作用：膜格
  **下方是坑**（air）时首 tick 灌入坑内（传送带下落语义）→ 最终流体
  状态 = 膜残留 + 坑内 L1，断言必须写**终态**（P58 E2E S2 首版按
  场景态写：以为 3 个坑，实际 4 个——(23,−4) 也是天然坑 top 44）。
- **物品 rest 位置确定性（零初速 + 位置式分离）（P58）**：掉落物品
  以**零初速**在 mob 死亡位生成（破坏块同款约定）→ x/z 永不因物理
  变化（只有 y 下落，落地 snap 整数地面）；分离 pass 是**位置投影**
  （setPosition，不注入速度），单帧内解析完 → 物品终位 = 死亡位 +
  有限次位置推移 = 固定步轨迹的纯函数 = ×2 字节稳。真实 dt 字段只
  有 age / pickup_delay（长跑恒 0）/ 玩家 hunger / saturation →
  savenorm 剥 {hunger, saturation, remaining, age}，**物品位置保留**
  （它正是断言）。反例：若物品带初速滑动，rest 位置 = real-dt 积分，
  字节比对必须连位置一起剥（P52 破坏掉落无初速 = 同性质）。
- **pgrep 前缀误匹配 minecraft_tests（P58）**：E2E 守卫
  `pgrep -f "[b]uild/bin/minecraft"` 会**前缀误匹配**
  `build/bin/minecraft_tests` → 探针脚本先跑测试（或用户并行跑
  测试）时守卫 FATAL "a minecraft process is already running"。
  守卫模式加 `$` 锚（`[b]uild/bin/minecraft$`）或核对 comm；探针
  脚本别在同一脚本里跑测试再跑游戏。
- **裸截图无窗口稳定化 = 错尺寸假失败（P58）**：不经过 launch() 窗口
  稳定化（unmap_covers + move/raise/focus + 1280×720 校验）直接
  `--screenshot`：截到的是 WM 初始平铺窗口（实测 1272×1552）→ 与
  base18（1280×720）cmp 假失败。游戏截图 = 窗口 framebuffer 尺寸，
  一切截图 E2E 必须先稳定化并校验 `([0-9]+x[0-9]+) == 1280x720`。
- **45° 瞄准射线沿格网角点穿过 → DDA tie-break 命中对角固块（P59）**：
  yaw 恰为 π/4 时破坏射线 |dx|=|dz|，x/z 边界**同一 t 同时跨越**
  （射线过每格角点），DDA 的角点 tie-break 选中**对角**单元格 →
  射线实际穿过的格序列与手算的"逐格平移"不同（P59 首跑：目标
  (21,46,−1) 后，射线在角点 tie 进 (19,46,−2) 而非 (19,46,−3)，
  连破 3 块对角 dirt，数量随 fps 变 = ×2 终态不一致）。对策：
  (a) 选 yaw-0 走廊目标（P52 几何，射线正面穿格无角点）；(b) 场景
  diff 显式清掉**真实 raycastBlock 模拟**（放目标→raycast→setBlock
  air→再 raycast 循环）给出的角点格 → 序列 = 目标唯一、fps 无关。
  P59 S2 = (b) + 3 格走廊。
- **block diff 可写 "air"（P59）**：`air` 是 BlockRegistry 注册哨兵
  （`findByName("air")` 恒有效，无 air.json）→ 存档 block diff 能
  表达"生成固块 → air"（场景清走廊/挖坑）。注意终态：被破坏的
  **场景放置块**恢复生成态 air = diff 条目**消失**；**走廊 air
  编辑保留**在 diff（生成固块 ≠ air）→ 断言必须写终态 diff 的
  **精确集合**（set 比对，非顺序）。
- **zsh 里 `$(cat file)` 独立成行 = 执行输出而非定义（P59）**：
  助手函数块（`name() { ... }`）想"引入"脚本时，`$(cat helpers.zsh)`
  会把输出当命令执行（command not found）——用
  `eval "$(cat helpers.zsh)"`。姊妹坑：外层 `python3 - << 'EOF'`
  的补丁内容若含恰好一行的 `EOF`（比如内嵌 zsh heredoc 的结束
  标记），外层 heredoc 提前终止、补丁半截——换外层标记名。
- **run_break 输入必须发在 `wait $game` 之前（P59）**：launch() 在
  窗口稳定化后**立即返回**，游戏还在后台跑（--frames 420 ≈ 7..18 s
  墙钟）；输入锚（t_ml + 2.9 s 读日志 + mousedown/hold/up）必须全部
  落在 wait 之前（P55 模式）——先 wait = 游戏已退出，输入发空。
  每跑终态存档必须 wait 后**立即 cp** 走（下一跑的源副本会覆盖
  data/saves/<tag>.json，P15 陷阱的 E2E 对跑形态）。
- **zsh 多行 echo 续行必须带反斜杠（P60）**：`echo "part1 "` 换行后
  跟缩进的 `"part2" >> $LOG`——zsh 把换行当命令分隔符：第一条 echo
  只发 part1，第二行被当命令执行（command not found，rc 127，脚本
  继续但日志行截断）。P59 脚本的同款写法带 ` \` 续行；跨行字符串
  要么每行尾加反斜杠，要么单行写完。
- **json.dump 改资产文件 = 整体重序列化（P61）**：python one-liner
  给 JSON 资产加一个键时，`json.dump` 用**自己的** indent 重排整个
  文件——indent 与仓库文件不一致（本项目 = indent 2）= 全文件 diff
  污染（534 行变动，实际只有 4 行语义）。改前先看原文件缩进
  （`head` 或 `git show`），dump 用同款 indent，提交前
  `git diff --stat` 核对"改动行数 ≈ 语义改动行数"。
- **熔岩场景跨 `flamable` 契约变更复用 = 重查全部 6-邻可燃格（P63）**：
  P58 S4 的熔岩池场景（y45 凹穴 2×1 + 3 石缘）当时安全——池位于**草
  高原**上的凹穴，**整个周边环 + 池底全是草方块**，彼时不燃；P63 草
  变可燃后原样复用有**两种**失稳：① 燃烧 pass 烧穿池底（2 格 y44 草）
  → L8 池落层进 y44 洞穴、推进时烧穿整层草、漫到玩家脚下烧死玩家；
  ② 池烧穿可燃目标格后，L7 流体的**草 6-邻**被逐个点燃 = 高原**野火**
  （熔岩前沿 + 烧穿的草互相推进，整片高原烧成熔岩湖）。修法 = 把**每
  个终态熔岩格**的 6-邻可燃格全部石质衬砌（P63 S3 = 9 石：3 缘 + 池底
  2 + 墙 2 + L7 的草 6-邻 2）。旧场景复用的判据 = 列出每个**可能出现
  的熔岩格**（含烧穿目标格后的流体级）的 6-邻、逐格判定可燃性；证据
  直接从**未修补跑的终态 diff** 拿（原可燃格被烧 = 出现 `"air"` 条目；
  非可燃固块 = 紧邻熔岩却不烧也不进流体；**注意：某格未被烧只证明它
  在"该跑实际出现的熔岩格"旁不燃——若该跑熔岩根本没到达邻格，推断
  无效**【P63 第一轮误判 (24,45,-5)/(23,45,-6) 的根因】）。
- **单引号 save_exact 体里的撇号 = zsh parse error（P63）**：断言体是
  单引号 shell 字符串，Python 文本里的 `'s`（如 "L7's"）提前闭合
  字符串 → `parse error near ')'`。zsh **逐命令解析执行**：前面的
  S1/S2/S3 跑完全正常、到该行才炸——症状误导（像"场景又错了"，实际
  是脚本语法）。改脚本后先 `zsh -n script` 全量语法检查再跑批。
- **"L8 源静态"是熔岩专属规则，水 L8 会横向漫灌（P64）**：P58/P63 的
  熔岩池场景里 L8 池不扩散（熔岩 flow = 1：L8 → 至多 1 格 L7）——但
  **水 flow = 7**：同一格放 L8 水源，4 个水平邻格逐格 L7..L1 漫开
  （P64 S4 首轮实证：29 格水漫过整片高原，含沿坡落到 y44 洞穴）。
  流体蔓延规则（fluid.cpp simulateType）：**只水平 + 下落，绝不向
  上**；支撑（下方是块）才水平蔓延；**任何块（含非 solid）挡流**。
  水场景要"静止一摊水" = 高架水：石台上 L8 + 四个水平邻格全石封
  （P64 S4 终版几何）；地面裸 L8 水源在平坦高原上必然漫开。
- **`--save` 短帧截图跑 = 可视 chunk 流送竞态（P64）**：60f 的
  `--save` 场景跑，截图帧的**可见 chunk 未必全部 mesh 完**——流送
  走 worker 异步（每帧预算列数 + 完成时刻受线程调度影响），frame 60
  的 chunk 集合跨跑不确定：P64 S5 首轮一跑下半屏整片是天空（地面
  缺失 52 万 px diff），对照跑正常。**新鲜世界 60f 基线不受影响**
  （S1 连续 37 跑稳定：出生演示区同步生成 + 该视角的可见集恰好
  frame 60 前确定）——坑只在**读档跑 + 短帧 + 截图断言**组合。
  修法 = 场景截图跑用 600f（S3 已证 600f 终态 RAW 字节可复现 =
  流送确定完成）；存档断言跑（S2/S3/S4 类）帧数 ≥ 流送收敛即可，
  短帧只用于"中间态"意图明确的场景。同批另见：8px WM 黑条
  （窗口右缘瞬时 8px 客户区被边框吃 → 前缓冲 8px 黑列，环境性，
  pair 级重试吸收）。

- **瞄准 pitch 符号：正 = 朝上（P65）**：项目相机约定 pitch 正 = 上
  （硬规则 5）。E2E 瞄准几何算"低于眼高的目标面"必须用**负** atan2
  （P52 先例：`atan2(-0.12, 5.0)`）。P65 首轮 S2 用 `+atan2(1.0, 4.5)`
  = 朝上看 → 射线从石块上方飞过，单次右键零效果（终态 ×2 字节一致 +
  heartcheck >50 = 输入生效但世界零变化——此模式先怀疑瞄准几何而非
  输入；核对射线是否真进目标格）。另：侧脸瞄准的路径会穿过表面层
  格（y45 = 生成装饰风险区）；顶面瞄准（pitch = -atan2(0.62, 4.0)，
  射线 (22.5,46.0,-3.5) 中顶面）全程走 y46 已验证空位 = 更稳。
- **破坏 = 左键按住，放置/交互 = 右键（P65）**：`updateBreaking`
  由左键按住驱动（hardness 0 = 按住首帧即破）；右键按住 = 放置/交互
  链（空格 no-op）。P65 S4 首轮用 `mousedown 3`（右键）→ 火把完好、
  零实体、×2 字节一致 = 典型"按错键"签名（与瞄准错误同形态，区分 =
  查目标格在射线上的位置）。
- **awesomeWM 窗口稳定化：float toggle 是 TOGGLE + 60f 跑预算（P65）**：
  用户桌面是 awesomeWM（tiling）：新窗口立即被平铺（1272×1552@4,44），
  tiled 客户端上 `xdotool windowsize/windowmove` 被忽略；浮动切换
  `Super+ctrl+space`（= awful.client.floating.toggle）是 **TOGGLE**
  （平铺↔浮动），浮动化应用时窗口被重新居中 (640,440)——每轮都发会
  来回翻转状态；且窗口刚创建有**瞬态**（1280×720@640,440，awesome
  几百 ms 内平铺它）——读到该态"只补移"会被随后的平铺覆盖。正确
  逻辑 = 状态机：1280×720 错位读取先 sleep 0.25 s 重读区分瞬态 vs
  稳定浮动；平铺态才 toggle；toggle 后 size→move 顺序执行（0.35/0.1/
  0.15 s）。**60f 跑预算**：进程寿命仅 ~1.85 s（窗口 0.16 s 出现），
  稳定化全路径须 <~1.2 s（含 xdotool 往返开销），否则游戏先退出
  （`kill -0` 断循环 = FATAL）；S1 类短帧截图给外层重试层（每次重跑
  无副作用）。长帧跑（240f+）预算充足。WM 桌面/布局随用户活动变化
  （P65 批 3 中途从正常桌面切到 tiling 桌面 = 同脚本行为突变），
  稳定化失败先查 `xprop -root _NET_CURRENT_DESKTOP` 与窗口几何。
- **cross 粒子必须假法线 (0,1,0) 满亮，勿走 cube 面法线（P66）**：
  粒子立方体六面若用 `face.normal`，shader 按法线给面光（±Z 面 0.6）
  → cross 块（火/火把/植物）的粒子侧脸偏暗 40%。P66 余烬首轮 E2E 的
  签名：火焰像素滤镜（r>180）计数**开粒子 < 关粒子**（2355 < 2383）
  ——暗色余烬像素盖住火体亮像素 + 自身过不了滤镜。chunk_mesher 与
  item_mesh 的既有 cross 分支都用 `v.normal = {0,1,0}`（"full-brightness
  face shade"）= cross 渲染的项目约定；`drawParticles` cross 分支同
  款（P66 修复）。排查口诀：**新渲染路径的 cross 内容先对齐既有两分支
  的法线/UV 约定**。
- **活粒子（real-dt 运动）的截图不可字节比；config 键作 A/C 对照组（P66）**：
  粒子发射是固定帧步（确定性）但**运动积分 real dt**（P15 约定，同
  实体）→ 有活体粒子的帧截图跨运行**永不**字节一致（P54 场景截图
  条款的粒子实例）。断言降级为：存档 ×2（savenorm 剥 fires remaining
  等 real-dt 字段）+ 像素**计数 margin**（非字节）+ A≠C。对照组
  范式 = `fire.particles: false` config（mobs.enabled 同款）：C 组
  无粒子 = 静态世界 = 截图 ×2 字节可比，A 组火焰像素计数须**两跑都**
  超 C + margin（单跑可能撞相位低谷）。另：`ParticleSystem::update`
  内部 `m_particles = std::move(alive)` 替换 vector → 测试里跨 update
  持 `const auto&` 引用 = 悬空（UB，实测读到"更新后"的值造成假
  等值）；before 快照必须**按值拷贝**。
- **破坏掉落的静止位 ≠ 被破格中心（P67 实测复核 P59/P61）**：从格
  (22,46,-5) 破坏出的掉落实体停在 **(22.5, 45, -4.5)**——z 比格中心
  (-5.5) 偏 **+1**（P59 flint / P61 cobblestone 两次证明的同值）。
  E2E 断言掉落位置要用**已证明的落点常量**（精确值 + 1e-6 容差），
  不要按"格中心下方"的几何直觉推范围——P67 首轮按中心 z∈(-6,-5)
  断言，实测 -4.5 出界 FAIL（机制本身全对，×2 savenorm 字节一致）。
- **xdotool 键名：`Left_Control` 非法、且失败是静默的（P71）**：
  `xdotool keydown Left_Control` 只打一行 `(symbol) No such key name
  'Left_Control'. Ignoring it.` 到 stderr 然后**忽略**——输入被静默丢弃，
  E2E 失败表现为行为断言错（玩家该坐着却走掉了），而不是输入错误。合法
  名 = X11 keysym（`Control_L`）或 xdotool 别名（`ctrl`/`shift`/`alt`/
  `super`）+ 单字符（`w`）。口诀：**新按键先 `xdotool key <name> &` 冒烟
  看 stderr 有没有 "No such key name"**；E2E 脚本里 `2>/dev/null` 会吞掉
  这个警告——键输入动作（act_*）不要静默 stderr。
- **键组合的 keydown 顺序 = 确定性变量（P71）**：多键输入里，"状态
  建立键"必须**先于**"移动键"落下。P71 座位 E2E 首轮 `keydown w` 后
  `keydown ctrl`：W 在 ctrl 落帧前的 1–2 帧以正常速度走出 ~0.09 格，
  漂移帧数取决于按键与帧边界的相位 → 两次运行存档不同（savenorm
  ×2 失败），且终态 pos 不 EXACT。修 = `keydown ctrl; sleep 0.1;
  keydown w`：坐位（清零 wish）在 W 首帧前建立，所有帧序交错下移动
  恒 0。口诀：**输入组合若含"锁/状态 + 动作"两键，锁先发**；断言
  EXACT 的输入跑先审 keydown 顺序。
- **每帧纯 SET 与帧间状态相抗（P72）**：P56 的 `syncAbsorption` 在
  `updateEffects` 末尾**每帧无条件**执行——bonus 模型下幂等（bonus 不变
  + clamp 无副作用），两分量模型下却把伤害扣减**每帧重置回效果总和**
  （E2E 实测：池 4 → 坠落伤害 4 → 0 → 下一帧同步回 4 → 存档 4 ≠ 期望
  0；`damage()` 内日志确认扣减真的发生过）。修 = 只有效果总和**变化**
  才同步（`absorptionBefore` 前后比较，到期/水熄灭才算变化；熔岩点燃
  的每帧 `apply` 刷新 fire 效果不改变吸收总和 → 不触发）。口诀：**把
  "X 的纯函数"从每帧 SET 改成会被其他路径覆盖的值时，审计帧间还有谁
  改它——有就把同步降级到变化点**；单测测不出这个（单测不调
  updateEffects 帧循环），必须 E2E 实跑。
- **自然回血污染长跑 E2E 伤害终态（P72）**：玩家受创后，run 超过
  `regen_delay_seconds`（10 s）就开始 +1 HP/4 s 自然回血（hunger ≥ 6）
  → 终态 health 变 **fps 相关**（P72 首轮 600f 实测：机器 ~15fps ≈
  40 s → 12+6 = 18 ≠ 期望 12；换台 37fps 的机器 = 13——同场景同断言
  跨机器不可复现）。修 = 坠落场景 run 缩到**延迟窗内**（120f ≤
  5.2 s @23fps，落地 ~40 帧后有 80 帧余量）。口诀：**EXACT 断言伤害
  终态 health 的 E2E，先算 run 真实时长上界（帧数 ÷ 最低合理 fps）
  < regen_delay，否则 scene config 关再生或 hunger 压到阈值下**。
