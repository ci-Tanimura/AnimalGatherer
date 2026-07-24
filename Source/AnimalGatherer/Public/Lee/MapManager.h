// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Tanimura/GridInteractInterface.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "MapManager.generated.h"

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
	UFUNCTION(BlueprintCallable, Category = "Map")
	void SetTileData(int32 GridX, int32 GridY, ETileType NewType);

	//==============================================================================
	// IGridInteractInterface 実装
	//==============================================================================

	/** @brief 指定グリッド座標のタイル状態を返す。 */
	virtual ETileType GetCellState_Implementation(FIntPoint GridCoords) const override;

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
};
