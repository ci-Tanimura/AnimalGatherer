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

	/**
	 * @brief 現在カーソル位置に指定方向の矢印タイルを配置する。
	 *        MapManager の SetTileData を呼び出し、ビジュアルも即時更新される。
	 * @param Direction 配置する方向（DirUp/DirDown/DirLeft/DirRight）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Gameplay")
	void PlaceDirection(ETileType Direction);

	/** @brief MapManager 参照を設定する（GameMode から呼ばれることを想定）。 */
	UFUNCTION(BlueprintCallable, Category = "References")
	void SetMapManager(AMapManager* InMapManager) { MapManagerRef = InMapManager; }

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
};
