// Fill out your copyright notice in the Description page of Project Settings.


#include "Tanimura/MiniGame/Actor/PlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Camera/PlayerCameraManager.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

APlayerCharacter::APlayerCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
    // 移動状態（bIsMoving）を毎フレーム更新するためTickを有効化する
    PrimaryActorTick.bCanEverTick = true;

    bIsDead = false;
    bIsMoving = false;

    // 移動方向にキャラクターの向きを自動回転させる
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);

    // ジャンプおよび滞空時パラメータの設定
    GetCharacterMovement()->JumpZVelocity = 350.0f;
    GetCharacterMovement()->AirControl = 0.35f;
    GetCharacterMovement()->MaxWalkSpeed = 300.0f;

    // めり込み解消を1フレームの接近量以上に保ち、突き飛ばしと沈み込みを抑える
    GetCharacterMovement()->MaxDepenetrationWithPawn = 12.0f;

    // 自分のカプセルを歩行面として扱わせず、他キャラクターが滑り上がるのを防ぐ
    GetCapsuleComponent()->SetWalkableSlopeOverride(FWalkableSlopeOverride(EWalkableSlopeBehavior::WalkableSlope_Unwalkable, 0.0f));

    // 足元に配置する円盤メッシュの作成
    PlayerCircleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlayerCircleMesh"));
    PlayerCircleMesh->SetupAttachment(GetRootComponent());

    // シリンダーを薄く潰して円盤形状にする
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CircleMeshAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (CircleMeshAsset.Succeeded()) {
        PlayerCircleMesh->SetStaticMesh(CircleMeshAsset.Object);
    }

    // 半径と厚みを調整する
    PlayerCircleMesh->SetRelativeScale3D(FVector(2.0f, 2.0f, 0.02f));

    // 影・衝突を無効化し、最前面に描画する
    PlayerCircleMesh->SetCastShadow(false);
    PlayerCircleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PlayerCircleMesh->SetTranslucentSortPriority(100);

    // 入力アクション（BP側で未設定でも動作するようC++で生成する）
    MoveAction = NewObject<UInputAction>(this, TEXT("IA_Move"));
    MoveAction->ValueType = EInputActionValueType::Axis2D;

    JumpAction = NewObject<UInputAction>(this, TEXT("IA_Jump"));
    JumpAction->ValueType = EInputActionValueType::Boolean;
}

void APlayerCharacter::BeginPlay()
{
    Super::BeginPlay();

    // BP側で移動コンポーネントが欠落していてもクラッシュさせない
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    if (!Movement) {
        // 落下も移動も効かなくなるため、原因究明用にエラーを残す
        UE_LOG(LogTemp, Error, TEXT("APlayerCharacter: CharacterMovement が見つかりません。BP_PlayerCharacter のコンポーネント構成を確認"));
    }
    else {
        // 移動方向へ確実に向かせる（BP側の設定に依存しない）
        bUseControllerRotationYaw = false;
        Movement->bOrientRotationToMovement = true;
        Movement->bUseControllerDesiredRotation = false;
        Movement->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
    }

    // カプセル底面（足元）へ円盤の高さを合わせる
    const float BottomZ = -GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    PlayerCircleMesh->SetRelativeLocation(FVector(0.0f, 0.0f, BottomZ + 3.0f));

    // プレイヤーカラーを足元の円へ反映する
    ApplyPlayerColor();
}

void APlayerCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // 移動コンポーネントが欠落しているPawnは歩行判定を行わない
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    if (!Movement) {
        return;
    }

    // 生存かつ接地中に速度がしきい値を超えているときだけ歩行中とみなす（ジャンプ中は接地していない）
    const bool bWalking = !bIsDead
        && Movement->IsMovingOnGround()
        && GetVelocity().SizeSquared2D() > FMath::Square(WalkAnimSpeedThreshold);

    // 変化があったときだけBPへ伝わるように反映する
    if (bIsMoving != bWalking) {
        bIsMoving = bWalking;
    }
}

void APlayerCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    // プレイヤー操作用の入力をコントローラーのサブシステムへ適用する
    AddDefaultInputContext();
}

void APlayerCharacter::AddDefaultInputContext()
{
    // プレイヤー操作用のコントローラーでなければ何もしない
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) {
        return;
    }

    ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
    if (!LocalPlayer) {
        return;
    }

    // BP側で不正な型のアクションが設定されていても、軸2D/ブールに補正する
    if (!MoveAction || MoveAction->ValueType != EInputActionValueType::Axis2D) {
        MoveAction = NewObject<UInputAction>(this, TEXT("IA_Move"));
        MoveAction->ValueType = EInputActionValueType::Axis2D;
    }
    if (!JumpAction || JumpAction->ValueType != EInputActionValueType::Boolean) {
        JumpAction = NewObject<UInputAction>(this, TEXT("IA_Jump"));
        JumpAction->ValueType = EInputActionValueType::Boolean;
    }

    // 未生成ならマッピングコンテキストを作成し、操作キーを割り当てる
    if (!MiniGameInputContext) {
        MiniGameInputContext = NewObject<UInputMappingContext>(this, TEXT("IMC_MiniGameRuntime"));

        // キーボード（WASD）
        AddMoveKeyMapping(EKeys::W, true, false);
        AddMoveKeyMapping(EKeys::S, true, true);
        AddMoveKeyMapping(EKeys::A, false, true);
        AddMoveKeyMapping(EKeys::D, false, false);

        // キーボード（矢印キー）
        AddMoveKeyMapping(EKeys::Up, true, false);
        AddMoveKeyMapping(EKeys::Down, true, true);
        AddMoveKeyMapping(EKeys::Left, false, true);
        AddMoveKeyMapping(EKeys::Right, false, false);

        // ゲームパッド（左スティック、中央付近はデッドゾーンで無効化）
        AddMoveKeyMapping(EKeys::Gamepad_LeftX, false, false, StickDeadZone);
        AddMoveKeyMapping(EKeys::Gamepad_LeftY, true, false, StickDeadZone);

        // ジャンプ（スペース、ゲームパッド下ボタン）
        if (JumpAction) {
            MiniGameInputContext->MapKey(JumpAction, EKeys::SpaceBar);
            MiniGameInputContext->MapKey(JumpAction, EKeys::Gamepad_FaceButton_Bottom);
        }
    }

    // プレイヤーの入力サブシステムへ適用する
    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer)) {
        Subsystem->AddMappingContext(MiniGameInputContext, 0);
    }
}

void APlayerCharacter::AddMoveKeyMapping(const FKey& Key, bool bSwizzleToY, bool bNegate, float DeadZone)
{
    FEnhancedActionKeyMapping& Mapping = MiniGameInputContext->MapKey(MoveAction, Key);

    // 1D入力値をY成分へ変換する（W/Sとゲームパッド上下）
    if (bSwizzleToY) {
        UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(this);
        Swizzle->Order = EInputAxisSwizzle::YXZ;
        Mapping.Modifiers.Add(Swizzle);
    }

    // 方向を反転する（S/Aとゲームパッド左方向）
    if (bNegate) {
        Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(this));
    }

    // スティックのデッドゾーン（中央付近の微小な入力を無効化してドリフトを防ぐ）
    if (DeadZone > 0.0f) {
        UInputModifierDeadZone* DeadZoneModifier = NewObject<UInputModifierDeadZone>(this);
        DeadZoneModifier->LowerThreshold = DeadZone;
        DeadZoneModifier->Type = EDeadZoneType::Axial;
        Mapping.Modifiers.Add(DeadZoneModifier);
    }
}

void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (EnhancedInputComponent) {
        // 移動アクションのバインド
        if (MoveAction) {
            EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Move);
        }

        // ジャンプアクションのバインド
        if (JumpAction) {
            EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &APlayerCharacter::StartJump);
            EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &APlayerCharacter::StopJump);
        }
    }
}

void APlayerCharacter::Move(const FInputActionValue& Value)
{
    if (bIsDead) {
        return;
    }

    const FVector2D MovementVector = Value.Get<FVector2D>();

    // 俯瞰カメラの向きを基準に移動方向を算出し、画面の上下左右と入力を一致させる
    FRotator CameraYaw = FRotator::ZeroRotator;
    if (const APlayerController* PC = Cast<APlayerController>(GetController())) {
        if (const APlayerCameraManager* CameraManager = PC->PlayerCameraManager) {
            CameraYaw = CameraManager->GetCameraRotation();
        }
    }

    // ピッチ/ロールを無視し、ヨー角のみで地面方向の前後・左右を求める
    const FRotator YawRotation(0.0f, CameraYaw.Yaw, 0.0f);
    const FVector ForwardDir = YawRotation.RotateVector(FVector::ForwardVector);
    const FVector RightDir = YawRotation.RotateVector(FVector::RightVector);

    AddMovementInput(ForwardDir, MovementVector.Y);
    AddMovementInput(RightDir, MovementVector.X);
}

void APlayerCharacter::StartJump()
{
    if (!bIsDead) {
        Jump();
    }
}

void APlayerCharacter::StopJump()
{
    StopJumping();
}

void APlayerCharacter::Die()
{
    if (bIsDead) {
        return;
    }

    bIsDead = true;

    // 移動停止と衝突判定の無効化
    if (UCharacterMovementComponent* Movement = GetCharacterMovement()) {
        Movement->DisableMovement();
    }
    SetActorEnableCollision(false);

    // 死亡を購読者（ゲームモード等）へ通知する
    OnPlayerDied.Broadcast(GetController());

    UE_LOG(LogTemp, Warning, TEXT("Player Character Has Died."));
}

void APlayerCharacter::ReceiveDamage_Implementation()
{
    // ダメージを受けたため死亡させる
    Die();
}

void APlayerCharacter::SetPlayerColor(const FLinearColor& NewColor)
{
    PlayerColor = NewColor;

    // 設定済みの円へ即座に反映する
    ApplyPlayerColor();
}

void APlayerCharacter::ApplyPlayerColor()
{
    UMaterialInstanceDynamic* CircleMID = PlayerCircleMesh->CreateAndSetMaterialInstanceDynamic(0);
    if (CircleMID) {
        CircleMID->SetVectorParameterValue(FName(TEXT("CircleColor")), PlayerColor);
    }
}