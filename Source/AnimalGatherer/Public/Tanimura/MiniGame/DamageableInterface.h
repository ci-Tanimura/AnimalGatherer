#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DamageableInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UDamageableInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 * ダメージを受ける対象を抽象化するインターフェース
 * GridTileから具体クラスへ依存せずにダメージ処理を伝える
 */
class ANIMALGATHERER_API IDamageableInterface
{
    GENERATED_BODY()

public:
    // ダメージを受けた際に呼ばれる処理（C++/Blueprintどちらでも実装可能）
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "MiniGame|Damage")
    void ReceiveDamage();
};
