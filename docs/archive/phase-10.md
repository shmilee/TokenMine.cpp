# Phase 10 细节存档

> Phase 30 文档整理：由 `SESSION_CONTEXT.md`「Phase 10 已完成（细节存档）」节 移入（未改写原文）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/...` 驱动脚本）指向当时状态；当前文档布局
> 见 `README.md`「文档地图」。

## 会话存档（原 SESSION_CONTEXT）

- `src/entity/{entity,item_entity,projectile,item_mesh}.{hpp,cpp}`：Entity 基类
  （约定/物理见核心架构节）；Player 并入（行为位级一致——310 个旧测试 + 基线字节一致
  证明）；`src/block/cube_face_table.hpp` 从 chunk_mesher 匿名命名空间抽出，mesher 与
  item mesh 共享（绕序回归测试同时守护两者）。
- Game：`m_entities`（vector<unique_ptr<Entity>>；player 仍单独 `m_player`——本地 actor+
  相机）；breakBlock 按 `properties.drops`（数据驱动）在方块中心生成 ItemEntity（零初速——
  静止位精确、截图可复现）；updateEntities(dt) = 逐实体 update → 合并 pass（索引序 j→i、
  gone[] 标记跳过已移除目标）→ shouldDespawn → 延迟移除；drawItemEntities（按方块所属
  pass 与 chunk 同 pass 绘制；model = translate(pos+(0, h/2+bob, 0)) × rotateY(spin)）；
  itemMesh(id) 懒缓存（unordered_map<BlockID, Mesh, BlockIDHash>）；动画取 m_frameCount
  （kItemBobPeriodFrames=90 / kItemSpinPeriodFrames=240 / kItemBobAmplitude=0.05，非墙钟）。
- 重构中发现并修复回归：Player 成员上移 Entity 后 ctor 初始化列表漏 m_spawnPos → 虚空坠落
  重生在 (0,0,0)；既有 PlayerTest.VoidFallRespawnsAtSpawn 捕获。
- 验证：
  - 328/328 测试（18 新增：EntityTest 5 下落/静止精确·无抖动·贴墙 flush·滑动·防穿透；
    ItemEntityTest 7 摩擦停驻·120s·kill plane·合并·异方块不合并·距离不合并·堆叠上限 64；
    ProjectileTest 3 抛物线落地·撞墙 flush·kill plane 无命中；ItemMeshTest 3 居中尺寸+
    烘焙光·绕序·UV 在 tile 内）。Projectile 30s 消亡无专测（与 120s 共用 age 路径，活箭
    必先撞 solid/kill plane——已文档化，非遗漏）。
  - 60 帧无输入基线与 P9 `base3.png` 逐字节一致（空实体列表 → 渲染不变，Player 重构
    位级一致；含移除调试日志后的重编）。
  - E2E（`/tmp/opencode/p10_e2e.zsh`，config 种子 123456789：spawn (0,0) surface y=53
    forest；时序 +0.5s mousemove 1284 804 / +0.7s 锁指针 click / +1.0s
    mousemove_relative 0 520（直下）/ +1.3s 破坏 click）：A 组（破坏）准星 = dirt
    (131,86,58) = 掉落物顶面（物品在中心射线上遮住洞底）；恰 1 次破坏 (0,53,0) 帧 77
    （临时调试日志确认，后移除）；玩家物理性掉进 1 格深洞（AABB 全在破坏格上方 → 无
    支撑，物理正确）；A120==A120b 逐字节一致（实时 xdotool 输入仍可复现——物品静止位
    精确整数，bob/spin 只依赖 frameCount，break click ±1-2 帧抖动无害）；A120 vs A240
    唯一 diff = 99×96px 簇（dirt vs dirt，max Δ24=纹理噪声）= 物品 bob（~17px）+ spin
    （帧 120 vs 240 相位差 π）；C 组（不破坏）C120==C240 零 diff（证明 A 组 diff 是
    物品而非昼夜——直下视角太阳在相机背后，不渲染）。
  - 移除调试日志重编后：60 帧基线字节一致 + A 组 2 次运行字节一致 + 与移除前截图
    字节一致（日志无副作用）。
- seed 123456789 出生列事实（/tmp/opencode/probe_p10.cpp）：(0,53,0)=grass（drops=dirt）、
  下 dirt 50-52、stone ≤49、无洞穴。
- 俯视几何事实：pitch -1.55（离垂直仅 1.5°）时中心列 = [远洞壁(暗)][~300px 亮洞底/
  物品][近洞壁(暗)][草]——截图"大块泥土"是几何投影，非渲染 bug（像素剖面验证）。
