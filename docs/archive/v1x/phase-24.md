# Phase 24 细节存档

> Phase 30 文档整理：由 `SESSION_CONTEXT.md`「Phase 24 已完成（细节存档）」节、`KNOWN_ISSUES.md`「已修复（归档）」条目 合并而成（未改写原文）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/...` 驱动脚本）指向当时状态；当前文档布局
> 见 `README.md`「文档地图」。

## 阶段报告（原 KNOWN_ISSUES「已修复（归档）」）

- **`[2026-10-05, Phase 24]` 弓无蓄力（P21）/ 无箭物品·无弹药（P21）/ 无插墙残箭（P10，P21 保留）/ 选中弓左键无近战（P21）** — Phase 24 回收 P21 简化项。**数据** `arrow` material 物品（max_stack 64，图标 `tools/generate_item_icons.py` 对角箭：2px 杆 + 钢头 + 浅色尾羽）+ shaped 配方 `arrow`（2 stick 竖排 1 列宽 → 2，首个 1 字符宽 pattern）+ 发射器物品新字段 `ammo`（物品名；`ItemRegistry::parseAmmo`：显式空串/非字符串拒绝——ABSENT 才是"无弹药"缺省；存在性在 init 时经 ItemRegistry 交叉校验，同 icon 文件检查模式）+ `bow.json` 加 `ammo: arrow` + `attack_damage: 1.0` + "bow" 弦音（180Hz 拨弦 + 噪声，0.15s → 13 WAV）。**代码** `core/bow.hpp`（纯函数 `bowPower(draw)=clamp(draw/1.6, 0.1, 1)` + `kBowFullDrawSeconds=1.6`/`kBowMinPower=0.1`，游戏与单测共享同一映射）/ `Input::wasReleased(MouseButton)`（释放边沿数组，frameBegin 清；**未按下的按钮收到 up 事件不设边沿**）/ `Projectile` stuck 态（撞 solid 由 `m_dead` 改 `m_stuck`：无物理、age 继续按 `kMaxAge` 30s 消亡、`shouldDespawn` 表达式不变；**`stuckDir` 每帧在子步前捕获 `normalized(m_velocity)`**——碰撞解算器 `moveAxis` 会清零碰撞轴速度，撞击时刻的速度已丢失，必须帧首快照；P10 头注释"撞墙钉住"本意，P10 测试 `dead()` 断言同步改 `stuck()`）/ Game：`handleInteraction` 右键分支 = 食物（P23）→ **发射器 → 起拉**（吞键：不 interact/不放置——Minecraft 弓语义；需 `ammoAvailable()`；播 "bow"）→ interact/place；`updateBowDraw`（真实 dt 累加、封顶 1.6s；**release 边沿 → `fireBow`**；按下同帧释放 = 最低威力 0.1 仍射；按钮 up 无边沿 = 取消）；`fireBow`（弹药 = **槽序升序第一个持有槽**扣 1，拉弓中途耗尽静默取消；速度/伤害 = 数据 × `bowPower`；眼前 0.3 格沿瞄准；播 "shoot"）；左键分支 = 发射器选中 → 准星 mob `updateAttacking`（物品 `attack_damage`，bow=1）且**永不挖掘**；蓄力清零点：解锁/Escape/`cycleHotbar`/`openPanel`（死亡经解锁路径覆盖）；`updateEntities` 扫掠命中**跳过 stuck 箭**（零位移 `raycastAabb` 退化）；`drawArrows` 朝向 = `stuck() ? stuckDir() : velocity()`；`renderUi` 手持图标拉弓位移（`kBowDrawPullPx=64` × power 向屏幕中心）；debug_overlay 版本行 Phase 24。**测试** 626/626（+11：InputTest 释放边沿 1[含 stale up 不设边沿]、ItemRegistryTest ammo 解析/缺省空/显式空串+非串拒绝 3、BowChargeTest power 映射 4[0→0.1/半程 0.5/满 1.0/过冲封顶/负值钳制]、ProjectileTest stuck 2[不动+计时消亡/保留撞击方向——初版断言"发射方向"失败：重力使飞行中方向持续弯曲，`stuckDir` = 撞击前瞬时速度方向（箭的实际入射角，比发射方向更正确），修正断言为水平入射+下弯]、RecipeTest 竖排 1 列 1）。**E2E**（`/tmp/opencode/p24_e2e.zsh`，存档 bowtest24：bow slot 0 + arrow×16 slot 1，nomobs config）：base18 门 ×2 字节不变；**满蓄力射击 ×2（7 次运行取 f1=f4、f2=f6 两对）与短蓄力 ×2（s1=s3）与对照 ×2 各自逐字节一致**——松键边沿的墙钟→帧钟 ±1 帧抖动（xact 启动相对场景构建的偏移 ±10~30ms）使射击用例偶发箭位 158/440px 漂移（全在箭带内），多跑取字节一致对解决；full vs ctrl 178 px 差（174 = 飞行箭 x[636,643] y[407,429]、4 = 热栏计数 16→15 x[512,517] y[700,704]，UI 外零差）；short vs ctrl 1002 px 差（998 = 插墙箭 x[608,647] y[544,610]——短蓄力 0.3s → power 0.1875 → 速度 2.81 插进身前 0.85 格草地，满蓄力 15 飞向天空，**蓄力缩放同屏验证**；4 = 计数）。**E2E 工具坑（重大）**：自制 PNG diff 脚本的逐像素比较循环 slice 终点公式错（`(y+1)*ch + x*ch`，正确 `start+ch`）→ y≥1 的 slice 全为空字节 → **全部"0 diff"为假阴性**，一度误判"截图读到陈旧帧/合成器冻结"（X11 外部抓帧、P23 参考二进制 A/B 等排查全是 diff 工具在说谎）；教训 = 新 diff 工具必须先用独立工具（magick raw）交叉验证解码与比较循环，且对一个已知有差的图像对验证。另 pdiff 首版硬编码 3 通道 stride（截图是 RGBA 4 通道）——同样先解码比对 magick 再信任。

## 会话存档（原 SESSION_CONTEXT）

提交 `4cf8746`（2026-10-05，27 文件 +588/−115）。

### 设计（P21 简化项回收：蓄力 + 箭物品/弹药 + 插墙残箭 + 弓近战）
- **蓄力（MC 式，右键）**：发射器选中 → 右键按下起拉（吞键：不 interact/不放置——弓语义；需 `ammoAvailable()`；播 "bow"）；按住累加真实 dt（封顶 `kBowFullDrawSeconds=1.6`）；释放边沿 → `fireBow`；按下同帧释放 = 最低威力 0.1 仍射（MC 快点击）。`power = clamp(拉弓/1.6, 0.1, 1)`——`core/bow.hpp` 头文件纯函数（游戏与单测共享同一映射，无 GL 依赖）。速度 = `shoot_speed × power`、伤害 = `shoot_damage × power`。**左键 = 弓近战**：准星 mob 走 `updateAttacking`（物品 `attack_damage`，bow.json 显式 1.0），**永不挖掘**（P21「选中弓左键无近战/挖掘」解决）。蓄力清零点：解锁/Escape/`cycleHotbar`/`openPanel`（死亡经解锁路径）。拉弓渲染 = 手持 2D 图标按 `kBowDrawPullPx=64 × power` 向屏幕中心拉（v1 指示，无 3D 手）。
- **箭物品 + 弹药**：`arrow` material（max_stack 64，图标 = 对角箭：2px 杆 + 钢头 + 尾羽）；shaped 配方 2 stick 竖排 1 列宽 → 2（首个 1 字符宽 pattern，验证解析器 1..3 字符/行成立）。发射器新数据字段 `ammo`（物品名）：`ItemRegistry::parseAmmo` 显式空串/非串拒绝——ABSENT 才是"无弹药"缺省；存在性在 game init 经 ItemRegistry 交叉校验（同 icon 文件检查模式；ammo 物品可能在任意文件，不能逐文件校验）。发射扣**槽序升序第一个持有槽** 1 个；拉弓中途耗尽 → 松键静默取消。
- **插墙残箭（P10 头注释"撞墙钉住"本意）**：`Projectile::afterSubstep(blocked)` 由 `m_dead` 改 `m_stuck`：无物理（update 早退，age 继续）→ 按现有 `shouldDespawn`（m_dead || age≥kMaxAge || 杀界）30s 消亡（MC 100s → KNOWN_ISSUES）。**`stuckDir` 每帧在子步前快照 `normalized(m_velocity)`**——关键细节：`Entity::moveAxis` 撞墙时清零碰撞轴速度，撞击时刻速度已丢失该轴（撞地的箭会渲染成水平）；帧首快照 = 撞击方向（实际入射角，比发射方向更正确——单测初版断言"发射方向"失败，重力使飞行方向持续弯曲）。`drawArrows` 朝向 = `stuck() ? stuckDir() : velocity()`；扫掠命中测试**跳过 stuck 箭**（零位移使 `raycastAabb` 退化）。插住方块被破坏时箭留在原处（KNOWN_ISSUES）。
- **"bow" 弦音**：180Hz 拨弦正弦 exp(−22t) + 噪声层 exp(−35t)，0.15s → 13 WAV。
- **输入**：`Input::wasReleased(MouseButton)`（释放边沿数组 + frameBegin 清；未按下的按钮收到 up 不设边沿——WM 合成事件防御）。

### 测试（+11，626/626）
- InputTest 释放边沿 ×1（按下/释放边沿、frameBegin 清、stale up 不设边沿）
- ItemRegistryTest ammo ×3（解析/缺省空/显式空串+非串拒绝）
- BowChargeTest ×4（0→0.1 / 半程 0.5 / 满 1.0 / 过冲封顶；映射 = clamp(t/1.6, 0.1, 1)）
- ProjectileTest stuck ×2（不动 + 计时消亡 / 保留撞击方向：dir.y<0 证明是撞击前速度而非发射方向或清零速度）+ P10 两处撞 solid 测试 dead()→stuck()（位置断言不变）
- ShapedRecipeTest 竖排 1 列 ×1（pattern ["S","S"]）
- debug_overlay 版本行 → Phase 24

### E2E（驱动 /tmp/opencode/p24_e2e.zsh，p23 驱动改造；存档 bowtest24：format 4，bow slot 0 + arrow×16 slot 1，nomobs）
- base 60f ×2 与 base18 字节一致。
- full（240f：C1 锁定 → 抬头 → B3 拉弓 → 保持 2.68s ≥1.6 → U3 释放；~f225 射出）：7 次运行取 **f1=f4、f2=f6 两对字节一致**；vs full_ctrl 178 px 差（174 = 飞行箭 x[636,643] y[407,429]、4 = 热栏计数 16→15 x[512,517] y[700,704]，UI 外零差）。
- short（低头 + 0.3s 短拉弓）：s1=s3 字节一致；vs short_ctrl 1002 px 差（998 = **插墙箭** x[608,647] y[544,610]——power 0.1875 → 速度 2.81 插进身前 0.85 格草地；4 = 计数）。
- **蓄力缩放同屏验证**：满蓄力 15 飞向天空（箭在天空带）vs 短蓄力 2.81 插地（箭在地面带），同弓同场景。
- 对照 ×2（full_ctrl/short_ctrl）各自字节一致。
- **抖动**：释放边沿（U3）的墙钟→帧钟有 ±10~30ms 漂移（xact 启动相对场景构建的偏移随机器负载变）→ 射击用例偶发箭位 158/440 px 漂移（全在箭带内）→ 多跑取字节一致对解决。

### 本会话踩坑（重要，勿再踩）
1. **自制 PNG diff 脚本的逐像素比较循环 slice 终点公式错**——`a[y*w*ch + x*ch : (y+1)*ch + x*ch]`（应为 `start+ch`）：y≥1 时终点 < 起点 → slice 为空字节 → 所有比较"相等" → **所有"0 diff"是假阴性**。一度导致误判链：怀疑截图 glReadPixels 读陈旧帧（渲染 DBG 证明箭和计数都画了）、X11 `import -window` 外部抓帧（也"一致"）、P23 参考二进制 A/B（也"0 diff"——其实也是假阴性）。转折点 = 同一图像对 magick raw 158 px vs pdiff 0 px → 隔离验证解码（== magick ✓）→ 隔离比较循环（字节循环 158 vs slice 循环 0 → 发现公式错）。**教训：新图像比较工具必须先对独立工具（magick raw）交叉验证解码，再对已知有差的图像对验证比较循环。**
2. pdiff 首版硬编码 3 通道 stride（截图是 RGBA 4 通道）——同样的"解码 vs magick"交叉验证可提前发现。
3. E2E 时序标定跨会话失效：P21/P23 时代"xact 距场景构建 ~0.371s"在本会话不成立（实测 0.717~0.720s）——事件对应帧号须按每轮日志 "Scene built" 时间戳 + xact t 重算。
4. zsh：未加引号的变量在 for 循环不词分割（SH_WORD_SPLIT）；`python3 - <<'EOF'` 之后写的参数会当命令执行——脚本写文件。
5. `git worktree remove` 带 build/ 的 worktree 需 `--force`（未跟踪文件）；本会话 P23 A/B 参考 worktree `/tmp/opencode/p23ref` 已移除。

### 变更文件
- `src/core/bow.hpp`（新，蓄力纯函数）、`src/core/input.{hpp,cpp}`（wasReleased）
- `src/item/item.hpp`（ammo 字段）、`src/item/item_registry.{hpp,cpp}`（parseAmmo + 已知字段表）
- `src/entity/projectile.{hpp,cpp}`（stuck/stuckDir/update 早退/afterSubstep）
- `src/core/game.{hpp,cpp}`（ammoAvailable/updateBowDraw/fireBow/resetBowDraw、右键/左键分支、清零点、扫掠跳过 stuck、drawArrows stuckDir、手持图标拉弓位移、init ammo 交叉校验）
- `assets/data/items/arrow.json`（新）、`assets/data/items/bow.json`（+ammo +attack_damage）、`assets/data/recipes/arrow.json`（新）
- `assets/textures/items/arrow.png`（新）、`assets/sounds/bow.wav`（新）
- `tools/generate_item_icons.py`（+arrow）、`tools/generate_sounds.py`（+bow）
- `src/ui/debug_overlay.cpp`（版本行 Phase 24）
- `tests/test_input.cpp`（+1）、`tests/test_item.cpp`（+3）、`tests/test_bow.cpp`（新 +4）、`tests/test_entity.cpp`（+2 新 2 改）、`tests/test_recipe.cpp`（+1）、`tests/test_debug_overlay.cpp`（行更新）、`CMakeLists.txt`（+test_bow）
- README（弓节重写、音效 12→13、物品 24→25、配方 5→7【含 P21 漏记的 bow 配方】、物品 JSON +ammo、roadmap +P24 行、文件树 WAV 数修正）、KNOWN_ISSUES（弓节 4 条移除入归档 + 4 条新 v1 条 + P24 归档条）
- `config/config.json` 仍为用户手改未提交（view_distance 80 / day_length 600），与 P24 无关。
