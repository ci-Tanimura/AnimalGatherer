// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "MapManager.generated.h"


/**
 * @brief タイルの状態を定義する列挙型。
 *        方向指示・動物出現・ゴールの8状態を持つ。
 */
UENUM(BlueprintType)
enum class ETileType : uint8
{
	Empty = 0      UMETA(DisplayName = "方向なし (デフォルト)"),
	DirUp = 1      UMETA(DisplayName = "上方向"),
	DirDown = 2    UMETA(DisplayName = "下方向"),
	DirLeft = 3    UMETA(DisplayName = "左方向"),
	DirRight = 4   UMETA(DisplayName = "右方向"),
	Spawn = 5      UMETA(DisplayName = "動物出現ポイント"),
	GoalP1 = 6     UMETA(DisplayName = "1P ゴール"),
	GoalP2 = 7     UMETA(DisplayName = "2P ゴール")
};

/**
 * @brief グリッド上の1マスを表すデータ構造体。
 */
USTRUCT(BlueprintType)
struct FMapTileData
{
	GENERATED_BODY()

	/** @brief タイルのワールド座標。 */
	UPROPERTY(BlueprintReadOnly)
	FVector WorldLocation = FVector::ZeroVector;

	/** @brief タイルの種類。 */
	UPROPERTY(BlueprintReadWrite)
	ETileType TileType = ETileType::Empty;

	/** @brief 所有プレイヤーID（0 は未所有）。 */
	UPROPERTY(BlueprintReadWrite)
	int32 OwnerPlayerID = 0;
};

/**
 * @brief マップ全体のタイルデータ管理と可視化を担うアクター。
 *        HISM による高速レンダリングとエディタ編集機能を提供する。
 */
UCLASS()
class ANIMALGATHERER_API AMapManager : public AActor
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
	 * @brief 全タイルのビジュアルを最新状態に更新する。
	 *        StateVisuals をクリア後、GridData を走査してメッシュインスタンスを再配置する。
	 *        矢印タイルは方向に合わせた回転を適用する。
	 */
	UFUNCTION(BlueprintCallable, Category = "Map")
	void UpdateMapVisuals();

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
