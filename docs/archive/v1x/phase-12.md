# Phase 12 细节存档

> Phase 30 文档整理：由 `SESSION_CONTEXT.md`「Phase 12 已完成（细节存档）」节 移入（未改写原文）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/...` 驱动脚本）指向当时状态；当前文档布局
> 见 `README.md`「文档地图」。

## 会话存档（原 SESSION_CONTEXT）

- `src/item/recipe.{hpp,cpp}`（新）：`Recipe`（id/output/outputCount/ingredients 有序
  (ItemID,int) 向量）+ `RecipeRegistry`（`registerRecipe(def, items)` 经 ItemRegistry
  交叉校验；`loadDirectory(assets, items, "data/recipes")` 排序加载；`all()`/`find()`；
  vector 存储，init 后不可变）+ 自由函数 `canCraft`/`craft`（**副本模拟**：
  `simulateCraft(Inventory by-value)` 按配方序/槽序升序消耗 + `add` 产出，ok 才提交——
  全有或全无、失败逐位不变、产物是原料之一也正确）。
- `src/item/item.hpp`：`ItemType::Material`（显式材料，无 block/无 break_speed；
  现有放置/渲染/破坏路径都按 type 分发，零改动）。`item_registry`：显式物品接受
  `"tool"|"material"`（材料带 `tool` 字段=报错；`parseTool` 拆出 `parseMaxStack`）。
- `src/block/block.{hpp,cpp}`：`properties.dropsCount`（1..64 默认 1；无 drops 却
  drops_count>1=报错）。`game.cpp` init：drops 校验扩展为 dropsCount ≤ 掉落物
  maxStack（交叉）；其后加载 RecipeRegistry（严格）。`breakBlock` 传 dropsCount。
- `src/entity/item_entity.{hpp,cpp}`：ctor 加 `count` 参数（Q 丢弃=1，破坏=dropsCount；
  NSDMI `m_count=1` 移除）。
- 数据：6 材料 `assets/data/items/{planks,stick,iron_ingot,coal,gold_ingot,diamond}.json`
  （`{"id":...,"type":"material"}`，max_stack 默认 64）；4 配方
  `assets/data/recipes/{planks(1 wood→4),stick(2 planks→4),wooden_pickaxe(2 wood+2 stick→1),
  iron_pickaxe(3 iron_ingot+2 stick→1)}.json`；4 矿石 JSON 的 drops 改材料
  （coal_ore→coal 等；**wood 仍掉 wood 本身**——木板配方原料，改掉则链条断）。
  物品总数 22 = 14 block + 2 工具 + 6 材料。
- 测试 380/380（29 新增，见项目状态）。探针 `/tmp/opencode/probe_p12.cpp` 验证真实
  数据目录：items=22/recipes=4/矿石掉落正确。
- 回归：60 帧无输入基线与 `base3.png` **逐字节一致**；P11-A E2E 配方与 `p11_a.png`
  **逐字节一致**（交互链未动，准星 (83,62,40) 复现）。
- 刻意简化（KNOWN_ISSUES「合成（Phase 12）」节）：**无游戏内合成 UI/触发**（系统完整
  + 单测覆盖 + 启动严格校验，UI 在 Phase 15——同 P10 Projectile 无发射器模式）；
  配方只有 shapeless（无网格位置/催化剂/消歧）；coal/gold_ingot/diamond 暂无用途；
  矿石掉材料=矿方块物品不能再从采矿获得（行为变化，刻意）。
