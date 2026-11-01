# Phase 5 细节存档

> Phase 30 文档整理：由 `SESSION_CONTEXT.md`「Phase 5 已完成（细节存档）」节 移入（未改写原文）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/...` 驱动脚本）指向当时状态；当前文档布局
> 见 `README.md`「文档地图」。

## 会话存档（原 SESSION_CONTEXT）

- `src/player/player.{hpp,cpp}`：宽 0.6 / 高 1.8 / 眼高 1.62；walk 4.3 / sprint 5.6 /
  起跳 8.5 / 重力 32 / 终端速度 38；杀平面 y < -32 → 回出生点。
- `Camera` 改纯视点（移除 update/moveSpeed/sprintMultiplier/mouseSensitivity/kMaxPitch——
  自由飞行是 demo 脚手架，被 Player 取代；5 个 camera 自由飞行测试迁入 test_player，
  属设计替换非删功能）。
- 15 个 PlayerTest：贴地静止(精确 y==1.0) / 重力下落 / 走下悬崖 onGround=false /
  墙阻挡(精确贴面) / 沿墙滑动 / 跳跃峰值(2.0<peak<2.25) / 跳上 1 格爬不上 2 格 /
  天花板顶头(maxY≤1.2) / 水可穿过 / 移动方向随 yaw / 冲刺更快 / 鼠标转向+pitch clamp /
  虚空重生 / 子步防穿透(0.25s 大步不穿 1 格地板) / hasRoomAt。
- 验证基线不变：seed 123456789 → spawn (0,0) surface y=53 forest；
  同 seed 截图逐字节一致；新视角=眼高（天空占比 17% vs 旧高空相机 32%，证实相机在玩家眼位）。
