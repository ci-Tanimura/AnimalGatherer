// Fill out your copyright notice in the Description page of Project Settings.

#include "Lee/AnimalGatherPlayerController.h"
#include "Lee/CursorPawn.h"
#include "Lee/MapManager.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

AAnimalGatherPlayerController::AAnimalGatherPlayerController()
{
}

void AAnimalGatherPlayerController::SetViewToTaggedCamera(FName CameraTag)
{
	if (CameraTag.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("SetViewToTaggedCamera: NO Tag！"));
		return;
	}

	TArray<AActor*> FoundCameras;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), CameraTag, FoundCameras);

	if (FoundCameras.Num() > 0)
	{
		AActor* TargetCamera = FoundCameras[0];
		if (TargetCamera)
		{
			SetViewTarget(TargetCamera);
			UE_LOG(LogTemp, Log, TEXT("SetViewToTaggedCamera: switched to %s"), *TargetCamera->GetName());
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("SetViewToTaggedCamera: cannot find Tag [%s]！"), *CameraTag.ToString());
	}
}

void AAnimalGatherPlayerController::ApplyFixedCamera()
{
	FVector CameraLocation = FixedCameraLocation;

	// MapManager が見つかればマップ中央に自動配置（Z は手動設定値を維持）
	if (MapManagerRef)
	{
		const FVector MapOrigin = MapManagerRef->GetActorLocation();
		const float TileSz = MapManagerRef->TileSize;
		const float CenterX = (MapManagerRef->MapWidth - 1) * TileSz * 0.5f;
		const float CenterY = (MapManagerRef->MapHeight - 1) * TileSz * 0.5f;

		CameraLocation.X = MapOrigin.X + CenterX;
		CameraLocation.Y = MapOrigin.Y + CenterY;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ACameraActor* Cam = GetWorld()->SpawnActor<ACameraActor>(
		ACameraActor::StaticClass(),
		CameraLocation,
		FixedCameraRotation,
		SpawnParams);

	if (Cam)
	{
		// 正交投影に設定（平行投影、歪みなし）
		UCameraComponent* CamComp = Cam->GetCameraComponent();
		if (CamComp)
		{
			CamComp->SetProjectionMode(ECameraProjectionMode::Orthographic);
			CamComp->SetOrthoWidth(FixedCameraOrthoWidth);
		}

		SetViewTarget(Cam);
		UE_LOG(LogTemp, Log, TEXT("ApplyFixedCamera: spawned camera at %s"), *CameraLocation.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ApplyFixedCamera: failed to spawn camera"));
	}
}

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

	// 固定カメラ
	if (bUseFixedCamera)
	{
		ApplyFixedCamera();
	}
}

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

void AAnimalGatherPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EIC)
	{
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

	if (IA_MoveCursor)
	{
		EIC->BindAction(IA_MoveCursor, ETriggerEvent::Started, this, &AAnimalGatherPlayerController::OnMoveStarted);
		EIC->BindAction(IA_MoveCursor, ETriggerEvent::Triggered, this, &AAnimalGatherPlayerController::OnMoveTriggered);
		EIC->BindAction(IA_MoveCursor, ETriggerEvent::Completed, this, &AAnimalGatherPlayerController::OnMoveCompleted);
	}

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

void AAnimalGatherPlayerController::OnMoveStarted(const FInputActionValue& Value)
{
	// 初回押下：入力を記録→移動→自動リピートタイマー開始
	HeldInputValue = Value.Get<FVector2D>();
	PerformMoveInDirection(HeldInputValue);

	GetWorldTimerManager().SetTimer(AutoRepeatHandle, this, &AAnimalGatherPlayerController::OnAutoRepeatMove,
		AutoRepeatRate, true, AutoRepeatDelay);
}

void AAnimalGatherPlayerController::OnMoveTriggered(const FInputActionValue& Value)
{
	// 長押し中の方向変更に対応するため入力値を更新
	HeldInputValue = Value.Get<FVector2D>();
}

void AAnimalGatherPlayerController::OnMoveCompleted(const FInputActionValue& Value)
{
	// キーを離したらリピート停止
	HeldInputValue = FVector2D::ZeroVector;
	GetWorldTimerManager().ClearTimer(AutoRepeatHandle);
}

void AAnimalGatherPlayerController::OnAutoRepeatMove()
{
	if (!HeldInputValue.IsZero())
	{
		PerformMoveInDirection(HeldInputValue);
	}
}

void AAnimalGatherPlayerController::PerformMoveInDirection(const FVector2D& Input)
{
	ACursorPawn* CursorPawn = GetCursorPawn();
	if (!CursorPawn)
	{
		return;
	}

	// X 軸: 正 → GridX-1（左）、負 → GridX+1（右）
	if (FMath::Abs(Input.X) >= FMath::Abs(Input.Y))
	{
		const int32 Delta = Input.X > 0.0f ? -1 : (Input.X < 0.0f ? 1 : 0);
		if (Delta != 0)
		{
			CursorPawn->MoveCursor(Delta, 0);
		}
	}
	else
	{
		// Y 軸: 正 → GridY+1（上）、負 → GridY-1（下）
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

	const FIntPoint TargetCoords(CursorPawn->GridX, CursorPawn->GridY);
	const ETileType CurrentTile = MapManagerRef->GetCellState_Implementation(TargetCoords);

	// 特殊タイル（Spawn / GoalP1 / GoalP2）は上書き不可
	if (CurrentTile == ETileType::Spawn ||
		CurrentTile == ETileType::GoalP1 ||
		CurrentTile == ETileType::GoalP2)
	{
		UE_LOG(LogTemp, Display, TEXT("PlaceDirection: 特殊タイル (%d) は上書きできません"), (int32)CurrentTile);
		return;
	}

	// ボーダータイルには配置不可
	if (MapManagerRef->IsBorderTile(TargetCoords.X, TargetCoords.Y))
	{
		UE_LOG(LogTemp, Display, TEXT("PlaceDirection: ボーダータイル (%d, %d) には配置できません"),
			TargetCoords.X, TargetCoords.Y);
		return;
	}

	// 配置履歴は最大3件（FIFO）、4件目で最古の矢印が Empty に戻る
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
