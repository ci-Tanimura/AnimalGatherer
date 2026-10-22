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

	// 同時に存在できる動物の数
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxAnimalCount = 5;

	// 盤面と担当マスを受け取る
	UFUNCTION(BlueprintCallable, Category = "Animal")
	void Initialize(const TScriptInterface<IGridInteractInterface>& InGridMap, const FIntPoint InSpawnCell);

	// 出現処理を開始する
	UFUNCTION(BlueprintCallable, Category = "Animal")
	void StartSpawning();

	// 出現処理を停止する
	UFUNCTION(BlueprintCallable, Category = "Animal")
	void StopSpawning();

	// 盤面アクターを返す
	UFUNCTION(BlueprintPure, Category = "Animal")
	AActor* GetMapActor() const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 担当するスポーンマス
	FIntPoint SpawnCell = FIntPoint::ZeroValue;

	// 盤面情報を提供するアクター
	TScriptInterface<IGridInteractInterface> GridMap;

	// 出現した動物（同時数管理用）
	UPROPERTY()
	TArray<TObjectPtr<AAnimalBase_New>> SpawnedAnimals;

	// 出現タイマー
	FTimerHandle SpawnTimerHandle;

	// 動物を1匹出現させる
	void SpawnAnimal();

	// 破棄済みの動物を記録から除く
	void CleanupSpawnedAnimals();

	// 出現時の向きを4方向からランダムに選ぶ
	EGridDirection PickRandomFacing() const;

	// 対戦フェーズが出現を許可するか
	bool IsSpawnAllowedByMatch() const;

	// 盤面が注入されているか
	bool HasGridMap() const;
};
