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
// 2026.10.06 Lee start
class USkillSystemComponent;
// 2026.10.06 Lee end
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
};
