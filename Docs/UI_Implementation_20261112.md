# UI 与画面实施记录（进行中）

执行口径：`UI_Handoff_20261112.md`；美术补充：`UI/07_UI改版美术任务书.md`。本轮只做 UI；关卡任务另轮执行。

用户最新范围：这一轮只完成 UI 和画面。后续打包与关卡摆放暂停，先收齐编辑器内的最终画面和截图验收。

## 备份与边界

- 开工前完整备份：`Saved/CodeBackup_20261111_210340_UIRound1`，含源码、配置、原 UI 资源和地图。
- 地图开工基线 SHA256：`743c36361dcf8f0714d7d49094c78ca2bd5c1da3aa895b8f10ab0213f4c22a49`。后续编辑器保存后的当前文件为 `7a11bee059642ed9ac33007cccac69a05b04cfce6a6e8df30e9ed0db96b8d866`；保留当前文件，不能再宣称与开工基线完全相同。当前地图另存快照在 `Saved/UIMapSnapshot_20261112_031940`，最终包围盒检查将重新导出。
- 不回退已有墙体、外围、室内密度，以及已验收的门、楼梯、掩体。工作区原有源码修改和暂存状态保留。

## 1. R 与弹匣队列：已通过本机运行测试

- 原队列已符合 FIFO：换弹取队首，半弹匣回队尾，空弹匣丢弃。
- 短按松开才换弹；长按沿用可调 `ReloadHoldThreshold=0.35` 检视；R 不再使用双击弃弹手势；Canceled 只清理手势。
- 真实 Enhanced Input 注入测试：7 发半弹匣保留在队尾；长按阈值前不检视、阈值后检视；松开长按不换弹；动画正常结束；空弹匣使数量减一。
- 证据：`Saved/UI_ReloadGestureQA.json`，六项断言全部通过。双人真实 Enhanced Input 注入回归在靶场完成，`Saved/UIRound1QA/Multiplayer_Reload.json` 七项检查通过：两端短按松开才换弹、7 发半弹匣回队尾、阈值前不检视/阈值后检视、松开长按不换弹、检视正常结束、取消触发不换弹、快速双击保留 13 发半弹匣。

## 2. 局内 HUD：已编译并完成初步画面检查

- 设备四项横排，无底板和常驻按键提示；事件驱动更新，切换亮起 300ms。
- 检视时显示 FIFO 水位、当前精确弹量与开火模式；150ms 淡入，动画结束后保留 0.5s 再淡出。
- 常驻弹药/AP、伤情纸人已隐藏；伤情接口与看表代码保留，看表输入解绑。
- 交互改为准星下方单动作和非金色键帽；名牌和任务提示采用新字体颜色，位置保留。
- 低弹量提醒设置项默认关闭，仅用于检视时的文字提醒；常驻 HUD 不显示弹药数字。
- 字体：Barlow Condensed + Source Han Sans SC，来源/许可证在 `Content/FPS/UI/Fonts`。五个线性图标是可替换占位，SVG/PNG/UE 资产及来源清单在 `Content/FPS/UI/Icons`。
- 完整编译成功：`Saved/UI_FinalEditor_BuildLog.txt`。最终低弹量警告仅改变填充颜色，描边和数字保留主文字色；深色 A/B 填充也遵守警告规则。
- 截图与运行数据：`Saved/UIRound1QA`。已覆盖原生 NVG、实际手电照墙，以及相机淡黑形成的受控全黑对比底。后者只验证 HUD 对比度，不代表工厂照明验收。
- 浅/深填充各保留三种底截图。浅色目前默认，深色通过 `fps.UI.MagDark 1` 比较；尚待最终全屏分辨率复验。
- 设置项存在、两档、默认关的验证：`Saved/UIRound1QA/Settings.json`。

## 3. 加载叠加层：已通过初步自验

- 切图黑底、任务名、2px 真实进度线；未知进度保持，不按时间爬升，无百分比数字。
- 侦察信息框与原选择/结算界面分离：仅地图、任务简报、等待队友 x/x，无进度线。
- 编译成功：`Saved/UI_Loading_BuildLog2.txt`。
- 未知异步进度的起始、6.2s 和 6.9s 截图：`Saved/UIRound1QA/Loading_*.png`；保持原进度，状态尾部变化。
- 真实切入工厂后的等待框：`Saved/UIRound1QA/Recon_Waiting.png`；仅地图、简报、等待 x/x，旧 HUD 隐藏。
- MoviePlayer 仅承担引擎阻塞切图期间的纯 Slate 黑底 UI，没有视频路径和无人机 CG。无人机走世界内 Level Sequence。

## 4. 实时镜头：单人与双人运行已通过

- 资产：`/Game/FPS/UI/Sequences/LS_Factory_Recon_V3`，v3 十个循环点，60fps / 13.5s，首尾姿态相同，零端点切线。
- 循环、当前机位到 WP11、1.5s 俯冲、0.3s 噪点黑帧、现有装备举枪、所有客户端举枪完成后服务器开战的逻辑已实现并完整编译成功。
- 局内与镜头共用 `NightVisionProfile` 和原 `M_NVG_Phosphor`；等待时暂停电池消耗。开战前压住敌人及携带武器的可见性。
- 真实静态网格世界包围盒 2210 份，20cm 镜头半径；6113 个样本及连续线段/包围盒、返回轨迹三角面/包围盒 SAT 检查全部通过，覆盖循环任意位置开始返回。结果：`Saved/UIRound1QA/Drone_BoundsQA.json`。
- v3 无需微调 XY；以上包围盒结果属于开工基线，当前地图复验待更新，不能混用哈希。
- 原地图没有可用 PlayerStart，导致原点碰撞、无法出生和就绪。GameMode 仅在工厂无有效部署出生点时补充四个 RF_Transient 出生点；胶囊范围已核对，不写入地图。
- 单人 `Drone_Runtime_22s.json`：等待 22 秒完整循环，路径最大偏差 0.147cm；返回约 1.85s、俯冲约 1.52s、断信号约 0.31s、既有 Raise 约 1.98s；举枪完成前输入锁定，之后回到自己的角色。
- 双人 `Multiplayer_ClientLate.json` 和 `Multiplayer_HostLate.json`：分别让客户端、房主晚到；早到者保持循环，等待人数 1/2→2/2。双方返回/俯冲/断信号/举枪/开战流程完成，举枪期间输入锁定，最终各自回到自己的 Pawn 并解锁；复制敌人及其所有附属武器开战前隐藏、开战后可见。
- 修复客户端重启角色时输入锁被引擎重置、序列停止时恢复旧视角，以及 StopPIE 清理时访问失效 CameraManager 的问题。
- 双人测试后恢复原单人 PIE、窗口尺寸、后台节流和临时渲染参数；人数/延迟 QA CVar 恢复默认值。

## 后续必做

1. 当前保存地图已重新导出：2201 个 Actor、2112 个真实网格包围盒。6113 个采样及连续线段/返回轨迹 SAT 检查通过，零碰撞，SHA `7a11bee059642ed9ac33007cccac69a05b04cfce6a6e8df30e9ed0db96b8d866`。相对初始快照的差异保留在 `MapBoundsDelta.json`，未回滚当前地图。
2. 完成 1080p UI 最终画面检查、警告态及夜视/手电/受控黑底截图，将说明和截图集中到 `Docs/UI`。
3. Development/Shipping 独立包完整流程，按用户最新指示暂不继续；不计为当前 UI 画面完成条件。

## 打包自验入口

- 构建脚本：`Saved/ui_package.ps1 -Configuration Development` 或 `Shipping`。单线程 UBT、单 Cook 进程，只指定靶场和工厂地图，输出 `Saved/UIRound1Packages/<Configuration>`。
- 显式传入 `-UIQA -UIQATravel -UIReadyDelay=18` 时，从默认靶场走实际 `TravelSquadTo` 切到工厂；保留一整圈侦察等待后走正常准备屏障与既有 Raise。普通启动不启用此入口。
- QA 写出切图 Begin/End、无人机各阶段和 Gameplay 的 JSONL，并截图 Orbit/Raise/Gameplay；Shipping 不依赖控制台或 UE_LOG。
- 构建成功不等于运行验收；下面仅在实际运行后补齐结果。

当前打包状态（暂停）：Development 本地构建已成功；启动后未获得游戏窗口或完整流程证据，不能计为运行通过。Shipping 未开始。未发布、未上传。

首轮 Cook 暴露的旧引用已做最小兼容修复：可选 PSR 原始动画包缺失时不再执行失败的 CDO 硬加载；Coop 默认蓝图延后至 InitGame 解析并保留软引用供 Cook 发现；旧主菜单仅清理七个已删除页面/按钮的失效图节点，设计树未改；旧 HUD 编译后保存清除已删除伤势变量 GUID。备份在 `Saved/UI_PackageRepairBackup_20261112_034935`。

未完成项不视为通过，整轮尚未交付。
