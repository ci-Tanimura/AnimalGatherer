// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameTypes.h"
#include "AnimalGatherPlayerController.generated.h"

class ACursorPawn;
class AMapManager;
class UInputAction;
class UInputMappingContext;
class USoundBase;
class USoundConcurrency;
// 2026.10.06 Lee start
class USkillSystemComponent;
// 2026.10.06 Lee end
// 2026.10.21 Lee start（チュートリアル長押しスキップ：専用 IMC 解除の引数型）
class ULocalPlayer;
// 2026.10.21 Lee end
struct FInputActionValue;

/**
 * @brief グリッドカーソル操作とタイル配置指示を管理するプレイヤーコントローラ。
 *        Enhanced Input で WASD/十字キーによる移動と方向配置を受け付ける。
 *        ローカル2人対戦に対応（各コントローラが独立した IMC を持つ）。
 */
UCLASS()
class ANIMALGATHERER_API AAnimalGatherPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AAnimalGatherPlayerController();

	//==============================================================================
	// Enhanced Input アセット（ブループリントで割り当て）
	//==============================================================================

	/** @brief カーソル移動用 Input Action（Axis2D）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* IA_MoveCursor = nullptr;

	/** @brief 上方向矢印配置用 Input Action（Digital）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* IA_Set_Up = nullptr;

	/** @brief 下方向矢印配置用 Input Action（Digital）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* IA_Set_Down = nullptr;

	/** @brief 左方向矢印配置用 Input Action（Digital）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* IA_Set_Left = nullptr;

	/** @brief 右方向矢印配置用 Input Action（Digital）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* IA_Set_Right = nullptr;

	/** @brief 1P用 Input Mapping Context（WASD + 配置キー）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputMappingContext* IMC_P1 = nullptr;

	/** @brief 2P用 Input Mapping Context（矢印キー + 配置キー）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputMappingContext* IMC_P2 = nullptr;

	// 2026.10.06 Lee start（スキル用の可設定 Action：既存 Enhanced Input ルーティングへの追加のみ）
	/** @brief 反転スキル用 Input Action（LB 想定・Boolean）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* IA_SkillReverse = nullptr;

	/** @brief 加速スキル用 Input Action（RB 想定・Boolean）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* IA_SkillSpeed = nullptr;
	// 2026.10.06 Lee end

	//==============================================================================
	// カーソル自動連続移動設定
	//==============================================================================

	/** @brief 長押し時の初回リピート開始までの遅延（秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|AutoRepeat", meta = (ClampMin = "0.1", ClampMax = "1.0", Units = "s"))
	float AutoRepeatDelay = 0.3f;

	/** @brief リピート間隔（秒）。小さいほど早く動く。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|AutoRepeat", meta = (ClampMin = "0.05", ClampMax = "0.5", Units = "s"))
	float AutoRepeatRate = 0.15f;

	// 2026.10.14 Lee start
	/** @brief 実際にカーソルが移動した時に再生する短いゲーム効果音。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Gameplay")
	TObjectPtr<USoundBase> CursorMoveSound = nullptr;

	/** @brief 矢印配置が確定した時に再生する効果音。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Gameplay")
	TObjectPtr<USoundBase> ArrowPlaceSound = nullptr;

	/** @brief 配置または技能のプレイヤー向け失敗に共用する効果音。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Gameplay")
	TObjectPtr<USoundBase> ActionRejectSound = nullptr;

	/** @brief 反転技能が正常に使用された時の効果音。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Gameplay")
	TObjectPtr<USoundBase> SkillReverseSound = nullptr;

	/** @brief 加速技能が正常に使用された時の効果音。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Gameplay")
	TObjectPtr<USoundBase> SkillSpeedUpSound = nullptr;

	/** @brief カーソル音のプレイヤー別並発設定。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Concurrency")
	TObjectPtr<USoundConcurrency> CursorMoveConcurrency = nullptr;

	/** @brief 配置音のプレイヤー別並発設定。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Concurrency")
	TObjectPtr<USoundConcurrency> ArrowPlaceConcurrency = nullptr;

	/** @brief 配置失敗と技能失敗に共用するプレイヤー別並発設定。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Concurrency")
	TObjectPtr<USoundConcurrency> ActionRejectConcurrency = nullptr;

	/** @brief 技能音のプレイヤー別並発設定。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Concurrency")
	TObjectPtr<USoundConcurrency> SkillConcurrency = nullptr;
	// 2026.10.14 Lee end

	//==============================================================================
	// 固定カメラ設定
	//==============================================================================

	/** @brief 固定カメラを使うか。true なら Pawn の視点でなく指定座標の固定カメラになる。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Control")
	bool bUseFixedCamera = false;

	/** @brief 固定カメラのワールド座標。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Control")
	FVector FixedCameraLocation = FVector::ZeroVector;

	/** @brief 固定カメラの回転。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Control")
	FRotator FixedCameraRotation = FRotator(-90.f, 0.f, 0.f);

	/** @brief 正交投影時の視野幅（ワールド単位）。OrthoWidth が大きいほど広範囲が見える。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Control")
	float FixedCameraOrthoWidth = 2048.f;

	//==============================================================================
	// 公開メソッド
	//==============================================================================

	// 2026.10.06 Lee start（スキルシステム：既定装備コンポーネントと読み取り API）
	/** @brief プレイヤー別スキルスロット管理コンポーネント（既定装備）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill System")
	USkillSystemComponent* SkillSystemComponent = nullptr;

	/** @brief スキルシステムコンポーネントを取得する。 */
	UFUNCTION(BlueprintPure, Category = "Skill System")
	USkillSystemComponent* GetSkillSystemComponent() const;

	/** @brief LocalPlayer の ControllerId からプレイヤー ID を取得する（0 / 1 以外は 255）。 */
	UFUNCTION(BlueprintPure, Category = "Skill System")
	uint8 GetSkillPlayerId() const;
	// 2026.10.06 Lee end

	/**
	 * @brief 現在カーソル位置に指定方向の矢印タイルを配置する。
	 *        MapManager の SetTileData を呼び出し、ビジュアルも即時更新される。
	 * @param Direction 配置する方向（DirUp/DirDown/DirLeft/DirRight）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Gameplay")
	void PlaceDirection(ETileType Direction);

	// 2026.10.06 Lee start（旧インライン実装を cpp へ移動し、有効参照時にスキル初期化を依頼する）
	// /** @brief MapManager 参照を設定する（GameMode から呼ばれることを想定）。 */
	// UFUNCTION(BlueprintCallable, Category = "References")
	// void SetMapManager(AMapManager* InMapManager) { MapManagerRef = InMapManager; } ←元のコードは消さない
	/**
	 * @brief MapManager 参照を設定する（GameMode から呼ばれることを想定）。
	 *        有効な参照が設定された場合、普通対戦のスキル初期化を GameMode へ依頼する。
	 * @param InMapManager 設定するマップ管理アクター。
	 */
	UFUNCTION(BlueprintCallable, Category = "References")
	void SetMapManager(AMapManager* InMapManager);

	/** @brief バインド済みの MapManager 参照を取得する。 */
	UFUNCTION(BlueprintPure, Category = "References")
	AMapManager* GetMapManager() const;
	// 2026.10.06 Lee end

	// 2026.10.06 Lee start（スキル用の受控照会と一時入力のリセット）
	/**
	 * @brief 指定身分の現在有効な配置座標のコピーを返す（最大 3 件）。
	 *        同じ World・同じバインド地図・同じ身分の Controller の実履歴のみを対象とし、
	 *        重複除去・座標有効・非ボーダー・方向タイルかつ所有者一致の照合を行う。
	 *        可変の履歴配列は公開しない。
	 * @param OwnerPlayerId 対象プレイヤー ID（0 = 1P / 1 = 2P。自分自身も指定可能）。
	 * @return 有効な配置座標のコピー（該当なしは空配列）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Gameplay")
	TArray<FIntPoint> GetOwnedPlacedArrowCoords(uint8 OwnerPlayerId) const;

	/**
	 * @brief 連移入力・タイマー・スキル押下状態を即時クリアする（切断・フォーカス喪失向け）。
	 * @param bRequireSkillRelease true の場合、スキルは解放イベント後のみ再武装する。
	 */
	UFUNCTION(BlueprintCallable, Category = "Input")
	void ResetTransientInput(bool bRequireSkillRelease = false);
	// 2026.10.06 Lee end

	// 2026.10.06 Lee start（B2：ビューポート物理層からの解放通知）
	/**
	 * @brief ビューポート物理層の解放確認に基づき、該当スキルの再武装待ちのみを解除する。
	 *        使用回数・クールダウン・入力値には触れない。
	 * @param bSpeed true = 加速（RB）/ false = 反転（LB）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Input")
	void NotifySkillButtonReleased(bool bSpeed);
	// 2026.10.06 Lee end（B2）

	// 2026.10.21 Lee start（チュートリアル長押しスキップ：進行の読み取り専用照会）
	/**
	 * @brief チュートリアル長押しスキップの現在進行（0.0〜1.0）を返す（HUD の表示用。計時はしない）。
	 *        チュートリアル以外のレベルでは常に 0。進行は各 Controller が独立に保持し、
	 *        相手の進行とは合成・接続しない。
	 * @return 連続保持時間 / 必要時間 の割合。
	 */
	UFUNCTION(BlueprintPure, Category = "Input")
	float GetTutorialSkipHoldProgress() const;
	// 2026.10.21 Lee end

	/**
	 * @brief 指定 Tag を持つ CameraActor に視点を切り替える。
	 * @param CameraTag 対象カメラの Actor Tag。
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera Control")
	void SetViewToTaggedCamera(FName CameraTag);

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void SetPlayer(UPlayer* InPlayer) override;

	// 2026.10.21 Lee start（チュートリアル長押しスキップの毎フレーム計時）
	/** @brief 毎フレーム：押下中のみ連続保持時間を加算し、規定時間到達で GameMode へ要求する。 */
	virtual void Tick(float DeltaTime) override;
	// 2026.10.21 Lee end

	// 2026.10.06 Lee start
	/** @brief Pawn 保持時：Cursor の実マップ参照を優先して取り込み、スキル初期化を依頼する。 */
	virtual void OnPossess(APawn* InPawn) override;

	/** @brief 破棄時：連移タイマーを確実に停止する。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// 2026.10.06 Lee end

private:
	/** @brief 固定カメラの Actor をスポーンして SetViewTarget。 */
	void ApplyFixedCamera();

	//==============================================================================
	// 入力ハンドラ
	//==============================================================================

	/** @brief IA_MoveCursor の入力処理。軸方向を読み取りカーソルを1マス移動。 */
	void OnMoveStarted(const FInputActionValue& Value);

	/** @brief IA_MoveCursor Triggered — 長押し中の方向更新。 */
	void OnMoveTriggered(const FInputActionValue& Value);

	/** @brief IA_MoveCursor Completed — 長押し解除。 */
	void OnMoveCompleted(const FInputActionValue& Value);

	/** @brief 自動リピート用タイマーコールバック。 */
	void OnAutoRepeatMove();

	/** @brief IA_Set_Up Started 時の処理。 */
	void OnPlaceUp(const FInputActionValue& Value);

	/** @brief IA_Set_Down Started 時の処理。 */
	void OnPlaceDown(const FInputActionValue& Value);

	/** @brief IA_Set_Left Started 時の処理。 */
	void OnPlaceLeft(const FInputActionValue& Value);

	/** @brief IA_Set_Right Started 時の処理。 */
	void OnPlaceRight(const FInputActionValue& Value);

	//==============================================================================
	// 内部ユーティリティ
	//==============================================================================

	/** @brief 現在 Possess 中の ACursorPawn を取得。 */
	ACursorPawn* GetCursorPawn() const;

	/** @brief 入力ベクトルからカーソルを1マス移動する（内部処理）。 */
	void PerformMoveInDirection(const FVector2D& Input);

	// 2026.10.14 Lee start
	/** @brief プレイヤー操作音を所属 Controller を Owner として再生する。 */
	void PlayGameplaySound(USoundBase* Sound, USoundConcurrency* Concurrency) const;

	/** @brief 配置/技能失敗の共通限頻を通して失敗音を再生する。 */
	void PlayActionRejectSound();

	/** @brief スキル成功イベントを一度だけ音声へ変換する。 */
	UFUNCTION()
	void HandleSkillUsed(int32 SlotIndex);

	/** @brief プレイヤーに通知可能なスキル失敗だけを共通限頻で再生する。 */
	UFUNCTION()
	void HandleSkillUseFailed(int32 SlotIndex, ESkillUseResult Result);
	// 2026.10.14 Lee end

	//==============================================================================
	// 内部データ
	//==============================================================================

	/** @brief マップマネージャへのキャッシュ参照。 */
	UPROPERTY()
	AMapManager* MapManagerRef = nullptr;

	/** @brief 最近3手分の配置座標（FIFO）。4手目で最古を Empty に戻す。 */
	TArray<FIntPoint> PlaceHistory;

	/** @brief 長押し中の入力値。 */
	FVector2D HeldInputValue;

	/** @brief 自動リピート用タイマーハンドル。 */
	FTimerHandle AutoRepeatHandle;

	// 2026.10.14 Lee start
	/** @brief 次に失敗音を許可する World ゲーム時刻。 */
	double NextActionRejectSoundTime = 0.0;
	// 2026.10.14 Lee end

	// 2026.10.06 Lee start（スキル入力と一時状態）
	/** @brief 反転スキル IA の Started 処理（押下 1 回につき 1 回だけ試行）。 */
	void OnSkillReverseStarted(const FInputActionValue& Value);

	/** @brief 加速スキル IA の Started 処理（押下 1 回につき 1 回だけ試行）。 */
	void OnSkillSpeedStarted(const FInputActionValue& Value);

	/** @brief 反転スキル IA の Completed / Canceled 処理（解放で再武装）。 */
	void OnSkillReverseReleased(const FInputActionValue& Value);

	/** @brief 加速スキル IA の Completed / Canceled 処理（解放で再武装）。 */
	void OnSkillSpeedReleased(const FInputActionValue& Value);

	/** @brief 普通対戦の入力段階権限（チュートリアル等は true を返し従来フローを維持）。 */
	bool IsGameplayActionAllowed() const;

	/** @brief 準備が揃っていれば GameMode へスキル初期化を依頼する（冪等性は GameMode 側）。 */
	void TryInitializeMatchSkillsIfReady();

	/** @brief 反転スキル（LB）のスロット番号。 */
	static constexpr int32 SkillSlotIndex_Reverse = 0;

	/** @brief 加速スキル（RB）のスロット番号。 */
	static constexpr int32 SkillSlotIndex_SpeedUp = 1;

	/** @brief 反転スキルの再武装待ち（解放イベント後のみ Started を受付）。 */
	bool bSkillReverseAwaitRelease = false;

	/** @brief 加速スキルの再武装待ち（解放イベント後のみ Started を受付）。 */
	bool bSkillSpeedAwaitRelease = false;
	// 2026.10.06 Lee end

	// 2026.10.21 Lee start（チュートリアル長押しスキップ：実行時生成の専用入力と独立計時）
	//==============================================================================
	// チュートリアル長押しスキップ（MenuPlayerController と同様の実行時生成 IA / IMC。
	// チュートリアル GameMode のレベルでのみ生成・登録し、正式試合・メニューには漏らさない）
	//==============================================================================

	/** @brief 長押しスキップの進行状態。 */
	enum class ETutorialSkipHoldState : uint8
	{
		/** @brief 未押下（進行なし）。 */
		Idle,
		/** @brief 押下中（連続計時中）。 */
		Holding,
		/** @brief 要求済み（しきい値到達後は再計時・再要求しない）。 */
		Triggered
	};

	/** @brief チュートリアル長押しスキップが有効なレベルか（現 GameMode がチュートリアルか）。 */
	bool IsTutorialSkipInputAvailable() const;

	/** @brief スキップ用 IA / IMC を実行時生成する（アセット非依存・冪等）。 */
	void BuildTutorialSkipInputAssets();

	/** @brief スキップ専用 IMC を自 LocalPlayer の Enhanced Input サブシステムへ登録する（重複登録なし）。 */
	void RegisterTutorialSkipMappingContext();

	/** @brief スキップ専用 IMC の登録を解除する（切図・終了後の入力残留防止）。 */
	void UnregisterTutorialSkipMappingContext();

	/**
	 * @brief 指定 LocalPlayer のサブシステムからスキップ専用 IMC を解除する。
	 * @param InLocalPlayer 解除対象の LocalPlayer。
	 */
	void UnregisterTutorialSkipMappingContextFrom(ULocalPlayer* InLocalPlayer);

	/** @brief スキップ IA の Started 処理（連続計時を 0 から開始する）。 */
	void OnTutorialSkipStarted(const FInputActionValue& Value);

	/** @brief スキップ IA の Completed / Canceled 処理（連続進行を破棄する）。 */
	void OnTutorialSkipReleased(const FInputActionValue& Value);

	/** @brief スキップ用 Input Action（手柄 Menu / キーボード Tab・実行時生成）。
	 *         private 欄のため Blueprint アクセス指定子は付けない（UHT の private 属性
	 *         Blueprint アクセス制限のため）。UPROPERTY 登録によりリフレクション経由の参照は可能。 */
	UPROPERTY(Transient, VisibleAnywhere, Category = "Input|TutorialSkip")
	TObjectPtr<UInputAction> IA_TutorialSkip = nullptr;

	/** @brief スキップ専用 Input Mapping Context（実行時生成・対戦用 IMC を含まない。Blueprint 指定子なし）。 */
	UPROPERTY(Transient, VisibleAnywhere, Category = "Input|TutorialSkip")
	TObjectPtr<UInputMappingContext> IMC_TutorialSkip = nullptr;

	/** @brief スキップ成立に必要な連続保持時間（秒）。既定 2 秒。Blueprint 指定子なし。 */
	UPROPERTY(EditAnywhere, Category = "Input|TutorialSkip", meta = (ClampMin = "0.5", ClampMax = "10.0", Units = "s"))
	float TutorialSkipHoldDuration = 2.0f;

	/** @brief 長押し進行状態（Player ごとに独立・相手の進行と合成しない）。 */
	ETutorialSkipHoldState TutorialSkipHoldState = ETutorialSkipHoldState::Idle;

	/** @brief 現在の連続保持時間（秒）。解放・失焦・切断・切図で 0 に戻す。 */
	double TutorialSkipHoldElapsed = 0.0;

	/** @brief スキップ専用 IMC の登録済みフラグ（Player 再関連付け時の解除判定用）。 */
	bool bTutorialSkipIMCRegistered = false;
	// 2026.10.21 Lee end
};
