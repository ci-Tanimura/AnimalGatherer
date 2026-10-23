// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "EditorUndoClient.h"
#include "GameTypes.h"
#include "Widgets/SCompoundWidget.h"

// 2026.10.22 Lee start（マップ工作台パネル：二维キャンバスとツール操作 UI）

class AAnimalSpawner;
class AMapManager;
class FScopedTransaction;
class STextBlock;
class SWorkbenchCanvas;
class UStaticMesh;
struct FSpawnTileBinding;
struct FToolActorRow;

template <typename ArgType> class SSpinBox;
template <typename ItemT> class SListView;

/**
 * @brief ワークベンチのペン種別。
 *        Select は座標選択のみ（盤面へ書き込まない）、他は対応する ETileType へ対応する。
 */
enum class EWorkbenchBrush : uint8
{
	Select = 0,
	Empty = 1,
	DirUp = 2,
	DirDown = 3,
	DirLeft = 4,
	DirRight = 5,
	Spawn = 6,
	GoalP1 = 7,
	// 2026.10.23 Lee start
	// GoalP2 = 8
	GoalP2 = 8,
	/** @brief 床を除去する固定地形のペン。 */
	Hole = 9
	// 2026.10.23 Lee end
};

/**
 * @brief 地图工作台主面板。
 *        綁定された AMapManager の盤面を二维キャンバスで編集し、
 *        ツール生成アクター・出生タイル関連付け・盤面パラメータ操作を提供する。
 *        すべての書き込み操作は FScopedTransaction 内で行い、PIE/SIE・レベルロック中は拒否する。
 */
class SMapWorkbench : public SCompoundWidget, public FSelfRegisteringEditorUndoClient
{
public:
	SLATE_BEGIN_ARGS(SMapWorkbench) {}
	SLATE_END_ARGS()

	/** @brief パネルを構築し、キャンバス・筆肋選択・各操作 UI を配置する。 */
	void Construct(const FArguments& InArgs);

	/** @brief デストラクタ。未確定のストローク取引を確定させて終了する。 */
	virtual ~SMapWorkbench() override;

	/** @brief 低頻度ポーリング：外部変更（Undo / アクター移動等）の検出と画面更新。 */
	virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;

	/** @brief Undo 完了後にパネル表示を更新する。 */
	virtual void PostUndo(bool bSuccess) override;

	/** @brief Redo 完了後にパネル表示を更新する。 */
	virtual void PostRedo(bool bSuccess) override;

private:
	//==============================================================================
	// 目標バインド
	//==============================================================================

	/** @brief 綁定済み目標を検証して返す。無効時は nullptr と問題説明を返す（バインド解除は行わない）。 */
	AMapManager* GetValidTarget(FText* OutProblem = nullptr) const;

	/** @brief 書き込み可否の総合判定（目標有効・エディタ世界・非 PIE/SIE・レベル未ロック）。 */
	bool CanEditTarget(FText* OutReason) const;

	/** @brief エディタで選択された唯一の AMapManager へ綁定する（無 / 複数は拒否）。 */
	void HandleBindToSelection();

	/** @brief 現在の綁定を明示的に解除する。 */
	void HandleUnbindTarget();

	/** @brief 目標の破棄・世界切替を検出したら自動で綁定を解除する（Tick から呼ぶ）。 */
	void ForceUnbindIfNeeded();

	/** @brief 待機適用パラメータを現在の盤面値へ同期する（UI のみ、Modify しない）。 */
	void SyncPendingParamsFromCurrent();

	//==============================================================================
	// 盤面描画
	//==============================================================================

	/** @brief ペン種別を対応する ETileType へ変換する（Select は Empty を返す）。 */
	ETileType BrushToTileType(EWorkbenchBrush InBrush) const;

	/**
	 * @brief 単一マスへ筆肋を適用する（キャンバスのストロークから呼ばれる）。
	 *        最初の実書き込み時に FScopedTransaction を生成する。
	 * @return 書き込み（または同一値の無操作）で処理を継続できる場合 true。
	 */
	bool PaintCell(const FIntPoint& InCell, ETileType InType, FText& OutReason);

	/** @brief ストローク終了（確定）をキャンバスから受け取る。 */
	void NotifyStrokeCommitted();

	/** @brief 「座標選択」ペンでのクリックを受け取り、選択マスを更新する（書き込まない）。 */
	void HandleCellSelected(const FIntPoint& InCell);

	/** @brief 選択マスの情報テキストを更新する。 */
	void RefreshCellInfo();

	//==============================================================================
	// 座標換算
	//==============================================================================

	/** @brief マス基準点（MapOrigin + X*TileSize, Y*TileSize）のワールド XY を返す。 */
	FVector2D CellAnchorWorldXY(const FIntPoint& InCell) const;

	/** @brief 実ワールド座標を最寄りマスへ投影する（自由配置アクターの表示用）。 */
	FIntPoint ProjectWorldToCell(const FVector& InWorld) const;

	//==============================================================================
	// ツール生成アクター
	//==============================================================================

	/**
	 * @brief 配置素材を検証する。静的メッシュまたは配置可能 Actor ブループリントのみ許可。
	 * @param InAsset 検証するオブジェクト。
	 * @param OutMesh 静的メッシュの場合はそのメッシュ。
	 * @param OutReason 拒否理由（成功時は空）。
	 * @return スポーンに使うクラス。拒否時は nullptr。
	 */
	UClass* ValidateAssetForPlacement(UObject* InAsset, UStaticMesh*& OutMesh, FText& OutReason) const;

	/** @brief コンテンツブラウザの選択素材を読み取り、検証して表示する。 */
	void HandlePickAssetFromSelection();

	/** @brief 検証済み素材を選択マスへ配置し、ToolCreatedActors へ登録する。 */
	void HandlePlaceAssetAtSelectedCell();

	/** @brief リスト選択中のツール生成アクターを取引内で削除し、追跡参照を除去する。 */
	void HandleDeleteSelectedToolActor();

	/** @brief 指定アクターをエディタで選択し、ビューポートへフォーカスする。 */
	void HandleFocusToolActor(AActor* InActor);

	/** @brief 実ワールド位置に基づきツール生成アクターのリスト行を再構築する。 */
	void RefreshToolActorRows();

	//==============================================================================
	// 生成器（AnimalSpawner）
	//==============================================================================

	/** @brief 指定スポナーの出生タイル関連付けを検索する。 */
	const FSpawnTileBinding* FindBindingForSpawner(AAnimalSpawner* InSpawner) const;

	/** @brief 指定マスへ関連付いた出生タイル関連付けを検索する。 */
	const FSpawnTileBinding* FindBindingAtCell(const FIntPoint& InCell) const;

	/** @brief エディタで選択された唯一の AAnimalSpawner を読み取り表示する。 */
	void HandlePickSpawnerFromSelection();

	/** @brief 選択スポナーを選択マス（Spawn グリッド）へ関連付ける。 */
	void HandleBindSpawner();

	/** @brief 選択スポナーの出生タイル関連付けを解除する。 */
	void HandleUnbindSpawner();

	/** @brief 選択スポナーの出生タイルを選択マスへ移動する（旧マス Empty・新マス Spawn）。 */
	void HandleMoveSpawnerSpawnTile();

	/** @brief スポナー状態テキスト（未関連 / 関連マス・高さオフセット）を更新する。 */
	void RefreshSpawnerInfo();

	//==============================================================================
	// 盤面パラメータ
	//==============================================================================

	/**
	 * @brief 縮小時に盤外へあふれるデータの競合報告文を組み立てる（競合なしは false）。
	 *        ツール生成アクターの投影は待機適用の NewTileSize 基準で行い、負座標も越界扱いする。
	 */
	bool BuildShrinkConflictReport(int32 NewWidth, int32 NewHeight, float NewTileSize, FText& OutReport) const;

	/** @brief 待機パラメータを検証して適用する（1 取引：Resize → TileSize → Rebuild 同期）。 */
	void HandleApplyParams();

	//==============================================================================
	// 保存・取り消し
	//==============================================================================

	/** @brief 目標レベル（および追跡アクターの外部パッケージがあればそれら）のみを保存する。 */
	void HandleSaveTargetLevel();

	/** @brief エディタの取り消しを実行し、パネルを更新する。 */
	void HandleUndo();

	/** @brief エディタのやり直しを実行し、パネルを更新する。 */
	void HandleRedo();

	//==============================================================================
	// 表示更新
	//==============================================================================

	/** @brief ステータスバーへメッセージを表示する。 */
	void UpdateStatus(const FText& InMessage);

	/** @brief 全表示要素（目標情報・リスト・セル情報・キャンバス）を更新する。 */
	void RefreshPanel();

	/** @brief 盤面状態の署名を計算する（外部変更ポーリング用）。 */
	uint32 ComputeSignature() const;

	/** @brief 署名を比較し、変化があれば再描画要求を返す。 */
	bool PollExternalChanges();

	//==============================================================================
	// 状態
	//==============================================================================

	/** @brief 綁定中の AMapManager。 */
	TWeakObjectPtr<AMapManager> BoundMap;

	/** @brief 「生成器を選択」で読み取った AAnimalSpawner。 */
	TWeakObjectPtr<AAnimalSpawner> SelectedSpawner;

	/** @brief 「素材を選択」で読み取った配置素材。 */
	FAssetData SelectedAssetData;

	/** @brief 配置素材を読み取り済みか。 */
	bool bHasSelectedAsset = false;

	/** @brief 「座標選択」で選択したマス。 */
	TOptional<FIntPoint> SelectedCell;

	/** @brief 現在のペン種別。 */
	EWorkbenchBrush CurrentBrush = EWorkbenchBrush::Select;

	/** @brief 二维キャンバス（実装は SMapWorkbench.cpp 内）。 */
	TSharedPtr<SWorkbenchCanvas> Canvas;

	/** @brief 目標情報テキスト。 */
	TSharedPtr<STextBlock> TargetInfoText;

	/** @brief ステータスバーテキスト。 */
	TSharedPtr<STextBlock> StatusText;

	/** @brief 選択マス情報テキスト。 */
	TSharedPtr<STextBlock> CellInfoText;

	/** @brief 選択素材情報テキスト。 */
	TSharedPtr<STextBlock> AssetInfoText;

	/** @brief 選択スポナー情報テキスト。 */
	TSharedPtr<STextBlock> SpawnerInfoText;

	/** @brief 待機適用：マップ幅。 */
	TSharedPtr<SSpinBox<int32>> PendingWidthBox;

	/** @brief 待機適用：マップ高さ。 */
	TSharedPtr<SSpinBox<int32>> PendingHeightBox;

	/** @brief 待機適用：タイルサイズ。 */
	TSharedPtr<SSpinBox<float>> PendingTileSizeBox;

	/** @brief ツール生成アクターのリスト。 */
	TSharedPtr<SListView<TSharedPtr<FToolActorRow>>> ToolActorList;

	/** @brief ツール生成アクターのリスト行データ。 */
	TArray<TSharedPtr<FToolActorRow>> ToolActorRows;

	/** @brief ストローク中の取引（最初の実書き込み時に生成、終了時に確定）。 */
	TUniquePtr<FScopedTransaction> StrokeTransaction;

	/** @brief 現在のストロークで実書き込みが発生したか。 */
	bool bStrokeChanged = false;

	/** @brief 前回ポーリング時の盤面署名。 */
	uint32 LastSignature = 0;

	/** @brief 署名を一度以上計算済みか。 */
	bool bHasSignature = false;

	/** @brief ポーリング間隔の累積時間。 */
	float RefreshAccum = 0.0f;

	/** @brief 前回フィット時に使用した盤面設定（変化検出でビュー再フィット）。 */
	int32 LastFitWidth = -1;
	int32 LastFitHeight = -1;
	float LastFitTileSize = -1.0f;
};
// 2026.10.22 Lee end
