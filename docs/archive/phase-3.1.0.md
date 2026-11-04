# Phase V3-7 — 存档 gzip 压缩（v3.1.0，门 191，全 PASS）

> 设计定案 = `/tmp/opencode/v310_design.md`（唯一范围权威；11-04 用户
> 实机诉求定案：data/saves 体积大 → 整档 gzip(JSON) 落盘，默认开，
> V2-19 提前项同款先例，独立 V3-1）。MC 机制核定 = gzip 容器 RFC 1952
> （10B 头 MTIME/FLG/XFL/OS 字段 + raw deflate + CRC32/ISIZE 尾）；
> 项目取舍 = 手写容器钉死每字节（zlib 自带 gzip 封装的 MTIME/XFL/OS
> 随 zlib 版本漂移，确定性一级纪律排除）。实测压缩率立项前（11-04）
> 5.4×（4 万块 journal 档 1.74 MB → 0.32 MB）/ 小档 84% / 2.7 MB
> 门档外推 ≈ 0.5 MB。

## 实现

### 确定性 gzip 容器（src/core/save.cpp，新函数族，手写 = 字节可钉）
- **编码** `gzipEncode`：固定 10B 头 `kGzipHeader` = `1f 8b | 08
  (CM=deflate) | 00 (FLG=0: 无 FEXTRA/FNAME/FCOMMENT/FHCRC) | 00 00
  00 00 (MTIME=0) | 00 (XFL=0: level 6 = zlib 默认档, 0 诚实) | 03
  (OS=Unix)` + raw deflate（`deflateInit2(level = kSaveGzipLevel 6,
  Z_DEFLATED, windowBits = -15, memLevel = kSaveGzipMemLevel 8,
  Z_DEFAULT_STRATEGY)`，32 KiB 块循环至 Z_STREAM_END）+ 尾 CRC32(LE)
  + ISIZE(LE)。同输入 = 逐字节同输出（单测 ×2 恒等钉 + 20B 最小容器
  逐字节钉：空 payload = 头 + `03 00`（final stored block）+ 8B 零尾）。
- **偏离 1（实施期发现，验收）**：环境 zlib 1.3.2 的 zconf.h **无
  `Z_DEFAULT_MEM_LEVEL` 宏**（上游有，本机发行版裁剪；编译实证）→
  命名常量 `kSaveGzipMemLevel = 8`（= zlib 文档默认档）钉死，容器
  字节零影响。
- **解码** `gzipDecode`（写侧精确逆，机器生成文件纪律 = 严格）：
  <20B → "truncated gzip" / 10B 头逐字节 ≠ kGzipHeader → "unsupported
  gzip header"（带偏离字节号 + 实际/期望 hex——系统 gzip CLI 的真
  MTIME / 外 OS 字节同样点名，不静默容忍）/ inflate 数据错 →
  "corrupt gzip body"（半体断流 = "deflate stream ends before its
  final block"）/ 尾 CRC32 或 ISIZE 不符 → "corrupt gzip trailer"
  （带计算/存储值 hex；亦捕获容器后追加的尾随数据）。全部点名带
  `what` = 文件路径。
- **魔数探测** `looksLikeGzip`（前两字节 1f 8b；<2B = false）= **唯一
  编码判据**；非魔数 = 遗留明文路径位级不变（零迁移根）。
- **config 严格读** `saveCompression`（`save.auto_save_seconds` +
  `render.ao` 双先例）：absent = **Gzip（代码默认 = 用户定案**；
  committed config.json 无 save 节 = 零 config 改动）/ "gzip" /
  "none"；非 string / 空串 / 未知词 → init 点名拒（消息含
  "save.compression"）。init 读入 `Game::m_saveCompression`
  （game_init.cpp autoSave 块旁）。
- **zlib 零新依赖**（自研 PNG 已链接，CMake pkg_check_modules(ZLIB)）。

### 集成（写侧单 choke point / 读侧编码无关）
- `Game::writeSave`（exit/F5/autosave 共享）：payload = 既有
  `dump(2) + '\n'` 一字不动 → 按 config gzipEncode 或原样 →
  `writeAtomicFile` 零改动（tmp+rename 原子性对 gzip 字节同样成立 =
  崩溃安全不变）。info 日志加耗时 ms + 体积变换（`gzip: 4237 B
  payload -> 678 B on disk` / `plain: 4234 B`）——real-dt 性能证据，
  不入任何字节门。
- `Game::readSaveFile`：二进制读原始字节 → looksLikeGzip 探针 →
  gzipDecode（失败 = 点名 MC_FAIL）→ Json::parse 后续
  parseSave/validateStats/validateEnchants **零改动**。
- `tryLoadSave` "Loaded save" 行加耗时 ms + 编码标记（`gzip: 678 B
  on disk` / `plain`）。
- `save_slot.cpp::readSaveSlotSummary`（`--list-saves`）同款探针；
  解码失败 = `problem = "invalid gzip: ..."`（readable=false，文件
  留列表 = 既有 problem 命名纪律）。`copy/rename/delete` = 纯字节
  操作零改动（gzip 槽 copy = 字节忠实 gzip 槽）。
- `tools/gate_compare.py`：`cmp_norm` 加 `load_save`（魔数 →
  gzip.decompress）；`cmp_raw` 不变（raw 字节比对 = 恒等最强口径）。

### format 29（schema 零变更，时代标记）
- `kSaveFormat` 28→29；save.hpp 注释块 29 段 = "GZIP ERA"：编码与
  format 号**正交**（魔数唯一判据；none 配置的 v3.1.0 文件 = 明文，
  与同状态 v3.0.0 文件差异 = 恰 root.format 单键）；per-format 规则
  全为 `>= N` 下界检查 → 29 自动继承 28 全部规则，**解析侧代码零
  改动**；旧构建拒 29 = 任何 format bump 同款版本互斥。
- 既有测试 12 处 "writes are always format 28" 字面量钉 → 29
  （test_save ×9 / test_bed / test_campfire / test_durability 各 1；
  PITS 355 钉纪律同款强制有意识更新，测试逻辑零改动）。

### 单测族（tests/test_save_gzip.cpp 新 19 例 + test_save_slot.cpp +2）
- roundtrip ×4（32 B / 1140 B 精确 / 100 KB / 真 format-29
  serializeSave fixture——恒等式 `gunzip(gzip) ≡ payload` 逐字节）。
- 确定性钉 ×4（10B 头逐字节 = kGzipHeader / 同输入 ×2 恒等 / 异输入
  异输出 / 空 payload 20B 最小容器逐字节 `kGzipHeader + 03 00 + 8×00`）。
- 魔数判定 ×2（1f 8b 真 / 1f 00、00 8b、`{\n`、1 B、空 六形假 +
  真明文档 `{\n` 首两字节钉）。
- 拒收 ×4（gzip 内非 JSON = decode OK + Json::parse 点名 / 截断三形
  【10B 头、19B、半体】点名带路径 / 头偏离四字节【CM=1、FLG=4、
  XFL=2、OS=FF】点名 / 尾 CRC·ISIZE 位翻点名带 hex）。
- config 严格读 ×4（absent→Gzip / gzip·none 词 / 非 string ×2 点名 /
  未知词 lzma + 空串点名）。
- none 位级恒等 ×1（"format" 全文恰 1 次结构证明 + 模拟 v3.0.0
  format-28 单键翻转 → 重解析 → 重序列化 ≡ format-29 原文逐字节）。
- save_slot ×2（gzip 槽 summary 可读且与明文孪生同值 / corrupt 槽
  【15B 截断 + 半体】problem = "invalid gzip: truncated gzip /
  corrupt gzip body"）。

## 门 191（结构免疫第 72 次；锚 224468 B 不变免重锚）

**结构免疫论证**：存档 = 渲染无关路径——无输入 60f 运行中零存档
参与（autosave 默认 120 s real-dt ≫ 运行时长；读侧仅 `--save` 启动、
写侧仅退出/F5/autosave），截图管线零触碰。

- **S1** 无输入 `--new --save --frames 60 --config QUIET --screenshot`
  ×2：截图 RAW ×2 恒等 **且 ≡ 锚 224468 B 逐字节**（第 72 次结构
  免疫 PASS）；存档 10B 头逐字节 = kGzipHeader（`1f8b 0800 0000 0000
  0003` 实证）；落盘 677 B / payload 4237 B = **6.3×**；存档双跑 RAW
  diff = **恰 fires[0..2].remaining 三键**（演示区燃烧火块 = 硬规则 8
  real-dt 字段，PITS 244/P56 savenorm 剥离族，与 gzip 无关的正交
  漂移）→ **savenorm EXACT**（既有口径）；gzip 容器层确定性由单测
  `SameInputYieldsIdenticalOutput` 钉（E2E 层 RAW 恒等仅当 payload
  稳定时成立 = fires 无燃烧的场景才可达，本门不另设）。
- **S2** 存档格式四臂（全 headless CLI）：
  (a) **gzip roundtrip**：默认跑 678 B gzip（6.25×）→ 再跑加载
  "Loaded save ... in 0.3 ms (gzip: 678 B on disk)" → pos 保持
  [0.5,54,0.5] + time 恰 +0.001667（= 60f × 1/36000 帧锁定推进，
  python 验算恒等）；reload ×2（PITS 15 每次自 a1 新鲜副本）=
  savenorm EXACT（fires real-dt 同族）。
  (b) **遗留零迁移（最强形式）**：v3.0.0 参照树（tm_v310_base，
  banner v3.0.0 实证 = PITS 379①）fresh-60f = 明文 format 28；
  savenorm(old fresh-60f, new fresh-60f) = **恰 `.format: 28 vs 29`
  单键**（schema 恒等实证）；新构建 `--frames 1` 直载旧档 =
  "Loaded save ... (plain)" + 日志 6 blocks/2 fluids/0 entities ==
  文件 JSON 计数（faithful load 逐计数断言）。
  (c) **none 模式**：`save.compression: "none"` 跑 = 明文首字节
  `7b 0a` + savenorm ≡ gzip 档 gunzip 后（raw diff = fires real-dt
  同族；容器恒等式由单测钉）。
  (d) **corrupt**：512 B 截断 gzip → 启动 exit=1 点名 "has a corrupt
  gzip body (the deflate stream ends before its final block)" 带完整
  路径，不崩溃，半档不留（readSaveFile 在 writeAtomicFile 前失败 =
  原文件未触）。
- **S3** 全套测试 2503/2503（编排者独立复跑）。
- **S6** 点名 3/3（`save.compression: true` / `1` / `"lzma"` → init
  拒 rc=1，消息含 "save.compression"；bool/num 同句 "must be the
  string"、word 句带 got 值）。
- **性能 A/B（用户硬要求）**：旧 = v3.0.0（tm_v310_perf = 8ecf674 +
  g139，banner v3.0.0 实证），新 = v3.1.0（tm_v310_perf_new = worktree
  + g139，banner v3.1.0 实证）。主对 900f 无输入同 seed：PERF-WIN
  三窗全指标同档（upd med 0.651/0.636/0.598 vs 0.605/0.537/0.580 ms /
  render med 0.650/0.641/0.574 vs 0.636/0.549/0.561 / swap med
  15.16/15.29/15.40 vs 15.31/15.46/15.41 = vsync 主导 / fluid·fires·
  spread·ents·stream 全同档）+ **changesApplied 9/1/3 逐位恒等** +
  **终档 savenorm 恰 `.format: 28 vs 29` 单键**（集合恒等最强形）。
  **IO 增量口径**：save 写 gzip 1.1 ms（6744 B payload → 899 B 落盘
  = 7.5×，旧侧无压缩行 = 基线；2.7 MB 级大档外推 ≈ 0.5 MB 落盘、
  压缩百 ms 级 ≪ 既有 journal 退出成本）/ load inflate 0.2-0.3 ms
  （S2 臂实证）。门内无可测系统增量（gzip 仅退出/载档路径消费，
  900f 帧内零参与）。

## 实测数据

- 门档级（demo 区 6 块 2 流体 40+ 帧）：4237 B payload → 677-678 B
  落盘 = **6.3×（−84%）**。
- 立项前大档实测（11-04）：4 万块 journal 档 1.74 MB → 0.32 MB =
  5.4×（−82%）。
- save 写耗时：1.2 ms（4 KB 档，含 collectSave+serialize+gzip+
  原子写）/ load 0.2-0.3 ms（含 inflate+parse+apply）。
- 单测新增 21 例（2482 → 2503）。

## 偏离/记档

1. `Z_DEFAULT_MEM_LEVEL` 宏本机 zlib 1.3.2 缺失 → 命名常量 8
   （实施期发现，行为 = zlib 文档默认，容器字节不变）。
2. S1 存档双跑 RAW 恒等最强形在 demo 区场景不可达（fires real-dt
   漂移，正交既有 savenorm 口径）→ 门按 savenorm EXACT + 头钉 +
   单测容器恒等收口；RAW 恒等的 E2E 适用域 = 无燃烧火场景（记档，
   不另设门臂）。
3. 既有测试 12 处 format 字面量钉 28→29（设计红线"既有测试零改动"
   未预见 format 字面量同类；PITS 355 钉纪律 = 有意识更新，逻辑零
   改动）。

## 变更文件（显式路径）

src/core/save.hpp（29 注释块 + enum + 常量 + 4 API 声明）/
src/core/save.cpp（+217：4 实现 + zlib 头）/ src/core/game.hpp
（m_saveCompression）/ src/core/game_init.cpp（+9 init 块）/
src/core/game_save.cpp（writeSave 编码分支 + readSaveFile 探针 +
日志 ms）/ src/core/save_slot.cpp（summary 探针）/
tools/gate_compare.py（load_save 解压步）/ tests/test_save_gzip.cpp
（新，19 例）/ tests/test_save_slot.cpp（+2 例）/
tests/test_save.cpp + test_bed + test_campfire + test_durability
（format 字面量 12 处）/ tests/test_debug_overlay.cpp（F3 钉
v3.1.0）/ CMakeLists.txt（VERSION 3.1.0 + 测试列表）。

零改动红线核证：解析侧 schema/迁移/交叉检（parseSave 全部）/
writeAtomicFile / copy·rename·delete / 渲染管线 / config.json（用户
ui.scale 2.0 工作树改动原样）/ 场景文件机制（恒明文，魔数直读）。
