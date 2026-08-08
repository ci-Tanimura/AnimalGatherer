// Fill out your copyright notice in the Description page of Project Settings.


#include "Tanimura/MiniGame/Actor/PlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

APlayerCharacter::APlayerCharacter()
{
    PrimaryActorTick.bCanEverTick = false;

    bIsDead = false;

    // 移動方向にキャラクターの向きを自動回転させる
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

    // ジャンプおよび滞空時パラメータの設定
    GetCharacterMovement()->JumpZVelocity = 700.0f;
    GetCharacterMovement()->AirControl = 0.35f;
    GetCharacterMovement()->MaxWalkSpeed = 500.0f;

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
}

void APlayerCharacter::BeginPlay()
{
    Super::BeginPlay();

    // カプセル底面（足元）へ円盤の高さを合わせる
    const float BottomZ = -GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    PlayerCircleMesh->SetRelativeLocation(FVector(0.0f, 0.0f, BottomZ + 3.0f));

    // プレイヤーカラーを足元の円へ反映する
    ApplyPlayerColor();
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

    // 固定俯瞰視点のため、ワールド座標系の軸（X:前後, Y:左右）に直接移動入力を与える
    AddMovementInput(FVector::ForwardVector, MovementVector.Y);
    AddMovementInput(FVector::RightVector, MovementVector.X);
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
    GetCharacterMovement()->DisableMovement();
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