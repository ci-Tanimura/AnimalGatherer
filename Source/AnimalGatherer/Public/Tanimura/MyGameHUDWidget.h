// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
// 2026.10.06 Lee start（スキル・共有効果スナップショットの共有型）
#include "GameTypes.h"
// 2026.10.06 Lee end
#include "MyGameHUDWidget.generated.h"

// 2026.10.06 Lee start（購読先の前方宣言）
class AMainGameMode;
class AAnimalGatherPlayerController;
class USkillSystemComponent;
class UMatchSkillEffectComponent;
// 2026.10.06 Lee end

/**
 * ゲーム本編のHUDウィジェット
 * MainGameModeのイベントを購読し、BlueprintImplementableEvent経由でWBP側の表示更新を呼び出す
 * 購読と中継のみを担い、見た目の構築はWBPが担う
 */
UCLASS()
class ANIMALGATHERER_API UMyGameHUDWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	// ウィジェットが階層に追加されるたびに呼ばれる
	virtual void NativeConstruct() override;

	// 残り時間が変わったらテキストを更新するイベント
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Update")
	void UpdateTimerText(int32 RemainingTime);

	// スコアが変わったらテキストを更新するイベント
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Update")
	void UpdateScoreText(int32 P1Score, int32 P2Score);

	// タイムアップ時にゲーム終了演出を開始するイベント
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Update")
	void PlayTimeUpSequence();

	// 2026.10.06 Lee start（表示更新の新規イベントと寿命管理）
	virtual void NativeDestruct() override;

	/** @brief 開始前カウントダウン表示の更新（Mode の OnCountdownChanged を中継）。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Update")
	void UpdateCountdownText(int32 RemainingCountdown);

	/** @brief 指定プレイヤーのスキルスロット状態を表示へ反映する。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Skill")
	void UpdateSkillState(int32 PlayerId, const TArray<FSkillSlotSnapshot>& Snapshots);

	/** @brief 共有加速効果の状態を表示へ反映する。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Skill")
	void UpdateSharedSpeedEffect(const FMatchSpeedSnapshot& Snapshot);

	/** @brief スキル使用失敗を所属プレイヤーの領域に短時間表示する。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Skill")
	void ShowSkillUseFailure(int32 PlayerId, int32 SlotIndex, ESkillUseResult Result);

	/** @brief 両プレイヤーのスキルパネルの表示切替（チュートリアルは非表示）。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Skill")
	void SetSkillPanelsVisible(bool bVisible);

private:
	/** @brief P1 のスロット状態変化ハンドラ。 */
	UFUNCTION()
	void OnP1SkillStateChanged();

	/** @brief P2 のスロット状態変化ハンドラ。 */
	UFUNCTION()
	void OnP2SkillStateChanged();

	/** @brief P1 の使用失敗ハンドラ。 */
	UFUNCTION()
	void OnP1SkillUseFailed(int32 SlotIndex, ESkillUseResult Result);

	/** @brief P2 の使用失敗ハンドラ。 */
	UFUNCTION()
	void OnP2SkillUseFailed(int32 SlotIndex, ESkillUseResult Result);

	/** @brief 共有加速効果の変化ハンドラ。 */
	UFUNCTION()
	void OnSharedSpeedEffectChanged();

	/** @brief Mode のカウントダウン通知を BPE へ中継する。 */
	UFUNCTION()
	void OnMatchCountdownChanged(int32 RemainingCountdown);

	// 2026.10.06 Lee HUD範囲手直し start（失敗表示の連打制限・表示のみで権威値には影響しない）
	/** @brief 使用失敗表示を 0.25 秒間隔に制限して BPE へ中継する。 */
	void TryShowSkillUseFailure(int32 PlayerId, int32 SlotIndex, ESkillUseResult Result);
	// 2026.10.06 Lee HUD範囲手直し end

	/** @brief 0.1 秒表示タイマー：読み取り専用スナップショットで遅延バインドと残り表示を処理する。 */
	void PollDisplayState();

	/** @brief 必要時（CD・効果・未バインド）のみ表示タイマーを動かし、不要時は停止する。 */
	void UpdateDisplayTimer(bool bForceKeep = false);

	/** @brief PlayerId 0/1 の Controller から技能コンポーネントを明示的に取得して購読する。 */
	bool AcquireSkillComponents();

	/** @brief 初期スナップショット（時間・スコア・カウントダウン・技能・共有効果）を表示へ反映する。 */
	void PublishInitialSnapshot();

	/** @brief 指定プレイヤーのスナップショットを変更時のみ BPE へ流す。 */
	void PublishSkillSnapshots(int32 PlayerId, bool bForce);

	/** @brief 共有加速のスナップショットを変更時のみ BPE へ流す。 */
	void PublishSharedSpeed(bool bForce);

	/** @brief 全ての自前バインドを解除し参照を破棄する。 */
	void UnsubscribeAll();

	/** @brief 表示用に 0.1 秒刻みへ量子化する。 */
	static float QuantizeForDisplay(float Value);

	/** @brief スロットスナップショットの表示関連値が同一か。 */
	static bool IsSameSnapshot(const FSkillSlotSnapshot& A, const FSkillSlotSnapshot& B);

	/** @brief 共有加速スナップショットの表示関連値が同一か。 */
	static bool IsSameSpeedSnapshot(const FMatchSpeedSnapshot& A, const FMatchSpeedSnapshot& B);

	/** @brief 購読元の MainGameMode。 */
	UPROPERTY()
	TObjectPtr<AMainGameMode> CachedMode = nullptr;

	/** @brief PlayerId 0/1 の Controller（OwningPlayer には依存しない）。 */
	UPROPERTY()
	TObjectPtr<AAnimalGatherPlayerController> PlayerControllers[2];

	/** @brief 各 Controller の技能コンポーネント。 */
	UPROPERTY()
	TObjectPtr<USkillSystemComponent> SkillComponents[2];

	/** @brief 共有加速効果コンポーネント。 */
	UPROPERTY()
	TObjectPtr<UMatchSkillEffectComponent> SharedEffect = nullptr;

	/** @brief 0.1 秒表示タイマー。 */
	FTimerHandle DisplayTimerHandle;

	/** @brief 技能パネルが有効か（普通対戦のみ true）。 */
	bool bSkillPanelsEnabled = false;

	/** @brief 表示タイマーが動作中か。 */
	bool bDisplayTimerRunning = false;

	/** @brief 前回表示したスナップショット（変化フィルタ用）。 */
	TArray<FSkillSlotSnapshot> LastPublishedSnapshots[2];
	FMatchSpeedSnapshot LastSpeedSnapshot;

	/** @brief 前回表示した基本値（重複 BPE 抑制用）。 */
	int32 LastTimeRemaining = -1;
	int32 LastCountdownRemaining = -1;
	int32 LastP1Score = -1;
	int32 LastP2Score = -1;

	// 2026.10.06 Lee HUD範囲手直し start（失敗表示の前回表示時刻）
	float LastFailureShowTime[2] = { -1.0f, -1.0f };
	// 2026.10.06 Lee HUD範囲手直し end
	// 2026.10.06 Lee end
};