// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Tanimura/MiniGame/DamageableInterface.h"
#include "PlayerCharacter.generated.h"

class AController;
class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UStaticMeshComponent;

// プレイヤー死亡を通知するデリゲート（引数：死亡したプレイヤーのコントローラー）
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerDiedSignature, AController*, PlayerController);

/**
 * プレイヤーキャラクタークラス
 * 自由移動、ジャンプ機能、Enhanced Inputを管理する
 */

UCLASS()
class ANIMALGATHERER_API APlayerCharacter : public ACharacter, public IDamageableInterface
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

    // プレイヤーカラーを設定し、足元の円へ反映する
    UFUNCTION(BlueprintCallable, Category = "PlayerMarker")
    void SetPlayerColor(const FLinearColor& NewColor);

    // プレイヤー死亡通知イベント
    UPROPERTY(BlueprintAssignable, Category = "Status")
    FOnPlayerDiedSignature OnPlayerDied;

    // ダメージを受けた際の処理（IDamageableInterface）
    virtual void ReceiveDamage_Implementation() override;

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

    // 足元のプレイヤーカラー円
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerMarker", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> PlayerCircleMesh;

    // このプレイヤーを表すカラー
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerMarker", meta = (AllowPrivateAccess = "true"))
    FLinearColor PlayerColor = FLinearColor::White;

    // 足元の円へプレイヤーカラーを反映する
    void ApplyPlayerColor();
};
