# 锈蚀车间：准备、交互与任务语音

选定工厂关卡立即播放任务简介，所有客户端各自播放本地 2D 无线电。按 Y 只切换自己的准备状态；全员就绪才倒计时。倒计时至少 5 秒，并等待正在播放的简介结束。取消准备只取消倒计时；房主取消部署或重新选图会停止／替换简介。队伍成员变化会清空准备状态。

进图沿用加载确认、无人机镜头与举枪屏障，全部完成后开始任务并播放 Opening。指挥官与任务汇报按服务器时间排队，各客户端本地播放，不从场景内某个公共声源发声。迟到的客户端从当前语音进度接入，不重播已结束语音。

任务内空按 F 使用现有交互输入与服务器警告逻辑，并继续向 AI 发送警告噪声。每名玩家分别随机选择六种警告，避免连续重复，冷却也分别计算。警告声音跟随说话者，附近队友可以听见；它不占用指挥官的无线电队列。看着不可用的交互物时不会误触发警告。

## 工厂内的交互

在 `/Game/Maps/Lvl_Factory_Whitebox` 的 `Gameplay/FactoryMission` 文件夹中调整。全部使用现有网格，Scale=(1,1,1)，不修改原有墙体、门和道具位置。

| Actor | 位置／规则 | 完成语音 |
|---|---|---|
| Task_Ledger_L1/L2/L3 | 办公区三张现有办公桌；每回合服务器随机保留一本 | VO_TM_Rpt_LedgerFound |
| Task_Hidden_Shipping | 装卸区 FD_102 顶层托盘上的夹板 | VO_CMD_Hidden_Shipping |
| Task_Hidden_Crates | 锅炉房东北侧弹药箱 | VO_TM_Hidden_Crates |
| Task_Hidden_Breaker | 地下配电室总闸，约 (5700,-4540,-270) | VO_CMD_Hidden_Breaker |
| Task_Extraction_X1 | 厂区外 (2000,800)，有 EXTRACT / X1 标记 | VO_TM_Rpt_RoomClear |

携带账本、所有存活队员在撤离点 3 米内，才可按 F 撤离。出货单、弹药箱与总闸为可选隐藏目标。成功撤离且从未被发现、未拉响警报时，结算加入“无声行动”并播放 VO_CMD_Hidden_Silent。

总闸通过 `Factory.SwitchablePower` 标签控制普通室内灯光及对应灯具的显隐；地下应急灯和保留的外部灯不随之关闭。完成状态和断电效果由服务器同步，重复按 F 不会重复计分。

拘捕嫌疑人、解救人质和击倒嫌疑人分别连接 TargetSecured、CivilianSecured 和 SuspectDown。BenchSecured、SuspectIncap 已导入，供后续对应玩法使用，目前不会假触发。

## 修改内容

任务标题、地点和简介：`Config/DefaultGame.ini` 中的 `[FPS.MissionUI]` 工厂字段。平板和任务选择共用这些内容，目标进度继续读取原有 MissionFlow。

语音资产：`/Game/FPS/Audio/Voice/Factory`。原 WAV 保存在 `ArtSource/FactoryVoice`；SC_MissionRadio 与 SC_OperatorVoice 可分别调节无线电和玩家喊话音量。替换不同长度的录音时，同步修改 `MissionVoiceTypes.cpp` 的时长表，保证同步、排队和倒计时正确。

`Scripts/SetupFactoryMissionVoice.py` 用于重新导入与摆放本次任务对象；不要重跑旧工厂白盒生成器。改动前的地图备份在 `Saved/FactoryVoice/Before`。

## 验证记录

构建、自动测试与双客户端 PIE 记录存放在 `Saved/FactoryVoice`。最终验收结果见 `Validation.json`；自动测试覆盖音频资产与时长、独立警告随机状态、准备屏障、任务计分、目标流程和加载屏障。

本轮结果：Development Editor 编译成功，5/5 自动测试通过，23/23 验证项通过。双客户端验证了选图即播、独立 Y 就绪、往返切图、Opening、本地 2D 无线电、独立玩家喊话、四类 F 任务与全队撤离。最终摆放的 7/7 交互目标均通过 Visibility 球扫检查。未进行打包构建或真实双机音频延迟测量。
