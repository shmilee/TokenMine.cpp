# Phase CRAFT-3 — 材质音效（v1.0 任务表 25/32，门 106）

> per-block/per-item 材质音色 + 3D 空间化（距离衰减 + 声像）。
> 消费 KNOWN_ISSUES 两条："音效是固定事件集，无 per-block/per-item 差异" +
> "无 3D 空间化（单声道 2D 播放）"。CRAFT 行 3/3 收尾。

## 实现内容

1. **材质数据**（`block.hpp` + `block_registry.cpp`）：块 JSON 可选
   `properties.sound_material`（严格字符串；显式空串拒 = ammo 纪律；
   非字符串点名）
2. **init 交叉校验**（新文件 `src/block/block_sound.{hpp,cpp}` 纯函数
   `checkSoundMaterials`）：引用材质 ⇒ `<material>_dig/_place/_break.wav`
   3 件必须**文件级**存在（headless 音频降级时 init 照样严格；点名缺失
   件 = registry 序首个失败块 + 事件序 dig/place/break）
3. **SoundBank stereo 化**（`sound.{hpp,cpp}`）：输出流 mono → stereo
   （44100 不变）；旧 `play(name)` = mono 样本复制双声道居中（听感位级
   等价）；新 `play(name, std::optional<Vec3> origin)` = 空间化
   `setListener(pos, yaw)`（listener = 相机，Game 每交互帧同步，1 帧
   陈旧 = 零成本零非确定性，注释记档）
4. **空间化纯函数**（单测，无 SDL）：
   - `distanceGain(dist, cutoff) = clamp(1 − dist/cutoff, 0, 1)`（线性，
     全 3D 距离含高度，cutoff = `kSoundCutoffBlocks = 16.0f` 命名常量，
     零 config）
   - `panGains(origin, listenerPos, yaw) = ((1−t)/2, (1+t)/2)`（t =
     水平方向右分量，Camera::right 约定；高度不参与、前后居中、自身
     位置无除零；gL+gR=1 恒定增益）
   - `toStereo(pcm, gL, gR)`（交错展开 + 16-bit 钳制）
5. **事件名选择**（`blockSoundName(block, event)` 纯函数）：无
   `sound_material` = 通用名（dig/place/break）**位级不变**；有 =
   `<material>_<event>`
6. **6 调用点位空间化**（game.cpp）：dig tick（被破块材质@格心）/
   破坏完成（@dropPos）/ 放置（所放块材质@新格心）/ 潜行旋转（被旋块
   材质@格心）/ 丢弃（pickup@脚位）/ 物品拾取（pickup@物品位）
7. **内容**：`generate_sounds.py` 扩展 18 个确定性新 wav（6 材质 × 3
   事件；固定种子 LCG/sine 范式；**既有 17 wav 字节零变化** md5 实证 +
   重跑幂等）+ 32 块赋值（stone 8【含矿脉族 MC 语义】/ wood 3 / gravel
   1 / sand 3【sandstone 系】/ cloth 15【wool 全染变】/ glass 2）

**零 format（19）/ 零 config / 音频不入帧状态**（硬规则 8 既有性质）。

## 变更文件

- `src/core/sound.{hpp,cpp}`（stereo + 空间化 API + 3 纯函数）
- `src/block/block.hpp`（`soundMaterial` 字段）/ `block_registry.cpp`
  （严格解析）
- `src/block/block_sound.{hpp,cpp}`（新，`blockSoundName` +
  `checkSoundMaterials`）
- `src/core/game.cpp`（init 校验 + setListener + 6 调用点）
- `tools/generate_sounds.py`（18 新生成函数 + 重跑纪律 docstring）
- `assets/sounds/`（18 新 wav）+ `assets/data/blocks/×32`（加字段，
  surgical 行插入格式零扰动）
- `tests/test_sound.cpp`（+8：衰减/声像/stereo/降级 no-op 钉死）/
  `test_block_sound.cpp`（新 +8：事件名/校验点名/registry 序）/
  `test_block.cpp`（+2：解析 + 32 块内容钉死精确集）
- `CMakeLists.txt`

## 测试（1567/1567，+18 新）

`-Werror` 0 警告；生成器确定性 = 35 wav 重跑 md5 不变 + 既有 17 与
实现前基线逐一相等；18 新件 RIFF mono/16-bit/44100 结构合法。

## 门 106

- **S1 PASS = 第 106 次结构性免疫**（fresh 60f ×2 RAW + vs base18 字节
  一致，免重锚；论证 = 渲染路径零触碰【音频流声道数 + 块 JSON 数据字段
  + 18 音频资产，无 atlas/UV/mesh/光照变化】+ 无输入零音效事件
  【dig/place/break/pickup 全输入驱动、step 需移动、fresh 60f 无掉落
  无 mob】+ 音频不入帧状态）
- **S2**：材质音色/空间化无头不可听 ⇒ 机制层 18 单测 + S1 免疫 + 套件
  为门主体；**实际听感 = 用户实机**（记档）
- **S3** = 套件 1567 PASS
- **S6 init 失败 ×2 PASS**（headless 实证）：stone.json → "stonex" =
  `block 'stone' sound_material 'stonex' is missing 'sounds/stonex_dig.wav'`
  点名；删 stone_break.wav = `block 'coal_ore' sound_material 'stone'
  is missing 'sounds/stone_break.wav'`（registry 序首败块 + 事件序首缺
  件）。坑：首轮 S6b 在**陈旧 scratch 资产**上跑（32 块 JSON 无字段）=
  无报错假象 → 同步 scratch 后重跑有效（PITS 255-258 漏同步坑第 5 次）
- 性能 A/B v0.22.0（发布前必做，10-26 04:26）：fall 900f 三窗
  **fluidCells 123879/167781/236573 双侧逐位一致（同世界）+ upd/render/
  swap/ents/stream 全同档**（音频纯事件驱动 = 无输入零音效事件 = 结构性
  惰性实证）= 成本可忽略

## 偏离 / 记档

- **物品侧无 sound_material**（YAGNI）——pickup 音色恒通用
- 无 per-material 体积/音高参数（MC 有，v1 不做，零 config）
- 未赋值块（下轮候选）：obsidian/bedrock/glowstone/furnace/
  crafting_table/fence/torch/campfire 族/iron_block/stone_slab/
  stone_stairs×4/dirt/grass/作物族/leaves 等
- pan 仅横向（高度不参与、前后居中 = 横向声像固有歧义 v1 简化）
- 手持物品破碎（工具/弓/点火器）+ 箭拾取 + 营火放取保持 2D
  （origin = 玩家等价增益 1.0 居中，省成本）
- mono 设备：SDL 自动下混 (L+R)/2 = 2D 路径听感等价
- 既有 cloth.wav（染色/护甲事件音）与 cloth_* 材质音并存（不同文件名）
