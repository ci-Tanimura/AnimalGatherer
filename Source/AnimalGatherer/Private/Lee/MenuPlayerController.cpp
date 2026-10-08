// Fill out your copyright notice in the Description page of Project Settings.

#include "Lee/MenuPlayerController.h"
#include "Lee/MenuHUDWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"

namespace
{
	/** @brief dominant 軸を採用する際の最低入力値。スティックの微小な傾きによる誤選択を防ぐ。 */
	constexpr float MenuAxisStepThreshold = 0.5f;

	/** @brief メニュー専用 IMC の優先度。対戦から残存しうる IMC_P1 / IMC_P2（優先度 0）より
	 *        先に評価させ、Esc 等のキー競合（旧 IA_QuitGame による消費）を防ぐ。
	 *        メニュー終了時は自分のコンテキストのみ削除し、対戦側のマッピングには触れない。 */
	constexpr int32 MenuMappingContextPriority = 100;
}

AMenuPlayerController::AMenuPlayerController()
{
	// PlayerTick が TickPlayerInput / ProcessPlayerInput を駆動し Enhanced Input の処理が行われるため、
	// Tick は基底クラス既定（有効）のままにする。無効化すると入力アクションが処理されない。
}

//==============================================================================
// 公開メソッド
//==============================================================================

UMenuHUDWidget* AMenuPlayerController::GetMenuWidget() const
{
	// ウィジェットは GameMode BP 側で生成されるため、静的登録から同一 World の実体を引く。
	return UMenuHUDWidget::GetMenuWidgetFor(GetWorld());
}

void AMenuPlayerController::ApplyMenuInputMode()
{
	// メニューはゲームパッド主体で操作する（マウス互換は要件外のためカーソルは非表示）。
	bShowMouseCursor = false;

	FInputModeGameOnly Mode;
	Mode.SetConsumeCaptureMouseDown(false);
	SetInputMode(Mode);
}

//==============================================================================
// APlayerController 系オーバーライド
//==============================================================================

void AMenuPlayerController::SetPlayer(UPlayer* InPlayer)
{
	// Player 再関連付け時に旧 LocalPlayer への IMC 登録を解除するため、Super 前に旧参照を採取する。
	ULocalPlayer* PreviousLocalPlayer = GetLocalPlayer();
	const bool bWasRegistered = bMenuIMCRegistered;

	Super::SetPlayer(InPlayer);

	// 切図後も LocalPlayer は生存するため、別 Player への再関連付け時は旧側の登録を必ず解く。
	if (PreviousLocalPlayer != nullptr && PreviousLocalPlayer != GetLocalPlayer() && bWasRegistered)
	{
		UnregisterMenuMappingContextFrom(PreviousLocalPlayer);
	}

	// 関連付け後（SetupInputComponent 実行済み）の時点で確実に生成・登録・入力モード適用まで行う。
	BuildMenuInputAssets();
	RegisterMenuMappingContext();
	ApplyMenuInputMode();
}

void AMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// SetupInputComponent より後に環境が変わる場合に備えて生成を保証（冪等）。
	BuildMenuInputAssets();

	RegisterMenuMappingContext();
	ApplyMenuInputMode();

	// BP 側 BeginPlay（HUD 生成後の SetInputMode / SetShowMouseCursor 等）が
	// 同一 Tick 内で後から入力モードを上書きするケースに備え、次 Tick で一度だけ再適用する。
	GetWorldTimerManager().SetTimerForNextTick(this, &AMenuPlayerController::ApplyMenuInputMode);
}

void AMenuPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// 対戦用 PlayerController と同じ Enhanced Input 環境の防御
	// （非 Enhanced 環境では元の InputComponent を壊して作り直す。プロジェクト既知の対処）。
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EIC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MenuPC] InputComponent が Enhanced ではない (%s) → 再作成します"),
			InputComponent ? *InputComponent->GetClass()->GetName() : TEXT("null"));

		if (InputComponent)
		{
			InputComponent->DestroyComponent();
		}
		InputComponent = NewObject<UEnhancedInputComponent>(this, TEXT("MenuPC_InputComponent0"));
		InputComponent->RegisterComponent();

		EIC = Cast<UEnhancedInputComponent>(InputComponent);
		if (!EIC)
		{
			UE_LOG(LogTemp, Error, TEXT("[MenuPC] UEnhancedInputComponent の作成に失敗"));
			return;
		}
	}

	// バインド対象の IA が未生成のケースに備えて先に生成（冪等）。
	BuildMenuInputAssets();

	if (IA_MenuNavigate)
	{
		EIC->BindAction(IA_MenuNavigate, ETriggerEvent::Started, this, &AMenuPlayerController::OnNavigateStarted);
		EIC->BindAction(IA_MenuNavigate, ETriggerEvent::Triggered, this, &AMenuPlayerController::OnNavigateTriggered);
		EIC->BindAction(IA_MenuNavigate, ETriggerEvent::Completed, this, &AMenuPlayerController::OnNavigateCompleted);
		EIC->BindAction(IA_MenuNavigate, ETriggerEvent::Canceled, this, &AMenuPlayerController::OnNavigateCanceled);
	}
	if (IA_MenuConfirm)
	{
		EIC->BindAction(IA_MenuConfirm, ETriggerEvent::Started, this, &AMenuPlayerController::OnConfirmStarted);
	}
	if (IA_MenuBack)
	{
		EIC->BindAction(IA_MenuBack, ETriggerEvent::Started, this, &AMenuPlayerController::OnBackStarted);
	}
	if (IA_MenuQuit)
	{
		EIC->BindAction(IA_MenuQuit, ETriggerEvent::Started, this, &AMenuPlayerController::OnQuitStarted);
	}

	// SetupInputComponent は SetPlayer 内の InitInputSystem で LocalPlayer 関連付け後に呼ばれるため、
	// ここでも登録を試みる（未関連付け時は RegisterMenuMappingContext 内で安全にスキップされる）。
	RegisterMenuMappingContext();
}

void AMenuPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// リピートタイマーの残留を防ぐ。
	GetWorldTimerManager().ClearTimer(NavigateRepeatHandle);
	HeldNavigateValue = FVector2D::ZeroVector;
	LastAppliedStep = 0;

	// メニュー専用 IMC を解除（対戦画面への遷移後も IMC が残るのを防ぐ。LocalPlayer は切図後も生存するため）。
	UnregisterMenuMappingContext();

	Super::EndPlay(EndPlayReason);
}

//==============================================================================
// 入力アセット構築と IMC 登録
//==============================================================================

void AMenuPlayerController::BuildMenuInputAssets()
{
	if (IA_MenuNavigate && IA_MenuConfirm && IA_MenuBack && IA_MenuQuit && IMC_Menu)
	{
		return; // 既に生成済み。
	}

	// ── Input Action の生成 ──
	IA_MenuNavigate = NewObject<UInputAction>(this, TEXT("IA_MenuNavigate"));
	IA_MenuNavigate->ValueType = EInputActionValueType::Axis2D;

	IA_MenuConfirm = NewObject<UInputAction>(this, TEXT("IA_MenuConfirm"));
	IA_MenuBack = NewObject<UInputAction>(this, TEXT("IA_MenuBack"));
	IA_MenuQuit = NewObject<UInputAction>(this, TEXT("IA_MenuQuit"));

	// ── Input Mapping Context の生成（対戦用 IMC_P1 / IMC_P2 は含めない） ──
	IMC_Menu = NewObject<UInputMappingContext>(this, TEXT("IMC_Menu"));

	// 1D キーを Axis2D へ写像するヘルパー。
	// Right = +X、Left = -X（Negate bX）、Up = +Y（Swizzle YXZ）、Down = -Y（Swizzle 後 Negate bY）。
	// Swizzle → Negate の順で適用する（Swizzle 後の成分に対して反転する）。
	auto AddNavigateMapping = [this](const FKey& Key, const bool bNegateX, const bool bMapToY, const bool bNegateY)
	{
		FEnhancedActionKeyMapping& Mapping = IMC_Menu->MapKey(IA_MenuNavigate, Key);

		if (bMapToY)
		{
			UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(IMC_Menu);
			Swizzle->Order = EInputAxisSwizzle::YXZ; // 1D キー値を Y 成分へ載せ替える。
			Mapping.Modifiers.Add(Swizzle);
		}

		if (bNegateX || bNegateY)
		{
			UInputModifierNegate* Negate = NewObject<UInputModifierNegate>(IMC_Menu);
			Negate->bX = bNegateX;
			Negate->bY = bNegateY;
			Negate->bZ = false;
			Mapping.Modifiers.Add(Negate);
		}
	};

	// 選択移動：キーボード方向キー
	AddNavigateMapping(EKeys::Right, false, false, false);
	AddNavigateMapping(EKeys::Left, true, false, false);
	AddNavigateMapping(EKeys::Up, false, true, false);
	AddNavigateMapping(EKeys::Down, false, true, true);

	// 選択移動：ゲームパッド十字キー（方向キーと同一構成）
	AddNavigateMapping(EKeys::Gamepad_DPad_Right, false, false, false);
	AddNavigateMapping(EKeys::Gamepad_DPad_Left, true, false, false);
	AddNavigateMapping(EKeys::Gamepad_DPad_Up, false, true, false);
	AddNavigateMapping(EKeys::Gamepad_DPad_Down, false, true, true);

	// 選択移動：ゲームパッド左スティック（2D 軸そのまま + デッドゾーン。
	// 生成時に値を読むため、実行中の MenuStickDeadZone 変更には追従しない）
	FEnhancedActionKeyMapping& StickMapping = IMC_Menu->MapKey(IA_MenuNavigate, EKeys::Gamepad_Left2D);
	UInputModifierDeadZone* DeadZone = NewObject<UInputModifierDeadZone>(IMC_Menu);
	DeadZone->Type = EDeadZoneType::Radial;
	DeadZone->LowerThreshold = MenuStickDeadZone;
	DeadZone->UpperThreshold = 1.0f;
	StickMapping.Modifiers.Add(DeadZone);

	// 決定：A ボタン / Enter
	IMC_Menu->MapKey(IA_MenuConfirm, EKeys::Gamepad_FaceButton_Bottom);
	IMC_Menu->MapKey(IA_MenuConfirm, EKeys::Enter);

	// 戻る：B ボタン（タイトルではウィジェット側が無視する）
	IMC_Menu->MapKey(IA_MenuBack, EKeys::Gamepad_FaceButton_Right);

	// 終了：Esc
	IMC_Menu->MapKey(IA_MenuQuit, EKeys::Escape);
}

void AMenuPlayerController::RegisterMenuMappingContext()
{
	if (!IMC_Menu)
	{
		BuildMenuInputAssets();
	}

	ULocalPlayer* LP = GetLocalPlayer();
	if (!LP)
	{
		// BeginPlay が SetPlayer より先に走ったケース。SetPlayer / SetupInputComponent で再試行される。
		UE_LOG(LogTemp, Verbose, TEXT("[MenuPC] RegisterMenuMappingContext: LocalPlayer 未関連付けのためスキップ"));
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
	{
		if (!Subsystem->HasMappingContext(IMC_Menu))
		{
			// 高優先度で登録し、対戦から残存する IMC より先にメニュー側の Action を解決させる。
			Subsystem->AddMappingContext(IMC_Menu, MenuMappingContextPriority);
			UE_LOG(LogTemp, Log, TEXT("[MenuPC] IMC_Menu registered (priority=%d, ControllerId=%d)"),
				MenuMappingContextPriority, LP->GetControllerId());
		}
		bMenuIMCRegistered = true;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[MenuPC] EnhancedInputLocalPlayerSubsystem が取得できません"));
	}
}

void AMenuPlayerController::UnregisterMenuMappingContext()
{
	UnregisterMenuMappingContextFrom(GetLocalPlayer());
}

void AMenuPlayerController::UnregisterMenuMappingContextFrom(ULocalPlayer* InLocalPlayer)
{
	if (!IMC_Menu || !InLocalPlayer)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(InLocalPlayer))
	{
		if (Subsystem->HasMappingContext(IMC_Menu))
		{
			Subsystem->RemoveMappingContext(IMC_Menu);
			UE_LOG(LogTemp, Log, TEXT("[MenuPC] IMC_Menu unregistered (ControllerId=%d)"), InLocalPlayer->GetControllerId());
		}
	}

	if (InLocalPlayer == GetLocalPlayer())
	{
		bMenuIMCRegistered = false;
	}
}

//==============================================================================
// 入力ハンドラ
//==============================================================================

void AMenuPlayerController::OnNavigateStarted(const FInputActionValue& Value)
{
	// 初回押下：入力を記録 → 1 移動 → 自動リピートタイマー開始（初回遅延はこの歩から数える）。
	HeldNavigateValue = Value.Get<FVector2D>();
	LastAppliedStep = ResolveMenuStep(HeldNavigateValue);

	UE_LOG(LogTemp, Verbose, TEXT("[MenuPC] Navigate Started: vec=(%s) step=%d"),
		*HeldNavigateValue.ToString(), LastAppliedStep);

	PerformMenuMove(HeldNavigateValue);

	GetWorldTimerManager().SetTimer(NavigateRepeatHandle, this, &AMenuPlayerController::OnNavigateAutoRepeat,
		MenuAutoRepeatRate, true, MenuAutoRepeatDelay);
}

void AMenuPlayerController::OnNavigateTriggered(const FInputActionValue& Value)
{
	// 長押し中の入力値を更新。
	HeldNavigateValue = Value.Get<FVector2D>();

	// 長押し中の変向（十字 → スティック、右 → 下 等）は即時に 1 歩反映し、
	// 初回リピート遅延をこの歩から付け直す。しきい値未満・同方向は無操作。
	const int32 NewStep = ResolveMenuStep(HeldNavigateValue);
	if (NewStep != 0 && NewStep != LastAppliedStep)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[MenuPC] Navigate 変向: vec=(%s) step=%d → %d"),
			*HeldNavigateValue.ToString(), LastAppliedStep, NewStep);

		LastAppliedStep = NewStep;
		PerformMenuMove(HeldNavigateValue);

		GetWorldTimerManager().SetTimer(NavigateRepeatHandle, this, &AMenuPlayerController::OnNavigateAutoRepeat,
			MenuAutoRepeatRate, true, MenuAutoRepeatDelay);
	}
}

void AMenuPlayerController::OnNavigateCompleted(const FInputActionValue& Value)
{
	// キー / スティックを離したらリピート停止（タイマー残留なし）。
	UE_LOG(LogTemp, Verbose, TEXT("[MenuPC] Navigate Completed: リピート停止"));
	HeldNavigateValue = FVector2D::ZeroVector;
	LastAppliedStep = 0;
	GetWorldTimerManager().ClearTimer(NavigateRepeatHandle);
}

void AMenuPlayerController::OnNavigateCanceled(const FInputActionValue& Value)
{
	// Completed 共通：焦点喪失等でもリピートを停止する。
	UE_LOG(LogTemp, Verbose, TEXT("[MenuPC] Navigate Canceled"));
	OnNavigateCompleted(Value);
}

void AMenuPlayerController::OnNavigateAutoRepeat()
{
	if (!HeldNavigateValue.IsNearlyZero())
	{
		UE_LOG(LogTemp, Verbose, TEXT("[MenuPC] Navigate リピート: vec=(%s)"), *HeldNavigateValue.ToString());
		PerformMenuMove(HeldNavigateValue);
	}
}

void AMenuPlayerController::OnConfirmStarted(const FInputActionValue& Value)
{
	UMenuHUDWidget* Menu = GetMenuWidget();
	UE_LOG(LogTemp, Verbose, TEXT("[MenuPC] Confirm Started: menu=%s"),
		Menu ? *Menu->GetName() : TEXT("null"));

	if (Menu)
	{
		Menu->ConfirmMenuSelection();
	}
}

void AMenuPlayerController::OnBackStarted(const FInputActionValue& Value)
{
	UMenuHUDWidget* Menu = GetMenuWidget();
	UE_LOG(LogTemp, Verbose, TEXT("[MenuPC] Back Started: menu=%s"),
		Menu ? *Menu->GetName() : TEXT("null"));

	if (Menu)
	{
		Menu->RequestMenuBack();
	}
}

void AMenuPlayerController::OnQuitStarted(const FInputActionValue& Value)
{
	UMenuHUDWidget* Menu = GetMenuWidget();
	UE_LOG(LogTemp, Verbose, TEXT("[MenuPC] Quit Started: menu=%s"),
		Menu ? *Menu->GetName() : TEXT("null"));

	if (Menu)
	{
		// ウィジェット経由（切替ロックと BP 通知を効かせる）。
		Menu->RequestMenuQuit(this);
		return;
	}

	// メニューウィジェットが解決できない場合の保険（Esc で必ず終了できるようにする）。
	UE_LOG(LogTemp, Verbose, TEXT("[MenuPC] menu 未解決のため直接 QuitGame"));
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

//==============================================================================
// 内部ユーティリティ
//==============================================================================

int32 AMenuPlayerController::ResolveMenuStep(const FVector2D& Input) const
{
	// 対戦カーソル移動と同じ dominant 軸方式（斜め入力は絶対値の大きい軸のみ採用）。
	// 右 / 下 = 次のボタン、左 / 上 = 前のボタン（水平・垂直どちらの並びでも成立）。
	if (FMath::Abs(Input.X) >= FMath::Abs(Input.Y))
	{
		if (FMath::Abs(Input.X) < MenuAxisStepThreshold)
		{
			return 0;
		}
		return (Input.X > 0.0f) ? 1 : -1;
	}

	if (FMath::Abs(Input.Y) < MenuAxisStepThreshold)
	{
		return 0;
	}
	return (Input.Y > 0.0f) ? -1 : 1;
}

void AMenuPlayerController::PerformMenuMove(const FVector2D& Input)
{
	const int32 Step = ResolveMenuStep(Input);
	if (Step == 0)
	{
		return;
	}

	if (UMenuHUDWidget* Menu = GetMenuWidget())
	{
		UE_LOG(LogTemp, Verbose, TEXT("[MenuPC] メニュー移動要求: step=%d (widget=%s)"),
			Step, *Menu->GetName());
		Menu->MoveMenuSelection(Step);
	}
	else
	{
		UE_LOG(LogTemp, Verbose, TEXT("[MenuPC] メニュー移動要求: step=%d ただし widget 未解決"), Step);
	}
}
