# Phase 83 细节存档

> 范围（用户"接下来的三个 phase, 按成本排序进行" = 第 3 个最高成本，
> 代理定案为最小完整版：**信标 = 吸收第二来源**——数据驱动 `beacon`
> 块 + `beacon_effect` 指向吸收效果 + `beacon_base` 金字塔基座 + 每帧
> pass 施加 + 半透明光束；零 format）。

## 阶段报告

- **`[2026-10-15, Phase 83]`** 信标 — 1132/1132（+14：beaconLevel 7 +
  属性解析 4 + 内容钉死 3），`-Werror` 恒 0 警告；E2E 批跑全绿（无输入
  跑；S1 第 56 次结构性免疫 + S2 活跃信标【池 4】+ S2b 无基座对照【池
  0】+ S3 光束【6464 px】+ S6 init 失败 ×2）。

### 实现内容

**数据驱动块属性（2 新，`src/block/block.hpp` + `block_registry.cpp`）**：
- `beacon_base`（严格 bool，缺省 false）——基座材料标记（shipped 只
  `iron_block` 带；核心读属性、不 switch 块 id = 硬规则 3）。
- `beacon_effect`（严格字符串，缺省空 = 非信标，显式空串拒绝【ammo 纪律】）
  ——信标指向的效果名（shipped = `absorption`；init 交叉校验效果存在
  【grows_to 纪律】）。

**内容（2 新块 + 1 配方 + 2 纹理 + 2 atlas 空位）**：
- `beacon.json`：solid cube、hardness 8（MC）、自掉、`beacon_effect:
  "absorption"`、青色玻璃纹理（salt 929-932）、atlas [9,1]。
- `iron_block.json`：solid cube、hardness 5（MC）、自掉、**`beacon_base:
  true`**、金属银纹理（salt 927/928）、atlas [8,1]。
- `iron_block` 配方：shaped **3×3 iron_ingot → 1**（MC 同款）。
- atlas [8,1]/[9,1]（row 1 空位，无 UV 移位 = **无重锚**）；既有纹理确定性
  再生零变化。

**纯函数 `world/beacon.{hpp,cpp}`**：
- `beaconLevel(world, blocks, x, y, z)` = 信标正下方连续完整 `beacon_base`
  层数（3×3/5×5/7×7/9×9 = 等级 1..4，第一层缺角即停）。纯函数，无状态。

**游戏接线（`Game`，生长 P48 同款范式）**：
- `m_beacons`（`std::map<BlockPos, BlockID>`）：setBlock 收口维护 + 读档
  `rescanBeacons` 重建。**无 per-instance 状态**（等级每帧重算）→ **零
  format**。
- `updateBeacons()`（每帧，updateEffects 之前）：遍历 m_beacons，等级 ≥1
  且玩家脚到信标格 **Chebyshev ≤ `beacon.range_blocks`**（默认 50）→
  `m_playerEffects.apply(effect, 0.5s)` 刷新（火源范式）。**捕获施加前后
  `totalAbsorption`、变化即 `syncAbsorption`**（PITS——信标 apply 在
  updateEffects 前，其帧内变化检测看不到）。
- **光束**：`drawBeaconBeams()`（透明 pass）——`m_beaconActive`（updateBeacons
  填）每信标一条半透明薄柱（`basic.frag` 加 `u_alpha` 默认 1.0【纹理路径
  用 texel.a 不受影响】、薄柱 mesh 8 顶点 + 36 索引关裁剪绘制、顶点
  blockLight 1.0 常亮 + 法线全 +Y = 面 shade 恒 1.0 均匀、柱高 64 + 等级
  ×32）。`beacon.beam` 可关（E2E 对照）。
- `Mat4::scale(sx, sy, sz)` 非均匀重载（光束 Y 向拉伸）。
- config 2 键：`beacon.range_blocks`（默认 50，>0 硬校验）+ `beacon.beam`
  （bool，默认 true）。
- init 交叉校验：beacon_effect 指向的效果存在。

**测试（1118 → 1132，+14）**：
- `test_beacon.cpp`（7）：beaconLevel 纯函数（空 0 / 完整 1-4 / 缺角停 /
  下层缺 = 上层废 / 非基座不计数 / 无基座注册表 fast-exit / 确定性重复）。
- `test_block.cpp`（+7）：beacon_base 严格 bool / 存储、beacon_effect
  字符串 / 显式空串拒绝、`Phase83BeaconBlockContent`（hardness 8 / 自掉 /
  beacon_effect absorption）、`Phase83IronBlockContent`（hardness 5 / 自掉 /
  beacon_base true）、`Phase83IronBlockRecipeContent`（shaped 3×3 iron_ingot
  → 1）。

### E2E（全绿；批跑脚本 `/tmp/opencode/p83_e2e.zsh`，**无输入跑**）

- **S1 第 56 次结构性免疫**：信标/铁块永不生成 + 默认背包无 + atlas 空位
  [8,1]/[9,1] 无 UV 移位 + fresh 无信标 = m_beacons 空 fast-exit + 无光
  束 = base18 字节一致。
- **S2 活跃信标**（300f no-input ×2 RAW 截图 + ×2 savenorm + EXACT）：信标
  (22,47,−5) + 3×3 铁块基座 (y46)，玩家 (22.5,45,0.5) 距 5.5 ≤ 50 → 吸收
  **池 = 4**（beacon 刷新 absorption 效果，absorb_hp 4）+ effects 含
  absorption。×2 RAW 截图字节一致（世界 + 光束静态）。
- **S2b 无基座对照**（同信标无铁块 = 等级 0 → **池 0 + effects []**，互斥）。
- **S3 光束**（no-input）：活跃场景 `beacon.beam` on vs off = **6464 px**
  差（光柱隔离，HUD 心两场景相同）+ beam-off ×2 RAW 定。
- **S6 init 失败 ×2**：beacon_effect "absorptionz" → "names unknown
  effect" 点名 / config range 0 → "must be > 0" 点名。
- **PITS +1**：信标吸收池须在 updateBeacons 内同步（帧内变化检测看不到
  pass 外的 apply）。

### 变更文件

- `src/block/block.hpp`（+2 属性）、`src/block/block_registry.cpp`（解析 +
  空串拒绝）、`src/world/beacon.{hpp,cpp}`（新，纯函数）、
  `src/core/game.hpp`（m_beacons/m_beaconActive/config/方法）、
  `src/core/game.cpp`（收口 + rescan + updateBeacons + drawBeaconBeams +
  光束 mesh + config + init 校验）、`src/core/math/mat4.hpp`（非均匀 scale
  重载）、`assets/shaders/basic.frag`（u_alpha）
- `assets/data/blocks/{beacon,iron_block}.json`（新）、
  `assets/data/recipes/iron_block.json`（新）、
  `assets/textures/blocks/{beacon,iron_block}.png`（新，工具链产物）、
  `assets/data/textures/block_atlas.json`（+2 空位）、
  `tools/generate_block_textures.py`
- `tests/test_beacon.cpp`（新）、`tests/test_block.cpp`（+7）、
  `CMakeLists.txt`（+src/world/beacon.cpp、+tests/test_beacon.cpp）
- `docs/ROADMAP.md`、`docs/KNOWN_ISSUES.md`、`docs/MECHANICS.md`、
  `docs/GAMEPLAY.md`、`docs/PITS.md`（+1）、`AGENTS.md`、
  `docs/archive/phase-83.md`

### 已知问题（本阶段）

- 新节「信标（Phase 83）」4 条：**效果仅吸收**（其它 MC 信标效果无）/
  **基座仅 iron_block**（gold/diamond/netherite 块无 = 项目无这些金属块）/
  **光束不入光照**（不照亮周围）/ **范围 = Chebyshev 立方**（MC 球状，
  项目简化）。
- "方块无 per-instance 状态"条加信标例外说明（功率等级每帧重算、无独立
  存档字段）。

### 下一步

- 三个 phase（P81 光滑砂岩 / P82 大师弓 / P83 信标）按成本排序完成。
- 候选后续：信标更多效果（速度/力量，同范式加 effect 数据即可）/ 楼梯·台
  阶模型（中成本）/ 熔岩湖 worldgen（用户明示"留到很久的未来"）。
