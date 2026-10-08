// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameTypes.generated.h"

/**
 * @brief タイルの状態を定義する列挙型。
 *        方向指示・動物出現・ゴールの8状態を持つ。
 */
UENUM(BlueprintType)
enum class ETileType : uint8
{
	Empty = 0      UMETA(DisplayName = "方向なし (デフォルト)"),
	DirUp = 1      UMETA(DisplayName = "上方向"),
	DirDown = 2    UMETA(DisplayName = "下方向"),
	DirLeft = 3    UMETA(DisplayName = "左方向"),
	DirRight = 4   UMETA(DisplayName = "右方向"),
	Spawn = 5      UMETA(DisplayName = "動物出現ポイント"),
	GoalP1 = 6     UMETA(DisplayName = "1P ゴール"),
	GoalP2 = 7     UMETA(DisplayName = "2P ゴール")
};

/**
 * @brief グリッド上の1マスを表すデータ構造体。
 */
USTRUCT(BlueprintType)
struct FMapTileData
{
	GENERATED_BODY()

	/** @brief タイルのワールド座標。 */
	UPROPERTY(BlueprintReadOnly)
	FVector WorldLocation = FVector::ZeroVector;

	/** @brief タイルの種類。 */
	UPROPERTY(BlueprintReadWrite)
	ETileType TileType = ETileType::Empty;

	// 2026.09.25 Lee start
	/**
	 * @brief このタイル（方向矢印）を配置したプレイヤーID（0 = 1P, 1 = 2P）。
	 *        @see ACursorPawn::PlayerID と同じ規約。方向タイル以外では使用しない（常に 0）。
	 */
	UPROPERTY(BlueprintReadOnly)
	uint8 OwnerPlayerId = 0;
	// 2026.09.25 Lee end
};

// 2026.10.06 Lee start

// 前方宣言：実装ヘッダーには依存しない（UObject 派生クラスはポインタ経由でのみ参照する）。
class USkillDefinition;
class APlayerController;
class AMapManager;
class UMatchSkillEffectComponent;

/**
 * @brief 対戦の進行フェーズを定義する列挙型。
 *        スキル使用可否や配置・移動・採点の許可判定に用いる。
 */
UENUM(BlueprintType)
enum class EMatchPhase : uint8
{
	Ready = 0     UMETA(DisplayName = "準備中"),
	Playing = 1   UMETA(DisplayName = "対戦中"),
	Ended = 2     UMETA(DisplayName = "対戦終了")
};

/**
 * @brief スキル使用要求の結果を定義する列挙型。
 *        失敗時は使用回数・クールダウンに影響を与えない。
 */
UENUM(BlueprintType)
enum class ESkillUseResult : uint8
{
	Success = 0             UMETA(DisplayName = "成功"),
	NotPlaying = 1          UMETA(DisplayName = "対戦中ではない"),
	InvalidSlot = 2         UMETA(DisplayName = "スロット無効"),
	NoUses = 3              UMETA(DisplayName = "使用回数不足"),
	CoolingDown = 4         UMETA(DisplayName = "クールダウン中"),
	NoTarget = 5            UMETA(DisplayName = "対象なし"),
	MissingDependency = 6   UMETA(DisplayName = "依存コンポーネント不在"),
	Busy = 7                UMETA(DisplayName = "処理中のため拒否")
};

/**
 * @brief プレイヤーごとのスキル装着スロット1つ分の状態。
 *        定義・残り使用回数・クールダウン終了時刻のみを保持し、
 *        振る舞い（使用判定など）は SkillSystemComponent 側に委ねる。
 */
USTRUCT(BlueprintType)
struct FSkillSlot
{
	GENERATED_BODY()

	/** @brief このスロットに割り当てられたスキル定義。未割り当ての場合は nullptr。 */
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<class USkillDefinition> Definition = nullptr;

	/** @brief 残り使用回数。 */
	UPROPERTY(BlueprintReadOnly)
	int32 RemainingUses = 0;

	/** @brief クールダウンが完了するゲーム内時刻（秒）。0 以下はクールダウンなしとして扱う。 */
	UPROPERTY(BlueprintReadOnly)
	float CooldownEndTime = 0.0f;
};

/**
 * @brief スキル発動1回分のコンテキスト情報。
 *        発行者・発動者・バインド済みマップ・共有効果コンポーネントへの参照を持つ。
 */
USTRUCT(BlueprintType)
struct FSkillContext
{
	GENERATED_BODY()

	/** @brief スキルを発動したプレイヤーコントローラー。 */
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<class APlayerController> CasterController = nullptr;

	/** @brief 発動者のプレイヤーID（0 = 1P, 1 = 2P）。未設定・不明は 255（無効値）。
	 *        OwnerPlayerId と同じ規約。無効な身分を P1 に暗黙格上げしないための既定値。 */
	UPROPERTY(BlueprintReadOnly)
	// 2026.10.06 Lee start（無効身分の既定値 0 → 255 に修正）
	uint8 PlayerId = 255;
	// uint8 PlayerId = 0; ←元のコードは消さない
	// 2026.10.06 Lee end（無効身分の既定値 0 → 255 に修正）

	/** @brief バインド済みのマップマネージャー。未バインドの場合は nullptr。 */
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<class AMapManager> MapManager = nullptr;

	/** @brief GameMode が保持する共有の対戦スキル効果コンポーネント。 */
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<class UMatchSkillEffectComponent> MatchSkillEffect = nullptr;
};

/**
 * @brief スキルスロットの状態を外部（HUD など）へ渡すための読み取り専用スナップショット。
 */
USTRUCT(BlueprintType)
struct FSkillSlotSnapshot
{
	GENERATED_BODY()

	/** @brief 対象スロットのスキル定義。 */
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<class USkillDefinition> Definition = nullptr;

	/** @brief 残り使用回数。 */
	UPROPERTY(BlueprintReadOnly)
	int32 RemainingUses = 0;

	/** @brief クールダウン残り時間（秒）。完了済みの場合は 0。 */
	UPROPERTY(BlueprintReadOnly)
	float CooldownRemaining = 0.0f;

	/** @brief クールダウン全体の長さ（秒）。設定がない場合は 0。 */
	UPROPERTY(BlueprintReadOnly)
	float CooldownDuration = 0.0f;

	/** @brief 現在使用可能かどうか（対戦フェーズ・回数・クールダウンを総合した判定結果）。 */
	UPROPERTY(BlueprintReadOnly)
	bool bCanUse = false;
};

/**
 * @brief 共有の対戦速度効果の状態を外部へ渡すための読み取り専用スナップショット。
 */
USTRUCT(BlueprintType)
struct FMatchSpeedSnapshot
{
	GENERATED_BODY()

	/** @brief 現在適用中の速度倍率。効果が無い場合は 1.0。 */
	UPROPERTY(BlueprintReadOnly)
	float SpeedMultiplier = 1.0f;

	/** @brief 効果の残り時間（秒）。効果が無い場合は 0。 */
	UPROPERTY(BlueprintReadOnly)
	float RemainingDuration = 0.0f;

	/** @brief 効果の終了時刻（ゲーム内時刻・秒）。権威側の判定はこの時刻で行う。 */
	UPROPERTY(BlueprintReadOnly)
	float EndTime = 0.0f;
};

// 2026.10.06 Lee end