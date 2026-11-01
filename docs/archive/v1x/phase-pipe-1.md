# Phase PIPE-1 — worker 网格化（v1.0 任务表 第 2/29 项）

> 2026-10-21（夜）。来源 = `docs/BACKLOG.md` PIPE 行第 1 项：「mesher 计算搬
> worker、主线程只 GL 上传；快照式设计（光照 region 与 mesh 读竞争边界 =
> unlit 列 3×3 邻域）；价值待 STRM-1 后重测（99.x 残余 #3）」。

## 价值重测（step-0 插桩实测，先于实现定案）

vd80 静止填充跑（16000f，场景列 (2,0)，scratch day_length 3600 免夜）：
- **填充期主线程 remesh（预算 8 chunk/帧 + 列合并 GL 上传）= 7.3-11.3 ms/帧**
  （frame 3600/5400/7200 窗均值；脏集峰值 **45,895 chunk**）
- **complete 后拖尾 = ~2.8 min**（frame ~7570→14400 排空；frame 12600 窗仍
  2.57 ms/帧）——99.2 预算 8/帧 的固有代价，文档未记录过，step-0 实测确认
- 翻转触发器（<2 ms/帧则砍）未触发 → 按 BACKLOG 规则定案实现

## 实现内容

- **MeshJob**（game.cpp，与 StreamJob/LightJob 同 deque 范式）：job 私有
  **临时 World 快照**（目标列 3×3 邻域 9 列 × 8 chunk = 72 chunk memcpy，
  `static_assert(std::is_trivially_copyable_v<Chunk>)` 钉死——Chunk = 5
  std::array + bool，无 map/vector）→ worker 用**零改动的 ChunkMesher**
  出 8 chunk 几何（void 语义与活 World 构造上逐位一致 = 同一个 World 类）。
- **`World::insertSnapshotChunk`**（world.cpp，新增）：纯 emplace、无
  scanFluidChunks/dirty 副作用（现有 insertChunk 的活 World 簿记对一次性
  快照是纯浪费）——快照索引永不被消费。
- **选择规则 = 纯函数 `nextMeshColumns`**（新文件 `world/mesh_job.{hpp,cpp}`）：
  脏列投影 ∩ 非 unlit（99.4 lit pop-in 承接）∩ 距当帧 light-pending 列
  Chebyshev ≥2（`kMeshLightExclusionColumns = 1`，= kLightUpdateRadius 16 块
  的命名化——光 footprint E±1 列会改 C 的光，快照必陈旧）∩ 非在途，
  `columnDistanceSq`（STRM-1 助手）最近优先，cap `kMaxMeshBacklog = 2`/帧
  （独立 deque，不挤占 kMaxStreamBacklog=8 的 gen 判定）。
- **post 顺序不变量**：streamChunks（gen，只写 scratch）→ postMeshJobs
  （同步快照拷贝 + post）→ postLightJobs（窗口内唯一活 World 写者，最后
  post）。快照拷贝时唯一在跑的 worker 是 gen（不碰 World）= **零竞态窗口**
  （不是"小窗口"）。写进 thread_pool.hpp 契约注释 + 就地注释 + PITS。
- **integrateMeshJobs**（flush 后主线程窗口）：move 8 输出进
  m_chunkMeshData → 集成时 `clearChunkDirty` ×8 → 列标 m_columnRebuildPending
  → 出在途集。**顺序 = 先于 integrateStreamedColumns**（否则 stream insert
  的邻居标脏被 clear-dirty 吞掉 = 永久幽灵面；PITS 有录）。
- **updateChunkMeshes 瘦身**：删预算 mesher 循环（kRemeshBudgetPerFrame 删除），
  只留列合并 GL rebuild 循环（GL 上下文只在主线程，~0.3-0.4 ms/列 = 不可搬走
  部分）。**启动演示区网格化**自动走 worker（2 列/帧，帧 ≤10 完成）。
- **卸载保护**：m_meshPending 列入 protectedColumns（2 行，m_streamPending
  同款）。
- **帧时序**（文档化偏离）：新列 pop-in render N+1→N+2（帧锁定）；已 mesh 列
  的邻列 stream 进来 = 最坏 +2 帧收敛（边界面临时 void 烘焙，光足迹排除 →
  重 mesh，dirty 自愈，玩家 2 帧内进不了新地形 = 实际不可见）。

## 变更文件

- 新增：`src/world/mesh_job.{hpp,cpp}`（nextMeshColumns 纯函数 + 排除规则）、
  `tests/test_mesh_job.cpp`（5 TEST + 2 static_assert）
- `CMakeLists.txt`（mc_core + mesh_job.cpp；tests + test_mesh_job.cpp）
- `src/world/world.{hpp,cpp}`（insertSnapshotChunk + getStairFacing 注释修正
  【void/uncreated 读 255 哨兵非 0——既有注释与实现不符，fix-1 发现】）
- `src/core/game.{hpp,cpp}`（MeshJob/deque/在途集 + post/integrate + 卸载保护
  + updateChunkMeshes 瘦身 + 管线注释）
- `src/core/thread_pool.hpp`（契约注释：post 顺序 gen→mesh→light + flush
  时序推导）
- `docs/`（KNOWN_ISSUES remesh 条重写 + vd80 残余消解 / PITS +2 / ROADMAP /
  BACKLOG 勾选 / AGENTS 1205 + 门 82 / 本存档）

## 测试

1205/1205（1200 基线 + 5 新，无禁用/删除），`-Werror` 0 警告：
1. `SnapshotMeshMatchesLiveWorldMesh`（72-chunk fixture：流体双种类/玻璃透明
   pass/stair 朝向 ×3/光梯度/跨 chunk+列边界，8 层逐字节 + 远列 (3,0) 反证
   3×3 读集充分性）
2. `SnapshotVoidMatchesEmptyWorld`（6 读面 × 6 探针：界外列/y<0/y=128/y=192；
   钉住 created-unset stairFacing = 255）
3. `NextMeshColumnsNearestFirst`（(sq,x,z) 全序 + cap 2/0 + 空集）
4. `NextMeshColumnsExclusions`（unlit / 光足迹 Cheb-1 双排除【(0,2)∧(1,3)】/
   Cheb-2 放行 / 在途排除）
5. `IntegrationClearsDirtyAndEditRequeues`（清脏 8 / 在途不重排 / 编辑再标脏
   重入队）
6. static_assert Chunk / MeshVertex trivially copyable
（卸载保护 + GL 集成半 = Game 级不可单测——按 m_streamPending 同款范式落地，
E2E 填充期在途列不消失 = A/B 论证覆盖）

## 构建与验证结果（E2E）

- **S1 base18 门第 82 次**：fresh 60f ×2【新】+ ×2【3749e24 基线 worktree
  构建】+ **A/B 四对截图全字节一致** + 红心 0 + skygate (120,181,242) 严格。
  论证措辞变更（用户夜授权接受）："无输入零 job" → "无输入 worker 活动有界于
  启动 16 mesh job（2 列/帧、帧 ≤10 完成、帧锁定）+ 流送永不 engage + 其后
  零活动"；帧 60 终图像 = 同一纯函数（mesher 零改动）对同一世界状态的输出。
- **S2/S3 填充 A/B（vd16 scratch 797 列，day_length 3600）**：填充墙钟
  8.1 s vs 8.0 s（gen/post-rate bound，**无回归**——ora-1 预测兑现）；
  4000f 存档（time/age 剥离后）**跨 build 字节一致**（世界状态确定性保持）；
  CPU 尾窗 18.1% vs 20.0%（vd16 小环 = 收益稀释，headline 看 step-0b）。
- **step-0b（vd80，插桩 build，与 step-0 同场景同配置）**：主线程 mesh-path
  **填充期 0.27-0.42 ms/帧**（step-0：7.3-11.3 ms/帧 = **−96%**）；**拖尾
  ~830 帧归零**（step-0：~6800 帧 = 2.8 min，**8× 缩短**）；残留 0.0002 ms/帧
  = 纯 GL 上传簿记。队列时序证明兑现：填充帧 flush 不增长（mesh 藏于 light
  之下，worker FIFO）。
- **S4 pop-in 回归**（场景列 (64,0)，3600f ×2，P54 条款）：存档字节一致 +
  notblack 0 + skygate 严格；截图差 3229 px = 流送边界 pop-in（墙钟相关，
  条款内）。99.4"网格只出现在光终值上"在新管线保持（unlit 排除规则单测 +
  零黑像素实证）。
- **S5 编辑收敛**（放置+破坏 ×2 截图）：夜间点击被 away 模式偷（STRM-1 环境
  发现）→ **推迟晨间**（机制由单测 5 + S4 的 dirty 自愈路径覆盖）。
- 插桩（step-0/step-0b）提交前全删（grep PIPE1 零命中），干净重建 0 警告。

## 已知问题 / 偏离

- 流送 fps 改善**有限**（35 → ~37-42 预估）：fps 下限 = 光照 flush 13-16 ms
  （LGT-1 union-region 的杠杆），PIPE-1 定位 = 负载摊平 + 拖尾消除 + 主线程
  封顶（ora-1 重测结论，验收口径按此）。
- pop-in +1 帧（N+2）与最坏 +2 帧收敛 = 帧锁定、不可感知、已文档化。
- 启动帧 1-10 部分可见序列与现状不同（现状逐 chunk 预算 vs 新逐列 worker）
  ——现行协议只断言终帧，安全（边界已写进 KNOWN_ISSUES）。
- GL 上传残留 ~0.3-0.4 ms/列 不可搬走（GL 上下文主线程）——流送 fps 上限
  ~45-52 的另一原因（与光照 flush 并列）。

## 下一步建议

- 晨间补：S5 编辑收敛输入跑（点击需用户在桌前 = away 模式休眠）。
- **PIPE-2 存档增量**（BACKLOG 第 3 项，设计案已就绪）：退出 O(环大小) →
  O(会话编辑数)；vd80 全环 legacy 退出实测 **6.2 s**（step-0b run 提取：
  frame limit → World saved，102,824 chunk / 303+1226 编辑）= 基线数字。
