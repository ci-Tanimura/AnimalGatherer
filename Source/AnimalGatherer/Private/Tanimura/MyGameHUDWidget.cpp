// Fill out your copyright notice in the Description page of Project Settings.


#include "Tanimura/MyGameHUDWidget.h"
#include "Tanimura/MainGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"

// 2026.10.06 Lee start（技能データ購読に必要な依存）
#include "Lee/AnimalGatherPlayerController.h"
#include "Lee/Skill/SkillSystemComponent.h"
#include "Lee/Skill/MatchSkillEffectComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
// 2026.10.06 Lee end
// 2026.10.06 Lee start（スナップショット定義の完全型利用 — IsValid 判定のため）
// GameTypes.h の FSkillSlotSnapshot は USkillDefinition を前方宣言しているのみのため、
// 完全クラス定義は本ファイルで直接 include して取得する
// （ユニティビルドの偶発的な間接 include には依存しない）。
#include "Lee/Skill/SkillDefinition.h"
// 2026.10.06 Lee end（スナップショット定義の完全型利用 — IsValid 判定のため）

void UMyGameHUDWidget::NativeConstruct()
{
	// 2026.10.06 Lee HUD範囲手直し start（再構築に備え先に旧タイマー・自前バインド・キャッシュを解除し、
	//  前回値を未初期化へ戻して初値を必ず再配信する。BP 側 Construct はその後の Super で維持）
	if (GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().ClearTimer(DisplayTimerHandle);
	}
	bDisplayTimerRunning = false;
	UnsubscribeAll();
	LastTimeRemaining = -1;
	LastCountdownRemaining = -1;
	LastP1Score = -1;
	LastP2Score = -1;
	LastSpeedSnapshot = FMatchSpeedSnapshot();
	// 2026.10.06 Lee HUD範囲手直し end

	Super::NativeConstruct();

	// MainGameModeを取得
	AMainGameMode* GameMode = Cast<AMainGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (!GameMode) {
		UE_LOG(LogTemp, Warning, TEXT("UMyGameHUDWidget::NativeConstructで、AMainGameModeを取得できませんでした。"));
		return;
	}
	// スコア変更イベントのバインド
	GameMode->OnTimeChanged.AddUniqueDynamic(this, &UMyGameHUDWidget::UpdateTimerText);
	GameMode->OnScoreChanged.AddUniqueDynamic(this, &UMyGameHUDWidget::UpdateScoreText);
	GameMode->OnTimeUp.AddUniqueDynamic(this, &UMyGameHUDWidget::PlayTimeUpSequence);

	// 2026.10.06 Lee start（購読→初期スナップショットの順で初期化。AddUniqueDynamic により再構築でも二重バインドしない）
	CachedMode = GameMode;
	CachedMode->OnCountdownChanged.AddUniqueDynamic(this, &UMyGameHUDWidget::OnMatchCountdownChanged);

	// 普通対戦のみ技能区を有効化（チュートリアルは非表示・技能イベントは発さない）。
	bSkillPanelsEnabled = CachedMode->IsNormalMatch();
	SetSkillPanelsVisible(bSkillPanelsEnabled);
	if (bSkillPanelsEnabled)
	{
		AcquireSkillComponents();
	}
	PublishInitialSnapshot();
	UpdateDisplayTimer();
	// 2026.10.06 Lee end
}

// 2026.10.06 Lee start（データ購読と表示タイマーの実装）

void UMyGameHUDWidget::NativeDestruct()
{
	// 2026.10.06 Lee start（自前バインドの全解除・タイマー停止・参照解放。HUD 再構築で補填しない）
	if (GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().ClearTimer(DisplayTimerHandle);
	}
	bDisplayTimerRunning = false;
	UnsubscribeAll();
	// 2026.10.06 Lee end

	Super::NativeDestruct();
}

void UMyGameHUDWidget::OnP1SkillStateChanged()
{
	PublishSkillSnapshots(0, false);
	UpdateDisplayTimer();
}

void UMyGameHUDWidget::OnP2SkillStateChanged()
{
	PublishSkillSnapshots(1, false);
	UpdateDisplayTimer();
}

void UMyGameHUDWidget::OnP1SkillUseFailed(int32 SlotIndex, ESkillUseResult Result)
{
	TryShowSkillUseFailure(0, SlotIndex, Result);
}

void UMyGameHUDWidget::OnP2SkillUseFailed(int32 SlotIndex, ESkillUseResult Result)
{
	TryShowSkillUseFailure(1, SlotIndex, Result);
}

// 2026.10.06 Lee HUD範囲手直し start（失敗表示は 0.25 秒間隔に制限。使用結果・クールダウン等の権威値には影響しない）
void UMyGameHUDWidget::TryShowSkillUseFailure(int32 PlayerId, int32 SlotIndex, ESkillUseResult Result)
{
	if (!bSkillPanelsEnabled || PlayerId < 0 || PlayerId > 1)
	{
		return;
	}
	UWorld* World = GetWorld();
	const float Now = (World != nullptr) ? World->GetTimeSeconds() : 0.0f;
	if (Now - LastFailureShowTime[PlayerId] < 0.25f)
	{
		return;
	}
	LastFailureShowTime[PlayerId] = Now;
	ShowSkillUseFailure(PlayerId, SlotIndex, Result);
}
// 2026.10.06 Lee HUD範囲手直し end

void UMyGameHUDWidget::OnSharedSpeedEffectChanged()
{
	PublishSharedSpeed(false);
	UpdateDisplayTimer();
}

void UMyGameHUDWidget::OnMatchCountdownChanged(int32 RemainingCountdown)
{
	if (RemainingCountdown != LastCountdownRemaining)
	{
		LastCountdownRemaining = RemainingCountdown;
		UpdateCountdownText(RemainingCountdown);
	}
}

bool UMyGameHUDWidget::AcquireSkillComponents()
{
	if (!bSkillPanelsEnabled || !IsValid(CachedMode.Get()))
	{
		return false;
	}
	bool bAllBound = true;
	for (int32 i = 0; i < 2; ++i)
	{
		// 2026.10.06 Lee HUD範囲手直し start（無効な弱参照・強参照は IsValid で判定して再取得する。
		//  OwningPlayer に依存せず PlayerId 0/1 の Controller から取得し、身分不一致は誤バインドしない）
		// if (SkillComponents[i] == nullptr)
		// {
		//     AAnimalGatherPlayerController* PC = Cast<AAnimalGatherPlayerController>(UGameplayStatics::GetPlayerController(this, i));
		//     if (PC != nullptr)
		//     {
		//         PlayerControllers[i] = PC;
		//         SkillComponents[i] = PC->FindComponentByClass<USkillSystemComponent>();
		//     }
		// }
		if (!IsValid(SkillComponents[i].Get()))
		{
			AAnimalGatherPlayerController* PC = Cast<AAnimalGatherPlayerController>(UGameplayStatics::GetPlayerController(this, i));
			if (PC != nullptr && PC->GetSkillPlayerId() == static_cast<uint8>(i))
			{
				PlayerControllers[i] = PC;
				SkillComponents[i] = PC->FindComponentByClass<USkillSystemComponent>();
			}
			else
			{
				PlayerControllers[i] = nullptr;
			}
		}
		// 2026.10.06 Lee HUD範囲手直し end
		USkillSystemComponent* SkillSystem = SkillComponents[i].Get();
		if (SkillSystem == nullptr)
		{
			bAllBound = false;
			continue;
		}
		if (i == 0)
		{
			SkillSystem->OnSkillStateChanged.AddUniqueDynamic(this, &UMyGameHUDWidget::OnP1SkillStateChanged);
			SkillSystem->OnSkillUseFailed.AddUniqueDynamic(this, &UMyGameHUDWidget::OnP1SkillUseFailed);
		}
		else
		{
			SkillSystem->OnSkillStateChanged.AddUniqueDynamic(this, &UMyGameHUDWidget::OnP2SkillStateChanged);
			SkillSystem->OnSkillUseFailed.AddUniqueDynamic(this, &UMyGameHUDWidget::OnP2SkillUseFailed);
		}
	}
	// 2026.10.06 Lee HUD範囲手直し start（共有効果も IsValid で判定して再取得）
	// if (SharedEffect == nullptr)
	// {
	//     SharedEffect = CachedMode->GetMatchSkillEffect();
	//     if (SharedEffect != nullptr)
	//     {
	//         SharedEffect->OnSpeedEffectChanged.AddUniqueDynamic(this, &UMyGameHUDWidget::OnSharedSpeedEffectChanged);
	//     }
	// }
	if (!IsValid(SharedEffect.Get()))
	{
		SharedEffect = CachedMode->GetMatchSkillEffect();
	}
	if (SharedEffect != nullptr)
	{
		SharedEffect->OnSpeedEffectChanged.AddUniqueDynamic(this, &UMyGameHUDWidget::OnSharedSpeedEffectChanged);
	}
	else
	{
		bAllBound = false;
	}
	// 2026.10.06 Lee HUD範囲手直し end
	return bAllBound && SharedEffect != nullptr;
}

void UMyGameHUDWidget::PublishInitialSnapshot()
{
	if (CachedMode == nullptr)
	{
		return;
	}

	// 基本表示（時間・スコア・カウントダウン）の初期値。
	const int32 TimeRemaining = CachedMode->GetTimeRemaining();
	if (TimeRemaining != LastTimeRemaining)
	{
		LastTimeRemaining = TimeRemaining;
		UpdateTimerText(TimeRemaining);
	}
	const int32 CurrentP1Score = CachedMode->GetP1Score();
	const int32 CurrentP2Score = CachedMode->GetP2Score();
	if (CurrentP1Score != LastP1Score || CurrentP2Score != LastP2Score)
	{
		LastP1Score = CurrentP1Score;
		LastP2Score = CurrentP2Score;
		UpdateScoreText(CurrentP1Score, CurrentP2Score);
	}
	const int32 CountdownRemaining = CachedMode->GetCountdownRemaining();
	if (CountdownRemaining != LastCountdownRemaining)
	{
		LastCountdownRemaining = CountdownRemaining;
		UpdateCountdownText(CountdownRemaining);
	}

	// 技能・共有効果の初期スナップショット（チュートリアルでは発さない）。
	if (bSkillPanelsEnabled)
	{
		PublishSkillSnapshots(0, true);
		PublishSkillSnapshots(1, true);
		PublishSharedSpeed(true);
	}
}

void UMyGameHUDWidget::PublishSkillSnapshots(int32 PlayerId, bool bForce)
{
	if (!bSkillPanelsEnabled || PlayerId < 0 || PlayerId > 1)
	{
		return;
	}
	USkillSystemComponent* SkillSystem = SkillComponents[PlayerId].Get();
	if (SkillSystem == nullptr)
	{
		return;
	}

	const TArray<FSkillSlotSnapshot> Snapshots = SkillSystem->GetSnapshot();
	const TArray<FSkillSlotSnapshot>& Last = LastPublishedSnapshots[PlayerId];
	bool bChanged = bForce || Last.Num() != Snapshots.Num();
	for (int32 Index = 0; !bChanged && Index < Snapshots.Num(); ++Index)
	{
		bChanged = !IsSameSnapshot(Last[Index], Snapshots[Index]);
	}
	if (bChanged)
	{
		LastPublishedSnapshots[PlayerId] = Snapshots;
		UpdateSkillState(PlayerId, Snapshots);
	}
}

void UMyGameHUDWidget::PublishSharedSpeed(bool bForce)
{
	// 共有効果が無い間は「倍率 1.0・残り 0」の既定スナップショットを表示へ流す。
	FMatchSpeedSnapshot Snapshot;
	if (SharedEffect != nullptr)
	{
		Snapshot = SharedEffect->GetSnapshot();
	}
	if (bForce || !IsSameSpeedSnapshot(Snapshot, LastSpeedSnapshot))
	{
		LastSpeedSnapshot = Snapshot;
		UpdateSharedSpeedEffect(Snapshot);
	}
}

void UMyGameHUDWidget::PollDisplayState()
{
	if (CachedMode == nullptr)
	{
		return;
	}

	// 遅延バインド：未取得なら読み取り専用で取り直す（権威操作は一切行わない）。
	bool bPendingLateBinding = false;
	if (bSkillPanelsEnabled)
	{
		if (SkillComponents[0] == nullptr || SkillComponents[1] == nullptr || SharedEffect == nullptr)
		{
			AcquireSkillComponents();
		}
		bPendingLateBinding = SkillComponents[0] == nullptr || SkillComponents[1] == nullptr || SharedEffect == nullptr;

		PublishSkillSnapshots(0, false);
		PublishSkillSnapshots(1, false);
		PublishSharedSpeed(false);
	}
	UpdateDisplayTimer(bPendingLateBinding);
}

void UMyGameHUDWidget::UpdateDisplayTimer(bool bForceKeep)
{
	bool bNeeded = bForceKeep;
	if (bSkillPanelsEnabled)
	{
		// 2026.10.06 Lee HUD範囲手直し start（未バインド成分がある間は遅延バインド用にポーリングを続ける）
		if (SkillComponents[0] == nullptr || SkillComponents[1] == nullptr || SharedEffect == nullptr)
		{
			bNeeded = true;
		}
		// 2026.10.06 Lee HUD範囲手直し end

		// 2026.10.06 Lee HUD範囲手直し start（LastPublishedSnapshots は配列のため各プレイヤーの全スロットを走査する。旧コードは消さない）
		// for (int32 i = 0; i < 2; ++i)
		// {
		//     const FSkillSlotSnapshot& Last = LastPublishedSnapshots[i];
		//     if (IsValid(Last.Definition.Get()) && Last.CooldownRemaining > 0.0f)
		//     {
		//         bNeeded = true;
		//     }
		// }
		for (int32 i = 0; i < 2; ++i)
		{
			for (const FSkillSlotSnapshot& SlotSnapshot : LastPublishedSnapshots[i])
			{
				if (IsValid(SlotSnapshot.Definition.Get()) && SlotSnapshot.CooldownRemaining > 0.0f)
				{
					bNeeded = true;
				}
			}
		}
		// 2026.10.06 Lee HUD範囲手直し end
	}
	if (LastSpeedSnapshot.RemainingDuration > 0.0f)
	{
		bNeeded = true;
	}

	if (bNeeded && !bDisplayTimerRunning)
	{
		if (UWorld* World = GetWorld())
		{
			bDisplayTimerRunning = true;
			World->GetTimerManager().SetTimer(DisplayTimerHandle,
				FTimerDelegate::CreateWeakLambda(this, [this]()
				{
					PollDisplayState();
				}), 0.1f, true);
		}
	}
	else if (!bNeeded && bDisplayTimerRunning)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(DisplayTimerHandle);
		}
		bDisplayTimerRunning = false;
	}
}

void UMyGameHUDWidget::UnsubscribeAll()
{
	// 2026.10.06 Lee HUD範囲手直し start（参照は IsValid で判定。RemoveDynamic はマクロ文字列化のため三項演算子を使わず明示分岐で全解除）
	// if (CachedMode != nullptr)
	if (IsValid(CachedMode.Get()))
	{
		CachedMode->OnTimeChanged.RemoveDynamic(this, &UMyGameHUDWidget::UpdateTimerText);
		CachedMode->OnScoreChanged.RemoveDynamic(this, &UMyGameHUDWidget::UpdateScoreText);
		CachedMode->OnTimeUp.RemoveDynamic(this, &UMyGameHUDWidget::PlayTimeUpSequence);
		CachedMode->OnCountdownChanged.RemoveDynamic(this, &UMyGameHUDWidget::OnMatchCountdownChanged);
	}
	for (int32 i = 0; i < 2; ++i)
	{
		USkillSystemComponent* SkillSystem = SkillComponents[i].Get();
		if (SkillSystem != nullptr)
		{
			// SkillSystem->OnSkillStateChanged.RemoveDynamic(this,
			//     (i == 0) ? &UMyGameHUDWidget::OnP1SkillStateChanged : &UMyGameHUDWidget::OnP2SkillStateChanged);
			// SkillSystem->OnSkillUseFailed.RemoveDynamic(this,
			//     (i == 0) ? &UMyGameHUDWidget::OnP1SkillUseFailed : &UMyGameHUDWidget::OnP2SkillUseFailed);
			if (i == 0)
			{
				SkillSystem->OnSkillStateChanged.RemoveDynamic(this, &UMyGameHUDWidget::OnP1SkillStateChanged);
				SkillSystem->OnSkillUseFailed.RemoveDynamic(this, &UMyGameHUDWidget::OnP1SkillUseFailed);
			}
			else
			{
				SkillSystem->OnSkillStateChanged.RemoveDynamic(this, &UMyGameHUDWidget::OnP2SkillStateChanged);
				SkillSystem->OnSkillUseFailed.RemoveDynamic(this, &UMyGameHUDWidget::OnP2SkillUseFailed);
			}
		}
		PlayerControllers[i] = nullptr;
		SkillComponents[i] = nullptr;
		LastPublishedSnapshots[i].Reset();
	}
	if (IsValid(SharedEffect.Get()))
	// if (SharedEffect != nullptr)
	{
		SharedEffect->OnSpeedEffectChanged.RemoveDynamic(this, &UMyGameHUDWidget::OnSharedSpeedEffectChanged);
		SharedEffect = nullptr;
	}
	CachedMode = nullptr;
	// 2026.10.06 Lee HUD範囲手直し end
}

float UMyGameHUDWidget::QuantizeForDisplay(float Value)
{
	return FMath::RoundToFloat(Value * 10.0f) / 10.0f;
}

bool UMyGameHUDWidget::IsSameSnapshot(const FSkillSlotSnapshot& A, const FSkillSlotSnapshot& B)
{
	return A.Definition == B.Definition
		&& A.RemainingUses == B.RemainingUses
		&& A.bCanUse == B.bCanUse
		&& FMath::IsNearlyEqual(QuantizeForDisplay(A.CooldownRemaining), QuantizeForDisplay(B.CooldownRemaining))
		&& FMath::IsNearlyEqual(QuantizeForDisplay(A.CooldownDuration), QuantizeForDisplay(B.CooldownDuration));
}

bool UMyGameHUDWidget::IsSameSpeedSnapshot(const FMatchSpeedSnapshot& A, const FMatchSpeedSnapshot& B)
{
	return FMath::IsNearlyEqual(QuantizeForDisplay(A.SpeedMultiplier), QuantizeForDisplay(B.SpeedMultiplier))
		&& FMath::IsNearlyEqual(QuantizeForDisplay(A.RemainingDuration), QuantizeForDisplay(B.RemainingDuration))
		&& FMath::IsNearlyEqual(QuantizeForDisplay(A.EndTime), QuantizeForDisplay(B.EndTime));
}
// 2026.10.06 Lee end