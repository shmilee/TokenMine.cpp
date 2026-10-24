# Phase ENT-1 存档（mob 四肢摆动）

> v1.0 任务表 12/29（BACKLOG ENT 行第一项，行序推进）。commit `8d60b74`，
> 发布 v0.10.0。消费 99.x 残余 #4（「摆腿/摆臂动画未做」）。

## 定案

最小完整版 = **per-part 摆腿动画，纯渲染层**（AI/物理/存档零改动）。
humanoid（zombie/skeleton）99.7 几何无独立 arm part（仅头+躯干两 box）→
**不造几何、记档**（摆臂需先重做模型 = 独立内容 phase）。

## 实现

- **swing 相位 = per-mob 整数**（`MobEntity::m_swingPhase`，0..N-1；
  MobEntity 只持此整数 = 渲染隔离规则）：
  - 「移动」= 帧边界水平位移严格不等式（`update()` 顶部捕获 prevX/prevZ，
    AI+物理之后比较；**不读 AI 意图/速度目标** = 帧锁定确定）
  - 移动中 `phase = (phase + kSwingStep=2) % 64`（整周期 32 帧 = 0.533 s）
  - 静止 `phase = max(0, phase − kSwingDecay=4)`（16 帧收步到恒等姿势）
  - spawn = 0；**从不入档**（cosmetic session-local；单测钉死存档无 swing
    表面 + player 键白名单拒 `swing_phase`）
- **constexpr 16.16 定点正弦表**（64 条，编译期 14 项 Taylor，余数界
  x²⁹/29! ≤ 9.2e-9 ≪ 半量子 7.6e-6；**零运行期 libm** = std::sin 跨平台
  位级不稳定）
- **数据驱动 mob JSON `swings` 段**（box 可选 `name` + 条目
  `{part, amplitude (0,1] 弧度, group 0/1}`；model/box/swing **三级未知键
  拒绝**；part 存在性**解析期**交叉校验【文件内引用比 grows_to 更早可验】；
  重名/重复认领点名）
- **CPU 逐帧部件顶点变换**：pivot = box 顶面中心（纯函数，shipped 全部
  pivot 落 body box 内 = 关节不可见）；绕 mob 局部 X 轴（yaw 前）；limb 角
  θ = amplitude×表值（16.16 定点）；sinθ/cosθ = 整数 Taylor（sin 6 项/
  cos 5 项，|θ|≤1 rad ≤2 ulp，每部件 64 组预计算）；**法线同旋**（faceShade
  桶跟随，几何必需）；**phase 0 位级恒等** = 静态 fast path（结构性免疫）
- **per-mob 动态 VBO**（`Mesh::uploadDynamic` GL_DYNAMIC_DRAW +
  `updateVertices` glBufferSubData；首摆帧分配、死亡拆除释放、指针键稳定）
- **config `mobs.limb_swing`**（默认 true；false = v0.9.0 静态 = E2E 对照组）
- 内容：chicken 2 腿 0.5 rad 半周相位 / cow·sheep 4 腿 0.7 rad **对角步态**
  （FL+RR g0、FR+RL g1 = MC 四足）/ spider 8 腿 0.9 rad 偶奇分组；翅不摆
  【MC 只飞行时扇翅】

## 变更文件（8d60b74，19 文件 +1328/−9）

- `src/entity/mob_swing.{hpp,cpp}`（新，322 行）：纯定点数学 + SwingModel/
  SwingFrame 构建
- `src/entity/mob_registry.{hpp,cpp}`（+160）：name/swings 严格解析
- `src/entity/mob_entity.{hpp,cpp}`（+30）：相位推进
- `src/renderer/mesh.{hpp,cpp}`（+95）：动态 VBO
- `src/core/game.{hpp,cpp}`（+78）：drawMobs 门 + swingMobMesh + 拆除释放
- `assets/data/mobs/{chicken,cow,sheep,spider}.json`：swings 内容
- `tests/test_mob.cpp`（+574）/ `tests/test_save.cpp`（+22）
- `docs/BACKLOG.md` / `docs/KNOWN_ISSUES.md`（fixer 提交内）

## 测试

1367/1367（+17：正弦表 6 项钉死 / 相位推进回绕衰减 / 门矩阵 / pivot 公式 /
零角位级恒等 / 已知角度 Taylor / 部件钉死 / phase0==静态 / 只动摆动部件 /
半周对跖全相位交叉 / JSON 解析 / 11 例错误表 / 真实 assets 内容钉死 /
追逐推进 / 停步衰减 / 无数据惰性 / 存档无 swing 表面）、`-Werror` 0 警告。

## E2E（门 92，2026-10-25 晨）

- **A base 门**：spawn 60f ×2 对 gate88 锚**字节一致**（结构性免疫：基线帧
  无 mob = first_spawn_frame 60 + phase 0 恒等 fast path 绝对惰性）
- **正式对（白天 240f，ent2 草原场景 + dense 生成环 4-6 块，vd80）**：
  - swing on ×2 **RAW 字节一致** / swing off ×2 RAW 字节一致（确定性）
  - on vs off 像素 diff = **11,773 px（world 11,749 / sky 3 = AA 边 /
    hud 24 = 饥饿饱和度 real-dt）**= 摆腿可见且隔离
  - on vs off **savenorm EXACT**（动画纯渲染层：模拟/存档零改动）
  - 玩家 health 20（无战斗干扰）
- **夜间支撑对**：N120 on/off diff = **8,060 px（sky 0 / hud 0 = 完美隔离**，
  spider 接近中）；N240 diff = 11,773 px（与白天同值 = 被动 mob 位置帧锁定
  跨昼夜同种子，spider 该帧在视野外）
- **S6 init 失败**：spider `swings[8]` 指 `leg_9` → rc=1 + 点名
  `references unknown part 'leg_9'`（解析期交叉校验实证）
- **性能（插桩 900f dense mob 场景，发布前测试）**：render med 0.46-0.56 ms
  （max 2-6 ms = 流送 GL 上传，非 swing）/ ents（mob AI）10-22 ms/300f /
  update med/p95/max 47.6/82/150 ms（流送期）= **动画成本可忽略**
- **夜坑（PITS +2）**：① scratch config 生成 python 语法错（关键字参数
  `mobs.limb_swing=False` 非法）→ 文件未建 → 游戏 `--config` 不存在时
  **静默种子 defaultConfigJson**（vd8/无 mobs 段）→ "on/off 对照"实为
  on/on → diff 恒 0 连错三轮（判别 = 流送 197 列 vs 12853 + 日志
  "Config not found"）；② zsh `[[ == "*(...)*" ]]` 引号 RHS = 字面串不
  glob → 尺寸检查恒 false（整轮误判重试）。教训 = config 回读 assert +
  种子哨兵 grep + off 侧先验证真的 off。

## 偏离

- zombie/skeleton 无 arm part = 不摆臂（内容范围定案）
- limb 角 sin/cos 整数 Taylor 而非相位角直接查表（amplitude 任意值的
  数学必然；16.16 同体系）
- 摆幅不随速度缩放（移动中恒 +2/帧 = 文档化简化，MC 按速度）
- 相位不入档（cosmetic session-local，读档 = 0）
- 生成参数（mob_spawning.json）shipped 值未动（check 30/max 4/ring 4-6/
  despawn 40）；E2E 用 scratch 加密版（S6 先例：主二进制 + scratch cwd）
