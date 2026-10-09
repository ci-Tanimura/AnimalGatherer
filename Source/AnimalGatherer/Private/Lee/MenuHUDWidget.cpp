// Fill out your copyright notice in the Description page of Project Settings.

#include "Lee/MenuHUDWidget.h"
#include "Lee/MenuPlayerController.h"
#include "Components/Button.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
	/** @brief 実行中のメニューウィジェットの静的登録（弱参照・切図後の残留参照を防ぐ）。 */
	TArray<TWeakObjectPtr<UMenuHUDWidget>>& MenuRegistry()
	{
		static TArray<TWeakObjectPtr<UMenuHUDWidget>> Registry;
		return Registry;
	}

	/** @brief 選択ハイライトの着色（既定ボタン色と区別できる暖色）。 */
	const FLinearColor MenuSelectedColor(1.0f, 0.6f, 0.05f, 1.0f);
}

UMenuHUDWidget* UMenuHUDWidget::GetMenuWidgetFor(const UWorld* InWorld)
{
	if (InWorld == nullptr)
	{
		return nullptr;
	}

	// 期限切れエントリを掃除しつつ、同一 World の先頭登録の有効ウィジェットを返す。
	UMenuHUDWidget* Found = nullptr;
	TArray<TWeakObjectPtr<UMenuHUDWidget>>& Registry = MenuRegistry();
	for (int32 i = Registry.Num() - 1; i >= 0; --i)
	{
		UMenuHUDWidget* Widget = Registry[i].Get();
		if (Widget == nullptr)
		{
			Registry.RemoveAt(i);
			continue;
		}
		if (Widget->GetWorld() == InWorld)
		{
			Found = Widget; // 逆順走査のため、最終的に先頭登録分が残る。
		}
	}
	return Found;
}

void UMenuHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UE_LOG(LogTemp, Verbose, TEXT("[MenuHUD] NativeConstruct entry (%s)"), *GetName());

	// 再入（同じ Widget の再表示）に備えて切替ロック・選択状態・ハイライト参照を初期化する。
	// 背景色は NativeDestruct で復済みのため、ここではフラグと参照のみ初期化する。
	bTransitionRequested = false;
	SelectedIndex = INDEX_NONE;
	HighlightedButton = nullptr;

	ResolveOrderedButtons();
	SetSelectedIndex(0, true); // Title = 開始 / Result = 再戦 を既定選択（どちらも先頭）。

	// 実行ウィジェットを静的登録へ（重複登録なし）。
	bool bAlreadyRegistered = false;
	for (const TWeakObjectPtr<UMenuHUDWidget>& Entry : MenuRegistry())
	{
		if (Entry.Get() == this)
		{
			bAlreadyRegistered = true;
			break;
		}
	}
	if (!bAlreadyRegistered)
	{
		MenuRegistry().Add(this);
	}

	// 構築完了時点でもメニュー入力モードを保障（BP BeginPlay の後続ノードが UI モードへ
	// 上書きする場合は MenuPlayerController::BeginPlay の次 Tick 再適用が後勝ちする）。
	if (AMenuPlayerController* MenuPC = Cast<AMenuPlayerController>(GetOwningPlayer()))
	{
		MenuPC->ApplyMenuInputMode();
	}
}

void UMenuHUDWidget::NativeDestruct()
{
	UE_LOG(LogTemp, Verbose, TEXT("[MenuHUD] NativeDestruct entry (%s)"), *GetName());

	// Button_Quit の動的バインドを解除（再構築時は AddUniqueDynamic が二重接続を防ぐ）。
	if (IsValid(Button_Quit))
	{
		Button_Quit->OnClicked.RemoveDynamic(this, &UMenuHUDWidget::HandleQuitButtonClicked);
	}

	// 全ボタンへ本来の背景色を復元し、ハイライト関連のキャッシュと状態を空にする
	// （再表示時に選択状態と色が乱れず、切替ロックも再度機能する）。
	for (const TPair<TObjectPtr<UButton>, FLinearColor>& OriginalPair : OriginalColors)
	{
		if (IsValid(OriginalPair.Key))
		{
			OriginalPair.Key->SetBackgroundColor(OriginalPair.Value);
		}
	}
	OriginalColors.Empty();
	HighlightedButton = nullptr;
	SelectedIndex = INDEX_NONE;
	bTransitionRequested = false;

	// 静的登録を解除（切図・破棄後の残留参照防止）。
	MenuRegistry().RemoveAll([this](const TWeakObjectPtr<UMenuHUDWidget>& Entry)
	{
		return Entry.Get() == this;
	});

	Super::NativeDestruct();
}

//==============================================================================
// メニュー操作
//==============================================================================

void UMenuHUDWidget::MoveMenuSelection(int32 Step)
{
	if (Step == 0 || OrderedButtons.Num() == 0)
	{
		return;
	}
	SetSelectedIndex(SelectedIndex + Step);
}

void UMenuHUDWidget::ConfirmMenuSelection()
{
	if (OrderedButtons.Num() == 0)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[MenuHUD] Confirm: 候補なしのため無操作"));
		return;
	}

	// 無効な選択はロックを消費せず無操作（メニューが永久にロックされるのを防ぐ）。
	UButton* Selected = GetSelectedMenuButton();
	if (Selected == nullptr)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[MenuHUD] Confirm: 選択無効のため無操作"));
		return;
	}

	UE_LOG(LogTemp, Verbose, TEXT("[MenuHUD] Confirm: index=%d button=%s lockBusy=%d"),
		SelectedIndex, *GetNameSafe(Selected), bTransitionRequested ? 1 : 0);

	// 切替ロック：同一フレームの二重発火と、切替開始後の後続要求の両方を拒否する。
	if (!TryBeginMenuTransition())
	{
		UE_LOG(LogTemp, Verbose, TEXT("[MenuHUD] Confirm: 切替ロック済みのため拒否"));
		return;
	}

	// BP 側の演出フック（決定音等）。
	HandleMenuConfirmed(SelectedIndex, Selected);

	// 終了ボタン選択中は BP の OnClicked を経由せず単一の退出入口へ直行する
	// （ロック消費済みの状態で RequestMenuQuit が同じロックに拒否されるのを避けるため）。
	if (Selected == Button_Quit)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[MenuHUD] Confirm: 終了入口へ直行"));
		ExecuteMenuQuit(GetOwningPlayer());
		return;
	}

	// 既存 BP の OnClicked を発火（遷移挙動の入口を従来どおりボタン操作に統一）。
	UE_LOG(LogTemp, Verbose, TEXT("[MenuHUD] Confirm: OnClicked を発火 (%s)"), *GetNameSafe(Selected));
	Selected->OnClicked.Broadcast();
}

void UMenuHUDWidget::RequestMenuBack()
{
	// 戻る先ボタンが無いメニュー（タイトル）では B は無操作（終了もしない・ロックもしない）。
	if (!IsValid(Button_ReTitle))
	{
		UE_LOG(LogTemp, Verbose, TEXT("[MenuHUD] Back: ReTitle なしのため無操作（ロックしない）"));
		return;
	}

	if (!TryBeginMenuTransition())
	{
		UE_LOG(LogTemp, Verbose, TEXT("[MenuHUD] Back: 切替ロック済みのため拒否"));
		return;
	}

	HandleMenuBack();

	// 「タイトルへ戻る」ボタンの OnClicked をそのまま発火（挙動を B と一致させる）。
	UE_LOG(LogTemp, Verbose, TEXT("[MenuHUD] Back: OnClicked を発火 (Button_ReTitle)"));
	Button_ReTitle->OnClicked.Broadcast();
}

void UMenuHUDWidget::RequestMenuQuit(APlayerController* Requester)
{
	UE_LOG(LogTemp, Verbose, TEXT("[MenuHUD] Quit 要求 (lockBusy=%d, Requester=%s)"),
		bTransitionRequested ? 1 : 0, *GetNameSafe(Requester));

	if (!TryBeginMenuTransition())
	{
		UE_LOG(LogTemp, Verbose, TEXT("[MenuHUD] Quit: 切替ロック済みのため拒否"));
		return;
	}
	ExecuteMenuQuit(Requester);
}

//==============================================================================
// 照会
//==============================================================================

UButton* UMenuHUDWidget::GetSelectedMenuButton() const
{
	return OrderedButtons.IsValidIndex(SelectedIndex) ? OrderedButtons[SelectedIndex] : nullptr;
}

UButton* UMenuHUDWidget::GetMenuButton(int32 Index) const
{
	return OrderedButtons.IsValidIndex(Index) ? OrderedButtons[Index] : nullptr;
}

//==============================================================================
// 動的バインド（Button_Quit の OnClicked → 退出入口）
//==============================================================================

void UMenuHUDWidget::HandleQuitButtonClicked()
{
	// BP 接線なしでボタン直接クリックでも確実に終了できるようにする（切替ロックはこちらで消費）。
	UE_LOG(LogTemp, Verbose, TEXT("[MenuHUD] Button_Quit が直接クリックされた"));
	RequestMenuQuit(GetOwningPlayer());
}

//==============================================================================
// 内部処理
//==============================================================================

void UMenuHUDWidget::ResolveOrderedButtons()
{
	OrderedButtons.Reset();

	// 候補順は明示固定（タイトル = [開始, 終了] / リザルト = [再戦, タイトル, 終了]）。
	// 可視かつ有効なボタンのみを選択対象とする。
	UButton* Candidates[] = { Button_Start, Button_ReStart, Button_ReTitle, Button_Quit };
	for (UButton* Candidate : Candidates)
	{
		if (IsValid(Candidate) && Candidate->IsVisible() && Candidate->GetIsEnabled())
		{
			OrderedButtons.Add(Candidate);
			UE_LOG(LogTemp, Verbose, TEXT("[MenuHUD] 候補採用: %s"), *GetNameSafe(Candidate));
		}
	}

	if (OrderedButtons.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MenuHUD] 候補ボタンが 1 つも見つかりません（BP 変数名を確認してください）"));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[MenuHUD] 候補ボタン %d 個を解決"), OrderedButtons.Num());
	}

	// 終了ボタンの OnClicked を C++ 側から退出入口へ接続（BP 接線は不要・再入安全）。
	if (IsValid(Button_Quit))
	{
		Button_Quit->OnClicked.AddUniqueDynamic(this, &UMenuHUDWidget::HandleQuitButtonClicked);
	}
}

void UMenuHUDWidget::SetSelectedIndex(int32 NewIndex, bool bForce)
{
	if (OrderedButtons.Num() == 0)
	{
		const bool bChanged = (SelectedIndex != INDEX_NONE);
		SelectedIndex = INDEX_NONE;
		if (bChanged || bForce)
		{
			ApplySelectionHighlight();
			HandleSelectionChanged(SelectedIndex, nullptr);
		}
		return;
	}

	const int32 Clamped = FMath::Clamp(NewIndex, 0, OrderedButtons.Num() - 1);
	if (Clamped == SelectedIndex && !bForce)
	{
		return;
	}

	const int32 PreviousIndex = SelectedIndex;
	SelectedIndex = Clamped;

	UE_LOG(LogTemp, Verbose, TEXT("[MenuHUD] 選択更新: %d → %d (%s)"),
		PreviousIndex, SelectedIndex, *GetNameSafe(OrderedButtons[SelectedIndex]));

	// 選択ハイライトは C++ 既定実装で必ず可視化する（BP 未接線でも成立）。
	ApplySelectionHighlight();

	// BP 側の追加演出は任意フックとして通知する。
	HandleSelectionChanged(SelectedIndex, OrderedButtons[SelectedIndex]);
}

void UMenuHUDWidget::ApplySelectionHighlight()
{
	UButton* NewSelected = GetSelectedMenuButton();
	if (HighlightedButton == NewSelected)
	{
		return;
	}

	// 前の選択ボタンへ本来の背景色を復元する。
	if (IsValid(HighlightedButton))
	{
		if (const FLinearColor* Original = OriginalColors.Find(HighlightedButton))
		{
			HighlightedButton->SetBackgroundColor(*Original);
		}
	}

	HighlightedButton = NewSelected;

	if (IsValid(NewSelected))
	{
		// 初回のみ本来の色を保存し（再入時に上書きしない）、選択色へ着色する。
		if (!OriginalColors.Contains(NewSelected))
		{
			OriginalColors.Add(NewSelected, NewSelected->GetBackgroundColor());
		}
		NewSelected->SetBackgroundColor(MenuSelectedColor);
		UE_LOG(LogTemp, Verbose, TEXT("[MenuHUD] ハイライト適用: %s"), *GetNameSafe(NewSelected));
	}
}

bool UMenuHUDWidget::TryBeginMenuTransition()
{
	// 共有の切替要求フラグ：先着 1 回のみ通す。切図 / 退出の実開始後は
	// NativeConstruct（再表示）で初期化されるまで後続の確定 / 戻る / 終了を拒否する。
	if (bTransitionRequested)
	{
		return false;
	}
	bTransitionRequested = true;
	UE_LOG(LogTemp, Verbose, TEXT("[MenuHUD] 切替ロック取得"));
	return true;
}

void UMenuHUDWidget::ExecuteMenuQuit(APlayerController* Requester)
{
	UE_LOG(LogTemp, Verbose, TEXT("[MenuHUD] 退出実行 (Requester=%s)"), *GetNameSafe(Requester));

	HandleMenuQuit();

	// SpecificPlayer は所有プレイヤーを優先し、未設定なら要求元へフォールバック。
	APlayerController* SpecificPlayer = GetOwningPlayer();
	if (SpecificPlayer == nullptr)
	{
		SpecificPlayer = Requester;
	}

	UKismetSystemLibrary::QuitGame(this, SpecificPlayer, EQuitPreference::Quit, false);
}
