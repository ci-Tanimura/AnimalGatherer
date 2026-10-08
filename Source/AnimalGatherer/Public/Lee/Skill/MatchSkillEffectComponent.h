// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameTypes.h"
#include "MatchSkillEffectComponent.generated.h"

// 2026.10.06 Lee : 共有試合スキル効果コンポーネント（本局唯一の加速状態）新規作成

class AMapManager;

/** @brief 共有加速状態の変化を HUD 等へ通知するデリゲート（引数なし。受信側は GetSnapshot() を読み直す）。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSpeedEffectChanged);

/**
 * @brief 普通対戦の MainGameMode に装着し、本局のバインド済みマップに対する
 *        全場共有の加速状態（倍率・終了時刻）を唯一保持するアクターコンポーネント。
 *        - 時刻は同じ World の GetTimeSeconds（ゲーム時間）基準。
 *        - 倍率は積算しない（再使用時は倍率を上書きし、終了時刻のみ刷新）。
 *        - 動物や HUD への値の配布は放送キャッシュに依存せず、常に本コンポーネントへ照会する。
 *        - 動物の列挙と倍率の書き込みは行わない。動物側は毎回 GetSpeedMultiplierForMap() で
 *          権威照会し、各自のアニメ入口へ通知する。
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ANIMALGATHERER_API UMatchSkillEffectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMatchSkillEffectComponent();

	/** @brief 共有加速状態の変化通知（引数なし）。受信側は GetSnapshot() で現在状態を読み直す。 */
	UPROPERTY(BlueprintAssignable)
	FOnSpeedEffectChanged OnSpeedEffectChanged;

	/**
	 * @brief 本試合用に初期化する。バインドマップを設定し、旧効果・タイマーを破棄して
	 *        本局の初期状態に戻す（同一局での再呼び出しも冪等）。
	 * @param InBoundMap 本試合で使用するマップマネージャー。
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill Effect")
	void InitializeForMatch(AMapManager* InBoundMap);

	/**
	 * @brief 共有加速を適用する。既に効果がある場合は倍率の上書きと終了時刻の刷新のみ
	 *        行い、倍率は積算しない。
	 *        適用条件：所有者が普通対戦の MainGameMode で IsMatchPlaying() 中、
	 *        指定マップがバインド済みマップと一致、倍率・時間が正の有限値。
	 *        検証に失敗した場合は状態を一切変更しない。
	 * @param TargetMap 対象マップ（バインド済みマップと一致する必要がある）。
	 * @param Multiplier 適用する速度倍率（正の有限値）。
	 * @param DurationSeconds 継続時間（秒・正の有限値）。
	 * @return 適用に成功した場合 true。
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill Effect")
	bool ApplySpeedEffect(AMapManager* TargetMap, float Multiplier, float DurationSeconds);

	/**
	 * @brief 指定アクター（マップ）に対する現時点の有効な速度倍率を返す権威照会。
	 *        現在時刻と IsMatchPlaying() をその都度確認する。マップ不一致・試合非進行中・
	 *        終了時刻以降（ちょうど一致を含む）は 1.0 を返す。
	 * @param MapActor 照会対象のマップアクター。
	 * @return 現在の有効倍率（無効時は 1.0）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill Effect")
	float GetSpeedMultiplierForMap(const AActor* MapActor) const;

	/**
	 * @brief 共有加速状態のスナップショットを返す（HUD 表示用）。
	 *        効果が無い・既に切れている場合は倍率 1.0・残り時間 0・終了時刻 0 のスナップショット。
	 * @return 現在の状態スナップショット。
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill Effect")
	FMatchSpeedSnapshot GetSnapshot() const;

	/**
	 * @brief 共有効果を即時解除する（タイマー停止・倍率 1.0・終了時刻 0 へ戻して変化を通知）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill Effect")
	void ClearEffects();

protected:
	/**
	 * @brief 破棄時のクリーンアップ。満了タイマーを停止し状態を初期値へ戻す
	 *        （破棄経路では変化通知を放送しない）。
	 * @param EndPlayReason 破棄理由。
	 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * @brief 共有加速の満了監視タイマーコールバック。
	 *        発火時に終了時刻を再確認し、本当に満了している場合のみ解除して通知する。
	 *        効果途中に終了時刻が刷新されていた場合は、残り時間でタイマーを張り直す。
	 */
	UFUNCTION()
	void OnSpeedEffectTimerExpired();

	/** @brief 本試合でバインド中のマップマネージャー。 */
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AMapManager> BoundMap = nullptr;

	/** @brief 現在の共有速度倍率（効果なしは 1.0）。 */
	UPROPERTY(BlueprintReadOnly)
	float SpeedMultiplier = 1.0f;

	/** @brief 共有効果の終了時刻（同 World の GetTimeSeconds 基準）。効果なしは 0 以下。 */
	UPROPERTY(BlueprintReadOnly)
	float SpeedEffectEndTime = 0.0f;

	// 2026.10.06 Lee start（同一マップ再初期化の判定用フラグ追加）
	/** @brief 試合初期化済みフラグ。同一マップの再初期化で効果を壊さないために使用。 */
	bool bMatchInitialized = false;
	// 2026.10.06 Lee end（同一マップ再初期化の判定用フラグ追加）

	/** @brief 共有加速の満了監視タイマーハンドル。 */
	FTimerHandle SpeedEffectTimerHandle;
};