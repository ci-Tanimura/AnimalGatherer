// Fill out your copyright notice in the Description page of Project Settings.

#include "Lee/Skill/SkillSystemComponent.h"

#include "Lee/AnimalGatherPlayerController.h"
#include "Tanimura/MainGameMode.h"

// 2026.10.06 Lee start（A範囲レビュー修正：直接依存の明示的 include）
#include "Lee/Skill/SkillDefinition.h"
#include "Lee/MapManager.h"
#include "Engine/World.h"
#include "TimerManager.h"
// 2026.10.06 Lee end（A範囲レビュー修正）

// 2026.10.06 Lee : プレイヤー別スキルシステムコンポーネント 実装
// 【次バッチ契約（無い間はコンパイル不可・許容済み）】Controller::GetSkillPlayerId / GetMapManager、
//  MainGameMode::IsMatchPlaying / IsNormalMatch / GetMatchSkillEffect
// 2026.10.06 Lee 最終審査小修正 : 上記 MainGameMode 側 API は A 範囲で、Controller 側 API は B 範囲で実装済み（備考は原文のまま保持）。

bool USkillSystemComponent::InitializeSkills(AMainGameMode* InMode, AMapManager* InMap, uint8 InPlayerId, const TArray<USkillDefinition*>& InDefinitions)
{
	// 2026.10.06 Lee start（A範囲レビュー修正：使用処理中の初期化は拒否）
	if (bIsUsing)
	{
		return false;
	}
	// 2026.10.06 Lee end（A範囲レビュー修正）

	// 所有者・身分・地図・モード・定義の検証（失敗時は状態を一切変更しない）。
	AAnimalGatherPlayerController* OwnerPC = Cast<AAnimalGatherPlayerController>(GetOwner());
	if (OwnerPC == nullptr)
	{
		return false;
	}
	if ((InPlayerId != 0 && InPlayerId != 1) || InPlayerId != OwnerPC->GetSkillPlayerId())
	{
		return false; // 無効な身分（255 を含む）は P1 へ暗黙格上げしない。
	}
	if (InMap == nullptr || InMap != OwnerPC->GetMapManager())
	{
		return false;
	}
	if (InMode == nullptr || InMode->GetWorld() == nullptr || InMode->GetWorld() != GetWorld())
	{
		return false;
	}
	// 2026.10.06 Lee start（A範囲レビュー修正：空定義配列・通常対戦以外・地図の World 不一致は拒否）
	if (InDefinitions.Num() == 0)
	{
		return false;
	}
	// 2026.10.08 Lee start（教程拡張：モード判定を IsNormalMatch から初期化支援判定へ変更）
	// if (!InMode->IsNormalMatch())
	// {
	//     return false;
	// }
	if (!InMode->SupportsSkillInitialization())
	{
		return false;
	}
	// 2026.10.08 Lee end（教程拡張）
	if (InMap->GetWorld() != GetWorld())
	{
		return false;
	}
	// 2026.10.06 Lee end（A範囲レビュー修正）
	for (const USkillDefinition* Definition : InDefinitions)
	{
		if (Definition == nullptr || !Definition->IsConfigurationValid())
		{
			return false;
		}
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	// 同一局（Mode・地図・身分・定義がすべて同一）の再呼び出しは冪等：回数・クールダウンを触らない。
	if (bInitialized && BoundMode.Get() == InMode && BoundMap.Get() == InMap && PlayerId == InPlayerId && Slots.Num() == InDefinitions.Num())
	{
		bool bSameDefinitions = true;
		for (int32 Index = 0; Index < InDefinitions.Num(); ++Index)
		{
			if (Slots[Index].Definition.Get() != InDefinitions[Index])
			{
				bSameDefinitions = false;
				break;
			}
		}
		if (bSameDefinitions)
		{
			return true;
		}
	}

	// 2026.10.06 Lee start（A範囲レビュー修正：同一 Mode で内容だけ違う再初期化は拒否（試合中の補填防止））
	if (bInitialized && BoundMode.Get() == InMode)
	{
		return false;
	}
	// 2026.10.06 Lee end（A範囲レビュー修正）

	// 新しい局として初期化：スロットを作り直す。使用可否は SetSkillsEnabled(true) まで無効。
	BoundMode = InMode;
	BoundMap = InMap;
	PlayerId = InPlayerId;
	Slots.Reset();
	Slots.Reserve(InDefinitions.Num());
	for (USkillDefinition* Definition : InDefinitions)
	{
		FSkillSlot Slot;
		Slot.Definition = Definition;
		Slot.RemainingUses = Definition->MaxUses;
		Slot.CooldownEndTime = 0.0f;
		Slots.Add(Slot);
	}
	bInitialized = true;
	bEnabled = false;
	World->GetTimerManager().ClearTimer(CooldownTimerHandle);
	CooldownTimerHandle.Invalidate();
	OnSkillStateChanged.Broadcast();
	return true;
}

ESkillUseResult USkillSystemComponent::TryUseSkill(int32 SlotIndex)
{
	// 重入は Busy で即 return（失敗委譲を発火しない：放送ハンドラからの再帰を断つ）。
	if (bIsUsing)
	{
		return ESkillUseResult::Busy;
	}

	// 検証〜実行〜状態更新〜放送の全部をガード内で行い、放送コールバックの重入も Busy にする。
	TGuardValue<bool> UsingGuard(bIsUsing, true);

	//==============================================================================
	// 検証（失敗時は回数・クールダウンに影響しない）
	//==============================================================================
	ESkillUseResult Result = ESkillUseResult::Success;
	const AMainGameMode* Mode = BoundMode.Get();
	UWorld* World = GetWorld();
	USkillDefinition* Definition = nullptr;
	FSkillSlot* SlotPtr = nullptr;

	if (!bInitialized)
	{
		Result = ESkillUseResult::InvalidSlot;
	}
	else if (!bEnabled)
	{
		Result = ESkillUseResult::NotPlaying;
	}
	// 2026.10.08 Lee start（教程拡張：権限判定をモードの IsSkillUseAllowed（プレイヤー/スロット込み）へ統一。
	//  エラー種別の維持のため Mode null → スロット範囲 → 権限の順で検証する）
	// else if (Mode == nullptr || !Mode->IsNormalMatch() || !Mode->IsMatchPlaying())
	// {
	//     Result = ESkillUseResult::NotPlaying;
	// }
	else if (Mode == nullptr)
	{
		Result = ESkillUseResult::NotPlaying;
	}
	// 2026.10.08 Lee end（教程拡張）
	else if (!Slots.IsValidIndex(SlotIndex))
	{
		Result = ESkillUseResult::InvalidSlot;
	}
	// 2026.10.08 Lee start（教程拡張：プレイヤー/スロット込みの権威判定）
	else if (!Mode->IsSkillUseAllowed(PlayerId, SlotIndex))
	{
		Result = ESkillUseResult::NotPlaying;
	}
	// 2026.10.08 Lee end（教程拡張）
	// 2026.10.06 Lee start（A範囲レビュー修正：実行前に毎回、OwnerPC の現在地図・現身分・Mode の確定地図との一致を検証）
	else if (!IsContextValidForUse())
	{
		Result = ESkillUseResult::MissingDependency;
	}
	// 2026.10.06 Lee end（A範囲レビュー修正）
	else
	{
		SlotPtr = &Slots[SlotIndex];
		Definition = SlotPtr->Definition.Get();
		APlayerController* Caster = Cast<APlayerController>(GetOwner());
		if (Definition == nullptr || !Definition->IsConfigurationValid() || BoundMap.Get() == nullptr || World == nullptr || Caster == nullptr)
		{
			Result = ESkillUseResult::MissingDependency;
		}
		else if (SlotPtr->RemainingUses <= 0)
		{
			Result = ESkillUseResult::NoUses;
		}
		else if (World->GetTimeSeconds() < SlotPtr->CooldownEndTime)
		{
			Result = ESkillUseResult::CoolingDown;
		}

		// 実行：Context を明示的に完全代入して定義へ委譲する（失敗時は何も消費しない）。
		if (Result == ESkillUseResult::Success)
		{
			FSkillContext Context;
			Context.CasterController = Caster;
			Context.PlayerId = PlayerId;
			Context.MapManager = BoundMap.Get();
			Context.MatchSkillEffect = Mode->GetMatchSkillEffect();
			Result = Definition->ExecuteSkill(Context);
		}
	}

	if (Result != ESkillUseResult::Success)
	{
		OnSkillUseFailed.Broadcast(SlotIndex, Result);
		return Result;
	}

	// 成功：回数消費 → クールダウン開始 → 最短満了タイマー再安排 → 状態放送。
	SlotPtr->RemainingUses -= 1;
	const float Now = World->GetTimeSeconds();
	SlotPtr->CooldownEndTime = Now + Definition->CooldownSeconds;

	float EarliestEndTime = FLT_MAX;
	for (const FSkillSlot& Slot : Slots)
	{
		if (Slot.CooldownEndTime > Now && Slot.CooldownEndTime < EarliestEndTime)
		{
			EarliestEndTime = Slot.CooldownEndTime;
		}
	}
	World->GetTimerManager().ClearTimer(CooldownTimerHandle);
	if (EarliestEndTime != FLT_MAX)
	{
		World->GetTimerManager().SetTimer(CooldownTimerHandle, this, &USkillSystemComponent::OnCooldownTimerExpired, EarliestEndTime - Now, false);
	}

	OnSkillStateChanged.Broadcast();

	// 2026.10.08 Lee start（教程用：成功確定後の使用通知。回数消費・クールダウン開始の後に一度だけ放送する）
	OnSkillUsed.Broadcast(SlotIndex);
	// 2026.10.08 Lee end（教程用）

	return ESkillUseResult::Success;
}

TArray<FSkillSlotSnapshot> USkillSystemComponent::GetSnapshot() const
{
	TArray<FSkillSlotSnapshot> Snapshots;
	if (!bInitialized)
	{
		return Snapshots;
	}

	const UWorld* World = GetWorld();
	const float Now = (World != nullptr) ? World->GetTimeSeconds() : 0.0f;
	const AMainGameMode* Mode = BoundMode.Get();
	// 2026.10.06 Lee start（A範囲レビュー修正：スナップショットもコンテキスト有効性を検証）
	// const bool bMatchPlaying = (Mode != nullptr) && Mode->IsMatchPlaying();
	// 2026.10.08 Lee start（教程拡張：権限がスロット別になるため循環外の一律計算を廃止し文脈検証のみ残す）
	// const bool bMatchPlaying = (Mode != nullptr) && Mode->IsMatchPlaying() && IsContextValidForUse();
	const bool bContextValid = IsContextValidForUse();
	// 2026.10.08 Lee end（教程拡張）
	// 2026.10.06 Lee end（A範囲レビュー修正）

	Snapshots.Reserve(Slots.Num());
	// 2026.10.08 Lee start（教程拡張：スロット別権威判定のためインデックス循環へ変更）
	// for (const FSkillSlot& Slot : Slots)
	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		const FSkillSlot& Slot = Slots[SlotIndex];
		FSkillSlotSnapshot Snapshot;
		Snapshot.Definition = Slot.Definition;
		Snapshot.RemainingUses = Slot.RemainingUses;
		const float CooldownRemaining = FMath::Max(0.0f, Slot.CooldownEndTime - Now);
		Snapshot.CooldownRemaining = CooldownRemaining;
		// 2026.10.06 Lee start（A範囲レビュー修正：TObjectPtr のメンバー IsValid ではなくグローバル IsValid を使用）
		// Snapshot.CooldownDuration = (Slot.Definition.IsValid()) ? Slot.Definition->CooldownSeconds : 0.0f;
		Snapshot.CooldownDuration = IsValid(Slot.Definition.Get()) ? Slot.Definition->CooldownSeconds : 0.0f;
		// bCanUse は毎回権威値から再計算する（キャッシュや放送に依存しない）。
		// Snapshot.bCanUse = bMatchPlaying && bEnabled && Slot.Definition.IsValid() && Slot.RemainingUses > 0 && CooldownRemaining <= 0.0f;
		// 2026.10.08 Lee start（教程拡張：モード権限はプレイヤー/スロット込みで毎回権威判定する）
		const bool bMatchPlaying = (Mode != nullptr) && Mode->IsSkillUseAllowed(PlayerId, SlotIndex) && bContextValid;
		// Snapshot.bCanUse = bMatchPlaying && bEnabled && IsValid(Slot.Definition.Get()) && Slot.RemainingUses > 0 && CooldownRemaining <= 0.0f;
		Snapshot.bCanUse = bMatchPlaying && bEnabled && IsValid(Slot.Definition.Get()) && Slot.RemainingUses > 0 && CooldownRemaining <= 0.0f;
		// 2026.10.08 Lee end（教程拡張）
		// 2026.10.06 Lee end（A範囲レビュー修正）
		Snapshots.Add(Snapshot);
	}
	return Snapshots;
}

float USkillSystemComponent::GetCooldownRemaining(int32 SlotIndex) const
{
	if (!bInitialized || !Slots.IsValidIndex(SlotIndex))
	{
		return 0.0f;
	}
	const UWorld* World = GetWorld();
	const float Now = (World != nullptr) ? World->GetTimeSeconds() : 0.0f;
	return FMath::Max(0.0f, Slots[SlotIndex].CooldownEndTime - Now);
}

void USkillSystemComponent::SetSkillsEnabled(bool bInEnabled)
{
	if (bEnabled == bInEnabled)
	{
		return;
	}
	bEnabled = bInEnabled;
	// 回数・クールダウンは保持したまま可否のみ変える。
	OnSkillStateChanged.Broadcast();
}

bool USkillSystemComponent::IsInitialized() const
{
	return bInitialized;
}

void USkillSystemComponent::OnCooldownTimerExpired()
{
	UWorld* World = GetWorld();
	if (World == nullptr || !bInitialized)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	bool bChanged = false;
	float EarliestEndTime = FLT_MAX;
	for (FSkillSlot& Slot : Slots)
	{
		if (Slot.CooldownEndTime > 0.0f && Now >= Slot.CooldownEndTime)
		{
			// 満了済み：終了時刻のみ 0 へ整理する（残回数は触れない）。
			Slot.CooldownEndTime = 0.0f;
			bChanged = true;
		}
		else if (Slot.CooldownEndTime > Now && Slot.CooldownEndTime < EarliestEndTime)
		{
			EarliestEndTime = Slot.CooldownEndTime;
		}
	}

	if (bChanged)
	{
		OnSkillStateChanged.Broadcast();
	}
	if (EarliestEndTime != FLT_MAX)
	{
		World->GetTimerManager().SetTimer(CooldownTimerHandle, this, &USkillSystemComponent::OnCooldownTimerExpired, EarliestEndTime - Now, false);
	}
}

void USkillSystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CooldownTimerHandle);
	}
	CooldownTimerHandle.Invalidate();

	Super::EndPlay(EndPlayReason);
}

// 2026.10.06 Lee start（A範囲レビュー修正：使用・スナップショット共通のコンテキスト検証）
bool USkillSystemComponent::IsContextValidForUse() const
{
	const AAnimalGatherPlayerController* OwnerPC = Cast<AAnimalGatherPlayerController>(GetOwner());
	if (OwnerPC == nullptr)
	{
		return false;
	}
	if (OwnerPC->GetMapManager() != BoundMap.Get() || OwnerPC->GetSkillPlayerId() != PlayerId)
	{
		return false;
	}
	const AMainGameMode* Mode = BoundMode.Get();
	if (Mode == nullptr || Mode->GetMatchMap() != BoundMap.Get())
	{
		return false;
	}
	return true;
}
// 2026.10.06 Lee end（A範囲レビュー修正）