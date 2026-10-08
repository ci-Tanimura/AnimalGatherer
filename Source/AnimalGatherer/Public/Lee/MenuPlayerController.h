// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MenuPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UMenuHUDWidget;
struct FInputActionValue;

/**
 * @brief Title / Result メニュー専用のプレイヤーコントローラ。
 *        対戦用 AAnimalGatherPlayerController の固定カメラ・スキル初期化を持たない。
 *        実行時にメニュー専用 IMC / Input Action を生成し、方向キー / 十字キー / 左スティック
 *        によるボタン選択、A 決定、B 戻る（Result のみ有効）、Esc 終了を受け付ける。
 *        複数 LocalPlayer が同じ UMenuHUDWidget を共有して操作する前提
 *        （Result は対戦後に LocalPlayer が 2 体残るため、片方だけの割当にはしない）。
 *        注意：PlayerTick が入力処理を駆動するため Tick は無効化しないこと。
 */
UCLASS()
class ANIMALGATHERER_API AMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMenuPlayerController();

	//==============================================================================
	// メニュー入力の調整パラメータ
	//==============================================================================

	/** @brief 長押し時の初回リピート開始までの遅延（秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Input", meta = (ClampMin = "0.1", ClampMax = "1.0", Units = "s"))
	float MenuAutoRepeatDelay = 0.3f;

	/** @brief 長押しリピート間隔（秒）。小さいほど早く移る。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Input", meta = (ClampMin = "0.05", ClampMax = "0.5", Units = "s"))
	float MenuAutoRepeatRate = 0.15f;

	/** @brief 左スティック 2D 軸へ適用するデッドゾーン下限（0.0〜0.9）。IMC 生成時に反映される。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Input", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float MenuStickDeadZone = 0.25f;

	//==============================================================================
	// 実行時生成の Enhanced Input アセット（確認用に公開・アセット依存なし）
	//==============================================================================

	/** @brief メニュー選択移動用 Input Action（Axis2D・実行時生成）。 */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Menu Input")
	TObjectPtr<UInputAction> IA_MenuNavigate = nullptr;

	/** @brief メニュー決定用 Input Action（Boolean・実行時生成）。 */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Menu Input")
	TObjectPtr<UInputAction> IA_MenuConfirm = nullptr;

	/** @brief メニュー戻る用 Input Action（Boolean・実行時生成）。 */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Menu Input")
	TObjectPtr<UInputAction> IA_MenuBack = nullptr;

	/** @brief メニュー終了用 Input Action（Boolean・実行時生成）。 */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Menu Input")
	TObjectPtr<UInputAction> IA_MenuQuit = nullptr;

	/** @brief メニュー専用 Input Mapping Context（実行時生成・対戦用 IMC を含まない）。 */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Menu Input")
	TObjectPtr<UInputMappingContext> IMC_Menu = nullptr;

	//==============================================================================
	// 公開メソッド
	//==============================================================================

	/** @brief 本コントローラが操作対象とする共有メニューウィジェットを取得する（未生成時は null）。 */
	UFUNCTION(BlueprintPure, Category = "Menu")
	UMenuHUDWidget* GetMenuWidget() const;

	/** @brief メニュー用の入力モード（GameOnly・マウスカーソル非表示）を（再）適用する。 */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ApplyMenuInputMode();

protected:
	virtual void BeginPlay() override;
	virtual void SetPlayer(UPlayer* InPlayer) override;
	virtual void SetupInputComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	//==============================================================================
	// 入力アセット構築と IMC 登録
	//==============================================================================

	/** @brief IA / IMC を実行時生成する（アセット非依存・冪等）。 */
	void BuildMenuInputAssets();

	/** @brief メニュー専用 IMC を自 LocalPlayer の Enhanced Input サブシステムへ登録する（重複登録なし）。 */
	void RegisterMenuMappingContext();

	/** @brief メニュー専用 IMC の登録を解除する（自 LocalPlayer・切図・終了後の入力残留防止）。 */
	void UnregisterMenuMappingContext();

	/**
	 * @brief 指定 LocalPlayer のサブシステムからメニュー専用 IMC を解除する。
	 * @param InLocalPlayer 解除対象の LocalPlayer。
	 */
	void UnregisterMenuMappingContextFrom(ULocalPlayer* InLocalPlayer);

	//==============================================================================
	// 入力ハンドラ
	//==============================================================================

	/** @brief IA_MenuNavigate Started — 初回移動とリピート開始。 */
	void OnNavigateStarted(const FInputActionValue& Value);

	/** @brief IA_MenuNavigate Triggered — 長押し中の方向更新と変向の即時反映。 */
	void OnNavigateTriggered(const FInputActionValue& Value);

	/** @brief IA_MenuNavigate Completed — リピート停止。 */
	void OnNavigateCompleted(const FInputActionValue& Value);

	/** @brief IA_MenuNavigate Canceled — 焦点喪失等でもリピートを停止。 */
	void OnNavigateCanceled(const FInputActionValue& Value);

	/** @brief 自動リピート用タイマーコールバック。 */
	void OnNavigateAutoRepeat();

	/** @brief IA_MenuConfirm Started — 現在選択ボタンの決定。 */
	void OnConfirmStarted(const FInputActionValue& Value);

	/** @brief IA_MenuBack Started — 戻る操作（ReTitle が無いタイトルでは無操作）。 */
	void OnBackStarted(const FInputActionValue& Value);

	/** @brief IA_MenuQuit Started — QuitGame（メニューウィジェット経由、無ければ直接）。 */
	void OnQuitStarted(const FInputActionValue& Value);

	//==============================================================================
	// 内部ユーティリティ
	//==============================================================================

	/**
	 * @brief 入力ベクトルを選択移動量へ変換する（dominant 軸方式）。
	 * @param Input 入力ベクトル。
	 * @return +1 = 次 / -1 = 前 / 0 = しきい値未満で無操作。
	 */
	int32 ResolveMenuStep(const FVector2D& Input) const;

	/** @brief 入力ベクトルから選択を 1 つ移動する。 */
	void PerformMenuMove(const FVector2D& Input);

	//==============================================================================
	// 内部データ
	//==============================================================================

	/** @brief 長押し中の入力値。 */
	FVector2D HeldNavigateValue = FVector2D::ZeroVector;

	/** @brief 直前に適用した選択移動量（変向の即時反映判定用）。 */
	int32 LastAppliedStep = 0;

	/** @brief 自動リピート用タイマーハンドル。 */
	FTimerHandle NavigateRepeatHandle;

	/** @brief メニュー専用 IMC の登録済みフラグ（Player 再関連付け時の解除判定用）。 */
	bool bMenuIMCRegistered = false;
};
