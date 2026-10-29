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
- **range-for 遍历的 vector 被回调 push_back = 悬空引用（P27 潜伏，P73
  暴露）**：`Game::updateEntities` 原用 `for (auto& entity :
  m_entities)` 跑实体更新；skeleton 射击回调（在 `mob->update()` 内
  被调）里 `m_entities.push_back(arrow)`——vector 一旦重分配，循环的
  `entity` 引用悬空 → SIGSEGV（rc=139）。**潜伏 P27 至今**：P58 水膜
  对照里水挡视线，骨架从未射出成功的箭；P73 火把膜（光 14 缺口环 →
  敌对生成）让骨架首次拿到无遮挡视线，首帧即崩。修 = **索引循环**
  （`for (i < m_entities.size()) { auto& e = m_entities[i]; ... }`）：
  每轮从当前存储重取引用，延迟的箭同帧被走到（原意图语义）。口诀：
  **回调/lambda 里改容器，调用方遍历该容器 = 重分配即 UB；要么索引
  重取、要么延迟到循环外**。崩溃定位 = 日志最后一行就是肇事回调
  （"skeleton shot: ..." 之后无输出）。
- **E2E 场景生成的存在性守卫被残留文件整块跳过（P74）**：批跑脚本的场
  景生成块用 `[ -f $T/第一个场景.json ] || python3 ...` 守卫——冒烟测
  试已在 $T 留下同名场景文件，正式批跑时整个生成块被跳过 → 只有冒烟
  建过的场景存在，其余场景缺失（`cp` 报 "没有那个文件" 才暴露）。修
  = 守卫条件覆盖**全部**场景文件，或无条件生成（python 生成幂等，几
  ms）。口诀：**幂等生成块不要加"存在即跳过"守卫——冒烟残留是最常见
  的误判来源**。
- **Json 数组没有元素变异 API——`arr[i] = x` 运行期炸（P75 实测）**：
  给存档测试的 JSON 换数组里一个元素时写了 `slots[0] = slot`（编译
  通过！运行期抛 "basic_string: construction from null is not valid"
  ——误导性的异常，完全看不出是 Json 的问题）。Json 数组只读接口 =
  `at(i)` / `size()`，写 = `push_back`。口诀：**要换数组元素 = 重建整个
  数组**（逐项 `at` 出来、改第 i 个、`push_back` 进新数组）——P75 测试
  的 `withSlot` lambda 范式。
- **项目改名后 xdotool 按名搜索误匹配终端窗口（P76）**：E2E helpers 用
  `xdotool search --name "游戏标题"` 找游戏窗口——`search --name` 是
  **子串**匹配。项目从 mine-craft 改名 TokenMine.cpp 后，用户终端的窗
  口标题 `user@host:~/project/TokenMine.cpp` **包含**游戏标题子串 →
  `tail -1` 可能取到终端窗口：状态机对终端"稳定化"（它本来就
  1280×720 在目标位 = 直接 winok，日志里**没有 stabilizing 行**这一
  特征可辨），真正的游戏窗口被 awesomeWM 平铺成 1272×1552，截图尺寸
  检查失败且 3 次重试全中同一假窗。修 = **锚定正则** `--name
  "^TokenMine.cpp$"`（xdotool search --name 接受 regex）。口诀：**窗
  口搜索名必须锚定首尾——标题是短词时，终端路径/项目名极易形成子串
  包含**。
- **弓的拉弓 = 右键，输入脚本用错键 = 静默无箭（P77 实测）**：P24 起
  "发射器右键恒为拉弓"——左键选中弓 = **近战挥击**（不射箭、不挖掘）。
  P77 首轮 E2E 的射击动作用了 `mousedown 1`（左）：20 次"射击"全是挥
  拳，终态箭数与"没射"完全相同（不消耗也不返还），且**无任何错误日
  志**（挥击是合法操作）——失败表现为断言数值差 2，而非崩溃。修 =
  `mousedown 3`。口诀：**输入脚本里"用发射器"先查 P24 右键链纪律
  （拉弓 = 右键按住、松键射出；左键 = 近战）；射箭类场景的终态断言
  要选"射出与否可判别"的量（本阶段 = 拾取返还的箭数）**。
- **截图比对跑的帧数要够 chunk 流送落定——120f 不够（P78 首轮）**：
  P64 的教训（60f `--save` 跑可视 chunk 流送未竟 = 地面缺失 worker
  竞态）同样适用于**截图**比对：P78 首轮 S2 用 120f，同场景 ×2 存档
  RAW 字节一致但**截图不一致**（流送进度在截图时刻跨跑不同）。600f
  落定后 ×2 RAW 全过（P71 S2r 同款 600f 先例）。口诀：**无输入
  RAW 截图对跑 ≥600f；存档 RAW 对跑可短（存档不含未落定 chunk 的
  可见性），截图不行**。
- **real-dt 锚定输入 → 固定步长实体的帧相位抖动（P79）**：输入锚定
  t_ml+2.9 s（墙钟）释放弓，箭在**不同帧**出生 → 固定 1/60 弹道整体
  平移 ≤1 步（0.25 块 @15 b/s）→ 插地点亚块差 → 箭底面光采样不同 →
  同弓两跑截图在世界区差 0..~90 px（实测 power vs plain 跨批 0/70/
  82）。**存档不受影响**（箭不入档）——只有像素断言要设**相位抖动界
  限**（同弹道 ≤150，真位移 ≥100 可分）。口诀：**real-dt 锚定的发射
  类输入，终态截图里会动的实体位置带 ≤1 帧抖动——像素恒等断言改"抖
  动界 + 位移界"双界关系式**。
- **信标吸收池须在 updateBeacons 内同步（P83）**：吸收池 = 效果总和的纯函
  数只在同步点更新（P72），`updateEffects` 的"帧内总和变化才同步"检测**看
  不到**信标的 apply（信标在 updateEffects 之前施加）——首轮 E2E 池 = 0
  但 effects 含 absorption（施加了却没同步池）。修复：`updateBeacons` 捕获
  施加前后 `totalAbsorption`，变化即 `syncAbsorption()`。口诀：**凡在
  updateEffects 之外改效果总和的路径，须自带前后总和比对 + 同步**。
- **stepUp 的"1 格台阶"判定用格 solid 属性 → 踏上墙/栅栏的帽（P95/P98 潜
  伏，P99 跳跃对照场景首触发）**：stepUp 的"台阶存在 + 上方两格空"检查
  原用 `isSolidCell`（该格 properties.solid）：墙/栅栏实心区 1.5 格、**上
  方格是空气** → 检查全过 → 脚被抬 +1.0 **钻进帽内**（46 < 46.5）；随后
  的跳跃从脚 46 起跳（+1.125 > 46.5−46）**翻过 1.5 barrier**（S3b 墙对照
  场景 50% 翻越率 = 跳时抖动决定起跳时脚在 45 还是 46）。修复：台阶判定
  改用 `solidTopHeight ≤ 1.0`（真 1 格台阶；楼梯/台阶 顶 <1.0 仍可行
  走上去）。口诀：**"1 格台阶"类判定一律用实心区顶（solidTopHeight），
  不用格的 solid 属性——实心区 >1 的块（墙/栅栏）上方格是空气但帽占着**。
- **长帧（流送卡顿）上的跳跃顶点膨胀——86ms 帧翻过 1.5 墙（P99 插桩定
  位）**：子步数用的 `maxSpeed` 估计取**跳跃前**的 vy（=0）→ 长帧
  （dt≈0.086，流送卡顿常见）只切 1 个子步，整帧按 8.5 b/s 跳速位移 0.73
  块（> 0.4 子步上限，违反硬规则 #6）→ 跳跃顶点从 1.129 膨胀到实测
  **1.649**，翻过 1.5 墙/栅栏（物理帧率依赖：60fps 稳态翻不过，卡帧翻
  得过——E2E 同场景两 run 一个停 -0.7 一个到 -6.4 即此）。修复：pending
  跳跃冲量（onGround && !sitting && 非流体 && space 按下）的 `jumpSpeed`
  计入 maxSpeed → 长帧切 2 子步，顶点 ≈1.16 稳定 <1.5。口诀：**子步内
  才会 SET 的冲量（跳跃/游泳上涌）必须计入 maxSpeed 估计，否则长帧退
  化成单子步、冲量帧全位移**。
- **薄片形状（fence/glass_pane）破坏崩溃——P65 模式的第二次出现（99.1 定
  位）**：破坏栅栏/玻璃板时游戏 terminate（`unordered_map::at`）。这两类
  形状**恰有一个纹理**（`side`；top/bottom 空串是设计——init 校验强制
  cross 纪律），而破坏的**两条渲染路径**都读 cube 面：破碎粒子
  （drawParticles 的 `cubeFaceTexture` 分支只 guard 了 Cross）+ 掉落物
  的 item cube（makeItemCubeGeometry 同款）→ `uvMin("")` → 抛异常。
  P98/P99 的 E2E 只放置/走进、从不破坏这两个块 → 路径从未被测。排查
  弯路：地形内容全部审计干净（76 纹理名 vs atlas/磁盘全对、全环流送
  两跑无崩溃）后，才从"崩溃只发生在有用户交互的 run"锁定交互路径。
  修复：两路径对 Fence/GlassPane 用 side 纹理（cross 规则同款）+ 单测
  （side/top 分行判别）+ 破坏 E2E ×2 形状。口诀：**新块模型形状必须
  配"破坏"场景的 E2E——破坏路径（粒子 + 掉落物渲染）的纹理契约与
  chunk mesh 独立，放置/走进测不到**。
- **部分列 = 永久虚空洞 + "engaged 但零 post"停摆（99.2 定位）**：单
  chunk 创建（存档流体编辑列 / 放置首触）留下缺 7 chunk 的列；流送
  loader 把"有任何 chunk 的列"计为 loaded → 缺的 chunk 永不生成；
  同时 missingColumns 排除它（算 loaded）而 nearestMissing 计入它
  （算 missing）→ 永久 engaged + missing 列表为空 + 池空 = "流送
  停摆"（误诊方向：worker/积压上限/isDead 全部排查过）。修复 =
  列完整性不变式：`World::ensureColumn`/`columnComplete`，每个单
  chunk 创建点（setBlock 收口 + applyWorldEdits）补全整列。口诀：
  **流送的单位是列，chunk 创建的单位也必须是列——任何单 chunk 创建
  路径要顶格补全，否则"loaded 判定"与"缺失判定"用两个真相源**。
- **存档 time 覆盖 config + 夜间 mob 杀死静止玩家 → streamChunks 的
  isDead 停摆（99.2 排查弯路）**：无输入性能跑用存档场景，以为从
  config 的正午 0.25 开始，实际存档 `time=0.541`（夜，优先于
  --time/config）→ 敌对 mob 生成 ~60 s 内杀死静止玩家 → 流送停
  （isDead 停 post 是**设计**：死亡界面拥有输入）→ "零集成"被误诊
  为 worker/积压 bug。判别器 = **R 键**：玩家死则 R 重生且流送立
  刻恢复；R 无效才怀疑代码。口诀：**带存档的长跑先核存档 time
  （它赢过 config）；"流送停了"先 R 测 isDead，再怀疑流送代码**。
- **存档同格块+流体规则过严 + 既有单测假阳性（99.3 定位）**：Phase 51
  规则拒绝"任何块编辑与流体编辑同格"，但 diff 的合法组合有（块编辑
  →air + 流体>0：P90 烧掉的格被熔岩流进——live = 纯流体）与（非 air
  块 + 流体=0：块排开生成的流体——live = 纯块）；唯一非法 = 非 air
  块 + 流体>0（live 双占）。且该规则**既有的单测是假阳性**：它用流
  体 edit 的 lambda 传 `"stone"` 造"块编辑"→ 实为 5 元素畸形块编
  辑 → 解析在更早处失败，规则从未真被验证（测试绿 ≠ 规则活）。另
  level-0 流体编辑对已干格必须 no-op 跳过（setFluid(0) 对干格返回
  false = 加载失败）。口诀：**"单占不变式"类校验要按 implied live
  state 表达，不按"编辑共现"；校验类单测先确认测试数据本身合法
  （看报错消息命中的是哪条检查）**。
- **环满时 4 次流体全扫描是主成本 + 限扫描集要含"同 tick 可写入的邻
  域"（99.2 定位，插桩实测）**：reaction/burn/water/lava 每 pass
  各 `chunkPositions()`（全量拷贝+排序）+ 32K 格遍历——8712 chunk 环
  满 + 活熔岩湖时 update=151 ms/帧（~6-7 FPS，worker 池空闲、CPU
  仅 1.1 核 = 主线程扫描瓶颈）。修复：World 维护 per-kind 流体 chunk
  集（setFluid/setBlock 排空/insert/erase/生成 五处维护），simulate
  迭代"流体 chunk + 六邻"（**flow 写入每 tick 至多跨一个 chunk 边界
  ——邻域必访才保住同 tick 传播语义**；只访流体 chunk 会把跨块前沿
  延迟 1 tick/边界 = 改变 E2E 帧时序），reaction/burn 只访 lava 集。
  配套：重 mesh 帧预算 8（最近优先，消 100-400 ms 集成卡顿）、worker
  积压上限 8（flush 全队列等待的有界化）、World 脏列集 + 增量
  loadedColumns + 显式 lightPendingColumns（替 4 次游戏全扫描）。
  实测：环满 update 151→21 ms（~35+ FPS）；走路跑 RSS 平台
  470-474 MB（修复前无限增长）。口诀：**每帧扫描单位 = 受影响集
  （流体 chunk/脏 chunk/loaded 列），不是全 chunk 表；模拟限集必须
  包含该 tick 可写入的全部目标（含邻块），否则传播时序变 = E2E 帧
  断言变**。
- **光照写 chunk 只设标志、绕开 World 脏集 → 新列永久黑面（99.2 走路跑
  用户发现）**：把重 mesh 工作表从"每帧全 chunk 扫标志"换成 World 脏
  集后，`Lighting::updateRegion`（worker）/`propagateAll` 对光变 chunk
  的 `findChunkMut(pos)->markDirty()` 只设了 chunk 自身标志——脏集看不
  到 → 该 chunk 永不重 mesh → 刚光照的列保持光 0 mesh = 地表黑面
  （旧代码靠全扫描兜底所以从未暴露）。修复：`World::markChunkDirty`
  （标志 + 脏集一体），光照两处改走 World API；单测钉死标志/集合同步
  + 缺失 chunk no-op。口诀：**脏性只允许一个真相源（World 脏集）——
  任何直写 chunk 的子系统（光照 worker 等）改完必须经 World API 报
  脏；把"全量扫描兜底"换成"显式集合"时，先 grep 出所有绕过集合直写
  底层状态的路径**。
- **重 mesh 帧预算把"光照重网格"推迟成秒级黑面（99.4 修复）**：99.2 把
  每帧重 mesh 限到 8 块（最近优先）后，新流送列的"光 0 首网格 →
  光照 job → 重网格"二段式里，第二段排在整条脏块积压后面（连续流送
  时积压不消）→ 新地形黑面持续数秒（用户走路跑报告"一排侧面黑/
  单块侧面黑/草块一面黑"，打掉邻居 = 手动标脏 → 立即正常）。正确
  设计 = **lit pop-in**：unlit 列在光照 job 完成前根本不网格化
  （m_unlitColumns；LightJob 带 done + columns，flush 后收集），完成后
  一次网格（光照终值）——黑面消失 + 网格工作量减半（每新列 mesh 一
  次不 mesh 两次）。判别 E2E：边缘玩家无输入 600 f（流送活跃、边缘
  列在画面里），世界区零纯黑像素 + x2 RAW（旧代码该窗口内必黑）。
  口诀：**"先画错的再修"的两段式更新，一旦第二段被帧预算排队，错误
  就可见化——要么让第二段免预算，要么根本别画第一段（pop-in）**。
- **SDL X11 静默拒绝非 ASCII 窗口标题 → FPS 仪表从未显示过（99.6）**：
  标题串含 em dash "—" 时 SDL_SetWindowTitle 返回 false（"Failed to
  convert title to UTF8"——SDL3 的 X11 编码转换对该字符失败），游戏
  代码忽略返回值，窗口名永远是初始 "TokenMine.cpp"，标题里的
  "N FPS | M draw calls" 仪表**从未生效**（多个阶段的 E2E harness
  靠标题轮询 FPS 全部拿到 0，当时以为是 harness 问题）。最小复现：
  SDL3 + X11 + 含 U+2014 的 SetWindowTitle → false；ASCII 连字符 →
  true。口诀：**SDL 的 setter（SetWindowTitle 等）返回 bool——忽略
  返回值 = 静默功能死亡；窗口标题/WM 属性只用 ASCII**。
- **"level ≥ 2 = 活跃"的流体门控抓不住静态流动梯度（99.8 两轮定位）**：
  第一版活动门控用"chunk 内最大流体等级 ≥ 2 = 需遍历"——流动中的熔岩
  （level 8→1 的梯度）处处 ≥2，但**流到静态平衡后梯度仍保留 level 2-7**
  （下坡流停在斜面上 = 每格水位差 1 的静态梯度）→ 整个峡谷流被永远判
  "活跃"，40 ms/tick 恒定 5000+ 帧不衰减（实测：lava 状态 150 s 与
  300 s 逐格相同，成本纹丝不动）。正确判据 = **变化热**（MC 的 cell
  队列性质）：上一 tick 该 chunk 有流体单元格被写入 = 热，遍历后消费；
  块变更（挖支撑）= walk-pending 强制一次。口诀：**"可能变化"≠"正在
  变化"——静态平衡态可以带着全部中间状态（level 2-7 梯度）存在；
  活动门控必须锚定"最近被写入"，不能锚定状态值**。排查手法：给
  simulateType 加"实际遍历 chunk 数 / 流体 chunk 总数"计数——59/17
  恒定不动 = 门控形同虚设，一眼看出（状态对比 + 计数双证据）。
- **列主存矩阵取"行"：m.m+4k 是第 k 列不是第 k 行（99.9 视锥裁剪首测
  全挂）**：Gribb-Hartmann 视锥提取要 MVP 的 row0..row3；列主存里
  row k = 步长访问 (m[k], m[4+k], m[8+k], m[12+k])，而 `m.m + 4*k`
  是第 k 列。写成连续指针后 6 个平面全错，**identity 测试却过**
  （identity 行=列，bug 被掩盖）——矩阵布局的测试必须用非对称矩阵
  （perspective×lookAt）才暴露行列混淆。口诀：**列主存取行要步长 4；
  identity 单测测不出转置类 bug**。
- **单增 worklist BFS 只能从"入队"的格子传播光——种子集不是
  "变化格+壳"，是"区域内全部有光格"（99.9 两轮回归）**：第一版把
  天空光种子减成"rescan 写过的格子 + region 外壳"→ InsertedColumn
  测试 6712 格差：天花板下的新列整列天生暗（rescan 0→0 零写入），
  侧邻列的 15 光必须流入，但侧邻格"未变化"→ 未入队 → 永远传不进
  来（worklist 不经过的格子不转发）。第二版把种子减成"rescan 列 +
  壳"仍挂：rescan footprint 缩到 box 自身列后，±R 内"穿过 void 借光"
  的旧列保持陈旧高光（rescan 不复扫→不清除），且邻列种子缺失。
  教训：**重光照的 region（±R）和 rescan footprint 都是语义必需的**
  （void→terrain 改变 region 内任意列的光场），能省的只有"查找成本"
  （chunk 级扫描 + BFS last-chunk 缓存：12→~7 ms/box）与"无事可做"
  的整段跳过（rescan 零写入 且 非新地形 → 跳过天空 BFS；**新地形必须
  强制传播** = `newTerrain` 参数，单测钉死）。
- **给帧管线插桩时把 `m_pool->flush()` 连同注释一起替换掉了（99.9
  首测 SIGSEGV）**：插桩脚本的 old/new 文本把 flush 块包进替换范围、
  新文本忘写回 → 每帧不再等 worker → 主线程 insertChunk 与 worker
  光照读 unordered_map 并发 → core 里三线程同碰一个 hash map
  （main=insert，两 worker=findChunkMut）。core 回溯 + 时序推理定位
  （"pipeline 契约下不可能"的状态出现 = 先怀疑本次改动，再怀疑
  契约）。口诀：**插桩/重构帧管线后，第一件事 = 确认 flush 还在、
  post 还在窗口内；"不可能"的崩溃快照 = 窗口被打破，不是竞态
  概率事件**。
- **away-mode 桌面偷点击/隐藏窗口（STRM-1 E2E，2026-10-21 夜）**：用户 awesome
  的 "Away" 组件（壁纸+天气，5 分钟周期）在无值守过夜运行时**隐藏客户窗口
  （~10-30 s/周期，窗口对 X 消失但进程存活）** + 移动桌面 → 指针落在
  **root 窗**上，XTest 点击**静默丢失**（键盘事件路由不同、部分时刻不受
  影响）；启动时的 unmap_covers 无效（跑中会再发生）。注意：用户 10-21 夜
  只修了壁纸切换，**窗口隐藏整夜持续**（用户其他窗口亦 IsUnviewable）。
  修复 = 多重防线：① `tm_window_guard` 后台（IsUnviewable/IsUnMapped →
  windowmap + raise + 必要时重 stabilize）② 点击前指针目标检查
  （`getmouselocation` 必须报游戏窗）+ windowraise ③ 点击对游戏
  `"Pointer captured"` DEBUG 日志行验证（已锁时盲目重试点击 = 挖块）。
  **夜间长跑变体 = 小环**（TM_S4WALK_R=8 scratch config：填充 ~3 s +
  走路 12 s 塞进窗口存活安全窗；TM_S4WALK_NOLOCK=1 跳点击——移动键不受
  指针锁定门控，输入有效性 = 自校准 k 证明；A6 红心降 informational）。
  本 xdotool 版**无 getwindowdesktop 命令**（"Unknown command"，stderr
  吞掉后静默空值 = 桌面修复分支从未生效过一次）。口诀：**输入 E2E
  "锁不上/走了又没走" = 先查指针下是哪个窗口 + 游戏窗 Map State +
  窗口是否还对 X 可见，别假设"窗口还在"；用户会话的 away/隐私组件是
  无值守 E2E 的头号敌人。**
- **XTest 假输入不重置 X screensaver 计时器（STRM-1 E2E）**：`xset q`
  timeout 600——X server 只计**真实设备输入**，xdotool/XTest 事件不重置
  → 过夜 E2E 显示被 saver 黑屏、间歇干扰输入/回读。修复 = 批次开始
  `xset s off; xset -dpms; xset s reset`，收工恢复（用户原状 = 600/600 +
  DPMS enabled）。
- **unmap_covers 豁免自己窗口须双重检查（STRM-1 E2E）**：`xwininfo -root
  -tree` 输出窗口 id 是 **hex**（0x3420017）、`xdotool search` 输出
  **decimal**（54526007）——直接比较豁免静默失败（被平铺成 1272×1552@(4,44)
  的游戏窗满足 cover 谓词 → 把自己的游戏窗 unmap → 点击落 root）。豁免 =
  `id == $TM_WID` + `_NET_WM_PID == $TM_PID` 双保险。口诀：**写"豁免自己"
  白名单先验证两边 id 格式一致。**
- **real-dt 火 + 含活跃熔岩湖焦土的场景存档（STRM-1 E2E）**：火寿命 =
  real-dt（P64 设计）；600f（~10-17 s）场景跑跨越燃尽边界 → 两跑存档帧
  的**火集合**不同（实测：对角火格互换 + fluids 17 vs 16 + blocks
  air vs fire），存档 ×2 不可字节比。修复 = 含活跃火的场景跑须越过 P90
  静态终态窗（3600f ≈ 2-3 min，实测存档 ×2 字节一致）；截图仍差（流送
  pop-in）→ P54 条款（存档 ×2 + notblack + skygate）。
- **合成器全黑回读（STRM-1 E2E）**：窗口跑中被遮挡/unmapped 时 picom 停
  更新 drawable → 进程内 `glReadPixels` 截图 = **整屏纯黑** 1280×720
  （PNG 显示 "Depth: 8/1-bit"、游戏日志完全正常、尺寸守卫抓不到）。修复
  = 尺寸守卫 + 全黑守卫（`notblack ≥ 800000 = 废拍重试`，P25 陈旧弃跑
  先例）。
- **PIPE-1 post 顺序不变量（2026-10-21）**：post→flush 窗口内三类 job 的 post 顺序固定 =
  streamChunks（gen，只写 scratch）→ postMeshJobs（**同步快照拷贝** + post）→
  postLightJobs（窗口内唯一活 World 写者，最后 post）。快照拷贝一致性靠"拷贝时
  唯一在跑的 worker 是 gen（不碰 World）"；任何重排（light 先于 mesh post）=
  快照可能拍到同窗口被写的光 = 间歇性撕裂读（非确定像素、难复现）。口诀：**窗口内
  新增读 World 的 job 类型，先定它在 gen→mesh→light 序中的位置，写进
  thread_pool.hpp 契约。**
- **PIPE-2 observer 安装时序（2026-10-21）**：存档 journal 的
  World 层 observer 必须在 **buildScene 顶部、任何格级写之前**安装
  （demo 区生成走生成器回调的 chunk 级直写 = 天然不触发；读档重放
  applyWorldEdits 经 World::setBlock/setFluid = 自动播种，零额外代码）。
  晚装（首帧 update 后 / tryLoadSave 后）= 静默丢编辑——退出存档缺编辑
  但照常加载成功 = 数据丢失级且难发现。口诀：**"全量审计"机制
  （journal/计数器/diff）的挂点 = 第一个写者之前，播种路径必须有单测
  钉死（读档重放 → journal 含全部编辑）。**
- **PIPE-2 位移陷阱（2026-10-21）**：`World::setBlock` 放块进流体格时
  **内部** `chunk.setFluidAt(local, 0)` 排空流体（world.cpp:64-68）——
  不走 setFluid。只挂 setFluid 的 observer 漏记排空格（diffWorld 会输出
  level-0 编辑而 journal 漏 = 跨路径字节等价断言碎）。修复 = setBlock
  成功点双上报（CellMutation.fluidCode=0）。口诀：**审计"全部写路径"
  要逐行读收口点实现，别假设"流体写都走 setFluid"（99.2 残余① 同款
  根源：FluidSim 三 pass 绕过 Game 收口直写 World）。**
  `integrateStreamedColumns`——stream insert 对邻居列的标脏若被 mesh 集成的
  clear-dirty 吞掉 = 新列网格永久 void 烘焙（幽灵面）。最坏 +2 帧收敛：列 C 已
  mesh、邻列 L 才 stream 进来 → C 边界面临时对 void 烘焙，C 在 L 的光照 job 期
  被光足迹规则（Cheb ≥2）排除、L 插入后 2 帧 C 重 mesh（dirty 自愈）；幽灵面朝 L
  内侧、玩家 2 帧内进不了新地形 = 实际不可见。口诀：**同窗内"清脏"操作必须排在
  "新标脏"操作之前，否则标脏被静默吞。**
- **zsh 函数前缀赋值不持久（STRM-1 E2E）**：`TM_OUT="$log" wait_game_window
  ...`——函数内可见，**返回后恢复原值（空）**→ 后续 `lock_pointer` 的
  `${TM_OUT:-}` 看到空 = 静默跳过验证分支（报"成功"但没捕获，连跑 3 次
  才发现）。修复 = 调用前独立持久赋值行 `TM_OUT="$log"`。口诀：**函数要
  下游读的环境变量用持久赋值，不用前缀。**
- **real-dt 火寿命 × 帧锁定蔓延 = fps 耦合焦土环（2026-10-21 PIPE-2 夜
  E2E 发现，既有缺陷非 journal bug）**：火寿命 = real-dt 倒计时（P64
  设计）× 蔓延 roll = 每 6 帧（P66 帧步）→ 一簇火寿命内的蔓延 roll 次数
  = 4 s × fps——不同二进制（主线程负载不同 → fps 不同）roll 次数不同 →
  P90 表面湖焦土环**跨二进制差几个边界格**（实测：两二进制 3600f 黄金
  A/B 仅 (24,35,19) 一格差，fluids/stairs 全同；同二进制 ×2 同 fps 档
  字节一致 = P90 E2E 先例）。后果：**跨 build 的存档字节 A/B 断言必须
  消除 real-dt 蔓延变量**（scratch config `fire.spread_chance: 0` = P74
  "spread0" 先例；直烧 = 帧锁定熔岩 burn pass，fps 无关），或接受边界
  格差并逐一列明。口诀：**"帧锁定 roll + real-dt 寿命"的组合天生 fps
  耦合——跨 build 确定性断言先查这个组合在不在场。**
- **LGT-1 翻转：性能优化的相邻前提被选列策略实际分布证伪（2026-10-21）**：
  union-region 设计案假设"同帧选出的 2 box 相邻（Δ 典型 (1,0)/(2,0)/(1,1)）"，
  step-0 实测：选列 = (distSq,x,z) 序，同 distSq 平局时取同半径圆上位置序
  最小的两点 = 左弧 z=±z* 远隔对称对（Δz = 2z* 典型 15-60 列）→ 可合并
  （双轴 |Δ|≤2）帧仅 6/6418 = 0.1%，预期节省 0.002 ms/帧（≪ 1 ms 砍除线，
  500× 过线）。口诀：**性能前提依赖某选择/调度策略的输出分布时，先跑
  step-0 直方图再写设计，别用"最近 2 个 = 相邻"的几何直觉推 Δ。**
- **sky rescan clobber（LGT-1 设计时发现，潜伏未修，2026-10-21）**：顺序
  两次 region 光照传播中，天花板板骑跨 footprint 边界时，后一 region 的
  rescan（sky = 纯地形函数重写）把边界格的前一 region 水平光流擦成 0，且
  sky BFS 无边界 seed 无法自愈 = 1-2 格 stale-dark 暗带（block 光有 shell
  注入自愈、不受影响；确定性不受影响）。修复形态 = 单遍 union（LGT-1 形态，
  已砍除）或 sky shell seed（留 100+）。口诀：**"纯地形重写"式 rescan 跨
  region 边界会丢水平流——region 化传播的测试要含骑跨边界的天花板 fixture。**
- **step-0 插桩读 moved unique_ptr 空引用（LGT-1）**：postLightJobs 里
  `m_lightJobs.push_back(std::move(job))` **之后**再读 `job->columns`——
  unique_ptr move 后为 null = SIGSEGV（首跑死在第一个光照 job，EXIT=139；
  构建 0 警告抓不到这类空引用）。插桩日志块必须放在 push_back 之前。
  口诀：**`std::move` 之后任何对源句柄的读 = 空；统计/日志放 move 前。**
- **复用 P94 时代场景坐标未探针地形（ENT-2 E2E，2026-10-22）**：S2 场景
  首版按 AGENTS 记载的 P94 几何（stone@(22,45,0) + stairs@(22,46,0)、玩家
  feet 45）构造 → 落体探针实测 (22,*) 地面顶 = **38** → 楼梯悬空 8 格出框 +
  玩家坠落 7 格 → A/B 像素 diff 0 被误诊为 state→mesh 断链（用旧二进制
  A/B 对照才定位 = 场景几何错、非代码错）。P94 时代输入场景"工作"靠相位
  容错（点击时玩家仍在坠落中段、视线擦到悬空楼梯底），无输入渲染场景无
  此容错。落体探针 = pstrm1_scene.sh construct --pos X,120,Z → 300f 跑 →
  ground 子命令。口诀：**复用历史场景坐标前必须 ground 探针实测当前地形；
  记载坐标是当时场景参数，不是地形事实。**
- **旧二进制 A/B 必须跑旧版资产 scratch + TM_REPO 覆盖（GEN-3 E2E，
  2026-10-22）**：内容契约演进是单向的——新内容 JSON（sapling 的
  growth_ticks 配对依赖 tree 对象）被旧二进制的 registry 拒（init 失败）；
  旧 A/B 跑直接读新仓 assets = 3 连 init 失败废拍。修复 = `git archive
  v0.3.0 assets config | tar -x -C /tmp/scratch_oldcwd` + `TM_REPO=scratch
  chamber 脚本`（wait_game_window 强制 `cd $TM_REPO` 启动游戏，TM_REPO 可
  env 覆盖）。同次次坑：洞穴矿场景相机 pitch 必须按**真眼高（feet+1.62）**
  算视线——选点探针用了 +0.62 误判，实际视线被上方一格石檐挡住（全帧石墙
  diff=0）；另 GEN-1 深洞矿格光 ≈0（64 列区零 light ≥9 矿格）= 矿场景需
  场景火把照明（同场景新旧公平）。口诀：**跨版本 A/B = 同场景 + 同照明 +
  旧版资产 cwd；视线类选点探针的眼高公式要按玩家 AABB 复核。**
- **xsecurelock 锁屏持有输入 grab = 全量输入 E2E 静默失败（T450 会话，
  2026-10-23）**：机器离桌被 away/锁屏拉起 xsecurelock（全屏密码框 +
  键盘/指针 active grab）→ 后续一切 xdotool XTest 输入被锁吞掉：pointer
  lock 6 连败、"Pointer captured" 永不出现、xterm 里 type 无反应、
  awesome client 键无效——**但** `xinput test` 设备层仍见事件（假阴性！）、
  窗口结构事件（map/focus/_NET_ACTIVE_WINDOW）全正常 → 极易被误诊为
  WM/几何/harness 问题（本次连环误诊：toggle 失败→focus 失步→菜单 grab）。
  杂字符还会漏进锁屏密码框（用户解锁前需 Backspace 清除）。诊断：
  **输入 E2E 连败先 `import -window root` 截图看屏面**（锁屏/黑屏/
  异桌面一眼可见）+ `pgrep -ax xsecurelock`；开批前断言锁不存在。
  附带事实：`xinput test` 只收**设备名**不收 ID（传 ID = usage 报错静默
  零输出）；XTest 假输入只在 "Virtual core XTEST keyboard/pointer" 设备
  流里可见（监听物理键盘设备 = 0 事件假阴性）；`xdotool search --name
  'awesome'` 命中 awesome 内部窗口（drawin 等 ~50+）≠ 菜单打开（勿据
  窗口数判菜单）。
- **本机 awesome 两行为漂移：floating 保留平铺几何 + client 键作用于
  WM 自身焦点记录（T450 会话，2026-10-23）**：① 窗口置 floating 时
  **保留当前（平铺）几何**（旧机 re-center 回 1280x720）→
  stabilize_window 循环每轮都看见非 1280x720 → 反复 toggle（其实每次都
  生效！679x744↔tiled 振荡）→ 15 次"did not reach stable floating"。
  修复 = toggle 后立即 `windowsize 1280 720` + `windowmove 0 60`
  （floating 则 resize 生效、循环收敛；tiled 则被忽略、下轮重试）。
  ② client 键（Super+ctrl+space = floating toggle）作用于 **awesome 内部
  焦点记录**而非 X 输入焦点：`xdotool windowfocus`（裸 XSetInputFocus）
  不更新 WM 记录 → toggle 打到陈旧/错误客户端 → 看似无效（全局键不受
  影响，任何焦点下都生效 = 判别器）。修复 = `windowactivate --sync`
  （EWMH _NET_ACTIVE_WINDOW 请求，WM 自更新记录）。口诀：**换机器先测
  "floating 是否保留几何"与"activate 是否更新 WM 焦点记录"两行为；
  toggle 看似无效先怀疑振荡（几何不变 ≠ 键没生效）与焦点记录失步（全局
  键有效 + client 键无效 = 记录失步指纹）。** 附带：游戏被 SIGTERM =
  干净 "Exit requested" 退出 + 写存档（非崩溃）；`--screenshot` 只在帧数
  到达时截图（SIGTERM 早退 = 无截图 = harness INVALID TAKE，即使存档
  数据有效）；lock_pointer 无 TM_LOCK_VERIFY 时点击不验证 capture
  （"LOCK 成功"≠ 已锁定，旋转未生效要查存档 states 而非信脚本输出）。
- **输入 E2E 批处理与并发重 CPU 互斥（T450 会话，2026-10-23）**：14 跑批
  与 fixer 的 `rm -rf build && ninja`（4 线程全占）+ 65 s 测试并跑 → 批
  前 3 项 14 拍全废（黑帧 ×8 + LOCK FAILED ×12；游戏 fps 2.0→~0.5，窗口
  状态机各步 xdotool/xwininfo 节奏在 CPU 饥饿下竞态失步）。同场景零争抢
  单跑全绿（view/wtag/mapstate 全程钉死 + 旋转生效 + 截图 0% 黑）。
  口诀：**输入 E2E 批 = 独占机；批跑期间禁 build/测试/重子代理；批与批
  之间留 CPU 冷却。** 附带事实：`xdotool windowmove --desktop N` 直写
  _NET_WM_DESKTOP 属性 = WM 不可见（窗口真实 tag 不动、属性说谎 → 后续
  按属性修视图 = 把视图拉离窗口真 tag = 永久 IsUnviewable 振荡）——换
  tag 用 EWMH 请求（windowactivate 自带视图跟随）或 wmctrl -t。SDL3
  X11 每游戏进程还建一个**无任何属性的 1280x720 辅助窗**（与主窗同位、
  无 name/WM_CLASS/_NET_WM_PID）= 覆盖判据命中 → 误 unmap 后游戏自身
  可见性状态紊乱（黑帧/IsUnviewable）——排除规则 = "游戏启动前不
  存在"（快照比对），class/pid 匹配对它无效。
- **i915/Mesa GLX 间歇黑相 + 三个连带假信号（T450 会话，2026-10-23）**：
  游戏 in-process 截图间歇性 100% 黑（同流程 5 连跑 3 绿 2 黑、分钟级
  相位周期），而 X 级窗口内容（import -window）看起来正常（picom 旧
  pixmap 缓存 = 具迷惑性）。已排除：CPU 争抢 / 窗口隐藏（marker 151/151
  IsViewable）/ 键盘指针 grab（276 采样全 free）/ DPMS（watchdog 强制
  Disabled 下仍黑；但有东西 <30 s 周期重开 DPMS，atb 已排除，身份未
  定）/ picom 实例（重启无效）/ 盖子（open）/ 背光（852/852）。**黑帧跑
  的输入与存档完全有效**（hcorr 跑：捕获 ✓ 旋转 states 1→2 ✓ 帧满 ✓）
  → 输入驱动跑按协议以**存档为确定性权威**：take 门放宽为"帧满
  （Frame limit reached）+ 存档存在"（截图黑帧只降级为支撑证据，记
  BLACK-SHOT save-authority）。连带假信号三则：① **LOCK FAILED = 场景
  init 失败假信号**——场景存档 format 错（format 14 缺 stairs / format 14
  带 states 键 = 版本谎报）→ 游戏 init 秒退 → 窗口销毁 → harness 对死窗
  xwininfo 查询全空（tm_geom 静默 return 1）→ stabilize/lock 全静默失败
  （无任何诊断行！）——**LOCK FAILED 且零诊断行 = 先查场景 init**（grep
  游戏日志 "Initialization failed"）。② 场景键纪律：format 15 必须有
  states 且禁 stairs 键；format 14 必须有 stairs 且禁 states 键
  （save.cpp 版本谎报拒绝）。③ 2fps 慢机的输入时长 = 墙钟 hold × fps →
  游戏内位移按帧缩放（60fps 设计的 2.5 s hold 在 2fps 只走 1/24 距离）；
  ×2 savenorm 含实体/玩家 pos 时，墙钟 hold 的帧数抖动 = ×2 天然不稳
  （60fps 机才稳）。口诀：**黑帧先信存档不信像素；LOCK FAILED 零诊断
  行先查场景 format；慢机 hold 时长按 fps 折算。**
- **`--config <不存在的文件>` = 游戏静默种子默认配置**（game.cpp init：
  `Config not found at '...'` INFO 一行 + 写入 `Config::defaultConfigJson()`
  = vd8/day240/scale1/无 mobs 段）→ 该路径此后所有跑全部用默认值。
  10-24 ENT-1 门夜坑：scratch config 生成 python 有语法错（关键字参数
  `f(x, mobs.limb_swing=False)` 不是合法 Python = SyntaxError，脚本无
  set -e 静默继续）→ config 从未创建 → 游戏种子默认 → **"on/off 对照组"
  实际两跑同为默认 on** → 像素 diff 恒 0 连错三轮 E2E（~40 个游戏跑
  全部无效）。判别特征：种子 config 的流送 = **197 列（vd8）** 而非
  12853（vd80）；日志有 "Config not found" 行。**纪律**：scratch config
  生成后**回读 assert 关键值**（vd/mobs/limb_swing）；每跑完 grep 日志
  "Config not found" = 种子哨兵；on/off 类对照先验证 off 侧真的 off
  （diff=0 且 savenorm EXACT 不等于"对照成立"，可能两组同配置）。
- **zsh `[[ "$x" == "*(1280, 720)*" ]]` 引号内 RHS = 字面串（不 glob）**
  → 尺寸检查恒 false（10-24 ENT-1 门 gate92c 整轮 8 跑全误判 "bad"
  重试，截图其实全绿）。正确写法 `[[ "$x" == *"(1280, 720)"* ]]`
  （通配符在引号外）。连带教训：`if a && b; then ...; fi` 之后
  `echo $?` = if 语句退出码（恒 0），**不能当失败原因诊断用**——
  失败分支里要分别测 `a`/`b`。
- **敌对 mob 生成保护圈 = 世界出生点 (0,0) 半径 12（normal）**——fresh
  世界玩家就站在世界出生点上 → 4-6 块生成环**整圈在保护圈内** → 敌对
  mob（spawn_max_light 0）永远 "found no valid cell"（被动 mob 豁免保护
  = 照样生成，极具迷惑性）。E2E 要生成敌对 mob 必须用**玩家距 (0,0) >
  16 块的场景**（ent2 场景 (22,38) = 44 块 ✓；fresh --new 不行）。
  保护半径在 core/difficulty.hpp（easy 8 / normal 12 / hard 16）。
- **场景存档的 `time` 字段覆盖 config 的 start_time_of_day**（load 时
  `m_dayCycle.setTimeOfDay(save.time)` 在 config 之后生效；`--time` CLI
  同样被压过）——给"白天存的场景"配夜间 config 无效（zombie 光门
  恒不过 = 上面保护圈坑的连带假象）。夜间 E2E 场景 = **编辑存档的
  time 字段**（0.75）而非改 config。判别特征：日志只有 "no valid cell"
  且全是 hostile pick = 先查 ① 场景 time 是否真夜间 ② 玩家是否在世界
  出生点保护圈内。
- **夜间（表面光 0）场景截图 = 世界区合法纯黑**（texture×light 0 =
  (0,0,0)），"全图黑像素 < 800k"的 notblack 门槛把合法夜景误诊为 GLX
  黑相 → 整轮重试浪费（10-25 ENT-3 门：s1a 有效截图被当黑帧弃 8 次）。
  判别器 = **HUD 带（底 100 px）恒绘制**（UI pass 不依赖光照）：GLX 黑相
  = 100% 纯黑含 HUD；合法夜景 = HUD 带非黑。黑相判定用双条件
  （全图黑 < 800k **或** HUD 带非黑 > 50k）。
- **E2E 台阶场景须绕开 P16 已记 quirk**：1 格厚（接近方向）台阶 +
  **从 +X/+Z 侧接近 = 踏不过**（front 格查 snapped+1，player/mob 共享
  行为，ENT-3 记档）——zombie 沿 +Z 追逐 1 格厚 barrier 恒卡（10-25
  ENT-3 门 s1a hp=20 真因：spawn 正确、step-up 被 quirk 吞）。解法 =
  barrier 用 **2 格厚**台阶（或 −X/−Z 侧接近）；断言 hp 前先看 DEBUG
  "mob spawn" 行确认 zombie 真的出生（info 级日志无 spawn 事件，
  全灭时易误判为生成问题）。
- **E2E 对照 config 会被静默篡改（limb_swing 消失 = off 变 on）**——10-25
  ENT-1 门：`gate93_day_off.json` 创建时含 `mobs.limb_swing=false`（回读
  assert 通过），数小时后键消失（文件仍在、其余内容原样）→ 所有 "off"
  跑实为 on → 摆动 on/off 像素 diff 恒 0 连错 ~6 轮（含死亡画面/夜间
  不可见等干扰项叠加，定位耗半天）。篡改者身份未定（游戏实证不改写
  config——1 帧跑前后字节一致；疑为并发脚本/手误覆盖）。**纪律**：
  on/off 类对照的 config = **每次跑前即时重建 + 回读断言关键键 +
  跑后再验一次**（gate93sw5 三验协议后一次通过 = 157,158 world px）。
  判别特征：on/off diff = 0 且两跑 savenorm EXACT 时，**先验 config
  内容再怀疑产品**。
- **E2E 生成环审计必须用完整生成条件；墙/高台顶永远是生成平台**——10-25
  门 94 三态场景三轮失效，根因链：① 审计按"石=不生成"跳过墙列 →
  zombie 生成在 3 高墙顶（level 41 + 头顶自然空气 ≥2 格 = **可生成**）
  被墙封死（三态全 hp20）；② 墙列石填到 y42 想"无头空" → **y42 之上
  是未编辑的自然空气**（补丁只清到 y42）→ 墙顶 level 43 仍是生成平台
  （zombie 又生成在 y43 墙顶）；③ 环格 d 审计用中心点 [5,6] → 实际
  生成位置带 ±0.4 jitter，**环边界要按 [min−0.5, max+0.5] 审计**
  （墙顶格中心 d=6.4 看似环外，jitter 后 6.0 环内实生成）。**纪律**：
  环内（扩边界）**任何** floor + ≥2 格头空 + 非流体 + 暗 + 保护圈外的
  格皆是生成候选——高台/墙顶无法靠"加高+封顶"消除（自然空气无限）→
  正确设计 = **环内只留水膜（不生成）+ 唯一干通道（预期生成格）**，
  不放任何墙/岛缘平台（岛格中心距须 < min−0.5 出环）；生成场景后
  **程序化枚举全部合法生成格并断言"皆在预期接近道"** 才跑。判别特征：
  探针终位全态相同且远离玩家 = 生成格选错，先查生成环再怀疑产品。
- **存档 blocks 段随墙钟漂移 ±13 格 = shipped 表面熔岩湖的真实 dt 火
  （非 worldgen 非确定性，P90 内容 + P64 火寿命）**——10-25 门 94：
  同二进制+同场景+无输入 600f 的存档 block diff 数 974/975/988 波动，
  波及场景外 chunk (1,1) 洞穴格。backtrace 实证写入者 =
  `FluidSim::tick → World::setBlock`（帧 0 点燃表面湖 (24,26)【demo 区
  内 shipped 内容】周边可燃物；火寿命 4.0 s = real dt → 600f 时火已燃尽
  与否随墙钟变 → live 世界在湖周边 ±13-14 格真实不同 → diff 不同）。
  判别实验：`fire.lifetime_seconds=9999` 对照 ×2 → blocks 段字节 EXACT
  （924/924）。generateChunk 本身纯（valueNoise3 逐格哈希，无跨调用
  状态；PIPE-2 流送写不走 setBlock 收口 = journal 无 worldgen 条目）。
  **纪律**：跨墙钟比存档（savenorm EXACT）的场景跑须 `fire.lifetime
  =9999`（或比对时剔除湖周边区）；`--save-full-diff`（legacy 全 diff）
  与 journal 路径在火活体期输出不同 = 预期，不是回归。
- **`--new` 必须配 `--save <slot>`；notblack 须 fail-safe**——10-25 门
  94 S1 首跑：base 门 fresh 跑漏 `--save` → "Argument error: --new
  requires --save" 静默退出无截图；notblack 的 python 对缺失文件抛异常
  → 无 stdout → `awk '{exit !($1==1)}'` 空输入**执行 0 行 = exit 0 = 假
  GREEN**（两拍"通过"实为全废）。纪律：截图判定先 `[[ -f $png ]]`；
  notblack 的 python 用 try/except 恒打印 0/1。
- **E2E 效率协议（门 94 定案）：场景跑前必过 `tm_scene_check.py`，黑相用
  1 帧探针判恢复**——10-25 门 94 实证：① S3 场景设计连错 3 轮（墙顶生成
  /岛缘生成/环 jitter），每轮烧 6 跑 ~30 min；`tm_scene_check.py`（/tmp/
  opencode，生成环全条件枚举【扩 d±0.5 jitter 边】+ 未编辑自然地形=潜在
  生成平台必 FAIL + 全生成格到玩家 BFS 可达性）对 3 个失败场景均能在
  秒级拦下（负例验证过）。② GLX 黑相期整帧重试太贵（900f 跑 60-110 s/
  拍 ×45 s 等待）：改 **1 帧探针跑**（~15 s/拍，截图非黑才发全帧数）——
  黑相 8 拍 ~20 min → ~6 min。标准库 = `/tmp/opencode/tm_run_lib.zsh`
  （run2/scenecheck/notblack/savenorm）。
- **init 失败与黑相不可区分（皆"无截图"）+ scratch 资产必须每门刷新**——10-25
  门 95：scratch 仓库资产未 rsync 刷新（无护甲物品 JSON）→ format 16 场景
  init 失败（"save 'player.armor.chest' holds unknown item 'iron_chestplate'"）
  → run() 按黑相重试 8 拍全废 + 拷的"save"实为场景文件本身（游戏 init 前
  退出未写存档，cp 的是跑前复制的场景）→ hp=20/pos=51 假证据误导判读。
  判别特征：save 的 player.pos = 场景预置值原样未动 + log 有
  "Initialization failed"。**纪律**：run() 每拍退出先
  `grep "Initialization failed" $log`（产品/配置/资产错误 = 重试无用，立即
  停）再判黑相；断言存档前验证其为游戏所写；每个门脚本头部
  `rsync -a --delete 主树/assets/ scratch/assets/`（gate94 有、gate95 漏 =
  本坑）。
- **跨清空的引用复制 = 物品凭空消失（dangling reference，门 95 护甲脱下
  路径）**——10-25：`Game::tryArmor` 脱下分支 `const ItemStack& worn =
  m_player->armorSlot(slot);` 先 `setArmorSlot(slot, {}, 0)` 清空、再
  `m_inventory.setSlot(m_selectedSlot, worn)`——引用指向的栈已被清空，
  写入手的是**空栈**，护甲件消失（装备→脱下往返后 armor={} 且手空，
  E2E 现场实证）。换槽分支是拷贝（`const ItemStack worn = ...`）所以没
  事；1421 单测全绿也漏了 = **tryArmor 交互路径零单测覆盖**（只测了解析
  /存档）。修复：逻辑上提纯函数 `item/armor.{hpp,cpp}`
  `armorRightClickUse(Player&, Inventory&, ItemRegistry&, selectedSlot) ->
  ArmorUseAction`（campfire 范式，Game::tryArmor = 薄适配层只放音效）+
  9 单测（含回归：脱下件必到选中槽）。**纪律**：凡"引用容器槽 → 改该槽
  → 用引用"的三连 = 必拷贝；交互类 Game 方法（右键通道/放置/进食）逻辑
  一律纯函数化上提，单测拥有决策+写入。**附带语义坑**：脱下到选中槽后
  下一次右键 = 手非空 = 重新穿上（震荡）——"连点脱整套"不成立，脱整套
  须每点之间换空热栏槽（热栏只有 **Tab 循环**无数字键直选——E2E 序列
  L,R,Tab,R,Tab,R,Tab,R）。
- **T450 输入送达协议（门 95 三轮定位）：窗口期 = IsViewable + 指针 top
  = 游戏窗 + windowfocus --sync，点击走位置、键走焦点**——10-25 门 95
  S3 输入全夜失效（e/u 4 跑 LOCK WARN 无送达）三轮定位：① `xdotool
  search --name tokenmine` 误匹配 **Firefox 窗口与终端**（title 含项目
  名）→ 永远 `tm_find_game_window $PID`（_NET_WM_PID 匹配）；游戏窗
  口真实 title = "TokenMine.cpp - 60 FPS"（非 "tokenmine"）。② 窗口
  可被 away 组件间歇 **unmap/重平铺**（XID 会变 = SDL 重建，Bad
  Drawable 实证）→ 轮询"窗口期"：`xwininfo` IsViewable + warp 窗心 +
  `tm_mouse_top`==WID，命中才发输入（**不做** stabilize/unmap_covers/
  切桌面 = 那些"修复"步骤本身会把 viewable 相位破坏掉——lock_pointer
  的 Super+N 切桌面在窗口就在当前桌面时 = 把视图切走）。③ **点击是
  位置事件、键是焦点事件**：焦点漂移时点击仍可达游戏而 Tab/数字键
  静默丢失（实证：sel 恒 0 而点击全生效）→ 每个键前
  `xdotool windowfocus --sync $WID` + 0.2 s。④ **SDL 指针捕获（相对
  模式）要求窗口有 X 焦点**：无焦点时 L 点击 grab 失败（WARN 被
  --log-level error 压掉）→ 捕获前必 focus --sync；捕获成功 = log
  "Pointer captured"（debug 级 = 验证抓手）。⑤ 捕获会被 away 重平铺
  **释放**（窗口 1280x720→1362x744 实证）→ 需要截图时 HUD 可见的
  跑（S4）= 早捕获 + 晚重捕（T0+16.5/19.0 s 各一次 phase-gated re-L；
  捕获中 re-L = ~0.1 s 挖掘点击释放即重置 = 无害）。⑥ 长跑 1200 f
  （20 s）给输入窗口期（240 f = 4 s 常错过相位）。**库**：
  `tm_input_seq`（/tmp/opencode/tm_e2e_lib.zsh）编码 ①③④。
- **zsh `(( 空变量 < n ))` = 0 恒真：黑帧检查静默放行（门 96 全批
  假 GREEN）**——10-25 晨：`nb=$(notblack $png)`（函数恒 rc 0 只打
  黑像素计数）后 `if (( nb < 10000 ))`——notblack 瞬时取空（并发负载
  下 python 启动抖动）时 nb="" → zsh 算术把空当 **0** → 黑帧判
  GREEN（门 96 首批 10 跑全"过"，事后单张计数 = 9 张全黑 921600）。
  内联形式 `(( $(notblack $f) < 10000 ))` 则直接 bad math expression
  （可察觉）。**纪律**：凡计数判据先 `[[ -n "$nb" ]] || fail` 再比
  大小（gate96_rerun2 的 count_ok 范式）；批末对每张 GREEN 截图补
  单张计数复核。
- **away 重平铺改窗尺寸 = 历史锚与跨时段 RAW 对失效（门 96 S1
  弃锚）**——10-25 晨：gate88 锚 = 679x744（当年平铺态），当前窗态
  1362x744（重平铺）→ 同世界不同投影 = 110k px "差异"全假象；跨
  时段两张截图（07:2x 1280x720 vs 10:0x 1362x744）亦不可 RAW 比。
  **纪律**：base 门锚尺寸与当前窗态不符时弃用历史锚，改**新旧二进制
  同窗态 A/B**（同分钟同窗态两跑 = 免疫的直接证明，比历史锚更强——
  门 96 S1' NEW vs v0.13.0 60f 字节一致即此法）；RAW 对要求同批同
  窗态（stabilize_window 收敛 1280x720 浮动后成对连发，中间不隔
  重平铺）。
- **1 帧探针 ≠ 整跑干净：黑相干净窗只有 2-5 s（门 96 晨波 3.5 h）**
  ——10-25 晨：GLX 黑相从 5-15 min 级退化为 2-5 s 级波动（干净窗
  恰够 1 帧探针、不够 5-25 s 整跑）——探针干净后整跑 25 s 仍黑
  （s2a 3 连败）；10:42 波隙拉长到 25 s+ 时 1500 f 整跑一次干净
  （s2e 配主批 s2b = 门 96 唯一 RAW 对成立时刻）。**纪律**：探针
  干净 = 立即整跑（不 sleep）；整跑曝光越短越好（600 f < 1500 f）；
  成对连发（第一张干净立即发第二张，趁同一波隙）；stabilize 收敛
  窗尺寸保证对可比；多轮预算 8-12 轮后停手 = 记入晨间补跑清单
  （S4 气泡排像素隔离即此例，脚本 gate96_s4_retry.zsh 已备）。
- **场景文件 format 升级 = 该版全部必需键都要补齐（门 97 首轮 6/7
  场景全废）**——10-25：门 97 场景从 format 16/17 基座复制后直接
  `d['format'] = 18`，漏补 format 17 引入的必需键 `player.oxygen` →
  全部 init 失败（`save format 17 requires a 'player.oxygen' integer`）
  ——更隐蔽的是**失败的 run 表现 = "未送达/无窗口"**（游戏 init 即
  退，窗口从未出现，20 分钟误诊为 away 组件/窗口相位问题；diag 枚举
  进程才发现 save 槽里是旧档）。**纪律**：场景生成器升级 format 时
  先 `grep` 该 format 的必需键清单（save.hpp 的 format 段文档）逐个
  补；E2E 跑前对场景文件先跑 1 帧 `--log-level info` 冒烟看
  "Initialization failed"（PITS 245 判别法的场景版）；"未送达"误诊
  前先看 run 日志有无 init 失败行。**10-25 第 2/3 次复现**：① mine
  场景 blocks 改写（stairs 格→air+石）后 `states` 条目残留（ENT-2
  朝向状态随块走）→ states 交叉校验失败；② scene_s97_hud_damaged
  fmt18 升级漏 `player.armor` 键。两者皆 init 静默死伪装"未送达"
  （窗口全程不存在，~40 min 误诊为 away 相位）。**纪律补**：场景块
  编辑改写必须同步扫 states/campfires/furnaces 段同格位置引用。
  **10-26 第 4 次复现**：门 104 场景注入 `d['blocks'] = [...]` **整体
  替换**（fire 块编辑被删）而 `fires` 段条目残留 → fires 交叉校验 init
  死，性能 run2 报 "black/stalled" 伪装 GLX 黑相 stall（~30 min 误诊 +
  8 输入 run 全空转）。**纪律再补**：场景 blocks 改写一律**追加**（保留
  原编辑），同步扫段 = states/campfires/furnaces/**fires**（fire 块编辑
  与 fires 段联动）；run2 "stalled" 先查 run 日志 init 行再归因窗。
- **X 负载下窗口定位 = `xdotool search --pid` 直搜，name-regex 阶段
  flaky（PITS 252）**——10-25 午后：3000f 长跑窗口活着且标题正则可
  匹配（单独探针 子串/`--pid`/`--class` 三法全命中），但
  `tm_find_game_window`（name-regex + _NET_WM_PID 双阶段）150 轮轮询
  全程找不到；同刻 `search --pid $GPID` 5 s 即中。name 阶段在 X
  负载下引入额外 flaky 面（xdotool search 超时/部分结果）。**纪律**：
  输入协议窗口定位统一 `xdotool search --pid $GPID | head -1`
  （PITS 247 更新；PID 是锚，窗口 XID 随 away 重平铺重建）。
- **away 组件窗口 churn：稳定窗分钟级、销毁-重建循环内 PID 直搜也
  间歇失手（PITS 253）**——10-25 15:3x-15:5x：门 98 输入跑 6 项
  （s2 ×6 等）全程未送达；诊断现场 = 游戏窗口在两次 X 查询（<1 s
  间隔）之间被销毁重建（XID 0x400057 前 IsViewable 后 No such
  window）——churn 循环比 PITS 249 的重平铺更激进（窗口反复生灭
  而非仅改尺寸）。**判别**：采集器多项连续"未送达"且 no-input 跑
  （S1/S6）同时段正常 → 窗口 churn 而非脚本缺陷。**处置**：输入项
  进补跑清单不硬卡发布（门 97 先例：单测级行为证明 + S1/S6 足证门内
  核心；输入 E2E 窗口恢复后 gate98_run.zsh 一键补收）。
- **E2E 场景 cp 到不存在的 save 目录 = 旧侧二进制静默 fresh 世界，
  A/B 变"两个不同世界"空对（PITS 254）**——10-25 门 101 首轮：
  `tm_v0170_run`（旧版运行目录）只有 assets 无 data/saves，场景存档
  `cp scene.json $OLDRUN/data/saves/xxx.json` 报 "No such file or
  directory" 后**继续执行**——旧二进制 `--save xxx` 找不到存档 =
  新建世界（出生点），4 拍截图全是 spawn 视角，与场景侧的 diff 无
  判别意义且**不报错**（cp 错误只在日志里，截图照常产出）。**纪律**：
  ① 旧侧运行目录初始化时 `mkdir -p data/saves`（gate101 脚本已修）；
  ② 场景 A/B 拍完先验**双侧存档 player.pos 都等于场景值**（读回
  savenorm 的 pos/yaw/pitch = 场景加载成功的权威信号，截图前断言）；
  ③ 对跑脚本里 cp 类前置操作失败必须 abort（`|| exit` / set -e
  语义），不得静默续跑。
- **夜场景的 time 只在存档里改，`--time` CLI 无效（PITS 255）**——10-25
  门 102 三轮探针失效：场景存档（120f 构建于 start 0.25 日）的
  `time` 字段 = 读档权威，`--time 0.75` 只改 config 起始值（buildScene
  前 set），被存档 time 覆盖 → 全程白天跑（现象 = 被动生成、敌对
  全灭：敌对 spawn_max_light 0 被光 15 门杀）。**纪律**：昼夜敏感
  场景构建后必须编辑存档 `time` 字段（savenorm 断言读回）。
- **敌对生成保护圈锚定世界出生点（非玩家）（PITS 256）**——Phase 25
  语义：hostile 在 `spawnProtectionRadius(difficulty)` 内（距
  **m_spawnPoint**）不生成，被动豁免（pass 0）。场景玩家站在世界出生
  点时，生成环 [4,6] 全在保护圈内 → 敌对永不生成且**无任何报错**
  （表现 = 只有 sheep/cow/chicken 刷）。**纪律**：需要敌对的场景
  玩家放 >12 格外（实测 (334,16) 高原 312 格 ✓）；"被动生成 + 敌对
  全灭" = 本坑信号（先于 PITS 255 的昼夜判断）。
- **E2E scratch 资产 rsync 漏同步 = 新特性静默失效且 init 不炸
  （PITS 257）**——10-25 门 102：ent1_dense 资产停在 GEN-4 同步点
  （无 wither.json / skeleton.json 无 arrow_effect），SURV-5 的 NEW
  侧用旧资产跑 = wither 链整段不存在但 init 通过（旧 skeleton.json
  无 arrow_effect = 无交叉校验对象）→ 探针三轮"hp 掉血无效果"误诊
  为接线 bug（实为数据缺失）。**纪律**：内容变更 phase 的 E2E 脚本
  开头必须 `rsync -a --delete assets/ $D/assets/`（与 GEN-4 门 100
  同款动作），并对 NEW 侧跑 1 帧 init 冒烟确认关键数据在档
  （如 `skeleton.arrow_effect == wither` 读文件断言）。
- **状态效果 tick/duration 是 real-dt——慢帧 E2E 环境的时标放大
  （PITS 258）**——10-25 门 102：GLX 黑相下 ~0.28 s/帧（3.5 fps），
  420f 跑 = **118 s 实时间**（非 420/60=7 s）；skeleton 箭间隔 90f
  = 25 s 实时间 > wither duration 10 s → wither 每箭之间**过期再重
  施加**（4 施 × 2 tick = 8 伤，帧基估算 1 tick 差 8×）。**纪律**：
  效果相关断言写"帧 ↔ 实时间"双界（60fps 与 3.5fps 下同判据 PASS），
  或断言"效果名存在 + hp 互斥"（勿断言精确 tick 数）；场景跑长 =
  实时间预算（timeout 按 0.3 s/帧 兜底，非 1/60）。
- **背景 retry 环存活判断 = pgrep 明文自匹配假象 + 并行直拍撞 slot
  （PITS 259）**——10-25 门 103：① `ps aux | grep -c "[g]ate103_retry"`
  返回 1 = **我这次 ps 自己的 `/bin/zsh -c` 命令行**（`[g]` 技巧只挡
  grep 自身，挡不住外层 `zsh -c` 把明文模式带进 cmdline）→ 误判 retry
  已死 75 min 后重启同款脚本 = 双环并行；② 双环 + 我手动直拍共用
  **同一 save slot（g103s2）+ 同一截图路径** → 三进程互删/互覆写帧
  （12KB 残帧 + 拍黑连环）。**纪律**：判活 = `pgrep -af <script>`
  后 `grep -v "zsh -c"`（或 `ps -o args= -p <pid>` 核对），pid 记进
  SESSION_CONTEXT；retry 运行期间**禁止**对同 slot/同路径手动直拍——
  收帧一律交给环（其 good() 门 + 黑帧 rm = 自愈）；撞车残帧由环的
  `good && continue` / 拍黑 rm 自动清，勿手动删环在途文件。
- **门 104 S2 输入协议五坑 + 场景几何时效（PITS 260）**——10-26 双工作台
  互斥输入跑（~3 h 调试定案）：① **持食物右键 = `tryEat` 吞点击**——右键链
  次序 campfire→eat→drink→launcher→milk→interact(table/furnace)→dye→place，
  持 beef（food=3）右键 = 进食通道 RETURN，面板永不打开（且随后 swing 取消
  通道 = 物品也不消耗 = "点击消失"假象）；**开面板右键必须持惰性物品**
  （coal；RCHG 调试日志实证：sel=0 held=beef → 无 openTablePanel，持 coal →
  panel=2）。② **窗口中心 = 隐藏 starter 槽 24**——ui.scale 1.5 下面板布局
  巨大，屏中心 (640,360) = UI 空间 (427,240) = 存储 row1 col6 = slot 24
  （oak_sapling）；"幂等双 R"的第二发右键（面板开、光标仍在中心）=
  rightClick(slot24) 携走 1 苗 → 污染全部后续断言（SWAPWATCH 实证
  苗 3→2 + slot0 内容被换）；**面板开时任何光标位置的点击都是槽操作**，
  后续操作前先把光标移到面板外安全区（窗角 (10,10) 必在板外：panel UI
  原点 (44,126)）。③ **UI 空间换算**——`uiMouse()` = 鼠标/uiScale；
  `InventoryLayout::compute` 在 UI 空间 = round(窗/uiScale)（1280×720/1.5 =
  853×480）进行；屏点击位 = 布局 rect × uiScale。布局探针必须传 窗/1.5
  尺寸、结果 ×1.5 映回屏（rect 探针实证 slotAt(83,316)=0）。④ **xdotool
  click 偶发双发 press-release 对**（BTNDBG 实证 1 click = 2 pair）→ 跨帧
  两次 wasPressed = 槽位"拾取+放回"toggle 净零；**全部点击用
  mousedown/sleep 0.3-0.4/mouseup 拆分**。⑤ **焦点 blip 释放 relative
  mode**——SDL 失焦自动解锁；L（捕获）与 R 之间焦点被偷 → R 在解锁态被
  `!locked` 分支 early-return 忽略（未锁定右键 = 无世界交互）；协议 =
  L → `windowfocus --sync` 重断 → L2（幂等：失焦=重捕获、已锁=swing
  无害）→ R。**终存 yaw/pitch = 锁定态探测器**：锁定态 mousemove = 相机
  旋转 → yaw 漂移/pitch 撞 clamp = 面板从未开；面板开 = 指针释放 = 零旋转。
  另两坑：⑥ **场景几何时效**——P59/P92/P97 时代"证明高度 y45/46"常量是
  **GEN-1 前无洞穴世界**的；现世界 (21,-2) h=38（顶 39）、(22,-4) h=37
  （顶 38），注入玩家/方块/掉落必须**每场景用 surfaceHeight 探针重验
  实际地形**（否则玩家坠落 7 块、目标悬浮头顶、射线全 miss，曾误诊 ~30
  min）；⑦ **zsh 工具 shell 后台 job setgid flake**——`failed to change
  group ID: 不允许的操作` 偶发致命（函数内/顶层/sh launcher 均中过）→
  可靠模式 = 顶层 `( cd $D && $bin ... & echo $! > pid; wait ) &`（smoke
  6/6 验证）+ 每 slot 独立脚本 + 顺序 driver（g104_run_slot.zsh +
  gate104_s2_driver.zsh）。
- **出生点无输入跑永不流送（PITS 261）**——10-26 门 109 首轮 S2：雾效
  A/B 用 `--new` 出生点场景 1500f ×2，on/off diff=0 误判"雾未生效"。
  根因 = 流送惰性触发（`kStreamTriggerDistance=1`：玩家列须在 missing
  列 1 列内——`columnDistanceSq ≤ 1`，对角 d²=2 不触发）+ 初始 4×4
  demo 区（kDemoChunkMin/Max = -2..1）出生点恒在中心 = 离 missing 列 2
  列 → 无输入跑世界**恒 16 列 64 块**（任何 view_distance 同；vd80
  base18 门 108 次过门看到的"世界边缘" = demo 区边缘，不是流送边缘）。
  判别 = info 日志：有 `Chunk streaming engaged` / `Streaming:
  N/M columns` / `Streaming complete: N/N columns` 行 = 流送在跑；
  全无 = 未 engaged（静默，不是卡死——"silence in the log = the disk
  is full, not a stall" 仅对已 engaged 成立）。修正 = 场景存档把玩家
  放 demo 区**列 1**（x∈[16,32) 或 z∈[16,32)，邻 missing 列 → 帧 0
  engaged；vd8 盘 197 列 ~510f 填满）。**凡验证远视距/流送边缘效果的
  E2E 场景（雾/远山/边缘渲染）都必须用触发位场景，不能用 --new 出生
  点**。次坑同场：demo 区列 1 的低地盆地（种子 123456789 的 (24,8)
  区）填满后流体 churn 拉低 fps ~12.5（1500f > 120 s real）→ `timeout`
  须 ≥300 s（被杀的 run 存档照写但无截图 = 假完成）。
  **次坑 A（场景雾带不可见）**：低地相机（种子 123456789 的 (24,8) 盆地
  y35）看 -Z 时，vd4 雾带 48-64 块 / vd8 雾带 96-128 块全部被近处高原
  （y53，40-60 块）+ 树冠遮挡 → on/off diff=0 三连误判"雾未生效"（vd4
  场景 253k px 的"差异" = 流送积分状态差，非雾）。**远视距效果 A/B 的
  雾带必须含可见几何**：出生点高原相机（y54）看 -Z 的 demo 区北缘
  （32.5 块）在 vd2 雾带 24-32 内 = 实证 diff ~9676 px。**先跑 1 对
  300f 探针核 diff>0 再上 6 帧批**。
  **次坑 B（流送积分实时锁定）**：流送预算帧锁定（2 列/帧 post）但
  worker 完成实时锁定（CPU 争用时 flush 等不到 job → 集成延后帧）→
  **帧 N 的世界状态随机器负载漂移**（同场景三次跑两 X 一 Y = 253k px
  差，Y = 未收敛态）。远视距/边缘效果的 A/B 只允许两种世界态：
  ① 静态态（出生点无输入 = 流送永不 engaged，PITS 261 主坑）或 ② 收敛
  态（"Streaming complete: N/N" 日志 + ×2 字节一致双验）；禁止用
  半流送态做跨 run 断言。
  **次坑 C（--config 相对路径）**：探针命令里 `--config $cfg` 用相对
  路径 + `( cd $D && ... )` = 文件不在 $D → 静默回落内置默认 config
  （PITS 252 复现第 2 次）→ 两侧同 default = on/off diff=0 假阴性。
  config 路径一律绝对。
- **天然熔岩湖首次流体 tick 点燃草 rim = real-dt 非定帧源（PITS 262）**——10-26 门 110
  S2 场景：种子 123456789 demo 区 (x 23..25, z 25..27) 3×3×2 天然熔岩湖，首次
  FluidSim tick 即被 P58 燃烧 pass 点燃 4 格草 rim 成 fire 块；火寿命 = 真实 dt
  生存计时（PITS 244）→ 燃尽帧不定 → **含该区域的无输入 ×2 RAW 场景跨 run 火状态
  漂移**（blocks 差 = 木→air 烧损位置集不同）。除源两范式：① 场景存档石块填充湖格
  （块编辑驱离流体，存档往返干净 = PRE-3 场景用法）② fires 大 remaining（g95_fire9999
  config 范式）。设计无输入判别场景先扫 demo 区熔岩湖。

- **real-dt 效果时间带断言在低 fps 流送 run 下跨带失效（PITS 263）**——10-26
  门 98 补跑：600f 场景在 vd80 流送填充期实测 ~9.5 fps = 实时间 ≈63 s（帧
  时间 10 s）；weakness 90 s 的 remaining 判别带（>82 = "刚施加"）按帧时间
  设计，实际终档 remaining 32.6（已倒计时 57 s 实时间）= 带外误判 FAIL。
  修复纪律：存活类断言改用**施加事实互斥**（实验侧有效果/返还/消耗 vs 对照
  侧全零）而非精确 remaining 带；确需时间带的，run 帧数须按实测 fps 折算
  实时间（或场景去流送负载 = demo 区内 + 小盘）。s2b 同款：1200f ≈126 s
  实时间 → 双饮后 90 s 效果必过期，RESET 窗口 [84.5,90] 不可达（行为层
  test_food 单测兜底）。

- **场景缺 format 必需键 = init 静默死伪装"输入未送达"（PITS 264，PITS 251 变体）**
  ——10-26 弓补跑：scene_s97_bow.json（format 18）缺 `player.armor` 键（16+
  必需）→ 每次运行 init 立即失败零窗口，断言读到的"存档" = 未改动的场景
  副本（"NO 0 3" 假信号）——10-25 起 8 次"相位未送达" + 10-26 16 次重试
  全因同一 init 死，误诊为窗口相位 churn。判别：输入跑连续 delivered=0 或
  断言值恒等于场景初始值时，先查 log 的 "Initialization failed" + 冒烟
  5f 可载（无输入）再怪相位；场景格式升级后须全量核补跑场景的必需键
  （本次批量核 24 场景仅弓一处缺）。

- **无输入 60f 锚的远云带 = 机器负载态相关（PITS 265）**——10-26 门 114
  前发现：base18（15:4x/17:2x 拍）含 20235 云像素（顶带 y[0..70]），
  19:4x 起同 binary 同配置 fresh 60f 云像素 = 0，云带外逐像素全同。
  根因 = spawn 视锥顶带切云平面 d∈[139,192] 窄环，该距离的高原剪影
  在帧外（y<0 上方）但遮挡云环：vd80 流送在 60f 时是否加载 d~140 环
  取决于机器速度（调试日志拖慢 fps → 未加载 → 云可见；满速 60fps →
  已加载 → 云被遮）。判别 = `--log-level debug` 60f 跑云像素 20235 vs
  error 级 0 互斥实证。**纪律**：无输入锚对云带只做连续性参考，核心
  断言 = 同批同机 new≡old A/B 字节一致（不受机器态影响）；跨时段锚
  失配先查云带 + debug 对照，勿误判为渲染回归。

- **`xdotool key --window` = XSendEvent 合成事件，SDL 忽略（PITS 266）**
  ——10-26 门 114 S2b：`xdotool key --window $WID E`（XSendEvent，
  send_event=True）多次送达但面板从未开；当时的"etest 成功"（暗区
  48917）= 世界林荫阴影假阳性（面板根本没开，误判协议有效）。正确
  送达 = `xdotool windowfocus --sync $WID` + **裸** `xdotool key E`
  （XTEST 伪输入 = 真事件外观，与鼠标 mousedown 同原理）。配套坑：
  ① "Pointer captured" 锁定信号是 MC_LOG_DEBUG 级，`--log-level info`
  跑输入协议 = 点击重试循环盲试（16 连点击零信号实证）；② 面板签名
  断言用中心像素 (681,372) ≈ (27,27,33) kPanel 暗底 + 槽区内容判别，
  勿用大面积暗区计数（世界暗区 48k px 量级假阳性）。

- **L8-on-L8 stable 不横向扩散（PITS 267）**——10-26 门 115 S2 场景
  设计：2 深池（水 L8 y41,42）顶层水面**永不膜水**——流体规则只有
  "块支撑"的 L8 才向同层空气邻居扩散（supported-spread），叠在同类
  流体上的 L8 = Stable no-op。首版 A 场景用"池缘 1 行膜水"作入水
  入口静默失效（膜根本铺不开），探针症状 = mob 终态非预期，100f
  存档 fluids 审计才现形。设计判别入口前先 fluids 审计（5f 存档看
  实际流体格），别靠推演流体铺展。
- **AI 停点 = 玩家 + attack_range 沿路径（PITS 268）**——10-26 门 115
  S2 场景 A：zombie 攻击范围 1.6 使其在**崖缘**停住远程打玩家、永不
  进池（4 跑 old==new 全同 = 零判别，误诊为场景未生效）。判别场景
  的"入水/出水"动作必须发生在停点**内侧**（玩家位置往池内/出池侧
  移，使停点跨进判别区）；停点 = 探针末行常量，先探针定几何再长跑。
  关联：1 步崖下池入水瞬间身体尾仍压崖缘 floor → fresh
  isRestingOnSolid=TRUE → 行走 mob 物理到不了"干身踩水面"终态
  （仅击退上抛/预置可达）——设计"干身悬浮"判别须用预置/击退路径。
- **玩家受击红闪 = 全帧 real-dt 着色（PITS 269）**——10-26 门 115
  S2 截图判读：同场景两侧末帧 a_old 全帧 (86,37,72) 紫红 vs a_new
  (24,50,99) 夜蓝，初判"昼夜不同"实为**受击红闪**（real dt 衰减，
  末帧仍在闪窗内）——old 侧 zombie 池底涉水攻击玩家 5 次、new 侧
  游泳位 dy=1.1 超攻击垂直门零击。含 AI 攻击的 A/B 跑，末帧截图
  颜色差先核玩家 hp/受击史再判渲染；红闪存在本身可当攻击模式
  侧证（b_old 无闪 hp20 vs b_new 强红 hp11 = 出池攻击互斥的像素
  侧证）。

- **scratch 树 rsync `--exclude data` 连带排除 `assets/data`（PITS 270）**
  ——10-27 门 115 性能批：建 tm_v0320_src 用 `rsync -a --exclude data`
  （本意 = 排除存档目录），`--exclude data` 匹配**任意深度**同名目录
  → `assets/data/**`（71 块 JSON + 物品/配方/mob/音效布局 +
  block_atlas.json）整层缺失 → 每次 init 即死（日志有
  "Block directory not found" + "cannot read asset file" ERROR），
  但 run2 的失败分类只看"截图黑 + swap 不干净"= **4 轮 ×240 s 超时
  全误判 black/stalled**，真死因埋在 log 尾。判别：性能/场景跑连续
  try 全 black/stalled 且 log 无 STEP0 行 = 先 `tail log` 查
  Initialization failed（PITS 264 同款"init 死伪装"家族）；建
  scratch 树后必跑 5f 冒烟（`--new --frames 5`，exit 0 才算树可用）。

- **99.8 活动门的流送边界活性缺口（PITS 271）**——10-27 STRM-2 门 116
  场景首触发：settled（冷）流前对着 void 的 spread 被 `World::setFluid`
  拒（"the void stays dry"）后该 tick 零写入 → chunk 转冷；目标列
  流送集成（`insertChunk`）不触碰 fluidWalkPending → 流前**永久**卡在
  流送边界（门 116 场景插桩实证：熔岩级联 tick 4 被拒转冷，目标列
  frame ~15 已集成，其后 395 tick 零推进；spread 探针全局只触发 1
  次）。CellReader 把缺失 chunk 读作 air = "邻居看着是空的但目标不
  存在"，表面 spread 条件全满足而写入静默失败——排障必须直接探针
  `setFluid` 的拒绝分支（`no chunk`），不能只信 CellReader 侧读值。
  修 = `insertChunk`/`getOrCreateChunk` 镜像 setBlock 的 UNSTICK
  invalidate 范式（`wakeFluidChunks`：自身+6 面邻中持有该 kind 的
  chunk 各走一次；唤醒仅在插入事件 = 99.8 性能面零回退，判别单测
  FrontierCrossesTheStreamingBoundary pre-fix FAIL 实证 + 反向
  GeneratedSeaFillsTheDryDepression + SettledWorldStaysColdWithoutInsertions）。
- **帧锁定物理 × 低真实帧率 = 墙钟输入计时全部失真（PITS 272）**——
  10-27 门 116：本机 Mesa 软渲染 ~11-13 fps（vd8 场景海岸 churn 重时）
  至 ~50 fps（流送完静置），1 帧 = 1/60 s 游戏时间 = 0.08-0.09 s
  墙钟；按 60fps 假设排的"t=55s 松键"只走了 34 块（83 块需求），
  步行跑端点系统性短程。步行类输入跑的转向/终点判定必须**位置探针
  化**（F5 中途存档读 player pos——F5 代码明载"work in any pointer
  state"，任何指针态可用；存档 = 确定性权威同款），不能按墙钟偏移
  排布；--frames 预算按最坏 fps（11）算（1f≈0.09s 墙钟）。
- **away 组件换窗 XID = 缓存 WID 静默失效（PITS 273）**——10-27 门
  116 首轮：夜间 away 组件 unmap+recreate 游戏窗（PID 锚不变、XID
  变），持有旧 WID 的点击序列 30 连击全盲（02:52 窗内 3 轮 attempt
  全败，02:56 手工同序列首击即中 = 窗外相位）；且 master 流程漏了
  g104 范式的 **TM_PRE_WINDOWS 前置快照** → `unmap_covers` 的"新建窗
  永不动"保护失效（空 pre 列表 = 守卫短路反向）→ 游戏 SDL3 X11
  helper 窗（无 _NET_WM_PID、全尺寸）被 unmap = 主窗 orphaned = 点击
  永不到达（lib 注释 probe4/5 同款失效链）。输入协议补强：acquire
  每次迭代按 PID 重解 WID（tm_find_game_window）+ 周期性重稳定化 +
  几何不可读时跳击；跑前必建 TM_PRE_WINDOWS 快照 + 取 TM_GAME_CLASS。
- **`--new` 必须带 `--save <slot>`（PITS 274）**——10-27 门 116 S1
  首跑 rc=2："Argument error: --new requires --save <slot>"。S1 无输
  入截图跑加 --save 槽即可（退出写档无害，槽文件跑前 rm）。
- **pkill 输入跑 = X server 键态残留（PITS 275）**——10-27 门 116：
  walk 跑（keydown w）被 pkill 时"退出后 keyup"清理未执行 → W 键在
  X server 保持按下 → 下一个"无输入"冒烟静默变成步行跑（600f 无输入
  实证：玩家从 x=110 自走 45.7 块到洼地停下——恰好 = 上一跑端点；
  `xdotool getkeydowns` 清键后复跑零漂移）。纪律：① 每跑前
  `xdotool keyup w W a s d`（未按键 = no-op 安全）；② 杀输入跑后立
  即清键；③ "无输入"冒烟出现位移 = 先查 getkeydowns 再查物理。
- **场景桥/路面高度必须按"实际接近侧"定（PITS 276）**——10-27 门 116：
  洼地桥首轮 y=30（top 31）只核了西口（x=63 top 30，+1 阶），忘了
  玩家自东涉水而来（东口 x=74 脚位 28 → +3 不可上）= 整跑（9 min）
  白走、端点卡洼地 x=64.3。判别 = 跑后核端点 x 是否过桥（过桥判据
  x < 64）；修法 = 桥面降到接近侧脚位 +1（y=28 top 29，东 +1 西 3 降）。
- **玩家无法从 1 深水中 step-up 上岸（PITS 277）**——10-27 门 116 东
  出口 6 轮卡死（x=45.7/43.7/48.7/49.7 历次变体）：player.cpp:349
  stepUp 门 `!m_crouching && m_onGround`；1 深水 = 泳态（atSurface 0.4
  带外：脚位=底、水顶=底+1，Δ1.0）= FLUID-2 snap 脚位恒 = 水顶−0.9
  悬底 0.9 → onGround 假 → stepUp 永不触发 → **任何岸顶 > 脚位即墙**
  （+0.9 也是墙：moveAxis 全高碰撞，无"矮墙可挤"）。出水设计纪律：
  平底/河床缓升（地升推升 ≤0.9/步无门控）至岸顶 ≤ 泳脚位 = 平出，
  之后**旱地**再阶梯（每级 +1 平台宽 ≥2 = P16 宽 1 +X 阶免疫）。
  FLUID-3"涉水 mob 踏 1 格岸壁出池"= mob stepUp（ENT-3 同门已解禁），
  不适用于 player。
- **场景降顶/挖格前必须垂直剖面探针（PITS 278）**——10-27 门 116：
  x=49 顶 37 的"悬浮草"下是 (49,33..35) 3 格空腔（底 32 沙，GEN-1
  洞穴啃顶残留）：挖草（以为把顶从 37 降到 36）= 顶落至 33 成 3 深
  坑，玩家坠入卡死（实证 y=33 蠕动 8 min）。surface 探针只给 top，
  抓不到悬浮/空腔——降顶/挖格前打印该列 y-4..y+2 剖面；填洞 = 石填
  腔 + 挖顶（x=49：填 (49,34)(49,35) + 挖 (49,36)）。同类：x=48 全
  线 1 宽 6 深壕（水平探针才见）。
- **转向后的 z 漂移 + 甲板车道宽度（PITS 279）**——10-27 门 116 Run C：
  XTEST 180° 转向后玩家 z 自 0.5 漂至 −0.4..−1.3（转向事件类，非
  yaw 误差——yaw 实测仅偏 0.09°；西相 83 块涉水+甲板 z 恒 0.5 排除
  行走漂移）→ 单行（z=0）甲板 = 玩家漂出车道落回自然地形（宽 1 阶
  P16 卡死）。修法 = 步行基础设施按 z 三行（−1..+1）铺设（吸收
  ±1.0 偏移，AABB 半宽 0.3）+ 转向后 F5 探针读 yaw 精调（|Δ|>30px
  才补 xmotion，实测免纠）。漂移根因未追（XTEST 事件/SDL 锁 delta
  类），三行车道 = 结构性免疫于该漂移量级。

- **scratch 树漏 data/saves → 场景拷贝静默失败 = fresh 假绿（PITS 280）**
  ——10-27 PIPE-3 step-0：插桩树 rsync 用根级 `--exclude '/data'`，树内无
  data/saves/；run2 的 `cp /tmp/opencode/$scene $D/data/saves/...` 报
  "没有那个文件或目录"后继续（cp 失败非致命）→ --save 读不到场景 =
  FRESH 跑，截图照常 GREEN = 假绿（PIPE3-S0 数据仍"合理"掩盖）。判别 =
  场景跑后存档块/流体编辑计数 vs 场景源 + 跑前 `ls $D/data/saves/` 核目
  录存在。纪律：建 scratch 树后先 1 帧冒烟核场景已载（存档含场景特有
  编辑）再长跑。
- **pgrep -f 自匹配 = 等待循环永挂（PITS 259 第 4 次复现，PITS 281）**
  ——10-27 门 117：`while pgrep -f 'g94_old_fall_t2'; do sleep 10; done`
  的 shell 命令行自身含该字符串 → pgrep 恒匹配自身 → 300s 超时（目标
  进程早已 clean exit）。模式必锚定二进制/特征（`pgrep -f
  'tokenmine.*g94_old_fall'`）或 [g] 括号技巧；等待循环先核产物 mtime，
  不裸等 pgrep。
- **away 组件黑窗态 = 数分钟周期（PITS 273 扩展，PITS 282）**——10-27
  门 117 S2：同场景同帧同二进制，07:30 截图亮（mean 0.436）/ 08:07-
  08:22 连 4 跑全黑（mean 0.25，22KB 近单色 PNG）= 非场景相位（帧锁定
  确定性排除）= 环境窗态周期波动（亮窗 ~2-12 min）。黑帧跑**帧照样
  完成**（clean exit + 存档完整 = 确定性权威不受影响）→ 门协议 = 黑跑
  存档照收 + 像素检查降级 SKIPPED（同场景双侧皆亮才做像素 RAW）+ 长跑
  批前亮窗热身探针（60f，黑则 20s 重试）；黑帧判别先 `grep 'Frame
  limit reached' log`（帧完 = 黑帧非 stall，别按 240s 超时白等）。
- **fire 蔓延终区 = real-dt burnout × 帧锁定 roll 的非位稳类（PITS
  283）**——10-27 门 117 S2：cascade 场景（fire lifetime 4 s 默认）600f
  无输入 OLD vs NEW 块日差别差 = 恰 1 格 fire-spread 燃尽 air；**同 fps
  （11.8-12.1）同 OLD 二进制 3 跑亦 1 vs 2 分歧** = 非回归、非 fps 均值
  相关 = burnout 帧（real-dt 4 s 倒计时 × 逐帧 real-dt 抖动）迁移 → 帧
  锁定 spread roll 序列从分歧帧起重放不同 → 燃尽 air 足迹差 1-2 格。
  判别：跨侧块差先核差格邻域是否 fire 燃尽簇（air 密集区 + 场景 fire
  条目半径内）+ 同二进制 ×3 跑自分歧实证。位级 oracle 场景纪律 = fire
  生命周期出档的（lifetime 9999 s = 无 burnout = spread 纯帧锁定位稳，
  fall 场景范式）或 spread_chance 0（burnout 原位确定性，casc nospread
  范式）；savenorm 剥 fires[].remaining 不够——燃尽 air 在 **blocks**
  段。PITS 244 衍生（244 = fires remaining 漂移记档非缺陷，本条 = 其块
  足迹面）。

- **门像素带断言 = 任意像素行 ≠ 语义边界（PITS 284）**——10-27 门 118：
  以 y[381,655] 为"世界带"断言 diff 隔离，474,616 px "出带"报警——实为
  近景树冠剪影越过地平线进上带 + 底部带含热栏块立方图标（basic 程序
  按设计跟世界光）+ 远景雾面山体；蓝天 clear（(5,8,20) 午夜 /
  (182,86,46) 黄昏）实测零触碰。修正 = 隔离论证构造化（git diff 仅
  basic.frag ⇒ UI/sky 程序/clear/星云[light 恒 0] 结构性不变）+ 位置化
  open-sky 锚盒（两场景先验证恒等的蓝天点 ± 邻域，避开树冠带 y[128,
  139] 类剪影）。方向断言同理先行数学：深夜 max(stored−8, block) ≥
  max(0, block) = 仅亮化可证；黄昏 v>8/15 像素亮化、v<8/15 变暗，
  视图无深荫像素时 darker=0 = 合法非 bug。
- **scratch 树无 build 目录 + 驱动指 $D/build = 启动即死无告警（PITS 285）**——10-27 门 118 首轮 13 跑全 NO-CLEAN-EXIT：rsync 排除 /build
  的资产树被驱动当二进制树用（timeout 报"无法运行命令"埋在 per-run
  log，驱动只 grep 主日志）。纪律：门驱动的二进制 = 冻结 bin 路径
  （tm_vNNN_bin，与资产树解耦），资产树只作 cwd/assets；另 zsh f-string
  拼版本号易错（tm_v132_bin 式 = 13+2 拼接幻觉）——bin 路径用完整字面
  量。
- **黑帧/平面窗判别必须用 PNG 字节尺寸，mean 探针不可信（PITS 286）**——
  10-27 门 119：环境窗口态周期性把截图打成全画面均色（22106 B 压缩极小），
  均色值不固定（实测 mean 0.25 与 0.54 两变体）→ `fx:mean > 0.1` 探针假
  BRIGHT 放行，整批 6 跑 4 黑。有效场景帧 = 300-430 KB（60f fresh 387726 /
  day 信标 426729 / night 信标 403566）。修正 = 每跑后 `stat -c%s` 尺寸门
  （<100 KB 判 ENV-FLAT，45 s 重试），断言器输入同样先验尺寸。窗态时标
  分钟级且批内可混（同一 4 跑批 valid/env 交替出现），重试循环是唯一
  可靠路径。衍生：长墙钟跑（构建负载压低 fps 至 ~5，600f = 120 s）触发
  36 s 饥饿 real-dt tick + beacon 吸收效果倒计时 → savenorm 扩展剥离
  `player.hunger` + `player.effects[].remaining`（硬规则 8 real-dt 类，
  世界状态 blocks/fluids 不受影响）。
- **像素门先跑 on/off sanity 验取景，否则"恒等/零 diff"双假 PASS（PITS 287）**——
  10-27 LGT-4 门 119：信标 beam 柱 96 块高自块顶面起（近天顶），场景相机
  pitch 0.15（视锥 −26°..+44°）根本取景不到（beam 在 58°..87°）→ day
  "df=1 恒等"与 night "A/B 零 diff"双双失明假绿；同二进制 beacon.beam
  on/off 帧 diff=0 才是取景有效的判据（old/new 双二进制 on/off 均 0 =
  场景盲，非代码）。排查辅助链：GPU uniform readback
  （glGetUniform*，GLEW 2.2 下经 __glew* 函数指针 + 3 参核心形
  (program,loc,params)——2 参调用编译错"实参太少"= 形参表陷阱）证
  CPU→GPU 链路通，再怀疑取景/语义。beam 类（及一切"高出屏幕的大物体"）
  门场景先目检截图里有没有目标。
- **场景存档构建必须合并 base 块编辑（覆盖 = 孤儿 fires 交叉校验死，PITS
  288）**——10-27 门 120 首轮：场景脚本 `d['blocks'] = 房间编辑` 覆盖了
  fresh base 的 6 条 runtime 编辑（P90 湖火 2 fire + 石 1 + …），而
  fires[] 3 条目保留 → init 报 "save 'fires' has an entry at (23,35,28)
  which is not a fire block in the world's block edits" = 场景静默未加载
  伪装成跑挂。修正 = 合并（查重冲突即报错）+ 驱动侧场景生效校验（blocks
  数 + player pos 断言，PITS 254 场景 cp 静默 fresh 坑的第三变体：cp 通
  过但内容被脚本破坏）。
- **init 失败后旧产物残留 = 假 PASS（场景探针必看 ERROR + clean exit +
  真终档，PITS 289）**——10-27 门 121 首轮探针 v2：场景文件自身重复
  编辑（悬台格在井 air 循环内 = air+stone 双条）→ init "duplicate block
  edits" 死，但驱动只查"player pos 对不对"——读的是 **cp 进去的场景
  文件**（含预期 pos）而非游戏终档 + 截图是**上一轮 v1 场景的残留
  PNG** → 全绿假象。纪律：场景跑验证 = ① 日志 grep ERROR（init 死）
  ② 'Game shut down cleanly' ③ 游戏**退出后写的**终档（rm 场景槽先，
  再 cp 场景，跑完读新写的档；PITS 15 家族）④ 截图 stat mtime > 跑前
  时间戳。四者缺一 = 假跑。
- **平滑噪声场上局部峰特征检测失效（lround 平顶，PITS 290）**——10-27
  GEN-8 预备：山峰互感首版设计 = baseHeightAt 严格 3×3 局部最大值做
  峰检测。shipped 高度噪声 scale 0.02 极平滑，fbm2 值经 lround 整数化
  后穹顶顶是 5-10 格宽的**平顶**（等值格环），严格局部最大恒不成立——
  probe 三 seed 123456789/9/42 各 64×64：零峰/一峰/零峰；放宽 5×5
  apex 规则（ring-1 全 ≥ 且非严格时 ring-2 全 >）仍救不回（平顶直径
  5-6 = ring-2 内仍有等值格）。教训：特征检测（峰/谷/脊）在平滑噪声
  场不要用最终高度场的严格极值——用**独立低频掩膜场**（GEN-8 定稿
  路线：fbm2 @0.008 + 专用 salt + 阈值）或显式宽容 apex 规则，且
  **probe 先行**（改设计前用独立 C++ 驱动实测特征计数，勿凭噪声理论
  分布推断"峰应该存在"）。
- **场景 L8 水源必须 4 石封，否则流动毁场景（耕地湿润 S2 两连坑，
  PITS 291）**——10-28 GEN-9 门 123：本项目 L8 源**非 MC 静态源**，向
  4 水平邻流动（level 7,6,5…）。大场景首版水 (26,37,-4) 不封 → 沿坡
  西流淹相机-耕地走廊（水面与耕地顶面同高 = A/B diff 0 假象）；受控
  平台场景 240f 实测扩散 71 格、最近到距干耕地 3 格（= 对照耕地被
  意外湿化，隔离断言失效）。封 = 4 水平邻石（P64 高架水 / P75 浇灭
  场景同款先例）。纪律：任何"近耕地/作物/光源"的场景水一律封源，
  收档后断言场景格流体 = 恰 1 源（level 8）。
- **大场景远景湿耕地顶面 GL 不更新（新旧二进制同现 quirk，门改受控
  场景，PITS 292）**——10-28 GEN-9：自然地形场景（相机 8.5 块侧视
  demo 列 + 走廊挖通）中湿耕地顶面恒不渲染：CPU mesh 临时日志证湿
  quad 已发射（tile (9,4) ×2）+ 世界 dump 证视线盒 x[22..31] y[34..40]
  z[-9..-6] 全空 + 红纹理判别帧 0 红 px + **old v1.7.0 二进制帧字节
  一致**（= 非 GEN-9 回归，GL 列 mesh 集成时机 quirk 该场景配置特异）；
  受控平台场景（俯视/侧视近景）同二进制全正常（俯视 108k px / 侧视
  1.6k px 精确隔离）。教训：E2E 门场景选**受控几何**（自建平台 + 封
  源水 + 玩家站位标定），勿在自然地形远景上赌视线通 + mesh 集成时序；
  判别纹理（纯红）+ CPU mesh 临时日志 = 渲染不可见谜题的标准取证链。
  quirk 本体（demo 列 mesh 集成滞后）留 KNOWN_ISSUES 候选。
- **zsh 无引号 `${var:-word}` 不分词 = gate 脚本整段参数合一（PITS
  293）**——10-28 GEN-9 门批：batch inline 跑在 zsh（shell 工具默认），
  `${scene:---new --save g123_$tag}` 展开为**单个词** `--new --save
  g123_s1_new_a`（zsh 默认 SH_WORD_SPLIT off，无引号参数展开不分词）→
  游戏 CLI "unknown argument" 打整段 help + 截图 sz=0（**非黑窗
  22106 B**——sz=0 = 进程没跑到截图，日志特征 = help 文本开头
  "Argument error"）。修正 = gate 脚本写 .sh 文件 `bash` 跑 + 显式
  if/else 拼命令行（gate123_s1s2.sh 范式）。排查捷径：sz=0 先看日志
  头部是 help 还是黑帧。
- **场景 python 构建器"静默未写入"= 后续轮 A/B 全盲（PITS 294）**——
  10-28 GEN-9：v5 走廊 air 12 条编辑的 heredoc 构建输出在混排命令链中
  丢失，场景文件实为 v4 内容（未写入）→ 后续两轮 A/B diff=0 被误读为
  "功能不可见"（真因 = (23,37,-8) 草块挡视线，扫描探针才揪出）。线索
  当时已现：v4/v5 帧同尺寸 432583 B。纪律：场景文件构建后**必须从
  磁盘重读断言关键编辑在档**（`assert (x,y,z) in existing` 范式）；
  同尺寸帧 = 同内容强嫌疑，先验场景落盘再疑渲染。
- **右键交互只在指针锁定时执行——输入跑必须先左点捕获（PITS 295）**——
  10-28 FLUID-4 门 124：`handleInteraction` 在 `!locked` 分支**提前
  return**（只处理左点捕获 + ESC），右键链（interact/桶/放置）全部在
  locked 之后——裸右键点击被**静默丢弃**（SDL 层收到事件、游戏层零
  反应，INPUT-DBG 与 RCLICK-DBG 双层日志实证）。协议 = g104/CRAFT-2
  同款：左点 → **轮询 "Pointer captured"（DEBUG 级日志，--log-level
  debug）** → 单次右键；覆盖窗偷点击时左点重试。附带约束：左点会启动
  破坏进度（0.15 s ≈ 4-7% 裂纹 < 100% 不破块、不入存档 = 无害，但
  截图带裂纹 → 输入跑 ×2 RAW 对仅尽力而为，**存档状态 = 权威断言**
  （E2E 协议对输入跑的既有定位））。
- **同存档槽双游戏进程 = 终档竞态（读档先验进程，PITS 296）**——
  10-28 FLUID-4 门 124 调试：慢速 300f 探针跑与下一轮探针共享
  `--save` 槽（前者超时/未竟时后者 rm + 复用），两次 clean-exit 写档
  交错 → 探针 python 读到**另一进程的终态**（flow 7 vs 8 矛盾，文件
  mtime 单一写入却内容反复）。纪律：输入跑脚本启动前 `pgrep -f
  'tokenmi[n]e --save <slot>'` 确认无残留进程 + runner 断言前验
  'Game shut down cleanly'（无 clean exit = 槽内是**场景文件**而非
  终档，断言恒败且误诊为"功能失效"）。
- **输入跑场景流体必须达稳态，否则 ×2 RAW 帧差发散（PITS 297）**——
  10-28 FLUID-5 门 125 场景 A 首版：倒出的水在无墙平台上漫出边缘沿
  自然地形无限扩散，帧 600 终态依赖点击落帧（点击 = 实时间锚定 ±帧
  差）→ 新侧 ×2 RAW **MISMATCH（336169 vs 335712 实证）**。修复 =
  场景加 2 层周界墙封闭水体 + CPU 收敛探针（FluidSim::tick 循环至零
  changed：场景区 5 tick = 15 帧稳态）实证帧 600 与点击帧无关 → RAW
  对 EXACT 恢复。纪律：带持续流体活动的输入跑场景，先 CPU 探针验证
  稳态再收图；基线世界自然流体 churn（同帧锁定）双侧同帧数同状态，
  不影响 RAW。
- **World::getFluid 返回 level 非 chunk 编码（PITS 298）**——10-28
  FLUID-5：getFluid = 任一种类的 level（0..8），chunk 编码才是
  0/水 1..8/熔岩 9..16；`fluidKindOf(getFluid())` 对熔岩格恒返回
  Water（level 5 落在水编码带）→ 水桶对熔岩 8 新测全败（返回候选格
  而非反应倒位，"替换倒"分支吞掉全部异种路径），单测揪出；种类判定
  须走 `World::getFluidKind`。
- **输入跑场景流体必须达稳态，否则 ×2 RAW 帧差发散（PITS 297）**——
  10-28 FLUID-5 门 125 场景 A 首版：倒出的水在无墙平台上漫出边缘沿
  自然地形无限扩散，帧 600 终态依赖点击落帧（点击 = 实时间锚定 ±帧差）
  → 新侧 ×2 RAW MISMATCH（336169 vs 335712 实证）。修复 = 场景加 2 层
  周界墙封闭水体 + CPU 收敛探针（FluidSim::tick 循环至零 changed：
  场景区 5 tick = 15 帧稳态）实证帧 600 与点击帧无关 → RAW EXACT 恢复。
  纪律：带持续流体活动的输入跑场景，先 CPU 探针验证稳态再收图；基线
  世界自然流体 churn（同帧锁定）双侧同帧数同状态，不影响 RAW。
- **World::getFluid 返回 level 非 chunk 编码（PITS 298）**——10-28
  FLUID-5：getFluid = 任一种类的 level（0..8），chunk 编码才是
  0/水 1..8/熔岩 9..16；`fluidKindOf(getFluid())` 对熔岩格恒返回
  Water（level 5 落在水编码带）→ 水桶对熔岩 8 新测全败（返回候选格
  而非反应倒位，"替换倒"分支吞掉全部异种路径），单测揪出；种类判定
  须走 `World::getFluidKind`。
- **per-kind 固定容器尺寸 = 种类数+1（哨兵槽 0），非种类数（PITS 299）**
  ——10-28 FLUID-6：FluidKind 1 基（Water=1/Lava=2/Honey=3），per-kind
  数组（chunk m_fluidCells/m_fluidHot/m_fluidWalkPending + world
  m_fluidChunks/m_fluidActive）以 kind 值为下标 → 3 种类须尺寸 4。
  尺寸 3 时 Honey(3) 静默 OOB（std::array 无边界检查）→ 段错落在
  **下游**测试（RaycastTest，非流体路径）= 新增种类的第一排查点；
  同款家族 = chunk_mesher 的 (kind==Water)?A:B 两种类派发（第三种类
  落错分支：honey 格按 lavaId 查块，测试注册表无 lava → 0 顶点，单测
  揪出）。加种类检查单：编码带 / 容器尺寸 / 派发三元式 / 词汇表 /
  per-kind 循环列表（world.cpp ×2 + tests）。
- **隐藏 starter 槽范围 = [9,26]（PITS 300）**——10-28 FLUID-6：
  parseStarterSlots 硬校验 storage 槽 ∈ [9,26]（热栏 0-8 归 items
  填充）；honey_bucket 首版用槽 27 = init 点名拒绝
  （"slot must be a storage slot in [9, 26]"）→ 改 16。加隐藏
  starter 物品前先数已占槽（17-26 = 药水×7 + 树苗×3）。
- **tick 初采样的 m_inFluid 滞后一 tick = 漏 1 深池落地取消（PITS 301）**
  ——10-28 ENT-5：mob 坠落取消判定首版用 m_inFluid（Entity 本 tick
  移动前采样）——落地帧脚刚进入水格时值还是 false → 池底落地仍吃
  坠落伤。玩家 Phase-16 注释同款教训（"a falling player enters a
  1-deep pool one tick after the last airborne sample"）——落地流体
  判定必须 fresh 扫最终位置（overlapsFluid）。单测
  LandingInAPoolCancelsTheFall 钉死。
- **CPU harness 的 assets 相对路径：binary 须从所属树 cwd 运行（PITS 302）**
  ——10-28 ENT-5 S2：old 侧 harness 从 repo cwd 运行 → 加载新 assets
  （honey.json 的 fluid:"honey"）→ 旧注册表拒未知 fluid 名 →
  WorldGenerator::create() assert（RC=134、输出全空 → 门脚本误判
  "OLD died?!"）。纪律：harness 每 binary `( cd <tree> && <harness> )`；
  门脚本断言前验 harness 确有输出行。
- **输入跑墙钟时长 > 自然再生窗 → 损伤终态被回血（PITS 303）**
  ——10-28 ENT-7 S2：断言 hp 15（8 格坠落 5 伤）——1500f 跑 ≈ 46 s
  墙钟，落地后 ~10-15 s 自然再生把 5 伤回满 → 四跑全 hp 20（误判
  "坠落没发生"）。修 = 弃 hp 断言、用 y37.0 exact 落面证坠落（P72
  "自然回血污染长跑损伤终态：run 时长须 < regen_delay 窗"的输入跑版
  复现——输入跑时长 = 墙钟，fps 越低越长，比无输入跑更容易越窗）。
- **spawner 生成环是半径 5-6（数据 min/max_radius），不是 8-16（PITS 307）**
  ——10-28 SURV-7 S2 首版两连失效：按 ENT-8 文档"spawner 环 8-16"
  设计的 8-16 annulus 地板场景零生成（"found no valid cell"），
  SPAWNPROBE 实证 `ring = ceil(maxRadius) = 6`（真实数据
  min_radius 5 / max_radius 6；tm_scene_check.py v2 注释早有
  "min5 max6"——ENT-8 文档的 8-16 是把 Phase-25 的**敌对保护圈
  8/12/16** 误记成了生成环）。教训 = 场景设计前用 debug 日志/
  探针实证 spawner 入口参数（cx/cz/ring/floorY），文档里的环半径
  不可信；生成格 = 玩家格整数偏移 (dx,dz)，dx²+dz² ∈ [25,36]。
- **zsh 脚本无 `!` 管道前缀（bash 习惯），且 `zsh -n` 零诊断输出（PITS 310）**
  ——10-28 SURV-9 门 135：`grep -q X && ! grep -q Y && ...` 在 zsh 脚本里
  = 语法错（`!` 前缀仅 bash 支持），`zsh -n` 静默 RC=1 **无任何报错
  文本** → 只能 head -n 二分定位；单行 `! echo hi` 复现。解法 =
  `grep -qv`（匹配不含 pattern 的行）或 `if ! cmd; then`；批处理脚本
  跨 shell 移植时 `!` 前缀是隐形雷。
- **fps 随场景变化（PITS 311）**——10-28 SURV-9：real-dt 终态设计按
  fresh spawn 场景的 13-31 fps 带（P84 先例 600f = 19.4-46 s）算帧
  数，冒烟即失效——密封房场景（小世界轻渲染）实测 **50-60 fps**
  （1600f 只 4 次回复非 10 次）。教训 = fps 是场景属性不是机器属
  性，real-dt 终态设计先冒烟实测场景 fps，或消掉 real-dt 变量
  （本门解法 = nodecay config【hunger_decay_seconds 9999，g95 先例】
  + 帧数按最慢边界 + 恰满 10 次回复的精确终态）。
- **捕获点击的 grab delta 相机漂移在本 WM 平铺态系统性偏大（PITS 319）**——10-29
  门 140 输入跑：S2 双击分拣冒烟 8/8 次捕获后相机 yaw −0.255 / pitch −1.09（场景值
  0/−0.25，漂移 = 14.6° 俯，远超 PITS 28 ±0.01 rad 估计与 PITS 309 9.6° 实机值）——
  但 8/8 次背包操作终态全部正确（面板点击 = 绝对坐标、几何与探针逐次一致 = 几何读
  取正确，漂移只发生在 SDL 捕获瞬间的 grab delta）。教训 = ① 相机漂移断言不能当
  输入跑的门（PITS 309 的"相机基线守卫"在漂移系统性偏大的 WM 态失效）——**存档
  终态（物品布局/selected）是唯一权威**，相机只记录不门控；② 漂移时大时小（后续
  冒烟 cam 恒等 0/−0.25）= 捕获瞬间光标位置的 X 异步竞态，不可复现性本身不是
  缺陷信号；③ 双击/shift 操作在漂移下依然 100% 命中 = 操作坐标与捕获 delta 相互
  独立（面板 = 屏幕空间固定布局，不随相机变）。
- **服务器重启回滚 .git/index = 假暂存删除（PITS 318）**——10-29 门 139
  （PITS 317 同事件）：opencode 服务器重启不仅清了 /tmp，还把仓库
  .git/index 回滚到 CRAFT-5 提交前状态（HEAD/worktree 完好）：CRAFT-5
  的 2 个新文件 status 呈"D（暂存删除）+ ??（未跟踪）"混合态、13 个改
  动文件呈 MM——`git diff <HEAD>` 甚至报 worktree 删除（文件实际存在、
  尺寸吻合）。判定 = `git diff --cached --stat` 显示 index 落后 HEAD
  恰一个提交体量（439 行 = CRAFT-5 提交体量）。修 = `git reset`（mixed，
  index 从 HEAD 重建，worktree 零改动）+ `git diff <HEAD> --stat -- <疑
  点文件>` = 0 行验证。教训 = 任何重启/回滚事件后先 `git status` 体检；
  见已提交文件"D + ??"混合态，先疑索引回滚而非文件丢失（ls 实尺寸核
  验 + `git diff --cached --stat` 对差量）。
- **服务器重启清空 /tmp = 全部门资产丢失（PITS 317）**——10-29 门 139：
  opencode 服务器重启清掉 /tmp/opencode——tm_v1XX_src/perf 参照树、门
  脚本/场景/config/断言产物、base18 锚、插桩 step0 patch/hunk 脚本、性
  能 seed 全失（仓库无损）。重建清单 = ① xenv.sh（DISPLAY=:0）② g117
  对比器（raw 字节比 + savenorm real-dt 剥离：fires[*].remaining /
  entities[*].age / player.saturation）③ configs（以已提交
  config/config.json 用户调值为底 + nomobs/nodecay/perf 变体）④ 参照
  树（git archive <commit> + mkdir data/saves；性能树 = 同款补丁器两侧
  同打——CRAFT-6 零 game.cpp 变更 = v1.23/v1.24 树锚点字节一致可共享
  补丁器）⑤ base18 锚（S1 四方字节一致后刷新采用 = PITS 265/315 先
  例）⑥ 性能冻结 seed 可换 `--new --seed`（世界生成跨二进制确定性 =
  项目核心不变量，GEN-7/8/9 "同 seed 900f" 同款协议）。教训 = **门协议
  资产必须可从仓库 + commit 重建**；只存在于 /tmp 且不可推导者（step0
  patch）是风险点——本次重建为 g139_instrument.py 同款补丁器，长期须
  入 tools/（下轮基础设施项）。
- **性能树 step0 patch 的 hunk 锚定 update() 区 = 新插入移动上下文（PITS 316）**——10-29 门 138：
  CRAFT-5 在 `updateFires(dt);` 与 `updateFireSpread();` 之间插入 `updateFurnaceFlames();`
  → step0_patch 的 hunk #7（把这两行各自包进 chrono 测量块）上下文失配 FAILED（#6 恒 FAIL
  已知 + #7 新 FAIL）→ 性能树构建 -Werror 死（s9911_f0_active set-but-unused）。教训 =
  **性能树构建脚本接受后必须验两件事：marker 计数（9911-STEP0 = 15）+ 构建成功**；
  hunk #6/#7 类手工补块时，新 v1.x 的 pass 放**测量块外**（保持新旧树 fires/spread 数字
  可比的语义）。另：黑窗相位的 PNG 字节随窗口位置变（679×744+0+60 = 19817 B / +0+22 = 12037 B
  纯黑）——sz 阈值判定用大小门（>100000）而非字节恒等；10-29 夜黑相位偏长（单 tag 8-10 连黑，
  周期 ~40 min 级），重试 12 × 90 s 等待才收讫 = PITS 280 黑窗分钟周期的加长版
  。
  ③ 帧计数器 update/render **后**自增 → `--frames N` 末帧见 N−1：age-0 截图
  设计须门 `(m_frameCount+1) % period`；粒子光读炉格（opaque 无 emission）=
  黑粒子 on≡off diff 0——读上方空气格；**period 取寿命 2× 会撞 60 fps 边界**
  （30 帧 = 0.5 s = spawnFire 寿命上限 → 同侧 RAW 被 125 px 残留余烬破，差值
  全在火焰簇内世界零差）——取 36 帧留裕量；age-0 批的**当帧 update 仍积一帧
  真实 dt 位移**（fps 依赖）→ 活粒子同侧 RAW 恒差 = P66 类（P54 条款），
  证据 = savenorm EXACT + RAW 差异 100% 含于火焰簇。
- **unmap_covers 覆盖谓词缺口 = 浏览器吞点击（PITS 314）**——10-29 门 137 S2 首轮：
  `unmap_covers` 的覆盖判定要求 `y+h >= 770`，用户 Firefox 窗 1362×744+0+20 的
  y+h = 764 **不满足** → 预存全屏浏览器窗不被隐藏（TM_PRE_WINDOWS 快照也不够——
  快照只限定"预存"，尺寸谓词仍要过）→ 捕获/输入点击全部落入浏览器 = 静默失败
  （游戏日志零 "Pointer captured" 行，6/6 尝试捕获失败但脚本以为在重试捕获）。判
  别特征 = 游戏跑满 900f 干净退出 + 截图正常尺寸但点击全无效果 + 日志无 Pointer
  行。处置 = 输入跑前**显式隐藏预存 covers**（本门 = xdotool search --class/--name
  firefox → windowunmap，跑毕 windowmap 恢复用户窗口），不依赖 unmap_covers 谓词。
- **WM 平铺态环境依赖 = base18 锚尺寸失效（PITS 315）**——10-29 门 137：用户开
  Firefox 后 awesomeWM 平铺变化——S1 批（firefox 在场）游戏窗 = 679×744 半平铺
  （GEN-8 同款），S2 批（firefox 显隐）= 1362×744 全宽 → 旧锚（1362×744，门
  116-136 连续 387726 B）尺寸失配 168140 B。**S1 锚字节门 = 先验四方窗口尺寸一
  致 + 尺寸 = 锚尺寸**；失配 = 环境态变更（非回归，世界/代码零涉）→ 锚刷新 +
  记档。门 137 锚 = 679×744 168140 B（S1 firefox 在场态）；用户关 firefox 则再
  变，同法处置。另：zsh `for` 循环内 `local ev` + `ev=$(...)` 二次迭代会把上一轮
  旧值以 ev=单引号 引号形打到 stdout（门 137 证据门 i=5 假象「BAD 却 OK」）——循
  环内局部变量一次性声明；证据行与状态行同屏时按行序还原。
- **挤奶 E2E 瞄准链四坑（PITS 313）**——10-28 SURV-10 门 136：
  ① **宽 AABB 穿墙**：1.2 宽 cow 的 x 移动只查 leading cell（西面
  格 air）→ 1 格石墙直接穿过（东半仍嵌墙内），v1 笼子墙基座 y44 被
  ring-hole 分支覆盖成 air = 出口，封死基座后**仍穿**（leading 盲区
  与缺口无关）→ 弃"密封笼"设计，改用"spawn 重叠（AABB 1.2 vs 格
  1.0 + jitter 0.5）→ 物理确定性挤出驻车"（牛恒停墙西 (24.4,45,
  -22.5)，帧锁定 = fps 无关）+ 瞄准测得的驻车点。
  ② **射线被自家墙挡**：驻车点在笼西，玩家东南射线穿南端墙角格 →
  墙开 1 格 sightline 窗（y45-47，基座仍封）——牛 1.2 宽穿不过
  1.0 窗 = 窗只通光线不通牛。
  ③ **点击时相机 = 场景值**（修 PITS 309 归因）：MILKPROBE dump
  实证点击时 m_camera = 场景 yaw/pitch 无漂移；终存档的 (−0.0175,
  −0.1675) "漂移" = **脚本自身跑后 mousemove 到角落时指针仍锁定**
  （相对位移 = 视角旋转；固定窗几何 → 固定量 = 可复现根因）。按
  "终相机"预补偿瞄准 = 射线高过牛头 7cm 脱靶实证——**输入跑的瞄
  准直接用场景值，补偿只属于跑后动作**。
  ④ **低帧率相位 + 未禁衰减 = 同侧 RAW 破**：900f 实跑 >72 s 时衰
  减 tick 次数随实跑时长漂移（hunger 18 vs 19 = 饥饿图标差）→ 输
  入跑场景 config 禁衰减（hunger_decay_seconds 9999 先例）+ 终态
  证据门（期望值不达 = 整跑重试，不留无证据跑）。
- - **后台复合命令的 heredoc/python 步静默死（PITS 312）**——10-28 性
  能树三连（v118/v119/v120）：`set -e` + 后台 job + 内联 heredoc
  python（或 `python3 -c` 多行串）= 失败时**零输出**早死（.rej 文
  件在、build/ 不存在、markers 差 4），诊断须逐状态查。解法 = 关键
  编辑步前台跑（或直接脚本文件 `python3 /tmp/.../hunk6.py`），后台
  只留纯 shell 步。
- **输入跑终相机漂移（PITS 309；10-28 PITS 313 修正归因：漂移源 = 脚本跑后 mousemove 时指针仍锁定，非捕获点击 grab delta——点击时相机 = 场景值）：**
  ——10-28 SURV-8 门 134：左键捕获点击使 SDL 相对模式初始化时记入
  一次性 delta（冒烟实测 yaw −0.0175 / pitch −0.1675 = 9.6° 俯，
  远超 PITS 28 的 ±0.01 rad 估计）→ 终截图/存档相机偏离场景值，
  批内不同 run 的 RAW/savenorm 不可比（savenorm 不剥 yaw/pitch）。
  实证 = 同窗几何下**完全可复现**（3 冒烟跑终相机逐值相同 + 2 跑
  截图字节一致）→ 解法 = 门脚本加**相机基线守卫**（首个成功输入
  跑设基线，终相机不一致的跑重试）；跨侧同窗几何同 warp → 同
  delta → 四跑 RAW 全字节一致达成。教训 = 输入跑的 RAW/savenorm
  比较以相机一致为前提，先验漂移再设断言。
- **`grep -c` 零匹配 RC=1 静默断 `&&` 链 = 后续步骤整段不执行（PITS 308）**
  ——10-28 SURV-7：`cmake --build ... | grep -ciE 'error|warning' &&
  cd ... && python3 场景构建` —— 构建 0 错误时 grep -c 输出 "0"
  但 RC=1（无匹配）→ `&&` 链断 → 场景构建 python **静默未跑**，
  后续冒烟用的是旧场景文件（两连"零 kill"之谜的真根因之一）。
  教训 = 批处理里 `grep -c` 之后要么 `|| true` 要么不用 `&&` 串联
  关键步骤；"0" 输出 ≠ 成功输出。
- **cp -a 性能树带 build/ = 陈旧 CMakeCache + ninja 假"无活" → 静默用旧二进制（PITS 306）**
  ——10-28 SURV-6 性能 A/B 首跑：tm_v117_perf = cp -a tm_v116_perf
  （build/ 目录一并复制，CMakeCache.txt 指向源树绝对路径）+ 拷
  SURV-6 变更文件 + `cmake --build build`——构建作业输出 "0 error"
  但二进制 mtime 停留在 13:34（上上版 v1.15.0）：cmake 的
  CMakeCache 目录失配告警是 "CMake **E**rror"（大写 E，大小写敏感
  grep `\berror\b` 数 0 = 假绿），ninja 因 cp -a 保留了全部
  源文件 mtime 判"无活"不重编不重链。后果 = 门 132 性能 A/B 首轮
  v117 侧 RC=1 秒死（旧二进制读新场景格式 20？不——读旧 seed 正常
  跑但版本错），且**门 131 性能 A/B 的 v116 侧实为 v1.15.0 旧二进
  制**（ENT-9 零 src 差 ⇒ 结论仍成立且更稳，记档；若 ENT-9 有 src
  差则门 131 性能证据作废）。修 = `rm -rf build` 全量重配重编 +
  banner 版本实证（perf 日志 grep "TokenMine.cpp v"）+ 构建 grep
  改 `grep -ciE 'error|warning'`。纪律：**性能树从 cp -a 继承
  build/ 必删重配；每版 perf 跑前 banner 验版本；构建"0" 须区分
  "无告警"与"没干活"**（ninja "no work to do" = 没干活）。
- **场景 builder 的"存在即跳过"去重会静默丢弃后写的块（PITS 305）**
  ——10-28 ENT-9 S2 密封房：air carve 段已把房顶 y48 占位为
  `air`，builder 的 add() 见 (x,y,z) 已存在即跳过 → 432 glowstone
  房顶静默丢失、房间恒暗、掉落 A/B diff≈0（判别量消失）。修 =
  对占位格做 overwrite（把 air 条目改写为目标块）+ 落盘后重读
  断言房顶全 glowstone。教训 = 场景编辑器的"幂等 add"对"同一格
  先 air 后实体"的时序不幂等：凡后续要覆盖的占位格，builder 必须
  显式 replace 并在写后 verify。
- **输入跑判别量须 ≫ 输入抖动量（≥3×），否则改几何主导设计（PITS 304）**
  ——10-28 ENT-7 S2 首版：地面松键滑行判别——滑行 0.358 块 < 独立
  real-dt hold 时序抖动 ±0.22 块（xdotool 投递 ±50 ms × 4.3 m/s）→
  per-pair cross 实测负值 −0.18（抖动吞掉信号）。修 = 空中滑翔几何
  设计：cross 由 8 格坠落 + 空中衰减积分主导（~1.3 块）、释放时刻
  抖动在 cross 项近似抵消（两侧同窗 pre-release 漂移同值）→ 实测
  cross 1.4254/1.3332 ∈ (0.8,1.8) 稳过。教训 = 输入跑 A/B 的信号量
  须 ≥3× 抖动量，做不到就换判别面（位置 → 几何落点 / 损伤 → 落面）。
