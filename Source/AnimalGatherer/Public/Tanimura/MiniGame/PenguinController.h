// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PenguinController.generated.h"

class UInputMappingContext;

/**
 * プレイヤーコントローラークラス
 * IMCの注入および入力コンテキストの管理を行う
 */
UCLASS()
class ANIMALGATHERER_API APenguinController : public APlayerController
{
	GENERATED_BODY()

protected:
    virtual void BeginPlay() override;

private:
    // 初期ロード時に適用する Input Mapping Context
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

    // Mapping Context の優先度
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
    int32 MappingPriority = 0;
};
