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
    /**
     * 指定された座標（GridCoords）のマスが、現在どの状態（方向）かを取得する
     * C++でもBlueprintでもオーバーライド（実装）できるように設定
     * BlueprintNativeEventに設定したから、overrideのとき必ず後ろに_Implementationを入ること（今回はGetCellState_Implementation）
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Grid Interaction")
    ETileType GetCellState(FIntPoint GridCoords) const;
};