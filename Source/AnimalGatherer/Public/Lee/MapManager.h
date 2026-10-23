// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Tanimura/GridInteractInterface.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "MapManager.generated.h"

// 2026.10.22 Lee start（マップ工作台用の前方宣言：実装ヘッダーに依存しない）
class AAnimalSpawner;
// 2026.10.22 Lee end

// 2026.10.22 Lee start（マップ工作台用データ構造：ランタイム・エディタ共通で利用可能）

/**
 * @brief 盤面外（越界）グリッド1マス分の退避記録。
 *        盤面リサイズ時にレイアウトを復元できるよう、座標とタイルデータを対で保持する。
 */
USTRUCT(BlueprintType)
struct FOutOfBoundsTileRecord
{
	GENERATED_BODY()

	/** @brief 盤面座標系で記録した盤面外グリッド座標。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint GridCoords = FIntPoint::ZeroValue;

	/** @brief 退避元タイルのデータ。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FMapTileData TileData;
};

/**
 * @brief 出生タイルと AnimalSpawner の関連付け1つ分。
 *        スポナーを盤面グリッド座標に紐付け、高さオフセットで配置を微調整できる。
 */
USTRUCT(BlueprintType)
struct FSpawnTileBinding
{
	GENERATED_BODY()

	/** @brief 関連付ける AnimalSpawner。未バインドの場合は nullptr。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<class AAnimalSpawner> Spawner = nullptr;

	/** @brief 出生タイルとして割り当てた盤面グリッド座標。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint GridCoords = FIntPoint::ZeroValue;

	/** @brief タイル中心からの高さオフセット（ワールド単位）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HeightOffset = 0.f;
};
// 2026.10.22 Lee end

/**
 * @brief マップ全体のタイルデータ管理と可視化を担うアクター。
 *        HISM による高速レンダリングとエディタ編集機能を提供する。
 */

UCLASS()
class ANIMALGATHERER_API AMapManager : public AActor, public IGridInteractInterface
{
	GENERATED_BODY()

public:
	AMapManager();

protected:
	virtual void BeginPlay() override;

public:
	//==============================================================================
	// マップ設定パラメータ
	//==============================================================================

	/** @brief マップの横幅（タイル数）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Config")
	int32 MapWidth = 10;

	/** @brief マップの縦幅（タイル数）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Config")
	int32 MapHeight = 10;

	/** @brief 1タイルあたりのワールドサイズ。標準キューブ 100x100 を想定。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Config")
	float TileSize = 100.f;

	//==============================================================================
	// HISM レンダリングコンポーネント
	//==============================================================================

	/**
	 * @brief 床面用 HISM コンポーネント。
	 * @deprecated HISM_BaseFloor に移行済み。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map Components")
	UHierarchicalInstancedStaticMeshComponent* HISMFloor;

	/** @brief 全タイルのベースとなる床面メッシュ。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UHierarchicalInstancedStaticMeshComponent* HISM_BaseFloor;

	/**
	 * @brief タイル状態ごとの HISM レイヤーマップ。
	 *        状態別メッシュの一括描画を可能にする。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TMap<ETileType, UHierarchicalInstancedStaticMeshComponent*> StateVisuals;

	// 2026.09.25 Lee start
	/**
	 * @brief 2P（赤）用の方向矢印 HISM レイヤーマップ（DirUp～DirRight の4種のみ）。
	 *        1P 用は従来通り StateVisuals を使用する。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TMap<ETileType, UHierarchicalInstancedStaticMeshComponent*> P2DirectionVisuals;
	// 2026.09.25 Lee end

	//==============================================================================
	// コアデータ
	//==============================================================================

	/**
	 * @brief 全タイルを格納する1次元配列。
	 *        インデックス = Y * MapWidth + X で2次元アクセスする。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Map Data")
	TArray<FMapTileData> GridData;

	//==============================================================================
	// パブリックメソッド
	//==============================================================================

	/**
	 * @brief 全タイルのビジュアルを最新状態に更新する（全レイヤーを再構築）。
	 *        初期化用。ランタイムの単一タイル変更では RefreshStateVisual() を使用すること。
	 *        StateVisuals をクリア後、GridData を走査してメッシュインスタンスを再配置する。
	 *        矢印タイルは方向に合わせた回転を適用する。
	 */
	UFUNCTION(BlueprintCallable, Category = "Map")
	void UpdateMapVisuals();

	// 2026.10.23 Lee start
	/** @brief 空洞を除くセルの床だけを再構築する。座標・他アクター・取引は変更しない。 */
	void RefreshBaseFloorVisuals();
	// 2026.10.23 Lee end

	/**
	 * @brief 指定された1つのタイル状態レイヤーのみを再構築する。
	 *        GridData を走査し、該当する全タイルを一括で再描画する。
	 *        単一タイル変更時のちらつき防止のため、全レイヤー再構築の代わりに使用する。
	 * @param StateType 再構築対象のタイル状態。
	 */
	void RefreshStateVisual(ETileType StateType);

	/**
	 * @brief 指定グリッド座標のタイルデータを実行時に変更する。
	 *        範囲外の場合は警告ログを出力し何もしない。
	 * @param GridX グリッドX座標（列）。
	 * @param GridY グリッドY座標（行）。
	 * @param NewType 設定する新しいタイル種類。
	 */
	// 2026.09.25 Lee start
	// void SetTileData(int32 GridX, int32 GridY, ETileType NewType);
	/**
	 * @param InOwnerPlayerId 設定する所有プレイヤーID（0 = 1P, 1 = 2P）。方向タイルのみ有効。
	 */
	UFUNCTION(BlueprintCallable, Category = "Map")
	void SetTileData(int32 GridX, int32 GridY, ETileType NewType, uint8 InOwnerPlayerId = 0);
	// 2026.09.25 Lee end

	// 2026.09.25 Lee start
	/**
	 * @brief 指定座標の矢印が指定プレイヤーの所有物である場合のみ Empty に戻す。
	 *        相手プレイヤーに上書きされた矢印は消去しない（FIFO 消退の誤削除防止）。
	 * @param OwnerPlayerId 所有者を判定するプレイヤーID（0 = 1P, 1 = 2P）。
	 * @return 実際に消去できた場合 true。
	 */
	UFUNCTION(BlueprintCallable, Category = "Map")
	bool ClearArrowIfOwned(int32 GridX, int32 GridY, uint8 OwnerPlayerId);

	/**
	 * @brief 方向タイル（DirUp～DirRight）かどうかを判定する。
	 * @param Type 判定するタイル種類。
	 * @return 方向タイルの場合 true。
	 */
	static bool IsDirectionTile(ETileType Type)
	{
		return Type >= ETileType::DirUp && Type <= ETileType::DirRight;
	}
	// 2026.09.25 Lee end

	/**
	 * @brief 指定座標がマップ外周のボーダータイルかを判定する。
	 *        外周 1 マス: X==0, Y==0, X==MapWidth-1, Y==MapHeight-1。
	 *        範囲外座標は false を返す。
	 */
	UFUNCTION(BlueprintCallable, Category = "Map")
	bool IsBorderTile(int32 X, int32 Y) const
	{
		return X >= 0 && X < MapWidth && Y >= 0 && Y < MapHeight
			&& (X == 0 || Y == 0 || X == MapWidth - 1 || Y == MapHeight - 1);
	}

	// 2026.10.01 Lee start
	/**
	 * @brief 指定座標のタイル所有プレイヤーIDを取得する（0 = 1P, 1 = 2P）。
	 *        チュートリアルの「自分の矢印が置かれたか」判定用。
	 * @param GridX グリッドX座標（列インデックス）。
	 * @param GridY グリッドY座標（行インデックス）。
	 * @return 所有プレイヤーID。範囲外・無効インデックスは 255（所有者なし 0 との誤一致防止）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Map")
	uint8 GetTileOwner(int32 GridX, int32 GridY) const;
	// 2026.10.01 Lee end

	//==============================================================================
	// IGridInteractInterface 実装
	//==============================================================================

	/** @brief 指定グリッド座標のタイル状態を返す。 */
	virtual ETileType GetCellState_Implementation(FIntPoint GridCoords) const override;

	// 2026.10.08 Tanimura start------------------------------------------------------
	// 指定グリッド座標のマス中心ワールド座標を返す
	virtual FVector ToWorldCenter_Implementation(FIntPoint GridCoords) const override;

	// 指定グリッド座標が盤面内かどうかを返す
	virtual bool IsValidCoord_Implementation(FIntPoint GridCoords) const override;

	// 盤面のタイル数（幅, 高さ）を返す
	virtual FIntPoint GetGridSize_Implementation() const override;
	// 2026.10.08 Tanimura end--------------------------------------------------------

	//==============================================================================
	// エディタ構築処理
	//==============================================================================

	/**
	 * @brief エディタ上での配置・プロパティ変更時に自動実行される。
	 * @param Transform アクターのワールドトランスフォーム。
	 */
	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	/**
	 * @brief エディタのプロパティ変更時に自動呼び出し。
	 *        マップサイズ変更時は全再生成、Level Design Tools 変更時は即時反映する。
	 * @param PropertyChangedEvent 変更されたプロパティ情報。
	 */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	//==============================================================================
	// レベルデザインツール
	//==============================================================================

	/** @brief 編集対象のX座標（列インデックス）。 */
	UPROPERTY(EditAnywhere, Category = "Level Design Tools")
	int32 Edit_X = 0;

	/** @brief 編集対象のY座標（行インデックス）。 */
	UPROPERTY(EditAnywhere, Category = "Level Design Tools")
	int32 Edit_Y = 0;

	/** @brief タイルに設定する新しい種類。 */
	UPROPERTY(EditAnywhere, Category = "Level Design Tools")
	ETileType Edit_TileType = ETileType::Empty;

	/**
	 * @brief 指定座標のタイルを Edit_TileType に変更する。
	 *        座標が範囲外の場合は警告ログを出力する。
	 */
	UFUNCTION(CallInEditor, Category = "Level Design Tools")
	void ApplyTileEdit();

	/**
	 * @brief 全マップをクリアし、現在のサイズで空グリッドを再生成する。
	 */
	UFUNCTION(CallInEditor, Category = "Level Design Tools")
	void ResetAndGenerateBlankMap();

	// 2026.10.22 Lee start（マップ工作台用のデータ層宣言：CPP実装は MapManager.cpp、UIはEditorモジュール側）

#if WITH_EDITORONLY_DATA
	//==============================================================================
	// マップ工作台用エディタ専用データ（レベルに保存される）
	//==============================================================================

	// 2026.10.22 Lee start（旧アセットにはワークベンチ実行前の履歴が無いことを示す0センチネル）
	/** @brief 変更検出・位置再計算用に保存した旧マップ幅（タイル数）。0 は履歴なしセンチネル。 */
	UPROPERTY(VisibleAnywhere, Category = "Map Workbench")
	int32 SavedMapWidth = 0;

	/** @brief 変更検出・位置再計算用に保存した旧マップ高さ（タイル数）。0 は履歴なしセンチネル。 */
	UPROPERTY(VisibleAnywhere, Category = "Map Workbench")
	int32 SavedMapHeight = 0;

	/** @brief 変更検出・位置再計算用に保存した旧タイルサイズ。0 は履歴なしセンチネル。 */
	UPROPERTY(VisibleAnywhere, Category = "Map Workbench")
	float SavedTileSize = 0.f;
	// 2026.10.22 Lee end

	/** @brief 現在盤面外に退避されているタイルの記録配列。 */
	UPROPERTY(VisibleAnywhere, Category = "Map Workbench")
	TArray<FOutOfBoundsTileRecord> OutOfBoundsTiles;

	/** @brief 工作台ツールが生成したアクター（床・出生用等）の参照配列。 */
	UPROPERTY(VisibleAnywhere, Category = "Map Workbench")
	TArray<TObjectPtr<AActor>> ToolCreatedActors;

	/** @brief 出生タイルと AnimalSpawner の関連付け配列。 */
	UPROPERTY(VisibleAnywhere, Category = "Map Workbench")
	TArray<FSpawnTileBinding> SpawnTileBindings;
#endif

#if WITH_EDITOR
	//==============================================================================
	// マップ工作台用エディタ専用操作
	//==============================================================================

	/** @brief 盤面サイズの上限（256x256）。ResizeGridPreservingLayout のクランプに使用。 */
	static constexpr int32 MaxGridDimension = 256;

	/**
	 * @brief 床面HISMとGridData座標を現在のグリッド設定へ再構築・再同期する。
	 *        MapOrigin・TileSize・盤面サイズ変更後の位置同期に用いる。
	 * @param bSyncSpawnBindings true の場合、関連付け済み Spawner の SpawnLocation も同期する（Modify を伴う明示的編集操作専用）。
	 *        視覚再構築のみの場合は false を指定し、他アクターへの書き込みを行わない。
	 */
	void RebuildFloorAndPositions(bool bSyncSpawnBindings = true);

	/**
	 * @brief XYレイアウトを保持したまま盤面サイズを変更する。
	 *        幅・高さは MaxGridDimension を上限にクランプされる。
	 * @param NewWidth 新しい幅（タイル数）。
	 * @param NewHeight 新しい高さ（タイル数）。
	 * @param bPreserveOutOfBounds true の場合、盤面からあふれるタイルを OutOfBoundsTiles に退避し、
	 *        false の場合あふれたタイルを破棄する。
	 * @return 実際にサイズ変更を行った場合 true（同一サイズ・クランプ結果はログを出力し false）。
	 */
	bool ResizeGridPreservingLayout(int32 NewWidth, int32 NewHeight, bool bPreserveOutOfBounds);

	/**
	 * @brief AnimalSpawner を指定グリッドの出生タイルに関連付ける。
	 * @param Spawner 関連付けるスポナー（nullptr 不可）。
	 * @param GridCoords 出生タイルの盤面グリッド座標。
	 * @param HeightOffset タイル中心からの高さオフセット。
	 * @return 関連付けに成功した場合 true（未登録スポナー・範囲外座標・重複バインドは false）。
	 */
	bool BindSpawnTile(AAnimalSpawner* Spawner, FIntPoint GridCoords, float HeightOffset);

	/**
	 * @brief 指定スポナーの出生タイル関連付けを解除する（スポナー自体は移動しない）。
	 * @param Spawner 解除対象のスポナー。
	 * @return 解除できた場合 true（未バインドは false）。
	 */
	bool UnbindSpawnTile(AAnimalSpawner* Spawner);

	/**
	 * @brief 指定スポナーの出生タイルを NewGridCoords へ移動し、
	 *        スポナーアクターをタイル中心＋高さオフセットへ再配置する。
	 * @param Spawner 移動対象のスポナー。
	 * @param NewGridCoords 移動先の盤面グリッド座標。
	 * @return 移動できた場合 true（未バインド・範囲外座標は false）。
	 */
	bool MoveSpawnTile(AAnimalSpawner* Spawner, FIntPoint NewGridCoords);

	//==============================================================================
	// マップ工作台用エディタライフサイクル
	//==============================================================================

	/** @brief ロード直後に保存済み工作台データと現在のグリッド設定の整合を取る。 */
	virtual void PostLoad() override;

	/** @brief プロパティ変更前に旧サイズ等を退避し、リサイズ検出に備える。 */
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;

	/** @brief Undo/Redo 後に床・生成アクター・出生関連付けを現在データへ再同期する。 */
	virtual void PostEditUndo() override;

	/** @brief アクター移動後に MapOrigin を追従させ、床と関連アクターを再配置する。 */
	virtual void PostEditMove(bool bFinished) override;
#endif
	// 2026.10.22 Lee end
};
