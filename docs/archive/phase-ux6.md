# Phase UX-6 — 字体扩容（v1.x 任务表 39/40，门 160，UX 行 6/7）

> 消费 PRE-1 记档「CJK glyph 静态表 32 + 4 余量——长文本装不下」=
> **CJK 字集 33 → 392 字**（A+ 自动化方案，10-30 定案七点）。字体 = 运行期
> UI 的字符容量上限：教程（UX-7）/ 面板长文 / 未来本地化全靠它。
> **tag v1.40.0 = UX-5+6 合并 tag**（10-30 UX 行政策第 3 个兑现）。

## 定案（10-30 问答，七点）

- **方案 = A+（离线提取自动化）**：字形像素仍是提交数据（font.png +
  .py 内嵌表），运行期零 TTF 依赖、零光栅化依赖；`B`（运行期 stb/
  FreeType 光栅化）= 未来演进路径记档；**系统字体（fontconfig）=
  否决记档**——SDL3 核心无字体 API，官方 addon SDL3_ttf 也只是 FreeType
  封装，fontconfig 选择随机器/系统升级漂移 = 破「像素源全入库版本化」
  的字节复现根基 + 用户机/headless 缺 CJK 字体 = 豆腐块。
- **字集 392 数据定稿**（`/tmp/opencode/pipe3_s0/s160/charset.txt`，
  codepoint 序 0x00B7..0xFF1F）= 游戏文档语料 top 330（GAMEPLAY /
  CONTENT_GUIDE / README 的 CJK 频次）∪ **UX-7 教程全词集（零缺字
  预验）**∪ 既有 33 ∪ 标点（，。！？：；“”‘’（）…—·、）。字集是提交
  数据（.py 内 CJK_CHARSET 段），不依赖外部源可复现性。
- **来源钉定**：`/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc`
  （系统 fonts-noto-cjk 包，2025-02-03 置入），SHA-256
  `b76b0433203017ca80401b2ee0dd69350349871c4b19d504c34dbdd80541690a`
  （19,484,784 B）——脚本记 文件 + 大小 + 哈希 + face index，提取前
  验，不匹配/缺失 = 点名拒，**禁止静默用旧表**（PITS 255-258 漏同步
  坑的字体版）。
- **光栅化参数 = PRE-1 同款**：PIL 13 FreeType（`ImageFont.truetype(
  ttc, size=16, index=2)` 的 getbbox/getmask(mode='L')），em 16、
  line box (19,5)（PIL getmetrics 实证吻合 PRE-1 参数）、rows 5..20
  窗口、coverage ≥128、白墨全 alpha、8px 居中偏移。**选 PIL 而非
  fontTools 手工光栅化**：33/33 自校验要求与 PRE-1 离线提取同款
  光栅器（fontTools 在阈值边界无法保证位级同）；fontTools 4.68.0 仅
  用于 TTC 元数据验证（10 face、SC = index 2、内部版本 2.004）。
- **自校验先行**：脚本先对既有 33 字重光栅化与内嵌 CJK_FONT 表**逐位
  比对**，33/33 全同才继续产出（防 Noto 漂移/参数漂移，CRAFT-4 先例）。
- **产物 = 尾部 append 免重锚**：font.png 256×512 → **256×2080**
  （65 行；新字 359 按 codepoint 序填 cell 128..486 = rows 16..60；
  rows 61..64 = 32 cell 余量——**加字至 424 前零代码零重锚，一条
  命令**）；**顶 512 行逐字节不变**（生成器写盘前对既有文件逐像素
  自检 + 测试重编码 FNV 红线）= ASCII 区 UV 位级恒等 + 免重锚。
- **工具纪律**：generate_font.py 只写 assets/ + .py（src/ 不碰
  范式）；输出 kCjkGlyphs 片段，font.hpp 由人粘贴（机器核验一致）；
  PNG 手写写出保持零第三方运行时依赖（TTF/PIL/fontTools 仅工具期）。

## 实现

1. `tools/generate_font.py`（327→1085 行）：CJK_CHARSET 392 段
   （codepoint 序断言）+ TTF 钉定（哈希/大小/face 数/SC index/版本
   全验）+ legacy 33 位级自校验 + 新字扩展 + 顶 512 像素自检 +
   256×2080 写盘 + 片段输出。ASCII FONT 表 verbatim 零触碰。
2. `assets/textures/font.png`：29,483 B，md5 `704ca41ed294eb79070b8b94930d5b58`
   （重跑 ×3 恒）。
3. `src/ui/font.hpp`：`kCjkGlyphs[33]→[392]`（legacy 33 = cell
   0..31 + 127 位置不变；新字 359 = cell 128..486 codepoint 秩连续）
   + `kFontAtlasHeight 512→2080` + 布局注释。**font.cpp 零改动**
   （glyphUv 二分/advance/makeTextVertices 全走表 = 扩容零代码）。
4. `tests/test_font.cpp`（24→27 测）+ CMakeLists 1.39.0→1.40.0
   + test_debug_overlay F3 钉。

**零 format（22）/零 config/零新资产文件/零音效/零帧状态**（字体 =
init 期一次性纹理上传，无输入零绘制 = 结构惰性）。

## 测试

1978/1978（+3）、`-Werror` 0 警告、banner v1.40.0 实证：

- **CjkTablePinned 演进 392**：计数 / 序端点 0x00B7..0xFF1F /
  legacy 33 精确集 + cell（0..31 + 127）/ extended 359 = 128..486
  codepoint 秩连续
- **CjkExtendedGlyphCellsPinned（新）**：教程 21 字 exact cell
  【僵 172 / 床 260 / 剑 192 / 镐 465 / 睡 380 / 健 171 / 康 264 /
  安 243 / 全 177 / 庇 259 / 护 284 / 所 281 / 挖 287 / 挤 288 / 奶
  237 / 毒 340 / 箭 395 / 岩 255 / 火 352 / 燃 359 / 烧 355】
- **FontPngIsTheUx6Atlas（新）**：常量钉 256×2080 + 解码尺寸/rgba
  大小
- **Top512RowsAreTheLegacyAtlas（新）= 红线最强形**：顶 512 行按
  生成器字节格式重编码（filter None + zlib level 6 单 IDAT）FNV =
  旧全图锚 `74b5d1418347dab6`——确定性写盘器下同哈希 = 字节恒等
  （ASCII rows 4..15 像素红线全覆盖）
- **FontPngBytesPinned FNV 重锚** `027c6b820b2818ad`
- **GlyphUvCjkCellPositions** 加 UX-6 端点/边界演进

**编排者独立核证**（fixer 报告 = claims 惯例）：git 面 6 文件 ✓ /
顶 512 行 raw 扫描线新旧**字节一致**（直接解码比对，不依赖重编码
路径）✓ / 双 FNV 锚复算 ✓ / font.hpp 表与片段逐条一致 ✓ / 重建
1978 自跑 ✓ / 重跑 ×3 md5 恒 + 自校验 33/33 原文 ✓。

## 门 160（编排者执行，old 侧 = v1.39.0 = 316dedf = tm_v140_base）

- **S1 = 第 160 次结构性免疫 + base18 锚连续 168140 B 免重锚**
  （fresh 60f 四方 ×2 全字节一致 new≡old + base18 + 跨版 savenorm
  EXACT；论证 = 无输入无死亡屏/无面板/无 F3/无悬停 → **零字体
  绘制**，atlas 仅 init 重上传零帧状态；顶 512 行位级不变 =
  已绘字形 UV 恒等）。
- **S3** = 1978/1978。**无 S6**（零运行期 parse 面 = 字集是工具期
  数据，运行期只读内嵌表）。
- **性能 A/B v1.39.0 vs v1.40.0（发布前硬门槛）PASS**：fall 900f
  三窗 upd med 0.359/0.404/0.380 vs 0.349/0.386/0.385、render med
  0.395/0.432/0.434 vs 0.386/0.442/0.413、swap med 15.77/15.74/
  15.69 vs 15.84/15.71/15.74 = 全指标同档；PIPE3-S0 changesApplied
  **9/3/1 三窗逐位**；**终档 fluids 18/18 + blocks 55/55 集合恒等
  diff 0 = 同世界同成本【结构惰性最强形】**。

## 偏离/记档

- **项目 FNV 约定 = 非标准混合**（fnv1a64Hex：offset basis
  `1469598103934665603` = FNV-1a **32 位**基数 + 64 位 prime
  `1099511628211`，自 PRE-1 沿用）——独立复算若用标准 64 位基数
  `0xcbf29ce484222325` 会「失配」假警报（**PITS 346**，本轮门核证
  实证：旧全图标准 FNV = 11975ce6d2b3924c ≠ 锚，项目约定 FNV =
  74b5d1418347dab6 吻合）。
- **容量阈值**：rows 61..64 = 32 cell 余量 → **424 字以内加字零
  代码**（改 CJK_CHARSET + 重跑脚本，顶 512 行不变免重锚）；>424
  = atlas 加高（kFontAtlasHeight + 重锚，PRE-4 云先例）。
- **运行期无 TTF 能力**（A+ 定案）：字集外字符 = `?` 回落（既有
  语义不变）；UX-7 教程文本若需新字 = 脚本一条命令（字集已预含
  全教程词集）。
- 教程字「尸/康/晚/僵/造/砍/镐/安/健/恢/怕/睡/近/床/路/附/建/挤/
  怪/剑/庇/所/苦/打/集/觉」26 个首轮缺字 = 定案时直接并入字集
  （不等 UX-7 补 = 衔接零返工）。
