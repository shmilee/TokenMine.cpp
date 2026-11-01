# Phase 3 细节存档

> Phase 30 文档整理：由 `KNOWN_ISSUES.md`「已修复（归档）」条目 移入（未改写原文）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/...` 驱动脚本）指向当时状态；当前文档布局
> 见 `README.md`「文档地图」。

## 阶段报告（原 KNOWN_ISSUES「已修复（归档）」）

- **`[2026-10-03, Phase 3]` face 表 +Y/-Y 面绕序反了** — 顶面从上方看是顺时针（被 GL 背面剔除，永远看不见），底面从上方可见。Phase 1/2 的截图验证只采样了侧面纹理，掩盖了该 bug；Phase 3 排查渲染异常时发现并修正，新增绕序回归测试（`FaceWindingIsCounterClockwiseFromOutside`）防复发。
