// Fill out your copyright notice in the Description page of Project Settings.

#include "Lee/AnimalGatherPlayerController.h"
#include "Lee/CursorPawn.h"
#include "Lee/MapManager.h"
// 2026.10.02 Lee start
#include "Lee/TutorialGameMode.h"
// 2026.10.02 Lee end
// 2026.10.06 Lee start
#include "Containers/Set.h"
#include "Lee/Skill/SkillSystemComponent.h"
#include "Tanimura/MainGameMode.h"
// 2026.10.06 Lee end
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
	// 2026.10.06 Lee start（プレイヤー別スキルコンポーネントを既定装備）
	SkillSystemComponent = CreateDefaultSubobject<USkillSystemComponent>(TEXT("SkillSystemComponent"));
	// 2026.10.06 Lee end
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

	// 2026.10.06 Lee start（地図検索後に普通対戦のスキル初期化を依頼：未整備時は GameMode 側で再試行）
	TryInitializeMatchSkillsIfReady();
	// 2026.10.06 Lee end
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

	// 2026.10.06 Lee start（Player 関連付け後にもスキル初期化を依頼：地図未バインド時は GameMode 側で判定する）
	TryInitializeMatchSkillsIfReady();
	// 2026.10.06 Lee end
}

// 2026.10.06 Lee start（旧インライン実装を cpp へ移動）
void AAnimalGatherPlayerController::SetMapManager(AMapManager* InMapManager)
{
	// MapManagerRef = InMapManager; ←元のコードは消さない
	MapManagerRef = InMapManager;

	// 有効な参照が設定された場合のみ、普通対戦のスキル初期化を依頼する。
	if (IsValid(InMapManager))
	{
		TryInitializeMatchSkillsIfReady();
	}
}

AMapManager* AAnimalGatherPlayerController::GetMapManager() const
{
	// バインド済みマップをそのまま返す。
	return MapManagerRef;
}
// 2026.10.06 Lee end

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
		// 2026.10.06 Lee start（フォーカス喪失・切断時の Canceled でも連移を停止する）
		EIC->BindAction(IA_MoveCursor, ETriggerEvent::Canceled, this, &AAnimalGatherPlayerController::OnMoveCompleted);
		// 2026.10.06 Lee end
	}

	// 2026.10.06 Lee start（スキル 2 Action：Started は押下 1 回につき 1 回のみ試行、Completed / Canceled で解放）
	if (IA_SkillReverse)
	{
		EIC->BindAction(IA_SkillReverse, ETriggerEvent::Started, this, &AAnimalGatherPlayerController::OnSkillReverseStarted);
		EIC->BindAction(IA_SkillReverse, ETriggerEvent::Completed, this, &AAnimalGatherPlayerController::OnSkillReverseReleased);
		EIC->BindAction(IA_SkillReverse, ETriggerEvent::Canceled, this, &AAnimalGatherPlayerController::OnSkillReverseReleased);
	}
	if (IA_SkillSpeed)
	{
		EIC->BindAction(IA_SkillSpeed, ETriggerEvent::Started, this, &AAnimalGatherPlayerController::OnSkillSpeedStarted);
		EIC->BindAction(IA_SkillSpeed, ETriggerEvent::Completed, this, &AAnimalGatherPlayerController::OnSkillSpeedReleased);
		EIC->BindAction(IA_SkillSpeed, ETriggerEvent::Canceled, this, &AAnimalGatherPlayerController::OnSkillSpeedReleased);
	}
	// 2026.10.06 Lee end

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
	// 2026.10.06 Lee start（普通対戦：準備中・終了後は入力を記録しない。チュートリアルは従来フロー）
	if (!IsGameplayActionAllowed())
	{
		return;
	}
	// 2026.10.06 Lee end

	// 初回押下：入力を記録→移動→自動リピートタイマー開始
	HeldInputValue = Value.Get<FVector2D>();
	PerformMoveInDirection(HeldInputValue);

	GetWorldTimerManager().SetTimer(AutoRepeatHandle, this, &AAnimalGatherPlayerController::OnAutoRepeatMove,
		AutoRepeatRate, true, AutoRepeatDelay);
}

void AAnimalGatherPlayerController::OnMoveTriggered(const FInputActionValue& Value)
{
	// 2026.10.06 Lee start（普通対戦の段階権限）
	if (!IsGameplayActionAllowed())
	{
		return;
	}
	// 2026.10.06 Lee end

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
	// 2026.10.06 Lee start（普通対戦の段階権限：終了後の連移を停止）
	if (!IsGameplayActionAllowed())
	{
		return;
	}
	// 2026.10.06 Lee end

	if (!HeldInputValue.IsZero())
	{
		PerformMoveInDirection(HeldInputValue);
	}
}

void AAnimalGatherPlayerController::PerformMoveInDirection(const FVector2D& Input)
{
	// 2026.10.06 Lee start（普通対戦の段階権限：移動実行の最終ゲート）
	if (!IsGameplayActionAllowed())
	{
		return;
	}
	// 2026.10.06 Lee end

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
	// 2026.10.06 Lee start（普通対戦の段階権限：準備中・終了後は配置しない。チュートリアルは従来フロー維持）
	if (!IsGameplayActionAllowed())
	{
		return;
	}

	// 方向タイルのみ受付（Empty / Spawn / Goal 等の誤用を弾く）。
	if (!AMapManager::IsDirectionTile(Direction))
	{
		UE_LOG(LogTemp, Display, TEXT("PlaceDirection: 無効な方向 (%d) は配置できません"), (int32)Direction);
		return;
	}
	// 2026.10.06 Lee end

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

	// 2026.10.06 Lee start（座標の範囲内チェックのみ追加：既存の FIFO・上書き・重複除去・特殊タイル規則は変更しない）
	if (TargetCoords.X < 0 || TargetCoords.Y < 0 ||
		TargetCoords.X >= MapManagerRef->MapWidth || TargetCoords.Y >= MapManagerRef->MapHeight)
	{
		UE_LOG(LogTemp, Display, TEXT("PlaceDirection: 範囲外の座標 (%d, %d) には配置できません"),
			TargetCoords.X, TargetCoords.Y);
		return;
	}
	// 2026.10.06 Lee end
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

	// 2026.10.02 Lee start
	// チュートリアル中は GameMode 側の配置制限に従う（通常ゲームでは Cast 失敗するため何もしない）
	const ATutorialGameMode* TutorialGameMode = Cast<ATutorialGameMode>(UGameplayStatics::GetGameMode(this));
	if (TutorialGameMode && !TutorialGameMode->IsPlacementAllowed(static_cast<uint8>(CursorPawn->PlayerID), TargetCoords))
	{
		UE_LOG(LogTemp, Display, TEXT("PlaceDirection: チュートリアルの制限により (%d, %d) には配置できません"),
			TargetCoords.X, TargetCoords.Y);
		return;
	}
	// 2026.10.02 Lee end

	// 2026.09.25 Lee start
	const uint8 OwnerId = static_cast<uint8>(CursorPawn->PlayerID);

	// 同一座標への再配置は重複登録を防ぐため先に履歴から取り除く
	// （重複のまま FIFO すると実矢印数と履歴数が乖離し、
	//  古い矢印が 3 手分より長く残ってしまうバグの原因だった）
	PlaceHistory.Remove(TargetCoords);
	// 2026.09.25 Lee end

	// 配置履歴は最大3件（FIFO）、4件目で最古の矢印が Empty に戻る
	static constexpr int32 MaxHistory = 3;
	if (PlaceHistory.Num() >= MaxHistory)
	{
		const FIntPoint Oldest = PlaceHistory[0];
		PlaceHistory.RemoveAt(0);
		// 2026.09.25 Lee start
		// 相手プレイヤーに上書きされた矢印は消さず、自分の矢印のみ消退する
		// MapManagerRef->SetTileData(Oldest.X, Oldest.Y, ETileType::Empty);
		const bool bCleared = MapManagerRef->ClearArrowIfOwned(Oldest.X, Oldest.Y, OwnerId);
		if (!bCleared)
		{
			UE_LOG(LogTemp, Display,
				TEXT("PlaceDirection: 消退対象 (%d, %d) は他プレイヤー矢印のため保持"), Oldest.X, Oldest.Y);
		}
		// 2026.09.25 Lee end
	}

	// 2026.09.25 Lee start
	// MapManagerRef->SetTileData(TargetCoords.X, TargetCoords.Y, Direction);
	MapManagerRef->SetTileData(TargetCoords.X, TargetCoords.Y, Direction, OwnerId);
	// 2026.09.25 Lee end
	PlaceHistory.Add(TargetCoords);

	// 2026.10.08 Lee start（教程用：実配置完了の通知。地図と履歴の更新が確定した後でのみ送る）
	// 通常対戦では Cast が失敗するため何も起きない（チュートリアルの修復記録でのみ使用される）
	// 2026.10.08 Lee 第三批修正 start（C4456: 同関数内の既存変数 TutorialGameMode (旧 426 行) との
	//  名前衝突を解消するため変数名を変更）
	if (ATutorialGameMode* TutorialModeForPlacement = Cast<ATutorialGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		TutorialModeForPlacement->NotifyArrowPlaced(OwnerId, TargetCoords, Direction);
	}
	// 2026.10.08 Lee 第三批修正 end
	// 2026.10.08 Lee end
}

//==============================================================================
// 2026.10.06 Lee start（スキルシステム・一時入力の追加実装）
//==============================================================================

void AAnimalGatherPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// Cursor が既に実マップを持つ場合はそれを優先して取り込む
	// （世界の最初のマップへ強制切替はしない。初回保持時は InitCursor 前で未設定の可能性がある）。
	if (const ACursorPawn* CursorPawn = Cast<ACursorPawn>(InPawn))
	{
		if (AMapManager* PawnMap = CursorPawn->GetMapManager())
		{
			MapManagerRef = PawnMap;
		}
	}

	// マップ未設定でも依頼のみ行い、判定と再試行は GameMode 側に委ねる。
	TryInitializeMatchSkillsIfReady();
}

void AAnimalGatherPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 連移タイマーの残留を防ぐ。
	GetWorldTimerManager().ClearTimer(AutoRepeatHandle);

	Super::EndPlay(EndPlayReason);
}

USkillSystemComponent* AAnimalGatherPlayerController::GetSkillSystemComponent() const
{
	return SkillSystemComponent;
}

uint8 AAnimalGatherPlayerController::GetSkillPlayerId() const
{
	const ULocalPlayer* LP = GetLocalPlayer();
	if (LP == nullptr)
	{
		return 255; // Player 未関連付け。
	}

	const int32 ControllerId = LP->GetControllerId();
	return (ControllerId == 0 || ControllerId == 1) ? static_cast<uint8>(ControllerId) : static_cast<uint8>(255);
}

TArray<FIntPoint> AAnimalGatherPlayerController::GetOwnedPlacedArrowCoords(uint8 OwnerPlayerId) const
{
	TArray<FIntPoint> Result;

	// 身分は 0 / 1 のみ有効（255 などは空応答）。
	if (OwnerPlayerId != 0 && OwnerPlayerId != 1)
	{
		return Result;
	}

	AMapManager* BoundMap = GetMapManager();
	UWorld* World = GetWorld();
	if (BoundMap == nullptr || World == nullptr)
	{
		return Result;
	}

	// 同じ World・同じバインド地図・同じ身分の Controller を特定する（自分自身も対象）。
	const AAnimalGatherPlayerController* TargetController = nullptr;
	for (TActorIterator<AAnimalGatherPlayerController> It(World); It; ++It)
	{
		const AAnimalGatherPlayerController* Candidate = *It;
		if (Candidate == nullptr || !IsValid(Candidate))
		{
			continue;
		}
		if (Candidate->GetSkillPlayerId() != OwnerPlayerId)
		{
			continue;
		}
		if (Candidate->GetMapManager() != BoundMap)
		{
			continue; // 別マップの Controller は対象外。
		}
		TargetController = Candidate;
		break;
	}

	if (TargetController == nullptr)
	{
		return Result;
	}

	// 実履歴のコピーから抽出する（可変の履歴は公開しない）。
	const TArray<FIntPoint> HistoryCopy = TargetController->PlaceHistory;

	TSet<FIntPoint> SeenCoords;
	SeenCoords.Reserve(HistoryCopy.Num());
	Result.Reserve(FMath::Min(HistoryCopy.Num(), 3));

	for (const FIntPoint& Coord : HistoryCopy)
	{
		if (Result.Num() >= 3)
		{
			break; // 有効目標は最大 3 件。
		}

		bool bAlreadySeen = false;
		SeenCoords.Add(Coord, &bAlreadySeen);
		if (bAlreadySeen)
		{
			continue;
		}

		// 地図範囲内の座標か（履歴と地図のズレに対する防御）。
		if (Coord.X < 0 || Coord.Y < 0 || Coord.X >= BoundMap->MapWidth || Coord.Y >= BoundMap->MapHeight)
		{
			continue;
		}

		// 外周ボーダーは対象外。
		if (BoundMap->IsBorderTile(Coord.X, Coord.Y))
		{
			continue;
		}

		const int32 TileIndex = Coord.Y * BoundMap->MapWidth + Coord.X;
		if (!BoundMap->GridData.IsValidIndex(TileIndex))
		{
			continue;
		}

		// 現在も方向矢印で、かつ所有者が指定身分のマスのみ有効
		// （相手に上書き済み・消退済みの古い履歴は除外）。
		const FMapTileData& Tile = BoundMap->GridData[TileIndex];
		if (!AMapManager::IsDirectionTile(Tile.TileType) || Tile.OwnerPlayerId != OwnerPlayerId)
		{
			continue;
		}

		Result.Add(Coord);
	}

	return Result;
}

void AAnimalGatherPlayerController::ResetTransientInput(bool bRequireSkillRelease)
{
	// 連移の入力値とタイマーを即時クリア（Completed / Canceled を待たない）。
	HeldInputValue = FVector2D::ZeroVector;
	GetWorldTimerManager().ClearTimer(AutoRepeatHandle);

	// スキルの押下状態もクリア。要求時は解放イベント後のみ再武装する。
	bSkillReverseAwaitRelease = bRequireSkillRelease;
	bSkillSpeedAwaitRelease = bRequireSkillRelease;
}

// 2026.10.06 Lee start（B2：ビューポート物理層からの解放通知）
void AAnimalGatherPlayerController::NotifySkillButtonReleased(bool bSpeed)
{
	// 物理解放確認に基づく再武装のみ。回数・クールダウン・連移状態には触れない。
	if (bSpeed)
	{
		bSkillSpeedAwaitRelease = false;
	}
	else
	{
		bSkillReverseAwaitRelease = false;
	}
}
// 2026.10.06 Lee end（B2）

bool AAnimalGatherPlayerController::IsGameplayActionAllowed() const
{
	// 普通対戦の MainGameMode のみ段階権限に従う。
	const AMainGameMode* Mode = Cast<AMainGameMode>(UGameplayStatics::GetGameMode(this));
	if (Mode != nullptr && Mode->IsNormalMatch())
	{
		return Mode->IsGameplayInputAllowed();
	}

	// 2026.10.08 Lee 第三批修正 start（教程の Complete 中は移動・配置入力も停止する。
	// それ以外の教程ステップは従来どおり許可）
	if (const ATutorialGameMode* TutorialMode = Cast<ATutorialGameMode>(Mode))
	{
		return TutorialMode->GetCurrentStep() != ETutorialStep::Complete;
	}
	// 2026.10.08 Lee 第三批修正 end

	// チュートリアル等は従来の段階制限フロー（IsPlacementAllowed 等）を維持する。
	return true;
}

void AAnimalGatherPlayerController::TryInitializeMatchSkillsIfReady()
{
	// 普通対戦の GameMode のみ初期化を依頼（冪等性と再試行は GameMode 側の責務）。
	AMainGameMode* Mode = Cast<AMainGameMode>(UGameplayStatics::GetGameMode(this));
	if (Mode != nullptr && Mode->IsNormalMatch())
	{
		Mode->TryInitializeMatchSkills();
	}
}

void AAnimalGatherPlayerController::OnSkillReverseStarted(const FInputActionValue& Value)
{
	// 未解放の押下（再配信・解除漏れ）は無視する。
	if (bSkillReverseAwaitRelease)
	{
		return;
	}
	bSkillReverseAwaitRelease = true;

	// 準備中・終了後の Started はキャッシュしない（開始時に自動発動しない）。
	if (!IsGameplayActionAllowed())
	{
		return;
	}

	if (SkillSystemComponent != nullptr)
	{
		SkillSystemComponent->TryUseSkill(SkillSlotIndex_Reverse);
	}
}

void AAnimalGatherPlayerController::OnSkillSpeedStarted(const FInputActionValue& Value)
{
	// 未解放の押下（再配信・解除漏れ）は無視する。
	if (bSkillSpeedAwaitRelease)
	{
		return;
	}
	bSkillSpeedAwaitRelease = true;

	// 準備中・終了後の Started はキャッシュしない（開始時に自動発動しない）。
	if (!IsGameplayActionAllowed())
	{
		return;
	}

	if (SkillSystemComponent != nullptr)
	{
		SkillSystemComponent->TryUseSkill(SkillSlotIndex_SpeedUp);
	}
}

void AAnimalGatherPlayerController::OnSkillReverseReleased(const FInputActionValue& Value)
{
	// Completed / Canceled 共通：解放で再武装。
	bSkillReverseAwaitRelease = false;
}

void AAnimalGatherPlayerController::OnSkillSpeedReleased(const FInputActionValue& Value)
{
	// Completed / Canceled 共通：解放で再武装。
	bSkillSpeedAwaitRelease = false;
}

//==============================================================================
// 2026.10.06 Lee end（スキルシステム・一時入力の追加実装）
//==============================================================================

//==============================================================================
// 内部ユーティリティ
//==============================================================================

ACursorPawn* AAnimalGatherPlayerController::GetCursorPawn() const
{
	return Cast<ACursorPawn>(GetPawn());
}
