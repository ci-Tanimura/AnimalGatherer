// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "AnimalGathererViewportClient.generated.h"

/**
 * @brief ローカル2人対戦用の GameViewportClient。
 *        UE5 のデフォルトでは1つ目のゲームパッドが P1 にルーティングされてしまう。
 *        本クラスでは InputKey をオーバーライドし、ゲームパッド入力を
 *        直接 P2 (ControllerId=1) の PlayerController に転送する。
 */
UCLASS()
class ANIMALGATHERER_API UAnimalGathererViewportClient : public UGameViewportClient
{
	GENERATED_BODY()

public:
	virtual void Init(FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice = true) override;
	virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;

	/** @brief デバッグ用。true だとキーボード入力も P2 にルーティング。 */
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDebugKeyboardForP2 = false;
};
