// Fill out your copyright notice in the Description page of Project Settings.

#include "Lee/MapWorkbench/MapWorkbenchText.h"

// 2026.10.23 Lee start（マップ工作台テキスト実装：言語選択と全词条）

#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"

// 2026.10.23 Lee start（レビュー修正：再起動不要の記述を修正。静的メニュー/パネルは言語変更後に再起動が必要）
/**
 * @brief 現在の UE UI 言語（FInternationalization::GetCurrentLanguage）が日本語かを返す。
 *        結果は Culture インスタンスの変化を検知して再計算されるが、これは
 *        Get()/Format() 呼び出し時に取得するテキストにのみ有効で、既に構築済みの
 *        静的なメニュー/パネルの文言は構築時に確定する。言語変更後にエディタを
 *        再起動するまで静的UIへは反映されない（即時切替は保証しない）。
 *        Slate UI スレッドからの呼び出しを前提とする。
 */
// 2026.10.23 Lee end
///**
// * @brief 現在の UE UI 言語（FInternationalization::GetCurrentLanguage）が日本語系かを返す。
// *        Culture インスタンスの変化を検知して結果を再計算する（言語切替後の再起動不要）。
// *        Slate UI スレッドからの呼び出しを前提とする。
// */←元のコードは消さない
bool FMapWorkbenchText::IsJapaneseUI()
{
	const FCulturePtr CurrentCulture = FInternationalization::Get().GetCurrentLanguage();
	static FCulturePtr CachedCulture;
	static bool bCachedJapanese = false;
	if (CachedCulture != CurrentCulture)
	{
		CachedCulture = CurrentCulture;
		// 2026.10.23 Lee start（レビュー修正：先頭一致を廃止し、ISO 2文字言語コードの厳密一致へ変更）
		// ISO 639-1 の2文字言語コードが "ja" と厳密一致するかで判定する
		//（先頭一致だと "jv"（ジャワ語）など "ja" 以外の言語を誤判定する恐れがある）。
		bCachedJapanese = CachedCulture.IsValid() && CachedCulture->GetTwoLetterISOLanguageName() == TEXT("ja");
		// 2026.10.23 Lee end
		//// GetName は "ja-JP" / "ja" 等のリージョン付き名。先頭一致で日本語系を判定する。←元のコードは消さない
		//bCachedJapanese = CachedCulture.IsValid() && CachedCulture->GetName().StartsWith(TEXT("ja"));←元のコードは消さない
	}
	return bCachedJapanese;
}

/** @brief 中国語（既定フォールバック）の词条を返す。 */
static const TCHAR* GetTextZh(EWorkbenchText Id)
{
	switch (Id)
	{
	case EWorkbenchText::MenuTitle:                 return TEXT("地图工作台");
	case EWorkbenchText::MenuTooltip:               return TEXT("打开地图工作台面板（选中一个 MapManager 后使用）");
	case EWorkbenchText::BindSelection:             return TEXT("绑定选中的MapManager");
	case EWorkbenchText::BindSelectionTooltip:      return TEXT("在关卡中恰好选中一个 MapManager 后点击绑定；无选中或多选将被拒绝");
	case EWorkbenchText::UnbindButton:              return TEXT("解除绑定");
	case EWorkbenchText::TargetNone:                return TEXT("目标：未绑定");
	case EWorkbenchText::ProblemNoTarget:           return TEXT("未绑定目标或目标已销毁");
	case EWorkbenchText::ProblemWrongWorld:         return TEXT("目标不在当前编辑器世界中（可能已切换关卡）");
	case EWorkbenchText::CannotBindPie:             return TEXT("PIE/SIE 运行中，禁止切换绑定");
	case EWorkbenchText::NotSelectedMap:            return TEXT("未选中 MapManager：请在关卡中恰好选中一个后重试");
	case EWorkbenchText::MultiSelectedMapsFmt:      return TEXT("选中了 {count} 个 MapManager：请仅选中一个");
	case EWorkbenchText::BoundOkFmt:                return TEXT("已绑定：{actor}（Level：{level}）");
	case EWorkbenchText::UnboundOk:                 return TEXT("已解除绑定");
	case EWorkbenchText::TargetInvalidAutoUnbind:   return TEXT("目标已销毁或所在世界已切换，绑定已解除");
	case EWorkbenchText::SyncParamsDone:            return TEXT("待应用参数已同步为当前盘面值（未修改盘面）");
	case EWorkbenchText::SyncParamsNoTarget:        return TEXT("未绑定目标：无法同步参数");

	case EWorkbenchText::BrushSelect:               return TEXT("选择坐标");
	case EWorkbenchText::BrushErase:                return TEXT("清除");
	// 2026.10.23 Lee start
	case EWorkbenchText::BrushHole:                 return TEXT("空洞");
	case EWorkbenchText::TileHole:                  return TEXT("空洞（无地板，禁止放置箭头）");
	case EWorkbenchText::LegendItemHole:            return TEXT("空洞（无地板）");
	// 2026.10.23 Lee end
	case EWorkbenchText::BrushUp:                   return TEXT("上 (+Y)");
	case EWorkbenchText::BrushDown:                 return TEXT("下 (−Y)");
	case EWorkbenchText::BrushLeft:                 return TEXT("左 (+X)");
	case EWorkbenchText::BrushRight:                return TEXT("右 (−X)");
	case EWorkbenchText::BrushSpawn:                return TEXT("出生点");
	case EWorkbenchText::BrushGoalP1:               return TEXT("1P 终点");
	case EWorkbenchText::BrushGoalP2:               return TEXT("2P 终点");

	case EWorkbenchText::TileEmpty:                 return TEXT("空地");
	case EWorkbenchText::TileUp:                    return TEXT("上箭头 (+Y)");
	case EWorkbenchText::TileDown:                  return TEXT("下箭头 (−Y)");
	case EWorkbenchText::TileLeft:                  return TEXT("左箭头 (+X)");
	case EWorkbenchText::TileRight:                 return TEXT("右箭头 (−X)");
	case EWorkbenchText::TileSpawn:                 return TEXT("出生点");
	case EWorkbenchText::TileGoalP1:                return TEXT("1P 终点");
	case EWorkbenchText::TileGoalP2:                return TEXT("2P 终点");
	case EWorkbenchText::TileUnknown:               return TEXT("未知");

	case EWorkbenchText::LegendLine1:               return TEXT("图例：上笔刷 = +Y（画布向上）    下笔刷 = −Y（画布向下）    左笔刷 = +X（画布向右）    右笔刷 = −X（画布向左）");
	case EWorkbenchText::LegendLine2:               return TEXT("轴：画布 X 向右 = 世界 +X，画布 Y 向上 = 世界 +Y（箭头按世界方向绘制）");
	case EWorkbenchText::LegendPanelAxis:           return TEXT("画布方向：X 向右 = 世界 +X，Y 向上 = 世界 +Y");
	case EWorkbenchText::LegendItemEmpty:           return TEXT("空地");
	case EWorkbenchText::LegendItemUp:              return TEXT("上 +Y");
	case EWorkbenchText::LegendItemDown:            return TEXT("下 −Y");
	case EWorkbenchText::LegendItemLeft:            return TEXT("左 +X");
	case EWorkbenchText::LegendItemRight:           return TEXT("右 −X");
	case EWorkbenchText::LegendItemSpawn:           return TEXT("出生点");
	case EWorkbenchText::LegendItemGoalP1:          return TEXT("1P 终点");
	case EWorkbenchText::LegendItemGoalP2:          return TEXT("2P 终点");

	case EWorkbenchText::CanvasNoTarget:            return TEXT("未绑定 MapManager：在关卡中选中一个 MapManager，然后点击『绑定选中的MapManager』");
	case EWorkbenchText::OverlayPie:                return TEXT("PIE/SIE 运行中，禁止编辑");
	case EWorkbenchText::OverlayLocked:             return TEXT("目标所在关卡已锁定，禁止编辑");
	case EWorkbenchText::HoverCellFmt:              return TEXT("格 ({x}, {y})  {type}");
	case EWorkbenchText::BoundMark:                 return TEXT("  [已关联生成器]");
	case EWorkbenchText::CanvasNoTargetClick:       return TEXT("未绑定 MapManager：请先绑定目标");

	case EWorkbenchText::CellNoneHint:              return TEXT("所选格：未选择（用『选择坐标』笔刷左键点击画布）");
	case EWorkbenchText::CellOutOfBoundsFmt:        return TEXT("所选格：({x}, {y})（盘面外）");
	case EWorkbenchText::CellInfoFmt:               return TEXT("所选格：({x}, {y})  类型：{type}");
	case EWorkbenchText::CellActorListFmt:          return TEXT("  本格工具物件编号：{list}");
	case EWorkbenchText::PaintOobFmt:               return TEXT("坐标 ({x}, {y}) 越界，未写入");
	case EWorkbenchText::PaintBoundCellFmt:         return TEXT("格 ({x}, {y}) 已关联生成器：请先解绑或使用『移动绑定出生点到所选格』");

	case EWorkbenchText::ToolSectionTitle:          return TEXT("工具物件（仅跟踪工作台创建的 Actor）");
	case EWorkbenchText::BtnPickAsset:              return TEXT("读取内容浏览器选中素材");
	case EWorkbenchText::BtnPickAssetTooltip:       return TEXT("读取内容浏览器当前选中的静态网格体或可放置 Actor 蓝图并校验");
	case EWorkbenchText::BtnPlaceAsset:             return TEXT("放置到所选格");
	case EWorkbenchText::BtnPlaceAssetTooltip:      return TEXT("将已读取的素材放置到所选格（同格可多个；保留蓝图默认缩放）");
	case EWorkbenchText::BtnDeleteToolActor:        return TEXT("删除选中列表物件");
	case EWorkbenchText::BtnDeleteToolActorTooltip: return TEXT("删除列表中选中的工作台物件（事务内删除并移除跟踪）");
	case EWorkbenchText::AssetNone:                 return TEXT("素材：未读取");
	case EWorkbenchText::BtnFocusActor:             return TEXT("选中并聚焦");
	case EWorkbenchText::RowMarkOob:                return TEXT("  [越界]");
	case EWorkbenchText::RowMarkOtherLevel:         return TEXT("  [其他Level]");
	case EWorkbenchText::RowDescFmt:                return TEXT("{label}  |  位置 XYZ({x}, {y}, {z})  |  投影格 ({cx}, {cy}){marks}");
	case EWorkbenchText::ToolSectionHint:           return TEXT("同格可放置多个物件；放置后可自由移动/旋转/缩放，工具按实际世界位置显示，不自动吸附。");

	case EWorkbenchText::SpawnerSectionTitle:       return TEXT("生成器（AnimalSpawner 出生格关联）");
	case EWorkbenchText::BtnPickSpawner:            return TEXT("读取选中的AnimalSpawner");
	case EWorkbenchText::BtnPickSpawnerTooltip:     return TEXT("读取关卡中恰好选中的一个 AnimalSpawner；未选或多选将被拒绝");
	case EWorkbenchText::BtnBindSpawn:              return TEXT("绑定出生点到所选格");
	case EWorkbenchText::BtnBindSpawnTooltip:       return TEXT("将已读取的生成器关联到所选格（该格必须是出生点类型）");
	case EWorkbenchText::BtnMoveSpawn:              return TEXT("移动绑定出生点到所选格");
	case EWorkbenchText::BtnMoveSpawnTooltip:       return TEXT("将已关联生成器的出生格移到所选格：旧格置空、新格置为出生点，并同步生成器位置");
	case EWorkbenchText::BtnUnbindSpawnTooltip:     return TEXT("解除已读取生成器的出生格关联（生成器本身不移动）");
	case EWorkbenchText::SpawnerNone:               return TEXT("生成器：未读取");
	case EWorkbenchText::SpawnerInfoBoundFmt:       return TEXT("生成器：{actor}  已关联格 ({x}, {y})  高度偏移 {offset}");
	case EWorkbenchText::SpawnerInfoUnboundFmt:     return TEXT("生成器：{actor}  未关联出生格");
	case EWorkbenchText::NoTargetPickMap:           return TEXT("未绑定目标：请先绑定 MapManager");
	case EWorkbenchText::PieNoRead:                 return TEXT("PIE/SIE 运行中，禁止读取");
	case EWorkbenchText::NoSpawnerSelected:         return TEXT("未读到 AnimalSpawner：请在场景中恰好选中一个");
	case EWorkbenchText::MultiSpawnersFmt:          return TEXT("读到 {count} 个 AnimalSpawner：请仅选中一个");
	case EWorkbenchText::SpawnerCrossLevel:         return TEXT("生成器不在目标 MapManager 所在 Level：跨关卡关联被拒绝");
	case EWorkbenchText::PickedSpawnerBoundFmt:     return TEXT("已读取生成器：{actor}（已关联格 ({x}, {y})）");
	case EWorkbenchText::PickedSpawnerUnboundFmt:   return TEXT("已读取生成器：{actor}（未关联）");
	case EWorkbenchText::NeedPickSpawner:           return TEXT("未读取生成器：请先点击『读取选中的AnimalSpawner』");
	case EWorkbenchText::CrossLevelBindRejected:    return TEXT("跨关卡关联被拒绝：生成器与 MapManager 不在同一 Level");
	case EWorkbenchText::AlreadyBoundUnbindOrMove:  return TEXT("该生成器已关联出生格：请先解除绑定，或使用『移动绑定出生点到所选格』");
	case EWorkbenchText::SelectedCellOobFmt:        return TEXT("所选格 ({x}, {y}) 越界");
	case EWorkbenchText::CellBoundByOtherFmt:       return TEXT("格 ({x}, {y}) 已关联其他生成器");
	case EWorkbenchText::NotSpawnTileFmt:           return TEXT("格 ({x}, {y}) 不是出生点（Spawn）类型：请先用出生点笔刷绘制该格");
	case EWorkbenchText::BindOkFmt:                 return TEXT("已绑定 {actor} 到格 ({x}, {y})，高度偏移 {offset}");
	case EWorkbenchText::DataLayerBindFailed:       return TEXT("绑定失败：数据层拒绝（详见输出日志）");
	case EWorkbenchText::DataLayerUnbindFailed:     return TEXT("解绑失败：数据层拒绝（详见输出日志）");
	case EWorkbenchText::UnbindOkFmt:               return TEXT("已解除 {actor} 的出生格关联");
	case EWorkbenchText::NoBinding:                 return TEXT("该生成器未关联出生格");
	case EWorkbenchText::MoveNeedBinding:           return TEXT("该生成器未关联出生格：请先绑定");
	case EWorkbenchText::MoveDataLayerFailed:       return TEXT("移动失败：数据层拒绝（详见输出日志）");
	case EWorkbenchText::MoveOkFmt:                 return TEXT("已将 {actor} 的出生格移动到 ({x}, {y})");
	case EWorkbenchText::TargetTileOccupiedFmt:     return TEXT("格 ({x}, {y}) 已有 {type}：请先清除该格再移动");

	case EWorkbenchText::AssetPickerNone:           return TEXT("未读到素材：请先在内容浏览器选中一个静态网格体或 Actor 蓝图");
	case EWorkbenchText::MultiAssetsFmt:            return TEXT("读到 {count} 个素材：请仅选中一个");
	case EWorkbenchText::AssetValidateFailFmt:      return TEXT("素材校验失败：{reason}");
	case EWorkbenchText::AssetTagFailFmt:           return TEXT("素材：{name}  [校验失败] {reason}");
	case EWorkbenchText::AssetPickedFmt:            return TEXT("已读取素材：{name}，可用『放置到所选格』");
	case EWorkbenchText::AssetInfoFmt:              return TEXT("素材：{name}（{kind}）");
	case EWorkbenchText::KindStaticMesh:            return TEXT("静态网格体");
	case EWorkbenchText::KindBlueprint:             return TEXT("Actor 蓝图");
	case EWorkbenchText::AssetUnloadedLabel:        return TEXT("素材：已卸载，请重新读取");
	case EWorkbenchText::AssetUnloadedStatus:       return TEXT("素材已卸载：请重新读取");
	case EWorkbenchText::PlaceOkFmt:                return TEXT("已放置 {actor} 于格 ({x}, {y})");
	case EWorkbenchText::PlaceFailed:               return TEXT("放置失败：Actor 生成被拒绝（详见输出日志）");
	case EWorkbenchText::NoCellSelected:            return TEXT("未选择格：请用『选择坐标』笔刷左键点击画布");
	case EWorkbenchText::NoAssetPicked:             return TEXT("未读取素材：请先点击『读取内容浏览器选中素材』");
	case EWorkbenchText::AssetEmpty:                return TEXT("素材为空");
	case EWorkbenchText::OnlyMeshOrActorBp:         return TEXT("仅支持静态网格体或可放置 Actor 蓝图");
	case EWorkbenchText::OnlyNormalBp:              return TEXT("仅支持普通蓝图类（拒绝函数库/宏库/接口蓝图）");
	case EWorkbenchText::BpDeprecated:              return TEXT("该蓝图已标记弃用，拒绝放置");
	case EWorkbenchText::BpParentNotActor:          return TEXT("蓝图父类不是 Actor，拒绝放置");
	case EWorkbenchText::BpParentAbstract:          return TEXT("蓝图父类为抽象类，无法实例化");
	case EWorkbenchText::BpParentDeprecated:        return TEXT("蓝图父类已弃用，拒绝放置");
	case EWorkbenchText::BpNoGeneratedClass:        return TEXT("蓝图未生成可用类（可能未编译）");
	case EWorkbenchText::BpGeneratedClassInvalid:   return TEXT("蓝图生成类为抽象/弃用，拒绝放置");

	case EWorkbenchText::ParamsSectionTitle:        return TEXT("地图参数（待应用值，点击应用才修改）");
	case EWorkbenchText::LabelWidth:                return TEXT("宽");
	case EWorkbenchText::LabelHeight:               return TEXT("高");
	case EWorkbenchText::LabelTileSize:             return TEXT("TileSize");
	case EWorkbenchText::BtnApplyParams:            return TEXT("应用参数");
	case EWorkbenchText::BtnApplyParamsTooltip:     return TEXT("按先Resize后TileSize的顺序应用（一个事务）；缩图时将列出越界数据供选择保留或移除");
	case EWorkbenchText::BtnSyncParams:             return TEXT("从当前值刷新");
	case EWorkbenchText::BtnSyncParamsTooltip:      return TEXT("将待应用值同步为当前盘面参数（不修改盘面）");
	case EWorkbenchText::ParamsHintLine:            return TEXT("宽高范围 1..256；TileSize 需为有限正值。缩放尺寸保留类型与归属，扩图补空地；缩图先列出越界非空格/物件/绑定供选择。");
	case EWorkbenchText::NoChange:                  return TEXT("参数无变化：未创建事务");
	case EWorkbenchText::CancelledNoChange:         return TEXT("已取消：盘面参数未修改");
	case EWorkbenchText::InvalidWHFmt:              return TEXT("宽高无效：{w} x {h}（需 1..{max}）");
	case EWorkbenchText::InvalidTileSize:           return TEXT("TileSize 无效：需为有限正值");
	case EWorkbenchText::ShrinkReportHeadFmt:       return TEXT("缩小到 {w} x {h} 将有越界数据：\n\n◆ 越界非空格 {tiles} 个：\n");
	case EWorkbenchText::ShrinkReportActorsFmt:     return TEXT("\n◆ 越界工具物件 {actors} 个（仅统计本工具跟踪、同 Level）：\n");
	case EWorkbenchText::ShrinkReportBindingsFmt:   return TEXT("\n◆ 越界生成器绑定 {bindings} 个：\n");
	case EWorkbenchText::ShrinkReportKeep:          return TEXT("\n【保留越界数据并缩小】越界格存档保存，扩图后恢复；物件留在原处并显示为越界。\n");
	case EWorkbenchText::ShrinkReportRemove:        return TEXT("【移除越界数据并缩小】删除越界格/仅限本工具跟踪的越界物件，并清理相应绑定（不删资产或文件）。\n");
	case EWorkbenchText::ShrinkReportCancel:        return TEXT("【取消】放弃本次应用，盘面不做任何修改。");
	case EWorkbenchText::ReportTileLineFmt:         return TEXT("  格 ({x}, {y}) {type}");
	case EWorkbenchText::ReportActorLineFmt:        return TEXT("  {actor} @( {x}, {y} )");
	case EWorkbenchText::ReportBindingLineFmt:      return TEXT("  {actor} -> ({x}, {y})");
	case EWorkbenchText::ReportMoreFmt:             return TEXT("  …共 {count} 项\n");
	case EWorkbenchText::ShrinkDialogTitle:         return TEXT("缩小地图确认");
	case EWorkbenchText::ShrinkKeep:                return TEXT("保留越界数据并缩小");
	case EWorkbenchText::ShrinkRemove:              return TEXT("移除越界数据并缩小");
	case EWorkbenchText::ShrinkCancel:              return TEXT("取消");
	case EWorkbenchText::ApplyAbortedNoTx:          return TEXT("应用中止：Map 未能记录到事务（无任何状态变更）");
	case EWorkbenchText::ResizeRejectedRolledBack:  return TEXT("尺寸调整被数据层拒绝：已自动回滚，盘面保持原状");
	case EWorkbenchText::ApplyFailedRolledBack:     return TEXT("应用参数失败：已自动回滚，盘面保持原状");
	case EWorkbenchText::ApplyRollbackFailed:       return TEXT("应用参数失败且回滚失败：请立即手动撤销（Ctrl+Z）");
	case EWorkbenchText::ApplyOkFmt:                return TEXT("已应用参数：{w} x {h}，TileSize {ts}{suffix}");
	case EWorkbenchText::KeepOobSuffix:             return TEXT("（越界数据已保留为档案）");
	case EWorkbenchText::RemoveOobSuffix:           return TEXT("（越界数据已移除）");

	case EWorkbenchText::BtnSaveLevel:              return TEXT("保存目标关卡");
	case EWorkbenchText::BtnSaveLevelTooltip:       return TEXT("仅保存目标 Level 包（含跟踪物件的外部包，若存在）；不保存其他用户资源");
	case EWorkbenchText::BtnUndo:                   return TEXT("撤销");
	case EWorkbenchText::BtnRedo:                   return TEXT("重做");
	case EWorkbenchText::SaveNoLevelPkg:            return TEXT("无法取得目标 Level 的包：保存中止");
	case EWorkbenchText::SaveNothingDirtyFmt:       return TEXT("目标关卡 {pkg} 没有未保存的改动");
	case EWorkbenchText::SaveOkFmt:                 return TEXT("已保存：{pkg}");
	case EWorkbenchText::SaveFailedFmt:             return TEXT("保存失败：{count} 个包未能保存（详见输出日志）");
	case EWorkbenchText::SaveCancelled:             return TEXT("保存被取消或未完成（详见输出日志）");
	case EWorkbenchText::PieNoUndo:                 return TEXT("PIE/SIE 运行中，禁止撤销/重做");
	case EWorkbenchText::UndoDone:                  return TEXT("已执行撤销，面板已刷新");
	case EWorkbenchText::RedoDone:                  return TEXT("已执行重做，面板已刷新");

	case EWorkbenchText::DeleteAbortedNoTx:         return TEXT("删除中止：Map 未能记录到事务（无任何状态变更）");
	case EWorkbenchText::DeleteFailedRolledBackFmt: return TEXT("删除失败：{actor} —— 已自动回滚，未残留部分状态");
	case EWorkbenchText::DeleteRollbackFailedFmt:   return TEXT("删除失败：{actor} —— 回滚失败，请立即手动撤销（Ctrl+Z）");
	case EWorkbenchText::DeleteOkFmt:               return TEXT("已删除工具物件：{actor}");
	case EWorkbenchText::OtherTxActive:             return TEXT("其他编辑事务进行中：为避免误撤销，请稍后重试");
	case EWorkbenchText::DeleteNoSubsystem:         return TEXT("删除失败：无法取得 EditorActorSubsystem");
	case EWorkbenchText::NotTrackedActor:           return TEXT("该 Actor 不是本 Map 跟踪的物件（或其他 Level），拒绝删除");
	case EWorkbenchText::NoRowSelected:             return TEXT("未在列表中选中物件行");
	case EWorkbenchText::RowActorGone:              return TEXT("所选行物件已失效，正在刷新列表");
	case EWorkbenchText::FocusOkFmt:                return TEXT("已在场景中选中并聚焦：{actor}");

	case EWorkbenchText::TxPaint:                   return TEXT("地图工作台：绘制格");
	case EWorkbenchText::TxPlaceActor:              return TEXT("地图工作台：放置物件");
	case EWorkbenchText::TxDeleteActor:             return TEXT("地图工作台：删除工具物件");
	case EWorkbenchText::TxBindSpawn:               return TEXT("地图工作台：绑定出生格");
	case EWorkbenchText::TxUnbindSpawn:             return TEXT("地图工作台：解绑出生格");
	case EWorkbenchText::TxMoveSpawn:               return TEXT("地图工作台：移动出生格");
	case EWorkbenchText::TxApplyParams:             return TEXT("地图工作台：应用地图参数");

	case EWorkbenchText::StatusInitial:             return TEXT("请先在关卡中选中一个 MapManager，然后点击『绑定选中的MapManager』。");
	default:                                        return TEXT("");
	}
}

/** @brief 日本語（ja, ja-JP）の词条を返す。 */
static const TCHAR* GetTextJa(EWorkbenchText Id)
{
	switch (Id)
	{
	case EWorkbenchText::MenuTitle:                 return TEXT("マップ工作台");
	case EWorkbenchText::MenuTooltip:               return TEXT("マップ工作台パネルを開く（MapManager を1つ選択してから使用）");
	case EWorkbenchText::BindSelection:             return TEXT("選択中のMapManagerをバインド");
	case EWorkbenchText::BindSelectionTooltip:      return TEXT("レベルで MapManager をちょうど1つ選択してクリックしてください。未選択・複数選択は拒否されます");
	case EWorkbenchText::UnbindButton:              return TEXT("バインド解除");
	case EWorkbenchText::TargetNone:                return TEXT("ターゲット：未バインド");
	case EWorkbenchText::ProblemNoTarget:           return TEXT("ターゲット未バインドまたはターゲット破棄済み");
	case EWorkbenchText::ProblemWrongWorld:         return TEXT("ターゲットが現在のエディタワールドにありません（レベル切替の可能性）");
	case EWorkbenchText::CannotBindPie:             return TEXT("PIE/SIE 実行中のためバインド切替は禁止");
	case EWorkbenchText::NotSelectedMap:            return TEXT("MapManager 未選択：レベルでちょうど1つ選択してから再試行してください");
	case EWorkbenchText::MultiSelectedMapsFmt:      return TEXT("{count} 個の MapManager が選択されています：1つだけ選択してください");
	case EWorkbenchText::BoundOkFmt:                return TEXT("バインド済み：{actor}（レベル：{level}）");
	case EWorkbenchText::UnboundOk:                 return TEXT("バインドを解除しました");
	case EWorkbenchText::TargetInvalidAutoUnbind:   return TEXT("ターゲットが破棄されたかワールドが切替されたため、バインドを解除しました");
	case EWorkbenchText::SyncParamsDone:            return TEXT("適用待ちパラメータを現在の盤面値へ同期しました（盤面は未変更）");
	case EWorkbenchText::SyncParamsNoTarget:        return TEXT("ターゲット未バインド：パラメータを同期できません");

	case EWorkbenchText::BrushSelect:               return TEXT("座標選択");
	case EWorkbenchText::BrushErase:                return TEXT("消去");
	// 2026.10.23 Lee start
	case EWorkbenchText::BrushHole:                 return TEXT("穴");
	case EWorkbenchText::TileHole:                  return TEXT("穴（床なし・矢印配置不可）");
	case EWorkbenchText::LegendItemHole:            return TEXT("穴（床なし）");
	// 2026.10.23 Lee end
	case EWorkbenchText::BrushUp:                   return TEXT("上 (+Y)");
	case EWorkbenchText::BrushDown:                 return TEXT("下 (−Y)");
	case EWorkbenchText::BrushLeft:                 return TEXT("左 (+X)");
	case EWorkbenchText::BrushRight:                return TEXT("右 (−X)");
	case EWorkbenchText::BrushSpawn:                return TEXT("出生ポイント");
	case EWorkbenchText::BrushGoalP1:               return TEXT("1P ゴール");
	case EWorkbenchText::BrushGoalP2:               return TEXT("2P ゴール");

	case EWorkbenchText::TileEmpty:                 return TEXT("空地");
	case EWorkbenchText::TileUp:                    return TEXT("上矢印 (+Y)");
	case EWorkbenchText::TileDown:                  return TEXT("下矢印 (−Y)");
	case EWorkbenchText::TileLeft:                  return TEXT("左矢印 (+X)");
	case EWorkbenchText::TileRight:                 return TEXT("右矢印 (−X)");
	case EWorkbenchText::TileSpawn:                 return TEXT("出生ポイント");
	case EWorkbenchText::TileGoalP1:                return TEXT("1P ゴール");
	case EWorkbenchText::TileGoalP2:                return TEXT("2P ゴール");
	case EWorkbenchText::TileUnknown:               return TEXT("不明");

	case EWorkbenchText::LegendLine1:               return TEXT("凡例：上ペン = +Y（キャンバス上向き）    下ペン = −Y（下向き）    左ペン = +X（キャンバス右向き）    右ペン = −X（左向き）");
	case EWorkbenchText::LegendLine2:               return TEXT("軸：キャンバス X 右 = ワールド +X、キャンバス Y 上 = ワールド +Y（矢印はワールド方向で描画）");
	case EWorkbenchText::LegendPanelAxis:           return TEXT("キャンバス方向：X 右 = ワールド +X、Y 上 = ワールド +Y");
	case EWorkbenchText::LegendItemEmpty:           return TEXT("空地");
	case EWorkbenchText::LegendItemUp:              return TEXT("上 +Y");
	case EWorkbenchText::LegendItemDown:            return TEXT("下 −Y");
	case EWorkbenchText::LegendItemLeft:            return TEXT("左 +X");
	case EWorkbenchText::LegendItemRight:           return TEXT("右 −X");
	case EWorkbenchText::LegendItemSpawn:           return TEXT("出生ポイント");
	case EWorkbenchText::LegendItemGoalP1:          return TEXT("1P ゴール");
	case EWorkbenchText::LegendItemGoalP2:          return TEXT("2P ゴール");

	case EWorkbenchText::CanvasNoTarget:            return TEXT("MapManager 未バインド：レベルで MapManager を1つ選択し、『選択中のMapManagerをバインド』をクリックしてください");
	case EWorkbenchText::OverlayPie:                return TEXT("PIE/SIE 実行中のため編集禁止");
	case EWorkbenchText::OverlayLocked:             return TEXT("対象のレベルはロック中のため編集禁止");
	case EWorkbenchText::HoverCellFmt:              return TEXT("マス ({x}, {y})  {type}");
	case EWorkbenchText::BoundMark:                 return TEXT("  [スポナー関連あり]");
	case EWorkbenchText::CanvasNoTargetClick:       return TEXT("MapManager 未バインド：先にターゲットをバインドしてください");

	case EWorkbenchText::CellNoneHint:              return TEXT("選択マス：未選択（『座標選択』ペンで左クリックしてください）");
	case EWorkbenchText::CellOutOfBoundsFmt:        return TEXT("選択マス：({x}, {y})（盤面外）");
	case EWorkbenchText::CellInfoFmt:               return TEXT("選択マス：({x}, {y})  種類：{type}");
	case EWorkbenchText::CellActorListFmt:          return TEXT("  このマスの配置アクター番号：{list}");
	case EWorkbenchText::PaintOobFmt:               return TEXT("座標 ({x}, {y}) は盤面外のため書き込みません");
	case EWorkbenchText::PaintBoundCellFmt:         return TEXT("マス ({x}, {y}) はスポナーに関連付いています：先に解除するか『出生マスを選択マスへ移動』を使用してください");

	case EWorkbenchText::ToolSectionTitle:          return TEXT("配置アクター（工作台が作成した Actor のみ追跡）");
	case EWorkbenchText::BtnPickAsset:              return TEXT("コンテンツブラウザの選択素材を読む");
	case EWorkbenchText::BtnPickAssetTooltip:       return TEXT("コンテンツブラウザで選択中のスタティックメッシュまたは配置可能アクターブループリントを読み取り検証します");
	case EWorkbenchText::BtnPlaceAsset:             return TEXT("選択マスへ配置");
	case EWorkbenchText::BtnPlaceAssetTooltip:      return TEXT("読み込み済み素材を選択マスへ配置します（同一マスに複数可。ブループリントの既定スケールを保持）");
	case EWorkbenchText::BtnDeleteToolActor:        return TEXT("選択中リスト項目を削除");
	// 2026.10.23 Lee start（用語修正：UEのトランザクション用語へ変更）
	case EWorkbenchText::BtnDeleteToolActorTooltip: return TEXT("リストで選択した工作台アクターを削除します（トランザクション内で削除し追跡を解除）");
	// 2026.10.23 Lee end
	//case EWorkbenchText::BtnDeleteToolActorTooltip: return TEXT("リストで選択した工作台アクターを削除します（取引内で削除し追跡を解除）");←元のコードは消さない
	case EWorkbenchText::AssetNone:                 return TEXT("素材：未読み込み");
	case EWorkbenchText::BtnFocusActor:             return TEXT("選択してフォーカス");
	case EWorkbenchText::RowMarkOob:                return TEXT("  [盤外]");
	case EWorkbenchText::RowMarkOtherLevel:         return TEXT("  [他レベル]");
	case EWorkbenchText::RowDescFmt:                return TEXT("{label}  |  位置 XYZ({x}, {y}, {z})  |  投影マス ({cx}, {cy}){marks}");
	case EWorkbenchText::ToolSectionHint:           return TEXT("同一マスに複数配置できます。配置後は移動・回転・スケール自由で、実ワールド位置のまま表示されます（自動スナップなし）。");

	case EWorkbenchText::SpawnerSectionTitle:       return TEXT("スポナー（AnimalSpawner 出生マス関連付け）");
	case EWorkbenchText::BtnPickSpawner:            return TEXT("選択中のAnimalSpawnerを読む");
	case EWorkbenchText::BtnPickSpawnerTooltip:     return TEXT("レベルでちょうど1つ選択された AnimalSpawner を読み取ります。未選択・複数選択は拒否されます");
	case EWorkbenchText::BtnBindSpawn:              return TEXT("出生マスを選択マスへ関連付け");
	case EWorkbenchText::BtnBindSpawnTooltip:       return TEXT("読み込み済みスポナーを選択マスへ関連付けます（そのマスは出生ポイント種別である必要があります）");
	case EWorkbenchText::BtnMoveSpawn:              return TEXT("出生マスを選択マスへ移動");
	case EWorkbenchText::BtnMoveSpawnTooltip:       return TEXT("関連付きスポナーの出生マスを選択マスへ移動します：旧マスを空地に、新マスを出生ポイントにし、スポナー位置も同期します");
	case EWorkbenchText::BtnUnbindSpawnTooltip:     return TEXT("読み込み済みスポナーの出生マス関連付けを解除します（スポナー自体は移動しません）");
	case EWorkbenchText::SpawnerNone:               return TEXT("スポナー：未読み込み");
	case EWorkbenchText::SpawnerInfoBoundFmt:       return TEXT("スポナー：{actor}  関連マス ({x}, {y})  高さオフセット {offset}");
	case EWorkbenchText::SpawnerInfoUnboundFmt:     return TEXT("スポナー：{actor}  出生マス未関連");
	case EWorkbenchText::NoTargetPickMap:           return TEXT("ターゲット未バインド：先に MapManager をバインドしてください");
	case EWorkbenchText::PieNoRead:                 return TEXT("PIE/SIE 実行中のため読み取り禁止");
	case EWorkbenchText::NoSpawnerSelected:         return TEXT("AnimalSpawner 未検出：シーンでちょうど1つ選択してください");
	case EWorkbenchText::MultiSpawnersFmt:          return TEXT("{count} 個の AnimalSpawner を検出：1つだけ選択してください");
	case EWorkbenchText::SpawnerCrossLevel:         return TEXT("スポナーが対象 MapManager と別レベルにあります：レベル跨ぎの関連付けは拒否されました");
	case EWorkbenchText::PickedSpawnerBoundFmt:     return TEXT("スポナー読込済み：{actor}（関連マス ({x}, {y})）");
	case EWorkbenchText::PickedSpawnerUnboundFmt:   return TEXT("スポナー読込済み：{actor}（未関連）");
	case EWorkbenchText::NeedPickSpawner:           return TEXT("スポナー未読み込み：先に『選択中のAnimalSpawnerを読む』をクリックしてください");
	case EWorkbenchText::CrossLevelBindRejected:    return TEXT("レベル跨ぎの関連付けは拒否：スポナーと MapManager が同じレベルにありません");
	case EWorkbenchText::AlreadyBoundUnbindOrMove:  return TEXT("このスポナーは既に出生マスへ関連付いています：先に解除するか『出生マスを選択マスへ移動』を使用してください");
	case EWorkbenchText::SelectedCellOobFmt:        return TEXT("選択マス ({x}, {y}) は盤面外です");
	case EWorkbenchText::CellBoundByOtherFmt:       return TEXT("マス ({x}, {y}) は他のスポナーに関連付いています");
	case EWorkbenchText::NotSpawnTileFmt:           return TEXT("マス ({x}, {y}) は出生ポイント（Spawn）種別ではありません：先に出生ペンで描画してください");
	case EWorkbenchText::BindOkFmt:                 return TEXT("{actor} をマス ({x}, {y}) へ関連付けました。高さオフセット {offset}");
	case EWorkbenchText::DataLayerBindFailed:       return TEXT("関連付け失敗：データ層に拒否されました（詳細は出力ログ参照）");
	case EWorkbenchText::DataLayerUnbindFailed:     return TEXT("解除失敗：データ層に拒否されました（詳細は出力ログ参照）");
	case EWorkbenchText::UnbindOkFmt:               return TEXT("{actor} の出生マス関連付けを解除しました");
	case EWorkbenchText::NoBinding:                 return TEXT("このスポナーは出生マスへ関連付いていません");
	case EWorkbenchText::MoveNeedBinding:           return TEXT("このスポナーは出生マスへ関連付いていません：先に関連付けてください");
	case EWorkbenchText::MoveDataLayerFailed:       return TEXT("移動失敗：データ層に拒否されました（詳細は出力ログ参照）");
	case EWorkbenchText::MoveOkFmt:                 return TEXT("{actor} の出生マスを ({x}, {y}) へ移動しました");
	case EWorkbenchText::TargetTileOccupiedFmt:     return TEXT("マス ({x}, {y}) には既に {type} があります：移動前にそのマスを消去してください");

	case EWorkbenchText::AssetPickerNone:           return TEXT("素材未検出：コンテンツブラウザでスタティックメッシュまたはアクターブループリントを1つ選択してください");
	case EWorkbenchText::MultiAssetsFmt:            return TEXT("{count} 個の素材を検出：1つだけ選択してください");
	case EWorkbenchText::AssetValidateFailFmt:      return TEXT("素材検証失敗：{reason}");
	case EWorkbenchText::AssetTagFailFmt:           return TEXT("素材：{name}  [検証失敗] {reason}");
	case EWorkbenchText::AssetPickedFmt:            return TEXT("素材読込済み：{name}。『選択マスへ配置』で使用できます");
	case EWorkbenchText::AssetInfoFmt:              return TEXT("素材：{name}（{kind}）");
	case EWorkbenchText::KindStaticMesh:            return TEXT("スタティックメッシュ");
	case EWorkbenchText::KindBlueprint:             return TEXT("アクターブループリント");
	case EWorkbenchText::AssetUnloadedLabel:        return TEXT("素材：アンロード済み。再読み込みしてください");
	case EWorkbenchText::AssetUnloadedStatus:       return TEXT("素材がアンロードされています：再読み込みしてください");
	case EWorkbenchText::PlaceOkFmt:                return TEXT("{actor} をマス ({x}, {y}) へ配置しました");
	case EWorkbenchText::PlaceFailed:               return TEXT("配置失敗：アクター生成が拒否されました（詳細は出力ログ参照）");
	case EWorkbenchText::NoCellSelected:            return TEXT("マス未選択：『座標選択』ペンでキャンバスを左クリックしてください");
	case EWorkbenchText::NoAssetPicked:             return TEXT("素材未読み込み：先に『コンテンツブラウザの選択素材を読む』をクリックしてください");
	case EWorkbenchText::AssetEmpty:                return TEXT("素材が空です");
	case EWorkbenchText::OnlyMeshOrActorBp:         return TEXT("スタティックメッシュまたは配置可能アクターブループリントのみ対応");
	case EWorkbenchText::OnlyNormalBp:              return TEXT("通常のブループリントクラスのみ対応（関数ライブラリ・マクロライブラリ・インターフェースは拒否）");
	case EWorkbenchText::BpDeprecated:              return TEXT("このブループリントは非推奨マーク付きのため配置を拒否");
	case EWorkbenchText::BpParentNotActor:          return TEXT("ブループリントの親クラスが Actor ではないため配置を拒否");
	case EWorkbenchText::BpParentAbstract:          return TEXT("ブループリントの親クラスが抽象クラスのためインスタンス化できません");
	case EWorkbenchText::BpParentDeprecated:        return TEXT("ブループリントの親クラスが非推奨のため配置を拒否");
	case EWorkbenchText::BpNoGeneratedClass:        return TEXT("ブループリントに生成クラスがありません（未コンパイルの可能性）");
	case EWorkbenchText::BpGeneratedClassInvalid:   return TEXT("ブループリント生成クラスが抽象/非推奨のため配置を拒否");

	case EWorkbenchText::ParamsSectionTitle:        return TEXT("マップパラメータ（適用待ち値：適用クリックで反映）");
	case EWorkbenchText::LabelWidth:                return TEXT("幅");
	case EWorkbenchText::LabelHeight:               return TEXT("高さ");
	case EWorkbenchText::LabelTileSize:             return TEXT("TileSize");
	case EWorkbenchText::BtnApplyParams:            return TEXT("パラメータを適用");
	// 2026.10.23 Lee start（用語修正：UEのトランザクション用語へ変更）
	case EWorkbenchText::BtnApplyParamsTooltip:     return TEXT("Resize を先に、次に TileSize の順で適用します（1つのトランザクション）。縮小時は盤外データの一覧を提示し保持/削除を選択できます");
	// 2026.10.23 Lee end
	//case EWorkbenchText::BtnApplyParamsTooltip:     return TEXT("Resize を先に、次に TileSize の順で適用します（1取引）。縮小時は盤外データの一覧を提示し保持/削除を選択できます");←元のコードは消さない
	case EWorkbenchText::BtnSyncParams:             return TEXT("現在値へ同期");
	case EWorkbenchText::BtnSyncParamsTooltip:      return TEXT("適用待ち値を現在の盤面パラメータへ同期します（盤面は変更しません）");
	case EWorkbenchText::ParamsHintLine:            return TEXT("幅・高さは 1..256、TileSize は有限の正の値。サイズ変更は種類と所有を保持し、拡張は空地で埋めます。縮小時は盤外の非空マス/アクター/関連付けを一覧して選択させます。");
	// 2026.10.23 Lee start（用語修正：UEのトランザクション用語へ変更）
	case EWorkbenchText::NoChange:                  return TEXT("パラメータ変更なし：トランザクションは作成しません");
	// 2026.10.23 Lee end
	//case EWorkbenchText::NoChange:                  return TEXT("パラメータ変更なし：取引は作成しません");←元のコードは消さない
	case EWorkbenchText::CancelledNoChange:         return TEXT("キャンセル：盤面パラメータは未変更です");
	case EWorkbenchText::InvalidWHFmt:              return TEXT("幅・高さが無効：{w} x {h}（1..{max} が必要）");
	case EWorkbenchText::InvalidTileSize:           return TEXT("TileSize が無効：有限の正の値が必要です");
	case EWorkbenchText::ShrinkReportHeadFmt:       return TEXT("{w} x {h} へ縮小すると盤外データが発生します：\n\n◆ 盤外の非空マス {tiles} 個：\n");
	case EWorkbenchText::ShrinkReportActorsFmt:     return TEXT("\n◆ 盤外の配置アクター {actors} 個（本ツール追跡・同レベルのみ集計）：\n");
	case EWorkbenchText::ShrinkReportBindingsFmt:   return TEXT("\n◆ 盤外のスポナー関連付け {bindings} 件：\n");
	case EWorkbenchText::ShrinkReportKeep:          return TEXT("\n【盤外データを保持して縮小】盤外マスはアーカイブ保存し、拡張時に復元。アクターは元の位置に残り盤外表示になります。\n");
	case EWorkbenchText::ShrinkReportRemove:        return TEXT("【盤外データを削除して縮小】盤外マス/本ツール追跡の盤外アクターのみ削除し、対応する関連付けを整理します（アセットやファイルは削除しません）。\n");
	// 2026.10.23 Lee start（用語修正：中国語残留「本次」→「今回」）
	case EWorkbenchText::ShrinkReportCancel:        return TEXT("【キャンセル】今回の適用を中止し、盤面は一切変更しません。");
	// 2026.10.23 Lee end
	//case EWorkbenchText::ShrinkReportCancel:        return TEXT("【キャンセル】本次の適用を中止し、盤面は一切変更しません。");←元のコードは消さない
	case EWorkbenchText::ReportTileLineFmt:         return TEXT("  マス ({x}, {y}) {type}");
	case EWorkbenchText::ReportActorLineFmt:        return TEXT("  {actor} @( {x}, {y} )");
	case EWorkbenchText::ReportBindingLineFmt:      return TEXT("  {actor} -> ({x}, {y})");
	case EWorkbenchText::ReportMoreFmt:             return TEXT("  …計 {count} 件\n");
	case EWorkbenchText::ShrinkDialogTitle:         return TEXT("マップ縮小の確認");
	case EWorkbenchText::ShrinkKeep:                return TEXT("盤外データを保持して縮小");
	case EWorkbenchText::ShrinkRemove:              return TEXT("盤外データを削除して縮小");
	case EWorkbenchText::ShrinkCancel:              return TEXT("キャンセル");
	// 2026.10.23 Lee start（用語修正：UEのトランザクション用語へ変更）
	case EWorkbenchText::ApplyAbortedNoTx:          return TEXT("適用中止：Map をトランザクションへ記録できませんでした（状態変更なし）");
	// 2026.10.23 Lee end
	//case EWorkbenchText::ApplyAbortedNoTx:          return TEXT("適用中止：Map を取引へ記録できませんでした（状態変更なし）");←元のコードは消さない
	case EWorkbenchText::ResizeRejectedRolledBack:  return TEXT("サイズ変更がデータ層に拒否されました：自動ロールバック済み、盤面は元のまま");
	case EWorkbenchText::ApplyFailedRolledBack:     return TEXT("パラメータ適用に失敗：自動ロールバック済み、盤面は元のまま");
	case EWorkbenchText::ApplyRollbackFailed:       return TEXT("パラメータ適用に失敗しロールバックも失敗：直ちに手動で取り消してください（Ctrl+Z）");
	case EWorkbenchText::ApplyOkFmt:                return TEXT("パラメータを適用：{w} x {h}、TileSize {ts}{suffix}");
	case EWorkbenchText::KeepOobSuffix:             return TEXT("（盤外データはアーカイブとして保持）");
	case EWorkbenchText::RemoveOobSuffix:           return TEXT("（盤外データは削除済み）");

	case EWorkbenchText::BtnSaveLevel:              return TEXT("対象レベルを保存");
	case EWorkbenchText::BtnSaveLevelTooltip:       return TEXT("対象レベルのパッケージのみ保存します（追跡アクターの外部パッケージがあれば含む）。他のユーザーリソースは保存しません");
	case EWorkbenchText::BtnUndo:                   return TEXT("元に戻す");
	case EWorkbenchText::BtnRedo:                   return TEXT("やり直し");
	case EWorkbenchText::SaveNoLevelPkg:            return TEXT("対象レベルのパッケージを取得できません：保存を中止");
	case EWorkbenchText::SaveNothingDirtyFmt:       return TEXT("対象レベル {pkg} に未保存の変更はありません");
	case EWorkbenchText::SaveOkFmt:                 return TEXT("保存しました：{pkg}");
	case EWorkbenchText::SaveFailedFmt:             return TEXT("保存失敗：{count} 個のパッケージが保存できませんでした（詳細は出力ログ参照）");
	case EWorkbenchText::SaveCancelled:             return TEXT("保存がキャンセルされたか未完了です（詳細は出力ログ参照）");
	case EWorkbenchText::PieNoUndo:                 return TEXT("PIE/SIE 実行中のため元に戻す/やり直しは禁止");
	case EWorkbenchText::UndoDone:                  return TEXT("取り消しを実行し、パネルを更新しました");
	case EWorkbenchText::RedoDone:                  return TEXT("やり直しを実行し、パネルを更新しました");

	// 2026.10.23 Lee start（用語修正：UEのトランザクション用語へ変更）
	case EWorkbenchText::DeleteAbortedNoTx:         return TEXT("削除中止：Map をトランザクションへ記録できませんでした（状態変更なし）");
	// 2026.10.23 Lee end
	//case EWorkbenchText::DeleteAbortedNoTx:         return TEXT("削除中止：Map を取引へ記録できませんでした（状態変更なし）");←元のコードは消さない
	case EWorkbenchText::DeleteFailedRolledBackFmt: return TEXT("削除失敗：{actor} —— 自動ロールバック済み、部分状態は残っていません");
	case EWorkbenchText::DeleteRollbackFailedFmt:   return TEXT("削除失敗：{actor} —— ロールバックも失敗。直ちに手動で取り消してください（Ctrl+Z）");
	case EWorkbenchText::DeleteOkFmt:               return TEXT("配置アクターを削除しました：{actor}");
	// 2026.10.23 Lee start（用語修正：UEのトランザクション用語へ変更）
	case EWorkbenchText::OtherTxActive:             return TEXT("他の編集トランザクションが進行中：誤取り消しを避けるため後ほど再試行してください");
	// 2026.10.23 Lee end
	//case EWorkbenchText::OtherTxActive:             return TEXT("他の編集取引が進行中：誤取り消しを避けるため後ほど再試行してください");←元のコードは消さない
	case EWorkbenchText::DeleteNoSubsystem:         return TEXT("削除失敗：EditorActorSubsystem を取得できません");
	case EWorkbenchText::NotTrackedActor:           return TEXT("この Actor は本 Map の追跡対象ではありません（または別レベル）。削除を拒否します");
	case EWorkbenchText::NoRowSelected:             return TEXT("リストで行が選択されていません");
	case EWorkbenchText::RowActorGone:              return TEXT("選択行のアクターは無効化されています。リストを更新します");
	case EWorkbenchText::FocusOkFmt:                return TEXT("シーンで選択してフォーカスしました：{actor}");

	case EWorkbenchText::TxPaint:                   return TEXT("マップ工作台：マス描画");
	case EWorkbenchText::TxPlaceActor:              return TEXT("マップ工作台：アクター配置");
	case EWorkbenchText::TxDeleteActor:             return TEXT("マップ工作台：アクター削除");
	case EWorkbenchText::TxBindSpawn:               return TEXT("マップ工作台：出生マス関連付け");
	case EWorkbenchText::TxUnbindSpawn:             return TEXT("マップ工作台：出生マス関連解除");
	case EWorkbenchText::TxMoveSpawn:               return TEXT("マップ工作台：出生マス移動");
	case EWorkbenchText::TxApplyParams:             return TEXT("マップ工作台：マップパラメータ適用");

	case EWorkbenchText::StatusInitial:             return TEXT("先にレベルで MapManager を1つ選択し、『選択中のMapManagerをバインド』をクリックしてください。");
	default:                                        return TEXT("");
	}
}

FText FMapWorkbenchText::Get(EWorkbenchText Id)
{
	return FText::FromString(IsJapaneseUI() ? GetTextJa(Id) : GetTextZh(Id));
}

FText FMapWorkbenchText::Format(EWorkbenchText Id, const FFormatNamedArguments& Args)
{
	return FText::Format(FTextFormat(Get(Id)), Args);
}
// 2026.10.23 Lee end
