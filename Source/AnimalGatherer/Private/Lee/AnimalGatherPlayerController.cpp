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

	// IMC の登録は SetPlayer() で行う（Player が確実に設定された後に実行）
}

/**
 * @brief Player が関連付けられた後に呼ばれる。
 *        ここで ControllerId に応じた IMC を登録する。
 *        (動的に生成された P2 では BeginPlay 時点で Player が未設定のため）
 */
void AAnimalGatherPlayerController::SetPlayer(UPlayer* InPlayer)
{
	Super::SetPlayer(InPlayer);

	if (ULocalPlayer* LP = Cast<ULocalPlayer>(InPlayer))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
		{
			int32 ControllerId = LP->GetControllerId();
			UInputMappingContext* SelectedIMC = (ControllerId == 1) ? IMC_P2 : IMC_P1;
			if (SelectedIMC)
			{
				Subsystem->AddMappingContext(SelectedIMC, 0);
				UE_LOG(LogTemp, Log, TEXT("AnimalGatherPC: IMC registered for ControllerId=%d"), ControllerId);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("AnimalGatherPC: SelectedIMC is null for ControllerId=%d"), ControllerId);
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
		// 動的生成された P2 では InputComponent が UEnhancedInputComponent に
		// 自動アップグレードされない場合があるため、手動で再作成する
		UE_LOG(LogTemp, Warning, TEXT("AnimalGatherPC: InputComponent が Enhanced ではない (%s) → 再作成します"),
			InputComponent ? *InputComponent->GetClass()->GetName() : TEXT("null"));

		if (InputComponent)
		{
			InputComponent->DestroyComponent();
		}
		InputComponent = NewObject<UEnhancedInputComponent>(this, TEXT("PC_InputComponent0"));
		InputComponent->RegisterComponent();

		EIC = Cast<UEnhancedInputComponent>(InputComponent);
		if (!EIC)
		{
			UE_LOG(LogTemp, Error, TEXT("AnimalGatherPC: UEnhancedInputComponent の作成に失敗"));
			return;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("AnimalGatherPC: SetupInputComponent → InputComponent=%s, PlayerID will be set by pawn"),
		*EIC->GetName());

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
	// 注：カメラ俯視により MapManager の X 軸と画面左右が逆。DeltaX を反転して補正。
	if (FMath::Abs(Input.X) >= FMath::Abs(Input.Y))
	{
		// X 軸優先（左右）
		const int32 Delta = Input.X > 0.0f ? -1 : (Input.X < 0.0f ? 1 : 0);
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
