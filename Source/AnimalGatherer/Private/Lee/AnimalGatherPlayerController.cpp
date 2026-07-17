// Fill out your copyright notice in the Description page of Project Settings.

#include "Lee/AnimalGatherPlayerController.h"
#include "Lee/CursorPawn.h"
#include "Lee/MapManager.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Engine/World.h"
#include "EngineUtils.h"

AAnimalGatherPlayerController::AAnimalGatherPlayerController()
{
}

/**
 * @brief ゲーム開始時。MapManager を検索し、このプレイヤーの Input Mapping Context を登録する。
 */
void AAnimalGatherPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// MapManager をワールドから検索してキャッシュ
	if (!MapManagerRef)
	{
		for (TActorIterator<AMapManager> It(GetWorld()); It; ++It)
		{
			MapManagerRef = *It;
			break;
		}

		if (!MapManagerRef)
		{
			UE_LOG(LogTemp, Warning, TEXT("AnimalGatherPlayerController: MapManager が見つかりません"));
		}
	}

	// Enhanced Input Mapping Context をこのプレイヤー専用に追加
	if (APlayerController* PC = Cast<APlayerController>(this))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (IMC_Default)
			{
				Subsystem->AddMappingContext(IMC_Default, 0);
			}
		}
	}
}

/**
 * @brief Enhanced Input アクションのバインド。
 *        移動は Started で1マス移動、配置は各方向のアクションに個別バインド。
 */
void AAnimalGatherPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EIC)
	{
		return;
	}

	// カーソル移動（Axis2D → 支配的な軸方向に1マス移動）
	if (IA_MoveCursor)
	{
		EIC->BindAction(IA_MoveCursor, ETriggerEvent::Started, this, &AAnimalGatherPlayerController::OnMoveStarted);
	}

	// 方向矢印の配置（各方向個別の Digital アクション）
	if (IA_Set_Up)
	{
		EIC->BindAction(IA_Set_Up, ETriggerEvent::Started, this, &AAnimalGatherPlayerController::OnPlaceUp);
	}
	if (IA_Set_Down)
	{
		EIC->BindAction(IA_Set_Down, ETriggerEvent::Started, this, &AAnimalGatherPlayerController::OnPlaceDown);
	}
	if (IA_Set_Left)
	{
		EIC->BindAction(IA_Set_Left, ETriggerEvent::Started, this, &AAnimalGatherPlayerController::OnPlaceLeft);
	}
	if (IA_Set_Right)
	{
		EIC->BindAction(IA_Set_Right, ETriggerEvent::Started, this, &AAnimalGatherPlayerController::OnPlaceRight);
	}
}

//==============================================================================
// 入力ハンドラ
//==============================================================================

/**
 * @brief カーソル移動入力ハンドラ。
 *        2D 軸入力から支配的な軸方向を判定し、その方向に1マス移動する。
 */
void AAnimalGatherPlayerController::OnMoveStarted(const FInputActionValue& Value)
{
	ACursorPawn* CursorPawn = GetCursorPawn();
	if (!CursorPawn)
	{
		return;
	}

	const FVector2D Input = Value.Get<FVector2D>();

	// 支配的な軸を選択（斜め入力を防止）
	if (FMath::Abs(Input.X) >= FMath::Abs(Input.Y))
	{
		// X 軸優先（左右）
		const int32 Delta = Input.X > 0.0f ? 1 : (Input.X < 0.0f ? -1 : 0);
		if (Delta != 0)
		{
			CursorPawn->MoveCursor(Delta, 0);
		}
	}
	else
	{
		// Y 軸優先（上下）
		const int32 Delta = Input.Y > 0.0f ? 1 : (Input.Y < 0.0f ? -1 : 0);
		if (Delta != 0)
		{
			CursorPawn->MoveCursor(0, Delta);
		}
	}
}

void AAnimalGatherPlayerController::OnPlaceUp(const FInputActionValue& Value)
{
	PlaceDirection(ETileType::DirUp);
}

void AAnimalGatherPlayerController::OnPlaceDown(const FInputActionValue& Value)
{
	PlaceDirection(ETileType::DirDown);
}

void AAnimalGatherPlayerController::OnPlaceLeft(const FInputActionValue& Value)
{
	PlaceDirection(ETileType::DirLeft);
}

void AAnimalGatherPlayerController::OnPlaceRight(const FInputActionValue& Value)
{
	PlaceDirection(ETileType::DirRight);
}

//==============================================================================
// 公開メソッド
//==============================================================================

/**
 * @brief 現在カーソルが位置するタイルに指定方向の矢印を配置する。
 *        Spawn / Goal タイルは上書きしない。
 */
void AAnimalGatherPlayerController::PlaceDirection(ETileType Direction)
{
	ACursorPawn* CursorPawn = GetCursorPawn();
	if (!CursorPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlaceDirection: CursorPawn が存在しません"));
		return;
	}

	if (!MapManagerRef)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlaceDirection: MapManagerRef が未設定です"));
		return;
	}

	// Spawn / Goal タイルの保護（必要に応じてコメントアウトで許可）
	const FIntPoint TargetCoords(CursorPawn->GridX, CursorPawn->GridY);
	const ETileType CurrentTile = MapManagerRef->GetCellState_Implementation(TargetCoords);

	if (CurrentTile == ETileType::Spawn ||
		CurrentTile == ETileType::GoalP1 ||
		CurrentTile == ETileType::GoalP2)
	{
		UE_LOG(LogTemp, Display, TEXT("PlaceDirection: 特殊タイル (%d) は上書きできません"), (int32)CurrentTile);
		return;
	}

	// 3手制限：4手目以降は最古の配置を Empty に戻す
	static constexpr int32 MaxHistory = 3;
	if (PlaceHistory.Num() >= MaxHistory)
	{
		const FIntPoint Oldest = PlaceHistory[0];
		PlaceHistory.RemoveAt(0);
		MapManagerRef->SetTileData(Oldest.X, Oldest.Y, ETileType::Empty);
	}

	MapManagerRef->SetTileData(TargetCoords.X, TargetCoords.Y, Direction);
	PlaceHistory.Add(TargetCoords);
}

//==============================================================================
// 内部ユーティリティ
//==============================================================================

ACursorPawn* AAnimalGatherPlayerController::GetCursorPawn() const
{
	return Cast<ACursorPawn>(GetPawn());
}
