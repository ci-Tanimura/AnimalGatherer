// Fill out your copyright notice in the Description page of Project Settings.

#include "Lee/AnimalGathererViewportClient.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"

UAnimalGathererViewportClient::UAnimalGathererViewportClient()
{
}

void UAnimalGathererViewportClient::Init(FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice)
{
	Super::Init(WorldContext, OwningGameInstance, bCreateNewAudioDevice);

	// 入力デバイスの接続/切断を監視
	IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
	DeviceConnectionHandle = DeviceMapper.GetOnInputDeviceConnectionChange().AddUObject(
		this, &UAnimalGathererViewportClient::OnInputDeviceConnectionChange);

	// P1/P2 の PlatformUserId をキャッシュ
	CachePlayerPlatformUserIds();

	UE_LOG(LogTemp, Warning, TEXT("[ViewportClient] === Init: device connection delegate bound ==="));
}

void UAnimalGathererViewportClient::BeginDestroy()
{
	if (DeviceConnectionHandle.IsValid())
	{
		IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().Remove(DeviceConnectionHandle);
		DeviceConnectionHandle.Reset();
	}
	Super::BeginDestroy();
}

void UAnimalGathererViewportClient::CachePlayerPlatformUserIds()
{
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	const TArray<ULocalPlayer*>& LocalPlayers = GI->GetLocalPlayers();
	for (const ULocalPlayer* LP : LocalPlayers)
	{
		if (LP->GetControllerId() == 0)
		{
			P1PlatformUserId = LP->GetPlatformUserId();
		}
		else if (LP->GetControllerId() == 1)
		{
			P2PlatformUserId = LP->GetPlatformUserId();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[ViewportClient] Cached P1 PlatformUserId=%d, P2 PlatformUserId=%d"),
		P1PlatformUserId.GetInternalId(), P2PlatformUserId.GetInternalId());
}

FInputDeviceId UAnimalGathererViewportClient::GetOrCreateVirtualDeviceForUser(FPlatformUserId UserId)
{
	// 既存のキャッシュを確認
	if (const FInputDeviceId* Existing = CachedVirtualDevices.Find(UserId))
	{
		return *Existing;
	}

	IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();

	// 新しい仮想 InputDeviceId を割り当て
	const FInputDeviceId NewDeviceId = Mapper.AllocateNewInputDeviceId();

	// 指定 PlatformUser にマッピング
	Mapper.Internal_MapInputDeviceToUser(NewDeviceId, UserId, EInputDeviceConnectionState::Connected);

	// キャッシュに保存
	CachedVirtualDevices.Add(UserId, NewDeviceId);
	VirtualDeviceIds.Add(NewDeviceId);

	UE_LOG(LogTemp, Log, TEXT("[ViewportClient] Created virtual device (id=%d) for PlatformUser %d"),
		NewDeviceId.GetId(), UserId.GetInternalId());

	return NewDeviceId;
}

void UAnimalGathererViewportClient::OnInputDeviceConnectionChange(
	EInputDeviceConnectionState NewState, FPlatformUserId UserId, FInputDeviceId DeviceId)
{
	// 自分で作成した仮想デバイスは無視
	if (VirtualDeviceIds.Contains(DeviceId))
	{
		return;
	}

	if (NewState == EInputDeviceConnectionState::Disconnected)
	{
		if (KnownGamepadIds.Contains(DeviceId))
		{
			GamepadOrderList.Remove(DeviceId);
			KnownGamepadIds.Remove(DeviceId);

			UE_LOG(LogTemp, Warning, TEXT("[ViewportClient] Gamepad device %d disconnected. %d gamepad(s) remain."),
				DeviceId.GetId(), GamepadOrderList.Num());

			// すべてのゲームパッドが切断されたら入れ替えを無効化
			if (GamepadOrderList.Num() == 0)
			{
				bDisableSwapGamepadDevice = true;
				UE_LOG(LogTemp, Warning, TEXT("[ViewportClient] No gamepads connected → bDisableSwapGamepadDevice = true"));
			}
		}
	}
}

void UAnimalGathererViewportClient::RemapControllerInput(FInputKeyEventArgs& InOutKeyEvent)
{
	if (InOutKeyEvent.Key.IsGamepadKey())
	{
		const FInputDeviceId RealDeviceId = InOutKeyEvent.InputDevice;

		// 初回検出のゲームパッドを登録
		if (!KnownGamepadIds.Contains(RealDeviceId))
		{
			KnownGamepadIds.Add(RealDeviceId);
			GamepadOrderList.Add(RealDeviceId);

			UE_LOG(LogTemp, Warning, TEXT("[ViewportClient] New gamepad detected: device=%d, order index=%d"),
				RealDeviceId.GetId(), GamepadOrderList.Num() - 1);

			// ゲームパッドが再接続されたら入れ替えを再有効化
			if (GamepadOrderList.Num() == 1 && bDisableSwapGamepadDevice)
			{
				bDisableSwapGamepadDevice = false;
				UE_LOG(LogTemp, Warning, TEXT("[ViewportClient] Gamepad reconnected → bDisableSwapGamepadDevice = false"));
			}
		}

		// 入れ替え無効時はデフォルトルーティング（すべて P1）
		if (bDisableSwapGamepadDevice)
		{
			return;
		}

		const int32 GamepadIndex = GamepadOrderList.IndexOfByKey(RealDeviceId);

		if (GamepadIndex == 0)
		{
			// 1つ目のゲームパッド → P2 (LP1)
			if (P2PlatformUserId == PLATFORMUSERID_NONE)
			{
				CachePlayerPlatformUserIds();
			}
			if (P2PlatformUserId != PLATFORMUSERID_NONE)
			{
				InOutKeyEvent.InputDevice = GetOrCreateVirtualDeviceForUser(P2PlatformUserId);
			}
		}
		else if (GamepadIndex == 1)
		{
			// 2つ目のゲームパッド → P1 (LP0)
			if (P1PlatformUserId == PLATFORMUSERID_NONE)
			{
				CachePlayerPlatformUserIds();
			}
			if (P1PlatformUserId != PLATFORMUSERID_NONE)
			{
				InOutKeyEvent.InputDevice = GetOrCreateVirtualDeviceForUser(P1PlatformUserId);
			}
		}
		// GamepadIndex >= 2: そのまま（デフォルトで P1）
	}
	else
	{
		// ゲームパッド以外（キーボード/マウス）
		if (bDebugKeyboardForP2)
		{
			if (P2PlatformUserId == PLATFORMUSERID_NONE)
			{
				CachePlayerPlatformUserIds();
			}
			if (P2PlatformUserId != PLATFORMUSERID_NONE)
			{
				InOutKeyEvent.InputDevice = GetOrCreateVirtualDeviceForUser(P2PlatformUserId);
			}
		}
	}
}
