#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "AnimalSpawner.generated.h"

UCLASS()
class ANIMALGATHERER_API AAnimalSpawner : public AActor
{
	GENERATED_BODY()

public:
	AAnimalSpawner();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	//生成する動物のクラス
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal Spawner")
	TSubclassOf<APawn> AnimalClass;

	//動物を出す座標
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal Spawner")
	FVector SpawnLocation = FVector::ZeroVector;

	//動物を出す向き
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal Spawner")
	FRotator SpawnRotation = FRotator::ZeroRotator;

	//生成した動物に設定するマップ
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal Map")
	TObjectPtr<AActor> MapActor;

	//何秒ごとに生成するか
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal Spawner", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float SpawnInterval = 3.0f;

	//ステージに存在できる動物の数制限
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal Spawner", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxAnimalCount = 5;

	//ゲーム開始時にスポーンを始めるかどうか
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal Spawner")
	bool bStartSpawningOnBeginPlay = false;

	//スポーン処理を開始する関数
	UFUNCTION(BlueprintCallable, Category = "Animal Spawner")
	void StartSpawning();

	//スポーン処理を停止する関数
	UFUNCTION(BlueprintCallable, Category = "Animal Spawner")
	void StopSpawning();

	//動物を生成する関数
	UFUNCTION(BlueprintCallable, Category = "Animal Spawner")
	APawn* SpawnAnimal();

	//現在生きている動物の数を返す
	UFUNCTION(BlueprintPure, Category = "Animal Spawner")
	int32 GetSpawnedAnimalCount() const;

private:
	void TrySpawnAnimal();
	void CleanupSpawnedAnimals();

	UPROPERTY()
	TArray<TObjectPtr<APawn>> SpawnedAnimals;

	FTimerHandle SpawnTimerHandle;
};