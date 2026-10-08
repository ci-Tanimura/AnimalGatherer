// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "AnimalGathererViewportClient.generated.h"

// 2026.10.06 Lee start
class AAnimalGatherPlayerController;
// 2026.10.06 Lee end

// 2026.10.06 Lee start（B2：物理デバイスごとの入力ホールド状態）
/**
 * @brief 物理 1 デバイス分の押下・軸状態（仮想デバイスは含まない）。
 *        フォーカス喪失・切断時に遮断スナップショットへ複製して使う。
 */
struct FStableDeviceInputState
{
	/** @brief 押下中の物理ボタン。 */
	TSet<FKey> HeldButtons;

	/** @brief 非中立（値 != 0）の物理軸キー。 */
	TSet<FKey> NonNeutralAxes;

	/** @brief 何も保持していないか。 */
	bool IsEmpty() const { return HeldButtons.Num() == 0 && NonNeutralAxes.Num() == 0; }
};
// 2026.10.06 Lee end（B2）

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

	// 2026.10.06 Lee start（B2：物理入力フィルタとフォーカス処理）
	/** @brief キー入力の物理層フィルタ（未知デバイス遮断・ホールドゲート・肩鍵武装通知）。 */
	virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;

	/** @brief 軸入力の物理層フィルタ（AmountDepressed の中立判定で解除）。 */
	virtual bool InputAxis(const FInputKeyEventArgs& EventArgs) override;

	/** @brief フォーカス喪失：両 PC の一時入力を消去し、現在の物理ホールドを遮断対象にする。 */
	// 2026.10.06 Lee start（C4458: 引数 Viewport が基底メンバー UGameViewportClient::Viewport を覆うため InViewport へ改名）
	// virtual void LostFocus(FViewport* Viewport) override;
	virtual void LostFocus(FViewport* InViewport) override;
	// 2026.10.06 Lee end（C4458）

	/** @brief フォーカス回復：一時入力を再消去する（自動解放・自動発動はしない）。 */
	// 2026.10.06 Lee start（C4458: 引数 Viewport が基底メンバー UGameViewportClient::Viewport を覆うため InViewport へ改名）
	// virtual void ReceivedFocus(FViewport* Viewport) override;
	virtual void ReceivedFocus(FViewport* InViewport) override;
	// 2026.10.06 Lee end（C4458）
	// 2026.10.06 Lee end（B2）

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

	// 2026.10.06 Lee start（B2：スロット予約と物理ホールド管理）
	/** @brief 本局で安定割当したゲームパッドスロット（[0] = P2 担当 / [1] = P1 担当）。切断でも保持する。 */
	TArray<FInputDeviceId> GamepadSlotList;

	// 2026.10.06 Lee start（B2R：スロットは局キャッシュ扱い。SyncStableMatchIdentity が新局の初回有効化で初期化する）
	/** @brief 現在のスロットキャッシュが属する World（局の識別）。 */
	TWeakObjectPtr<const UWorld> StableMatchWorld;

	/** @brief キャッシュ確保時の両 PlayerController の身元（LocalPlayer 順＝[0] P1 / [1] P2）。 */
	TArray<TWeakObjectPtr<AAnimalGatherPlayerController>> StableMatchControllers;
	// 2026.10.06 Lee end（B2R）

	/** @brief 管理デバイスの現在の物理ホールド追跡（キーはデバイス id）。 */
	TMap<int32, FStableDeviceInputState> DeviceHoldStates;

	/** @brief フォーカス喪失・切断時に確定した遮断対象（実 Released / 中立で解除）。 */
	TMap<int32, FStableDeviceInputState> DeviceGateStates;

	/** @brief 両スロット予約済みで入力を消費した未知デバイスの警告済み id（1 回限りの警告用）。 */
	TSet<int32> WarnedUnknownDeviceIds;

	/** @brief ビューポートがフォーカスを失っている間 true（ゲームパッド入力を全て消費）。 */
	bool bStableFocusLost = false;

	/** @brief 主玩法（両ローカル Player が AnimalGatherPlayerController）で安定機構が有効か。 */
	bool IsStableDeviceRoutingActive() const;

	/** @brief デバイスのスロット番号を取得する（未登録は false）。 */
	bool FindGamepadSlot(FInputDeviceId DeviceId, int32& OutSlotIndex) const;

	/** @brief P2 仮想デバイスの遅延生成（旧 Remap 内処理の関数化）。 */
	void EnsureP2VirtualDeviceInitialized(const UWorld* ViewportWorld);

	// 2026.10.06 Lee start（B2R：対象にキーボード等の非 gamepad 押下を追加、未知 gamepad は空きスロットへ即時予約）
	/**
	 * @brief 安定機構有効時の物理入力（ゲームパッド＋キーボード等の押下）の追跡・遮断・肩鍵武装通知を行う。
	 *        ゲームパッド：未知デバイスは空きスロットへ即時予約し、初回イベントから完全追跡の対象にする。
	 *        キーボード等の非 gamepad 押下もホールド・遮断・失焦消費の対象（スロットは割当ない、軸は gamepad のみ）。
	 * @param EventArgs Remap 前の生入力（物理デバイス基準）。
	 * @param bAxisEvent true = InputAxis 経由（中立判定で解除）。
	 * @return true の場合その入力を消費する（派発しない）。
	 */
	// /**
	//  * @brief ゲームパッド物理入力の追跡・遮断・肩鍵武装通知を行う。
	//  * @param EventArgs Remap 前の生入力（物理デバイス基準）。
	//  * @param bAxisEvent true = InputAxis 経由（中立判定で解除）。
	//  * @return true の場合その入力を消費する（派発しない）。
	//  */ ←元のコードは消さない（キーボード統合後の説明に置換）
	bool TryGateStableGamepadEvent(const FInputKeyEventArgs& EventArgs, bool bAxisEvent);

	/**
	 * @brief 現在の World と両 PlayerController の身元を局キャッシュと突き合わせる。
	 *        新しい局（新 World / 新 Controller 組）の初回有効化ならスロット・未知デバイス警告・
	 *        ホールド／遮断状態を初期化し、P2 仮想デバイスの束縛を再構築することで、
	 *        新局の最初に検出されたデバイスを P2、次を P1 として再割当可能にする。
	 *        同一局への再呼び出し・切断では何もしない（門と状態を温存、冪等）。
	 */
	void SyncStableMatchIdentity();
	// 2026.10.06 Lee end（B2R）

	/** @brief スロットに対応する AnimalGatherPlayerController を取得する（スロット 0 = P2）。 */
	AAnimalGatherPlayerController* GetStablePCForDevice(FInputDeviceId DeviceId) const;
	// 2026.10.06 Lee end（B2）
};
