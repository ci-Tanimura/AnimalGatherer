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

	// グリッド座標をワールド座標のマス中心へ変換する
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Grid Interaction")
	FVector ToWorldCenter(FIntPoint GridCoords) const;

	// グリッド座標が盤面内かどうかを判定する
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Grid Interaction")
	bool IsValidCoord(FIntPoint GridCoords) const;

	// 盤面のタイル数を返す
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Grid Interaction")
	FIntPoint GetGridSize() const;
};