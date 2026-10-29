# SAVE-3 槽位管理（v1.x 矩阵 27/33，v1.27.0，门 142，SAVE 行 2/2 = SAVE 系统收官）

## 定案

原始定义已佚 = 定案 **CLI 槽位管理（headless 四操作）**（消费 KNOWN_ISSUES
存档节「多个槽只是多个文件，手动切换」+ SAVE-1 残余「无多槽轮转」条）：

- **`--list-saves`**：列出 data/saves 全部槽（名序确定性；摘要 = 纯文件
  内容【format/seed/昼时/玩家位置/血量】——**无 mtime、无墙钟** = 重复
  运行逐字节同输出；损坏文件**浮现 UNREADABLE + 点名**而非隐藏）
- **`--copy-save S D`**：字节保真复制（分支存档 = MC "Save Copy"；D 已
  存在点名拒；写 = SAVE-1 `writeAtomicFile` 同款原子 = 失败复制不留半写）
- **`--rename-save S D`**：原子改名（POSIX rename；D 已存在点名拒）
- **`--delete-save S`**：删除（点名）
- **槽名 = `--save` 字符集**（字母/数字/_/-，非空）——字符集天然杀路径
  分隔/穿越，槽名恒为 data/saves 内平凡文件名
- **独占纪律**：管理旗标每跑至多一个、与游戏旗标（--save/--new/--frames/
  --screenshot/--time/--seed/--save-full-diff）互斥 = rc 2 点名（管理跑
  不启动游戏：无窗口、无世界、无资产加载）
- **零 format（22）/零 config/零渲染/零运行期路径**：新代码只在管理旗标
  存在时于 Game 构造前执行 = 普通跑结构惰性（S1 免疫论证根）
- **记档偏离**：无游戏内选档面板/无会话内换档（游戏无标题画面，会话内
  载档 = 世界重建级架构变更，矩阵外）；「保存并退出」= 既有 ESC/退出
  存档路径（本已存在）；多槽切换 = CLI `--save <slot>`（项目既有切换面）

## 实现

- `src/core/save_slot.{hpp,cpp}` 新模块：`isValidSlotName` / `listSaveSlots`
  （名序、.json 过滤、目录缺失 = 空表）/ `readSaveSlotSummary`（内容纯
  函数、缺文件/坏 JSON/缺 format = readable=false + 点名）/
  `copySaveSlot`（原子）/ `renameSaveSlot` / `deleteSaveSlot`（全
  Result + 点名）
- `src/core/main.cpp`：4 旗标解析（含独占校验）+ `runSaveManagement`
  （headless 执行、stdout 输出、rc 0/1）；usage 更新
- `src/core/game.hpp`：GameArgs 4 管理字段
- **不动**：save.{hpp,cpp}（format 22 不变）/ game 运行路径 / 渲染 / 内容

## 测试

**1882/1882（+10 `SaveSlotTest`**：槽名字符集 / 列表名序+过滤+缺目录空 /
摘要读内容不读钟（二次读全等）/ 缺文件=点名非错 / 坏 JSON 浮现 / 非法名
拒 / copy 字节保真+已存在拒+缺失点名+坏名双侧点名 / **copy 失败零半写
残留** / rename 移动字节+自身目标拒 / delete 缺失点名**）**，`-Werror`
0 警告，banner v1.27.0。

## 门 142（22/22 PASS）

**S1 PASS = 第 142 次结构性免疫 + base18 锚连续**（387726 B 1362×744 =
门 141 刷新锚直接字节一致，免重锚）：quiet config（nomobs +
fire.spread_chance 0 消 churn）fresh 60f 无输入四方 ×2（new v1.27.0-dev ×2 /
old v1.26.0 tm_v126_src ×2）RAW 387726 B 全 EXACT + **new≡old 字节一致**
（管理代码只在管理旗标存在时于 Game 构造前执行 = 普通跑零执行，结构惰性
实证）+ 同侧 savenorm EXACT ×2 + **跨版 save savenorm EXACT**（零 format
变更：两侧恒写 format 22；fires remaining 剥离 = PITS 244 纪律）。

**S2 PASS = headless 管理矩阵 14/14**（fixture = 2 真存档 s142g_a/b
（`--new --frames 1` 生成）+ 1 损坏文件 s142g_bad，全 s142g_ 前缀跑完清理）：
① list ×2 **列表体逐字节一致**（确定性：摘要纯读文件内容无 mtime；INFO
banner 时间戳按设计可变 = PITS 321 剥行比）+ 真存档 fmt 22 行 + 损坏文件
**UNREADABLE: invalid JSON: expected object key string at line 1 column 3
浮现** + 名序（`LC_ALL=C sort -c` = std::sort 字节序）② copy 字节保真
（rc0 + cmp identical）③ copy 已存在 → rc1 "already exists (delete it first
or pick another name)" ④ copy 缺失 → rc1 "no such save slot 'ghost'
(data/saves/ghost.json is not a file)" ⑤ 非法名 `a/b` → rc1 "invalid save
slot name ... (letters, digits, '_' or '-')"（路径穿越死）⑥ rename 移动
字节（src 消失 + dst ≡ src cmp）⑦ rename 自身目标 → rc1 "already exists"
⑧ delete rc0 消失 ⑨ delete 缺失 → rc1 "no such save slot" ⑩ 管理+游戏
旗标 → **rc2** "save management runs without a game: drop the game-run
flags" ⑪ 双管理旗标 → rc2 "one per run" ⑫ **S2d 旧 v1.26.0 二进制拒
`--list-saves` → rc2 "unknown argument"**（负证据：旗标面新旧互斥）。

**S3 PASS**：套件 1882/1882。

**性能 A/B PASS**（真 v1.26.0 tm_v126_perf vs v1.27.0-dev tm_v127_perf，
banner 实证，同款 g139 插桩两侧同打 27 标记，fresh --new --seed 123456789
900f 三窗 + 终档集合）：PERF-WIN 全指标同档（upd med 0.395/0.354/0.416 vs
0.364/0.375/0.436 ms / render med 0.57-0.62 vs 0.57-0.62 / swap med
15.47-15.58 vs 15.52-15.58 vsync 锁定 / fluid 0.049-0.058 vs 0.043-0.055 /
fires 0.008-0.011 / spread 0.0005-0.0006 / ents 0.0032-0.0037 / stream
0.0134-0.0172）+ PIPE3-S0 tickMs med 0.050-0.058 vs 0.043-0.055 同档 +
**changesApplied max 9/3/1 三窗逐位一致** + **终档 fluids 18/18 + blocks
55/55 集合恒等（diff 0）= 同世界同成本**【结构惰性最强形：管理代码只在
管理旗标存在时执行，900f 无旗标跑零触碰】= 成本可忽略，零回归。

## PITS

- **PITS 321**（确定性 stdout ×2 比对须剥 INFO banner 时间戳行 = 比列表体
  `tail -n +2`；名单序断言须 `LC_ALL=C sort -c` = C++ std::sort 字节序，
  locale collation 把字母排下划线前 → g124_s2b 假"unordered"）——门脚本
  3 伪影全修后 22/22 首过。
- **PITS 270 第 2 次复现**（rsync `--exclude "data"` 把 **assets/data**
  一并排除 → 性能树 init "Block directory not found" + "cannot read asset
  file: data/textures/block_atlas.json" 秒死 RC=1）——排除项须精确到
  相对路径根（`--exclude /data` 或只排 /tmp 树专属目录）；补
  `rsync -a assets/data/` 后 ninja "no work to do"（源未变 = 二进制无需
  重编，16 s 重跑收讫）。
