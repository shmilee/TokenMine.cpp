# LGT-4 光束入光（v1.4.0，门 119/119b）

v1.x 任务矩阵第四行（docs/BACKLOG.md）。出处 = v1.0 砍掉清单回填：信标光
束（P83/91）= 全项目最后一个恒白 unlit 消费者（顶点 sky 0/block 1.0 →
light 恒 1.0：夜间不暗、近萤石不亮）。"光束入光" = 光束**接收**世界光
照。

## 定案：复用 PRE-6 per-draw 光照覆盖范式
- 纯函数 `beamLightSample(world, x, y, z)`（`world/beam_light.{hpp,cpp}`
  新文件，与 beacon 纯函数同族）= **信标块上方空气格 (x, y+1, z)** 的
  getSkyLight/getBlockLight ÷ kMaxLight 归一（= beam 模型最低格：
  drawBeaconBeams 把柱体放在块顶面 b.y+1 起；信标块自身格不透明恒读
  (0,0)——首版实现采样块格 = 门 119 E2E 揪出的缺陷，见下；无 dayFactor
  参 = LGT-3 仿射撤退经 frag 自动继承；World getter 回退覆盖
  void/世界下方）。
- `drawBeaconBeams` 每活跃信标 draw：bind → 采样 → set
  `u_useLightOverride=1` + sky/block → draw → bind → **复位 0**（PRE-6/
  UXFIX-1 跨 draw 残留纪律；Renderer::draw 后 unbind = 复位须重 bind）。
- **u_drop* 三 uniform 改名中性**（u_useLightOverride/u_lightOverrideSky/
  u_lightOverrideBlock，basic.vert + drop 两调用点 + 注释；纯改名零语义
  = 既有 8 drop 测全绿自证；grep 零残留）。
- 顶点烘焙值（sky 0/block 1.0）保留 = override 关闭时的恒白回退；几何不
  重烘。
- 语义：beam 亮度 = max(effSky(df), block)（frag LGT-3 公式）→ 深夜露天
  月光 7/15（非恒白）、近萤石/火把亮化、白天露天 = 1.0（与旧恒白同值 =
  df=1 恒等对照根）。2 world 读/活跃信标/帧（drop 同款可忽略）。
- 零 format/config/内容；帧锁定（光场 + dayFactor 皆帧状态，硬规则 8）。

## 实现（fixer 交付，编排者逐 hunk 核证毕 + 门 119 内修复 1 处）
6 文件 + 2 新文件（beam_light 纯函数 + 单测）+ CMakeLists 2 行。规格偏离
2 处核证接受：Game 级 GL uniform 序列单测不可断言（规格自带 fallback =
纯函数 + E2E ×2 RAW 兜底，已记档测试文件头）；纯函数落点 world/ 非
entity/（文件语义）。
**门 119 E2E 内修复**：首版 beamLightSample 采样信标块自身格——
opaque 格恒 (0,0) → beam 全环境 ambient floor ≈ 不可见（LGT-4 视觉
失效）。排查链（全部实证）：night A/B diff=0 → GPU uniform readback
（sky=0/block=0/use=1/bound=1 = 覆盖已喂到 shader）→ 同二进制
beacon.beam on/off 夜帧 diff=0 → **day 场景 on/off 双二进制（new/old）
diff 均 = 0** = 场景相机根本没取景到 beam（pitch 0.15 视锥
−26°..+44°，beam 柱在 58°..87° 仰角、96 块高自 y48 起 = 在框外）→
旧场景的 day"恒等"与 night"零 diff"双双失明。修复 = 采样点 (x,y+1,z)
（beam 最低空气格，语义正确：露天白天 (15,0)→1.0 恒白 / 深夜
(15,0)→7/15 月光 / 近萤石 (0,15)→1.0）+ 新场景 pitch 1.25 取景 beam
（门 119b）。

## 测试（1711/1711 = 1704 + 7 LGT-4，-Werror 0 警告）
既有 1704 零改动全绿（rename 零行为 + 光照/流体/spawn 回归网）+ 7 新：
OpenSkyFull (1,0) / GlowstoneFullBlock (0,1) / FullDark (0,0) /
**OwnCellOpaqueAlwaysIgnored**（块格 15/15 恒不读 = 本次缺陷回归钉死）/
ExactNormalized 7/15·3/15 / WorldFallbacks void+下方 /
DayIndependentAndPure。

## E2E 门 119/119b
- **S1 PASS = 第 119 次结构性免疫免重锚**：fresh 60f 四方（NEW v1.4.0×2 /
  OLD v1.3.0×2）RAW 387726 B 字节一致 + base18 锚（9c64b0f8）连续 +
  savenorm EXACT（无信标 = m_beaconActive 空 = 零 beam draw 结构惰性）。
- **S2a 信标场景（首版相机 pitch 0.15，beam 失明 = 记档废弃）**：
  存档 oracle 全 EXACT（day/night savenorm；world 状态两侧全同）；像素
  A/B 无效（beam 不在框内，on/off 双二进制 diff=0 实证）——坑 = PITS 287
  （beam 门必须先 on/off sanity 验取景）。
  - 既有 real-dt 类浮现（非回归）：night 场景 P90 湖焦土 rim 燃尽格
    (31,35,20,'air') 在**同 OLD 二进制两跑间自分歧**（fps 决定燃尽是否
    落于 600f 内 = PITS 244/283 家族块足迹面）；NEW 与 OLD 之一变体
    完全一致。
- **S2b 门 119b PASS（修复后：采样 (x,y+1,z) + 场景 pitch 1.25 取景
  beam 柱 58°..87°；玩家静止位 y38；mobs off；600f 无输入）**：
  - **sanity**：新二进制 day beam on/off diff = **1646 px**（场景有效取景；
    首版场景同款断言 = 0 px 双二进制失明实证）；
  - **day df=1 恒等 RAW**：new ≡ old 全帧字节一致（128226 B）——采样
    (15,0)→L=max(1.0,0)=1.0 = 旧恒白，**beam 在框内的非平凡恒等**
    （首版场景该断言因取景失明无效）；
  - **night ×2 RAW/侧**：old 125615 B ×2 / new 125612 B ×2 字节一致；
  - **night beam 列仅暗化**：diff = **1646 px 全限竖直窄带
    x[674..687]（w=14）y[222..436]（h=215）**，带内 **1646/1646 px
    new<old 三通道、零亮化**（old beam 带均 (66.0,104.8,116.3) →
    new (33.2,52.9,62.8)，比值 0.48-0.50 = 解析值
    (0.03+0.97×7/15)/1.0 ≈ 0.483 含 ambient 底吻合）；**带外 0 px
    diff** = 世界其余含 LGT-3 两侧逐像素同；
  - **savenorm EXACT**（night：世界状态两侧全同；day 同）。
- **S1 有效性**：S1 于修复前二进制跑成（fresh 60f 四方 + base18）——
  fresh 无信标 = m_beaconActive 空 = 零 beam draw，修复只动 beam draw
  采样点 = S1 结构免疫论证不受影响，免重跑。
- **S3**：1711/1711。**S6**：无（零解析面）。
- **性能 A/B（收）**：（fall 900f 全指标同档 + fluidCells 逐位 = 无信标
  结构惰性的帧成本零；信标场景 render 段 2 read/beacon/帧 ≈ 0）

## 偏离/记档
- 光束采样点 = **信标块上方空气格 (x,y+1,z)**（beam 最低格；首版块格 =
  缺陷已修）。MC 光束不发光 = 本项目"入光"语义为接收世界光，非 MC 对标
  项，矩阵回填项按字面定案。
- beam 柱 96 块高（base 64 + 32/级）穿顶出世界（kWorldHeight 128 之上
  = 天空区，P83 既有几何未动）。
- (31,35,20) 燃尽格 run 分歧 = 既有 real-dt 类（PITS 244/283 家族），
  位级 oracle 协议 = 匹配变体比对或剥离该格（记档）。

## 坑位（PITS 追加）
- PITS 286：黑帧/平面窗判别必须用 PNG 字节尺寸（22106 B 均色，mean 探针
  假亮）；长墙钟跑 savenorm 扩展剥离 player.hunger +
  effects[].remaining（real-dt 类）。
- PITS 287：beam 类像素门必须先跑同二进制 beacon.beam on/off sanity
  （diff>0 = 取景有效）——首版场景 pitch 0.15 对 96 块高、近天顶的 beam
  柱完全失明（day"恒等"+night"零 diff"双假 PASS），双二进制 on/off 均
  0 px 才定位。
