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
class UInputMappingContext;
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
    APlayerCharacter(const FObjectInitializer& ObjectInitializer);

protected:
    virtual void BeginPlay() override;

    // 移動状態を毎フレーム判定し、bIsMovingへ反映する
    virtual void Tick(float DeltaSeconds) override;

    // 所持時にプレイヤー操作用のマッピングコンテキストを適用する
    virtual void PossessedBy(AController* NewController) override;

public:
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    // プレイヤー死亡フラグ
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status")
    bool bIsDead;

    // 移動中かつ接地中かを示すフラグ（BP側で歩行アニメーションの再生条件に使用する）
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation")
    bool bIsMoving;

    // 移動中と判定する速度のしきい値（cm/s）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (ClampMin = "0.0"))
    float WalkAnimSpeedThreshold = 10.0f;

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

    // キーボード・ゲームパッドのキー割り当てを保持するマッピングコンテキスト
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UInputMappingContext> MiniGameInputContext;

    // 移動用のマッピングコンテキストを生成してプレイヤーへ適用する
    void AddDefaultInputContext();

    // 移動アクションへキーをマッピングする（1D入力をY軸へ変換、または反転）
    void AddMoveKeyMapping(const FKey& Key, bool bSwizzleToY, bool bNegate, float DeadZone = 0.0f);

    // スティック中央付近で入力が効かないようにするデッドゾーン量（0.0〜1.0）
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
    float StickDeadZone = 0.25f;

    // 足元のプレイヤーカラー円
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerMarker", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> PlayerCircleMesh;

    // このプレイヤーを表すカラー
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerMarker", meta = (AllowPrivateAccess = "true"))
    FLinearColor PlayerColor = FLinearColor::White;

    // 足元の円へプレイヤーカラーを反映する
    void ApplyPlayerColor();
};
