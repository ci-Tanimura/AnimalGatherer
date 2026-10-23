// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Internationalization/Text.h"

// 2026.10.23 Lee start（マップ工作台ユーザー可視テキストの集中管理：中日二言語）

/**
 * @brief ワークベンチUIの全ユーザー可視テキスト識別子。
 *        メニュー・筆肋・ボタン・ツールチップ・図例・ホバー・リスト・ステータス・
 *        ダイアログ・保存・取引文を含む。Actor名・座標・パス等のデータは対象外。
 */
enum class EWorkbenchText : uint8
{
	//==========================================================================
	// メニュー / 目標バインド
	//==========================================================================
	MenuTitle,
	MenuTooltip,
	BindSelection,
	BindSelectionTooltip,
	UnbindButton,
	TargetNone,
	ProblemNoTarget,
	ProblemWrongWorld,
	CannotBindPie,
	NotSelectedMap,
	MultiSelectedMapsFmt,
	BoundOkFmt,
	UnboundOk,
	TargetInvalidAutoUnbind,
	SyncParamsDone,
	SyncParamsNoTarget,

	//==========================================================================
	// 筆肋
	//==========================================================================
	BrushSelect,
	BrushErase,
	BrushUp,
	BrushDown,
	BrushLeft,
	BrushRight,
	BrushSpawn,
	BrushGoalP1,
	BrushGoalP2,

	//==========================================================================
	// タイル表示名
	//==========================================================================
	TileEmpty,
	TileUp,
	TileDown,
	TileLeft,
	TileRight,
	TileSpawn,
	TileGoalP1,
	TileGoalP2,
	TileUnknown,

	//==========================================================================
	// 図例
	//==========================================================================
	LegendLine1,
	LegendLine2,
	LegendPanelAxis,
	LegendItemEmpty,
	LegendItemUp,
	LegendItemDown,
	LegendItemLeft,
	LegendItemRight,
	LegendItemSpawn,
	LegendItemGoalP1,
	LegendItemGoalP2,

	//==========================================================================
	// キャンバス
	//==========================================================================
	CanvasNoTarget,
	OverlayPie,
	OverlayLocked,
	HoverCellFmt,
	BoundMark,
	CanvasNoTargetClick,

	//==========================================================================
	// 選択マス情報
	//==========================================================================
	CellNoneHint,
	CellOutOfBoundsFmt,
	CellInfoFmt,
	CellActorListFmt,
	PaintOobFmt,
	PaintBoundCellFmt,

	//==========================================================================
	// ツール配置アクター区
	//==========================================================================
	ToolSectionTitle,
	BtnPickAsset,
	BtnPickAssetTooltip,
	BtnPlaceAsset,
	BtnPlaceAssetTooltip,
	BtnDeleteToolActor,
	BtnDeleteToolActorTooltip,
	AssetNone,
	BtnFocusActor,
	RowMarkOob,
	RowMarkOtherLevel,
	RowDescFmt,
	ToolSectionHint,

	//==========================================================================
	// スポナー区
	//==========================================================================
	SpawnerSectionTitle,
	BtnPickSpawner,
	BtnPickSpawnerTooltip,
	BtnBindSpawn,
	BtnBindSpawnTooltip,
	BtnMoveSpawn,
	BtnMoveSpawnTooltip,
	BtnUnbindSpawnTooltip,
	SpawnerNone,
	SpawnerInfoBoundFmt,
	SpawnerInfoUnboundFmt,
	NoTargetPickMap,
	PieNoRead,
	NoSpawnerSelected,
	MultiSpawnersFmt,
	SpawnerCrossLevel,
	PickedSpawnerBoundFmt,
	PickedSpawnerUnboundFmt,
	NeedPickSpawner,
	CrossLevelBindRejected,
	AlreadyBoundUnbindOrMove,
	SelectedCellOobFmt,
	CellBoundByOtherFmt,
	NotSpawnTileFmt,
	BindOkFmt,
	DataLayerBindFailed,
	DataLayerUnbindFailed,
	UnbindOkFmt,
	NoBinding,
	MoveNeedBinding,
	MoveDataLayerFailed,
	MoveOkFmt,
	TargetTileOccupiedFmt,

	//==========================================================================
	// 素材読み取り・配置
	//==========================================================================
	AssetPickerNone,
	MultiAssetsFmt,
	AssetValidateFailFmt,
	AssetTagFailFmt,
	AssetPickedFmt,
	AssetInfoFmt,
	KindStaticMesh,
	KindBlueprint,
	AssetUnloadedLabel,
	AssetUnloadedStatus,
	PlaceOkFmt,
	PlaceFailed,
	NoCellSelected,
	NoAssetPicked,
	AssetEmpty,
	OnlyMeshOrActorBp,
	OnlyNormalBp,
	BpDeprecated,
	BpParentNotActor,
	BpParentAbstract,
	BpParentDeprecated,
	BpNoGeneratedClass,
	BpGeneratedClassInvalid,

	//==========================================================================
	// 盤面パラメータ
	//==========================================================================
	ParamsSectionTitle,
	LabelWidth,
	LabelHeight,
	LabelTileSize,
	BtnApplyParams,
	BtnApplyParamsTooltip,
	BtnSyncParams,
	BtnSyncParamsTooltip,
	ParamsHintLine,
	NoChange,
	CancelledNoChange,
	InvalidWHFmt,
	InvalidTileSize,
	ShrinkReportHeadFmt,
	ShrinkReportActorsFmt,
	ShrinkReportBindingsFmt,
	ShrinkReportKeep,
	ShrinkReportRemove,
	ShrinkReportCancel,
	ReportTileLineFmt,
	ReportActorLineFmt,
	ReportBindingLineFmt,
	ReportMoreFmt,
	ShrinkDialogTitle,
	ShrinkKeep,
	ShrinkRemove,
	ShrinkCancel,
	ApplyAbortedNoTx,
	ResizeRejectedRolledBack,
	ApplyFailedRolledBack,
	ApplyRollbackFailed,
	ApplyOkFmt,
	KeepOobSuffix,
	RemoveOobSuffix,

	//==========================================================================
	// 保存・取り消し
	//==========================================================================
	BtnSaveLevel,
	BtnSaveLevelTooltip,
	BtnUndo,
	BtnRedo,
	SaveNoLevelPkg,
	SaveNothingDirtyFmt,
	SaveOkFmt,
	SaveFailedFmt,
	SaveCancelled,
	PieNoUndo,
	UndoDone,
	RedoDone,

	//==========================================================================
	// 削除・共通
	//==========================================================================
	DeleteAbortedNoTx,
	DeleteFailedRolledBackFmt,
	DeleteRollbackFailedFmt,
	DeleteOkFmt,
	OtherTxActive,
	DeleteNoSubsystem,
	NotTrackedActor,
	NoRowSelected,
	RowActorGone,
	FocusOkFmt,

	//==========================================================================
	// 取引文
	//==========================================================================
	TxPaint,
	TxPlaceActor,
	TxDeleteActor,
	TxBindSpawn,
	TxUnbindSpawn,
	TxMoveSpawn,
	TxApplyParams,

	//==========================================================================
	// 初期ステータス
	//==========================================================================
	StatusInitial
};

// 2026.10.23 Lee start（レビュー修正：即時反映の記述を修正。静的メニュー/パネルは言語変更後に再起動が必要）
/**
 * @brief ワークベンチUIテキストの集中ヘルパ。
 *        UE の現在 UI 言語（FInternationalization::GetCurrentLanguage）が日本語（ISO 639-1 の
 *        2文字言語コードが ja）なら日本語、英語・中国語・その他は中国語を返す。
 *        OS ロケールや数値書式は参照しない。言語変更後に Get()/Format() されるテキストには
 *        反映されるが、既に構築済みの静的なメニュー/パネルは言語変更後にエディタ再起動が必要
 *        （即時切替は保証しない）。
 */
// 2026.10.23 Lee end
///**
// * @brief ワークベンチUIテキストの集中ヘルパ。
// *        UE の現在 UI 言語（FInternationalization::GetCurrentLanguage）が日本語系（ja, ja-JP など）
// *        なら日本語、英語・中国語・その他は中国語を返す。OS ロケールや数値書式は参照しない。
// *        言語切替は FCulture インスタンス変化を検知して即時反映（再起動不要の設計）。
// */←元のコードは消さない
class FMapWorkbenchText
{
public:
	/** @brief 識別子に対応する表示テキストを返す。 */
	static FText Get(EWorkbenchText Id);

	/**
	 * @brief 識別子のテキストを名前付き引数で整形して返す。
	 *        プレースホルダ名（{x} {y} {actor} など）は中国語・日本語で共通。
	 */
	static FText Format(EWorkbenchText Id, const FFormatNamedArguments& Args);

	/** @brief 現在のUI言語が日本語系か（テスト・デバッグ用）。 */
	static bool IsJapaneseUI();
};
// 2026.10.23 Lee end
