#pragma once

#include "CoreMinimal.h"
#include "GameTypes.h"
#include "AnimalTypes.generated.h"

/**
 * 動物がグリッド上で進む方向（盤面のETileTypeとは別の概念として扱う）
 */
UENUM(BlueprintType)
enum class EGridDirection : uint8
{
	None = 0	UMETA(DisplayName = "移動なし"),
	Up			UMETA(DisplayName = "上"),
	Down		UMETA(DisplayName = "下"),
	Left		UMETA(DisplayName = "左"),
	Right		UMETA(DisplayName = "右")
};

// 盤面のタイル種別を動物の移動方向へ変換する
EGridDirection ToGridDirection(ETileType TileType);

// 移動方向をグリッド座標の移動量へ変換する
FIntPoint ToGridDelta(EGridDirection Direction);
