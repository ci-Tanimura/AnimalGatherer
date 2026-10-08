#include "Takeuchi/Actor/AnimalSpawner.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Lee/MapManager.h"
#include "Takeuchi/Pawn/AnimalBase.h"
#include "TimerManager.h"
// 2026.10.06 Lee start（試合段階・本局マップ照会のため MainGameMode への依存を追加）
#include "Tanimura/MainGameMode.h"
// 2026.10.06 Lee end

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

	// 2026.10.06 Lee start（普通対戦では Ready / Ended での生成開始を禁止する。
	// 教程の ScoreGoal からの開始や独立テストの自動開始は従来どおり許可する）
	if (!IsSpawnAllowedByMatch())
	{
		UE_LOG(LogTemp, Verbose, TEXT("AnimalSpawner: match is not Playing or map mismatch. StartSpawning skipped."));
		return;
	}
	// 2026.10.06 Lee end

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
	// 2026.10.06 Lee start（遅延スポーンへ移行。旧実装はコメントとして保持）
	// //すでに消えた動物を数えないように、生成前にリストを整理する
	// CleanupSpawnedAnimals();
	//
	// if (SpawnedAnimals.Num() >= MaxAnimalCount || !AnimalClass || !GetWorld())
	// {
	// 	return nullptr;
	// }
	//
	// FActorSpawnParameters SpawnParams;
	// SpawnParams.Owner = this;
	//
	// //指定位置に何かあっても、可能なら位置を調整してスポーンする
	// SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	//
	// APawn* SpawnedAnimal = GetWorld()->SpawnActor<APawn>(AnimalClass,SpawnLocation,SpawnRotation,SpawnParams);
	//
	// if (SpawnedAnimal)
	// {
	// 	if (AAnimalBase* AnimalBase = Cast<AAnimalBase>(SpawnedAnimal))
	// 	{
	// 		//AnimalBaseが自動取得した参照をnullptrで上書きしない
	// 		if (MapActor)
	// 		{
	// 			AnimalBase->SetMapActor(MapActor);
	// 		}
	// 	}
	//
	// 	//最大数を管理するため、生成した動物を記録する
	// 	SpawnedAnimals.Add(SpawnedAnimal);
	// }
	//
	// return SpawnedAnimal;

	// 生成の都度、試合段階と本局マップ一致を再確認する（タイマー由来の呼び出しも含む）
	if (!IsSpawnAllowedByMatch())
	{
		return nullptr;
	}

	//すでに消えた動物を数えないように、生成前にリストを整理する
	CleanupSpawnedAnimals();

	if (SpawnedAnimals.Num() >= MaxAnimalCount || !AnimalClass || !GetWorld())
	{
		return nullptr;
	}

	// 指定位置に何かあっても、可能なら位置を調整してスポーンする（従来の衝突方針を維持）。
	// BeginPlay より前に最終マップと TileSize を確定させるため、遅延スポーンを使用する
	APawn* SpawnedAnimal = GetWorld()->SpawnActorDeferred<APawn>(
		AnimalClass,
		FTransform(SpawnRotation, SpawnLocation),
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

	if (SpawnedAnimal == nullptr)
	{
		return nullptr;
	}

	if (AAnimalBase* AnimalBase = Cast<AAnimalBase>(SpawnedAnimal))
	{
		//AnimalBaseが自動取得した参照をnullptrで上書きしない
		if (MapActor)
		{
			// FinishSpawning 前に最終マップ参照と TileSize を確定させる
			AnimalBase->SetMapActor(MapActor);
		}
	}

	// FinishSpawning 内で BeginPlay が走る（MapActor 設定済みのため自動取得はスキップされる）
	SpawnedAnimal->FinishSpawning(FTransform(SpawnRotation, SpawnLocation));

	if (IsValid(SpawnedAnimal))
	{
		if (AAnimalBase* AnimalBase = Cast<AAnimalBase>(SpawnedAnimal))
		{
			// 2026.10.06 Lee start（FinishSpawning 後に最終マップを再確認する。
			// BP Construction スクリプトなどが生成前後で MapActor を上書きした場合に、
			// 本局の最終マップへ巻き戻す。同一マップの再設定なら格子履歴は壊されない）
			if (MapActor)
			{
				AnimalBase->SetMapActor(MapActor);
			}
			// 2026.10.06 Lee end（FinishSpawning 後に最終マップを再確認する）

			// 最後に共有加速状態への冪等な接続を行う（新生動物は残り時間を権威照会で引き継ぐ）
			AnimalBase->InitializeMatchSkillEffects();
		}

		//最大数を管理するため、生成した動物を記録する
		SpawnedAnimals.Add(SpawnedAnimal);
	}

	return IsValid(SpawnedAnimal) ? SpawnedAnimal : nullptr;
	// 2026.10.06 Lee end
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

// 2026.10.06 Lee start（普通対戦フローでの生成許可判定の新実装）

/** @brief 普通対戦では Playing 中かつ本局バインドマップ一致の場合のみ生成を許可する。
 *         教程（IsNormalMatch = false）および AMainGameMode を使わない
 *         独立テストレベルは従来どおり常に許可する */
bool AAnimalSpawner::IsSpawnAllowedByMatch() const
{
	const AMainGameMode* GameMode = Cast<AMainGameMode>(UGameplayStatics::GetGameMode(this));
	if (GameMode == nullptr || !GameMode->IsNormalMatch())
	{
		return true;
	}

	// Ready / Ended では生成しない（StartMatch 以降の開始のみ許可）
	if (!GameMode->IsMatchPlaying())
	{
		return false;
	}

	// 本局のバインドマップと一致するスポーナーのみ生成する。
	// 未バインドや別マップ用のスポーナーは本局に影響を与えない
	if (GameMode->GetMatchMap() == nullptr || MapActor != GameMode->GetMatchMap())
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimalSpawner: MapActor does not match the current match map. Spawning skipped."));
		return false;
	}

	return true;
}
// 2026.10.06 Lee end