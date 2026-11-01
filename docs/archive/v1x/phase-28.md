# Phase 28 细节存档

> Phase 30 文档整理：由 `SESSION_CONTEXT.md`「Phase 28 已完成（细节存档）」节、`KNOWN_ISSUES.md`「已修复（归档）」条目 合并而成（未改写原文）。文中路径引用
> （如 `KNOWN_ISSUES.md`、`/tmp/opencode/...` 驱动脚本）指向当时状态；当前文档布局
> 见 `README.md`「文档地图」。

## 阶段报告（原 KNOWN_ISSUES「已修复（归档）」）

- **`[2026-10-06, Phase 28]` 无饱和度 / 进食瞬时 / 冲刺不额外耗饥饿 / 仅 1 种食物 / 掉落单字符串 schema（P23 简化项回收 + 食物扩充）** — Phase 28 回收 P23 的饥饿简化项并扩充食物（用户指定：加羊肉/鸡肉/牛肉等更多食物选择）+ 掉落数据改列表。**数据** 3 个新食物 material 物品 `beef`（restore_hunger 3）/ `mutton`（2）/ `chicken`（2）（MC 生肉值；max_stack 64；图标 = `tools/generate_item_icons.py` 3 个新程序化函数：beef = 深红肉块 + 浅色脂肪大理石纹、mutton = 粉色肉板 + 白色脂肪帽、chicken = 浅色鸡腿 + 骨；salts 81-88，既有图标重跑零变化）/ 5 个 mob 掉落 schema 迁移：`"drops": "item" + "drops_count"` → **列表** `"drops": [{"item","count"}]`（每条目 count 1..64 默认 1、列表顺序 spawn、缺省/`[]` = 不掉落；迁移防护：字符串形式 / 残留 `drops_count` / 非数组 / 非对象条目 / 缺 item / 空 item / 非整数 count / count 越界 = 硬拒绝；zombie [rotten_flesh×1]、skeleton [arrow×1]、cow [leather×1, beef×1]、sheep [wool×1, mutton×1]、chicken [feather×1, chicken×1]）。**代码** `core/food.hpp`（新纯状态机，同 P24 `bow.hpp` 模式：`kEatDurationSeconds=1.6` = MC 32 tick / `kSaturationFactor=2.0` = MC 熟食 ≈2× 经验法则（无逐食物数据字段）/ `EatChannel{start/update(dt,rightHeld)→None/Completed/Cancelled, progress}`；提前松开 = 取消不消耗、进食中再按右键 = 忽略）/ `Player`：`m_saturation` 隐藏缓冲 [0, maxHunger]（新世界 = 0、重生 = 满"一顿饱饭"）+ 4s 衰减 tick（real dt）先扣 1 饱和度、耗尽才掉饥饿条（MC exhaustion 简化：节奏固定 4s，饱和只推迟掉条）+ **疾跑（LeftShift + 移动 + 非水中——与选 `sprintSpeed` 同一输入条件，含贴墙冲刺）tick 节奏 ×2**（`sprintHungerDecayFactor=2.0`，MC 40s→20s 节奏）/ `Game`：`tryEat` = **起通道**（捕获 m_eatSlot/m_eatItem/m_eatStartHealth）、`updateEatChannel`（中断 = 提前松开 / 受击（health < start）/ 死亡 / 指针解锁 / 开面板 / 选中变更（slot 或 item 不匹配）/ 左键挥击——全部取消不消耗；完成 = 从捕获槽扣 1 + restoreHunger + restoreSaturation(2×) + "eat"）/ 手持图标拉取统一弓/进食（heldPull = bow power | channel.progress，共享 64px 几何）/ `MobRegistry`：`MobDrop` + 列表严格解析 / 死亡掉落循环每条目一个 ItemEntity（game.cpp 局部 struct 改名 `PendingDrop` 避免遮蔽 mc::MobDrop）/ init 逐条目交叉校验 / 存档格式 5（`player.saturation`，-1 省略 = 未指定，format 1-4 读档 = 0，负数拒绝）。**测试** 667/667（+17）：`tests/test_food.cpp` 新 6（通道起 / 完成【第 97 帧：96×(1/60) < double 1.6，IEEE 双精度累加边界】/ 松手取消 / stale 取消 / progress / 常量锚定）/ PlayerTest +4（饱和度吸收衰减 / 疾跑 ×2 + 站立对照 / 恢复钳制 / 重生满缓冲）/ ItemRegistryTest +1（3 种肉解析）/ MobRegistryTest +3（列表解析含默认 count / 旧 schema 拒绝 ×3 / 畸形条目拒绝 ×7）+ 5 个既有 drop 断言改列表 / SaveTest +3（format 5 往返 / format 4 读 0 / 负数拒绝）+ 2 个既有（未来版本 5→6 / 写入 format 4→5）。**E2E**（P25/P27 协议 + xdotool 脚本化输入，驱动 /tmp/opencode/p28_e2e.zsh）：**无输入回归全部字节级**——base18 门 ×2 字节不变（**第 4 次结构性免疫验证**：进食通道未激活 / 饱和度 0 → 旧衰减路径 / 疾跑 false / 掉落 schema 不影响无输入场景）、白天 360f mobs-on ×2 与 mobs-off ×2、夜晚（p27far）×2 + nomobs ×2——**5 对全部与 P27 同名输出逐字节一致**（skeleton 射箭场景 health 8 / hunger 18 / sat 0 比特级复现 = P28 生存代码变更对既有场景零扰动）；**eat 机制由存档状态确定性验证**（p28eat 存档：seed 123456789、t=0.25、day_length 480 → t(600f)=0.2708 正午平台、hunger 14、sat 0、slot0 beef×8 选中；脚本 D+1.5s 左键锁 → D+3.0s 右键按住 → D+5.5s 松开，600f）：**5 次运行存档全部一致且精确命中预测** health 20 / hunger 16 / sat 5 / beef×7 / sel 0（4s tick 保证先于进食 14→13【sat 0 → 直接掉条】、进食 D+4.6s 完成 13→16 + sat 0→6、8s tick sat 6→5、12s tick 未到；每 tick 余量 ≥1.6s，对 D∈[0,0.5] 全确定）。**踩坑（新教训，2 条）**：① **SDL 指针锁定 grab delta**——xdotool mousemove（XWarpPointer）只移虚拟光标；进 relative mode 时 SDL 上报一次性 motion delta ≈（warp 位置 − SDL 自算窗口中心）+ 物理鼠标微漂移 → 锁定时首帧相机旋转（0.0025 rad/px：4 px = ±0.01 rad）。实测：warp 位置 1:1 传导（+4 px warp → +4 px 漂移）；同一 warp 位 batch 首次 0 px、其后稳定 4 px（**手静止可复现** = 物理微漂移）；WM 实际窗口原点 (4,64)（windowmove 0 60 不被尊重）→ 中心 (636,416) 而非名义 (644,424)。对策 = warp 实测中心 + 背靠背同 warp + 手静止 + **存档 yaw/pitch 作漂移探测器**（干净 = 0.0000，漂移 = ±0.0100）；**输入驱动运行的全帧字节一致受此环境物理限制，存档状态 = 输入 E2E 确定性权威证据，全帧字节一致用于无输入回归**。② **`std::string::replace(pos, len, const char*, n)` 4 参重载取替换串前 n 字符**（非替换 len 字符）——测试代码误用致 JSON 截断损坏；字符串插入一律用 3 参 (pos, len, string)。

## 会话存档（原 SESSION_CONTEXT）

- **提交** `3771e29`（29 文件，+855/−134）。
- **目标**：P28 候选第 5 条——饥饿后续（P23 简化项回收：饱和度 / 进食通道 /
  冲刺额外耗 / 更多食物）+ 用户指定扩充食物选择（羊肉/鸡肉/牛肉等）；顺带把
  mob 掉落从单字符串升级为数据列表（多掉落 = 食物扩大的数据前提）。
- **数据**：
  - 3 个新食物 material 物品（MC 生肉值）：`beef`（restore_hunger 3，cow 掉落）、
    `mutton`（2，sheep）、`chicken`（2，chicken）；max_stack 64。图标 =
    `tools/generate_item_icons.py` 3 个新程序化函数（beef = 深红肉块 + 浅色脂肪
    大理石纹、mutton = 粉色肉板 + 白色脂肪帽、chicken = 浅色鸡腿 + 骨），
    salts 81-88，既有图标重跑零变化。
  - 5 个 mob JSON 掉落 schema 迁移：`"drops": "item" + "drops_count"` → **列表**
    `"drops": [{"item","count"}]`（每条目 count 1..64 默认 1、列表顺序 spawn、
    缺省/`[]` = 不掉落）：zombie [rotten_flesh×1] / skeleton [arrow×1] /
    cow [leather×1, beef×1] / sheep [wool×1, mutton×1] / chicken [feather×1,
    chicken×1]。迁移防护：字符串形式 drops / 残留 drops_count / 非数组 /
    非对象条目 / 缺 item / 空 item / 非整数 count / count 越界 = 硬拒绝。
- **代码**：
  - `core/food.hpp`（新，纯状态机，同 P24 `bow.hpp` 模式）：
    `kEatDurationSeconds=1.6`（MC 32 tick）、`kSaturationFactor=2.0`（MC 熟食
    ≈2× 经验法则，无逐食物数据字段）、`EatChannel{start/update(dt,rightHeld)
    →None/Completed/Cancelled, progress()}`。语义：提前松开 = 取消（不消耗）；
    进食中再按右键 = 忽略。
  - `Player`：`m_saturation` 隐藏缓冲 [0, maxHunger]（restore/set/getters；
    新世界 = 0，重生 = 满"一顿饱饭"）；4s 衰减 tick（real dt 累加器，玩家死亡
    冻结时不可达）先扣 1 饱和度、耗尽才掉 1 饥饿条（MC exhaustion 简化：衰减
    节奏固定 4s，饱和度只推迟掉条）；疾跑 = LeftShift + 移动意愿 + 非水中（与
    sprintSpeed 选择同一输入条件，含贴墙冲刺）→ tick 节奏 ×2（Params
    `sprintHungerDecayFactor=2.0`，MC 40s→20s 节奏）。
  - `Game`：`tryEat()` 改为**起通道**（捕获 m_eatSlot/m_eatItem/
    m_eatStartHealth）；`updateEatChannel` 在 handleInteraction 末尾；中断 =
    提前松开 / 受击（health < start）/ 死亡 / 指针解锁 / 面板打开 / 选中变更
    （slot 或 item 不匹配——Q 丢弃经 slot 检查覆盖）/ 左键挥击（显式）——
    全部取消不消耗；完成 = 从捕获槽扣 1 + restoreHunger + restoreSaturation(2×)
    + "eat" 音效。手持图标拉取统一：heldPull = bow power | channel.progress
    （共享 kBowDrawPullPx=64px 几何）。
  - `MobRegistry`：`MobDrop` 结构 + 列表严格解析；`Game` 死亡掉落循环每条目
    一个 ItemEntity（列表顺序；局部 struct 改名 `PendingDrop` 避免遮蔽
    mc::MobDrop）；init 对每条目交叉校验（未知物品 / count > maxStack 失败）。
  - 存档格式 5：`player.saturation`（-1 省略 = 未指定；format 1-4 读档 = 0；
    负数拒绝）。
- **测试** 667/667（+17）：`tests/test_food.cpp` 新 6（通道起 / 完成【第 97
  帧：96×(1/60) 的 IEEE double 累加和略短于 double 1.6】/ 松手取消 / stale
  取消 / progress 0→1 / 常量锚定）；PlayerTest +4（饱和度吸收衰减、疾跑 ×2 +
  站立对照【wallX(9,1,4,8,10) 贴墙冲刺】/ 饱和度恢复钳制 / 重生满缓冲）；
  ItemRegistryTest +1（ParsesMobMeatFoods）；MobRegistryTest +3（ParsesDropsList
  含默认 count / RejectsLegacyDropsSchema ×3 / RejectsMalformedDropsEntries ×7）
  + 5 个既有 drop 断言改列表；SaveTest +3（format 5 往返 / format 4 读 0 /
  负饱和度拒绝）+ 2 个既有（未来版本 5→6 / 写入 format 4→5）。
- **E2E**（P25/P27 协议 + xdotool 脚本化输入；驱动 /tmp/opencode/p28_e2e.zsh，
  12 拍批 /tmp/opencode/p28_e2e.log）：
  - **无输入回归全部字节级**：base18 门 ×2 字节不变（**第 4 次结构性免疫
    验证**：进食通道未激活 / 饱和度 0 → 走旧衰减路径 / 疾跑 false / 掉落
    schema 不影响无输入场景）；白天 360f mobs-on ×2 与 mobs-off ×2、夜晚
    （p27far）×2 + nomobs ×2——**5 对全部与 P27 同名输出逐字节一致**
    （skeleton 射箭场景 health 8 / hunger 18 / sat 0 比特级复现、nightc
    health 20、FRESH-FLASH 天门 a≈0.240 = P28 生存代码变更对既有场景零扰动）。
  - **eat 机制由存档状态确定性验证**：p28eat 存档（seed 123456789、t=0.25、
    day_length 480 → t(600f)=0.2708 正午平台、hunger 14、sat 0、slot0
    beef×8 选中 0）+ 脚本（窗口发现锚点 D：D+1.5s 左键锁 → D+3.0s 右键按住
    → D+5.5s 松开，600f）。**5 次运行（首批 4 次 + 最终背靠背对 1 次）存档
    全部一致且精确命中预测**：health 20 / hunger 16 / sat 5 / beef×7 / sel 0 /
    time 0.2708。tick-margin 分析：4s tick 保证先于进食（sat 0 → 直接掉条
    14→13）、进食 D+4.6s 完成（13→16，sat 0→6）、8s tick（sat 6→5）、
    12s tick 未达；每 tick 余量 ≥1.6s，对任意 D∈[0,0.5] 全确定。
  - **eat 全帧 ×2 字节一致受阻于环境**（SDL 指针锁定 grab delta，见坑位合集）：
    最终背靠背对（同 warp 到 SDL 实测中心 (636,416)、手静止）eat_a 干净
    （save yaw 0.0000 / pitch −0.2500）vs eat_b 稳定 4 px 漂移（yaw −0.0100 /
    pitch −0.2600）——**非代码非确定性**（存档 5 次比特一致 + 无输入回归全
    字节级）；输入驱动 E2E 全帧字节一致受指针锁定 grab 物理限制，存档状态
    （含 yaw/pitch）= 输入 E2E 的确定性权威证据。
- **新踩坑（2 条）**：① SDL 指针锁定 grab delta（见坑位合集）②
  `std::string::replace` 4 参重载截断（见坑位合集）。
- `config/config.json` 仍为用户手改未提交（view_distance 80 / day_length
  600），与 P28 无关。
