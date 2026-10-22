#include "Tanimura/Actor/AnimalSpawner_New.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Lee/MapManager.h"
#include "Tanimura/MainGameMode.h"
#include "TimerManager.h"

AAnimalSpawner_New::AAnimalSpawner_New()
{
	PrimaryActorTick.bCanEverTick = false;
}

// 盤面と担当マスを受け取る
void AAnimalSpawner_New::Initialize(const TScriptInterface<IGridInteractInterface>& InGridMap, const FIntPoint InSpawnCell)
{
	GridMap = InGridMap;
	SpawnCell = InSpawnCell;

	if (!HasGridMap()) {
		UE_LOG(LogTemp, Warning, TEXT("AnimalSpawner_New: GridMap is not assigned."));
	}
}

// 出現処理を開始する
void AAnimalSpawner_New::StartSpawning()
{
	UWorld* World = GetWorld();
	if (World == nullptr || !HasGridMap()) {
		return;
	}

	// 対戦外や別マップでは出現させない
	if (!IsSpawnAllowedByMatch()) {
		return;
	}

	FTimerManager& TimerManager = World->GetTimerManager();
	if (TimerManager.IsTimerActive(SpawnTimerHandle)) {
		return;
	}

	// 開始直後に1匹出し、以降は一定間隔で出現させる
	SpawnAnimal();
	TimerManager.SetTimer(SpawnTimerHandle, this, &AAnimalSpawner_New::SpawnAnimal, SpawnInterval, true, SpawnInterval);
}

// 出現処理を停止する
void AAnimalSpawner_New::StopSpawning()
{
	if (UWorld* World = GetWorld()) {
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
	}
}

void AAnimalSpawner_New::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopSpawning();
	Super::EndPlay(EndPlayReason);
}

// 動物を1匹出現させる
void AAnimalSpawner_New::SpawnAnimal()
{
	UWorld* World = GetWorld();
	if (World == nullptr || !HasGridMap() || AnimalClass == nullptr) {
		return;
	}

	// 出現の都度、対戦段階とマップ一致を再確認する
	if (!IsSpawnAllowedByMatch()) {
		return;
	}

	// 破棄済みの動物を除いてから同時数を確認する
	CleanupSpawnedAnimals();
	if (SpawnedAnimals.Num() >= MaxAnimalCount) {
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

	if (!IsValid(Animal)) {
		return;
	}

	// 生成後に確定したマップで速度倍率へ接続する
	Animal->InitializeMatchSkillEffects();
	SpawnedAnimals.Add(Animal);
}

// 破棄済みの動物を記録から除く
void AAnimalSpawner_New::CleanupSpawnedAnimals()
{
	// 添字がずれないよう後ろから確認する
	for (int32 Index = SpawnedAnimals.Num() - 1; Index >= 0; --Index) {
		if (!IsValid(SpawnedAnimals[Index])) {
			SpawnedAnimals.RemoveAtSwap(Index);
		}
	}
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

// 対戦フェーズが出現を許可するか
bool AAnimalSpawner_New::IsSpawnAllowedByMatch() const
{
	const AMainGameMode* GameMode = Cast<AMainGameMode>(UGameplayStatics::GetGameMode(this));
	if (GameMode == nullptr || !GameMode->IsNormalMatch()) {
		return true;
	}

	// Ready / Endedでは出現させない
	if (!GameMode->IsMatchPlaying()) {
		return false;
	}

	// 本局のバインドマップと一致するスポーナーのみ出現させる
	if (GameMode->GetMatchMap() == nullptr || GetMapActor() != GameMode->GetMatchMap()) {
		return false;
	}

	return true;
}

// 盤面アクターを返す
AActor* AAnimalSpawner_New::GetMapActor() const
{
	return Cast<AActor>(GridMap.GetObject());
}

// 盤面が注入されているか
bool AAnimalSpawner_New::HasGridMap() const
{
	return GridMap.GetObject() != nullptr;
}
