// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GameTypes.h"
#include "CursorPawn.generated.h"

class AMapManager;
class UStaticMeshComponent;

/**
 * @brief グリッドベースのカーソル Pawn。
 *        物理・衝突を持たない純粋なビジュアルカーソル。
 *        マップ上のグリッド座標で位置を管理し、1マス単位の移動を行う。
 */
UCLASS()
class ANIMALGATHERER_API ACursorPawn : public APawn
{
	GENERATED_BODY()

public:
	ACursorPawn();

	//==============================================================================
	// コンポーネント
	//==============================================================================

	/** @brief カーソルの見た目（Plane または枠メッシュを想定）。RootComponent。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cursor|Components")
	UStaticMeshComponent* CursorMesh;

	//==============================================================================
	// グリッド位置
	//==============================================================================

	/** @brief 現在のグリッドX座標（列インデックス）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Cursor|Position")
	int32 GridX = 0;

	/** @brief 現在のグリッドY座標（行インデックス）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Cursor|Position")
	int32 GridY = 0;

	//==============================================================================
	// プレイヤー識別
	//==============================================================================

	/** @brief プレイヤーID（0 = 1P, 1 = 2P）。
	 *        @see AMainGameMode::AddScore の PlayerID と統一。
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Cursor|Identity")
	int32 PlayerID = 0;

	//==============================================================================
	// 公開メソッド
	//==============================================================================

	/**
	 * @brief カーソルを初期化し、マップ参照と開始位置を設定する。
	 * @param InMapManager マップ管理アクター。
	 * @param InPlayerID プレイヤーID。
	 * @param StartGridX 開始グリッドX座標。
	 * @param StartGridY 開始グリッドY座標。
	 */
	UFUNCTION(BlueprintCallable, Category = "Cursor")
	void InitCursor(AMapManager* InMapManager, int32 InPlayerID, int32 StartGridX, int32 StartGridY);

	/**
	 * @brief カーソルを指定方向に1マス移動する。
	 *        境界外の場合は移動しない。
	 * @param DeltaX X方向の変化量（-1, 0, 1）。
	 * @param DeltaY Y方向の変化量（-1, 0, 1）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Cursor")
	void MoveCursor(int32 DeltaX, int32 DeltaY);

	/** @brief マップマネージャの参照を取得する。 */
	UFUNCTION(BlueprintCallable, Category = "Cursor")
	AMapManager* GetMapManager() const { return MapManagerRef; }

protected:
	virtual void BeginPlay() override;

private:
	//==============================================================================
	// 内部メソッド
	//==============================================================================

	/** @brief 指定グリッド座標の中心にワールド座標をスナップする。 */
	void SnapToGrid(int32 X, int32 Y);

	/** @brief 指定座標がマップ範囲内かを判定する。 */
	bool IsWithinBounds(int32 X, int32 Y) const;

	//==============================================================================
	// 内部データ
	//==============================================================================

	/** @brief マップマネージャへのキャッシュ参照。 */
	UPROPERTY()
	AMapManager* MapManagerRef = nullptr;
};
