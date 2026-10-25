# Phase PRE-1 — 中文字体（v1.0 任务表 27/32，门 108，PRE 行 1/6）

> UI 文本中文支持：UTF-8 解码渲染 + CJK glyph 图集扩展 + 用户可见 UI
> 字符串本地化。现状 = 16×16 ASCII 字体，中文按 UTF-8 字节逐字节渲染
> 成 `?` 且宽度按字节计。

## 实现内容

1. **UTF-8 解码 + codepoint 映射**（`src/ui/font.{hpp,cpp}`）：
   - 纯函数 `utf8NextCodepoint(text, pos) → optional<uint32_t>`：严格
     1..4 字节解码（拒 overlong / surrogate / >U+10FFFF / 5-6 字节 /
     孤立续字节 / 截断）；非法字节 → nullopt 且 pos +1（每坏字节一个
     `?` = 旧逐字节回退语义保持）
   - `glyphUv(char)` → `glyphUv(uint32_t)`：ASCII 32..126 走**原 cell
     公式零改动**；CJK codepoint 查静态表 `kCjkGlyphs[32]`（codepoint
     排序 + lower_bound 二分）；未知 → `?`（旧回退格不变）
   - `textWidth`/`makeTextVertices` 按解码后 codepoint 迭代（CJK 每字
     1 个 16px 等宽 advance，monospace 纪律保持；ASCII 文本宽度逐位不变）
2. **图集扩展**（`tools/generate_font.py` → `assets/textures/font.png`）：
   - **CJK 区 = 既有 rows 0..3 死格**（ASCII 0..31 控制字符 cell：旧
     代码恒映射 `?`、PNG 恒透明、从未被采样）——32 格恰装 32 字形；
     图集保持 **256×512**（纹理尺寸零变化 = init 上传零增量 + ASCII UV
     位级零变化 = 最高红线满足到字面）
   - CJK glyph 表 = 静态数据（ASCII 同款范式）：离线从 Noto Sans CJK SC
     Regular em 16 提取（行盒 rows 5..20 窗口，阈值 ≥128），TTF 仅工具期
   - **实证**：ASCII 区（像素行 128..511）vs 旧 font.png 逐像素 diff = 0；
     CJK 32 cell 与提取表逐位 0 失配；原 .py 先复现旧 PNG（md5
     a76096fd）→ 新 .py 重跑 ×3 md5 恒 ce205027（确定性钉死 + 测试内
     FNV-1a 64 字节钉）
3. **UI 本地化**（`src/core/game.cpp` 8 处，就地改字面量 = 记档）：
   "You died!"→"你死了！" / "Press R to respawn"→"按 R 重生" / "Furnace"
   →"熔炉" / "E to close"×2→"按 E 关闭" / "Crafting Table"→"工作台" /
   "Inventory"→"背包" / "Crafting"→"合成" / "needs: "→"需要："
   - **保持英文**：F3 调试屏全部（开发者工具 = 记档偏离）；悬停/配方行
     物品名 = JSON 数据驱动英文 id（翻译 = 内容变更，矩阵外）；配方
     " x" 分隔符 = ASCII

**CJK 字符集（32 = 18 UI 字 + 10 中文标点 + 4 余量【物品方块】）**：
" " 。 了 你 关 包 台 合 品 块 工 成 按 方 死 炉 熔 物 生 背 要 重 闭 需 ！
（ ） ， ： ； ？

## 变更文件

- `src/ui/font.{hpp,cpp}`（UTF-8 解码器 + kCjkGlyphs 静态表 + glyphUv
  泛化 + 文本函数 codepoint 迭代）
- `tools/generate_font.py`（+CJK_FONT 32 字形静态表 + rows 0..3 绘制
  + 断言；ASCII 表/写 PNG 逻辑零改动）
- `assets/textures/font.png`（重生成，256×512 不变，rows 0..3 入 1756
  CJK ink 像素）
- `src/core/game.cpp`（8 处字符串）
- `tests/test_font.cpp`（+20：UTF-8 解码 12 例 / ASCII UV 全表位级红线 /
  CJK cell 点名 + 32 表内容钉死 / 宽度·顶点 / 坏字节回退 / font.png
  FNV 钉）

## 测试（1610/1610，+20 新）

`-Werror` 0 警告；既有 7 个 FontTest 零改动全绿（glyphUv(char) 重载
退役 = char→uint32_t 隐式转换语义一致，grep 实证无其他调用点）。

## 门 108

- **S1 PASS = 第 108 次结构性免疫**（10-26 06:09，fresh 60f ×2 RAW
  字节一致 + vs base18 **字节一致**，免重锚；论证 = addText 19 调用点
  全枚举门控，fresh 60f 无输入全为假【hotbar 满堆无数值/无死亡/无
  面板/F3 关】= 零字体绘制 + ASCII 区逐像素不变）
- **S2 死亡屏文本 A/B PASS**（10-26 06:2x）：场景 = 玩家 feet
  (22.5,100,0.5) 坠落实测死（hp 0，终位 (22.5,38,0.5)，900f 死亡屏
  静态 ≥11 s 物理冻结 + mobs off + time 0.25 钉死），NEW ×2 RAW 一致
  + OLD（frozen v0.24.0）×2 RAW 一致 + **A/B diff = 1911 采样 px 限定
  屏幕中部文本带 x[454..900] y[312..412]**（旧 You died! / Press R to
  respawn vs 新 你死了！/ 按 R 重生——世界区逐像素一致 = 同帧同物理）
  + 双侧存档 savenorm 一致。坑：原断言 `c != d` 比较错对象（new_a vs
  new_b 恒假路径 = gate105 同款死代码坑）——diff 块本身正确（old_a vs
  new_a）实证有效
- **S6** = 无（零 config/零 format parse 面；UTF-8 坏字节回退 = 单测面）
- **S3** = 套件 1610 PASS
- 性能 A/B v0.24.0（发布前必做，10-26 06:4x）：fall 900f 三窗
  **fluidCells 123879/167781/236573 双侧逐位一致（同世界）+ upd/render/
  swap/stream 全同档**（重跑对：upd 46.6/49.0/46.7 vs 45.9/48.0/47.6、render
  0.320/0.468/0.642 vs 0.324/0.471/0.657、stream 1842/2129/2502 vs
  1826/2108/2497 = ±1% 级；首轮 stream +30% 偏 = 执行序页缓存环境噪声，
  重跑对称性证伪 = 非回归；两侧 timing-clean 无 stall）= **成本可忽略**
  （图集尺寸 256×512 不变 + 无输入零文本绘制 = 结构惰性实证）

## 偏离 / 记档

- **CJK 区位 = 既有 rows 0..3 死格**（非"向下扩展 atlas 高度"：派发件
  两条红线【ASCII UV 位级不变 + 图集高度扩展】数学不相容——UV 按纹理
  实际高度归一化，高度一变 ASCII UV 浮点必变；死格方案同时满足字面）
- **glyph 集上限 32**（rows 0..3 容量）；扩集 = 重新定案放置策略
- F3 调试屏保持英文；物品名/配方行 = 数据驱动英文 id 不译
- 提取窗口 rows 5..20：全 32 字形 ink 裁剪最小化（仅 `；` 尾 1 行亚阈值
  AA 丢失，视觉不可辨）
- `""` 引号按 Noto CJK 原生设计渲染（开口右上/闭合左上，fontTools 轮廓
  坐标实证 = 忠实字体非镜像错误）
- 坏 UTF-8 语义演进：Latin-1 合法但 UTF-8 无效的串（C3A9）新 = 解码
  U+00E9 表外 → 1 个 `?` 非 2（输入契约 = UTF-8，garbage-in 无既有
  消费者）
