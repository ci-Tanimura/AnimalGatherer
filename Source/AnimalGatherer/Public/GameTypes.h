// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameTypes.generated.h"

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