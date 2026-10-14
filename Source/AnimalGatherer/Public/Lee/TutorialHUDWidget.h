// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameTypes.h"
#include "Lee/TutorialGameMode.h"
#include "TutorialHUDWidget.generated.h"

class UTextBlock;
class UImage;
class UCanvasPanel;
class UOverlay;
class UScaleBox;
class UTutorialGamepadWidget;
class ATutorialGameMode;
class UTexture2D;
class UPanelWidget;
class UWidgetTree;
// 2026.10.08 Lee 返工 start（旧 Root の実型は UWidget のため前方向宣言を追加）
class UWidget;
// 2026.10.08 Lee 返工 end

/**
 * @brief チュートリアル用 HUD の C++ 基底クラス。
 *        C++ 側はデリゲート購読・権威スナップショットの定期読み取り・原生 UMG 呈現を担い、
 *        ブループリント側の BlueprintImplementableEvent は従来どおり残す。
 *        表示のみを行い、技能の初期化・回数・クールダウン・進行フローは一切変更しない。
 */
UCLASS()
class ANIMALGATHERER_API UTutorialHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 2026.10.08 Lee 第三批 start（ゲームパッド輪郭の底図。資産バッチで割り当てる）
	/** @brief パッド輪郭の底図テクスチャ（1536x1024 源図を 768x512 設計で表示）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Tutorial|Gamepad")
	TObjectPtr<UTexture2D> TutorialGamepadBodyTexture = nullptr;
	// 2026.10.08 Lee 第三批 end

protected:
	// 2026.10.08 Lee 第三批 start（Slate 取得前の Root 構築と解放のライフサイクル）
	// 2026.10.08 Lee 返工 start（RebuildWidget の実際の署名は TSharedRef<SWidget> を返す）
	/** @brief Slate 取得前に原生レイアウトを構築してから Super の戻り値をそのまま返す（Root 差し替えはここでのみ）。 */
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// 2026.10.08 Lee 返工 end

	/** @brief 購読と定期読み取りの開始、および現在状態の初回反映。 */
	virtual void NativeConstruct() override;

	/** @brief 購読解除と定期 Timer の停止のみ（Root は破棄しない）。 */
	virtual void NativeDestruct() override;

	/** @brief Slate 解放後に旧 Root へ復元し、次回 RebuildWidget で再構築できるようにする。 */
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	// 2026.10.08 Lee 第三批 end

	// ── デリゲート受信ハンドラ ──

	/** @brief ステップ変化の受信。 */
	UFUNCTION()
	void HandleStepChanged(ETutorialStep NewStep);

	/** @brief プレイヤー達成状態変化の受信。 */
	UFUNCTION()
	void HandlePlayerDone(uint8 PlayerID, bool bDone);

	/** @brief チュートリアル完了の受信。 */
	UFUNCTION()
	void HandleComplete();

	// ── 実装はブループリント側（WBP_Tutorial）── 旧インターフェース（保持） ──

	/** @brief ステップ表示を更新する（説明文の切替 + 両プレイヤーの達成マーク消去）。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Tutorial|Update")
	void ShowStep(ETutorialStep Step);

	/** @brief プレイヤーの達成マークを更新する（0 = 1P 青 / 1 = 2P 赤）。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Tutorial|Update")
	void SetPlayerReady(uint8 PlayerID, bool bReady);

	/** @brief チュートリアル完了演出を表示する。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Tutorial|Update")
	void ShowComplete();

	// ── 2026.10.08 Lee 第三批：スナップショット系の新ブループリントイベント ──

	/**
	 * @brief 参加者（両側）の表示状態の変化を通知する。
	 * @param PlayerId 対象プレイヤーID（0 = 1P / 1 = 2P。OwningPlayer から推測しない）。
	 * @param bActive 現ステップの操作担当か。
	 * @param bWaiting 観察・基線など待ち状態か。
	 * @param ControlHighlight 強調ボタン名（None / DPad / Y / A / X / B / LB / RB / All）。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Tutorial|Update")
	void UpdateTutorialParticipant(uint8 PlayerId, bool bActive, bool bWaiting, FName ControlHighlight);

	/**
	 * @brief 指定プレイヤーの技能スロットのスナップショットを通知する（権威値の写し）。
	 * @param PlayerId 対象プレイヤーID（0 / 1）。
	 * @param Snapshots GetSnapshot() の結果（未初期化時は空配列）。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Tutorial|Update")
	void UpdateTutorialSkillState(uint8 PlayerId, const TArray<FSkillSlotSnapshot>& Snapshots);

	/**
	 * @brief 共有加速効果のスナップショットを通知する。
	 * @param Snapshot GetSnapshot() の結果（効果なしは倍率 1.0・残り 0）。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Tutorial|Update")
	void UpdateTutorialSharedSpeedEffect(const FMatchSpeedSnapshot& Snapshot);

private:
	/** @brief 1920x1080 設計の原生レイアウトを構築する（冪等。旧 Root は折り畳んで保持）。 */
	void BuildNativeLayout();

	/**
	 * @brief 側欄 1 枠分（見出し・パッド早見・任務文言・技能カード）を構築する。
	 * @param Tree この Widget の WidgetTree。
	 * @param Panel 追加先の側欄キャンバス。
	 * @param PlayerId 側のプレイヤーID（0 = 左 / 1 = 右）。
	 */
	void BuildSidePanel(UWidgetTree* Tree, UCanvasPanel* Panel, uint8 PlayerId);

	/** @brief 現在の GameMode / PC / 共有効果を毎回読み直して表示とイベントを更新する。 */
	void RefreshFromGameMode();

	/** @brief ステップ見出しと目標の文言を更新する。 */
	void RefreshStepTexts();

	/** @brief 両側のパッド強調・任務文言を更新し UpdateTutorialParticipant を放送する。 */
	void RefreshParticipants();

	/** @brief 両側の技能カード（画像・残回数・CD）を更新し UpdateTutorialSkillState を放送する。 */
	void RefreshSkillCards();

	/** @brief 共有加速の残り時間を更新し UpdateTutorialSharedSpeedEffect を放送する。 */
	void RefreshSharedEffect();

	/** @brief 参加者表示の算出結果。 */
	struct FParticipantDisplay
	{
		bool bActive = false;
		bool bWaiting = false;
		FName ControlHighlight = TEXT("None");
	};

	/** @brief 指定プレイヤーの現ステップ表示状態（操作担当・待ち・強調ボタン）を算出する。 */
	FParticipantDisplay ComputeParticipantDisplay(uint8 PlayerId) const;

	/** @brief 方向タイルを対応する配置ボタン名へ変換する（Y/A/X/B）。 */
	static FName DirectionToButton(ETileType Direction);

	/** @brief 指定側の任務文言（日次表示用）を算出する。 */
	FText ComputeSideTaskText(uint8 PlayerId) const;

	// ── 原生レイアウトのwidget参照 ──

	/** @brief 構築済みか（冪等判定）。 */
	bool bNativeLayoutBuilt = false;

	// 2026.10.08 Lee 返工 start（WidgetTree.RootWidget の実型は UWidget）
	/** @brief 差し替え前の旧 Root（解放時に復元する）。 */
	UPROPERTY()
	TObjectPtr<UWidget> CachedOldRoot = nullptr;
	// 2026.10.08 Lee 返工 end

	/** @brief 中央上段のステップ見出し。 */
	UPROPERTY()
	TObjectPtr<UTextBlock> StepTitleText = nullptr;

	/** @brief 中央上段の目標一文。 */
	UPROPERTY()
	TObjectPtr<UTextBlock> StepObjectiveText = nullptr;

	/** @brief 共有加速の残り表示（加速系ステップのみ表示）。 */
	UPROPERTY()
	TObjectPtr<UTextBlock> SharedEffectText = nullptr;

	/** @brief 1P 側プレイヤー見出し（左欄）。 */
	UPROPERTY()
	TObjectPtr<UTextBlock> P1TitleText = nullptr;

	/** @brief 2P 側プレイヤー見出し（右欄）。 */
	UPROPERTY()
	TObjectPtr<UTextBlock> P2TitleText = nullptr;

	/** @brief 1P 側パッド早見。 */
	UPROPERTY()
	TObjectPtr<UTutorialGamepadWidget> P1Gamepad = nullptr;

	/** @brief 2P 側パッド早見。 */
	UPROPERTY()
	TObjectPtr<UTutorialGamepadWidget> P2Gamepad = nullptr;

	/** @brief 1P 側任務文言。 */
	UPROPERTY()
	TObjectPtr<UTextBlock> P1TaskText = nullptr;

	/** @brief 2P 側任務文言。 */
	UPROPERTY()
	TObjectPtr<UTextBlock> P2TaskText = nullptr;

	/** @brief 1P 側技能カード格納枠（表示/非表示の単位）。 */
	UPROPERTY()
	TObjectPtr<UPanelWidget> P1SkillCardBox = nullptr;

	/** @brief 2P 側技能カード格納枠。 */
	UPROPERTY()
	TObjectPtr<UPanelWidget> P2SkillCardBox = nullptr;

	/** @brief 1P 側技能カード画像。 */
	UPROPERTY()
	TObjectPtr<UImage> P1SkillCardImage = nullptr;

	/** @brief 2P 側技能カード画像。 */
	UPROPERTY()
	TObjectPtr<UImage> P2SkillCardImage = nullptr;

	/** @brief 1P 側残回数表示。 */
	UPROPERTY()
	TObjectPtr<UTextBlock> P1SkillUsesText = nullptr;

	/** @brief 2P 側残回数表示。 */
	UPROPERTY()
	TObjectPtr<UTextBlock> P2SkillUsesText = nullptr;

	/** @brief 1P 側クールダウン表示。 */
	UPROPERTY()
	TObjectPtr<UTextBlock> P1SkillCooldownText = nullptr;

	/** @brief 2P 側クールダウン表示。 */
	UPROPERTY()
	TObjectPtr<UTextBlock> P2SkillCooldownText = nullptr;

	/** @brief スナップショット定期読み取り用タイマー（0.1 秒）。 */
	FTimerHandle RefreshTimerHandle;
};
