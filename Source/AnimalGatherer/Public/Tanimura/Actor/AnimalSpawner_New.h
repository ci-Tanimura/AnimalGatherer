#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Tanimura/GridInteractInterface.h"
#include "Tanimura/Actor/AnimalBase_New.h"
#include "AnimalSpawner_New.generated.h"

/**
 * 担当するスポーンマスに、一定間隔で動物を1匹ずつ出現させる
 */
UCLASS()
class ANIMALGATHERER_API AAnimalSpawner_New : public AActor
{
	GENERATED_BODY()

public:
	AAnimalSpawner_New();

	// 出現させる動物のクラス
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal")
	TSubclassOf<AAnimalBase_New> AnimalClass;

	// 動物を出現させる間隔（秒）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float SpawnInterval = 3.0f;

	// 盤面と担当マスを受け取り、出現を開始する
	UFUNCTION(BlueprintCallable, Category = "Animal")
	void Initialize(const TScriptInterface<IGridInteractInterface>& InGridMap, const FIntPoint InSpawnCell);

private:
	// 担当するスポーンマス
	FIntPoint SpawnCell = FIntPoint::ZeroValue;

	// 盤面情報を提供するアクター
	TScriptInterface<IGridInteractInterface> GridMap;

	// 出現タイマー
	FTimerHandle SpawnTimerHandle;

	// 動物を1匹出現させる
	void SpawnAnimal();

	// 出現時の向きを4方向からランダムに選ぶ
	EGridDirection PickRandomFacing() const;

	// 盤面が注入されているか
	bool HasGridMap() const;
};
