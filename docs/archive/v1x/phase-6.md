# Phase 6 细节存档

> Phase 30 文档整理：由 `SESSION_CONTEXT.md`「Phase 6 已完成（细节存档）」节 移入（未改写原文）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/...` 驱动脚本）指向当时状态；当前文档布局
> 见 `README.md`「文档地图」。

## 会话存档（原 SESSION_CONTEXT）

- `src/world/raycast.{hpp,cpp}`：`raycastBlock(world, origin, dir, maxDist)`
  （DDA，起点 cell 不测试，air 判定只走 World::getBlock）+
  `canPlaceAt(world, blocks, cell, pMin, pMax)`（Y∈[0,128) + 非 solid + 玩家 AABB
  严格不重叠）。几何层不读属性，交互规则在 Game 里（数据驱动）。
- 输入状态机（Game::handleInteraction）：未锁定左键=锁定指针；锁定时
  左键=破坏（destroyable 才生效，水/基岩数据里已 false）、右键=放置
  （canPlaceAt 校验）、Tab=切 hotbar、ESC=释放（未锁定 ESC 才退出，
  run() 退出检查用上一帧锁定状态）。
- hotbar：`assets/data/hotbar.json`（7 个方块名，未知名/空=启动报错），
  Game 成员 `m_hotbar`/`m_hotbarSelected`；reach=`kReach=5.0f`（game.cpp）。
- 21 个 RaycastTest：空世界/零向量/非正 reach/六面法线/停止于第一块/斜向/
  负坐标(√13 斜率避角落 tie)/reach 边界(恰好 reach 命中)/起点在方块内/
  起点恰在边界(t=0 命中)/平行轴/飞出世界/水几何命中+destroyable 数据/
  canPlaceAt（空/solid/水/Y 范围/重叠×3/邻接×3/跨边界玩家×3）。
- E2E（xdotool 脚本）：base/break/place 三张截图——破坏后地面出现洞
  （暗腔+内部泥土），Tab+右键后洞被 dirt 填充（大块棕色区域）；
  ESC 未锁定退出正常；无输入运行仍逐字节一致。
