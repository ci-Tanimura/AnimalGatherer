// Fill out your copyright notice in the Description page of Project Settings.

#include "Lee/Skill/SkillDefinition.h"

// 2026.10.06 Lee : スキル定義（DataAsset 基底クラス）新規作成

/**
 * @brief 設定値の整合性を検証する。
 *        名前・テクスチャは未設定でもスキル動作に影響しないため検証対象外とし、
 *        数値設定（使用回数上限・クールダウン秒数）の範囲のみを判定する。
 * @return 設定が有効な場合 true。
 */
bool USkillDefinition::IsConfigurationValid() const
{
	if (MaxUses < 1)
	{
		return false;
	}
	// 2026.10.06 Lee start（CooldownSeconds に非有限値チェックを追加）
	// if (CooldownSeconds < 0.0f)
	// {
	// 	return false;
	// }
	if (!FMath::IsFinite(CooldownSeconds) || CooldownSeconds < 0.0f)
	{
		return false;
	}
	// 2026.10.06 Lee end（CooldownSeconds に非有限値チェックを追加）
	return true;
}

/**
 * @brief スキル効果を同期実行する。
 *        基底クラスは具体的効果を持たないため、Context の内容にかかわらず
 *        MissingDependency（依存不足）を返して失敗扱いとする。
 *        実際の効果はサブクラス側での上書き実装に委ねる。
 * @param Context 発動 Controller・プレイヤーID・バインド済みマップ・共有効果コンポーネントの文脈。
 * @return 常に ESkillUseResult::MissingDependency。
 */
ESkillUseResult USkillDefinition::ExecuteSkill(const FSkillContext& Context) const
{
	// Context は基底クラスでは使用しない（サブクラスでの利用を想定）。
	return ESkillUseResult::MissingDependency;
}