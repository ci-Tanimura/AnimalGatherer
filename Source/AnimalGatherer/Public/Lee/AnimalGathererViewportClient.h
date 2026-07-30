// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "AnimalGathererViewportClient.generated.h"

/**
 * @brief ローカル2人対戦用の GameViewportClient。
 *        RemapControllerInput をオーバーライドしてゲームパッドの InputDevice を
 *        適切な PlatformUser にマッピングし、2P 対戦の入力ルーティングを実現する。
 *        キーボード → P1、ゲームパッド1 → P2、ゲームパッド2 → P1。
 */
UCLASS()
class ANIMALGATHERER_API UAnimalGathererViewportClient : public UGameViewportClient
{
	GENERATED_BODY()

public:
	UAnimalGathererViewportClient();

	virtual void Init(FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice = true) override;
	virtual void BeginDestroy() override;
	virtual void RemapControllerInput(FInputKeyEventArgs& InOutKeyEvent) override;

	/** @brief デバッグ用。true だとキーボード入力も P2 にルーティング。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDebugKeyboardForP2 = false;

	/** @brief true にするとゲームパッドの入れ替えを無効化（すべてのゲームパッドがデフォルトルーティングに戻る）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bDisableSwapGamepadDevice = false;

private:
	/** @brief P1/P2 の PlatformUserId を LocalPlayer からキャッシュする。 */
	void CachePlayerPlatformUserIds();

	/**
	 * @brief 指定 PlatformUserId に対応する仮想 InputDevice を取得または作成する。
	 * @param UserId 対象の Platform User。
	 * @return マッピング済みの仮想 InputDeviceId。
	 */
	FInputDeviceId GetOrCreateVirtualDeviceForUser(FPlatformUserId UserId);

	/**
	 * @brief 入力デバイスの接続/切断時のコールバック。
	 * @param NewState 新しい接続状態。
	 * @param UserId 対象の Platform User。
	 * @param DeviceId 対象の Input Device。
	 */
	void OnInputDeviceConnectionChange(EInputDeviceConnectionState NewState, FPlatformUserId UserId, FInputDeviceId DeviceId);

	/** @brief 検出順に並んだゲームパッドの InputDeviceId。 */
	TArray<FInputDeviceId> GamepadOrderList;

	/** @brief 既知のゲームパッド InputDeviceId の集合。 */
	TSet<FInputDeviceId> KnownGamepadIds;

	/** @brief PlatformUserId → 割り当て済み仮想 InputDeviceId のキャッシュ。 */
	TMap<FPlatformUserId, FInputDeviceId> CachedVirtualDevices;

	/** @brief 自前で作成した仮想 InputDeviceId の集合（切断通知のフィルタ用）。 */
	TSet<FInputDeviceId> VirtualDeviceIds;

	/** @brief P1 (ControllerId=0) の PlatformUserId キャッシュ。 */
	FPlatformUserId P1PlatformUserId = PLATFORMUSERID_NONE;

	/** @brief P2 (ControllerId=1) の PlatformUserId キャッシュ。 */
	FPlatformUserId P2PlatformUserId = PLATFORMUSERID_NONE;

	/** @brief デバイス接続変更デリゲートのハンドル。 */
	FDelegateHandle DeviceConnectionHandle;
};
