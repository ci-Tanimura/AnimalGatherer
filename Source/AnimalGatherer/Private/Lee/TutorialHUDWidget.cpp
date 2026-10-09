// Fill out your copyright notice in the Description page of Project Settings.

#include "Lee/TutorialHUDWidget.h"
#include "Lee/TutorialGameMode.h"
// 2026.10.08 Lee 第三批 start（原生 UMG 呈現と権威スナップショットの読み取り）
#include "Lee/TutorialGamepadWidget.h"
#include "Lee/AnimalGatherPlayerController.h"
#include "Lee/Skill/SkillSystemComponent.h"
#include "Lee/Skill/MatchSkillEffectComponent.h"
#include "Lee/Skill/SkillDefinition.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"
// 2026.10.08 Lee 第三批 end

namespace
{
	// 2026.10.08 Lee 返工 start（旧 Root の Slot を全面拉伸に変更したため設計解像度定数は不要に）
	/** @brief 側欄の幅と外側余白。 */
	constexpr float SidePanelWidth = 360.0f;
	constexpr float SidePanelMargin = 16.0f;

	/** @brief 技能カードの表示サイズ。 */
	constexpr float SkillCardW = 160.0f;
	constexpr float SkillCardH = 213.0f;

	/** @brief スナップショットの定期読み取り間隔（秒）。 */
	constexpr float RefreshInterval = 0.1f;
	// 補足: 強調ボタン名（None/DPad/LB/RB/All）は都度 FName リテラルで組み立てる
	//（匿名名前空間のグローバル FName 定数は静的初期化順序の危険があるため使わない）
}

//==============================================================================
// 2026.10.08 Lee 第三批：ライフサイクル
//==============================================================================

// 2026.10.08 Lee 返工 start（RebuildWidget の実際の署名は TSharedRef<SWidget> を返す）
TSharedRef<SWidget> UTutorialHUDWidget::RebuildWidget()
{
	// Slate が旧 Root を取得する前に原生レイアウトを構築する
	// （NativeConstruct での Root 差し替えは新 UI が表示されないため行わない）
	BuildNativeLayout();
	TSharedRef<SWidget> RebuiltSlate = Super::RebuildWidget();

	// 2026.10.09 Lee start（参照の前後検証：RebuildWidget の前後で RemoveFromParent
	// などの経路により ReleaseSlateResources が呼ばれていた場合は、
	// 実行時参照と bNativeLayoutBuilt が解放済みになる。実行時ルートは Tree に残り
	// 再利用可能なため、フラグ検証を 1 回行い、解放済みのときだけ参照を張り直す）
	if (!bNativeLayoutBuilt)
	{
		BuildNativeLayout();
	}
	// 2026.10.09 Lee end

	return RebuiltSlate;
}
// 2026.10.08 Lee 返工 end

void UTutorialHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// TutorialGameMode を取得して従来のデリゲートを購読する
	if (ATutorialGameMode* GameMode = Cast<ATutorialGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
	{
		GameMode->OnTutorialStepChanged.AddDynamic(this, &UTutorialHUDWidget::HandleStepChanged);
		GameMode->OnTutorialPlayerDone.AddDynamic(this, &UTutorialHUDWidget::HandlePlayerDone);
		GameMode->OnTutorialComplete.AddDynamic(this, &UTutorialHUDWidget::HandleComplete);

		// 購読開始時点のステップを反映（遅延生成対応）
		ShowStep(GameMode->GetCurrentStep());
	}

	// 2026.10.08 Lee 第三批 start（権威スナップショットの定期読み取り。初期化の遅れも毎回の読み直しで吸収する）
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RefreshTimerHandle, this, &UTutorialHUDWidget::RefreshFromGameMode, RefreshInterval, true);
	}
	RefreshFromGameMode();
	// 2026.10.08 Lee 第三批 end
}

void UTutorialHUDWidget::NativeDestruct()
{
	// 購読したデリゲートは必ず解除する（成対 Hygiene）。Root の解体は行わない
	if (ATutorialGameMode* GameMode = Cast<ATutorialGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
	{
		GameMode->OnTutorialStepChanged.RemoveAll(this);
		GameMode->OnTutorialPlayerDone.RemoveAll(this);
		GameMode->OnTutorialComplete.RemoveAll(this);
	}
	// 2026.10.08 Lee 第三批 start（定期読み取り Timer の停止）
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}
	// 2026.10.08 Lee 第三批 end

	Super::NativeDestruct();
}

void UTutorialHUDWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	// 先に Slate を解放してから旧 Root へ復元し、次回 RebuildWidget で再構築できるようにする
	Super::ReleaseSlateResources(bReleaseChildren);

	// 2026.10.09 Lee start（HUD 消失の根本修正：本関数の責務は Slate 資源の解放のみで、
	// 実行時ツリールートを折り畳み済みの旧 BP ルートへ戻すことではない。
	// 旧実装はここでループ復元・親解除を行ったため、RemoveFromParent 後の再追加で
	// RebuildWidget が折り畳み済み旧 BP ルートから組み直し、実行時 Canvas が
	// 木の外へ外れて HUD 全体が見えなくなる（UObject は生存し Tick も続く＝実機症状と一致）。
	// そのためルートの復元・RemoveFromParent は完全に廃止する。実行時ルートは
	// Tree に残して再利用可能とし、再取得時は BuildNativeLayout が名前検出と
	// bNativeLayoutBuilt 検証で参照のみ張り直す）
	// 【旧実装（保持）】
	// if (CachedOldRoot != nullptr)
	// {
	// 	CachedOldRoot->RemoveFromParent();
	// }
	//
	// if (UWidgetTree* Tree = WidgetTree.Get())
	// {
	// 	if (CachedOldRoot != nullptr)
	// 	{
	// 		Tree->RootWidget = CachedOldRoot;
	// 	}
	// }
	// 2026.10.09 Lee end
	// 2026.10.08 Lee 第三批 start（実行時参照の解放。再構築に備えて全て初期化する）
	CachedOldRoot = nullptr;
	bNativeLayoutBuilt = false;
	StepTitleText = nullptr;
	StepObjectiveText = nullptr;
	SharedEffectText = nullptr;
	P1TitleText = nullptr;
	P2TitleText = nullptr;
	P1Gamepad = nullptr;
	P2Gamepad = nullptr;
	P1TaskText = nullptr;
	P2TaskText = nullptr;
	P1SkillCardBox = nullptr;
	P2SkillCardBox = nullptr;
	P1SkillCardImage = nullptr;
	P2SkillCardImage = nullptr;
	P1SkillUsesText = nullptr;
	P2SkillUsesText = nullptr;
	P1SkillCooldownText = nullptr;
	P2SkillCooldownText = nullptr;
	// 2026.10.08 Lee 第三批 end
}

//==============================================================================
// 2026.10.08 Lee 第三批：原生レイアウト構築
//==============================================================================

void UTutorialHUDWidget::BuildNativeLayout()
{
	if (bNativeLayoutBuilt)
	{
		return;
	}
	// 2026.10.08 Lee 返工 start（GetWidgetTree は存在しないため保護メンバーの WidgetTree を直接読む）
	UWidgetTree* Tree = WidgetTree.Get();
	// 2026.10.08 Lee 返工 end
	if (Tree == nullptr)
	{
		return;
	}

	// 2026.10.09 Lee start（再取得経路：実行時ルートが既に Tree にある場合は
	// 新しい Canvas を重ねて作らず、名前検索で参照だけ張り直して再利用する）
	if (const UWidget* CurrentRoot = Tree->RootWidget.Get())
	{
		if (CurrentRoot->GetFName() == TEXT("TutorialNativeRootCanvas"))
		{
			StepTitleText = Tree->FindWidget<UTextBlock>(TEXT("StepTitleText"));
			StepObjectiveText = Tree->FindWidget<UTextBlock>(TEXT("StepObjectiveText"));
			SharedEffectText = Tree->FindWidget<UTextBlock>(TEXT("SharedEffectText"));
			P1TitleText = Tree->FindWidget<UTextBlock>(TEXT("P1TitleText"));
			P2TitleText = Tree->FindWidget<UTextBlock>(TEXT("P2TitleText"));
			P1Gamepad = Tree->FindWidget<UTutorialGamepadWidget>(TEXT("P1Gamepad"));
			P2Gamepad = Tree->FindWidget<UTutorialGamepadWidget>(TEXT("P2Gamepad"));
			P1TaskText = Tree->FindWidget<UTextBlock>(TEXT("P1TaskText"));
			P2TaskText = Tree->FindWidget<UTextBlock>(TEXT("P2TaskText"));
			P1SkillCardBox = Tree->FindWidget<UPanelWidget>(TEXT("P1SkillCardBox"));
			P2SkillCardBox = Tree->FindWidget<UPanelWidget>(TEXT("P2SkillCardBox"));
			P1SkillCardImage = Tree->FindWidget<UImage>(TEXT("P1SkillCardImage"));
			P2SkillCardImage = Tree->FindWidget<UImage>(TEXT("P2SkillCardImage"));
			P1SkillUsesText = Tree->FindWidget<UTextBlock>(TEXT("P1SkillUsesText"));
			P2SkillUsesText = Tree->FindWidget<UTextBlock>(TEXT("P2SkillUsesText"));
			P1SkillCooldownText = Tree->FindWidget<UTextBlock>(TEXT("P1SkillCooldownText"));
			P2SkillCooldownText = Tree->FindWidget<UTextBlock>(TEXT("P2SkillCooldownText"));
			bNativeLayoutBuilt = true;
			return;
		}
	}
	// 2026.10.09 Lee end

	// 旧 Root（ブループリント既存の名前付き Widget 群）は折り畳んだ子樹として保持する
	// 2026.10.08 Lee 返工 start（RootWidget の実型は UWidget）
	UWidget* OldRoot = Tree->RootWidget.Get();
	// 2026.10.08 Lee 返工 end
	UCanvasPanel* RootCanvas = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TutorialNativeRootCanvas"));
	Tree->RootWidget = RootCanvas;
	CachedOldRoot = OldRoot;
	if (OldRoot != nullptr)
	{
		if (UCanvasPanelSlot* OldSlot = RootCanvas->AddChildToCanvas(OldRoot))
		{
			// 2026.10.08 Lee 返工 start（全面拉伸アンカーの場合は Offsets を全 0 にする。
			//  右/下を残すと余白扱いになり旧 Root が 0 サイズへ縮む）
			OldSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			OldSlot->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 0.0f));
			// 2026.10.08 Lee 返工 end
		}
		OldRoot->SetVisibility(ESlateVisibility::Collapsed);
	}

	// ── 中央上段：ステップ見出し + 目標一文（y30..170） ──
	// 2026.10.09 Lee start（文字の中央寄せと拡大：見出し 40 号・枠 800x64、目標 30 号・枠 800x44。
	// 旧レイアウト行はコメントとして保持）
	StepTitleText = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StepTitleText"));
	if (UCanvasPanelSlot* TitleSlot = RootCanvas->AddChildToCanvas(StepTitleText))
	{
		TitleSlot->SetAnchors(FAnchors(0.5f, 0.0f));
		// TitleSlot->SetPosition(FVector2D(-400.0f, 30.0f));
		// TitleSlot->SetSize(FVector2D(800.0f, 46.0f));
		TitleSlot->SetPosition(FVector2D(-400.0f, 30.0f));
		TitleSlot->SetSize(FVector2D(800.0f, 64.0f));
	}
	StepTitleText->SetJustification(ETextJustify::Center);
	StepObjectiveText = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StepObjectiveText"));
	if (UCanvasPanelSlot* ObjSlot = RootCanvas->AddChildToCanvas(StepObjectiveText))
	{
		ObjSlot->SetAnchors(FAnchors(0.5f, 0.0f));
		// ObjSlot->SetPosition(FVector2D(-400.0f, 82.0f));
		// ObjSlot->SetSize(FVector2D(800.0f, 34.0f));
		ObjSlot->SetPosition(FVector2D(-400.0f, 100.0f));
		ObjSlot->SetSize(FVector2D(800.0f, 44.0f));
	}
	StepObjectiveText->SetJustification(ETextJustify::Center);
	// 2026.10.09 Lee end

	// 2026.10.09 Lee start（PIE 実機で文字が小さすぎたため段階見出し・目標・共有効果の字号を指定）
	// 旧実装は既定フォントサイズに依存していた（旧コードは消さない）
	// StepTitleText->SetFontSize(...); ←旧実装は無し（既定値）
	// 2026.10.09 Lee start（文字と達成フィードバック：見出し 40 / 目標 30 へ拡大。旧値はコメント保持）
	// StepTitleText->SetFontSize(32.0f);
	// StepObjectiveText->SetFontSize(24.0f);
	StepTitleText->SetFontSize(40.0f);
	StepObjectiveText->SetFontSize(30.0f);
	// 2026.10.09 Lee end
	// 2026.10.09 Lee end

	// ── 共有加速の残り表示（加速系ステップのみ。中央上段の直下） ──
	SharedEffectText = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SharedEffectText"));
	// 2026.10.09 Lee start（C4458: UWidget::Slot メンバーとの遮蔽を避けるため局所名を EffectSlot へ変更）
	if (UCanvasPanelSlot* EffectSlot = RootCanvas->AddChildToCanvas(SharedEffectText))
	{
		EffectSlot->SetAnchors(FAnchors(0.5f, 0.0f));
		// 2026.10.09 Lee start（共有効果 30 号へ拡大：枠 600x40 に広げ中央寄せ。旧行はコメント保持）
		// EffectSlot->SetPosition(FVector2D(-200.0f, 172.0f));
		// EffectSlot->SetSize(FVector2D(400.0f, 30.0f));
		EffectSlot->SetPosition(FVector2D(-300.0f, 172.0f));
		EffectSlot->SetSize(FVector2D(600.0f, 40.0f));
		// 2026.10.09 Lee end
	}
	// 2026.10.09 Lee end
	SharedEffectText->SetVisibility(ESlateVisibility::Collapsed);
	SharedEffectText->SetJustification(ETextJustify::Center);
	// 2026.10.09 Lee start（共有効果表示の字号指定。24 → 30 へ拡大。旧値はコメント保持）
	// SharedEffectText->SetFontSize(24.0f);
	SharedEffectText->SetFontSize(30.0f);
	// 2026.10.09 Lee end

	// ── 側欄（左右）の構築 ──
	const FVector2D PanelSize(SidePanelWidth, 850.0f);

	// 左欄（1P）
	if (UCanvasPanel* LeftPanel = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("P1SidePanel")))
	{
		if (UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(LeftPanel))
		{
			PanelSlot->SetAnchors(FAnchors(0.0f, 0.0f));
			PanelSlot->SetPosition(FVector2D(SidePanelMargin, 210.0f));
			PanelSlot->SetSize(PanelSize);
		}
		BuildSidePanel(Tree, LeftPanel, 0);
	}

	// 右欄（2P）
	if (UCanvasPanel* RightPanel = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("P2SidePanel")))
	{
		if (UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(RightPanel))
		{
			PanelSlot->SetAnchors(FAnchors(1.0f, 0.0f));
			PanelSlot->SetPosition(FVector2D(-(SidePanelWidth + SidePanelMargin), 210.0f));
			PanelSlot->SetSize(PanelSize);
		}
		BuildSidePanel(Tree, RightPanel, 1);
	}

	bNativeLayoutBuilt = true;
}

void UTutorialHUDWidget::BuildSidePanel(UWidgetTree* Tree, UCanvasPanel* Panel, uint8 PlayerId)
{
	// プレイヤー見出し（1P = 青 / 2P = 赤）
	TObjectPtr<UTextBlock>& TitleText = (PlayerId == 0) ? P1TitleText : P2TitleText;
	TitleText = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
		(PlayerId == 0) ? TEXT("P1TitleText") : TEXT("P2TitleText"));
	// 2026.10.09 Lee start（C4458: UWidget::Slot メンバーとの遮蔽を避けるため局所名を TitleSlot へ変更）
	// 2026.10.09 Lee start（識別 34 号へ拡大：枠高さ 36 → 44、中央寄せ。旧行はコメント保持）
	if (UCanvasPanelSlot* TitleSlot = Panel->AddChildToCanvas(TitleText))
	{
		// TitleSlot->SetPosition(FVector2D(0.0f, 0.0f));
		// TitleSlot->SetSize(FVector2D(SidePanelWidth, 36.0f));
		TitleSlot->SetPosition(FVector2D(0.0f, 0.0f));
		TitleSlot->SetSize(FVector2D(SidePanelWidth, 44.0f));
	}
	TitleText->SetJustification(ETextJustify::Center);
	// 2026.10.09 Lee end
	// 2026.10.09 Lee end
	TitleText->SetText((PlayerId == 0) ? FText::FromString(TEXT("1P")) : FText::FromString(TEXT("2P")));
	TitleText->SetColorAndOpacity(FSlateColor(
		(PlayerId == 0) ? FLinearColor(0.25f, 0.5f, 1.0f, 1.0f) : FLinearColor(1.0f, 0.3f, 0.3f, 1.0f)));
	// 2026.10.09 Lee start（側欄プレイヤー識別の字号指定。28 → 34 へ拡大。旧値はコメント保持）
	// TitleText->SetFontSize(28.0f);
	TitleText->SetFontSize(34.0f);
	// 2026.10.09 Lee end

	// パッド早見（330x220。768x512 設計を ScaleBox で等比縮小）
	TObjectPtr<UTutorialGamepadWidget>& Gamepad = (PlayerId == 0) ? P1Gamepad : P2Gamepad;
	Gamepad = Tree->ConstructWidget<UTutorialGamepadWidget>(
		UTutorialGamepadWidget::StaticClass(), (PlayerId == 0) ? TEXT("P1Gamepad") : TEXT("P2Gamepad"));
	if (UScaleBox* PadScale = Tree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(),
		(PlayerId == 0) ? TEXT("P1PadScale") : TEXT("P2PadScale")))
	{
		// 2026.10.08 Lee 返工 start（SetStretch の実際の列挙型は EStretch）
		PadScale->SetStretch(EStretch::ScaleToFit);
		// 2026.10.08 Lee 返工 end
		PadScale->AddChild(Gamepad);
		// 2026.10.09 Lee start（C4458: UWidget::Slot メンバーとの遮蔽を避けるため局所名を PadSlot へ変更）
		if (UCanvasPanelSlot* PadSlot = Panel->AddChildToCanvas(PadScale))
		{
			PadSlot->SetPosition(FVector2D(15.0f, 44.0f));
			PadSlot->SetSize(FVector2D(330.0f, 220.0f));
		}
		// 2026.10.09 Lee end
	}
	Gamepad->SetBodyTexture(TutorialGamepadBodyTexture);

	// 任務文言（折り返し許可）
	TObjectPtr<UTextBlock>& TaskText = (PlayerId == 0) ? P1TaskText : P2TaskText;
	TaskText = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
		(PlayerId == 0) ? TEXT("P1TaskText") : TEXT("P2TaskText"));
	// 2026.10.09 Lee start（C4458: UWidget::Slot メンバーとの遮蔽を避けるため局所名を TaskSlot へ変更）
	if (UCanvasPanelSlot* TaskSlot = Panel->AddChildToCanvas(TaskText))
	{
		// 2026.10.09 Lee start（任務 28 号へ拡大：枠 120 → 150。旧行はコメント保持）
		// TaskSlot->SetPosition(FVector2D(0.0f, 274.0f));
		// TaskSlot->SetSize(FVector2D(SidePanelWidth, 120.0f));
		// 2026.10.09 Lee start（実機返工：複数行の長文（観察/速查）で卡图と重ならないよう
		// 高さ 150 → 170 とし、卡图 Canvas を 410 → 455 へ下げる。旧値はコメント保持）
		// TaskSlot->SetPosition(FVector2D(0.0f, 274.0f));
		// TaskSlot->SetSize(FVector2D(SidePanelWidth, 150.0f));
		TaskSlot->SetPosition(FVector2D(0.0f, 274.0f));
		TaskSlot->SetSize(FVector2D(SidePanelWidth, 170.0f));
		// 2026.10.09 Lee end
		// 2026.10.09 Lee end
	}
	// 2026.10.09 Lee end
	TaskText->SetAutoWrapText(true);
	// 2026.10.09 Lee start（任務文言を中央寄せ）
	TaskText->SetJustification(ETextJustify::Center);
	// 2026.10.09 Lee end
	// 2026.10.09 Lee start（任務文言の字号指定。22 → 28 へ拡大。旧値はコメント保持）
	// TaskText->SetFontSize(22.0f);
	TaskText->SetFontSize(28.0f);
	// 2026.10.09 Lee end

	// 2026.10.08 Lee 返工 start（技能カードはカード内 Canvas 配置へ変更。
	//  残回数・CD はカード画像の下の独立 2 行とし、焼き込み名称を覆わない）
	TObjectPtr<UPanelWidget>& CardBox = (PlayerId == 0) ? P1SkillCardBox : P2SkillCardBox;
	TObjectPtr<UImage>& CardImage = (PlayerId == 0) ? P1SkillCardImage : P2SkillCardImage;
	TObjectPtr<UTextBlock>& UsesText = (PlayerId == 0) ? P1SkillUsesText : P2SkillUsesText;
	TObjectPtr<UTextBlock>& CooldownText = (PlayerId == 0) ? P1SkillCooldownText : P2SkillCooldownText;
	UCanvasPanel* CardCanvas = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
		(PlayerId == 0) ? TEXT("P1SkillCardBox") : TEXT("P2SkillCardBox"));
	CardBox = CardCanvas;
	// 2026.10.09 Lee start（C4458: UWidget::Slot メンバーとの遮蔽を避けるため局所名を CardSlot へ変更）
	if (UCanvasPanelSlot* CardSlot = Panel->AddChildToCanvas(CardCanvas))
	{
		// 2026.10.09 Lee start（技能文字を卡図 160px から独立させる：枠幅 240 に拡大し
		// 卡図は中央 (40,0) に配置して別々に中央寄せ。旧行はコメント保持）
		// 画像 213 + 文字 2 行（24x2 + 余白）の高さを確保する
		// CardSlot->SetPosition(FVector2D((SidePanelWidth - SkillCardW) * 0.5f, 410.0f));
		// CardSlot->SetSize(FVector2D(SkillCardW, SkillCardH + 52.0f));
		// 2026.10.09 Lee start（実機返工：任務 170px 分の縦余白を確保し 410 → 455 へ。旧値はコメント保持）
		// CardSlot->SetPosition(FVector2D((SidePanelWidth - 240.0f) * 0.5f, 410.0f));
		CardSlot->SetPosition(FVector2D((SidePanelWidth - 240.0f) * 0.5f, 455.0f));
		CardSlot->SetSize(FVector2D(240.0f, SkillCardH + 64.0f));
		// 2026.10.09 Lee end
		// 2026.10.09 Lee end
	}
	// 2026.10.09 Lee end

	CardImage = Tree->ConstructWidget<UImage>(UImage::StaticClass(),
		(PlayerId == 0) ? TEXT("P1SkillCardImage") : TEXT("P2SkillCardImage"));
	// 2026.10.09 Lee start（卡図は拡大枠の中央へ（x = (240-160)/2 = 40）。旧行はコメント保持）
	if (UCanvasPanelSlot* ImageSlot = CardCanvas->AddChildToCanvas(CardImage))
	{
		// ImageSlot->SetPosition(FVector2D(0.0f, 0.0f));
		// ImageSlot->SetSize(FVector2D(SkillCardW, SkillCardH));
		ImageSlot->SetPosition(FVector2D(40.0f, 0.0f));
		ImageSlot->SetSize(FVector2D(SkillCardW, SkillCardH));
	}
	// 2026.10.09 Lee end
	CardImage->SetVisibility(ESlateVisibility::HitTestInvisible);

	UsesText = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
		(PlayerId == 0) ? TEXT("P1SkillUsesText") : TEXT("P2SkillUsesText"));
	// 2026.10.09 Lee start（残回数 28 号へ拡大：幅 160 → 240 で卡図に依存しない。旧行はコメント保持）
	if (UCanvasPanelSlot* UsesSlot = CardCanvas->AddChildToCanvas(UsesText))
	{
		// UsesSlot->SetPosition(FVector2D(0.0f, SkillCardH + 2.0f));
		// UsesSlot->SetSize(FVector2D(SkillCardW, 24.0f));
		UsesSlot->SetPosition(FVector2D(0.0f, SkillCardH + 2.0f));
		UsesSlot->SetSize(FVector2D(240.0f, 30.0f));
	}
	// 2026.10.09 Lee end
	UsesText->SetJustification(ETextJustify::Center);
	// 2026.10.09 Lee start（残回数表示の字号指定。22 → 28 へ拡大。旧値はコメント保持）
	// UsesText->SetFontSize(22.0f);
	UsesText->SetFontSize(28.0f);
	// 2026.10.09 Lee end

	CooldownText = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
		(PlayerId == 0) ? TEXT("P1SkillCooldownText") : TEXT("P2SkillCooldownText"));
	// 2026.10.09 Lee start（クールダウン 28 号へ拡大：幅 160 → 240・高さ 24 → 30 で
	// 長文の截断を防ぐ。旧行はコメント保持）
	if (UCanvasPanelSlot* CdSlot = CardCanvas->AddChildToCanvas(CooldownText))
	{
		// CdSlot->SetPosition(FVector2D(0.0f, SkillCardH + 26.0f));
		// CdSlot->SetSize(FVector2D(SkillCardW, 24.0f));
		CdSlot->SetPosition(FVector2D(0.0f, SkillCardH + 36.0f));
		CdSlot->SetSize(FVector2D(240.0f, 30.0f));
	}
	// 2026.10.09 Lee end
	CooldownText->SetJustification(ETextJustify::Center);
	// 2026.10.09 Lee start（クールダウン表示の字号指定。22 → 28 へ拡大。旧値はコメント保持）
	// CooldownText->SetFontSize(22.0f);
	CooldownText->SetFontSize(28.0f);
	// 2026.10.09 Lee end

	CardBox->SetVisibility(ESlateVisibility::Collapsed);
	// 2026.10.08 Lee 返工 end
}

//==============================================================================
// デリゲート受信（従来ハンドラ。Blueprint 実装への転送のみ）
//==============================================================================

void UTutorialHUDWidget::HandleStepChanged(ETutorialStep NewStep)
{
	ShowStep(NewStep);
	// 原生呈現も即時反映させる（定期 Timer を待たない）
	RefreshFromGameMode();
}

void UTutorialHUDWidget::HandlePlayerDone(uint8 PlayerID, bool bDone)
{
	// 旧Blueprint実装への転送は互換のため保持
	SetPlayerReady(PlayerID, bDone);

	// 2026.10.09 Lee start（達成状態の変化を新HUDへ即時反映する。
	// 旧Rootは折り畳まれており SetPlayerReady だけでは見えないため、権威再読へ委ねる）
	RefreshFromGameMode();
	// 2026.10.09 Lee end
}

void UTutorialHUDWidget::HandleComplete()
{
	ShowComplete();
	RefreshFromGameMode();
}

//==============================================================================
// 2026.10.08 Lee 第三批：権威スナップショットの読み取りと表示
//==============================================================================

void UTutorialHUDWidget::RefreshFromGameMode()
{
	// 毎回現在の GameMode / PC を読み直す（技能初期化の遅れや遅延バインドに追従する）
	RefreshStepTexts();
	RefreshParticipants();
	RefreshSkillCards();
	RefreshSharedEffect();
}

void UTutorialHUDWidget::RefreshStepTexts()
{
	if (StepTitleText == nullptr || StepObjectiveText == nullptr)
	{
		return;
	}
	const ATutorialGameMode* GameMode = Cast<ATutorialGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (GameMode == nullptr)
	{
		return;
	}

	FText Title;
	FText Objective;
	switch (GameMode->GetCurrentStep())
	{
	case ETutorialStep::Intro:
		Title = FText::FromString(TEXT("チュートリアル"));
		Objective = FText::FromString(TEXT("矢印で動物を自分のゴールへ導こう"));
		break;
	case ETutorialStep::MoveCursor:
		Title = FText::FromString(TEXT("カーソル移動"));
		Objective = FText::FromString(TEXT("十字キーで光るセルまで動かそう"));
		break;
	case ETutorialStep::PlaceArrow:
		Title = FText::FromString(TEXT("矢印を置こう"));
		Objective = FText::FromString(TEXT("自分の色のセルへ正しい向きの矢印を置こう"));
		break;
	case ETutorialStep::ScoreGoal:
		Title = FText::FromString(TEXT("ゴールへ導こう"));
		Objective = FText::FromString(TEXT("動物がゴールに入れば得点"));
		break;
	case ETutorialStep::ReverseSkill:
		Title = FText::FromString(TEXT("反転スキル"));
		Objective = FText::FromString(TEXT("LB で相手の矢印の向きを変えよう"));
		break;
	case ETutorialStep::ObserveReverse:
		Title = FText::FromString(TEXT("反転を観察"));
		Objective = FText::FromString(TEXT("動物の流れが変わる様子を見よう"));
		break;
	case ETutorialStep::RepairArrow:
		Title = FText::FromString(TEXT("矢印を直そう"));
		Objective = FText::FromString(TEXT("元の向きの矢印を置き直そう"));
		break;
	case ETutorialStep::SpeedBaseline:
		Title = FText::FromString(TEXT("通常速度の確認"));
		Objective = FText::FromString(TEXT("今の動物の速さをよく見ておこう"));
		break;
	case ETutorialStep::SpeedSkill:
		Title = FText::FromString(TEXT("加速スキル"));
		Objective = FText::FromString(TEXT("RB で場の動物を加速させよう"));
		break;
	case ETutorialStep::ObserveSpeed:
		Title = FText::FromString(TEXT("加速を観察"));
		Objective = FText::FromString(TEXT("動物が速くなる様子を見よう"));
		break;
	case ETutorialStep::SpeedRecovered:
		Title = FText::FromString(TEXT("速度の復帰"));
		Objective = FText::FromString(TEXT("元の速さに戻るのを確認しよう"));
		break;
	case ETutorialStep::Complete:
		Title = FText::FromString(TEXT("チュートリアル完了"));
		Objective = FText::FromString(TEXT("この調子で本番へ！"));
		break;
	default:
		break;
	}
	StepTitleText->SetText(Title);
	StepObjectiveText->SetText(Objective);
}

UTutorialHUDWidget::FParticipantDisplay UTutorialHUDWidget::ComputeParticipantDisplay(uint8 PlayerId) const
{
	FParticipantDisplay Display;
	const ATutorialGameMode* GameMode = Cast<ATutorialGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (GameMode == nullptr)
	{
		return Display;
	}
	const ETutorialStep Step = GameMode->GetCurrentStep();
	const uint8 ActiveId = GameMode->GetCurrentActivePlayerId();
	const uint8 AffectedId = 1u - ActiveId;
	// 2026.10.09 Lee start（権威 IsTutorialPlayerDone の参照。達成側は入力強調を消す）
	const bool bPlayerDone = GameMode->IsTutorialPlayerDone(PlayerId);
	// 2026.10.09 Lee end

	switch (Step)
	{
	case ETutorialStep::Intro:
		// 両側とも基本の早見を見せる
		Display.ControlHighlight = FName(TEXT("All"));
		break;
	case ETutorialStep::MoveCursor:
		// 2026.10.09 Lee start（達成済み側は入力控件を突出させない。旧行はコメント保持）
		// Display.ControlHighlight = FName(TEXT("DPad"));
		Display.ControlHighlight = bPlayerDone ? FName(TEXT("None")) : FName(TEXT("DPad"));
		// 2026.10.09 Lee end
		break;
	case ETutorialStep::PlaceArrow:
		// 実際の目標方向に対応するボタン（Y=上 / A=下 / X=左 / B=右）
		// 2026.10.09 Lee start（達成済み側は入力控件を突出させない。旧行はコメント保持）
		// Display.ControlHighlight = DirectionToButton(GameMode->GetExpectedDirectionFor(PlayerId));
		Display.ControlHighlight = bPlayerDone ? FName(TEXT("None")) : DirectionToButton(GameMode->GetExpectedDirectionFor(PlayerId));
		// 2026.10.09 Lee end
		break;
	case ETutorialStep::ScoreGoal:
	case ETutorialStep::ObserveReverse:
	case ETutorialStep::SpeedBaseline:
	case ETutorialStep::ObserveSpeed:
	case ETutorialStep::SpeedRecovered:
		// 両側とも観察待ち
		Display.bWaiting = true;
		break;
	case ETutorialStep::ReverseSkill:
		if (PlayerId == ActiveId)
		{
			Display.bActive = true;
			Display.ControlHighlight = FName(TEXT("LB"));
		}
		else
		{
			Display.bWaiting = true;
		}
		break;
	case ETutorialStep::RepairArrow:
		if (PlayerId == AffectedId)
		{
			Display.bActive = true;
			Display.ControlHighlight = DirectionToButton(GameMode->GetExpectedDirectionFor(AffectedId));
		}
		else
		{
			Display.bWaiting = true;
		}
		break;
	case ETutorialStep::SpeedSkill:
		if (PlayerId == ActiveId)
		{
			Display.bActive = true;
			Display.ControlHighlight = FName(TEXT("RB"));
		}
		else
		{
			Display.bWaiting = true;
		}
		break;
	case ETutorialStep::Complete:
		Display.ControlHighlight = FName(TEXT("All"));
		break;
	default:
		break;
	}
	return Display;
}

FName UTutorialHUDWidget::DirectionToButton(ETileType Direction)
{
	// 配置ボタン対応: Y=上 / A=下 / X=左 / B=右
	switch (Direction)
	{
	case ETileType::DirUp:    return FName(TEXT("Y"));
	case ETileType::DirDown:  return FName(TEXT("A"));
	case ETileType::DirLeft:  return FName(TEXT("X"));
	case ETileType::DirRight: return FName(TEXT("B"));
	default:                  return FName(TEXT("None"));
	}
}

FText UTutorialHUDWidget::ComputeSideTaskText(uint8 PlayerId) const
{
	const ATutorialGameMode* GameMode = Cast<ATutorialGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (GameMode == nullptr)
	{
		return FText::GetEmpty();
	}
	const ETutorialStep Step = GameMode->GetCurrentStep();

	// 完了時は両側にボタン早見と技能の要点を示す
	// 2026.10.09 Lee start（操作速査に十字キー移動・LB の相手矢印反転・RB の全体加速
	// （両者に影響）を明記する。旧文言はコメントとして保持）
	// if (Step == ETutorialStep::Complete)
	// {
	// 	return FText::FromString(TEXT("LB: 反転 / RB: 加速　各2回・CD5秒"));
	// }
	// 2026.10.09 Lee start（完成速査を意味ごとに分行。情報は不変。旧 1 行はコメント保持）
	if (Step == ETutorialStep::Complete)
	{
		// return FText::FromString(TEXT("十字キー:移動　LB:相手の矢印を反転　RB:場の動物を加速(両者に影響)　各2回・CD5秒"));
		return FText::FromString(TEXT("十字キー:移動\nLB:相手の矢印を反転\nRB:場の動物を加速(両者に影響)\n各2回・CD5秒"));
	}
	// 2026.10.09 Lee end

	// 2026.10.09 Lee start（権威 IsTutorialPlayerDone による達成フィードバック。
	// 基礎 3 段階は達成側に達成と待機を明示し、未達成側は従来の目標提示のまま。
	// 観察 3 段階は施放者の達成表示と観察提示を複数行で併記。
	// 完了はキャッシュせず毎回 GameMode の権威状態から読むため、段階を跨ぐ汚染はない）
	const bool bPlayerDone = GameMode->IsTutorialPlayerDone(PlayerId);
	if (Step == ETutorialStep::MoveCursor || Step == ETutorialStep::PlaceArrow || Step == ETutorialStep::ScoreGoal)
	{
		if (bPlayerDone)
		{
			// 2026.10.09 Lee start（達成と待機を独立した 2 行に分ける。旧 1 行はコメント保持）
			// return FText::FromString(TEXT("目標達成！あいての操作を待っています"));
			// 2026.10.09 Lee start（実機確認：28 号・幅 360 で 2 行目「あいての操作を待っています」は
			// 末尾 1 文字が折り返るため、待機行のみ「あいてを待っています」へ短縮する。
			// 1 行目と真实改行は不変。旧 2 行目はコメント保持）
			// return FText::FromString(TEXT("目標達成！\nあいての操作を待っています"));
			return FText::FromString(TEXT("目標達成！\nあいてを待っています"));
			// 2026.10.09 Lee end
			// 2026.10.09 Lee end
		}
	}
	if (Step == ETutorialStep::ObserveReverse || Step == ETutorialStep::ObserveSpeed || Step == ETutorialStep::SpeedRecovered)
	{
		if (bPlayerDone)
		{
			return FText::FromString(TEXT("達成！\n引き続き動物の様子を観察してください"));
		}
	}
	// 2026.10.09 Lee end

	const FParticipantDisplay Display = ComputeParticipantDisplay(PlayerId);
	if (Display.bActive)
	{
		// 操作担当（技能実習・修復）
		return FText::FromString(TEXT("あなたの番です"));
	}
	if (Display.bWaiting)
	{
		// 2026.10.09 Lee start（待機文言を段階別に修正する。
		// 修復段階の待機側は「修復担当＝1 - 操作者」を指す。観察段階は観察提示へ切替、
		// 加速観察は両者の動物が対象であることを明記。技能実習の待機側は従来どおり
		// 操作者を指す。旧実装は全段階で操作者を指していたため下記に保持）
		// const uint8 ActiveId = GameMode->GetCurrentActivePlayerId();
		// const FString ActiveLabel = (ActiveId == 0) ? TEXT("1P") : TEXT("2P");
		// return FText::FromString(FString::Printf(TEXT("%s の操作を待っています"), *ActiveLabel));
		const uint8 ActiveId = GameMode->GetCurrentActivePlayerId();
		switch (Step)
		{
		case ETutorialStep::RepairArrow:
		{
			// 修復担当は反転の影響を受けた側（1 - 操作者）
			const uint8 RepairPlayerId = 1u - ActiveId;
			const FString RepairLabel = (RepairPlayerId == 0) ? TEXT("1P") : TEXT("2P");
			return FText::FromString(FString::Printf(TEXT("%s が矢印を直すのを待っています"), *RepairLabel));
		}
		case ETutorialStep::ObserveReverse:
			// 反転観察は両側に観察を促す（技能入力の待ちではない）
			return FText::FromString(TEXT("動物の流れが変わる様子を観察してください"));
		case ETutorialStep::SpeedBaseline:
			// 加速前の基線観察
			return FText::FromString(TEXT("通常速度の動物を観察してください"));
		case ETutorialStep::ObserveSpeed:
			// 2026.10.09 Lee start（観察と対象を 2 行に分ける。旧 1 行はコメント保持）
			// 加速は両者の動物へ影響する点を明記
			// return FText::FromString(TEXT("加速中の動物を観察してください（両者の動物が対象）"));
			return FText::FromString(TEXT("加速中の動物を観察してください\n（両者の動物が対象）"));
			// 2026.10.09 Lee end
		case ETutorialStep::SpeedRecovered:
			// 復帰観察
			return FText::FromString(TEXT("通常速度へ戻る様子を観察してください"));
		default:
			// ReverseSkill / SpeedSkill の待機側は操作者（施放者）を指し示す
			const FString ActiveLabel = (ActiveId == 0) ? TEXT("1P") : TEXT("2P");
			return FText::FromString(FString::Printf(TEXT("%s の操作を待っています"), *ActiveLabel));
		}
		// 2026.10.09 Lee end
	}
	// 2026.10.09 Lee start（基礎未達成側は段階ごとの短い具体提示へ（汎用長文の
	// 末尾 1 文字折返しを回避）。すべて明示的な 2 行。旧汎用文はコメント保持）
	// 基礎ステップ（両側が操作者）
	// return FText::FromString(TEXT("ふたりとも操作してください"));
	if (Step == ETutorialStep::MoveCursor)
	{
		return FText::FromString(TEXT("十字キーで\n光るマスへ移動しよう"));
	}
	if (Step == ETutorialStep::PlaceArrow)
	{
		return FText::FromString(TEXT("光るマスに\n正しい向きの矢印を置こう"));
	}
	if (Step == ETutorialStep::ScoreGoal)
	{
		return FText::FromString(TEXT("動物を\n自分のゴールへ入れよう"));
	}
	return FText::FromString(TEXT("ふたりとも操作してください"));
	// 2026.10.09 Lee end
}

void UTutorialHUDWidget::RefreshParticipants()
{
	for (uint8 PlayerId = 0; PlayerId < 2; ++PlayerId)
	{
		const FParticipantDisplay Display = ComputeParticipantDisplay(PlayerId);

		// パッド早見の強調と任務文言（原生呈現）
		UTutorialGamepadWidget* Gamepad = (PlayerId == 0) ? P1Gamepad.Get() : P2Gamepad.Get();
		if (Gamepad != nullptr)
		{
			Gamepad->SetHighlightedControl(Display.ControlHighlight);
		}
		UTextBlock* TaskText = (PlayerId == 0) ? P1TaskText.Get() : P2TaskText.Get();
		if (TaskText != nullptr)
		{
			TaskText->SetText(ComputeSideTaskText(PlayerId));
		}

		// ブループリント実装への通知（PlayerId で左右を明示）
		UpdateTutorialParticipant(PlayerId, Display.bActive, Display.bWaiting, Display.ControlHighlight);
	}
}

void UTutorialHUDWidget::RefreshSkillCards()
{
	const ATutorialGameMode* GameMode = Cast<ATutorialGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (GameMode == nullptr)
	{
		return;
	}
	const ETutorialStep Step = GameMode->GetCurrentStep();

	// 表示する技能: 反転系ステップはスロット 0、加速系ステップはスロット 1、それ以外は非表示
	int32 SlotIndex = INDEX_NONE;
	if (Step == ETutorialStep::ReverseSkill || Step == ETutorialStep::ObserveReverse || Step == ETutorialStep::RepairArrow)
	{
		SlotIndex = 0;
	}
	else if (Step == ETutorialStep::SpeedBaseline || Step == ETutorialStep::SpeedSkill
		|| Step == ETutorialStep::ObserveSpeed || Step == ETutorialStep::SpeedRecovered)
	{
		SlotIndex = 1;
	}

	for (uint8 PlayerId = 0; PlayerId < 2; ++PlayerId)
	{
		UPanelWidget* CardBox = (PlayerId == 0) ? P1SkillCardBox.Get() : P2SkillCardBox.Get();
		UImage* CardImage = (PlayerId == 0) ? P1SkillCardImage.Get() : P2SkillCardImage.Get();
		UTextBlock* UsesText = (PlayerId == 0) ? P1SkillUsesText.Get() : P2SkillUsesText.Get();
		UTextBlock* CooldownText = (PlayerId == 0) ? P1SkillCooldownText.Get() : P2SkillCooldownText.Get();
		if (CardBox == nullptr || CardImage == nullptr || UsesText == nullptr || CooldownText == nullptr)
		{
			continue;
		}

		// 未初期化・非表示ステップではカードを隠す（空スナップショットを技能があるようには見せない）
		TArray<FSkillSlotSnapshot> Snapshots;
		const AAnimalGatherPlayerController* PC = Cast<AAnimalGatherPlayerController>(
			UGameplayStatics::GetPlayerController(GetWorld(), PlayerId));
		if (PC != nullptr && PC->GetSkillSystemComponent() != nullptr)
		{
			Snapshots = PC->GetSkillSystemComponent()->GetSnapshot();
		}

		if (SlotIndex == INDEX_NONE || !Snapshots.IsValidIndex(SlotIndex))
		{
			CardBox->SetVisibility(ESlateVisibility::Collapsed);
			if (SlotIndex != INDEX_NONE)
			{
				// 表示ステップだが未初期化: 準備中として枠だけ見せる（回数の捏造はしない）
				CardBox->SetVisibility(ESlateVisibility::HitTestInvisible);
				UsesText->SetText(FText::FromString(TEXT("準備中")));
				CooldownText->SetText(FText::GetEmpty());
			}
			UpdateTutorialSkillState(PlayerId, Snapshots);
			continue;
		}

		CardBox->SetVisibility(ESlateVisibility::HitTestInvisible);
		const FSkillSlotSnapshot& Snapshot = Snapshots[SlotIndex];
		if (Snapshot.Definition != nullptr && Snapshot.Definition->DisplayTexture != nullptr)
		{
			CardImage->SetBrushFromTexture(Snapshot.Definition->DisplayTexture, false);
		}
		UsesText->SetText(FText::FromString(FString::Printf(TEXT("残り %d 回"), Snapshot.RemainingUses)));

		// 2026.10.09 Lee start（権威スナップショットの 3 値で状態を 4 分類する。
		//  冷却 0 を即「使用可能」と同一視せず、bCanUse の権威判定を優先する。
		//  使用不可のカードは明度を下げて強調度を落とす）
		// if (Snapshot.CooldownRemaining > 0.0f) … else 「使用可能」 ←元のコードは消さない
		// （旧実装は冷却残り 0 秒をそのまま使用可能表示へ落としていた）
		const FLinearColor CardDimColor(0.4f, 0.4f, 0.4f, 1.0f);
		const FLinearColor CardFullColor(1.0f, 1.0f, 1.0f, 1.0f);
		FLinearColor CardColor = CardDimColor;
		if (Snapshot.RemainingUses <= 0)
		{
			// 回数耗尽: 冷却が切れても復活しないため専用表示にする
			CooldownText->SetText(FText::FromString(TEXT("回数使い切り")));
		}
		else if (Snapshot.CooldownRemaining > 0.0f)
		{
			CooldownText->SetText(FText::FromString(FString::Printf(TEXT("CD 残り %.1f 秒"), Snapshot.CooldownRemaining)));
		}
		else if (Snapshot.bCanUse)
		{
			// 権威判定で使用可能（段階・操作者・スロットの許可を含む）
			CooldownText->SetText(FText::FromString(TEXT("使用可能")));
			CardColor = CardFullColor;
		}
		else
		{
			// 回数・冷却とも問題ないが段階権限で使用不可（観察・基線などの待ち状態）
			CooldownText->SetText(FText::FromString(TEXT("待機中")));
		}
		CardImage->SetColorAndOpacity(CardColor);
		// 2026.10.09 Lee end
		UpdateTutorialSkillState(PlayerId, Snapshots);
	}
}

void UTutorialHUDWidget::RefreshSharedEffect()
{
	if (SharedEffectText == nullptr)
	{
		return;
	}
	const ATutorialGameMode* GameMode = Cast<ATutorialGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (GameMode == nullptr)
	{
		return;
	}
	const UMatchSkillEffectComponent* Effect = GameMode->GetMatchSkillEffect();
	const FMatchSpeedSnapshot Snapshot = (Effect != nullptr) ? Effect->GetSnapshot() : FMatchSpeedSnapshot();

	if (Snapshot.SpeedMultiplier > 1.0f && Snapshot.RemainingDuration > 0.0f)
	{
		SharedEffectText->SetText(FText::FromString(FString::Printf(TEXT("加速中: 残り %.1f 秒"), Snapshot.RemainingDuration)));
		SharedEffectText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		SharedEffectText->SetVisibility(ESlateVisibility::Collapsed);
	}
	UpdateTutorialSharedSpeedEffect(Snapshot);
}
