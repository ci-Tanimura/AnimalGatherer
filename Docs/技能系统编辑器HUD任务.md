# 编辑器接入第二批：现有主 HUD 的技能展示

项目 C:/unreal/AnimalGatherer；用户授权既定计划及自动执行，休息中。先读 AGENTS.md、CLAUDE.md、Docs/技能系统实施资料.md 第11、19、20节和下级规范，Docs/技能系统编辑器只读检查.md。核心参数和第二版竖排布局已确认，本批只有 HUD 资产实现，禁止改源码、输入、GameMode、关卡和动物。

资料事实：主关卡 /Game/Maps/LV_MainGame.LV_MainGame:PersistentLevel.LV_MainGame.EventGraph 在 BeginPlay 已创建唯一 WBP_MainGame；Mode.HUDWidgetClass=None 必须保持。当前 HUD 有 CanvasPanel_20、Text_Countdown、Txt_Timer、Img_P1score、Txt_P1_Score、Img_P2score、Txt_P2_Score。比分位于两侧 x=125/-125、y=200。现有 Construct 有倒计时绑定，保持原有演出，不能额外重复绑定或播放声音。新的 C++ 接口在 Source/AnimalGatherer/Public/Tanimura/MyGameHUDWidget.h，状态快照在 Public/GameTypes.h；已有原生定时器刷新UI，无需新UI权威计时。

允许修改：Content/UI/WBP_MainGame；允许新建 Content/UI/Skill/ 下 WBP_SkillCard、必要的面板、M_UI_SkillCooldown 等。允许核对并按UI用途设置两张现有 Content/Image/skill-*-v3 纹理（TextureGroup UI、合适RGBA/Alpha、Nearest、NoMipmaps）；不移动/复制/覆盖源图。不修改现有其他Widget、关卡或全局配置。备份在 Saved/SkillEditorBackup/20261008/DiskBaseline，修改前保存本批相关已存在资产，结束保存并回读确认。

特别工具约束：先describe得到schema再call_tool，tool_name使用不含toolset前缀的短名！ typed refPath必须完整对象路径，如 /Game/UI/WBP_MainGame.WBP_MainGame。Codex已保存 Docs/MCP-Schema-UMG.json、Blueprint.json、Object.json、Asset.json、Batch.json 与 MCP-DSL-Docs.json，可读取避免重复巨大工具描述。使用批处理必须先 get_execution_environment，并遵守工具编排沙盒。

BlueprintTools.write_graph_dsl 的实现会删除其认为未被执行根连接的旧节点！严禁对原有 EventGraph 或已有用户函数使用它。保留全部现有节点和连接，新增事件通过 add_event/create_node/connect_pins接线；复杂逻辑放新增独立函数图表，在全新图表可用DSL。不要重写原Construct、Countdown、Score、Timer图表。新图表可迭代修复，仅处理本批生成的逻辑。

目标与表现：
1. 一张可复用 WBP_SkillCard，四个实例（P1反向/P1加速/P2反向/P2加速），各自状态独立；禁止从OwningPlayer自动判断两侧玩家。保持3:4卡图，建议140x187，上方36x24 LB/RB徽章，下方/右下x2徽章。画面1920x1080，比分下方两侧竖排，反向上、加速下；左列围绕x125右列围绕-125，起点y280左右，卡间留空。只调新增UI尺寸/锚点，保留地图和相机。
2. 新卡片状态函数接收 FSkillSlotSnapshot（需要时 SlotIndex/按键提示）；读取Definition.DisplayTexture，RemainingUses，CooldownRemaining/Duration，bCanUse。初始次数x2，Ready阶段稍暗无CD；可用正常亮度；有余次且CD>0时暗色遮罩、向上取整秒数与径向进度环；0次数时x0/已用完、无CD环；Ended不可用。不能自行扣次数、计算独立权威CD或初始化技能。UI动态材质必须每实例独立MID，不能共享改变参数影响其他卡。
3. 遮罩只覆盖插画区域，保留烧录名称、LB/RB和次数清楚。源图1086x1448；反向名称面板约从y1170开始，加速约y1100开始，遮罩高度分开配置。原图不翻转、不重复叠加名称。
4. WBP_MainGame 实现 UpdateSkillState(PlayerId, Snapshots)：合法ID0/1明确分流；数组长度检查后更新对应两卡。SetSkillPanelsVisible只控制新增区，不隐藏原比分/倒计时。
5. UpdateSharedSpeedEffect(Snapshot)：中央比赛计时下方独立 SPEED UP 1.5x + 剩余时间（可1位小数）+条，RemainingDuration>0显示，无效/结束Collapse。其进度按已确认3秒归一化，不能把5秒技能CD混为一条。
6. ShowSkillUseFailure(PlayerId, SlotIndex, Result)：所属玩家卡/区域短暂英文反馈（NO TARGET、COOLDOWN、NO USES等），不能遮挡名字，不扣次数、不排队。按玩家各自0.8秒清理显示timer，持续失败时刷新；该timer仅显示用。成功可按RemainingUses下降检测给卡片短暂亮边/轻量反馈，初始快照与重建不误判成功。避免音效重复播放。
7. 新显示Widget不抢焦点、不改变InputMode，用HitTestInvisible/不Focusable。已有倒计时逻辑保留，原生 UpdateCountdownText 可不新增重复演出；报告原因。

验收：编译新Widget和主HUD（不能忽略error）；回读WidgetTree确保原七个Widget和旧图表保留，四张卡、LB/RB、状态事件、共享条引用已接通；确认MID独立；所有本批资产保存is_dirty=false。不启动PIE，本批结束后由Codex独立验收并安排实机。返回路径、实际检查结果、任何未验证项，中文报告不引用日文注释/卡图文字。若工具限制，报告具体API错误，不能悄悄省略状态显示。

已确认工具限制：ObjectTools.get_properties 对 Blueprint 自动读CDO，不能用它查询 Blueprint.Status/图表元数据；不要为这一不可读取字段反复编译。以编译API返回及本次编译对应日志判断，回读接口与保存状态验收。主HUD原WidgetTree已有快照 Docs/MCP-HUD-Before.json。
