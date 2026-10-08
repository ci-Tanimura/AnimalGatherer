// Fill out your copyright notice in the Description page of Project Settings.

#include "Lee/Skill/MatchSkillEffectComponent.h"

#include "Tanimura/MainGameMode.h"

// 2026.10.06 Lee start（直接依存の明示的な include 追加）
#include "Engine/World.h"
#include "TimerManager.h"
#include "Lee/MapManager.h"
// 2026.10.06 Lee end（直接依存の明示的な include 追加）

// 2026.10.06 Lee : 共有試合スキル効果コンポーネント 新規作成
//
// 【次バッチ（MainGameMode 側）で実装される API 契約】
//   - bool AMainGameMode::IsMatchPlaying() const（Playing のみ true、Ready / Ended は false）
// この実装が無い間は本ファイルはコンパイル不可（許容済み）。存在しない機能を偽らない。
// 2026.10.06 Lee 最終審査小修正 : 上記 MainGameMode 側 API は A 範囲で実装済み（備考は原文のまま保持）。

UMatchSkillEffectComponent::UMatchSkillEffectComponent()
{
	// 照会ベース + タイマー監視のみのため、毎フレーム Tick は不要。
	PrimaryComponentTick.bCanEverTick = false;
}

/**
 * @brief 本試合用に初期化する。旧タイマー・効果状態を破棄してからバインドマップを
 *        設定する（冪等）。初期状態を HUD へ通知する。
 * @param InBoundMap 本試合で使用するマップマネージャー。
 */
void UMatchSkillEffectComponent::InitializeForMatch(AMapManager* InBoundMap)
{
	// 2026.10.06 Lee start（同一マップの再初期化では効果・タイマーを壊さない）
	// if (UWorld* World = GetWorld())
	// {
	// 	World->GetTimerManager().ClearTimer(SpeedEffectTimerHandle);
	// }
	// SpeedEffectTimerHandle.Invalidate();
	// SpeedMultiplier = 1.0f;
	// SpeedEffectEndTime = 0.0f;
	// BoundMap = InBoundMap;
	//
	// OnSpeedEffectChanged.Broadcast();
	if (bMatchInitialized && InBoundMap == BoundMap.Get())
	{
		return;
	}
	// 2026.10.06 Lee end（同一マップの再初期化では効果・タイマーを壊さない）

	// 2026.10.06 Lee start（A範囲レビュー修正：新しい地図は sameWorld を検証してからバインドする）
	if (InBoundMap == nullptr || InBoundMap->GetWorld() != GetWorld())
	{
		return;
	}
	// 2026.10.06 Lee end（A範囲レビュー修正）

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpeedEffectTimerHandle);
	}
	SpeedEffectTimerHandle.Invalidate();
	SpeedMultiplier = 1.0f;
	SpeedEffectEndTime = 0.0f;
	BoundMap = InBoundMap;

	// 2026.10.06 Lee start（初期化フラグ設定）
	bMatchInitialized = true;
	// 2026.10.06 Lee end（初期化フラグ設定）

	OnSpeedEffectChanged.Broadcast();
}

/**
 * @brief 共有加速を適用する。
 *        適用条件（所有者が普通対戦 MainGameMode の IsMatchPlaying 中・マップ一致・
 *        正の有限パラメータ）をすべて満たした場合のみ、倍率を上書きし終了時刻を
 *        now + DurationSeconds へ刷新してタイマーを張り直し、変化を通知する。
 * @param TargetMap 対象マップ（バインド済みマップと一致する必要がある）。
 * @param Multiplier 適用する速度倍率。
 * @param DurationSeconds 継続時間（秒）。
 * @return 適用に成功した場合 true。検証失敗時は false（状態は不変）。
 */
bool UMatchSkillEffectComponent::ApplySpeedEffect(AMapManager* TargetMap, float Multiplier, float DurationSeconds)
{
	//==============================================================================
	// 1) 適用条件の検証（失敗時は状態を一切変更しない）
	//==============================================================================
	const AMainGameMode* Mode = Cast<AMainGameMode>(GetOwner());
	if (Mode == nullptr || !Mode->IsMatchPlaying())
	{
		return false;
	}

	if (TargetMap == nullptr || TargetMap != BoundMap.Get())
	{
		return false;
	}
	// 2026.10.06 Lee start（A範囲レビュー修正：sameWorld 検証を追加）
	if (TargetMap->GetWorld() != GetWorld())
	{
		return false;
	}
	// 2026.10.06 Lee end（A範囲レビュー修正）

	// 非有限値（NaN / Inf）と非正値は不許可。
	if (!FMath::IsFinite(Multiplier) || Multiplier <= 0.0f)
	{
		return false;
	}
	if (!FMath::IsFinite(DurationSeconds) || DurationSeconds <= 0.0f)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	//==============================================================================
	// 2) 適用：倍率は上書き（積算しない）、終了時刻は now + Duration へ刷新
	//==============================================================================
	const float Now = World->GetTimeSeconds();
	SpeedMultiplier = Multiplier;
	SpeedEffectEndTime = Now + DurationSeconds;

	// 満了タイマーを刷新。コールバック側で終了時刻を再確認するため、
	// 途中で刷新されても旧コールバックが効果を早切りしない。
	World->GetTimerManager().SetTimer(SpeedEffectTimerHandle, this, &UMatchSkillEffectComponent::OnSpeedEffectTimerExpired, DurationSeconds, false);

	//==============================================================================
	// 3) 変更通知
	//==============================================================================
	OnSpeedEffectChanged.Broadcast();
	return true;
}

/**
 * @brief 指定アクター（マップ）に対する現時点の有効な速度倍率を返す権威照会。
 *        放送で配布したキャッシュに依存せず、毎回現在時刻と試合状態を確認する。
 * @param MapActor 照会対象のマップアクター。
 * @return 現在の有効倍率。マップ不一致・試合非進行中・終了時刻以降は 1.0。
 */
float UMatchSkillEffectComponent::GetSpeedMultiplierForMap(const AActor* MapActor) const
{
	// マップ不一致（未バインドを含む）は即 1.0。
	if (MapActor == nullptr || MapActor != BoundMap.Get())
	{
		return 1.0f;
	}
	// 2026.10.06 Lee start（A範囲レビュー修正：sameWorld 検証を追加）
	if (MapActor->GetWorld() != GetWorld())
	{
		return 1.0f;
	}
	// 2026.10.06 Lee end（A範囲レビュー修正）

	const AMainGameMode* Mode = Cast<AMainGameMode>(GetOwner());
	if (Mode == nullptr || !Mode->IsMatchPlaying())
	{
		return 1.0f;
	}

	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return 1.0f;
	}

	// 権威条件：現在のゲーム時刻が終了時刻より前の間のみ有効（ちょうど一致は無効）。
	if (World->GetTimeSeconds() >= SpeedEffectEndTime)
	{
		return 1.0f;
	}

	return SpeedMultiplier;
}

/**
 * @brief 共有加速状態のスナップショットを返す。
 *        時刻ベースの権威値をそのまま写す（試合終了時の無効化は ClearEffects の責務）。
 * @return 現在の状態スナップショット（無効時は倍率 1.0・残り 0・終了時刻 0）。
 */
FMatchSpeedSnapshot UMatchSkillEffectComponent::GetSnapshot() const
{
	FMatchSpeedSnapshot Snapshot;

	const UWorld* World = GetWorld();
	const float Now = (World != nullptr) ? World->GetTimeSeconds() : 0.0f;

	// 2026.10.06 Lee start（動物側 GetSpeedMultiplierForMap と同じ権威条件で判定し表示を統一）
	// if (Now < SpeedEffectEndTime)
	const AMainGameMode* SnapshotMode = Cast<AMainGameMode>(GetOwner());
	const bool bAuthorityValid = (BoundMap.Get() != nullptr) && (SnapshotMode != nullptr) && SnapshotMode->IsMatchPlaying();
	if (bAuthorityValid && Now < SpeedEffectEndTime)
	// 2026.10.06 Lee end（動物側 GetSpeedMultiplierForMap と同じ権威条件で判定し表示を統一）
	{
		Snapshot.SpeedMultiplier = SpeedMultiplier;
		Snapshot.RemainingDuration = SpeedEffectEndTime - Now;
		Snapshot.EndTime = SpeedEffectEndTime;
	}
	else
	{
		Snapshot.SpeedMultiplier = 1.0f;
		Snapshot.RemainingDuration = 0.0f;
		Snapshot.EndTime = 0.0f;
	}

	return Snapshot;
}

/**
 * @brief 共有効果を即時解除し、タイマーを停止して変化を通知する。
 */
void UMatchSkillEffectComponent::ClearEffects()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpeedEffectTimerHandle);
	}
	SpeedEffectTimerHandle.Invalidate();
	SpeedMultiplier = 1.0f;
	SpeedEffectEndTime = 0.0f;

	OnSpeedEffectChanged.Broadcast();
}

/**
 * @brief 破棄時のクリーンアップ。タイマー停止と状態リセットのみ行い、放送は行わない。
 * @param EndPlayReason 破棄理由。
 */
void UMatchSkillEffectComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpeedEffectTimerHandle);
	}
	SpeedEffectTimerHandle.Invalidate();
	SpeedMultiplier = 1.0f;
	SpeedEffectEndTime = 0.0f;

	Super::EndPlay(EndPlayReason);
}

/**
 * @brief 満了監視タイマーのコールバック。発火時に終了時刻を再確認する。
 *        本当に満了（現在時刻が終了時刻以降）の場合のみ解除して通知する。
 *        効果途中の刷新で終了時刻が未来へずれている場合は、残り時間で張り直す。
 */
void UMatchSkillEffectComponent::OnSpeedEffectTimerExpired()
{
	UWorld* World = GetWorld();
	const float Now = (World != nullptr) ? World->GetTimeSeconds() : 0.0f;

	if (Now >= SpeedEffectEndTime)
	{
		// 本当に満了している場合のみ解除（ちょうど一致も無効扱い）。
		SpeedMultiplier = 1.0f;
		SpeedEffectEndTime = 0.0f;
		SpeedEffectTimerHandle.Invalidate();
		OnSpeedEffectChanged.Broadcast();
	}
	else if (World != nullptr)
	{
		// 旧コールバックが刷新後の効果期間に到着したケース：残り時間で張り直す。
		World->GetTimerManager().SetTimer(SpeedEffectTimerHandle, this, &UMatchSkillEffectComponent::OnSpeedEffectTimerExpired, SpeedEffectEndTime - Now, false);
	}
}