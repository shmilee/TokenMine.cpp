# v3.52.0 demo 重拍批——⑤ guide_food 全链农耕重拍（门 244，11-25）

> V3-24「4.0 预发布 demo 检查」批量 9 支的第二支（day1 ⑤种植与进食）。
> 用户 11-25 实机验收第一支后提三条本支诉求 + 两条相机报障（重拍中反馈），
> 全部本版内兑现。banner 维持 3.52.0（重拍批无版本号语义，逐支单独提交）。

## 用户诉求（11-25，guide_food 专属）

1. **种植阶段要体现**——挖土、拿种子、开垦、种下去（不能一上来就长在地里）
   → 场景手术：16b 原 24 株预种麦全清（journal → air 空田）+ 5 格凸起泥土条
   （(7,54,0) + (8,54,-4..-1)，石 → dirt——**齐平版射线被邻石顶面遮挡否决**）
   + h0 预置 wheat_1×5（麦种 bootstrap：项目无野麦，16a prepared-start 先例）。
   demo 链 = 挖土 5 → E 泥土变耕地 ×5 → 放耕地 5（条带留空 = 结尾「下一代
   耕地已备好」面）→ 田北行（2..6,54,-4）种 5 → 生长 → 收割 → 面包 ×3 → 吃 ×2 → 饱了即收（用户 11-25 第三轮：吃完的 360° 巡视「没必要」= 砍，终态面 = 满饥饿条 + 手里麦/面包 + 田景）。
2. **生长阶段可加速** → 新 demo 面 **`growth_speed`**（dmo world-spec 场景形
   可选 strict int 1..8，absent = 1 位级）：cropGrowthSteps/saplingGrowthSteps
   尾参 paceFactor（ticks = 区间/pace，钳 ≥1；roll 值不变 = 高速档通过帧是
   基础档的确定性超集）。m_demoGrowthPace 会话态零 format，**sim 不 bump**
   （旧 dmo 无字段 = 位级；新 dmo 旧二进制 strict 拒收 = 门内 boot 实证）。
3. **底部按键提示逐 beat 定制**（不用教学默认「W 向前/砍树」）→ 新 demo 事件
   kind **`hint`**（panel_click 16a 先例）：author 独占、strict 字符串 ≤24
   码点 + 字形安全 fail-fast（可打印 ASCII 或 kCjkGlyphs 成员，否则 init
   命名拒收 U+XXXX）、replay 底部中线渲染（教学行同位）、net --script 命名
   拒收（40B 无文本字段）、**表现层不入哈希**（D21 孪生）。replay 期教学
   系统休眠（runDemoReplay 置 m_tutorialEnabled=false，D12 扩展——会话旗
   零 format，demo 永不回写红线不变）。10 条提示全在 471 字形表内（缺字
   措辞规避）。

## 重拍中的相机双修（用户实机 11-25 第二轮反馈）

1. **旋转卡顿**——sweep 初稿 50 步/600-1600f = 每 12-32 帧 12-13px 一阶
   （可见楼梯）。mining 验收口径 = 3 帧一拍 → `sweep_gradual` 默认
   steps = dur//3（显式 steps 保留兼容）。
2. **视野突变/侧边沿着方块**——双根因（门内密拍截图实证）：
   ① **walk_leg 单帧 auto-face snap**：走位前相机一帧硬转到走路方向
   （最多 180°），beat 再转回目标 = 玩家侧身走向目标 + 两连甩头。
   修 = `walk_leg(face=...)`：**平滑预转**（2 rad/s 档）后**面朝目标行走
   路**（键 = 走路方向相对面朝的 w/s/a/d 自动选——平移走位），挖/放/种/收
   全程面朝方块（mining 视觉语言）。face=None = 走路方向（旧语义，现已平滑）。
   ② **look_ground/look_point/place_at/place_ahead 单帧全量瞄准**（19-58°
   瞬间低头/甩头）→ 全部改插值（同 look_gradual 档），place 类 RMB 改在
   瞄准落定 +25f。
   附带结构坑：渐进瞄准改动放置时序后 beat 帧计数落后 DSL cursor → 后续
   beat 堆叠（末格耕地静默没放成，h2 余 1 实证）→ place 类 beat 后
   `f = g._now() + N` 重同步。

## 门 244（全 PASS）

- **3040/3040**（+9：V325Demo 6【hint codec round-trip / 拒收族 7 面 /
  author 时间线 + 拒收 / script 拒收 / inject 零模拟面 + 出参缺失点名 /
  growth_speed 形守卫 8 面】+ Growth pace 3【pace1 位级 / 超集性质 /
  钳 1 除零腰带】）；`-Wall -Wextra -Wpedantic -Werror` 恒 0 警告。
- **author ×2 字节恒等**（growth_speed 入 dmo 后）。
- **S1 ≡ base20 247963 B** ×2 逐字节 + 槽不创建（菜单面冷）。
- **S2 ×2 RAW + savenorm EXACT** + 终态钉：hunger 17/sat 11/hp 20 /
  h0 麦×1 / h2 面包×1 / 网格净 / 田北行空（5 株全收）/ 条带耕地×5 /
  实体 0 / pos (6.58,54,-2.42)。麦粒×10 算术 = 5 株全 wheat_7（未熟株会
  掉 wheat_1 现形）。
- **空闲审计 max 静帧 0f**（--screenshot-every 30 全长 386 拍，像素恒等
  段零——户外太阳恒动）。
- **生长视觉臂**：beat sheet 12 拍（P0 开场→P9 收尾，stage 跳变可见）。
- **canary 4 ×2 新树 RAW + savenorm EXACT**；跨树（tm_v351 v3.51.0）
  savenorm EXACT，仅 example/tutorial_fly 差 `.player.tutorial.id` 一行
  （**教学休眠的预期偏离**：demo 动作不再推进教学步，D12 扩展记档）；
  跨树 RAW 差 = 表现层时代差（第三人称相机 v3.52 后续批 + 教学行休眠 +
  Speed 行）——sim 面全冷。
- **solo 非 demo 跨树 RAW + savenorm EXACT**（教学正常面冷）。
- **性能 A/B 同档**（OW 900f 三窗：new upd med 0.689/0.477/0.484 ·
  0.736/0.679/0.619 · 0.623/0.630/0.629 vs old 0.699/0.680/0.715 ·
  0.620/0.652/0.639 · 0.601/0.635/0.641 median 交错 + render/swap 同档
  14.9）= hint/growth 面 demo-gated 结构零成本。

## 门内捕获（PITS 440 十坑）

① 齐平 dirt 条射线被邻石顶面挡（抬升 1 格解）② 反向走跳残速过冲 0.45m
（转向后 30f settle 解）③ 同排相邻 cross 块遮挡播种射线（南侧站位平行
射线解）④ E 面板 out 每点击只合 1 份（1:1 配方 N 份须 N 轮，网格残留
关面板不退回）⑤ 磁吸 2.5m 是范围非拾取距离（远端拉取 200-700f 蠕动，
per-dig 驻留解）⑥ `$((n+500))` bash 陷阱 ×2（python 变量 n 非 shell
变量 = 跑成 500 帧）⑦ walk_leg 单帧 auto-face snap（face= 平移解）
⑧ 渐进瞄准改时序后 beat 帧落后 cursor（_now() 重同步解）⑨ serialize
漏写 growth_speed（前期全部「pace 6」参照跑实为 pace 1，M pin 整体作废
重钉）⑩ C++ raw string `R"json()"` 定界符与 JSON `})` 撞车（字面量
提前终止，字节级修）。

## 资产 / 纪律

- dmo 9717f（2:37；初版 11590f 含吃完 360° 巡视 1870f，用户第三轮反馈砍除）/ scene 4675 B（gzip pin 手术 v2）/ index length 重钉。
- 逐支提交（验收后单独 commit，永不 `git add -A`，config 不入库）。
- 下一支：⑥ guide_regen（受伤与回血）——重拍口径沿用本支纪律
  （3f/step sweep + face= 走位 + 渐进瞄准 + _now() 同步 + 空闲审计 0 静帧）。
