# LGT-5 半块透光（v1.5.0，门 120）

v1.x 任务矩阵第五行（docs/BACKLOG.md）。出处 = v1.0 砍掉清单回填：
slab/stairs/wall/fence 现 opaque 挡光（LGT-2 派生 light_opacity 15），
Minecraft 半块透光 = 光穿过这些格不额外衰减。**内容变更**（7 块 JSON
各加 `light_opacity: 0`），**零核心代码**（LGT-2 的统一进入格成本机制
已就位：`enterCost = max(1, opacity)`，竖直天空经零 opacity 格免费 =
空气同款）。

## 定案
- 7 块：stone_slab / stone_stairs{,_east,_south,_west} / stone_wall /
  fence 加 `properties.light_opacity: 0`（= MC 半块规则：光如空气穿过）。
- **`opaque: true` 保持不变** = 剔面/网格渲染零变更（P85"slab 邻满块
  同层侧面剔面"局限原样保留 = 记档偏离：MC 的剔面规则与透光是两件
  事，本项目最小范围 = 只改光场）。
- 语义核对（world/lighting.cpp enterCost）：opacity 0 = 竖直天空免费 +
  其余每格成本 1 = **与空气完全同衰减**；玻璃 1 / 叶 2 / opaque 15
  对照不变。
- glass_pane 已 opaque false（派生 0）= 本项不涉。
- 零 format/config/音效/纹理；帧锁定（光场纯帧状态）。

## 测试（1714/1714 = 1711 + 3 LGT-5，-Werror 0 警告）
- `Lgt5HalfBlockLightTransmission`（test_block，真实资产内容钉死：7 块
  light_opacity 0 + opaque 保持 true + stone 仍派生 15）。
- `SkyVerticalThroughHalfBlock`（test_lighting：slab5 冠层 1/5 层竖直
  天空 = 15 恒【空气同款免费】vs stone 0 vs leaves 13 对照）。
- `BlockLightThroughHalfBlockLayer`（密封盒 + glowstone 15 源：slab5
  层上读 13 = 空气世界逐值同【零额外衰减】vs stone 层 11【竖直死只剩
  侧路 15−4】+ 层内 14 vs 0）。

## E2E 门 120
- **S1 PASS = 第 120 次结构性免疫免重锚**：fresh 60f 四方（NEW v1.5.0×2
  / OLD v1.4.0×2）RAW 387726 B 字节一致 + base18 锚（9c64b0f8）连续 +
  savenorm EXACT（半块永不生成 = 世界光场零变化）。
- **S2 半块透光判别场景 PASS**（密封 5×5×4 石室【地板 y38/墙 y39-41/
  顶 y42】+ 西墙一格 (20,40,0) = stone_slab + 墙外 (19,40,0) = glowstone
  15 = **室唯一开口**；玩家室中心 (22.5,39,0.5) 朝西 yaw π/2 pitch
  −0.15；mobs off + fire 9999s（燃尽足迹帧锁定位稳）；600f 无输入）：
  old slab 挡光（15−15 死 = 密封室恒暗）vs new 光穿 slab 格（14）经
  室内 BFS **灌亮全室**——×2 RAW/侧（old 61020 B / new 124725 B）+
  **A/B 双对**：old 帧均 (10.6,11.8,12.7) 暗 vs new (76.8,84.9,74.6)
  亮，diff 945782 px（93.3% = 室内可见面）**0 px 变暗 = 纯光增益** +
  **savenorm EXACT**（世界状态两侧全同 = 差异纯光场，光不入档）。
  首轮两坑：① 场景构建器**覆盖** base blocks 数组 = 孤儿 fires 条目
  破交叉校验 init 死（PITS 288，改合并 + 场景生效校验）；② 黑窗长周期
  25 min（new 侧 4+4 跑全黑，old 侧同时段亮 = PITS 286 批内混现加长
  型，尺寸门重试收）。
- **S3**：1714/1714。**S6**：无（零解析面——light_opacity 解析 LGT-2
  既有）。
- **性能 A/B（收）**：同起跑态 fall 900f 头对头（v1.4.0 vs v1.5.0，
  共用 gperf 终态 268 块 + 66425 流体 + 50 fire 起跑）：**fluidCells
  三窗 229457/218800/221654 双侧逐位一致** + PIPE3 工作负载逐位一致
  （visitedChunks 1810/1810、fluidCellsSeen 250395、changesApplied
  2853）+ fluid/fire/spread/stream/upd/render/swap 全同档（±0.7%）=
  成本可忽略（半块不在生成世界 = 结构惰性，light_opacity 只在持块格
  被读）

## 偏离/记档
- 剔面规则不动（opaque 保持 true）：MC 的"满块邻半块出面"未做（P85
  局限保留）= 最小范围定案。
- 半块透光经 LGT-2 机制 = 零核心代码（light_opacity 字段消费面 +1）。
- **坑位 PITS 288**：场景存档构建必须**合并** base 的 blocks 数组（非
  覆盖）——base 的 runtime 状态（P90 湖火）带 fires[] 条目，覆盖 = 孤儿
  fires 破"fires 条目须有 fire 块编辑"交叉校验 = init 静默死伪装场景
  未加载（PITS 251 同款家族）；驱动侧加场景生效校验（blocks 数 +
  player pos 断言）。
