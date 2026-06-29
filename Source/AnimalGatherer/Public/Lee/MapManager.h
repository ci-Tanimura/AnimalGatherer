// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "MapManager.generated.h"


// タイルが取りうる全状態を定義する
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

// 最小限のタイルデータ構造
USTRUCT(BlueprintType)
struct FMapTileData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite)
    ETileType TileType = ETileType::Empty;

	UPROPERTY(BlueprintReadWrite)
	int32 OwnerPlayerID = 0;
};

UCLASS()
class ANIMALGATHERER_API AMapManager : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMapManager();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
    // --- マップ設定パラメータ（Blueprint から調整可能） ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Config")
    int32 MapWidth = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Config")
    int32 MapHeight = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Config")
    float TileSize = 100.f; // Unreal の基本立方体はデフォルトで 100x100

    // --- 高速レンダリング用コンポーネント ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map Components")
    UHierarchicalInstancedStaticMeshComponent* HISMFloor;

    // --- コアデータ格納 ---
    UPROPERTY(BlueprintReadOnly, Category = "Map Data")
    TArray<FMapTileData> GridData;


    // --- ビジュアルコンポーネント ---
    // ベースフロア（常に表示される基本の床）
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UHierarchicalInstancedStaticMeshComponent* HISM_BaseFloor;

    // タイル状態ごとの追加メッシュを保持（矢印、ゴール等）
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TMap<ETileType, UHierarchicalInstancedStaticMeshComponent*> StateVisuals;

    // --- コアメソッド ---
    UFUNCTION(BlueprintCallable, Category = "Map")
    void UpdateMapVisuals(); // 全タイルのビジュアルを最新の状態に更新する

    // --- エンジン組み込みコンストラクタ（エディタでプロパティを変更するたびに自動実行） ---
    virtual void OnConstruction(const FTransform& Transform) override;

    // --- レベルデザインツール (Level Design Tools) ---
    // 以下の3つの変数でエディタ上で編集したいタイルを指定する
    UPROPERTY(EditAnywhere, Category = "Level Design Tools")
    int32 Edit_X = 0;

    UPROPERTY(EditAnywhere, Category = "Level Design Tools")
    int32 Edit_Y = 0;

    UPROPERTY(EditAnywhere, Category = "Level Design Tools")
    ETileType Edit_TileType = ETileType::Empty;

    // CallInEditor により Blueprint の詳細パネルにボタンとして表示される
    UFUNCTION(CallInEditor, Category = "Level Design Tools")
    void ApplyTileEdit();

    // 安全策：明示的にリセット操作を行ったときのみ全マップデータをクリアする
    UFUNCTION(CallInEditor, Category = "Level Design Tools")
    void ResetAndGenerateBlankMap();
};
