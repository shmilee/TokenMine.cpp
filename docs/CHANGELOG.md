# 版本历史（CHANGELOG）

> 本文件只留 **v2.11.0 起** 逐版明细（2.x 第二批 = 原 3.x 八项并入，
> 11-03 用户定案；V2-11~V2-18，原 4.x「世界扩展」= 下一大版本 v4.x）。
> v2.x 第一批（v2.10.0→v2.0.0）→ `docs/archive/v2x/CHANGELOG.md`；
> v1.x（v1.50.0→v1.0）→ `docs/archive/v1x/CHANGELOG.md`；
> v0.x → `docs/archive/v0x/CHANGELOG.md`。逐版细节（机制/证据/
> 偏离/性能 A/B）见对应时代 `docs/archive/*/phase-*.md` 阶段存档。
> README 只保留里程碑版本（v0.1.0 首个公开版 / v1.0 大版本 /
> v2.10.0 第一批收官 tag / v2.18.0 二批收官 tag）。

## v2.12.0（2026-11-04）— V2-11 XP 系统（经验球 + 等级曲线 + 三发放源 + format 26）（门 183 全 PASS）

第二批 SURV 行首项（实现源 = fixer 43 文件半成品 11-03 中断取消 →
git stash → 11-04 pop 零冲突恢复至 v2.11.0 之上 + 编排者修 3 处 WIP
编译破损 + 1 处 save.cpp 潜在 bug）：
**XP 曲线**（core/xp_curve 全整数，MC 现代公式 11-03 wiki 核定）——
needed 2L+7/5L−38/9L−158 + total L²+6L/(5L²−81L+720)/2/
(9L²−325L+4440)/2 + 逆 = 整数二分（往返构造性恒等，无浮点）；玩家
单整数 pool（player.xp 恒写），**等级永不存储** = 派生单源真值。
**经验球**（entity/exp_orb）——值 1..9 无合并无分裂【记档偏离】+
ItemEntity 同款物理纪律 + V2-6 磁力同款帧锁定拉取（2.5）+ 1.0 内
pickup pass 收 → pool + "pickup" 音色。
**三发放源**（数据驱动严格解析 [0,9] 点名拒）——mob 击杀
（hostile 5 / passive 2 = MC 1-3 中值）/ 熔炼完成（item smelt_xp
**默认 1 统一**【MC 分数表 = 记档偏离】）/ 挖掘完成（block mine_xp：
coal_ore 1 / diamond_ore 5 / iron_ore 0 / gold_ore 0 = 现代 MC 1.17
raw 化值）。
**format 26**——player.xp 恒写 + entities 第二 kind type∈{item,xp}
三向键纪律（V2-3 同款：pre-26 载迁 0 / 25 携 xp 点名拒 / 26 缺键
点名拒）。**save.cpp 潜在 bug 修复（PITS 361 违例第二实例）**：
tutorial 白名单 isV25 = fmt >= kSaveFormat → 字面量 >= 25（bump 后
format-25 文件误拒 series；UX-8f 1667 行同款先例注释佐证）。
**HUD**——热栏下 XP 条 + 等级数（全整数数学，恒可见 = 门 183 唯一
像素源；零新字形）+ F3 xp 行。2249/2249（+42：xp_curve / exp_orb /
SaveFormat26Test 族）、banner v2.12.0。门 183：S1 **base18 第 4 次
重锚论证**（旧侧 ≡ 现锚 178676 B 环境干净 / 新侧 ×2 恒等 / diff
2031 px 全落底部 XP 条带 bbox (30,714)-(647,740) 零泄漏 / savenorm
仅 {format, player.xp} 预期键 → **新锚 179027 B**）+ S2 三发放源
（熔炼无输入 xp=1 / 击杀 scratch spider 短按住 + W 步行拾取 xp=5
【orb 距静止玩家 2.54 > 磁力 2.5 新坑】/ 挖掘 xp=1；旧侧全 format 25
无 xp 键形状互斥【场景 py25 降级 = PITS 377】）+ S3 + S6 四项点名
+ **性能 A/B 同世界同成本**（changesApplied 9/1/3 逐位 + 终档
16/16 36/36 恒等 + 熔炼场景球全生命周期同档不可测）。PITS 377（旧侧
新格式场景 init 拒 / orb 磁力 3D 边界）。详见
docs/archive/phase-2.12.0.md。

## v2.11.0（2026-11-03）— V2-19 视角/移动/时间显示批（滚轮缩放 + 玩家飞行 + 24h HUD 时钟）（门 182 全 PASS）

用户 11-03 实机诉求三项（UX 行续编号 V2-19，**提前执行 = 版本
v2.11.0**——任务号与版本号解耦，V2-1/V2-2 同款先例）。全会话态
= **零 format / 零 config 新键 / 零新数据**：
**① 第三视角滚轮缩放**——SDL 滚轮事件入 Input 帧态（无输入恒 0
免疫根）+ session distance clamp 2.0-8.0 step 0.5（默认
kThirdPersonDistance 4.0 = **resolveThirdPersonCamera 默认参数
位级不变**，既有 16 单测零改动）+ 仅 Third 模式 + ThirdFront
aimReach = distance + kReach 联动（MC 前视约定泛化；**瞄准射线仍
自眼位 UX-8g 零触碰**）+ F3 `cam <Mode> D x.x` 读法；MC 原版无
滚轮缩放 = mod 惯例记档。
**② 玩家飞行（"中国人会飞"）**——双击 Space 0.3 s real-dt 窗
（CRAFT-7 先例纯函数 core/fly，第二击吞跳跃，仅 locked + 存活）；
飞行 = 重力跳过 + Space 升 / Shift 降 4.0 b/s（潜行机制照常）+
水平 ×1.6（6.88 b/s > 冲刺 5.6 = 快于走路诉求）+ 碰撞照常 +
**坠落累加器飞行中每帧归零**（切出帧自切换高度计坠无爆发伤害）
+ 水/熔岩自动切出（记档）+ 不存档（记档，省 format）+ 死亡重生
复位。
**③ 24 小时 HUD 时钟**——纯函数 `DayCycle::clockString`：
`hour24 = fmod((t − kDawnStart 0.05)×24 + 6, 24)` **与 ramp 边精确
对齐**（晨 06:00 / 午 12:00 @ t0.3 / 昏 18:00 / 夜 00:00 @ t0.8；
ulp 钉 clockString(0.0) = "04:47"）+ renderUi 右上角恒可见
（MC 无 HUD 时钟 = 记档新功能）+ ':' 字形 ASCII 行在列实证（零
字体扩展）。2207/2207（+26）、banner v2.11.0。门 182：S1
**base18 第 3 次重锚论证**（旧侧 ≡ 现锚 178148 B 环境干净 / 新侧
×2 恒等 / 新旧 diff 609 px 全落右上角时钟区 bbox (544,12)-
(657,29) 零泄漏 / savenorm EXACT → **新锚 178676 B 采用**）+ S2
三判别（zoom：new 视差全框 diff vs old wheel 无动作 None = 互斥 /
flight：new 终档 y 62.13 升空 vs old y 54.00 跳跃落地 = 互斥 /
clock = S1 区）+ S3 + S6（命名单测钉族）+ **性能 A/B 同世界同
成本**（changesApplied 9/1/3 逐位 + 终档 16/16 36/36 集合恒等 +
PERF-WIN 全指标同档——时钟 +1 文本 draw 成本不可测）。门协议
发现 PITS 376（输入 E2E：capture 重试环【DEBUG 级 captured 假象】
/ 帧限收尾禁 SIGTERM【截图 + kill 超时壳双坑】/ pgrep 轮询假
PASS 源）。详见 docs/archive/phase-2.11.0.md。

## v2.x 第二批（V2-11~V2-18，进行中）
（V2-12 附魔起，逐版条目入此。）

