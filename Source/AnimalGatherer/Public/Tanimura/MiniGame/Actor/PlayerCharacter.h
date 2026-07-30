// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "PlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;

/**
 * プレイヤーキャラクタークラス
 * 自由移動、ジャンプ機能、Enhanced Inputを管理する
 */

UCLASS()
class ANIMALGATHERER_API APlayerCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    APlayerCharacter();

protected:
    virtual void BeginPlay() override;

public:
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    // プレイヤー死亡フラグ
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status")
    bool bIsDead;

    // 死亡処理
    UFUNCTION(BlueprintCallable, Category = "Status")
    void Die();

protected:
    // 自由移動入力処理
    void Move(const FInputActionValue& Value);

    // ジャンプ開始処理
    void StartJump();

    // ジャンプ終了処理
    void StopJump();

private:
    // 移動用 Input Action
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UInputAction> MoveAction;

    // ジャンプ用 Input Action
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UInputAction> JumpAction;
};
