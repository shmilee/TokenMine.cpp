# Phase v3.44.0 — V3-17b 主菜单：设置页（门 234）

> V3-17 相位 B/4（17a 标题 + 教学入口 = v3.43.0 / **17b 设置 = 本版** /
> 17c 登陆 + 教学页两级 / 17d 多人页 + MOTD）。BACKLOG 规格 = config
> 表单化编辑：view_distance/day_length/天气/声音/ui.scale/窗口——严格
> 读 + 写回 config.json，config 纪律不变。**零模拟面**：设置页 = 菜单
> 表现层（m_config 的编辑 + 落盘），世界未建；唯一运行时生效面 =
> ui.scale（m_uiScale 每帧 uiSize() 读——菜单布局实时 reflow = 活体
> 证明；世界启动时 buildScene 重读 config = 同值）。

## 实现

### 表单（menu_layout.hpp 纯面 + game_menu.cpp codec）
8 行 + 保存钮 + 「部分项重启后生效」hint：

| 行 | 键 | 形态 | 域（clamp） | 生效 |
|---|---|---|---|---|
| 视图距离 | world.view_distance | stepper ±4 | 4..64 | 世界启动（streamRadius = buildScene 读） |
| 白天长度 | world.day_length_seconds | stepper ±60 | 60..86400 | 世界启动（帧锁定步长） |
| 天气 | render.weather | toggle 开/关 | — | 世界启动（无天气的菜单期无可见面） |
| 声音 | audio.volume | stepper ±1（×10） | 0..10 | 世界启动（init 面） |
| 界面缩放 | ui.scale | stepper ±1（×2） | 2..8 | **运行时**（m_uiScale 活面——菜单 reflow） |
| 全屏 | window.fullscreen | toggle 开/关 | — | 下次启动（Window::init only，无运行时 apply 路径） |
| 窗口宽 | window.width | stepper ±64 | 640..2560 | 下次启动 |
| 窗口高 | window.height | stepper ±64 | 480..1440 | 下次启动 |

- 严格读 = 既有 strict 助手的 Result 拒收面（uiScaleFromConfig /
  weatherEnabled 同款——表单值域 = 助手域的同源 clamp）。
- **零文本输入**：数值全 stepper/toggle（SDL 文本输入面不碰——
  V3-11b 创造搜索的 SDL_StartTextInput 仅创造面板期开的先例不扩展）。
- 控件 id 编码 = row*2（minus/toggle）/ row*2+1（plus；toggle 行 dead）
  / kMenuSettingsSave 32；`menuSettingsControl/Step` 纯函数（test 孪生；
  gate xdotool 点击同源公式）。
- 输入面：ESC = 返回标题，Enter = 保存（便利面），click = 控件动作。

### 写回（P32 config 纪律面）
`Config::save(m_config.path())`（**既存未用**的 config.cpp:57 现成）
仅在保存钮 confirm 后触发。Json::Object = std::map → dump(2) 全量重写
= **键序字母归一 + 2-space 缩进**（committed config 的手排键序/
1-space 缩进变——值全保 = 未动键原样存活，gate 断言面；整档重写 =
记录偏离，手术式写回需保序容器 = 不值，YAGNI）。数字格式 = 整值出
整数（80 非 80.0）+ 非整最短往返（0.7 非 0.700000）——Json::dump 现
形。

### 字体 +3（屏宽项）
设置页词表核证缺 屏 U+5C4F / 宽 U+5BBD / 项 U+9879——代码点序插入 +
后缀 +1..+3 重格；**atlas 2208 不增长**（region 424 扩展字形 128..551，
max cell 551 = row 68——69 行的 headroom 承接）= **归一化 v-UV 不变**
→ 跨二进制文本边缘差（PITS 425④ 17a 的门面）**不复发**——canary
跨树 RAW 应恢复 EXACT（本版验证）。top-512 红线恒等 + FNV re-pin
6b4fde4b271b467e + test_font 全 re-pin。

### 测试（+2 = 3012）
MenuSettingsTest：LayoutGeometryAtGateSpace（453×496 全 rect 钉 + 8 面
hit test【含 toggle plus dead】）/ StepActions（11 面：esc/dec/inc/
toggle×2/save 钮/Enter 便利/air click/无输入）。

## 门内捕获（PITS 426 两坑）
① **设置表单 value 列中心 = 标签右缘与按钮左缘的中点**：初版
`(w-150)/2` = 151（453 空间）→ 值文本（4 字符 @1.5 ≈ 100 px 宽）左缘
101 与标签右缘 140 重叠（截图冒烟「视图距离80」叠字）——修 = `w/2`
（226）+ test 钉。② **菜单标题含版本串 = S1 锚逐版重锚**：标题版本行
「v3.43.0→v3.44.0」= 有意的像素变化——S1 断言改形 = ×2 逐字节（无
输入可复现不变式）+ 与旧锚 diff 有界断言【95 px 单行 rows 164-168 =
版本行】+ 重锚（base20 v3.44.0 21917 B）；17c/17d 同款。

## 门结论（门 234 全 PASS）

**3012/3012**（+2 MenuSettingsTest：LayoutGeometryAtGateSpace 8 面 hit
+ StepActions 11 面；banner re-pin 1）。

**S1 base20 重锚（版本行论证）**【f60 无输入 = 标题 ×2 RAW 逐字节 +
与旧锚 diff = 95 px 单行（rows 164-168）= 版本行 v3.43.0→v3.44.0
有界断言 PASS → base20 = v3.44.0 标题 21917 B】+ defined mouse face
(10,10) no-hover 纪律继承。

**设置臂（表单活面 + 写回）**：xdotool 序列【设置 → 天气 开→关 →
界面缩放 1.5→2.0（LIVE reflow——点击序 = 布局稳定形：reflow 控件在
@1.5 几何点击中最后，保存按 @2.0 几何 (340,640)）→ 保存】：
`settings saved to config_set.json` + **写回断言 PASS**【ui.scale 2.0
+ render.weather false 落盘 + view_distance 80 / day_length 600 / seed
/ window 尺寸 / player 节全存活——整档 dump 值保全实证】+ **a3 臂**
【修改后 config 的标题 ×2 RAW EXACT @ scale 2.0——新 ui 几何的确定
性】。

**Canary 4×2 新≡旧（tm_v343 v3.43.0）**【设置页只在点击后画 = 结构
免疫】：树内 RAW r1==r2 EXACT ×4 + savenorm 跨树 EXACT ×4 + **跨树
RAW EXACT ×4——PITS 425④ 文本边缘白名单的一次性门面验证**（17b +3
字形走 headroom 不增 atlas = v-UV 不变 = 白名单不复发；17c/17d 同
字体 = 跨树 RAW 恒 EXACT 直至下次字体增长）。

**性能 A/B 同档**【旧 = tm_v343 + g139 双树：OW 900f 无输入 new upd
med 0.589/0.558/0.604 vs old 0.552/0.606/0.602——median 交错（win2
new 低 0.049 / win3 恒等 ±0.002 / win1 new 高 0.037 = 机载噪声带）+
render/swap 同档 + changesApplied 9/3/1 逐位 + **终档跨树 savenorm
EXACT** = 设置页零世界面结构零成本】。
