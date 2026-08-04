// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "AnimalGathererViewportClient.generated.h"

/**
 * @brief ローカル2人対戦用の GameViewportClient。
 *        RemapControllerInput をオーバーライドし、ゲームパッドの InputDevice ID を
 *        0↔1 で入れ替えることでエンジンの自然なプレイヤー派発に任せる。
 *        キーボード → P1、ゲームパッド1 → P2、ゲームパッド2 → P1。
 *        デバイスの接続/切断にも対応（device 0 の接続状態で入れ替え有効/無効を切替）。
 */
UCLASS()
class ANIMALGATHERER_API UAnimalGathererViewportClient : public UGameViewportClient
{
	GENERATED_BODY()

public:
	UAnimalGathererViewportClient();

	virtual void BeginDestroy() override;

	/**
	 * @brief ゲームパッドの InputDevice ID を 0↔1 で入れ替え、エンジン派発前に
	 *        対象 LocalPlayer を切り替える。キーボードは非 gamepad key なので対象外。
	 * @param InOutEventArgs 入力イベント引数（InputDevice を書き換える）。
	 */
	virtual void RemapControllerInput(FInputKeyEventArgs& InOutEventArgs) override;

	/** @brief true にするとゲームパッドの入れ替えを無効化。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bDisableSwapGamepadDevice = false;

private:
	/**
	 * @brief 入力デバイスの接続/切断時のコールバック。
	 *        切断時に対象ゲームパッドをリストから除外し、P2 担当を再選する。
	 */
	void OnInputDeviceConnectionChange(EInputDeviceConnectionState NewState, FPlatformUserId UserId, FInputDeviceId DeviceId);

	/** @brief デバイス接続変更デリゲートのハンドル。 */
	FDelegateHandle DeviceConnectionHandle;

	/** @brief P2 へのルーティングに使う仮想デバイス（遅延生成）。 */
	FInputDeviceId P2VirtualDevice;

	/** @brief P2VirtualDevice の生成・マッピング完了フラグ。 */
	bool bP2VirtualDeviceInitialized = false;

	/** @brief 起動時に既接続デバイスの user 0 再割当てを完了したか。 */
	bool bInitialReassignDone = false;

	/** @brief 検出順に並んだゲームパッドの InputDeviceId（先頭が P2 担当）。 */
	TArray<FInputDeviceId> GamepadOrderList;

	/** @brief 既知のゲームパッド InputDeviceId の集合。 */
	TSet<FInputDeviceId> KnownGamepadIds;

	/**
	 * @brief デバイスをキーボードと同じ PlatformUser（user 0＝ビューポートフォーカス持ち）に
	 *        強制再割当てする。gamepad が user 1 に追いやられて視口フォーカス経由で
	 *        InputKey に届かないのを是正する。冪等。
	 * @param DeviceId 再割当て対象のデバイス。
	 */
	void ForceDeviceToPrimaryUser(FInputDeviceId DeviceId);
};
