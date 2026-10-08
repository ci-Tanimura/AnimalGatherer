// Fill out your copyright notice in the Description page of Project Settings.

#include "Lee/AnimalGathererViewportClient.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
// 2026.10.06 Lee start（B2：安定機構の PC 参照と物理入力判定のため）
#include "Lee/AnimalGatherPlayerController.h"
#include "GameFramework/PlayerController.h"
// 2026.10.06 Lee end（B2）

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

	// 2026.10.06 Lee start（B2：順序ではなく本局固定スロットでルーティング（検出順の初回割当は維持））
	if (IsStableDeviceRoutingActive())
	{
		// 2026.10.06 Lee start（B2R：局替わり同期（新 World / 新 Controller 組なら初回有効化で再割当可能にする））
		SyncStableMatchIdentity();
		// 2026.10.06 Lee end（B2R）

		int32 SlotIndex = INDEX_NONE;
		if (!FindGamepadSlot(InOutEventArgs.InputDevice, SlotIndex))
		{
			// 空きスロットがあれば新規割当（先頭 = P2、次 = P1）。満員の未知デバイスは物理層で消費済み。
			// 2026.10.06 Lee start（B2R：通常の初回割当は物理層フィルタ側で実施済み。ここは同一スロット確認後の保険）
			// 2026.10.06 Lee end（B2R）
			if (GamepadSlotList.Num() < 2)
			{
				GamepadSlotList.Add(InOutEventArgs.InputDevice);
				SlotIndex = GamepadSlotList.Num() - 1;
				UE_LOG(LogTemp, Warning,
					TEXT("[ViewportClient] stable slot dev=%d -> slot=%d"),
					InOutEventArgs.InputDevice.GetId(), SlotIndex);
			}
			else
			{
				return; // ここには通常到達しない（Filter 側で消費済み）。ルーティングもしない。
			}
		}

		EnsureP2VirtualDeviceInitialized(ViewportWorld);

		if (SlotIndex == 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[ViewportClient] dev=%d -> P2 (stable slot 0)"),
				InOutEventArgs.InputDevice.GetId());
			InOutEventArgs.InputDevice = P2VirtualDevice;
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[ViewportClient] dev=%d -> P1 (stable slot %d)"),
				InOutEventArgs.InputDevice.GetId(), SlotIndex);
		}
		return;
	}
	// 2026.10.06 Lee end（B2）

	// 新規ゲームパッドを検出順に登録
	if (!KnownGamepadIds.Contains(InOutEventArgs.InputDevice))
	{
		KnownGamepadIds.Add(InOutEventArgs.InputDevice);
		GamepadOrderList.Add(InOutEventArgs.InputDevice);
		UE_LOG(LogTemp, Warning, TEXT("[ViewportClient] New gamepad dev=%d order=%d"),
			InOutEventArgs.InputDevice.GetId(), GamepadOrderList.Num() - 1);
	}

	// 2026.10.06 Lee start（B2：P2 仮想デバイスの遅延生成を EnsureP2VirtualDeviceInitialized へ移動）
	// P2 用仮想デバイスを遅延生成して P2 の PlatformUser に紐付ける
	// if (!bP2VirtualDeviceInitialized)
	// {
	// 	P2VirtualDevice = FInputDeviceId::CreateFromInternalId(P2_VIRTUAL_DEVICE_ID);
	// 	bP2VirtualDeviceInitialized = true; // Internal_MapInputDeviceToUser のブロードキャスト再入ガード
	//
	// 	const TArray<ULocalPlayer*>& LPs = ViewportWorld->GetGameInstance()->GetLocalPlayers();
	// 	const FPlatformUserId P2User = (LPs.IsValidIndex(1) && LPs[1])
	// 		? LPs[1]->GetPlatformUserId()
	// 		: FPlatformUserId::CreateFromInternalId(1);
	//
	// 	IPlatformInputDeviceMapper::Get().Internal_MapInputDeviceToUser(
	// 		P2VirtualDevice, P2User, EInputDeviceConnectionState::Connected);
	//
	// 	UE_LOG(LogTemp, Warning,
	// 		TEXT("[ViewportClient] P2 virtual dev=%d -> user(id=%d)"),
	// 		P2VirtualDevice.GetId(), P2User.GetInternalId());
	// } ←元のコードは消さない（同一処理が EnsureP2VirtualDeviceInitialized 内で稼働）
	EnsureP2VirtualDeviceInitialized(ViewportWorld);
	// 2026.10.06 Lee end（B2）

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

	// 2026.10.06 Lee start（B2：安定機構と旧動作の分岐判定）
	const bool bStableRouting = IsStableDeviceRoutingActive();
	// 2026.10.06 Lee end（B2）

	// 2026.10.06 Lee start（B2R：安定機構時は先に局キャッシュ同期（同一局は冪等で無操作、切断でも状態温存））
	if (bStableRouting)
	{
		SyncStableMatchIdentity();
	}
	// 2026.10.06 Lee end（B2R）

	if (NewState == EInputDeviceConnectionState::Connected)
	{
		// 接続されたデバイスを user 0 へ（gamepad が user 1 に割り当てられるのを上書き）
		ForceDeviceToPrimaryUser(DeviceId);

		// 2026.10.06 Lee start（B2：再接続時も対応 PC の連移・スキル押下状態を安全解放）
		if (bStableRouting)
		{
			if (AAnimalGatherPlayerController* PC = GetStablePCForDevice(DeviceId))
			{
				PC->ResetTransientInput(true); // 回数・クールダウンは保持
			}
		}
		// 2026.10.06 Lee end（B2）
	}
	// 2026.10.06 Lee start（B2：安定機構では切断でもスロット予約を保持し、別デバイスの横入りを防ぐ）
	else if (NewState == EInputDeviceConnectionState::Disconnected && bStableRouting)
	{
		if (GamepadSlotList.Contains(DeviceId))
		{
			// 現在の物理ホールドを遮断対象へ固定（再接続後の実 Released / 中立到達まで有効）。
			FStableDeviceInputState* Hold = DeviceHoldStates.Find(DeviceId.GetId());
			if (Hold != nullptr && !Hold->IsEmpty())
			{
				DeviceGateStates.Add(DeviceId.GetId(), *Hold);
			}

			// 対応 PC の連移・スキル押下状態のみ解放（回数・クールダウンは保持）。
			if (AAnimalGatherPlayerController* PC = GetStablePCForDevice(DeviceId))
			{
				PC->ResetTransientInput(true);
			}

			UE_LOG(LogTemp, Warning,
				TEXT("[ViewportClient] gamepad dev=%d disconnected. stable slot retained (%d slots)."),
				DeviceId.GetId(), GamepadSlotList.Num());
		}
	}
	// 2026.10.06 Lee end（B2）
	else if (NewState == EInputDeviceConnectionState::Disconnected && KnownGamepadIds.Contains(DeviceId))
	{
		// 旧動作（安定機構無効時のみ到達）：検出順リストから除外して P2 担当を再選
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

//==============================================================================
// 2026.10.06 Lee start（B2：安定デバイス割当と物理入力ゲート）
//==============================================================================

bool UAnimalGathererViewportClient::InputKey(const FInputKeyEventArgs& EventArgs)
{
	// Remap 前の生入力（物理デバイス基準）で遮断判定する。
	if (TryGateStableGamepadEvent(EventArgs, /*bAxisEvent*/ false))
	{
		return true;
	}
	return Super::InputKey(EventArgs);
}

bool UAnimalGathererViewportClient::InputAxis(const FInputKeyEventArgs& EventArgs)
{
	// 軸は AmountDepressed の中立到達で遮断を解除する。
	if (TryGateStableGamepadEvent(EventArgs, /*bAxisEvent*/ true))
	{
		return true;
	}
	return Super::InputAxis(EventArgs);
}

// 2026.10.06 Lee start（C4458: 引数 Viewport が基底メンバー UGameViewportClient::Viewport を覆うため InViewport へ改名。旧定義はコメントとして保持）
// void UAnimalGathererViewportClient::LostFocus(FViewport* Viewport)
// {
// 	Super::LostFocus(Viewport);
void UAnimalGathererViewportClient::LostFocus(FViewport* InViewport)
{
	Super::LostFocus(InViewport);
// 2026.10.06 Lee end（C4458）

	// 2026.10.06 Lee start（B2：フォーカス喪失で物理ホールドを遮断対象化し、両 PC の一時入力を消去）
	const bool bStableRouting = IsStableDeviceRoutingActive();
	bStableFocusLost = bStableRouting;
	if (!bStableRouting)
	{
		return;
	}

	// 各管理デバイスの現在の押下・非中立軸を遮断スナップショットへ複製する
	// （Enhanced Input の Flush 由来の Completed / Canceled で早期再武装されるのを物理層で防ぐ）。
	// 2026.10.06 Lee start（B2R：スロットだけでなく全物理 HoldStates（キーボード含む）から門を生成）
	// for (const FInputDeviceId& Dev : GamepadSlotList)
	// {
	// 	FStableDeviceInputState* Hold = DeviceHoldStates.Find(Dev.GetId());
	// 	if (Hold != nullptr && !Hold->IsEmpty())
	// 	{
	// 		DeviceGateStates.Add(Dev.GetId(), *Hold);
	// 	}
	// } ←元のコードは消さない（全デバイス走査へ置換）
	for (auto& HoldPair : DeviceHoldStates)
	{
		if (!HoldPair.Value.IsEmpty())
		{
			DeviceGateStates.Add(HoldPair.Key, HoldPair.Value);
		}
	}
	// 2026.10.06 Lee end（B2R）

	// 両 PC の連移・スキル押下状態を消去（回数・クールダウンは保持、自動解放はしない）。
	if (UGameInstance* GI = GetGameInstance())
	{
		for (const ULocalPlayer* LP : GI->GetLocalPlayers())
		{
			if (AAnimalGatherPlayerController* PC = Cast<AAnimalGatherPlayerController>(LP ? LP->PlayerController : nullptr))
			{
				PC->ResetTransientInput(true);
			}
		}
	}
	// 2026.10.06 Lee end（B2）
}

// 2026.10.06 Lee start（C4458: 引数 Viewport が基底メンバー UGameViewportClient::Viewport を覆うため InViewport へ改名。旧定義はコメントとして保持）
// void UAnimalGathererViewportClient::ReceivedFocus(FViewport* Viewport)
// {
// 	Super::ReceivedFocus(Viewport);
void UAnimalGathererViewportClient::ReceivedFocus(FViewport* InViewport)
{
	Super::ReceivedFocus(InViewport);
// 2026.10.06 Lee end（C4458）

	// 2026.10.06 Lee start（B2：回復時も一時入力を再消去。再武装は実 Released / 新規押下まで待つ）
	bStableFocusLost = false; // 活性判定と無関係に消費モードは必ず解除する

	if (!IsStableDeviceRoutingActive())
	{
		return;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		for (const ULocalPlayer* LP : GI->GetLocalPlayers())
		{
			if (AAnimalGatherPlayerController* PC = Cast<AAnimalGatherPlayerController>(LP ? LP->PlayerController : nullptr))
			{
				PC->ResetTransientInput(true);
			}
		}
	}
	// 2026.10.06 Lee end（B2）
}

bool UAnimalGathererViewportClient::IsStableDeviceRoutingActive() const
{
	// 手動無効時は旧動作（検出順・自然派発）を完全に維持する。
	if (bDisableSwapGamepadDevice)
	{
		return false;
	}

	const UWorld* ViewportWorld = GetWorld();
	UGameInstance* GI = ViewportWorld ? ViewportWorld->GetGameInstance() : nullptr;
	if (GI == nullptr || GI->GetNumLocalPlayers() < 2)
	{
		return false;
	}

	// 両ローカル Player の Controller が本作 Controller の場合のみ有効（ミニゲーム等は旧動作）。
	for (const ULocalPlayer* LP : GI->GetLocalPlayers())
	{
		if (LP == nullptr || Cast<AAnimalGatherPlayerController>(LP->PlayerController) == nullptr)
		{
			return false;
		}
	}
	return true;
}

bool UAnimalGathererViewportClient::FindGamepadSlot(FInputDeviceId DeviceId, int32& OutSlotIndex) const
{
	OutSlotIndex = GamepadSlotList.IndexOfByKey(DeviceId);
	return OutSlotIndex != INDEX_NONE;
}

void UAnimalGathererViewportClient::EnsureP2VirtualDeviceInitialized(const UWorld* ViewportWorld)
{
	if (bP2VirtualDeviceInitialized || ViewportWorld == nullptr)
	{
		return;
	}

	// 旧 Remap 内にあった遅延生成処理（元コードは RemapControllerInput 側にコメント保持）。
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

AAnimalGatherPlayerController* UAnimalGathererViewportClient::GetStablePCForDevice(FInputDeviceId DeviceId) const
{
	int32 SlotIndex = INDEX_NONE;
	if (!FindGamepadSlot(DeviceId, SlotIndex))
	{
		return nullptr;
	}

	const UWorld* ViewportWorld = GetWorld();
	UGameInstance* GI = ViewportWorld ? ViewportWorld->GetGameInstance() : nullptr;
	if (GI == nullptr)
	{
		return nullptr;
	}

	const TArray<ULocalPlayer*>& LPs = GI->GetLocalPlayers();
	const int32 LPIndex = (SlotIndex == 0) ? 1 : 0; // スロット 0 = P2、その他 = P1。
	if (!LPs.IsValidIndex(LPIndex) || LPs[LPIndex] == nullptr)
	{
		return nullptr;
	}
	return Cast<AAnimalGatherPlayerController>(LPs[LPIndex]->PlayerController);
}

bool UAnimalGathererViewportClient::TryGateStableGamepadEvent(const FInputKeyEventArgs& EventArgs, bool bAxisEvent)
{
	// if (!IsStableDeviceRoutingActive() || !EventArgs.Key.IsGamepadKey())
	// {
	// 	return false;
	// } ←元のコードは消さない（active 判定と gamepad 判定を分割して非 gamepad 押下も追跡へ）
	if (!IsStableDeviceRoutingActive())
	{
		return false;
	}

	// 2026.10.06 Lee start（B2R：活性確認後に局キャッシュ同期（新 World / 新 Controller 組の初回有効化で作り直す。手動無効時は触れない））
	SyncStableMatchIdentity();
	// 2026.10.06 Lee end（B2R）

	if (EventArgs.InputDevice.GetId() == P2_VIRTUAL_DEVICE_ID)
	{
		return false; // 仮想デバイスは物理追跡の対象外（Remap 後にのみ存在）。
	}

	const FKey Key = EventArgs.Key;
	const bool bGamepadKey = Key.IsGamepadKey();

	// 2026.10.06 Lee start（B2R：非 gamepad の軸（マウス軸等）は軸門・ホールド追跡の対象外）
	if (bAxisEvent && !bGamepadKey)
	{
		return false;
	}
	// 2026.10.06 Lee end（B2R）

	const int32 DevId = EventArgs.InputDevice.GetId();

	// 2026.10.06 Lee start（B2R：未知 gamepad は空きスロットへ即時予約してから完全追跡へ進む）
	// int32 SlotIndex = INDEX_NONE;
	// const bool bManaged = FindGamepadSlot(EventArgs.InputDevice, SlotIndex);
	//
	// // 未知デバイス：両スロット予約済みなら入力を消費する（スロットは奪わない・再割当もしない）。
	// if (!bManaged)
	// {
	// 	if (GamepadSlotList.Num() >= 2)
	// 	{
	// 		if (!WarnedUnknownDeviceIds.Contains(DevId))
	// 		{
	// 			WarnedUnknownDeviceIds.Add(DevId);
	// 			UE_LOG(LogTemp, Warning,
	// 				TEXT("[ViewportClient] unknown gamepad dev=%d input dropped (both slots reserved)"), DevId);
	// 		}
	// 		return true;
	// 	}
	// 	return false; // 空きスロットあり：Remap 側で登録・ルーティングされる。
	// } ←元のコードは消さない（空きスロット時はここで即予約し、追跡・通知を継続する）
	int32 SlotIndex = INDEX_NONE;
	const bool bManaged = !bGamepadKey || FindGamepadSlot(EventArgs.InputDevice, SlotIndex);

	if (bGamepadKey && !bManaged)
	{
		// 未知デバイス：両スロット予約済みなら入力を消費する（スロットは奪わない・再割当もしない）。
		if (GamepadSlotList.Num() >= 2)
		{
			if (!WarnedUnknownDeviceIds.Contains(DevId))
			{
				WarnedUnknownDeviceIds.Add(DevId);
				UE_LOG(LogTemp, Warning,
					TEXT("[ViewportClient] unknown gamepad dev=%d input dropped (both slots reserved)"), DevId);
			}
			return true;
		}

		// 空きスロットへ即時予約（先頭 = P2、次 = P1）。Remap 側は同一スロットを参照し再割当しない。
		// 初回の Pressed / 軸イベントもホールド追跡・肩鍵武装通知の対象になる。キーボードは対象外。
		GamepadSlotList.Add(EventArgs.InputDevice);
		SlotIndex = GamepadSlotList.Num() - 1;
		UE_LOG(LogTemp, Warning,
			TEXT("[ViewportClient] stable slot dev=%d -> slot=%d (first physical event)"),
			DevId, SlotIndex);
	}
	// 2026.10.06 Lee end（B2R）

	// 2026.10.06 Lee start（B2R：キーボード等の非 gamepad 押下も同じ物理ホールド管理へ統合（スロット不割当・P1 自然派発のまま））
	// 管理デバイス：先に物理ホールドの実態を追跡する（消費時も状態を実態へ合わせる）。
	// 2026.10.06 Lee end（B2R）
	FStableDeviceInputState& Hold = DeviceHoldStates.FindOrAdd(DevId);
	FStableDeviceInputState& Gate = DeviceGateStates.FindOrAdd(DevId);

	bool bBlocked = false;

	if (bAxisEvent)
	{
		const bool bNeutral = FMath::IsNearlyZero(EventArgs.AmountDepressed);
		if (bNeutral)
		{
			Hold.NonNeutralAxes.Remove(Key);
		}
		else
		{
			Hold.NonNeutralAxes.Add(Key);
		}

		// 2026.10.06 Lee start（B2R：失焦中の新規非中立軸も遮断対象へ追加（復帰後の誤連移を防止））
		if (bStableFocusLost && !bNeutral && !Gate.NonNeutralAxes.Contains(Key))
		{
			Gate.NonNeutralAxes.Add(Key);
		}
		// 2026.10.06 Lee end（B2R）

		if (Gate.NonNeutralAxes.Contains(Key))
		{
			if (bNeutral)
			{
				Gate.NonNeutralAxes.Remove(Key); // 中立到達で遮断解除し、この中立イベントは通す。
			}
			else
			{
				bBlocked = true; // 中立へ戻るまで軸を遮断。
			}
		}
	}
	else
	{
		const bool bDown = (EventArgs.Event == IE_Pressed || EventArgs.Event == IE_Repeat);
		const bool bUp = (EventArgs.Event == IE_Released);
		const bool bWasHeld = Hold.HeldButtons.Contains(Key);

		// 2026.10.06 Lee start（B2R：入口時点の遮断状態を保存（fresh 判定・失焦中の門追加に使用））
		const bool bWasGated = Gate.HeldButtons.Contains(Key);
		// 2026.10.06 Lee end（B2R）

		if (bDown)
		{
			Hold.HeldButtons.Add(Key);
		}
		else if (bUp)
		{
			Hold.HeldButtons.Remove(Key);
		}

		// 2026.10.06 Lee start（B2R：失焦中の新規押下も遮断対象へ追加（復帰後の誤連移を防止））
		if (bStableFocusLost && bDown && !bWasGated)
		{
			Gate.HeldButtons.Add(Key);
		}
		// 2026.10.06 Lee end（B2R）

		// 2026.10.06 Lee start（B2R：fresh は真の IE_Pressed 且つ未保持・未遮断のみ（IE_Repeat は fresh に数えない））
		// 肩鍵：物理解放、または遮断対象外の新規押下（生履歴に無い FreshPressed）で再武装を通知する。
		// 既に押下中の連続 Pressed は再武装しない。Action が Flush 済みでも Released では必ず通知する。
		// if (Key == EKeys::Gamepad_LeftShoulder || Key == EKeys::Gamepad_RightShoulder)
		// {
		// 	const bool bSpeed = (Key == EKeys::Gamepad_RightShoulder);
		// 	if (bUp || (bDown && !bWasHeld && !Gate.HeldButtons.Contains(Key)))
		// 	{
		// 		if (AAnimalGatherPlayerController* PC = GetStablePCForDevice(EventArgs.InputDevice))
		// 		{
		// 			PC->NotifySkillButtonReleased(bSpeed);
		// 		}
		// 	}
		// } ←元のコードは消さない（IE_Repeat を fresh に数えない条件へ置換）
		if (Key == EKeys::Gamepad_LeftShoulder || Key == EKeys::Gamepad_RightShoulder)
		{
			const bool bSpeed = (Key == EKeys::Gamepad_RightShoulder);
			const bool bFreshPressed = (EventArgs.Event == IE_Pressed) && !bWasHeld && !bWasGated;
			if (bUp || bFreshPressed)
			{
				if (AAnimalGatherPlayerController* PC = GetStablePCForDevice(EventArgs.InputDevice))
				{
					PC->NotifySkillButtonReleased(bSpeed);
				}
			}
		}
		// 2026.10.06 Lee end（B2R）

		if (Gate.HeldButtons.Contains(Key))
		{
			if (bUp)
			{
				Gate.HeldButtons.Remove(Key); // 実 Released で遮断解除し、Released は通す。
			}
			else
			{
				bBlocked = true; // 実 Released まで Pressed / Repeat を遮断。
			}
		}
	}

	// 2026.10.06 Lee start（B2R：失焦中の消費対象はゲームパッドに加え追跡済みの押下入力（キーボード等）も含む）
	// フォーカス喪失中は追跡・通知を済ませた上で、ゲームパッド入力を全て消費する（派発しない）。
	// 2026.10.06 Lee end（B2R）
	if (bStableFocusLost)
	{
		return true;
	}
	return bBlocked;
}

// 2026.10.06 Lee start（B2R：局単位スロットキャッシュの同期ヘルパー）
void UAnimalGathererViewportClient::SyncStableMatchIdentity()
{
	const UWorld* ViewportWorld = GetWorld();
	UGameInstance* GI = ViewportWorld ? ViewportWorld->GetGameInstance() : nullptr;
	if (GI == nullptr || GI->GetNumLocalPlayers() < 2)
	{
		return; // 前提不成立の間はキャッシュに触らない（局替わり判定も保留）
	}

	// 現在の両 Controller の身元を収集（どちらかが非本作 Controller なら安定機構の局ではない）。
	TArray<TWeakObjectPtr<AAnimalGatherPlayerController>> CurrentControllers;
	for (const ULocalPlayer* LP : GI->GetLocalPlayers())
	{
		AAnimalGatherPlayerController* PC = Cast<AAnimalGatherPlayerController>(LP ? LP->PlayerController : nullptr);
		if (PC == nullptr)
		{
			return; // ミニゲーム等：旧動作に任せる（キャッシュは温存）
		}
		CurrentControllers.Add(PC);
	}

	// 同一 World ＋同一 Controller 組＝同一局：何もしない（冪等。門・ホールド・スロットを温存）。
	bool bSameIdentity = (StableMatchWorld.Get() == ViewportWorld) &&
		(StableMatchControllers.Num() == CurrentControllers.Num());
	if (bSameIdentity)
	{
		for (int32 i = 0; i < CurrentControllers.Num(); ++i)
		{
			if (StableMatchControllers[i] != CurrentControllers[i])
			{
				bSameIdentity = false;
				break;
			}
		}
	}
	if (bSameIdentity)
	{
		return;
	}

	// 新しい局（新 World / 新 Controller 組）の初回有効化：旧局のスロット封鎖を解いて作り直す。
	UE_LOG(LogTemp, Warning,
		TEXT("[ViewportClient] stable match cache reset (new world/controller pair). old slots=%d"),
		GamepadSlotList.Num());

	StableMatchWorld = ViewportWorld;
	StableMatchControllers = CurrentControllers;

	GamepadSlotList.Empty();        // 旧局のスロット（旧デバイス恒久拒絶の原因）を破棄
	WarnedUnknownDeviceIds.Empty(); // 未知デバイス警告も局単位でやり直す
	DeviceHoldStates.Empty();       // 旧局のホールド追跡を破棄
	DeviceGateStates.Empty();       // 旧局の遮断状態を破棄（新局の門は以降の失焦・切断で生成）

	// P2 仮想デバイスの束縛を新局の LocalPlayer 構成で作り直す（EnsureP2VirtualDeviceInitialized は冪等）。
	bP2VirtualDeviceInitialized = false;
	P2VirtualDevice = FInputDeviceId();
	EnsureP2VirtualDeviceInitialized(ViewportWorld);
}
// 2026.10.06 Lee end（B2R）

//==============================================================================
// 2026.10.06 Lee end（B2：安定デバイス割当と物理入力ゲート）
//==============================================================================
