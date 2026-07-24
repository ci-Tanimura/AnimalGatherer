#include "Takeuchi/Actor/AnimalSpawner.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Lee/MapManager.h"
#include "Takeuchi/Pawn/AnimalBase.h"
#include "TimerManager.h"

AAnimalSpawner::AAnimalSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AAnimalSpawner::BeginPlay()
{
	Super::BeginPlay();

	//マップ参照が未設定の場合は、レベル上のMapManagerを自動取得する
	if (!MapActor)
	{
		MapActor = UGameplayStatics::GetActorOfClass(GetWorld(), AMapManager::StaticClass());
	}

	if (!MapActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimalSpawner: MapActor is not set."));
	}

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

	//すでに生成タイマーが動いている場合は、開始処理を重複させない
	if (GetWorldTimerManager().IsTimerActive(SpawnTimerHandle))
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
		if (AAnimalBase* AnimalBase = Cast<AAnimalBase>(SpawnedAnimal))
		{
			//AnimalBaseが自動取得した参照をnullptrで上書きしない
			if (MapActor)
			{
				AnimalBase->SetMapActor(MapActor);
			}
		}

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