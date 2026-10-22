#include "Tanimura/Actor/AnimalSpawner_New.h"

#include "Engine/World.h"
#include "TimerManager.h"

AAnimalSpawner_New::AAnimalSpawner_New()
{
	PrimaryActorTick.bCanEverTick = false;
}

// 盤面と担当マスを受け取り、スポーンを開始する
void AAnimalSpawner_New::Initialize(const TScriptInterface<IGridInteractInterface>& InGridMap, const FIntPoint InSpawnCell)
{
	GridMap = InGridMap;
	SpawnCell = InSpawnCell;

	if (!HasGridMap()) {
		UE_LOG(LogTemp, Warning, TEXT("AnimalSpawner_New: GridMap is not assigned."));
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr) {
		return;
	}

	// 担当マスへ1匹ずつ出現させ続ける
	FTimerManager& TimerManager = World->GetTimerManager();
	TimerManager.SetTimer(SpawnTimerHandle, this, &AAnimalSpawner_New::SpawnAnimal, SpawnInterval, true);
}

// 動物を1匹出現させる
void AAnimalSpawner_New::SpawnAnimal()
{
	UWorld* World = GetWorld();
	if (!HasGridMap() || AnimalClass == nullptr || World == nullptr) {
		return;
	}

	const FVector SpawnLocation = IGridInteractInterface::Execute_ToWorldCenter(GridMap.GetObject(), SpawnCell);
	const FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLocation);

	// BeginPlayより前に盤面と向きを注入するため、生成を保留する
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.bDeferConstruction = true;

	AAnimalBase_New* Animal = World->SpawnActor<AAnimalBase_New>(AnimalClass, SpawnTransform, SpawnParams);
	if (Animal == nullptr) {
		return;
	}

	Animal->Initialize(GridMap, PickRandomFacing());

	// 注入が済んでから出現させる
	Animal->FinishSpawning(SpawnTransform);
}

// 出現時の向きを移動できる4方向からランダムに選ぶ
EGridDirection AAnimalSpawner_New::PickRandomFacing() const
{
	// 抽選対象は移動できる4方向（Noneは除く）
	static const EGridDirection Directions[] = {
		EGridDirection::Up,
		EGridDirection::Down,
		EGridDirection::Left,
		EGridDirection::Right
	};

	const int32 Index = FMath::RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(Directions)) - 1);
	return Directions[Index];
}

// 盤面が注入されているか
bool AAnimalSpawner_New::HasGridMap() const
{
	return GridMap.GetObject() != nullptr;
}
