# BACKLOG.md — v1.x 任务矩阵（执行权威）

> 10-27 用户定案：v1.0 已发布（34/34 全完成，tag v1.0.0（原 v1.0 改名）+ 正式 release）。
> 任务矩阵刷新 = **v1.0 砍掉清单回填**（33 项；原砍掉 34 项中 PIPE-4
> 跨 chunk 合并维持"不立项" = 99.x 数据不支持，见变更记录）。两处代号
> 冲突重编号：原砍掉 GEN-4/5/6（基岩多层/地形互感/耕地湿润）→
> **GEN-7/8/9**（v1.0 已占用 GEN-4/5/6 = 洞穴破碎度/浮空树/树放置半区）；
> 原砍掉 FLUID-3/4/5（倒向水面/水熔岩反应/流体种类扩展）→ **FLUID-4/5/6**
> （v1.0 已占用 FLUID-3 = mob 涉水修正）。此后任务只从本表取，不另列
> 菜单；增删格子需用户确认。

## v1.x 任务矩阵（33）

| 系统 | ① | ② | ③ | ④ | ⑤ | ⑥ |
|---|---|---|---|---|---|---|
| STRM 流送与内存 | ~~STRM-2 流体列保留~~ ✅ v1.1.0 | | | | | |
| PIPE 帧管线与并发 | ~~PIPE-3 流体 per-cell~~ ✅ v1.2.0 | | | | | |
| LGT 光照 | ~~LGT-3 夜光重传播~~ ✅ v1.3.0 | ~~LGT-4 光束入光~~ ✅ v1.4.0 | ~~LGT-5 半块透光~~ ✅ v1.5.0 | | | |
| GEN 世界与生成 | ~~GEN-7 基岩多层~~ ✅ v1.6.0 | ~~GEN-8 地形互感~~ ✅ v1.7.0 | ~~GEN-9 耕地湿润~~ ✅ v1.8.0 | | | |
| FLUID 流体 | ~~FLUID-4 倒向水面~~ ✅ v1.9.0 | ~~FLUID-5 水熔岩反应~~ ✅ v1.10.0 | ~~FLUID-6 流体种类扩展~~ ✅ v1.11.0 | | | |
| ENT 实体/Mob/移动 | ~~ENT-5 坠落伤害~~ ✅ v1.12.0 | ~~ENT-6 AI 补强~~ ✅ v1.13.0 | ~~ENT-7 移动惯性~~ ✅ v1.14.0 | ~~ENT-8 和平/保护环~~ ✅ v1.15.0 | ~~ENT-9 掉落可视化~~ ✅ v1.16.0 | |
| SURV 生存/战斗/效果 | ~~SURV-6 效果档位~~ ✅ v1.17.0 | ~~SURV-7 死亡统计~~ ✅ v1.18.0 | ~~SURV-8 工具 10×~~ ✅ v1.19.0 | SURV-9 饱和细化 | SURV-10 挤奶冷却 | |
| CRAFT 物品/合成/面板 | CRAFT-4 配方提示 | CRAFT-5 炉子表现 | CRAFT-6 燃料熔炼 | CRAFT-7 背包交互 | | |
| SAVE 存档 | SAVE-2 速度入档 | SAVE-3 槽位管理 | | | | |
| PRE 表现层 | PRE-7 中毒/燃烧表现 | PRE-8 mob 火焰 | PRE-9 箭/弓/手视觉 | PRE-10 图标重叠修复★ | PRE-11 HUD 细化 | PRE-12 tooltip/F3 |
| UX 实机报障 | （新实机报障入此行，用户定案） | | | | | |

★ PRE-10 = 原砍掉清单唯一 bug 级项（效果图标行与吸收红心重叠，3 效果
同激活可触发）——默认走行序，用户可一句话提前提到任意位置。

## 执行顺序

1. 从上到下逐行、行内从左到右：STRM-2 → PIPE-3 → LGT-3 → … → PRE-12。
2. 一任务一 phase，走标准阶段流程（实现 → 构建+测试 → E2E → 文档 →
   提交 → 报告）。
3. 版本号仍逐版 bump（每 phase 一个 = 代码内二进制身份，供 A/B 旧侧 /
   CHANGELOG 逐版条目 / F3 钉），但 **phase 级不打任何 tag**
   （10-27 用户定案：任务矩阵每完成一个打一个 tag 太频繁）；
   **每版发布前必做性能 A/B**（用户硬要求），旧侧 = 上一 release tag 或
   指定 commit。**tag = 1:1 对应 release**，只在发 GitHub release 时打：
   - 组 release：5 版一组，tag = 组末版本号（标题 "TokenMine.cpp v1.XX.0"）；
     v1.x 组边界 = 1.6–1.10 / 1.11–1.15 / …（矩阵余 26 项 ≈ 6 个组 release）。
   - 里程碑版本：单独正式 release（v1.0.0 先例）。
   - 组内用户定案单独发：tag 该版本、下一版本起新组。

## 表外

- **无界项（矩阵外，立项需用户定案）**：新内容开放空间（方块/物品/
  配方/mob 可无限加）、经验（XP）/附魔系统
- **已接受偏离（非任务）**：锻石不可熔回、箭不自伤、水熔岩 apply 顺序、
  实体 age 墙钟、喝奶不恢复饥饿、同色染色不消耗、mob 无 swim-up 爬岸
  （FLUID-3）、游泳位与池底实体垂直攻击门（FLUID-3 S2 实证）等（文档
  明确接受的设计决策，详见各 phase 存档「偏离」节）
- **不立项**：PIPE-4 跨 chunk 合并（99.x 数据不支持：同帧选列可合并
  6/6418=0.1%，0.002 ms/帧 ≪ 1 ms 砍除线，用户确认——见 phase-lgt1-flip.md）

## v1.0 完成清单（34 项，2026-10-27 归档，详情见 `docs/archive/phase-*.md`）

| 项 | 内容 | 版本 |
|---|---|---|
| STRM-1 | 流送环方形→圆盘 + 远平面帽（vd80 内存 −49.5%） | v0.2.0 |
| PIPE-1 | worker 快照网格化（主线程 remesh −96%） | v0.2.0 |
| PIPE-2 | 存档增量 journal（全量退出 6.2 s→17 ms） | v0.2.0 |
| LGT-1 | union-region（砍除记档：数据不支持） | — |
| LGT-2 | 叶子光衰减 | v0.2.0 |
| GEN-1 | 面条洞穴 | v0.2.0 |
| ENT-2 | BlockState 通用 per-block 状态 | v0.3.0 |
| GEN-2 | 3 树种生长 | v0.4.0 |
| GEN-3 | 矿脉 MC 形态 | v0.5.0 |
| FLUID-1 | 水量守恒（会计恒等式） | v0.6.0 |
| FLUID-2 | 游泳划水 | v0.7.0 |
| ENT-1 | 四肢摆动（+1b 实机模型修复） | v0.10.0/0.11.0 |
| ENT-3 | mob 踏阶 | v0.11.0 |
| ENT-4 | 水体交互（共享流体模型） | v0.12.0 |
| SURV-1 | 护甲（MC 精确值 + floor 减伤） | v0.13.0 |
| SURV-2 | 氧气窒息 | v0.14.0 |
| SURV-3 | 耐久（27 件 MC 值） | v0.15.0 |
| SURV-4 | 药水饮用 | v0.16.0 |
| GEN-4 | 洞穴破碎度（实机报障） | v0.17.0 |
| GEN-5 | 浮空树（实机报障） | v0.18.0 |
| SURV-5 | mob 效果（wither/poison） | v0.19.0 |
| GEN-6 | 树放置半区修复（交付上报） | v0.20.0 |
| CRAFT-1 | 工作台分块（format 19） | v0.21.0 |
| CRAFT-2 | 物品 3D 模型（16 件） | v0.22.0 |
| CRAFT-3 | 材质音效 + 3D 空间化 | v0.23.0 |
| SAVE-1 | 自动存档（原子写） | v0.24.0 |
| PRE-1 | 中文字体 | v0.25.0 |
| PRE-2 | 边缘雾 | v0.26.0 |
| PRE-3 | 水面波纹 | v0.27.0 |
| PRE-4 | 云与环境音 | v0.28.0 |
| PRE-5 | 透明 discard | v0.29.0 |
| PRE-6 | 掉落光照 | v0.30.0 |
| UXFIX-1 | 渲染状态泄漏修复（实机报障：白盒/黑块） | v0.31.0 |
| FLUID-3 | mob 涉水修正（实机报障：悬空/站水） | v0.32.0 |

## 变更记录（v1.0 时代）

- 2026-10-27：**tag 策略刷新（用户定案）**。"任务矩阵每完成一个打一个
  tag" 太频繁 → **phase 级不打 tag**；版本号仍逐版 bump（代码内二进制
  身份），tag = 1:1 对应 release（只在组末 / 里程碑发 release 时打）。
  v1.7.0 tag（1ab06f5，无 release）删除；组边界 = 1.6–1.10（tag
  v1.10.0）/ 1.11–1.15 / …，A/B 旧侧引用 = 上一 release tag 或 commit
  SHA。详见执行顺序 #3。
- 2026-10-27：**tag 清理（用户要求）**。39 tag → **9**（= release 1:1：
  v0.1.0 / v0.5.0 / v0.10.0 / v0.15.0 / v0.20.0 / v0.25.0 / v0.30.0 /
  v1.0.0 / v1.5.0），30 个无 release 的组内成员 tag 删除（远端 + 本地；
  commit 不受影响 = main 历史仍可达）。今后 A/B 旧侧引用按版本
  （commit 内 CMake PROJECT_VERSION）/ commit SHA，不再按 tag 名。
- 2026-10-27：**GitHub release 收尾（用户要求）**。① tag v1.0 → **v1.0.0**
  （仓库唯一两段式 tag 统一为三段式，指向 14e793d 不变；release 更名
  "TokenMine.cpp v1.0.0"，正文原样沿用；代码内 PROJECT_VERSION 保持 1.0
  = 历史事实，未重写 git 历史）② v1.1.0-1.5.0 组 release 标题
  "v1.5.0（v1.x 组 release：STRM-2 / PIPE-3 / LGT-3 / LGT-4 / LGT-5）" →
  **"TokenMine.cpp v1.5.0"**（符合执行顺序 #3 标题模式），正文按 v1.0.0
  结构重写整合（矩阵表 + 主题整合【流送与管线性能 / 光照三连】+ 工程数据 +
  构建与运行 + 范围与限制 + 许可），PITS 范围 267-288 修正为 271-288
  （267-270 属 v1.0 组时代门 114/115）③ Latest 收尾回 v1.5.0 组 release
  （重建时序）。同步 README/CHANGELOG/BACKLOG/AGENTS 旧引用。
- 2026-10-28：**SURV-8 工具 10×完成**（v1.x 第 19/33 项，v1.19.0，
  门 134，SURV 行 3/5）："工具 10×"原始定义已佚 = 定案 **MC 无效率
  工具 10× 惩罚规则**（消费 KNOWN_ISSUES Phase-11 条"仍缺：MC 的'无
  效率工具慢 10×'规则"）——工具对目标方块无效率加成（∉ fast_against）
  时采矿速度 = 自身速度 ÷ 10（`kWrongToolFactor` 0.1f 命名常量，MC
  精确 1/10；无列表工具对一切方块吃惩罚 = MC 剑情形；手/非工具恒 1.0
  位级不变）。纯函数 effectiveBreakSpeed 一个分支 = 零数据模型/零
  内容/零 config（9 工具 JSON 零改动）+ **P69 obsidian 排除条款随规
  则演进**（铁镐挖 obsidian 3.75 s flat → 37.5 s 慢档 = MC 方向一
  致）。exact 值单测钉死：铁镐×木 7.5 s（前 0.75）/ 铁斧×石 22.5 s /
  on-list 位级保持（铁镐×石 0.28125 / 铁斧×木 1.5）。1828/1828（+1
  净：Surv8WrongToolDurations 真资产 exact-time【EXPECT_NEAR 1e-6 =
  0.1f 非二进制精确残差说明】+ 4 处 off-list 钉演进 + 1 测改名）、
  -Werror 0。门 134：S1 第 134 次结构性免疫免重锚（四方 ×2 387726 B
  + base18 + savenorm；无输入无按住破坏 = 规则永不执行）+ S2a 判别
  PASS（铁镐×木 off-list：按住 0.85 s ∈ (0.75, 7.5) 窗口：OLD 侧 0.75 s
  破 + wood×1 掉落静止 (22.5,45,-4.5)【距玩家 3.0 > kPickupDistance
  1.0 不拾取】/ NEW 侧存活无掉落 = 终态互斥 ×2/侧 RAW + 存档断言 4/4）
  + S2b 对照 PASS（铁镐×石 on-list：按住 0.42 s 双侧同破 0.28125 s +
  cobblestone 掉落【P61 内容】：**四跑 old×2+new×2 RAW 全字节一致
  609396 B + 跨侧 savenorm 场景区 EXACT** = 零回归最强形）+ S3 1828
  + 无 S6【零 parse 面】+ 性能 A/B 真 v1.18 vs v1.19 PASS（banner 实
  证；fluidCells win2/3 29803/29800 逐位 + win1 ±2 + PIPE3
  changesApplied 1404 逐位 + 全同档 = 结构性惰性）；**PITS 309**
  （输入跑捕获点击 grab delta = 终相机漂移 yaw −0.0175/pitch
  −0.1675，批内可复现 → 相机基线守卫 = RAW/savenorm 前提）+ 自然区
  real-dt churn 裁定【S2a old×2 blocks 差 3 格距场景 107 块 + S2b
  fluids 差 1 格自然湖区 = PITS 244/283 类同二进制自分歧，非回归】。
- 2026-10-28：**SURV-7 死亡统计完成**（v1.x 第 18/33 项，v1.18.0，
  门 133，SURV 行 2/5）："死亡统计"原始定义已佚 = 代理定案**统计
  系统 v1 = 死亡相关计数器（deathCount + 每 mob kill.<name>），帧锁
  定事件 + format 21 存档 + 封闭词表校验**（消费 KNOWN_ISSUES
  Phase-16 条"无死亡分数/死亡统计"的死亡次数半；统计**显示** =
  PRE-12 呈现框架边界，本相零 UI）。词表封闭 = deathCount（玩家
  alive→dead 过渡，任意死因）+ 每注册 mob 一个 kill.<name>（**mob
  注册表合成 = block item 合成先例**，加 mob JSON 自动获得统计；
  无新数据文件）；计数事件 = 死亡过渡块 + despawn pass 每个 dead()
  mob（含无掉落；距离 despawn 不算死）；容器 m_stats map 只存非零
  （omit-when-default）；format 20→21 `player.stats` 恒写对象（空
  合法 = tables 键纪律；pre-21 带键 = 版本谎报；pre-21 读入 = 空
  免迁）+ **词表校验 = game 加载点 validateStats 单收口**（parseSave
  不加 MobRegistry 参 = 184 调用点涟漪，rescan 范式孪生）+ 未知
  kill.<name> 点名。**顺带修复 SURV-6 漏网 bug**（game 填充路径
  SavedEffect 只传 2 值 = 活体 tier-1 效果存盘丢档降 0，门 133 S2c
  终档 tier 1 断言 = 回归证）。1827/1827（+7 净：SaveFormat21 ×5 +
  ValidateStats ×2；38 处旧 format 迁移/降级测试契约演进 = 降级须
  剥 stats 键【含 2 downgrade helper 内建 + 行尾注释/(*j) 写法漏网
  3 处二轮补——机械批量后须全量套件验证】）、-Werror 0。门 133：
  S1 第 133 次结构性免疫免重锚（四方 ×2 387726 B + base18 +
  savenorm；零死零 mob = 零计数 + 零 UI）+ S2a 昼步道 kills PASS
  （**真生成环 5-6** 的 band 石地板 + 5×5 玩家台 + 台间/带外 22 深
  洞 = PITS 307 两连零生成后重做：被动 mob 生成 band 游走掉洞 →
  stats = kill.chicken×2 + kill.sheep×2 双侧逐值 + deathCount 缺
  （玩家安全）×2 RAW + savenorm）+ S2b 夜 + resistance tier 1
  （hp 0 + **deathCount == 1 exact** + effects [] ×2 RAW + savenorm）
  + S2c 密封房 tier 往返（终档 effects[0] = {resistance, ~110,
  **tier 1**} = SURV-6 填充 bug 回归证 ×2 RAW + savenorm）+ S2d 旧
  v1.17 侧 format 20 无 stats 键（系统不存在）+ S6 ×3 点名【
  kill.dragon / deathCount −1 / f21 缺 stats 键】+ S3 1827 + 性能
  A/B 真 v1.17 vs v1.18 PASS（banner 实证；fluidCells win2/3
  29803/29800 **逐位** + win1 ±4 + PIPE3 三项逐位 + 全同档 =
  结构性惰性实证）；PITS 307（spawner 环 5-6 非 8-16 = ENT-8 文档
  误记）+ 308（grep -c 零匹配 RC=1 断 && 链 = 场景脚本静默未跑）。
- 2026-10-28：**SURV-6 效果档位完成**（v1.x 第 17/33 项，v1.17.0，
  门 132，SURV 行 1/5）："效果档位"原始定义已佚 = 代理定案**状态
  效果档位化（amplifier I/II/III，全数据驱动）**（消费 KNOWN_ISSUES
  Phase-46 条"无 amplifier"残余；与 PRE-12 边界 = 档位机制/存档/
  行为 vs F3/tooltip 显示）。效果 JSON 2 新键 `tier_max`（int
  [1,3] 缺省 1）+ `tier_steps`（仅 5 加性修饰字段：speed/
  absorption/strength/weakness/resistance，pairing/死字段/no-op 三
  门）+ **全 grant 对可选 `tier`**（grants_effect(s)/物品 arrow/
  mob on_hit/mob arrow；范围 init 交叉校验点名）+ ActiveEffect 持
  tier（RESET 语义扩到档位不叠加）+ 5 聚合 tier 化（effectTiered
  Value 纯函数；tier 0 = 位级旧值）+ **format 19→20**（tier
  omit-when-0 = tier-0 条目与 f19 字节同形 ⇒ 迁移免迁 + f19 带键 =
  版本谎报名单）+ **内容 = MC 档位值 5 效果**（speed 1.2/1.4/1.6、
  absorption 4/6/8、resistance 0.2/0.4/0.6、strength 2/4/6 项目刻
  度、weakness 4/8/12 MC 精确）；tick/instant 形单档 = 记档偏离；
  无档位 in-game 消费面（信标 beacon_effects = 纯名列表 / 药水无酿
  造 / starter 27 槽满）= 记档偏离（档位实例现经存档注入 E2E 范
  式 + 未来内容扩展）。1820/1820（+17 净：parse 6 + save 6 + item
  3 + mob 2，7 处 format 钉 19→20 契约演进）、-Werror 0。门 132：
  S1 第 132 次结构性免疫免重锚（四方 ×2 387726 B + base18 +
  savenorm；fresh 零效果 = count 0 fast-exit 聚合零执行）+ S2 密封
  坠落室三值互斥 PASS（8 格坠落 5 伤禁再生：s2a 无效果 hp 15 / s2b
  resistance tier 0 = 5×0.8 hp 16 / s2c tier 1 = 5×0.6 hp 17——各
  ×2 RAW 327492 + savenorm【剥 effects remaining real-dt】EXACT，
  档位经 存档→apply→减伤管线 端到端实证）+ S2d 迁移证明（旧
  v1.16.0 二进制 + f19 孪生文件 hp 16 = pre-SURV-6 代码原样读取）
  + S6 ×3 点名【旧二进制拒 format 20 / tier_max 4 / grants tier 3
  越界】+ S3 1820 + 性能 A/B 真 v1.16 vs v1.17 二进制 PASS（零效果
  场景结构惰性：双侧终档 effects 全空 + win1 51927/51924 ±3 +
  win2/3 ±100 = 4 格 rim 簇 PITS 283 类非回归 + PIPE3 1404 逐位 =
  成本可忽略）；PITS 306（cp -a 性能树带 build/ = 陈旧 CMakeCache +
  ninja mtime 假"无活" → 静默用旧二进制——门 131 v116 侧实为旧
  v1.15 二进制【ENT-9 零 src 差结论仍成立记档】，修 = rm build 重
  配 + banner 版本实证 + 构建 grep 大小写）。
- 2026-10-28：**ENT-9 掉落可视化完成**（v1.x 第 16/33 项，v1.16.0，
  门 131，**ENT 行 5/5 收尾**）："掉落可视化"原始定义已佚 = 代理
  定案**全量掉落可见（纯内容，零核心代码）**（消费 KNOWN_ISSUES
  Phase-11 条"非 block 物品的掉落物在 world 中不可见" + Phase-15
  条"物品无 3D 模型"）：CRAFT-2 后 63 件非 block 无 model 物品掉落
  不可见（含 mob 掉落 rotten_flesh/bone/arrow/feather/beef/mutton/
  leather + 所有可 Q 丢弃物）→ 全部补 `model` 段（texture = 既有
  icon PNG `<id>.png`，零新纹理免重锚；家族几何 1-3 box 剪影：
  ingot/dye/wheat/stick 1 / bone/arrow 3 / bottle/potion/fands/
  rotten 2 / master_bow = bow 结构复用；CRAFT-2 渲染路径自动覆盖
  手持/世界掉落/进食三上下文）；共面 Z-fight 首轮套件揪出 4 家族
  10 件修正（颈/头/团相交埋入，apple 柄先例）；model 集合契约
  16→全 79 件（目录 = 全非 block 集，block 物品合成无 JSON）。
  零 src 变更/零 format（19）/config/音效。1803/1803（+1 净：
  ModelItemSetIsExactly16 演进 → AllShippedItemsHaveModels +
  Ent9FamilyGeometrySpotChecks，mesh 守护 modelCount 16→79）、
  -Werror 0。门 131：S1 第 131 次结构性免疫免重锚（四方 ×2 387726 B
  + base18 + savenorm；手持 slot0 = stone block 立方路径不变 + 增益
  件全在隐藏槽 17-26 + 模型惰性缓存）+ S2 密封房 A/B PASS（10 件
  预置掉落 + ×2 RAW/侧 + 跨侧 savenorm EXACT = 纯渲染 + diff 684 px
  全限世界带 + 心带/热栏带零）+ S3 1803 + 性能 A/B 最强形 PASS（两
  性能树 src 位级一致【零 src 变更】⇒ win1 fluidCells 51926 逐位 +
  win2/3 ±100 = 同二进制 v115perf 自分歧 + 终档 3 格 rim 簇实证
  PITS 283 类 = 结构性惰性）；场景坑 PITS 305（builder 去重丢
  glowstone 房顶）。
- 2026-10-28：**ENT-8 和平/保护环完成**（v1.x 第 15/33 项，v1.15.0，
  门 130，ENT 行 4/6）："和平/保护环"原始定义已佚 = 代理定案
  **peaceful 难度**（消费 KNOWN_ISSUES Phase-25 条"无 Peaceful 难度"；
  保护环本体 = Phase 25 既有敌对生成保护圈 8/12/16 锚世界出生点 +
  被动豁免）。difficulty 词汇表 3→4（严格解析同款纪律）+ peaceful 语义
  = **Phase-25 机制复用零新代码路径**：spawnProtectionRadius(Peaceful)
  = kSpawnProtectionPeaceful 1e6 = 保护圈覆盖全球 → 敌对自然生成全拒
  + 被动豁免照生（MC 和平核心）；damageScale 0.0 / rateScale 1.0 =
  API 一致性值（无宿主消费）；偏离记档 = 无"现有敌对消亡"（mobs
  ephemeral + difficulty 只 init 读取）/ 无自动回血 / 保护锚 = 世界
  出生点。零 format（19）/config 新键/内容/音效。1802/1802（6 难度测
  演进 +peaceful 断言）、-Werror 0。门 130：S1 第 130 次结构性免疫免
  重锚（四方 ×2 387726 B + savenorm）+ S2a peaceful 夜 ×2 hp 20 恒安全
  【RAW 342632 + savenorm】+ S2b normal 夜新 ×2 hp 0【RAW 337109】+
  S2c normal 夜旧 ×2 hp 0 + **S2b≡S2c 字节一致零回归** + S2d peaceful
  昼 ×2 hp 20【RAW 346925 + vs nomobs 对照 diff 45148 px = 被动豁免实
  证】+ S3 1802 + 性能 A/B 全同档（fluidCells win1 51969 逐位 +
  win2/3 29803 vs 29703 = ±100 rim churn 相位一周期，**同二进制 v114
  自分歧 fires 50↔51 + 流体簇 3 格位移实证 = PITS 283 类非回归**）。
  **v1.15.0 = 组 1.11-1.15 组末 → tag v1.15.0 + GitHub release**
  （notes 覆盖 v1.11 FLUID-6 / v1.12 ENT-5 / v1.13 ENT-6 / v1.14 ENT-7
  / v1.15 ENT-8）。
- 2026-10-28：**ENT-7 移动惯性完成**（v1.x 第 14/33 项，v1.14.0，门
  129，ENT 行 3/6）："移动惯性"原始定义已佚 = 代理定案 **玩家地面/空中
  移动惯性**（消费 KNOWN_ISSUES Phase-5 条"水平速度无惯性 = 期望速度
  每 tick 直接设定，松键即停"）。指数趋近模型（fps 无关 exp(−k·dt)
  真实 dt，knockback 衰减 std::exp 先例）：有 wish = v += (dir×S − v)×
  (1−e^{−k dt})（地面 kGroundAccelRate 8.0 敏捷起步 / 空中 kAirSteerRate
  2.0 弱转向）+ 无 wish = v ×= e^{−k dt}（地面 kGroundDecelRate 12.0 =
  MC 0.5448/tick 摩擦档【walk 松键滑 ~0.36 块】/ 空中 kAirDecelRate 1.9
  = MC 0.91/tick 拖曳档【松键滑翔 ~2.3 块】）+ kVelocityStopEps 0.001
  恰好 0 = 静止态确定性红线；FLUID-2 档保持瞬置（独立调参模型 = 记档
  偏离）；mob 侧范围外（AI 每帧精确速度 = 既有设计）。零 format（19）/
  config/内容/音效。1802/1802（+4 PlayerInertiaTest：滑行解析带 + 恰好 0
  静止 / 空中滑翔档位互斥 / 流体档瞬置钉死 / 静止不漂移）、-Werror 0、
  3 契约演进重钉（SlidesAlongWall 斜向减 ramp / EdgeBlockNoFloat 0.5 s
  减 ramp / SprintIsFasterThanWalk 双档减 ramp 冲刺优势保持）。门 129：
  S1 第 129 次结构性免疫免重锚（四方 ×2 387726 B + savenorm；无 wish =
  速度恒 0 = 衰减 no-op）+ S2 **空中滑翔判别输入跑 PASS**（s129_glide：
  平台 x22-33 顶 y45 + 深坑 x34-39 底 y37 = 8 格坠落；hold W 0.8 s →
  出檐 ~0.22 s 空中松键：old 瞬清零直落 x≈34.6 / new 弱衰减滑翔
  x≈36.0——per-pair cross 1.4254/1.3332 ∈ (0.8,1.8) ×2 + 四跑 y37.0
  exact + z 0.5 恒 + within-pair < 0.6；首版 ground-slide 判别废弃 =
  松键抖动 ±0.22 与滑行 0.358 同量级实测 cross 负值 −0.18 = PITS 304）
  + S3 放置零回归输入跑 PASS（单次右键 stone 落 (27,45,−7)：new×2/old×2
  RAW EXACT 381031 + new≡old 字节一致【玩家恒静止 = 结构惰性】+
  savenorm EXACT）+ S4 1802 + 性能 A/B fluidCells win2/3 29803/29800
  逐位 + PIPE3 逐位 + 全同档 = 成本可忽略。PITS 303-304（输入跑墙钟 >
  再生窗回血 / 判别量须 ≥3× 抖动量）。
- 2026-10-28：**ENT-6 AI 补强完成**（v1.x 第 13/33 项，v1.13.0，门
  128，ENT 行 2/6）："AI 补强"原始定义已佚 = 代理定案 **mob 追逐寻路**
  （KNOWN_ISSUES 记档缺口"追墙会沿墙滑"）。新纯函数模块
  `entity/pathfind.{hpp,cpp}`（cellStandable = feet 格 + 头空 air + 下方
  solid FullCube 整数地板 / pathfindBFS = 统一代价 BFS，固定邻居序
  -X/-Z/+Z/+X × flat/step-up(+1)/fall(1..8 深走廊) 三族边，target zone
  = Chebyshev ≤1，展开帽 512，无随机无 float = 帧锁定安全）+ MobEntity
  卡住触发集成（chase 窗 0.5 s progress < 25% 期望 → tryPathfind →
  MobState::Pathfind 跟随 + 0.5 s 重规划 + 1 s lost 冷却 + **2 s 节点
  超时逃逸**【A* 模型与物理的 +X/+Z 单格 step quirk 失配兜底】；未卡住
  = 结构惰直线追逐位级恒等）。零 format（19）/config/内容/音效。
  1798/1798（+19：standable 5 + BFS 10 + 集成 4）、-Werror 0；**4 契约
  演进重钉**（MobStepUpTest：Wall/Fence/TwoBlockStep → "绕行而非翻越"
  【sawPathfind + maxY 恒 1.0 = 原判定存活 pin】+ PlusX → 改名
  SingleCellStepPlusXStaysClampedAtTheQuirk【走廊封死 = 唯一通路是
  quirk 步 → 节点超时弃路径 → x 9.7 恒夹 = quirk pin 存活】）。门 128：
  S1 第 128 次结构性免疫免重锚（四方 ×2 387726 B + savenorm）+ S2
  harness A/B 矩阵 PASS【gen128 单源双库：wall NEW hits=19 绕墙攻击 vs
  OLD hits=0 恒夹 x7.7 互斥 / flat NEW≡OLD 逐值全同【hits=29 结构惰性】
  / sealed NEW≡OLD 逐值全同【无路 lost 回落 = 旧行为】×2/侧】+ S3 围栏
  竞技场 900f new≡old 字节一致零回归 + 性能 A/B fluidCells win2/3
  29803/29800 双侧逐位（重跑复现）+ 全同档【win3 upd 首轮偏置 = 环境
  噪声重跑证伪】+ 终档流体 4 格差 = PITS 283 类 churn 簇。无新增 PITS
  （设计期预判化解 quirk 失配 = 节点超时）。
- 2026-10-28：**ENT-5 坠落伤害完成**（v1.x 第 12/33 项，v1.12.0，门
  127，ENT 行 1/6）：玩家侧 Phase 16 已完整（fall>3 且无流体 →
  damage(fall−3)）= 缺口 **mob 侧坠落伤害**（MC mob 同样受坠伤）。
  MobEntity::update 落地块镜像玩家规则：净落差 > kMobFallDamageOffset
  (3.0f 命名常量 = MC 同值) 且落地格 fresh 无流体（overlapsFluid 扫
  最终位置——m_inFluid tick 初值滞后一 tick 漏 1 深池 = PITS 301）→
  damage() 既有死亡/掉落管线；mobs ephemeral = 构造函数一次钉
  fallStartY（无读档重钉需求）+ 离地转换重钉（1 子步滞后 = 确定性，
  紧公差断言）。帧锁定（硬规则 8）、零 format/config/内容/音效。
  E2E 设计约束记档：spawner floor band = 玩家 ±2 → 生成 mob 的致死
  坠落（需 23+）无法自然生成构造（chase 受 detect 16 限 / knockback
  方向推不向崖）→ S2 = CPU harness 探针（2×1 高台 25 落差 + wander
  走落 + 种子探测 1/5/6/8：new 全坠落死 / old 全存活 hp20 互斥终态
  ×2 逐值；PITS 302 harness 树 cwd 坑）。1779/1779（+5 MobFallDamage
  Test）、-Werror 0；门 127：S1 第 127 次结构性免疫免重锚（四方 ×2
  387726 B + savenorm）+ S2 harness 矩阵 PASS + S3 围栏竞技场零回归
  A/B（900f 夜 mob on：new×2/old×2 RAW EXACT 235680 + new≡old 字节
  一致 + savenorm EXACT + 玩家同帧死同位）+ 性能 A/B fluidCells win2/3
  29803/29800 双侧逐位（v112 首跑 29703 = 一次性 churn 相位波动重跑
  复现 29803 = PITS 283 类）+ 全同档 = 成本可忽略。
- 2026-10-28：**FLUID-6 流体种类扩展完成**（v1.x 第 11/33 项，
  v1.11.0，门 126，FLUID 行 3/3 收尾）：第三流体种类 = **honey（MC
  1.14+ 蜂蜜）**——编码三层化（0/1..8/9..16/**17..24**，水/熔岩带位级
  不变）+ **粘度 = 帧锁定 tick 门控**（kHoneyTickPeriodFluidTicks=3：
  蜜每 3 个流体 tick 模拟一次 = 1/3 速，水/熔岩零改动）+ 不参与水-
  熔岩反应（honey 块无 water_reaction = 数据驱动惰性）+ 可桶舀/倒 +
  无波纹（emitFluid 既有 kind==Water 门控）+ 无 worldgen（桶源 = 无
  蜂箱偏离）。per-kind 容器 3→4【PITS 299：kind 1 基下标 = 尺寸须
  种类数+1，静默 OOB 段错在下游测试】+ CellReader 4 元组 + mesher 三
  种类派发 + 隐藏 starter 槽 16【PITS 300：storage 范围 [9,26]】+
  atlas 空位 [15,1] 免重锚。1774/1774（+8：编码带/门控冻结/落速/扩环/
  蜜水共存/他种支撑/门控不影响水/蜜顶面无波纹）、-Werror 0；门 126：
  S1 第 126 次结构性免疫免重锚（四方截图 ×2 387726 B + savenorm
  new×2/old×2 EXACT；new vs old 存档差 = 恰好 2 项 honey_bucket
  starter 内容演进）+ S2 六场景 PASS【A 蜜倒 new×2 RAW 407951：蜜 L8
  + 邻 L7 封源池稳态 + 空桶 / W 水回归 new≡old 字节一致 409647 = 蜜
  代码惰性零回归 / B 落蜜 40f 井中 (24,39) oracle 逐位 ×2 RAW 708974
  / C 落水 40f 落底 (24,37) ×2 RAW = **B vs C 1/3 速证明** / D 无反应
  行 600f 三源 8/8/8 零转化 ×2 RAW 699617】+ S6 old 二进制蜜场景
  init 点名 unknown fluid 'honey'；性能 A/B fluidCells win2/3 逐位 +
  PIPE3 changesApplied 逐位 + 全同档 = 成本可忽略。**组 1.6-1.10 已
  收官（v1.10.0 tag + release）；v1.11.0 = 新组 1.11-1.15 首版**
  （组内不打 tag）。
- 2026-10-28：**FLUID-5 水熔岩反应完成**（v1.x 第 10/33 项，v1.10.0，门
  125，FLUID 行 2/3；消费 FLUID-4 范围外记档"水桶对熔岩面 no-op"；
  模拟侧 reaction pass P51 已完整 = 零新反应规则）：纯函数
  `bucketPourTarget` 扩展异种两规则——① 水桶对熔岩格 = 反应倒（水源放
  candidate+受击法向一步的干空气格【无 block 命中 = +Y 约定】，既有
  reaction pass 下 tick 转 obsidian，无空气位 = no-op 桶保留）② 熔岩桶
  对水格 = 替换倒（setFluid 既有"替换他种"语义，下 tick 按残留邻水转
  obsidian/stone）；game 写路径零改动。1766/1766（+10 改 1）、-Werror
  0；门 125：S1 第 125 次结构性免疫免重锚（四方 ×2 387726 B + savenorm
  EXACT）+ S2 三场景单次右键输入跑 PASS【A 反应倒：new obsidian+空桶 /
  old no-op ×2 RAW 对；B 替换倒：new obsidian+水留存+空桶 / old no-op
  ×2 RAW 对；C 守卫：placement 持流体 no-op 桶保留】+ 场景 A 双层周界
  墙稳态化【首版无墙 ×2 RAW MISMATCH = PITS 297，CPU 收敛探针 5 tick
  稳态】+ 性能 A/B fluidCells win2/3 逐位一致 + 全同档 = 成本可忽略；
  PITS 297-298（场景流体稳态 / getFluid 返回 level 非编码）。**组边界
  v1.10.0 = tag + GitHub release**（覆盖 GEN-7/8/9 + FLUID-4/5 组内 5
  版；10-28 定案 tag 策略）。
- 2026-10-28：**FLUID-4 倒向水面完成**（v1.x 第 9/33 项，v1.9.0，门
  124；消费 P47 桶记档偏离"倒水只能倒向方块受击面"）：纯函数
  `bucketPourTarget`（world/raycast：候选 = block 射线受击面邻格【旧
  放置位级不变】或（block 射线无命中）fluid 射线命中的格；接受空气无
  流体或桶自身种类流动格 level<8 → 变源；已源/异种 = no-op 桶保留 =
  MC parity）+ tryFluidBucket pour 分支重接（写路径零改动）；零 format
  （19）/config/内容/音效。1756/1756（+11）、-Werror 0；门 124：S1 第
  124 次结构性免疫免重锚（四方 ×2 字节一致 + savenorm EXACT）+ S2 单
  次右键输入跑 PASS（封源池受控场景：new 流动 L7→L8 + 桶→空桶 互斥
  终态 ×2 / old 恒 no-op ×2 / S2b 已源格 no-op 桶保留 + ×2 RAW/侧 +
  A/B diff = 手持/热栏图标 + 池区水面）+ 性能 A/B 同 seed 900f
  fluidCells win2/3 逐位一致 + 全同档 = 成本可忽略；PITS 295-296
  （右键只在指针锁定时执行 = 输入跑先左点捕获轮询 / 同槽双进程终档
  竞态 = 断言前验 clean exit）。只提交（组内不打 tag，10-28 定案）。
- 2026-10-28：**GEN-9 耕地湿润完成**（v1.x 第 8/33 项，v1.8.0，门
  123；代理定案 = farmland 湿/干视觉状态，GEN 行 3/3 收尾，原始定义
  已佚）：块模型可选 `moisture_top`（FullCube-only 形状门控 + 严格非空
  串 + requiredTextures init 交叉校验）+ 纯谓词 `soilIsWet`（Chebyshev
  4 同层水，P50 生长节奏同款半径，cropIsWet 委托 = 一函数两消费者）+
  mesher FullCube 顶面 mesh 时派生（湿/干邻格永不贪婪并 quad）+ Game
  `m_moistureBlocks` 追踪 + **kMoistureRefreshEveryNFrames=10 周期
  sweep**（worker 快照预烘 stale 收敛 + seed 点强制重烘）；shipped =
  farmland `moisture_top: farmland_wet`（atlas 空位 [9,4] 免重锚 + 同
  salt 暗化 0.72×）；零 format（19）/config/音效。1745/1745（+12）、
  -Werror 0；门 123：S1 第 123 次结构性免疫免重锚（fresh 60f 四方 ×2
  字节一致 + savenorm EXACT）+ S2 受控平台场景双视角 PASS（俯视 diff
  108,542 px 全限耕地顶面 100% 更暗 old (102,72,49)→new (72,52,36)
  纹理均值精确 / 侧视 888 px 隔离 + 干耕地带零 diff + 场景区存档
  EXACT）+ 性能 A/B 同 seed 900f（fluidCells win2/3 逐位一致 + PIPE3
  逐位 + 全同档 = 成本可忽略，零耕地 sweep 结构惰性实证）；坑 PITS
  291-294（L8 源必封 / 大场景远景 GL quirk【新旧同现非回归，门改受控
  场景】/ zsh 无引号展开不分词 / 场景构建器静默未写入）。只 tag。
- 2026-10-27：**GEN-8 地形互感完成**（v1.x 第 7/33 项，v1.7.0，门
  122；代理定案 = 山峰互感 mountain passes，原始定义已佚）：worldgen
  `terrain.passes`（enabled/noise_scale/threshold/amplitude 逐条点名，
  缺省 = disabled 位级 legacy）；coarse ridge mask 纯函数
  （独立低频 fbm2 0.008/3 oct/kSaltPasses，>threshold 时 bonus 0..amplitude
  只升不降，heightAt = min(base+bonus, cap)）；shipped {0.008,0.55,6}
  探针单轮定案；首版局部峰设计废弃（lround 平顶 = PITS 290）；1733/1733
  （+11）、-Werror 0、4 契约演进重钉（矿计数 3 + 洞穴探针 3，方向 pin
  全保持）；门 122：S1 第 122 次结构性免疫免重锚（三方 ×2 六跑字节一致
  + demo 242 bonus 列全在视锥后方探针论证）+ S2 山脊盆地排水判别 PASS
  （off≡old 字节一致 + diff 525,506 px old 30.5% 水域→new 0% 100%
  地形 + player EXACT）+ 性能 A/B 同 seed 900f 代码成本可忽略；坑 =
  Firefox 同名窗平铺分半 + raw string 吞 TEST 宏。只 tag。
- 2026-10-27：**GEN-7 基岩多层完成**（v1.x 第 6/33 项，v1.6.0，门
  121）：worldgen `terrain.bedrock_layers`（int [1,8]，缺省 1 = 旧行为
  位级复现）；shipped 3 = MC 经典基岩底（y0/y2 恒床岩 + y1 每格确定性
  50% roll `bedrockLayerRoll` 纯谓词，kSaltBedrock）；fillTerrain 单
  分支；零 format（世界生成演进）。1722/1714→（+8）、-Werror 0；GEN-3
  矿计数契约演进重钉 4 值；门 121：S1 第 121 次结构性免疫免重锚 + S2
  竖井壁带判别（diff 13172 px 93.2% 横带 + 99.0% new 更暗 95.9→42.1
  解析吻合 + savenorm EXACT + ×2 RAW/侧）+ 性能 A/B fluidCells 三窗
  逐位一致 + 全同档 = 成本可忽略；PITS 289（init 失败后旧产物残留假
  PASS）。只 tag。
- 2026-10-27：**LGT-5 半块透光完成**（v1.x 第 5/33 项，v1.5.0，门
  120）：slab/stairs×4/wall/fence 7 块加 `light_opacity: 0` = MC 半块
  透光规则（光如空气穿过：竖直天空免费 + 每格成本 1 = 与空气逐值同），
  **零核心代码**（LGT-2 统一进入格成本消费面 +1）；opaque 保持 true =
  剔面/渲染零变更（P85 局限保留记档）。1714/1714（+3：内容钉死/竖直
  天空透光/密封盒块光层透）、-Werror 0 警告；门 120：S1 第 120 次结构
  性免疫免重锚（半块永不生成）+ S2 密封石室唯一开口 slab 判别：old 室
  恒暗 (10.6) vs new 光穿半块灌亮全室 (76.8)，diff 945782 px 0 px 变暗
  = 纯光增益 + savenorm EXACT + ×2 RAW/侧 + 性能 A/B 同起跑态 fall 900f
  fluidCells 三窗 229457/218800/221654 逐位一致 + 全同档 = 成本可忽略；
  PITS 288（场景构建覆盖 base 块编辑 = 孤儿 fires 交叉校验 init 死）。
  **release 组 1.1-1.5 组末 = 本版本发 GitHub release**（notes 覆盖
  v1.1.0-1.5.0）。
- 2026-10-27：**LGT-4 光束入光完成**（v1.x 第 4/33 项，v1.4.0，门
  119/119b）：信标光束（最后一个恒白 unlit 消费者）经 PRE-6 per-draw
  光照覆盖范式接收世界光——`beamLightSample` 纯函数采样**信标块上方空气
  格 (x,y+1,z)**（= beam 最低格；首版采样块自身格 = opaque 恒 (0,0) 缺陷
  被门 119 E2E 揪出【GPU uniform readback + on/off 双二进制 diff=0 取证链】
  修复 + OwnCellOpaqueAlwaysIgnored 回归单测）+ u_drop* 三 uniform 改名
  中性 + 每 beam draw 后 bind+复位 0（UXFIX-1 纪律）；深夜月光 7/15
  （LGT-3 仿射继承）/近萤石亮化/白天恒白（df=1 恒等根）；零
  format/config/内容；1711/1711（+7）、-Werror 0 警告；门 119：S1 第 119
  次结构性免疫（最终二进制复验）+ S2a 首版场景 beam 失明废弃【pitch 0.15
  取景不到 96 块高近天顶 beam 柱，PITS 287】+ S2b 门 119b（pitch 1.25）：
  day on/off sanity 1646 px + day df=1 恒等 RAW + night beam 列 1646 px
  全限竖直窄带仅暗化（带均比值 0.483 = 解析值，零亮化，带外 0 diff）+
  savenorm EXACT；性能 A/B 同起跑态 fall 900f：fluidCells 三窗
  214627/194500/230306 双侧逐位一致 + 全指标同档 = 成本可忽略。
- 2026-10-27：**LGT-3 夜光重传播 + MC 月光地板完成**（v1.x 第 3/33 项，
  v1.3.0）：呈现层昼夜缩放 → 天空光场对源等级仿射（无损）重演（所有天空
  光源共享 S + 逐格成本与 S 无关 ⇒ 有效 = max(0, stored−(15−S)) 逐格
  精确，confluence 单测钉死）= MC 同款日落撤退/黎明回涌，**零重传播**
  （shader 天空项一行 + spawn 光门整数镜像 + propagateAll 源等级 seam 生产
  恒默认）；**用户实机定案（10-27 夜）MC 月光地板**：S = 7 + 8×dayFactor
  （kNightSkyLevel 7 = 午夜露天月光，深夜可夜视）+ 敌对 spawn_max_light
  0→7 MC 配对（夜露天保险/火把免疫/白天免 = 玩法闭环）；零 format/config/
  内容解析面。1704/1704（+9）、-Werror 0；门 118：S1 = 第 118 次结构性
  免疫免重锚（正午恒等）+ S2 深夜 A/B 828,043 px 全仅亮化 + 黄昏同数
  diff（时间不变集）+ open-sky 锚盒零 diff + savenorm EXACT + 性能 A/B
  fall 900f 全同档 + fluidCells 逐位 = 零成本；PITS 284-285——详见
  docs/archive/phase-lgt3.md。
- 2026-10-27：**PIPE-3 流体 per-cell 完成**（v1.x 第 2/33 项，v1.2.0）：
  99.8 chunk 粒度活动门控 per-cell 化——per-chunk per-kind 流体格列表
  （setFluidAt 成员变化 push / drain lazy stale / walk 过滤+列序+去重+
  压缩，不变式 = 活格 ⇒ ≥1 条目）+ World per-kind 活动 chunk 集合（6
  插入点穷尽 flag 置位点 + eraseChunk purge，walk 消费，flag 双检 belt）
  + falling 标志 per-column 化（colFlag[256] + 跨 chunk topFlags[256B]，
  引用语义位级同旧）+ reaction/burn 活格扫 + propose map max 合并（序
  无关）；tick = O(活动 chunk + 活流体格)（原 O(visited×32K×2 + 全流体
  集合遍历)）。流体状态逐位不变 = 门 117 S2 位级一致 oracle（fall/fill
  跨侧 savenorm EXACT + 截屏 RAW 字节同；cascade nospread 位稳 oracle
  存档 RAW 854590 B 逐字节同；fire 区 1 格差 = 同 OLD 二进制自分歧实证
  = 既有 real-dt 类 PITS 283 非回归）；1695/1695（+9：列表维护/跨 chunk
  边界/活动集生命周期 ×4/reaction-burn 收集/churn 有界/手算参考/belt
  消费）、-Werror 0 警告；门 117：S1 = 第 117 次结构性免疫免重锚（四方
  fresh 60f 字节一致 + base18 锚连续）+ 性能 A/B（用户硬要求）：fall
  流体段 1261/2486/3785 → 631/1267/1880 ms/300f 窗（−50%）、活跃 tick
  34.8→12.6 ms（2.8×）、fill churn 窗 1.5-2.3→0.3-0.4 ms（−80%）、沉降
  下界 0.19-0.30→0.10-0.16 ms（−50%）、fluidCells 逐位一致 + 帧指标全
  同档 = 纯收益零回退；顺带修 setFluidAt 潜伏 debug assert（lava 编码
  9..16 > kMaxFluidLevel；NDEBUG 零变化）+ test_mesh_job static_assert
  契约演进（vector 成员 → copy/move-constructible）；PITS 280-283
  （场景拷贝假绿 / pgrep 自匹配第 4 次 / 黑窗数分钟周期 / fire 蔓延非位
  稳类）——详见 docs/archive/phase-pipe3.md。
- 2026-10-27：**STRM-2 流体列保留完成**（v1.x 第 1/33 项，v1.1.0）：驱逐
  journal diff 快照按列保留（retainColumnDiff = journalDiffChunk 字节同语义、
  保留 diff 条目非 chunk = 内存有界）+ 再集成回放（blocks 先 fluids 后、不插
  m_editedColumns = fluid-only 列保持可卸载）+ 退出存档双路径 mergeRetained
  （--save-full-diff oracle 不失真）+ 零 format（19）/config/内容；**随门 116
  场景首触发修复 99.8 活动门流送边界活性缺口**（冷流前对 void 被拒后目标列
  到达不唤醒 = 级联永久卡边，395 tick 插桩实证；修 = insertChunk/
  getOrCreateChunk 的 wakeFluidChunks 镜像 setBlock UNSTICK，唤醒仅在插入
  事件 = 99.8 性能面零回退）。1686/1686（+9：保留 diff 6 + 活性判别 3 含
  pre-fix FAIL 实证）、-Werror 0 警告；门 116：S1 = 第 116 次结构性免疫
  （四方 new≡old 字节一致；base18 锚刷新 ca639e50→9c64b0f8 = PITS 265 机器
  态）+ S2 三跑全 PASS【Run A 卸载保留：NEW 10 格 C 膜（对角扇）×2 在档 /
  OLD ×2 缺，玩家四跑终位逐位同 (22.3,38,0.5)，渠道对照 3/3 双侧在 + C 列
  零 block 编辑；Run B 读档回放：NEW 在 / OLD+CTRL 缺，×2 savenorm EXACT +
  CTRL≡OLD 惰性；Run C 会话内 re-apply：XTEST 标定 180° 转向 + F5 位置探针
  双相步行，NEW ×2 膜在（西 32.4 卸载 → 东 165 回流回放）/ OLD ×2 缺，
  四跑步行轨迹逐探针一致】+ S3 1686 + 性能 A/B fall 900f：upd/render/swap
  同档或更优、flush −3..−10%、stream −3..−6%、fluidCells 123879 逐位一致 +
  167781→167782 / 236573→236575（+1/+2 = 99.8 修复行为语义：静态梯度提前
  一格越流送边界，非 churn 放大）= 成本可忽略；场景物理坑 4 项（泳态
  stepUp 门 / 悬浮草洞 / 桥接近侧 / 转向 z 漂移三行车道）——详见
  docs/archive/phase-strm2.md；PITS 271-279。
- 2026-10-27：**v1.0 发布 + 矩阵刷新**。34/34 全完成（tag v1.0.0 14e793d，
  原 tag v1.0 同日改名 + GitHub 正式 release "TokenMine.cpp v1.0.0"；版本号
  bump 0.15.0→1.0 收口二进制内版本字符串滞后）。用户定案清理：v1.0 完成项移入「v1.0 完成
  清单」（详情在 phase 存档），砍掉清单 34 项回填 v1.x 矩阵 = **33 项**
  （PIPE-4 维持不立项）+ 冲突重编号（GEN-7/8/9、FLUID-4/5/6）+ UX 行保留
  实机报障入口。


- 2026-10-20：建立。63 项（文档全量后续任务分类）→ 用户决定每类砍半
  至 29 项 + 10-25 用户新增 2 项（GEN-4/5）+ 10-25 交付上报 1 项（GEN-6，执行序 SURV-5 后）= **32 项 = v1.0 范围**；执行顺序与前置约束见上。
- 2026-10-20：表外文档债已解决（KNOWN_ISSUES 9 条过期描述 + 连带 obsidian
  括注，逐条代码复核；6 条标已解决 / 3 条改写为活条目 / ⑨ 复核 = 99.2 已解决）。
- 2026-10-21：STRM-1 完成（用户定案选项 b：圆盘 + 远平面帽 R_eff=64；实测
  vd80 9.08→4.59 GB −49.5%；base18 第 81 次门）。
- 2026-10-21：PIPE-1 完成（worker 快照网格化；step-0 重测定案做；base18
  第 82 次门）。
- 2026-10-21：PIPE-2 完成（存档增量 journal；与 diffWorld 字节等价；
  vd80 全环退出 6.2 s → **17 ms**（journal 路径；3600f 场景 legacy 对照
  3.28 s ≈ 190×，退出成本 = O(会话编辑数)、与环大小脱钩））。PIPE 行 2/2
  全清，v1.0 任务表
  3/29 完成）。
- 2026-10-21：LGT-1 union-region **砍除记档**（step-0 实测：可合并帧
  6/6418 = 0.1%、预期节省 0.002 ms/帧 ≪ 1 ms 砍除线——同帧选列 (distSq,x,z)
  平局按位置序配对远隔列，设计前提被证伪；PIPE-1 先例翻转纪律触发，
  用户确认砍除；设计过程发现 1 个潜伏 sky rescan clobber bug 记 PITS；
  详见 `archive/phase-lgt1-flip.md`）。
- 2026-10-21：LGT-2 完成（叶子光衰减：light_opacity 0-15 字段缺省由 opaque
  派生 = 未设内容逐位复现旧行为 + 统一进入格成本 + leaves 2 / glass 1；
  base18 第 83 门 = 重锚【首个光值类重锚，A/B diff 限定冠/冠下表面、天空
  零变化】；封盒 S3a 全帧新旧字节一致 = 派生 O 的 E2E 级证明；1226/1212；
  S4 输入放置留用户在场补跑）。LGT 行 2/2 全清（1 砍 1 完成），v1.0 任务表
  推进至 GEN 行。
- 2026-10-21（夜）：GEN-1 完成（面条洞穴：两 3D 噪声中位带相交隧道 +
  洞口 surface_guard 0 + spawn_clear_columns 2 + 每湖 lava 带排除盒；标定
  零调参轮定稿；gen 1.17 ms/列；1240/1226；门 84 结构性免疫不重锚【新 ×2
  与 gate 83 锚字节一致】；E2E 批按用户要求压缩 = S2b 进洞走路推迟，
  其余全过）。
- 2026-10-22（夜，夜间自主推进）：ENT-2 完成（BlockState：P94 楼梯朝向
  泛化为通用 per-instance 状态机制；format 15 states 段 + 14→15 值保真
  迁移 + init 不变量【worldgen 放置源禁 stateful】+ 双门控数据驱动
  state_count/state_default；1260/1240；门 85 结构性免疫不重锚；E2E A/B
  298k px = 朝向首次像素级验证；晨间潜旋回归推迟）。发布 **v0.3.0**。
- 2026-10-22（夜，夜间自主推进）：GEN-2 完成（树生长：3 树种 sapling +
  tree 对象 + m_crops 平行生长引擎 + tree_shape 纯几何 3 形状 + emitCross
  stage 缩放 + 落树 placeGrownTree + 内容闭环 leaves 掉落/隐藏 starter；
  1285/1260；门 86 结构性免疫不重锚；E2E 探针预计算选点 + 落树 ×2 RAW +
  stage 4 逐帧一致 + A/B 324k px + S6 点名；世界生成零触碰）。发布
  **v0.4.0**。GEN 行 2/3（剩 GEN-3 矿脉形态）。
- 2026-10-22（夜，夜间自主推进）：GEN-3 完成（矿脉形态：随机游走 → MC
  3×2×1 旋转椭球 21 格脉 + vein_size = 簇数 K∈[1,8] shipped 全 1 + gravel
  同改 + 单轮密度标定【diamond/lapis 残留记档】；1297/1285；门 87 结构性
  免疫不重锚；E2E 洞壁矿 A/B 6,703 px【场景火把 + 真眼高 pitch，PITS
  #209 旧二进制 A/B 资产 scratch 纪律】）。发布 **v0.5.0**。**GEN 行
  3/3 全清**，v1.0 任务表推进至 FLUID 行（FLUID-1 设计已定稿）。
- 2026-10-22（夜→23 晨）：FLUID-1 完成（水量守恒：根因三路径无记账；
  step-0 探针触发翻转【统一吸收 = 崖膜永久 churn 6.4×】→ 路线 A = 只删
  抬升一分支 + 静态沉积稳压器保留；零 format【level 即 flow time 反相】；
  会计恒等式 ΔF=σ−α−κ−δ 逐 tick 单测；1312/1297；门 88 结构性免疫不重
  锚；E2E 封闭盒 64 格精确终态 + P90 新旧二进制 core identical）。发布
  **v0.6.0**。FLUID 行 1/2（FLUID-2 设计 ora 运行中）。输入 E2E 积压
  （重启后桌面态：root-top 点击 + xdotool 键名 Left_Control 失效 → 已修
  Control_L + xprop 回退，等窗口重跑）。
- 2026-10-23：输入 E2E 积压清理（本机 T450，原 10-23 定案"留新机器"
  的积压在本机提前清完）：7 项全部验证输入链正常（lock/crouch/
  click/hold/walk），**零产品代码缺陷**——失败全部 = 环境（i915 GLX
  间歇黑相，PITS 有录：黑帧跑存档完全有效 → 输入跑按协议以存档为权威，
  take 门放宽为"帧满+存档"）或场景设计缺口。完整协议 ×2 savenorm
  EXACT：**ENT-2 潜旋**（states 1→2 + 相机钉死）/ **GEN-2 潜旋催熟**
  （states 0→1 精确 + 树苗保留 + ×2 EXACT）；功能性通过：**GEN-2 放置**
  （放置 ✓ 槽消耗 ✓ ×2 savenorm OK；stage 漂移 = 生长引擎活跃，"stage 0"
  断言需暗室场景重建——留新机器）/ **GEN-2 破坏**（块消失 ✓ 状态清 ✓
  掉落→拾取 ✓ slot 1→2；×2 受流送环自然世界 fps 耦合 churn 阻塞——留
  新机器）/ **GEN-3 挖矿**（矿消失 ✓ 石保留 ✓ coal 掉落实体 ✓；×2 同
  churn + 断言 kind 字段 bug 记档——留新机器）；部分：**GEN-1 进洞**
  （走位输入 ✓ ×2 逐位一致 z=4.7 撞墙停；阈值 5.5 不可达 = 场景走廊
  几何死路，需重建——留新机器）/ **LGT-2 放叶**（点击送达 ✓，放置被
  plant_on 正确拒绝 = 瞄准打石地，场景瞄准重建——留新机器）。harness
  修复 8 处 + 场景 format 15 修正 3 个（均入资产 tarball，PITS +5）。
- 2026-10-23（夜）：FLUID-2 游泳划水完成（被动上浮 + 水面漂浮几何
  钳制 + 潜行下沉；5 处设计案偏差机制级合理；1328/1312；门 89 结构性
  免疫不重锚【与门 88 状态锚字节一致】；S2/S2b y==47 深度无关 ×2
  EXACT、S2c y==42 互斥 ×2 EXACT【场景预置修正】；S3 输入三项留新
  机器）。发布 **v0.7.0**。FLUID 行 2/2 全清，v1.0 任务表 11/29。
- 2026-10-24（夜）：ENT-1 四肢摆动完成（swing 相位 per-mob 整数 +
  constexpr 16.16 定点正弦表零运行期 libm + `swings` 数据段严格解析 +
  CPU 逐帧顶点变换 per-mob 动态 VBO + config mobs.limb_swing；chicken
  2/cow 4/sheep 4/spider 8 腿，zombie·skeleton 无臂记档；AI/物理/存档
  零改动；1367/1350 +17、0 警告）。ENT 行 1/4，v1.0 任务表 12/29。
- 2026-10-24（夜，补记）：99.10 光照 job 并行 + 99.11 流体光照异步化
  发布（性能系列，非任务表项；99.11 = 用户卡顿深层根因修复——流体 tick
  巨盒主线程同步 re-light → 列粒度待集 + 每帧 2 列预算 worker 追赶；
  vd16 填充 244.7→44.6 s 5.5×、静置 26× 实时、update p95 7.6 s→80 ms）。
  发布 **v0.8.0 / v0.9.0**。
- 2026-10-25：ENT-1b 模型修复（用户实机反馈：四足 body 长>高对齐 MC
  比例带 + 关节出体 + 摆幅下调 + 蜘蛛腿加长外移 + 穿模谓词
  checkSwingClip 全相位 896 验收；zombie·skeleton 人形未动）。
- 2026-10-25：ENT-3 mob 踏阶完成（step-up 纯几何上提 Entity::stepUp，
  player 行为逐位不变 = test_player 零改动钉死；mob 镜像调用
  pre-substep 捕获 + onGround 门 + 本 tick 距离重放 ≤0.633 无穿透；
  判定与 player 完全一致 = P99 TRUE-1-block：slab 0.5/满块 1.0/玻璃板
  可踏、墙/栅栏 1.5 不可踏；帧锁定纯几何无 real dt；零 format/config/
  内容；1376/1367 +9、0 警告；门 93 竞技场三态互斥【new+台阶 zombie
  跨越击杀玩家 hp<20 / old v0.10.0+台阶 hp=20 卡住 / new+墙 hp=20
  不可踏；×2 字节一致 + savenorm EXACT】+ 摆动可见性 157,158 world px
  on/off 隔离 + base 门对锚 + 插桩性能 render med 0.47-0.60 ms）。
  记档 2 个 P16 潜伏 quirk（+X/+Z 恰 1 格宽台阶踏不过 / slab face
  巧合抬升——player/mob 共享，修复 = player 行为变更单独立项）。
  ENT 行 2/4，v1.0 任务表 13/29。发布 **v0.11.0**（含 ENT-1b）。
- 2026-10-25：ENT-4 水体交互完成（玩家 FLUID-2 无输入孪生上提 Entity 共享；
  mob 涉水/水下 ×0.6 + 禁 step-up + 漂浮不动点；item 浮面静止 + 熔岩对称；
  箭不动；陆上逐位不变；1400/1400（+11）、门 94 全绿【base 对锚 + item
  39/36 exact 互斥 + 涉水三态互斥】、插桩 A/B 成本可忽略；场景设计三轮
  失效根因链 + 工具定案 tm_scene_check/tm_run_lib PITS 243-245）。
  ENT 行 4/4 全清，v1.0 任务表 14/29。发布 **v0.12.0**。
- 2026-10-25：SURV-1 护甲完成（12 件 MC 精确值 + floor 减伤全源含坠落 +
  右键穿戴/交换/脱下 + format 16 + HUD 护甲排；1430/1430（+21/+9）、
  门 95 全绿【base 对 gate88 锚 + 坠落 10.0/15.6/18.0 三值互斥 ×2 RAW +
  穿戴/整套脱下 RAW+savenorm2 EXACT + 护甲排 81 px 包围盒隔离 + S6 ×2】；
  门内修复脱下 dangling reference → 纯函数 item/armor + 9 单测 PITS 246；
  T450 输入送达协议 PITS 247；插桩 A/B 成本可忽略）。
  SURV 行 1/5，v1.0 任务表 15/29。发布 **v0.13.0**。
- 2026-10-25：SURV-2 氧气窒息完成（帧锁定氧气 150/6 帧/150 帧 2.0 伤 +
  眼位单格水/熔岩判定 + 溺水豁免护甲 = MC 伤害调整表实证 + format 17 +
  HUD 气泡排替换饥饿排；1448/1448（+18）、门 96：新旧 A/B 基线字节一致
  + 死亡 RAW 对 + savenorm3 ×5 + 豁免死/活互斥 + S6 ×3；S4 像素隔离 =
  晨间补跑【黑相波 PITS 250】；插桩 A/B 成本可忽略）。
  SURV 行 2/5，v1.0 任务表 16/29。发布 **v0.14.0**。
- 2026-10-25：SURV-3 耐久完成（27 件 MC 精确值 + ItemStack.damage +
  五类帧锁定磨损【护甲 −max(1,incoming/4) float 精确、溺水 bypass 不
  磨损】+ format 18 + HUD 2px 三色耐久条；1466/1466（+18）、门 97：
  S1 新旧 A/B 免疫 + S3 盔 2.5 exact + S3b 豁免死/恒 0 互斥 + S6 ×3；
  输入跑 S2/S4/S5 = 窗口相位关闭补跑清单；场景坑 PITS 251；插桩 A/B
  成本可忽略）。
  SURV 行 3/5，v1.0 任务表 17/29。发布 **v0.15.0**。
- 2026-10-25：SURV-4 药水饮用完成（饮用通道给效果 + instant_hp 瞬时原语
  + 6 药水 + 瓶-水循环；1485/1485（+19）、门 98 S1 免疫 + S6 ×5；输入跑
  10-26 补收全 PASS）。任务表 18/29。发布 **v0.16.0**。
- 2026-10-25：GEN-4 洞穴破碎度完成（用户报障"洞穴太多太碎"：noise_scale
  0.09→0.05 单参数重标定，pocket −91%；门 100 免疫）。任务表 19/31
  （10-25 用户新增 GEN-4/5 = 分母 29→31）。发布 **v0.17.0**。
- 2026-10-25：GEN-5 浮空树完成（用户报障"树长半空"：caveCarvesCell 纯谓词
  守卫；门 101 免疫 + S2 补收 PASS）。任务表 20/31。发布 **v0.18.0**。
- 2026-10-25：SURV-5 mob 效果完成（wither/poison 双载体 + 不死族免疫；
  1519/1519（+25）、门 102；S3/S4 = 补跑清单）。SURV 行 5/5 全清。任务表
  21/32（10-25 交付上报 GEN-6 = 分母 31→32）。发布 **v0.19.0** + release ③。
- 2026-10-25：GEN-6 树放置半区修复完成（交付上报 bug 立项：wz 循环上界
  X 基→Z 基 2 处一行级；1521/1521、门 103 免疫 + S2 树群 A/B PASS）。
  任务表 32/32 执行序 22 项。发布 **v0.20.0**。
- 2026-10-26：CRAFT-1 工作台分块完成（per-block 容器 + format 19 tables 段；
  1535/1535（+14）、门 104 免疫 + S2 双工作台互斥 PASS）。任务表 23/32。
  发布 **v0.21.0**。
- 2026-10-26：CRAFT-2 物品 3D 模型完成（item JSON model 段 1..16 box +
  手持/掉落/进食三上下文；1549/1549（+14）、门 105 免疫 + S2 掉落 A/B
  PASS；H1/H2/H3 手持输入跑 10-26 补收）。任务表 24/32。发布 **v0.22.0**。
- 2026-10-26：CRAFT-3 材质音效完成（sound_material + SoundBank stereo +
  6 材质 ×3 事件 18 新 wav + 32 块赋值；1567/1567（+18）、门 106 免疫）。
  CRAFT 行 3/3 全清。任务表 25/32。发布 **v0.23.0**。
- 2026-10-26：SAVE-1 自动存档完成（120 s 真实 dt + 原子写三路共用；
  1590/1590（+23）、门 107 免疫 + S2 自动存档实证）。SAVE 行 1/1 全清。
  任务表 26/32。发布 **v0.24.0**。
- 2026-10-26：PRE-1 中文字体完成（UTF-8 渲染 + 32 CJK glyph 死格复用 +
  8 处 UI 中文化；1610/1610（+20）、门 108 免疫 + S2 死亡屏 A/B PASS）。
  任务表 27/32。发布 **v0.25.0**。
- 2026-10-26：PRE-2 边缘雾完成（流送盘缘距离雾，frag 雾 4 uniform +
  独立深度表达式；1621/1621（+11）、门 109 免疫 + S2 三方雾 A/B PASS）。
  任务表 28/32。发布 **v0.26.0**。
- 2026-10-26：PRE-3 水面波纹完成（水顶面 tiled 采样窗帧锁定平移；
  1631/1631（+10）、门 110 免疫 + S2 三方 A/B PASS）。任务表 29/32。
  发布 **v0.27.0**。
- 2026-10-26：PRE-4 云与环境音完成（云层平板 + 3 上下文环境音 + 3 新 wav；
  1648/1648（+17）、门 111 = 重锚【云入基线天空区】+ S2 PASS）。任务表
  30/32。发布 **v0.28.0**。
- 2026-10-26：PRE-5 透明 discard 完成（pass 三分 + cutout 20 块数据驱动 +
  frag 唯一 discard 行；1664/1664（+16）、门 112 = 重锚【叶冠修复入基线】
  + S2 叶孔判别 PASS）。任务表 31/32。发布 **v0.29.0**。
- 2026-10-26：PRE-6 掉落光照完成（vert 端 per-draw uniform 覆盖 + item
  中心格光照采样；1672/1672（+8）、门 113 免疫 + S2 掉落光照 A/B PASS）。
  PRE 行 6/6 全清 = **32/32 全完成 = v1.0 候选**。发布 **v0.30.0** +
  release ⑦。
- 2026-10-26：实机报障 2 项入表（用户定案，分母 32→34）：UXFIX-1 渲染状态
  泄漏（热栏/面板白盒 + 挖掘黑块，PRE-4 云 per-draw 状态泄漏 locked 态专属）
  完成，1672/1672、门 114 免疫 + S2a/S2b A/B PASS，发布 **v0.31.0**（任务表
  33/34）。
- 2026-10-26：FLUID-3 mob 涉水修正完成（用户实机报障"牛羊鸡悬空/站水"：
  游泳位 + 深度门 + 下沉 inFluid 门 + 踏阶解禁；1677/1677（+5 新/重钉 6/
  反转 1）、门 115；玩家位级不变）。**34/34 全完成 = v1.0**。发布
  **v0.32.0**。