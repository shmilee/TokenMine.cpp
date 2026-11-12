# Phase v3.47.0 — V3-20 用户实机诉求批：demo 中文标题 + 菜单全景背景 + 戏字 + 主机无限等待（门 237）

> 11-12 用户实机 4 条反馈（v3.46.0 多人页首用）：①教学二级菜单是英文
> ②菜单黑屏不美观（建议默认种子固定游戏界面）③「创建游？」缺字
> ④主机 10s 无人连接即报错退出。**V3-20**（11-12 用户实机诉求批，
> BACKLOG 新增——V3-18/V3-19 同款先例）。

## 实现

### ① demo 中文标题（数据驱动，零核心渲染代码）
- index.json 每条目 +`title`（可选——absent = 文件名回落；10 条全配
  中文：挖矿打造工具/种植与进食/受伤与回血/房子与床/建造皇宫/材料
  收集/引水成河/黑夜历险/下界传送门/末影龙讨伐）。
- parseDemoIndex：key 词表 +title + **字形安全 fail-fast**（title 每
  码点 = 可打印 ASCII 或 kCjkGlyphs 成员，否则命名拒收 U+XXXX——
  ③「创建游？」的门盲点反转为输入纪律：菜单显示名缺字 = init 点名，
  不再静默 '?'）。
- 显示面 ×2：菜单 tier-2 行（title · mm:ss）+ --list-demos 列。

### ③ 字体 +12（戏植血房宫皇城河黑传送讨）
- ③实证 = 戏 U+620F **不在 457 表**（v3.46.0 文档误记「全在表内」=
  门 236 盲点：多人页从未被截图断言——S1 锚 = 标题页、canary = solo
  demo、网络臂 = 日志断言，无一覆盖多人页像素）。
- 12 字 = 戏 + 10 标题用字 + 讨（末影龙讨伐）——代码点序插入 + 后缀
  +1..+12 重格，atlas 2208→2272（71 行，region 551 满后增长）；
  top-512 红线恒等；FNV re-pin 5062bdace4d01da3；**跨树文本边缘
  白名单一次性门面**（PITS 425④，canary 审计脚本复用）。

### ② 菜单全景背景（MC 全景形）
- **`--panorama <out>` CLI seam**（管理旗——互斥全部游戏旗）：config
  种子 fresh 世界 + **40 帧 update 预热**（fixed dt 1/60——地形网格
  在 update 的流送集成，render-only 预热 = 纯天空门内捕获）+ 瞭望点
  （出生点 +18 块俯角 0.30 rad）+ **8 向 640×360 渲染拼接 5120×360
  环带** + 首帧 discard render（Mesa 首次交换黑帧门内捕获）；确定性
  = ×2 字节恒等（门臂）。committed 资产 assets/textures/panorama.png
  （704 KB，seed 123456789）。
- **renderMenu 背景**：环带纹理延迟加载（仅菜单路径——demo/net/--new
  零成本）+ **cover-fit UV**（等比填充——非等比拉伸 = 树木拉尖门内
  捕获）+ 帧锁定慢速平移（m_menuFrame 纯函数 = ×2 RAW 确定性；环带
  闭环 = GL_REPEAT 无缝）+ 0.55 冷暗 tint 面纱（文字对比度）；资产
  缺失 = named warn + 旧纯色 clear 回落（菜单不因装饰缺失失败）。
- render() +`withUi` 参数（缺省 true 位级红线；全景 seam 用 false =
  无 HUD 世界 pass）。
- takeScreenshot 重构 = readFrontBuffer 提取（拼接复用）。

### ④ 主机无限等待（10s 硬超时的用户报障）
- netHostHandshake 首客 = **无限等待**：250ms accept 轮询 + **1s 重
  渲染节拍**（等待期窗口显示天空 + HUD——黑窗改善）+ **10s 心跳日志**
  + **ESC/关窗 = 命名取消**（输入泵——等待期可退出，进程不再僵死）；
  附加客 3s 窗不变（N 定形 lockstep 前提）；kNetStallTimeoutSec 保留
  （运行期屏障 stall 消费）。
- 模拟冻结 = 等待期无 update() → 世界静态 → fresh WELCOME 零字节权
  威不变（首客仍走 fresh 形，无 REJOIN 快照复杂度）；mob/天气无活动
  （循环未启动）。

## 门内捕获（PITS 429 五坑）
① **新菜单页须截图断言（门盲点族首案）**：「创建游？」= 戏 U+620F
缺失——门 236 的 S1（标题页）/canary（solo demo）/网络臂（日志）
无一覆盖多人页像素；且 v3.46.0 文档误记「全在 457 表」（侦察输出
被误读）。纪律 = 新菜单页进门至少一张 ×2 RAW 截图断言 + 词表核证
输出逐字回读。② **全景两预热**：地形网格在 update() 流送集成
（render-only 预热 = 纯天空）+ Mesa 首交换黑帧（首帧 discard）。
③ **cover-fit 非 stretch**：非等比拉伸 = 树木拉尖 + 云带噪化（截图
捕获）；环带须 360° 闭环（GL_REPEAT 无缝平移的前提）。④ --list-
demos CJK 列宽 = 字节 padding（cosmetic 记录）。⑤ **net 臂存档对跑
须过火 real-dt 稳态帧**（PITS 385 族再确认）：等待臂 300f 宿主档
×2 差 = 1 簇火燃尽边界翻面（7 vs 8 fires）——700f 稳态 = EXACT；
live net 臂存档断言 ≥700f（demo 帧锁定臂豁免）。

## 门结论（门 237 全 PASS）

**3022/3022**（+1 V316aDemo.IndexTitleParse：absent 回落 / round-
trip / 非串 / 空 / 缺字形 U+9F98 点名 / 坏 UTF-8；banner re-pin 1）。

**S1 重锚 base20 = v3.47.0 全景标题**【f60 无输入 ×2 RAW 逐字节
（pan 帧锁定纯函数实证）+ 与旧锚 diff = 100% 全背景【用户诉求的
视觉变更——论证 = 全景即所求】+ defined mouse (10,10) + 槽不创建
→ 111279 B】。

**全景臂**：`--panorama` ×2 **字节恒等**（生成器确定性 = 同 seed
同视点的纯函数）+ **≡ committed 资产**（704 KB 5120×360，seed
123456789 出生点瞭望 +18 俯 0.30 rad，8 向）= 资产可从仓库再生
（PITS 317 纪律）。

**标题臂**：--list-demos 中文列（挖矿打造工具…末影龙讨伐 10 条）。
**S6 字形安全**：篡改 index 缺字形 title（于龘）→ boot rc 1
命名拒收 `missing glyph 'U+9F98' (the menu's '?' would render)`
（③的门盲点反转为输入纪律实证）。

**菜单页臂**：多人页 ×2 RAW EXACT（**创建游戏/加入游戏全字形 =
戏修复实证**）+ 教学 tier-2 ×2 RAW EXACT（中文标题 + mm:ss）。

**等待臂（④用户报障正面实证）**：菜单主机 wgate + CLI 客机 **13 s
延迟连接**（旧 10 s 超时的死区内）×2：心跳日志 ≥1 行（"still
waiting for the first client (10 s — the door's open)"）+ 客机
`joined the host` + 客机 `net chat[wgate]: TokenMine.cpp`（WELCOME
hostName 面回归）+ 双 rc 0 + **宿主档 ×2 savenorm EXACT @700f**
（PITS 429⑤ 火稳态帧纪律）。

**Canary 4×2 新≡旧（tm_v346 v3.46.0）**【字体增长 = 一次性文本
边缘白名单】：树内 RAW r1==r2 EXACT ×4 + savenorm 跨树 EXACT ×4 +
跨树 RAW = **144-187 px（0.016-0.020%）文本边缘，逐 px 审计
100% 通过**（diff 行全 UI 文本带：clock/badge/finished 面板/toast；
世界/心行/图标 0 差；审计脚本 g237/canary_audit.py 复用 g233）——
17e 起（无更多字体变化）恢复 EXACT。

**性能 A/B 同档**【OW 900f --new 双树 g139：new upd med 0.396/
0.468/0.410 vs old 0.407/0.424/0.456（median 交错：win1 old
+0.011 / win2 new +0.044 / win3 new −0.046 = 机载噪声域）+ render
0.540/0.611/0.583 vs 0.546/0.561/0.615 同档 + swap 15.55/15.38/
15.49 vs 15.53/15.48/15.35 同档 + **changesApplied 9/3/1 逐位 ×2**
+ **终档跨树 savenorm EXACT** = 菜单装饰零世界面结构零成本】。
