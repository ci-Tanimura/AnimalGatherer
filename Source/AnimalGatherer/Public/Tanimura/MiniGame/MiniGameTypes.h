// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MiniGameTypes.generated.h"

// 前方宣言
class AGridTile;

/**
 * グリッド上の8方向（上下左右＋斜め）を示す移動方向。
 */
UENUM(BlueprintType)
enum class EGridDirection8 : uint8
{
    None = 0,
    Up,
    Down,
    Left,
    Right,
    UpLeft,
    UpRight,
    DownLeft,
    DownRight
};

/**
 * マス（タイル）の状態を表すステート。
 */
UENUM(BlueprintType)
enum class ETileState : uint8
{
    Safe = 0            UMETA(DisplayName = "安全"),
    Warning = 1         UMETA(DisplayName = "出現予兆 (乗っても安全)"),
    ActiveHazard = 2    UMETA(DisplayName = "危険領域 (即死)"),
    ExpiringHazard = 3  UMETA(DisplayName = "消滅間近 (点滅・即死)")
};

/**
 * 敵（アザラシ等）の移動および生成バランスを制御するパラメータ群。
 */
USTRUCT(BlueprintType)
struct FEnemyParameters
{
    GENERATED_BODY()

    // 予兆を表示する時間（秒）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Parameters")
    float WarningDuration = 2.0f;

    // 次のマスへ移動するまでの間隔（秒）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Parameters")
    float MoveInterval = 1.0f;

    // 1秒あたりに移動間隔を減少させる量
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Parameters")
    float MoveIntervalDecreaseRate = 0.005f;

    // 移動間隔の最小値
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Parameters")
    float MinMoveInterval = 0.3f;

    // 消滅するまでの最大移動回数
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Parameters")
    int32 MaxTravelMoves = 10;

    // 消滅間近（点滅）とみなす残り移動回数
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Parameters")
    int32 ExpirationWarningMoves = 3;

    // 新たな敵の生成周期（秒）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Parameters")
    float SpawnInterval = 5.0f;

    // 1秒あたりに生成周期を減少させる量
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Parameters")
    float SpawnIntervalDecreaseRate = 0.1f;

    // 生成周期の最小値
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Parameters")
    float MinSpawnInterval = 0.05f;
};

/**
 * 盤面（グリッド）内の1マスに関するデータを保持する構造体。
 */
USTRUCT(BlueprintType)
struct FGridTileData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Grid Tile Data")
    FVector WorldLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Grid Tile Data")
    ETileState TileState = ETileState::Safe;

    UPROPERTY(BlueprintReadOnly, Category = "Grid Tile Data")
    AGridTile* TileActor = nullptr;
};