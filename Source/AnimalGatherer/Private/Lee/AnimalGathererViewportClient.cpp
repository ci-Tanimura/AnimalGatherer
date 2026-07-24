// Fill out your copyright notice in the Description page of Project Settings.

#include "Lee/AnimalGathererViewportClient.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"

void UAnimalGathererViewportClient::Init(FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice)
{
	Super::Init(WorldContext, OwningGameInstance, bCreateNewAudioDevice);
	UE_LOG(LogTemp, Warning, TEXT("[ViewportClient] === Init ==="));
}

bool UAnimalGathererViewportClient::InputKey(const FInputKeyEventArgs& EventArgs)
{
	const bool bIsGamepad = EventArgs.Key.IsGamepadKey();
	const bool bRouteToP2 = bIsGamepad || (bDebugKeyboardForP2 && !bIsGamepad);

	if (bRouteToP2 && GetWorld() && GetGameInstance() && GetGameInstance()->GetNumLocalPlayers() > 1)
	{
		// P2 の PlayerController を探して直接 InputKey を呼ぶ
		UGameInstance* GI = GetGameInstance();
		if (GI)
		{
			const TArray<ULocalPlayer*>& LocalPlayers = GI->GetLocalPlayers();
			for (const ULocalPlayer* LP : LocalPlayers)
			{
				if (LP->GetControllerId() == 1)
				{
					APlayerController* P2PC = LP->GetPlayerController(GetWorld());
					if (P2PC && P2PC->IsLocalController())
					{
						static bool bLoggedOnce = false;
						if (!bLoggedOnce)
						{
							bLoggedOnce = true;
							UE_LOG(LogTemp, Warning, TEXT("[ViewportClient] Routing %s to P2 Controller"), *EventArgs.Key.ToString());
						}

						return P2PC->InputKey(EventArgs);
					}
					break;
				}
			}
		}
		// P2 の Controller が見つからなければ何もしない
		return false;
	}

	// P1 のデフォルトルーティング（キーボード）
	return Super::InputKey(EventArgs);
}
