#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameTypes.h"
#include "GridInteractInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UGridInteractInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 動物やプレイヤーが、マップ（Grid）と通信するためのインターフェース
 */

class ANIMALGATHERER_API IGridInteractInterface
{
	GENERATED_BODY()

public:
	// 指定した座標（GridCoords）のマスが、現在どの状態かを取得する
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Grid Interaction")
	ETileType GetCellState(FIntPoint GridCoords) const;
};