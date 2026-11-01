# Phase UX-9a — 云方块化 + 厚度（v1.48.0，门 168 重锚）

> 消费用户两轮实机反馈：①「天空云朵的形状有了，但是看着像纸片，
> 不是方块组成的？」（= PRE-4 既有记档偏离「纯色平板无云团形状」被
> 看穿）②「云层检查后，我还是看不出云层厚度！我需要云层也是方块
> 组成的」（颗粒仍不足 = 要可见厚度）。**UX-9 体验细化（二轮实机
> 报障）行首项**。门内性能实证驱动一次架构翻转（mesh → fragment
> shader），四笔提交收口。

## 根因

`cloudMask(cx,cz,t)` 按 16 块 region 格整格 50%（splitmix64 顶位）→
每朵"云" = 一块 16×16 平滑大平板（纸片感）；且单层平面 = 零厚度。

## 演进链（四笔提交）

1. **139ebc2 两级颗粒 mesh**：region 层零改动 ∧ 新 `cloudSubMask`
   （4×4 个 4 块 sub-cell，世界 4 块格网锚定，专用盐 kSaltCloudSub
   0x5B8C92E7，同 t 步，75% = splitmix %4<3）+ buildCloudMesh 逐
   sub-cell 4×4 quad（窗口 3582/最坏 9216 quads）+ 共享 emitQuad
   提取（cloudCellVertices 位级不变）。覆盖度 37.5% ∈ 30-45%。
   2060/2060（+6）。
2. **d20e406 Mesh::upload 原地 subdata**（门内性能修复 #1）：走位
   探针实证每跨 16 块云格重建 = VBO 全量重分配上传 **139ms**
   （v1.47 slab 4.4ms）→ upload 加复用路径（同模式 ∧ 容量内 →
   glBufferSubData，grow-only 容量 m_vboBytes/m_iboBytes，GL 错误
   destroy 回落重试一次）。调用点 16 处自查零语义变化。
3. **b00193a 高度场 CPU oracle**（门内性能修复 #2 的基座）：
   `cloudSubHeight(wx,wz,t)` = region∧sub on 时 1 + splitmix64(
   hash3(·, kSaltCloudSubHeight 0x9C2E4B71) + t·golden) % 3 均匀
   {1,2,3}（均值 2 块厚，独立盐不追踪）+ 5 单测（分布 exact
   33111/32873/33291=99275 / 16 位置×3 档 48 值去相关 / 1536
   双向 on/off 等价）。mesh 厚度方案（底/顶/侧 quad）实证前撤销。
4. **031001d 云 shader 化（架构翻转）**：3× 预分配对照实验（强制
   subdata 路径 135ms 未改善）= **buffer 更新本身 ~140ms/MB
   （门机 iGPU Mesa BDW GT2），分配非瓶颈** → 静态窗口替代 = 云
   相对地标游移 6.25% 步行速可见 = 质量不可接受 → **fragment
   逐像素 mask**：
   - `assets/shaders/cloud.{vert,frag}`（独立 program，basic/sky/ui
     位级红线零触碰；sky/ui 加载范式）
   - **GLSL 330 core 无 64 位整数**（项目上下文 3.3 钉死）→ 64 位
     以 **uvec2 (lo,hi) 对模拟**（mulHi 16 位半分解，uint32 逐
     limb mod 2^32 回绕 = 位精确）
   - 逐式移植 cloudMask ∧ cloudSubMask（常量 limb 拆分逐 hex 核对）
     + 像素世界坐标选 sub 格（×0.25 幂二精确）+ floorDivI
   - 底面恒白（仰视主视角亮度 = mesh 版一致）+ **厚度 = 4 向边界
     带**（off 像素 + 邻 on + 相机在壁外侧 → MC 侧壁着色 ±X 0.8 /
     ±Z 0.6）——真 3D 投影带 h/tan(仰角) 的常数近似（带宽恒 1 sub
     格 4 块；带渲染在底面平面非真壁高；顶面/峰顶俯视 = 底面图案
     同款近似 = 4 条记档）
   - 几何 = init 一次性 **16384² quad**（~240B 全运行期唯一上传）+
     per-frame model 平移锚定 4 块 sub 格网（`4·floorDiv(floor(eye),4)
     − 8192`）= 世界锚定零游移 + **零窗缘 pop-in**（旧 384 块窗口
     硬边消失，云延伸地平线 0.38° = 附带视觉改善）
   - cloud.{hpp,cpp} mesh 面整体移除（净 −175 行：buildCloudMesh/
     cloudCellVertices/cloudSubCellVertices/emitQuad/
     cloudMeshNeedsRebuild/kCloudWindowCells）；纯场四函数（mask/
     sub/height/timestep）+ 三盐保留 = shader 的 **CPU oracle**；
     缓存键 4 成员退役（game.hpp）
   - 顺修：fixer 会话 UX-9a/9b 标签混标 18 处统一；上笔上报的
     O(n²) reserve（buildCloudMesh 逐 quad reserve 零余量，60k 顶点
     ~10s 实测）随 mesh 面移除消失

## 位级交叉核对（三层）

- C++ 镜像模拟 vs 真 uint64：hash 链 25,600 + 谓词 81,920 评估
  （±64 坐标 × t0..4 含负）**0 失配**（首跑揪出转写漏 sm64
  finalizer 一处 = 核对价值实证）
- 真驱动（Mesa 26.2.5 BDW GT2 3.3 core）：编译链接 OK + **131,072
  像素 E2E 0 失配**（隐藏窗 + FBO + 正交顶视双 pass 跨正负区/
  t0-t1：on 255 / wall 204 / wall 153 / off discard 逐像素 exact）
  + 白 quad 管线控制 65536/65536
- 门 S1 像素终裁（下节）38,153/38,153 云一致

## 测试

2061/2061（2054 基线 + 6 颗粒 + 5 高度 − 4 mesh 面退役）、
`-Werror` 0 警告、banner v1.48.0。保留面 = cloudMask 全钉 /
cloudSubMask 确定性 + 覆盖度 exact 8268+99275 + 16 位置去相关 /
cloudSubHeight 确定性 + 分布 + 去相关 + on/off 等价 / cloudTimeStep /
render.clouds config 语义（off = 零 draw 免疫根）。

## 门 168（编排者执行，**重锚门** = gate 111/112 先例；old 侧 =
v1.47.0 = 9dba9f0 = tm_v148_base）

**S1 = 第 168 次门 + base18 锚刷新**（最终架构，fresh 60f 无输入
4 侧 ×2 矩阵，quiet config seed 钉）：

- 每侧 ×2 RAW 全字节一致（t ≡ 0 + 相机恒位 → qx/qz 恒 = 静态）
- **no（new-off）≡ oo（old-off）167097 B ×2 = 代码级免疫**（云关
  路径跨版逐位不变，off = 首行 return 零 draw 零 program bind）
- **o（old-on）≡ base18.pre-ux9a.png 168140 B**（old 侧连续性 =
  原锚未被漂移污染）
- **savenorm 四方 EXACT**（云不入世界状态）
- **n vs o 像素 diff = 38,153 px，bbox y[0..207] 全限地平线（y219）
  以上天空区，地形零泄漏**（深度测试正常，quad 不画进山体）；
  x[6..678] 全宽 = 云延伸地平线的附带改善（旧窗口硬边 y~70 消失）
- **像素终裁 38,153/38,153 云一致**：每个 diff 像素 new 侧 ∈ {raw
  sky / 0.55 白混合 / 0.8 壁混合 / 0.6 壁混合}（sky 自 old 侧或
  深解恢复，±1 容差）= 零杂散像素，壁厚着色值精确
- **base18 锚刷新** → n1（179116 B，md5 9 位唯一 n1≡n2）；旧锚
  base18.pre-ux9a.png 存（168140 B v1.47 slab 云）

**S2 等价 = 走位探针（翻转最强实证）**：velocity/走位 1200f
W 走位（两侧同路径终位 (0.5,52,−18.7) 逐值同）：
- **跨格窗口 render max：v1.47 = 4.4ms（slab mesh）/ v1.48 颗粒
  mesh 期 = 142.9ms → 最终 shader 版 = 2.5ms**（57× 于 mesh 期，
  低于 old slab 基线 = 每跨格零上传实证；首帧 6.2ms = quad 240B
  上传 + shader 首次编译，一次性）
- 其余窗口 2.5-2.7ms 同档（无跨格亦无异常帧）
- 期间实证链（门内性能修复过程）：139ms 跨格（走位探针）→
  subdata 修复 142ms（未改善）→ 3× 预分配强制 subdata 135ms
  （= 上传本身 ~140ms/MB）→ 架构翻转 → 2.5ms

**S3** = 2061/2061。**无 S6**（零 parse 面——纯函数 + shader +
命名常量，无新数据字段/config 键；render.clouds 既有键语义不变）。

**性能 A/B v1.47.0 vs v1.48.0（发布前硬门槛）PASS**：fall 900f
无输入——render med 0.429/0.440/0.436 vs 0.457/0.479/0.464（+0.03
ms = 逐像素云 ALU，帧预算 <0.2%）+ 首窗 max 21.1 vs 8.8 ms（新版
首帧反而更快 = 240B quad vs 76KB slab 上传）+ upd/swap 全同档；
PIPE3-S0 changesApplied **9/3/1 三窗逐位** + **终档 fluids 18/18
+ blocks 55/55 集合恒等 diff 0 = 同世界同成本**。

## 偏离/记档

- 两级 mask + 高度场仍非 MC 噪声场（MC = 2D 噪声阈值有机团簇；
  本项目 = 16 格域 ∧ 4 格颗粒 ∧ 1-3 块厚 = 块状 + 厚度，大尺度
  团簇形状矩阵外）
- 厚度边界带 = 常数近似（4 条：带宽恒 4 块非 h/tan(仰角) / 带在
  底面平面非真壁高 / 只渲相机侧近壁（真 3D 远壁 backface 省）/
  峰顶俯视顶面 = 底面图案）
- **GLSL 330 无 64 位整数** → uvec2 对模拟（每 hash 链 ~6 次 mulHi
  = spec 估算 ALU ×2.5；纯天空早 discard 结构不变，实测 render
  med +0.03 ms 可忽略）
- K_SALT_H（高度盐）保留具名常量但 fragment 未消费（边界带近似
  不用高度值）= 未来 true-3D 移植的常数集对齐 + CPU oracle 在测
- 云延伸地平线（16384 quad 边沿仰角 0.19° 亚像素）= 旧 384 块窗口
  硬边消失（视觉改善，diff 带扩大 70→207 px 的根因）
- **门机驱动观察**：Mesa 26.2.5 i965 特定诊断序（两个 inline
  trivial program 后第三次 create 夹 buffer/VAO 操作）驱动内
  segfault = 疑驱动 bug；真实 init 四连建（basic→sky→ui→cloud）
  全门跑（S1 八跑 + 探针 + 性能）零复现 = 不影响交付
- Mesh::upload subdata 复用路径（d20e406）保留 = 通用加固（本树
  无每帧重上传调用方，grow-only 不变式文档化；云 = 原唯一受益者
  已 shader 化）
- 首轮门脚本 blend 断言两次勘误（① 检查错侧 0/77 ② 翻转后 diff
  双向非包含关系 = 改四假设分类 38,153/38,153）= 门协议演进记档
