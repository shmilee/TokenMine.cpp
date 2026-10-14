# Phase 73 细节存档

> 范围（用户"好的,那就按难易程度，从建议1开始下一阶段" = P72 定案
> 建议 1，代理定案为最小完整版：**火 v3 = Mob 火接触点燃**，火块 =
> P58 熔岩点燃的块版，全量消费 P70 遗留"Mob 站火里不受伤害"）：
> `MobEntity::update` 点火条件 `inLava || inFire` + 顺带修复 P27 潜伏
> UB（skeleton 射击回调在实体 range-for 内 push_back = 重分配悬空引
> 用，本阶段火把膜对照场景首次触发 SIGSEGV）。

## 阶段报告

- **`[2026-10-14, Phase 73]`** 火 v3：Mob 火接触点燃 — 1102/1102
  （+5），`-Werror` 恒 0 警告；E2E 批跑全绿（S1 第 46 次结构性免疫
  【无重锚】+ S2 火膜烧死【×2 savenorm 字节一致 + EXACT】+ S2ctl
  火把膜对照【×2 savenorm 字节一致 + EXACT】；无 S6 = 零 parse 面）。

### 实现内容

**核心（火块 = 熔岩点燃的块版，零新 config/内容/format）**：
- `src/entity/mob_entity.{hpp,cpp}`：
  - 构造时 **`m_fireBlockId(blocks.idForName("fire"))`**（无 fire 块
    的资产集 = `BlockID::invalid` = `overlapsBlock` 恒 false = 结构
    惰性——P70 玩家 `m_fireBlockId` 同款容错）。
  - `update()` 点火条件 `inLava` → **`inLava || inFire`**（`inFire` =
    `Entity::overlapsBlock(m_fireBlockId)`，AABB 严格不等式——fire
    块 cross 非 solid，mob 从下面走过即重叠）。其余全部复用 P58 既
    有路径：同一 fire 效果同一节奏（1 HP/0.2 s = 5 HP/s、可致死）、
    火源内每帧重点燃（duration 重置 + 相位保留 = 持续燃烧）、离开
    火源残留 8 s（`fire_duration_seconds`）、水瞬时熄灭且先于 tick
    （P57 水胜规则，水同时灭熔岩与火块点燃）。
- `src/core/game.cpp`（**P27 潜伏 UB 修复**，本阶段 E2E 暴露）：
  - 实体更新循环 range-for → **索引循环**（`for (i < m_entities.size())
    { auto& e = m_entities[i]; }`）。原因：skeleton 射击回调（在
    `mob->update()` 内被调）里 `m_entities.push_back(arrow)`——vector
    重分配使 range-for 的 `entity` 引用悬空 → SIGSEGV（rc=139）。
    潜伏 P27 至今的原因：P58 水膜对照里水挡视线（`hasLineOfSight`
    拒发），骨架从未射出成功的箭；P73 火把膜（光 14 缺口环 → 敌对
    生成 + 无遮挡视线）让回调首次真正 push，首帧即崩。索引循环每轮
    从当前存储重取引用，延迟的箭同帧被走到（取出生帧步 = 原意图
    语义）；循环内无 erase 路径（移除都在循环后的 despawn pass），
    索引推进安全。PITS 已收录。

**测试（1097 → 1102，+5 `MobFireContactTest`）**：
- 火块点燃速率（zombie 20HP：3×3 火块补丁上 2 s = 10 tick → 10）；
- 离火残留（tank 100HP：1 s 在火 = 5 tick → 95，清火后 countdown，
  到期帧不 tick——与熔岩残留测试同款相位算术）；
- 水瞬时熄灭（烧 1 s 后清火 + 灌满水：1 步灭、10 s 零伤害）;
- 未武装 mob 不燃（无 armFire = 火块上 10 s 零伤害，P57 结构条款）；
- 无 fire 块的注册表惰性（plain fixture：id invalid = 武装了也不
  燃——构造容错路径钉死）。
- 夹具：`makeFireBlocks`（stone + **`solid: false`** 的 fire——首轮
  测试用实心 fire 块，mob 生成在火格内被碰撞顶到补丁上面（y 2.0），
  AABB 不再重叠 y=1 = 永不点燃；真实 fire.json 非 solid，测试夹具
  须同款）。

### E2E（全绿；批跑脚本 `/tmp/opencode/p73_e2e.zsh`）

- **S1 第 46 次结构性免疫**：fresh 60f ×2 字节一致 + base18 字节一致
  （fire 块永不生成 → fresh 零 fire 格 → `inFire` 恒 false 路径惰
  性；无 config/内容变更）。
- **S2 火膜烧死**（no-input 2400f ×2 savenorm + EXACT）：场景 = seed 9、
  夜 0.75、mobs on、`fire.spread_chance 0`（膜静态）、**3×3 fire 膜
  (21..23, 46, -6..-4)**（地面顶 y=45 上方一格：mob 1.8 高 AABB 重叠
  即点燃；玩家 (22.5,45,0.5) 距膜 3.5+ 格永不重叠）+ **fires 9 条目
  remaining 1000 s**（场景存档注入 = 膜贯穿全 run：2400f ≤ ~160 s
  @15fps）→ 终态：**5 mob 烧死**（2 牛/1 鸡/2 羊 = 12 掉落 9 实体，
  mutton 对堆叠）+ 膜 9 格完整（无蔓延/无浇灭/无燃尽）+ 玩家 20 血
  无效果（光 15 挡敌对生成环 = P58 同款）。**mob 机制全帧确定**（固
  定 1/60 步、帧种子生成、整数帧燃烧 tick）→ ×2 savenorm 字节一致
  （剥 real-dt：hunger/saturation/effects remaining/entity age/fires
  remaining）。
- **S2ctl 火把膜对照**（no-input 2400f ×2 savenorm + EXACT）：同几何
  **torch 膜**（光 14、cross、非 flamable、**非 fire 块**）→ **entities
  [] = 无 mob 被烧死**（火把不点燃——判别证明：烧死来自 fire 块而非
  场景）。副产物（接受，P58 水膜对照同款设计）：torch 光 14 的敌对
  生成门比 fire 光 15 少一圈（环上光 6 缺口）→ 敌对生成并杀无输入
  玩家（health 0）——对照断言的是"无燃烧死亡"，玩家状态 = 门差证明。
- **无 S6**：零新 parse 面（场景存档 = P72 format-12 解析器既有文件）。
- **崩溃定位**：S2ctl 首轮 3 次尝试 rc=139——日志最后一行 = skeleton
  shot 回调（P27 潜伏 UB 首触发）；修索引循环后同场景 2400f 通过。

### 变更文件

- `src/entity/mob_entity.hpp`、`src/entity/mob_entity.cpp`
- `src/core/game.cpp`（实体更新循环索引化）
- `tests/test_mob.cpp`（+5 `MobFireContactTest` + 夹具）
- `docs/ROADMAP.md`、`docs/KNOWN_ISSUES.md`、`docs/MECHANICS.md`、
  `docs/GAMEPLAY.md`、`docs/PITS.md`（+1）、`AGENTS.md`、
  `docs/archive/phase-73.md`

### 已知问题（本阶段）

- P70 遗留"Mob 站火里不受伤害"**已解决**（KNOWN_ISSUES 火 v2 条划
  删 + `[Phase 73]`；伤害来源条同步）。
- 新文档化偏离（记入火 v2 条）：**玩家火接触仍无残留点燃**（P70 直接
  伤害、离开即停），mob 火接触 = 点燃（残留 8 s）——两实现不对称
  （MC 两者都点燃；玩家侧升级 = 后续候选）。

### 下一步建议

1. **玩家火块点燃**（本阶段暴露的不对称：玩家站火块 = P70 直接伤害
   无残留 → 改走 fire 效果点燃 = MC 语义，机制件全现成：玩家侧
   `inFire` 已有，点火路径 = P57 熔岩点燃同款；format 无变更【效果
   已入档】）。
2. **campfire v2**（4 槽 / 燃料 / 水浇灭，P71 遗留偏离条）。
3. **弓 v3 / 箭强化**（P42 遗留"无箭矢强化"）。
4. 信标（吸收第二来源，需方块实体系统 = 高成本，可拆 v1）。
5. 熔岩湖 worldgen（用户明示"留到很久的未来"，优先级最低）。
