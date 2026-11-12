# Phase v3.42.0 — V3-18 存档扩展名（门 232）

> 3.x「世界扩展」SAVE 行项（11-08 用户诉求——「.json 扩展名体现不出
> 压缩后的 json，与没压缩的混淆」）。**零 format**（容器不变 = V3-7
> 确定性 gzip）/ **零像素**（纯 IO）/ 零 config。

## 实现

### 写/读路径（save_slot 路径族 = 单源）
- **`slotWritePath(dir, name)`** = `<name>.json.gz`（新写形——gzip
  容器的可见扩展名）。
- **`slotReadPath(dir, name)`** = 两级探测：`.json.gz` 先（现写形）、
  legacy `.json` 后（V3-7 时代的文件名——其内容本就是 gzip 容器）→
  **零迁移**（legacy 槽可读，下次写自动换名 = 温和迁移）。
- `Game::saveWritePath/saveReadPath`（game.hpp）委托 save_slot 路径族
  ——写（writeSave/autosave/exit 共享）恒新形；读（tryLoadSave + demo
  save-spec 存在性检查）走探测。

### 管理面（SAVE-3，双扩展名）
- `listSaveSlots`：两扩展名都枚举，双形槽去重一次（.json.gz 赢摘要
  探测）。
- `readSaveSlotSummary`：探测读；缺失槽 = "no such save slot"（两级
  皆无 = 更早的点名）。
- `copySaveSlot`：源探测读、**目标写新形**（复制即迁移 legacy 槽名）。
- `renameSaveSlot`：槽整体搬移（双形皆存在 = 消费全槽，孪生删除）→
  新形。
- `deleteSaveSlot`：消费全槽（双形）。

### demo scene 资产（同 gzip 容器，随改）
- `assets/demos/scenes/*.json` 11 个 committed 资产 `git mv` →
  `.json.gz`（gate231_death 门资产同步）。
- `demoScenePath(name)` = 新形；`demoSceneReadPath(name, dir =
  "assets/demos/scenes")` = 两级探测（dir 参数 = 测试 scratch 面）。
- init 交叉检（loadDemoIndexChecked）走探测【门内捕获：初版沿用
  `scenesDir / (name + ".json")` 硬编码 = 重命名后全索引假拒——
  测试的 scratch 目录面暴露，PITS 424①】。
- `mk_scene.py` 输出 `.json.gz`。

### 文案/文档
- `--help` --save 行（.json.gz + legacy 零迁移注记）。
- CONTENT_GUIDE_CONFIG / ARCHITECTURE（目录树 + 存档节）/ README 存档
  槽行。

## 测试（+6 = 3005）
- SaveSlotTest 5 新：WritePathIsGz / ReadPathTwoStageFind /
  ListEnumeratesBothExtensionsDeduped / CopyAndRenameMigrateToTheNew
  Form（含双形槽整体消费）/ DeleteConsumesBothForms；1 既有断言更新
  （缺失槽点名 = 探测先行 "no such save slot"）。
- V316aDemo 1 新：ScenePathTwoStageFind（scratch dir 参数面）；
  1 既有断言更新（demoScenePath 后缀 .json.gz）。

## 门结论（门 232 全 PASS）

**3005/3005**（+6：SaveSlotTest 5 + V316aDemo ScenePathTwoStageFind +
2 既有断言 re-pin）。

**结构免疫第 113 次**【S1 ≡ base19 逐字节 ×2 + savenorm EXACT——纯
IO 零像素；**门内捕获 PITS 424④**：WM 平铺漂移 679×744→1280×720 破
锚 = S1 门 config 直接请求锚几何 679×744 钉死（config_s1.json 换
版），S1 ≡ base19 恢复】+ canary 4×2 新≡旧二进制（tm_v341 v3.41.0）
RAW+savenorm EXACT【存档写/读路径变更对无存档 demo 冷路径零影响】。

**迁移臂**【新写 .json.gz（5336 B payload → 764 B on disk）/ legacy
.json 名可读（零迁移）/ 干净退出写回自动换名（双形并存，list 去重
.json.gz 赢）/ copy = 新形写（复制即迁移）/ delete = 消费全槽 /
--list-saves 双扩展名枚举】+ **scene 臂**【guide_mining 11387f 自
pilot_mine.json.gz 回放 "Loaded demo scene ... .json.gz" + --list-
demos 交叉检 OK（10 scene 全 .json.gz）】。

**性能 A/B 同档**【旧 = tm_v341 v3.41.0 + g139 双树 27 markers：OW
900f 无输入 PERF-WIN 三轮 new 0.630/0.560/0.473 / 0.663/0.759/0.829 /
0.667/0.723/0.614 vs old 0.413/0.513/0.559 / 0.658/0.698/0.709 /
0.665/0.726/0.755——轮间 medial 交错（Mesa 软 GL 共享机噪声带），
clean 轮（c）win1/win2 ±0.003 恒等 + win3 new ≤ old + changesApplied
9/3/1 逐位 + 终档跨树 savenorm EXACT = 纯 IO 结构性零成本】。

**门内捕获 PITS 424 四坑**【①路径族 = 参数化探测单源（scene 交叉检
硬编码 .json 后缀 = 全索引假拒 + 测试 scratch 面恒假拒）②双形槽消费
语义（copy 新形写 / rename+delete 全槽删孪生）③list 去重 .json.gz
赢 ④WM 平铺漂移 = S1 几何破锚（锚几何门内钉死）】。
