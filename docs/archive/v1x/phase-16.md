# Phase 16 细节存档

> Phase 30 文档整理：由 `SESSION_CONTEXT.md`「Phase 16 已完成（细节存档）」节、`KNOWN_ISSUES.md`「已修复（归档）」条目 合并而成（未改写原文）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/...` 驱动脚本）指向当时状态；当前文档布局
> 见 `README.md`「文档地图」。

## 阶段报告（原 KNOWN_ISSUES「已修复（归档）」）

- **`[2026-10-04, Phase 16]` 无潜行/无 1 格自动台阶（P5）/ 虚空坠落=传送回出生点·无死亡机制（P5）/ 实体间互相穿过（P10，玩家↔掉落物部分）/ 无生命值·受伤·死亡 UI** — Phase 16 实现生存系统：`Player` 扩展 `maxHealth=20`（10 心）+ **坠落伤害**（`max(0, 净下落格数 − 3)`，离地帧记 `fallStartY`，落地结算，落水中取消）+ **虚空（y < −32）= 死亡** + 死亡冻结 + "You died! / Press R" + `R` 重生（回出生点满血、速度清零、**背包保留**）+ **自然回复**（脱离受伤 10s 后每 4s 回 1 HP）+ **受伤红闪**（`0.35×剩余/0.5s` 渐隐，`m_prevHealth` 轮询驱动，致死伤不重复播）+ **潜行**（`LeftControl`：身高 1.8→1.6 脚底不动、眼高 1.62→1.42、速度 ×0.3 水中除外、潜行不 step-up）+ **1 格自动台阶**（非潜行+贴地+贴整数块面+面前脚位 solid+上方 2 格空 → 抬 1 格继续本 tick 移动，X 轴优先、每 tick 至多一次）+ **玩家↔掉落物分离**（沿最小穿透轴把物品推开、远离玩家中心、面到面跨度，玩家不被动位移）+ **生命 HUD**（hotbar 上方 10 心 16px/2px，三态满/半/空取自 48×16 条带，填充 `clamp(hp−2i,0,2)`，仅游玩态渲染）。存档格式 2（`player.health`，省略=满；解析接受 1–2；`applySave` 映射 −1→maxHealth、0→死亡）。音效 +2（hurt/death → 10 样本）。**stepUp Z 轴回归 bug**（`cz` 误用 moveAxis 编号 `axis==2` 恒假 → Z 向永不 step，X 向正常）由新增 `ClimbsOneBlockAlongZ` 单测捕获并在同阶段修复（`axis==1`）。`addQuad` 拆分 `addQuadUv`（P15 遗留：`addQuad` 曾把 UV 留 (0,0)）。新增 15 个单测（PlayerTest 生存套件[坠落伤害精确/落水中取消/虚空死亡/重生满血清速留包/回复计时/潜行缩高+不 step/step-up X·Z 轴+2 格不抬+低顶不抬]、ItemEntityTest 玩家↔物品分离 ×3、SaveTest health 往返、InputTest R 键、DebugOverlayTest Phase 16 行）。E2E：base 逐字节不变；fall（4 格坑→hp 19.008842、心 10 半[左红(222,38,34)/右暗(55,57,70)]、心 1 满）×2 字节一致；step（Tab 泥土 B (0,54,-3)+Tab×8 石头 C (0,55,-3) 叠 B 顶+石头 A (0,54,-1)，W 1.5s 自动 step-up 上 A、跨越、1 格落下、被 B+C 墙钳 (0.5,54.0,−1.7) 精确）×2 字节一致。**驱动规则**：xdotool 鼠标按钮必须 `mousedown 1`/`mouseup 1`（`keydown 1` 是数字键"1"）；fall 瞄准 pitch −1.37（448px 下移，浅角度 −1.2 会削坑远壁）。新简化记入「生存（Phase 16）」节。

## 会话存档（原 SESSION_CONTEXT）

- **`src/player/player.{hpp,cpp}`** 生存扩展（`Player::Params` 新增，全部数据可调）：
  - `maxHealth=20` / `fallDamageOffset=3` / `crouchHeight=1.6` / `crouchSpeedScale=0.3` /
    `regenDelay=10` / `regenInterval=4`。
  - **坠落伤害**：离地帧（`onGround` 由 true→false 且非水中）记 `fallStartY`；落地帧
    （false→true）结算 `damage = max(0, 净下落格数 − fallDamageOffset)`，净下落 =
    `fallStartY − 落地 y`。**落水中落地取消伤害**（落地瞬间重判 `isInWater()`）。
    4 格下落净损 1 HP（E2E 实测 hp 19.008842）。
  - **虚空 = 死亡**：`belowKillPlane()`（y < −32）不再回出生点，改 `damage(maxHealth×1000)`
    走死亡流程。
  - **死亡**：`m_dead` 冻结玩家（不再 update 物理/输入移动），UI 显示 "You died! /
    Press R"；`GameKey::R` → `respawn()` = 出生点 + 满血 + 速度清零（**背包/实体保留**）。
  - **自然回复**：`m_timeSinceDamage`（受伤重置），≥`regenDelay` 后每 `regenInterval` 回 1 HP
    （到 maxHealth 停）。
  - **潜行**（`GameKey::LeftControl`）：身高 1.8→1.6（**脚底不动**，`m_position.y` 不变，
    只改 AABB height）、眼高 1.62→1.42、水平速度 ×`crouchSpeedScale`（水中不缩放）；
    **潜行时不 step-up**。
  - **1 格自动台阶 `stepUp`**：条件 = 非潜行 + `onGround` + 某水平轴身体**贴住整数块面**
    （`kFaceEps=1e-4`，碰撞 clamp 后贴面精确）+ 面前脚位格 solid + 其上方 2 格空 →
    `m_position.y += 1` 并**重放本 tick 该轴水平位移**（`moveAxis`，单步 <0.4 块隧道安全）。
    X 轴优先于 Z（固定顺序），每 `update` 至多抬 1 次。**回归 bug**：`cz` 曾误用 moveAxis
    编号 `axis==2`（循环轴只取 0/1，恒假）→ Z 向 step-up 永不触发、`cz` 恒为玩家自身格
    （air）→ 检查总失败；改 `axis==1`。`moveAxis` 编号是 0=x/1=y/2=z（step 循环轴 0=x/1=z，
    映射在 `moveAxis(axis==0?0:2)`）。
  - **玩家↔掉落物分离**（`Entity::setPosition` 新增）：game 的 `updateEntities` 在逐实体
    update 后、合并前跑一遍——玩家 AABB 与每个 ItemEntity AABB 相交则沿**最小穿透轴**
    （x→y→z 平手取先者）把**物品**推开（面到面跨度，方向远离玩家中心）；玩家自身不被动
    位移。
- **生命 HUD**（`renderUi`，`locked && !m_inventoryOpen` 门控）：hotbar 上方 10 颗心，
  每颗 16px、间隔 2px，居中。贴图 `assets/textures/health.png`（48×16，3 态横排：满/半/
  空，`tools/generate_health_icons.py` 生成）；心 i 填充 `fill = clamp(hp − 2i, 0, 2)`
  （2=满/1=半/0=空）。验证像素：中心行 **y=652**，心 i 左缘 x=553+18(i−1)；心 10 半 =
  左 (718,652) 红 (222,38,34) / 右 (724,652) 暗 (55,57,70)≈DARK(56,56,62)±8；心 1 满 =
  (562,652) 红。
- **受伤反馈**：`m_hurtFlash`（0.5s 渐隐，屏幕红 `0.35×剩余/0.5`）；game 轮询
  `m_prevHealth`（每帧比较 `player.health()`）：下降→红闪+播 "hurt"（**致死伤不播**），
  归 0→释放指针+播 "death"；重生复位。
- **存档格式 2**（`src/core/save.{hpp,cpp}`）：`player.health`（满血省略；`kSaveFormat=2`）；
  解析接受 format 1–2；`applySave` 映射 health −1→maxHealth、0→死亡（`m_dead`）、其余→
  clamp；`m_prevHealth` 读档同步（防加载误触发红闪）。
- **音效**：`tools/generate_sounds.py` +2（`hurt`/`death`）→ 10 样本；日志 "Audio ready:
  10 samples"。
- **F3**：overlay 加 "(Phase 16)"，第 9 行 `health %.1f / %.1f`。
- **P15 遗留修复**：`addQuad` 曾把 UV 留 (0,0)（新加的方块面不采样纹理）→ 拆
  `addQuadUv(b,x,y,w,h,uvMin,uvMax)`，`addQuad` = 全瓦片 UV 特例。
- 测试 474/474（15 新增，见项目状态）；构建 0 警告（`-Wall -Wextra -Wpedantic -Werror`）。
- E2E（驱动 `/tmp/opencode/p16_e2e.zsh`；config seed 123456789：spawn (0.5,54,0.5) 脚底、
  眼 55.62、pitch −0.25、yaw 0(−Z)；窗口 (644,444) 1280×720 中心 (1284,804)；**鼠标按钮
  必须 `xdotool mousedown 1`/`mouseup 1`**——`keydown 1` 是数字键"1"，曾致 fall 用例失败）：
  - base 60f 无输入：与 `base3.png` **逐字节一致**（门控规则，含 stepUp 修复+移除调试后）。
  - fall 600f：锁定；S 0.15s（脚底到 z~1.145）；瞄准 **pitch −1.37**（448px 下移；浅角
    −1.2 会削坑远壁）；破 4 格（grass 0.9s + dirt 3×1.5s）成 4 深坑于列 (0,0)；W 0.6s
    走进坑、坠落 4 格（1 伤害→hp 19.008842）、被坑远壁钳 (0.5,50.0,0.3) 精确。掉落物落坑底
    被分离推到玩家身后（背包满不拾取、背后不可见）。心 10 半[左红/右暗]、心 1 满。**×2 字节一致**。
  - step 360f（**所有瞄准从 spawn 出发，玩家走位前不动**）：Tab→slot1 泥土，pitch −0.50
    (100px)+RMB 放 B 泥土 (0,54,-3)（地面命中 z=−2.47）；Tab×8→slot0 石头（**9 格热键栏，
    从 slot1 需 8 次 Tab 才回 0**），pitch −0.21 (−116px)+RMB 放 C 石头 (0,55,-3)（B 顶面
    命中 z=−2.41）；pitch −0.85 (256px)+RMB 放 A 石头 (0,54,-1)（地面命中 z=−0.92）；
    pitch −0.55 (−120px)；W 1.5s：玩家**自动 step-up 上 A**（1 格）→跨越 A→A 远端 1 格
    落下（无伤害）→被 B+C 墙（2 高，不可 step）+Z 面 z=−2 钳 **(0.5,54.0,−1.7) 精确**。
    判据 (640,718)=B 泥土面棕 (80,55,40)（若 step-up 坏则玩家钳在 A 面 z=0、该像素=A 石头
    灰 |r−b|<25）。**×2 字节一致**。
  - **step 场景几何要点**：B 占 z∈[−3,−2)，其面向玩家(+Z)的面在 **z=−2**（不是 z=−3）→
    钳制 pos z=−2+0.3=**−1.7**；C 在 B 顶 (0,55,-3) 使墙 2 高挡住 step-up。热键栏覆盖屏幕
    行 ~686-716，**场景像素检查别用行 >686 的中心区**（(640,718) 在栏下沿，可用）。
- 坑位（本阶段新踩）：
  - **9 格热键栏 Tab 环绕算术**：从 slot k 到 slot 0 需 `(9−k) mod 9` 次 Tab；从 slot1 回 0
    是 **8** 次（不是 3）——放错块类型的 E2E 失败先查 Tab 计数。
  - **stepUp 循环轴 ≠ moveAxis 轴编号**：step 循环轴 0=x/1=z，moveAxis 0=x/1=y/2=z；
    换算只在 `moveAxis(axis==0?0:2)` 一处，`cz = axis==1 ? front : fz0`（曾误写 axis==2）。
  - **E2E 终态必须钳制/落定**：自由行走结束会抖 ±~0.001 格→亚像素场景位移→×2 字节不一致；
    用例设计成"最后被墙/面钳住"（fall=坑壁、step=墙）。
  - **放置块会改变可见场景光照/几何**：诊断"渲染不符预期"时先 dump 玩家终态 pos + 目标格
    block id + 邻格 sky light（本阶段用临时 P16DBG 日志实锤 step 场景状态正确后移除）。
