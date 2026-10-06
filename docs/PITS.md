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
