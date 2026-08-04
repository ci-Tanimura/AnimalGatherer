// Fill out your copyright notice in the Description page of Project Settings.

#include "Lee/AnimalGathererViewportClient.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"

namespace
{
	/** @brief P2 ルーティング用の仮想デバイス id。実機の手柄 id（1,2,3…）と衝突しない高位の値。 */
	constexpr int32 P2_VIRTUAL_DEVICE_ID = 1000;
}

UAnimalGathererViewportClient::UAnimalGathererViewportClient()
{
	// 入力デバイスの接続/切断を監視（构造期に一度だけバインド＝重複登録を回避）
	IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
	DeviceConnectionHandle = Mapper.GetOnInputDeviceConnectionChange().AddUObject(
		this, &UAnimalGathererViewportClient::OnInputDeviceConnectionChange);
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

void UAnimalGathererViewportClient::RemapControllerInput(FInputKeyEventArgs& InOutEventArgs)
{
	Super::RemapControllerInput(InOutEventArgs);

	// 起動直後に既接続の全デバイスを user 0（キーボード＝視口フォーカス持ち）へ強制再割当て。
	// gamepad2 が user 1 に割り当てられて視口フォーカス経由で InputKey に届かないのを是正する。
	// 任意のイベント（多くはマウス/キーボード）で一度だけ走る。
	if (!bInitialReassignDone)
	{
		bInitialReassignDone = true; // ForceDeviceToPrimaryUser がブロードキャスト→再入するため先に立てる
		TArray<FInputDeviceId> Devices;
		IPlatformInputDeviceMapper::Get().GetAllConnectedInputDevices(Devices);
		for (const FInputDeviceId& Dev : Devices)
		{
			ForceDeviceToPrimaryUser(Dev);
		}
	}

	const UWorld* ViewportWorld = GetWorld();
	const int32 NumLocalPlayers = ViewportWorld ? ViewportWorld->GetGameInstance()->GetNumLocalPlayers() : 0;
	const bool bIsGamepad = InOutEventArgs.Key.IsGamepadKey();

	UE_LOG(LogTemp, Warning,
		TEXT("[ViewportClient] Remap: dev=%d key=%s gamepad=%d numLP=%d swapDisabled=%d"),
		InOutEventArgs.InputDevice.GetId(), *InOutEventArgs.Key.ToString(),
		bIsGamepad, NumLocalPlayers, bDisableSwapGamepadDevice);

	// ローカル2プレイヤー未満、非ゲームパッド、または手動無効時は対象外（キーボードは自然に P1 へ）
	if (NumLocalPlayers <= 1 || !bIsGamepad || bDisableSwapGamepadDevice)
	{
		return;
	}

	// 新規ゲームパッドを検出順に登録
	if (!KnownGamepadIds.Contains(InOutEventArgs.InputDevice))
	{
		KnownGamepadIds.Add(InOutEventArgs.InputDevice);
		GamepadOrderList.Add(InOutEventArgs.InputDevice);
		UE_LOG(LogTemp, Warning, TEXT("[ViewportClient] New gamepad dev=%d order=%d"),
			InOutEventArgs.InputDevice.GetId(), GamepadOrderList.Num() - 1);
	}

	// P2 用仮想デバイスを遅延生成して P2 の PlatformUser に紐付ける
	if (!bP2VirtualDeviceInitialized)
	{
		P2VirtualDevice = FInputDeviceId::CreateFromInternalId(P2_VIRTUAL_DEVICE_ID);
		bP2VirtualDeviceInitialized = true; // Internal_MapInputDeviceToUser のブロードキャスト再入ガード

		const TArray<ULocalPlayer*>& LPs = ViewportWorld->GetGameInstance()->GetLocalPlayers();
		const FPlatformUserId P2User = (LPs.IsValidIndex(1) && LPs[1])
			? LPs[1]->GetPlatformUserId()
			: FPlatformUserId::CreateFromInternalId(1);

		IPlatformInputDeviceMapper::Get().Internal_MapInputDeviceToUser(
			P2VirtualDevice, P2User, EInputDeviceConnectionState::Connected);

		UE_LOG(LogTemp, Warning,
			TEXT("[ViewportClient] P2 virtual dev=%d -> user(id=%d)"),
			P2VirtualDevice.GetId(), P2User.GetInternalId());
	}

	// 先頭（最古検出）のゲームパッドを P2 へ、それ以外は自然に P1
	if (GamepadOrderList.Num() > 0 && InOutEventArgs.InputDevice == GamepadOrderList[0])
	{
		UE_LOG(LogTemp, Warning, TEXT("[ViewportClient] dev=%d -> P2"), InOutEventArgs.InputDevice.GetId());
		InOutEventArgs.InputDevice = P2VirtualDevice;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ViewportClient] dev=%d -> P1 (natural)"), InOutEventArgs.InputDevice.GetId());
	}
}

void UAnimalGathererViewportClient::OnInputDeviceConnectionChange(
	EInputDeviceConnectionState NewState, FPlatformUserId UserId, FInputDeviceId DeviceId)
{
	// 仮想デバイス（P2 ルーティング用）は常に無視。
	// インスタンス状態ではなく id で判定＝過去 PIE の stale リスナ等、どのインスタンスで呼ばれても確実に弾く。
	if (DeviceId.GetId() == P2_VIRTUAL_DEVICE_ID)
	{
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[ViewportClient] device %d %s (user=%d)"),
		DeviceId.GetId(),
		NewState == EInputDeviceConnectionState::Connected ? TEXT("connected") : TEXT("disconnected"),
		UserId.GetInternalId());

	if (NewState == EInputDeviceConnectionState::Connected)
	{
		// 接続されたデバイスを user 0 へ（gamepad が user 1 に割り当てられるのを上書き）
		ForceDeviceToPrimaryUser(DeviceId);
	}
	else if (NewState == EInputDeviceConnectionState::Disconnected && KnownGamepadIds.Contains(DeviceId))
	{
		KnownGamepadIds.Remove(DeviceId);
		GamepadOrderList.Remove(DeviceId);
		UE_LOG(LogTemp, Warning,
			TEXT("[ViewportClient] gamepad dev=%d disconnected. %d remain."),
			DeviceId.GetId(), GamepadOrderList.Num());
	}
}

void UAnimalGathererViewportClient::ForceDeviceToPrimaryUser(FInputDeviceId DeviceId)
{
	// P2 仮想デバイスは user 1 に留める必要があるため対象外（いかなるインスタンスから呼ばれても）
	if (DeviceId.GetId() == P2_VIRTUAL_DEVICE_ID)
	{
		return;
	}

	IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
	// キーボード（デフォルトデバイス）の所属 user を“プライマリ”（視口フォーカス持ち）とする
	const FPlatformUserId PrimaryUser = Mapper.GetUserForInputDevice(Mapper.GetDefaultInputDevice());
	const FPlatformUserId CurrentUser = Mapper.GetUserForInputDevice(DeviceId);
	if (CurrentUser != PrimaryUser)
	{
		Mapper.Internal_MapInputDeviceToUser(DeviceId, PrimaryUser, EInputDeviceConnectionState::Connected);
		UE_LOG(LogTemp, Warning,
			TEXT("[ViewportClient] device %d reassigned user %d -> %d"),
			DeviceId.GetId(), CurrentUser.GetInternalId(), PrimaryUser.GetInternalId());
	}
}
