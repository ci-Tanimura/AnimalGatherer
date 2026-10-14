// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "GameFramework/Pawn.h"
#include "AnimalBase.generated.h"

UCLASS()
class ANIMALGATHERER_API AAnimalBase : public APawn
{
	GENERATED_BODY()

public:
	AAnimalBase();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	//動物を動かすための基準コンポーネント
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animal Components")
	TObjectPtr<class USceneComponent> SceneRoot;

	//1秒あたりの移動速度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal Settings", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MoveSpeed = 100.0f;

	//初期移動方向
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal Movement")
	FVector InitialMoveDirection = FVector::ForwardVector;

	//現在の移動方向
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animal Movement")
	FVector CurrentMoveDirection = FVector::ZeroVector;

	//移動方向に合わせて体の向きを変えるか
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal Movement")
	bool bRotateToMoveDirection = true;

	//モデルの正面がずれている場合に補正するYaw角度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal Movement")
	float FacingYawOffset = -90.0f;

	//1タイルのワールドサイズ
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal Map", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float TileSize = 100.0f;

	//タイル情報を問い合わせる対象
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal Map", meta = (ExposeOnSpawn = "true"))
	TObjectPtr<AActor> MapActor;

	//移動方向を変更する
	UFUNCTION(BlueprintCallable, Category = "Animal Movement")
	void SetMoveDirection(FVector NewMoveDirection);

	//タイル情報を問い合わせるマップを設定する
	UFUNCTION(BlueprintCallable, Category = "Animal Map")
	void SetMapActor(AActor* NewMapActor);

	//タイル中心からこの距離以内に入ったら方向を取得する
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal Map", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float DirectionReadTolerance = 10.0f;

	// 2026.07.31 Gu start
	// 動物がゴールに入った時再生する効果音
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	class USoundBase* GoalSound;
	// 2026.07.31 Gu end

	// 2026.10.06 Lee start（共有加速・試合段階に対応する公開 API 群）
	/** @brief 最終的なマップ参照と TileSize 確定後に共有加速状態へ冪等に接続する
	 *         （新生動物は実行中の効果の残り時間を権威照会で引き継ぐ） */
	UFUNCTION(BlueprintCallable, Category = "Animal Skill")
	void InitializeMatchSkillEffects();

	/** @brief 現在の権威的な速度倍率を照会する
	 *         （マップ不一致・本局以外・効果なし・教程/独立テストは常に 1.0） */
	UFUNCTION(BlueprintPure, Category = "Animal Skill")
	float GetSpeedMultiplier() const;

	/** @brief 共有加速を含む実効移動速度（MoveSpeed の基底値そのものは変更しない） */
	UFUNCTION(BlueprintPure, Category = "Animal Skill")
	float GetEffectiveMoveSpeed() const;

	/** @brief 権威倍率が変化したときだけ呼ばれる通知（アニメ側の接続入口。
	 *         SkeletalMesh の GlobalAnimRateScale への自動適用は行わない） */
	UFUNCTION(BlueprintImplementableEvent, Category = "Animal Skill")
	void OnSpeedMultiplierChanged(float NewMultiplier);
	// 2026.10.06 Lee end

	// 2026.10.08 Lee start（教程用：読み取り済みセル中心の参照取得）
	/**
	 * @brief 実際に読み取った（中心に到達して処理した）直近セルのグリッド座標を返す。
	 *         チュートリアルが演示動物の実際の経過判定に使う読み取り専用アクセサ。
	 * @param OutGridCoords 読み取り済みセル座標（未読の場合は無効値）。
	 * @return 一度でもセル中心を読み取ったことがある場合 true。
	 */
	UFUNCTION(BlueprintPure, Category = "Animal Movement")
	bool GetLastReadGridCoords(FIntPoint& OutGridCoords) const;
	// 2026.10.08 Lee end

protected:
	// 2026.10.06 Lee start（旧実装。現在はマス中心分割処理の新経路が使用するため未呼び出し。
	// 既存の継承・デバッグ呼び出しを壊さないよう本体ごと保持する）
	//足元のマスを読み取り、移動方向を変更するための入口
	virtual void UpdateMoveDirectionFromCurrentTile(float DeltaTime);

	//現在の方向へ移動する
	void MoveAnimal(float DeltaTime);
	// 2026.10.06 Lee end

private:
	FIntPoint LastGridCoords = FIntPoint::ZeroValue;
	bool bHasLastGridCoords = false;

	// 2026.10.06 Lee start（マス中心分割移動と権威倍率照会のための内部状態・ヘルパー）
	/** @brief LastGridCoords が「処理済みのセル中心」を指す場合 true（読み取り重複防止用） */
	bool bProcessedLastGridCoords = false;

	// 2026.10.06 Lee start（Goal 処理の再入防止フラグを追加）
	/** @brief Goal 到達処理（計分〜破壊）を一度だけ行うためのフラグ。
	 *         AddScore の委譲（OnScoreChanged）がゲームコードを再入させる前に立てる */
	bool bGoalHandled = false;
	// 2026.10.06 Lee end（Goal 処理の再入防止フラグを追加）

	/** @brief 最後に OnSpeedMultiplierChanged へ通知した倍率（変化検出用の初期値 1.0） */
	float NotifiedSpeedMultiplier = 1.0f;

	/** @brief 普通対戦では Playing かつ試合截止前のみ移動を許可する（教程・独立テストは常に許可） */
	bool IsMatchMovementAllowed() const;

	/** @brief ゴール到達時の計分（得点・演出・破壊）が許可されるか */
	bool IsGoalScoringAllowed() const;

	/** @brief 権威倍率を再照会し、変化があった場合のみ OnSpeedMultiplierChanged を通知する */
	void RefreshSpeedMultiplier();

	/** @brief グリッド計算の原点（MapManager 権威。MapActor のワールド位置）を返す */
	FVector GetMapOrigin() const;

	/** @brief グリッド座標をセル中心のワールド座標へ変換する（Z は現在値を維持） */
	FVector GridCenterToWorld(const FIntPoint& GridCoords) const;

	/** @brief ワールド座標を最寄りセルのグリッド座標へ変換する */
	FIntPoint WorldToNearestGrid(const FVector& WorldLocation) const;

	// 2026.10.06 Lee start（前方最寄り中心の選択規約を明記）
	// /** @brief 現在位置から進行方向側で次に通過するセル中心とその距離を求める（軸方向移動専用） */
	// bool ComputeNextCenterAhead(FIntPoint& OutGridCoords, float& OutDistance) const;
	/** @brief 現在位置から進行方向側で「最も近い前方のセル中心」とその距離を求める（軸方向移動専用）。
	 *         到達許容内の未読中心は決して飛ばさず、実際に処理済みの中心
	 *         （LastGridCoords）だけを一度だけスキップする */
	bool ComputeNextCenterAhead(FIntPoint& OutGridCoords, float& OutDistance) const;
	// 2026.10.06 Lee end（前方最寄り中心の選択規約を明記）

	/** @brief sweep 付きで Distance だけ移動する。受阻（途中停止・実移動不足）時は false */
	bool SweepMoveBy(const FVector& Direction, float Distance, FHitResult& OutHit);

	// 2026.10.06 Lee start（2D 到達確認の規約を明記）
	// /** @brief 到達済みセル中心を処理する（方向読み取り / Goal）。戻り値 true は今フレームの停止 */
	// bool ProcessGridCenter(const FIntPoint& GridCoords);
	/** @brief 実際に2Dで到達したセル中心だけを処理する（方向読み取り / Goal）。
	 *         未到達・横ずれ状態では読み取らず、processed の記録も行わない。
	 *         戻り値 true は今フレームの停止 */
	bool ProcessGridCenter(const FIntPoint& GridCoords);
	// 2026.10.06 Lee end（2D 到達確認の規約を明記）

	// 2026.10.06 Lee start（Goal 処理の再入防止を明記）
	// /** @brief ゴール処理（計分 → 効果音 → 破壊）。戻り値 true は Actor を破壊した場合 */
	// bool HandleGoalTile(int32 ScoringPlayerId);
	/** @brief ゴール処理（計分 → 効果音 → 破壊）。bGoalHandled を委譲発火前に立てて
	 *         再入による二重計分を防ぐ。戻り値 true は Goal 処理を確定した場合 */
	bool HandleGoalTile(int32 ScoringPlayerId);
	// 2026.10.06 Lee end（Goal 処理の再入防止を明記）

	// 2026.10.06 Lee start（整列の許容・予計の規約を明記）
	// /** @brief 初回のみ最寄りセル中心へ有限許容内で sweep 整列してセルを読む。
	//  *         戻り値 true は今フレームの移動処理を停止することを意味する */
	// bool TryAlignToNearestCenter(float& InOutRemaining);
	/** @brief 初回のみ最寄りセル中心へ DirectionReadTolerance 以内・今フレーム予算以内で
	 *         sweep 整列してセルを読む。partial / blocked 時は本フレームを停止し、
	 *         次フレームに継続する。実際に中心へ到達した場合のみ processed を記録し、
	 *         unswept の強制移動は行わない。戻り値 true は今フレームの移動処理を停止 */
	bool TryAlignToNearestCenter(float& InOutRemaining);
	// 2026.10.06 Lee end（整列の許容・予計の規約を明記）

	/** @brief グリッド非依存の直線移動（マップ無し / interface 無し / 非軸方向の保護経路） */
	void MoveStraight(float Distance);

	// 2026.10.06 Lee start（区間数固定上限を廃止し進行保証へ移行）
	// /** @brief 1 フレームの移動を、進行上のセル中心ごとの区間へ分割して処理する本体 */
	// void MoveAlongGrid(float DeltaTime);
	/** @brief 1 フレームの移動を、進行上のセル中心ごとの区間へ分割して処理する本体。
	 *         固定の区間数上限は設けず、Goal / 受阻 / 試合終了 / 予算消費 / 無進行検出で抜ける */
	void MoveAlongGrid(float DeltaTime);
	// 2026.10.06 Lee end（区間数固定上限を廃止し進行保証へ移行）
	// 2026.10.06 Lee end
};
