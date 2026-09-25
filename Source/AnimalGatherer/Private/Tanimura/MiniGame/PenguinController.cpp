// Fill out your copyright notice in the Description page of Project Settings.


#include "Tanimura/MiniGame/PenguinController.h"
#include "EnhancedInputSubsystems.h"

void APenguinController::BeginPlay()
{
    Super::BeginPlay();

    // Enhanced Input Local Player Subsystem を取得して Mapping Context を追加
    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer())) {
        if (DefaultMappingContext) {
            Subsystem->AddMappingContext(DefaultMappingContext, MappingPriority);
        }
    }
}