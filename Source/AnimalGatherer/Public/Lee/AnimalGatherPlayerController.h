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

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void SetPlayer(UPlayer* InPlayer) override;

private:
	//==============================================================================
	// 入力ハンドラ
	//==============================================================================

	/** @brief IA_MoveCursor の入力処理。軸方向を読み取りカーソルを1マス移動。 */
	void OnMoveStarted(const FInputActionValue& Value);

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

	//==============================================================================
	// 内部データ
	//==============================================================================

	/** @brief マップマネージャへのキャッシュ参照。 */
	UPROPERTY()
	AMapManager* MapManagerRef = nullptr;

	/** @brief 最近3手分の配置座標（FIFO）。4手目で最古を Empty に戻す。 */
	TArray<FIntPoint> PlaceHistory;
};
