#include "Takeuchi/Actor/AnimalSpawner.h"

#include "Engine/World.h"
#include "TimerManager.h"

AAnimalSpawner::AAnimalSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AAnimalSpawner::BeginPlay()
{
	Super::BeginPlay();

	//設定が有効ならゲーム開始時に自動で生成を始める
	if (bStartSpawningOnBeginPlay)
	{
		StartSpawning();
	}
}

void AAnimalSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopSpawning();
	Super::EndPlay(EndPlayReason);
}

void AAnimalSpawner::StartSpawning()
{
	if (!GetWorld())
	{
		return;
	}

	//開始直後に1体生成し、その後は SpawnInterval 秒ごとに生成を試す
	SpawnAnimal();

	GetWorldTimerManager().SetTimer(SpawnTimerHandle,this,&AAnimalSpawner::TrySpawnAnimal,SpawnInterval,true,SpawnInterval);
}

void AAnimalSpawner::StopSpawning()
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	}
}

void AAnimalSpawner::TrySpawnAnimal()
{
	SpawnAnimal();
}

APawn* AAnimalSpawner::SpawnAnimal()
{
	//すでに消えた動物を数えないように、生成前にリストを整理する
	CleanupSpawnedAnimals();

	if (SpawnedAnimals.Num() >= MaxAnimalCount || !AnimalClass || !GetWorld())
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;

	//指定位置に何かあっても、可能なら位置を調整してスポーンする
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	APawn* SpawnedAnimal = GetWorld()->SpawnActor<APawn>(AnimalClass,SpawnLocation,SpawnRotation,SpawnParams);

	if (SpawnedAnimal)
	{
		//最大数を管理するため、生成した動物を記録する
		SpawnedAnimals.Add(SpawnedAnimal);
	}

	return SpawnedAnimal;
}

int32 AAnimalSpawner::GetSpawnedAnimalCount() const
{
	int32 Count = 0;

	for (const TObjectPtr<APawn>& Animal : SpawnedAnimals)
	{
		if (IsValid(Animal))
		{
			++Count;
		}
	}

	return Count;
}

void AAnimalSpawner::CleanupSpawnedAnimals()
{
	//配列から削除しても添字がずれにくいように、後ろから確認する
	for (int32 Index = SpawnedAnimals.Num() - 1; Index >= 0; --Index)
	{
		if (!IsValid(SpawnedAnimals[Index]))
		{
			SpawnedAnimals.RemoveAtSwap(Index);
		}
	}
}