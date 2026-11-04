# 版本历史（CHANGELOG）

> 本文件只留 **3.x 时代**（「世界扩展」期，v3.0.0 起）逐版明细。
> 2.x 全史（v2.19.0→v2.0.0，两批 20 版）→ `docs/archive/v2x/
> CHANGELOG.md`；v1.x（v1.50.0→v1.0）→ `docs/archive/v1x/CHANGELOG.md`；
> v0.x → `docs/archive/v0x/CHANGELOG.md`。逐版细节（机制/证据/偏离/
> 性能 A/B）见对应时代 `docs/archive/*/phase-*.md` 阶段存档。
> README 只保留里程碑版本（v0.1.0 首个公开版 / v1.0.0 首个正式版 /
> v2.0.0 1.x 收官 / v3.0.0 3.x 开启）。
> 版本命名注：3.x「世界扩展」期 11-04 曾命名 4.x（2.x 第二批收官日
> 晋升定案），11-04 同日用户重编号定案改 3.x（2.x 已吸收原 3.x「系
> 统扩展」八项，回归自然序号）——任务号 V3-1~V3-7。

## v3.1.0（2026-11-04）— V3-7 存档 gzip 压缩（3.x 首功能版，门 191 全 PASS）

11-04 用户实机诉求定案（data/saves 体积大）先执项：**整档 gzip(JSON)
落盘，默认开**——① **确定性 gzip 容器**（手写 = 字节可钉：固定 10B
头 `1f 8b 08 00 00 00 00 00 00 03`【FLG=0/MTIME=0/XFL=0/OS=3】+ raw
deflate level 6 + CRC32/ISIZE 尾；zlib 自带 gzip 封装的 MTIME/XFL/OS
随版本漂移 = 排除，PITS 385①）：同世界存档逐字节可复现；② **加载
魔数 `1f 8b` 自动探测**（唯一编码判据，扩展名恒 .json）：旧明文档
**零迁移直读**（format 28 及更早明文档新构建原样加载）；解码严格
（头 10B 逐字节全钉 / 截断 / 半体 / CRC·ISIZE 不符全点名带路径，
系统 gzip CLI 产物点名拒收不静默容忍）；③ **config 键
`save.compression: "gzip"|"none"` 严格读**（`save.auto_save_seconds` +
`render.ao` 双先例）：absent = **gzip（代码默认 = 用户定案**；
committed config.json 无 save 节 = 零 config 改动）/ "none" = 明文
（旧行为位级恒等：同状态文件与 v3.0.0 差异 = 恰 root.format 单键）；
非 string / 空串 / 未知词 init 点名拒；④ **format 29**（schema 与 28
完全一致——编码与 format 号正交，29 = "gzip 时代"标记；per-format
规则全 `>= N` 下界检查 → 解析侧代码零改动）；⑤ **集成 = 写侧单
choke point**（`Game::writeSave` exit/F5/autosave 共享：payload
`dump(2)+'\n'` 一字不动 → 按 config gzip 或明文 → `writeAtomicFile`
原子性不变）**/ 读侧两个消费点**（`Game::readSaveFile` +
`--list-saves` 摘要，corrupt 槽 problem 命名不隐藏文件）；
save copy/rename/delete = 纯字节操作零改动；⑥ **`tools/
gate_compare.py` savenorm 加解压步**（魔数 → gzip.decompress）；
⑦ **save/load 日志加耗时 ms + 体积变换**（real-dt 性能证据，不入
字节门）。**zlib 零新依赖**（自研 PNG 已链接）。门 191 = **结构免疫
第 72 次免重锚**（存档 = 渲染无关路径，零像素）：S1 无输入 60f ×2
≡ 锚 224468 逐字节 + 头 10B 实证 + 存档 savenorm EXACT（RAW diff =
fires real-dt 三键 = PITS 385③ 既有口径）+ **S2 四臂**（gzip
roundtrip 6.25× + pos/time 帧锁定保持 / **遗留零迁移最强形式 =
savenorm 新旧 fresh-60f 恰 `.format: 28 vs 29` 单键** + faithful load
逐计数 / none 明文 + savenorm 恒等 / corrupt 512B 截断 exit=1 点名
不崩溃）+ S6 点名 3/3 + 性能 A/B 同世界同成本（IO 增量口径：save
写 gzip 1.2 ms / load inflate 0.3 ms @ 4 KB 档，门档 2.7 MB 级外推
≈ 0.5 MB 落盘）。实测压缩率：门档 4237 B → 677 B = **6.3×**（立项
前大档 5.4× = 4 万块 journal 1.74 MB → 0.32 MB）。2503/2503（+21：
test_save_gzip 19 + save_slot 2；既有 12 处 format 字面量钉 28→29
有意识更新，逻辑零改动）。PITS 385（① zlib 封装字节漂移 = 手写容器
② 本机 zlib 1.3.2 缺 Z_DEFAULT_MEM_LEVEL 宏 = 命名常量 8 ③ 双跑 RAW
恒等适用域 = 无火场景）。详见 `docs/archive/phase-3.1.0.md`。

## v3.0.0（2026-11-04）— 3.x「世界扩展」期开启（原 4.x 命名重编号；纯文档 + banner，零玩法变更）

2.x 全史收官（v2.19.0，两批 20 版）后的时代开启版：**① 重编号定案
（11-04 同日用户定案）**——「世界扩展」期任务表由原 4.x 命名改
**3.x**（2.x 已吸收原 3.x「系统扩展」八项 = V2-11~V2-18，「世界扩
展」回归自然序号）：任务号 V4-1~V4-7 → **V3-1~V3-7**（V3-1 下界
维度锚 / V3-2 下界内容族 / V3-3 下界 mob / V3-4 netherite 材料链 /
V3-5 建筑内容族 / V3-6 世界边缘雾 / V3-7 存档 gzip 压缩【11-04 用户
实机诉求，默认开 = format 29，先执 = **v3.1.0**】），执行权威 =
`docs/BACKLOG.md`（3.x 任务表）；**② README「版本与发布」精简为四里
程碑**（v0.1.0 / v1.0.0 / v2.0.0 / v3.0.0，逐版明细指向 CHANGELOG
时代分档）；**③ 活文档重编号刷新**（AGENTS/BACKLOG/CHANGELOG 时代
切换）。**零玩法/零 format/零像素变更**：banner v2.19.0 → v3.0.0
（PITS 355 三件套：CMakeLists PROJECT_VERSION 单点 + test_debug_
overlay F3 钉 + banner 实证）+ 全套测试复跑 + 无输入 base18 结构免
疫（banner 仅 F3 调试覆盖层 + 启动日志消费，截图零触碰）。下一版
**v3.1.0 = V3-7 存档 gzip 压缩**（用户开新会话后执行），其后 V3-1
下界维度拆分立项。
