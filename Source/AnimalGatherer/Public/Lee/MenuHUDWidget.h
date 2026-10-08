// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MenuHUDWidget.generated.h"

class APlayerController;
class UButton;
class UWorld;

/**
 * @brief Title / Result 共通メニューの C++ 基底クラス。
 *        WBP_Title / WBP_Result をこのクラスへリペアレントして使う。
 *        同名の BP 変数（Button_Start / Button_ReStart / Button_ReTitle / Button_Quit）は
 *        BindWidgetOptional により自動接続される。
 *        候補ボタンは明示固定順（Start → ReStart → ReTitle → Quit）で並べ、
 *        可視かつ有効なボタンのみ選択対象とする。
 *        選択ハイライトは C++ 側で既定実装（元の背景色を保存 / 復元し、選択色へ着色）。
 *        選択状態は全 PlayerController で共有する（1 つのウィジェットを全員が操作）。
 *        決定時は既存 BP の OnClicked をそのまま発火するため、遷移先の挙動は BP 側実装を流用する。
 *        確定 / 戻る / 終了は単一の切替ロックで抑止する（同一フレームの重複に加え、
 *        実際の切図 / 退出を開始した後の後続要求も拒否する。無操作での B 押下はロックしない）。
 */
UCLASS()
class ANIMALGATHERER_API UMenuHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	//==============================================================================
	// メニュー操作（MenuPlayerController から呼ばれる / BP からも呼び出し可）
	//==============================================================================

	/**
	 * @brief 選択を Step だけ移動する（+1 = 次 / -1 = 前、両端でクランプ・ラップなし）。
	 * @param Step 移動量（0 は無操作）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void MoveMenuSelection(int32 Step);

	/** @brief 現在選択中のボタンを決定する。終了ボタン選択中は退出入口へ直行。同一フレームの重複は無視。 */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ConfirmMenuSelection();

	/** @brief 戻る操作。Button_ReTitle がある場合のみその OnClicked を発火（タイトルでは無操作）。 */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void RequestMenuBack();

	/**
	 * @brief ゲーム終了（QuitGame・SpecificPlayer は所有プレイヤー優先 / 要求元フォールバック）。
	 *        切替ロックを消費してから退出する。
	 * @param Requester 要求元コントローラ。
	 */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void RequestMenuQuit(APlayerController* Requester);

	//==============================================================================
	// 照会
	//==============================================================================

	/** @brief 現在の選択インデックス（ボタン未検出時は -1）。 */
	UFUNCTION(BlueprintPure, Category = "Menu")
	int32 GetSelectedMenuIndex() const { return SelectedIndex; }

	/** @brief 現在選択中のボタン（未検出時は null）。 */
	UFUNCTION(BlueprintPure, Category = "Menu")
	UButton* GetSelectedMenuButton() const;

	/** @brief メニュー候補ボタンの数。 */
	UFUNCTION(BlueprintPure, Category = "Menu")
	int32 GetMenuButtonCount() const { return OrderedButtons.Num(); }

	/**
	 * @brief 指定インデックスのボタンを取得する。
	 * @param Index 対象インデックス（範囲外は null）。
	 */
	UFUNCTION(BlueprintPure, Category = "Menu")
	UButton* GetMenuButton(int32 Index) const;

	/**
	 * @brief 指定 World で現在有効な共有メニューウィジェットを返す（未生成時は null）。
	 *        MenuPlayerController が操作先ウィジェットを解決するための静的アクセサ。
	 * @param InWorld 対象 World。
	 */
	static UMenuHUDWidget* GetMenuWidgetFor(const UWorld* InWorld);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//==============================================================================
	// ボタン（リペアレント後、同名 BP 変数へ自動接続）
	//==============================================================================

	/** @brief タイトルの開始ボタン（OnClicked → LV_Tutorial は既存 BP 実装を維持）。 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Start = nullptr;

	/** @brief リザルトの再戦ボタン（既定選択・OnClicked → LV_MainGame は既存 BP 実装を維持）。 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_ReStart = nullptr;

	/** @brief リザルトのタイトルへ戻るボタン（B 戻るの発火先・既存 BP 実装を維持）。 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_ReTitle = nullptr;

	/** @brief 両メニュー共通の終了ボタン（候補順では常に行末。アセット側で追加する）。
	 *        OnClicked は C++ 側から AddUniqueDynamic で退出入口へ接続する（BP 接線は不要）。 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Quit = nullptr;

	//==============================================================================
	// BP 実装フック（任意。選択ハイライトは C++ 既定実装があるため BP 未接線でも成立する）
	//==============================================================================

	/** @brief 選択変更の通知（追加演出用）。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Menu")
	void HandleSelectionChanged(int32 InSelectedIndex, UButton* SelectedButton);

	/** @brief 決定操作の通知（音等。OnClicked 発火前に呼ばれる）。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Menu")
	void HandleMenuConfirmed(int32 InSelectedIndex, UButton* SelectedButton);

	/** @brief 戻る操作の通知（タイトルへ戻る場合のみ）。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Menu")
	void HandleMenuBack();

	/** @brief 終了操作の通知（QuitGame 呼び出し前に呼ばれる）。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Menu")
	void HandleMenuQuit();

private:
	//==============================================================================
	// 動的バインド（Button_Quit の OnClicked → 退出入口）
	//==============================================================================

	/** @brief Button_Quit が直接クリックされた場合の退出入口（AddUniqueDynamic で接続）。 */
	UFUNCTION()
	void HandleQuitButtonClicked();

	//==============================================================================
	// 内部処理
	//==============================================================================

	/** @brief 明示固定順（Start → ReStart → ReTitle → Quit）で候補ボタンを再構築する。
	 *        可視かつ有効なボタンのみ採用し、Button_Quit の OnClicked を退出入口へ接続する。 */
	void ResolveOrderedButtons();

	/**
	 * @brief 選択インデックスを設定する（クランプ・同値は無操作）。
	 * @param NewIndex 新しいインデックス。
	 * @param bForce 同値でも通知を強制する。
	 */
	void SetSelectedIndex(int32 NewIndex, bool bForce = false);

	/** @brief 選択ハイライトを C++ 既定実装で適用する（旧ボタンへ元色を復元し新ボタンを選択色へ）。 */
	void ApplySelectionHighlight();

	/** @brief 確定 / 戻る / 終了の切替ロック獲得（先着 1 回のみ true。
	 *        切図 / 退出の実開始後は NativeConstruct で初期化されるまで後続要求を拒否する）。 */
	bool TryBeginMenuTransition();

	/**
	 * @brief 終了処理の実体（切替ロック消費済みの単一退出入口）。
	 * @param Requester 要求元コントローラ。
	 */
	void ExecuteMenuQuit(APlayerController* Requester);

	//==============================================================================
	// 内部データ
	//==============================================================================

	/** @brief 表示順に並んだ候補ボタン（Start → ReStart → ReTitle → Quit の採用分のみ・実行時解決）。 */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Menu", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UButton>> OrderedButtons;

	/** @brief ボタン本来の背景色（選択色へ着色する前の保存値）。 */
	UPROPERTY(Transient)
	TMap<TObjectPtr<UButton>, FLinearColor> OriginalColors;

	/** @brief 現在ハイライト中のボタン（実行時解決）。 */
	UPROPERTY(Transient)
	TObjectPtr<UButton> HighlightedButton = nullptr;

	/** @brief 現在の選択インデックス。 */
	int32 SelectedIndex = INDEX_NONE;

	/** @brief 確定 / 戻る / 終了 共通の切替要求済みフラグ（切替開始後の重複・連発抑止用）。 */
	bool bTransitionRequested = false;
};
