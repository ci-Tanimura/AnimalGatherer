// Fill out your copyright notice in the Description page of Project Settings.


#include "Tanimura/MiniGame/Actor/PlayerCharacter.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

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
}

void APlayerCharacter::BeginPlay()
{
    Super::BeginPlay();
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

    UE_LOG(LogTemp, Warning, TEXT("Player Character Has Died."));
}