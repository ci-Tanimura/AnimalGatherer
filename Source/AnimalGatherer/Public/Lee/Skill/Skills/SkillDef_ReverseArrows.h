// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Lee/Skill/SkillDefinition.h"
#include "SkillDef_ReverseArrows.generated.h"

// 2026.10.06 Lee : 反転スキル定義（相手の有効矢印を180度反転）新規作成

/**
 * @brief 相手プレイヤーが現在置いている有効な方向矢印を180度反転させるスキル定義。
 *        Up↔Down / Left↔Right の明示的な入れ替えのみを行い、列挙値の算術変換は行わない。
 *        対象は対戦相手の Controller 履歴に基づく最大 3 マスで、全対象の事前検証が
 *        完了した後にのみマップへ適用する（部分適用なし）。PlaceHistory は変更しない。
 */
UCLASS(BlueprintType)
class ANIMALGATHERER_API USkillDef_ReverseArrows : public USkillDefinition
{
	GENERATED_BODY()

public:
	/** @brief 1回の発動で反転できる最大ターゲット数（相手の FIFO 上限と一致）。 */
	static constexpr int32 MaxReverseTargets = 3;

	/**
	 * @brief スキル効果を同期実行する。
	 *        有効ターゲットが 1 つもない場合は NoTarget、依存（参照・身分）の欠落は
	 *        MissingDependency を返す。失敗時はマップを一切変更しない。
	 * @param Context 発動 Controller・プレイヤーID・バインド済みマップ・共有効果コンポーネントの文脈。
	 * @return 実行結果。
	 */
	virtual ESkillUseResult ExecuteSkill(const FSkillContext& Context) const override;

private:
	/**
	 * @brief 指定方向の180度反対方向を返す（明示的な switch による変換）。
	 * @param Direction 変換元の方向。
	 * @return 反対方向。方向タイル以外は Empty を返す。
	 */
	static ETileType GetOppositeDirection(ETileType Direction);
};