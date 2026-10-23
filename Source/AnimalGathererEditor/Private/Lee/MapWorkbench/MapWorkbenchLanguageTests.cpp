// Fill out your copyright notice in the Description page of Project Settings.

// 2026.10.23 Lee start（マップ工作台の言語選択・フォールバック・書式テスト）

#include "Lee/MapWorkbench/MapWorkbenchText.h"

// 2026.10.23 Lee start（レビュー修正：FCulture::GetName / GetTwoLetterISOLanguageName 用に Culture.h を include）
#include "Internationalization/Culture.h"
// 2026.10.23 Lee end
#include "Internationalization/Internationalization.h"
#include "Misc/AutomationTest.h"

// 2026.10.23 Lee start（レビュー修正：テスト名を AnimalGatherer.MapWorkbench 接頭辞へ統一）
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapWorkbenchLanguageTest, "AnimalGatherer.MapWorkbench.Language",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)
// 2026.10.23 Lee end
//IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapWorkbenchLanguageTest, "AnimalGathererEditor.MapWorkbench.Language",
//	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)←元のコードは消さない

// 2026.10.23 Lee start（レビュー修正：検証内容と BackupCultureState による復元方式を説明へ反映）
/**
 * @brief 言語選択（ISO 2文字言語コードが ja の場合のみ日本語、en/zh/その他=中国語フォールバック）と
 *        書式引数（中日でプレースホルダ名共通）、さらに言語とロケールの交叉ケースを検証する。
 *        テスト終了時に BackupCultureState で退避したカルチャ状態を RestoreCultureState で
 *        完全に復元し、ユーザーの言語設定は保存・変更しない
 *        （SetCurrentLanguage / SetCurrentLocale はランタイム状態のみ変更し、設定ファイルには書き込まない）。
 */
// 2026.10.23 Lee end
///**
// * @brief 言語選択（ja系=日本語、en/zh/その他=中国語フォールバック）と
// *        書式引数（中日でプレースホルダ名共通）を検証する。
// *        テスト終了時に必ず元の言語へ復元し、ユーザーの言語設定は保存・変更しない
// *        （FInternationalization::SetCurrentLanguage はランタイム状態のみ変更する）。
// */←元のコードは消さない
bool FMapWorkbenchLanguageTest::RunTest(const FString& Parameters)
{
	FInternationalization& I18N = FInternationalization::Get();
	// 2026.10.23 Lee start（レビュー修正：ロケールも変更するためカルチャ状態全体を退避する）
	// テスト前のカルチャ状態（言語・ロケール等）を丸ごと退避する。設定ファイルは書き換えない。
	// 2026.10.23 Lee start（再修正：実在する FInternationalization::FCultureStateSnapshot を使用）
	FInternationalization::FCultureStateSnapshot Backup;
	// 2026.10.23 Lee end
	//FCultureState Backup;←元のコードは消さない
	I18N.BackupCultureState(Backup);
	// 2026.10.23 Lee end
	//const FString SavedLanguageName = I18N.GetCurrentLanguage()->GetName();←元のコードは消さない

	// カルチャ名・期待（ja系のみ日本語）
	struct FCase
	{
		const TCHAR* Culture;
		bool bExpectJapanese;
	};
	const FCase Cases[] =
	{
		{ TEXT("en"),      false },
		{ TEXT("ja"),      true  },
		{ TEXT("ja-JP"),   true  },
		{ TEXT("zh-Hans"), false },
		{ TEXT("ko"),      false },
	};

	for (const FCase& Case : Cases)
	{
		// 2026.10.23 Lee start（レビュー修正：必須カルチャが利用不可の場合は警告で飛ばさずエラーで失敗させる）
		if (!I18N.SetCurrentLanguage(FString(Case.Culture)))
		{
			// 本テストに必要なカルチャが利用できない環境はテスト失敗扱いにする
			AddError(FString::Printf(TEXT("Culture %s unavailable"), Case.Culture));
			continue;
		}
		// 2026.10.23 Lee end
		//if (!I18N.SetCurrentLanguage(FString(Case.Culture)))
		//{
		//	// カルチャが利用できない環境では失敗ではなく警告でスキップ
		//	AddWarning(FString::Printf(TEXT("Culture %s unavailable, skipped"), Case.Culture));
		//	continue;
		//}←元のコードは消さない

		TestEqual(FString::Printf(TEXT("IsJapaneseUI(%s)"), Case.Culture),
			FMapWorkbenchText::IsJapaneseUI(), Case.bExpectJapanese);

		// 日本語時はカタカナ固有の「マップ工作台」、それ以外は中国語「地图工作台」（フォールバック）
		const FString MenuTitle = FMapWorkbenchText::Get(EWorkbenchText::MenuTitle).ToString();
		if (Case.bExpectJapanese)
		{
			TestTrue(FString::Printf(TEXT("MenuTitle ja(%s)"), Case.Culture), MenuTitle.Contains(TEXT("マップ")));
			TestFalse(FString::Printf(TEXT("MenuTitle ja not zh(%s)"), Case.Culture), MenuTitle.Contains(TEXT("地图")));
		}
		else
		{
			TestTrue(FString::Printf(TEXT("MenuTitle zh(%s)"), Case.Culture), MenuTitle.Contains(TEXT("地图")));
			TestFalse(FString::Printf(TEXT("MenuTitle zh not ja(%s)"), Case.Culture), MenuTitle.Contains(TEXT("マップ")));
		}

		// 書式：プレースホルダ名（{count}）は中日共通。引数値が必ず出力へ反映されること
		FFormatNamedArguments Args;
		Args.Add(TEXT("count"), FText::AsNumber(3));
		const FString Formatted = FMapWorkbenchText::Format(EWorkbenchText::MultiSelectedMapsFmt, Args).ToString();
		TestTrue(FString::Printf(TEXT("Format count reflected(%s)"), Case.Culture), Formatted.Contains(TEXT("3")));
		TestFalse(FString::Printf(TEXT("Format no raw placeholder(%s)"), Case.Culture), Formatted.Contains(TEXT("{count}")));
	}

	// 2026.10.23 Lee start（レビュー修正追加：言語とロケールを交叉させ、UI言語判定がロケールに従わないことを検証）
	// 言語とロケールを交叉させる：IsJapaneseUI は言語のみに従い、ロケール（数値・日付書式）に影響されないこと
	struct FMixedCase
	{
		const TCHAR* Language;
		const TCHAR* Locale;
		bool bExpectJapanese;
	};
	const FMixedCase MixedCases[] =
	{
		{ TEXT("en"), TEXT("ja"), false },
		{ TEXT("ja"), TEXT("en"), true  },
	};

	for (const FMixedCase& Case : MixedCases)
	{
		const bool bLanguageOk = I18N.SetCurrentLanguage(FString(Case.Language));
		const bool bLocaleOk = I18N.SetCurrentLocale(FString(Case.Locale));
		if (!bLanguageOk || !bLocaleOk)
		{
			// 本テストに必要な言語・ロケールが利用できない環境はテスト失敗扱いにする
			AddError(FString::Printf(TEXT("Language %s / Locale %s unavailable"), Case.Language, Case.Locale));
			continue;
		}

		TestEqual(FString::Printf(TEXT("IsJapaneseUI(lang=%s, locale=%s)"), Case.Language, Case.Locale),
			FMapWorkbenchText::IsJapaneseUI(), Case.bExpectJapanese);
	}
	// 2026.10.23 Lee end

	// 2026.10.23 Lee start（レビュー修正：退避したカルチャ状態を RestoreCultureState で完全に復元する）
	// 退避したカルチャ状態（言語・ロケール等）を必ず復元する
	I18N.RestoreCultureState(Backup);
	// 2026.10.23 Lee start（再修正：Culture ポインタ比較をやめ、スナップショットのカルチャ名で復元を検証）
	TestEqual(TEXT("RestoredLanguage"), I18N.GetCurrentLanguage()->GetName(), Backup.Language);
	TestEqual(TEXT("RestoredLocale"), I18N.GetCurrentLocale()->GetName(), Backup.Locale);
	// 2026.10.23 Lee end
	//TestTrue(TEXT("RestoredCultureState"),
	//	I18N.GetCurrentLanguage() == Backup.CurrentLanguage &&
	//	I18N.GetCurrentLocale() == Backup.CurrentLocale);←元のコードは消さない
	// 2026.10.23 Lee end
	//// 元の言語へ必ず復元する
	//I18N.SetCurrentLanguage(SavedLanguageName);←元のコードは消さない
	//TestEqual(TEXT("RestoredLanguage"), I18N.GetCurrentLanguage()->GetName(), SavedLanguageName);←元のコードは消さない
	return true;
}
// 2026.10.23 Lee end
