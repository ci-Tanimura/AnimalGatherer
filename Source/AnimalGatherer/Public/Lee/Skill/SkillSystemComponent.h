// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameTypes.h"
#include "SkillSystemComponent.generated.h"

// 2026.10.06 Lee : プレイヤー別スキルシステムコンポーネント 新規作成（宣言のみ）

class AMainGameMode;
class AMapManager;
class USkillDefinition;

/** @brief いずれかのスロットの状態（回数・クールダウン・有効化）が変化したことの通知。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSkillStateChanged);

/** @brief スキル使用の失敗通知。失敗時は回数・クールダウンへ影響しない。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSkillUseFailed, int32, SlotIndex, ESkillUseResult, Result);

/**
 * @brief 各 Controller に 1 個ずつ装着するスキルスロット管理コンポーネント。
 *        実行時状態（残回数・クールダウン終了時刻）のみを保持し、効果本体は USkillDefinition に委ねる。
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ANIMALGATHERER_API USkillSystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** @brief スロット状態の変化通知（引数なし。受信側は GetSnapshot() を読み直す）。 */
	UPROPERTY(BlueprintAssignable)
	FOnSkillStateChanged OnSkillStateChanged;

	/** @brief 使用失敗通知。失敗したスロット番号と理由を渡す。 */
	UPROPERTY(BlueprintAssignable)
	FOnSkillUseFailed OnSkillUseFailed;

	/**
	 * @brief 本試合のスキルを初期化する。同一局での再呼び出しは冪等で回数を補填しない。
	 *        検証：所有者が AAnimalGatherPlayerController / 身分が有効で一致 /
	 *        InMap が Controller のバインド地図と一致 / InMode が同 World / 定義設定が有効。
	 * @return 初期化に成功した場合 true。
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill System")
	bool InitializeSkills(AMainGameMode* InMode, AMapManager* InMap, uint8 InPlayerId, const TArray<USkillDefinition*>& InDefinitions);

	/**
	 * @brief 指定スロットのスキル使用を試みる（検証順：普通対戦→IsMatchPlaying→有効化→
	 *        スロット→残回数→依存→クールダウン）。成功時のみ回数消費とクールダウン開始。
	 *        bIsUsing ガードにより、効果実行から放送完了までの重入は Busy で拒否する。
	 * @param SlotIndex スロット番号。
	 * @return 実行結果。
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill System")
	ESkillUseResult TryUseSkill(int32 SlotIndex);

	/**
	 * @brief 全スロットのスナップショットを返す。bCanUse は毎回クールダウン終了時刻と
	 *        IsMatchPlaying() から再計算する（HUD 再構築・遅延バインドでも正しい値）。
	 * @return スナップショット配列。
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill System")
	TArray<FSkillSlotSnapshot> GetSnapshot() const;

	/**
	 * @brief 指定スロットのクールダウン残り秒数（権威値 max(0, 終了時刻 - now)）。
	 * @return 残り秒数。無効スロット・クールダウンなしは 0。
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill System")
	float GetCooldownRemaining(int32 SlotIndex) const;

	/** @brief スキル使用の可否を切り替える（回数・クールダウンは保持）。 */
	UFUNCTION(BlueprintCallable, Category = "Skill System")
	void SetSkillsEnabled(bool bInEnabled);

	/** @brief 本試合で初期化済みかどうか。 */
	UFUNCTION(BlueprintCallable, Category = "Skill System")
	bool IsInitialized() const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** @brief 最短満了監視タイマーのコールバック。満了済み終了時刻の整理と通知を行い、残りがあれば再安排。 */
	UFUNCTION()
	void OnCooldownTimerExpired();

	/** @brief 使用処理の重入防止フラグ（効果実行から状態更新・放送完了まで true）。 */
	bool bIsUsing = false;

	/** @brief 本試合で初期化済みか。 */
	bool bInitialized = false;

	/** @brief スキル使用の可否。 */
	bool bEnabled = false;

	/** @brief 所有 Controller と同じ試合の MainGameMode（FSkillContext 用に保持）。 */
	UPROPERTY()
	TObjectPtr<AMainGameMode> BoundMode = nullptr;

	/** @brief バインド済みマップ（FSkillContext::MapManager として渡す）。 */
	UPROPERTY()
	TObjectPtr<AMapManager> BoundMap = nullptr;

	/** @brief 所有プレイヤー身分（0 = 1P, 1 = 2P。255 は無効。FSkillContext::PlayerId と同じ規約）。 */
	UPROPERTY()
	uint8 PlayerId = 255;

	/** @brief 実行時スロット（Definition・RemainingUses・CooldownEndTime）。外部へ参照は渡さない。 */
	UPROPERTY()
	TArray<FSkillSlot> Slots;

	/** @brief 最短満了スロットを監視するタイマー。 */
	FTimerHandle CooldownTimerHandle;

	// 2026.10.06 Lee start（A範囲レビュー修正：使用・スナップショット共通のコンテキスト検証）
	/** @brief 所有 PC の現在地図・現身分・Mode の確定地図が BoundMap/PlayerId と一致するか。 */
	bool IsContextValidForUse() const;
	// 2026.10.06 Lee end（A範囲レビュー修正）
};