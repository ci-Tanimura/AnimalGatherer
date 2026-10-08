// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Lee/Skill/SkillDefinition.h"
#include "SkillDef_SpeedUpAnimals.generated.h"

// 2026.10.06 Lee : 全場加速スキル定義 新規作成

/**
 * @brief 試合全体の動物を一定時間加速させる共有効果のスキル定義。
 *        倍率と持続時間の設定のみを保持するステートレス定義で、適用は
 *        共有の UMatchSkillEffectComponent::ApplySpeedEffect() に委ねる。
 *        繰り返し使用時の刷新（終了時刻の更新）判断も共有効果コンポーネント側の責務。
 */
UCLASS(BlueprintType)
class ANIMALGATHERER_API USkillDef_SpeedUpAnimals : public USkillDefinition
{
	GENERATED_BODY()

public:
	/** @brief 共有加速の倍率。既定 1.5 倍。NaN / Inf などの非有限値・0 以下は無効設定。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Config", meta = (ClampMin = "0.0"))
	float SpeedMultiplier = 1.5f;

	/** @brief 共有加速の継続時間（秒）。既定 3 秒。NaN / Inf などの非有限値・0 以下は無効設定。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Config", meta = (ClampMin = "0.0", Units = "s"))
	float DurationSeconds = 3.0f;

	/**
	 * @brief 設定値の整合性を検証する。
	 *        基底クラスの検証に加え、倍率・時間の有限性（NaN / Inf 不許可）と正値を確認する。
	 * @return 設定が有効な場合 true。
	 */
	virtual bool IsConfigurationValid() const override;

	/**
	 * @brief スキル効果を同期実行する。
	 *        共有効果コンポーネントへ ApplySpeedEffect(Map, Multiplier, Duration) を依頼するだけの
	 *        ステートレス処理。動物の列挙・倍率の保持はコンポーネント側の責務。
	 * @param Context 発動 Controller・プレイヤーID・バインド済みマップ・共有効果コンポーネントの文脈。
	 * @return 実行結果（適用成功は Success、依存欠落・適用拒否は MissingDependency）。
	 */
	virtual ESkillUseResult ExecuteSkill(const FSkillContext& Context) const override;
};