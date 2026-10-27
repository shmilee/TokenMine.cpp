# GEN-7 基岩多层（v1.6.0，门 121）

v1.x 任务矩阵第六行（docs/BACKLOG.md，GEN 行 1/3；原 v1.0 砍掉清单
"基岩多层"重编号）。出处 = KNOWN_ISSUES「基岩只有单层」[Phase 4]：y=0
单层底（`terrain.bottom_block`），y≥1 石理论上可挖穿世界。

## 定案
- **`terrain.bedrock_layers`** 新字段（int [1,8]，缺省 1 = 旧行为位级
  复现；非整数/越界点名拒）；shipped worldgen.json = **3**（MC 经典
  基岩底：y0 恒 / y1 50% / y2 恒）。
- **奇偶规则（MC 同款）**：`wy < bedrockLayers` 的偶数层（y0,2,…）恒
  底块；奇数层（y1,3,…）每格独立确定性 50% roll（`bedrockLayerRoll`
  纯函数：hash3(wx, wz, wy, seed ^ kSaltBedrock) 顶位，P45 专用 salt
  纪律）→ 底块，否则原深石/地表分类不变。
- `fillTerrain` 单分支（`wy < h` 内）；layers=1 时分支恒假 = 位级复现
  旧填充（默认参数向后兼容）。
- **零 format**（生成世界演进 = 旧存档新底先例 P86/88/93/GEN-4/5）；
  零 config 键（worldgen.json 数据面）。
- 矿脉计数契约演进重钉（GEN-4 先例：y1/y2 变基岩 = 矿可放置深石集
  缩小 → Gen3ShippedOreVeinCellCountsPinned 4 值重钉 + 软带保留）。

## 实现
5 文件（worldgen_config.{hpp,cpp} + worldgen.json + world_generator.
{hpp,cpp}）+ 8 新单测 + 1 契约重钉。

## 测试（1722/1722 = 1714 + 8，-Werror 0 警告）
- config：默认 1 / shipped 3 / 越界 0·9·-1 点名 / 非整数 2.5 点名 /
  非数字 "three" 点名（5 例，含 EmptyObjectYieldsDefaults 演进）。
- 生成：BedrockFloorMatchesPurePredicate（y0/y2 恒底块 + y1 = 纯谓词
  钉死，1024 格 roll 计数 37.5%-62.5% 带）/ BedrockFloorDefaultIsLegacy
  （layers=1 → y1/y2 全深石 = 旧填充）/ BedrockLayerRollProperties
  （跨生成器确定性 + 双 50% 带 + 层间去相关）。
- 契约演进：Gen3ShippedOreVeinCellCountsPinned 重钉（coal 3448→3255 /
  iron 1684→1616 / diamond 103→96 / gravel 1087→1003，软带全过）。

## E2E 门 121
- **S1 PASS = 第 121 次结构性免疫免重锚**：fresh 60f 四方（NEW v1.6.0×2 /
  OLD v1.5.0×2）RAW 387726 B 字节一致 + base18 锚（9c64b0f8）连续 +
  savenorm EXACT（基岩底 y0-2 在出生点 y38+ 视图外 = 世界生成演进，
  GEN-1/GEN-4 先例）。
- **S2 竖井判别场景 PASS**（3×3 全深井 x9-11,z9-11 air y1..127 + 井西缘
  悬台 (9,6,10) + 玩家井内 (9.5,7,10.5) yaw −90° pitch −1.2 俯看井壁
  下部；mobs off；600f 无输入）：井壁 y1/y2 **侧面带** = 判别区（井底
  可见面 = y0 顶面，两版同床岩 = 非判别面）——old 带 = 深石侧面
  （124×0.8 ≈ 96 亮）vs new = 床岩格侧面（52×0.8 = 42 暗，y2 恒 + y1
  roll）：×2 RAW/侧（old 269337 B / new 270932 B）+ **A/B diff 13172
  px，93.2% 集中横带 rows 320-399**（壁带）+ **99.0% px new 严格更暗**
  （带均 old 95.9 石亮 / new 42.1 床岩暗 = 解析值吻合）+ savenorm
  EXACT（会话世界状态全同，生成差异不入档）。
- **S3**：1722/1722。**S6**：无（解析面 = config 4 点名已单测）。
- **性能 A/B（收）**：同起跑态 fall 900f 头对头（v1.5.0 vs v1.6.0，
  共用 gperf 终态 268 块 + 66425 流体 + 50 fire 起跑）：**fluidCells
  三窗 229457/218800/221654 双侧逐位一致** + PIPE3 工作负载逐位一致
  （visitedChunks 1810 / fluidCellsSeen 250395 / changesApplied 2853）
  + fluid/fire/spread/stream/upd/render/swap 全同档（±1.6%）= 成本可
  忽略（roll = 每 y1 格 1 hash3，仅生成期读）

## 偏离/记档
- 井底可见面 = y0 顶面（两版同床岩）——判别走井壁 y1/y2 侧面带
  （首轮误判"井底"为判别区的排查记档：场景内建后先跑 A/B 看 diff
  实际落位再写断言，勿按几何直觉先写断言）。
- bedrock_layers=8 = 世界底部全床岩帽（内容可调，无硬上限语义）。
- GEN-3 矿计数 4 值重钉 = 契约演进（GEN-4 同款先例，非回归）。

## 坑位（PITS 追加）
- **PITS 289**：init 失败后旧产物残留 = 假 PASS——门 121 首轮探针 v2
  场景自身重复编辑（悬台格在井 air 循环内 = air+stone 双条）→ init
  "duplicate block edits" 死，但验证读的是 cp 进去的场景文件（含预期
  pos）+ 上轮残留截图 = 全绿假象。纪律 = ① 日志 grep ERROR ② clean
  exit ③ 游戏退出后写的真终档（先 rm 槽）④ 截图 mtime > 跑前时间戳。
- 场景构建器合并 air 编辑时**必须排除悬台/实体格**（v2 构建器 bug =
  本坑触发器；v3 修正 = LEDGE 跳过 air 循环 + 无重复断言）。
