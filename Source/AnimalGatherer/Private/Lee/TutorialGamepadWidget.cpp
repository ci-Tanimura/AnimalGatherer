// Fill out your copyright notice in the Description page of Project Settings.

#include "Lee/TutorialGamepadWidget.h"

// 2026.10.08 Lee : 教程用ゲームパッド早見 Widget 実装
// 2026.10.08 Lee 返工 start（SizeBox 根・十字キー十字形・ABXY 方向表示・LB/RB 文字・動的文字色）
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Texture2D.h"
// 2026.10.09 Lee start（角丸ブラシ構築に FSlateBrush / FSlateColor の完全型が必要）
#include "Styling/SlateBrush.h"
// 2026.10.09 Lee end

namespace
{
	// 2026.10.09 Lee start（確定線稿：1470x1070 の線稿源図に合わせ設計 735x535 へ変更。
	// 旧値はコメントとして保持）
	/** @brief 設計座標のサイズ（1470x1070 の線稿源図の半分）。 */
	// constexpr float DesignWidth = 768.0f;
	// constexpr float DesignHeight = 512.0f;
	constexpr float DesignWidth = 735.0f;
	constexpr float DesignHeight = 535.0f;
	// 2026.10.09 Lee end

	// 2026.10.09 Lee start（非強調は透明度 0 とし、黒矩形で線稿を汚さない。旧色はコメント保持）
	/** @brief 強調なしの指示レイヤ色（完全透明）。 */
	// const FLinearColor MarkerDimColor(0.0f, 0.0f, 0.0f, 0.25f);
	const FLinearColor MarkerDimColor(0.0f, 0.0f, 0.0f, 0.0f);
	// 2026.10.09 Lee end

	/** @brief 各ボタン強調色（Y 黄 / A 緑 / X 青 / B 赤 / 十字キー白 / LB,RB は青白）。 */
	const FLinearColor MarkerHighlightWhite(1.0f, 1.0f, 1.0f, 0.85f);
	const FLinearColor MarkerHighlightY(1.0f, 0.85f, 0.10f, 0.90f);
	const FLinearColor MarkerHighlightA(0.20f, 1.0f, 0.35f, 0.90f);
	const FLinearColor MarkerHighlightX(0.25f, 0.50f, 1.0f, 0.90f);
	const FLinearColor MarkerHighlightB(1.0f, 0.25f, 0.25f, 0.90f);
	const FLinearColor MarkerHighlightShoulder(0.40f, 0.90f, 1.0f, 0.90f);

	// 2026.10.08 Lee 返工 start（高力度对比のための文字色。強調時は深色、非強調時は白色）
	/** @brief 強調ボタンの文字色（明色背景の上で読める深色）。 */
	const FLinearColor TextOnHighlightColor(0.02f, 0.02f, 0.04f, 1.0f);

	/** @brief 非強調ボタンの文字色（暗色背景の上で読める白色）。 */
	const FLinearColor TextOnDimColor(1.0f, 1.0f, 1.0f, 1.0f);
	// 2026.10.08 Lee 返工 end
}

void UTutorialGamepadWidget::SetBodyTexture(UTexture2D* InTexture)
{
	// 2026.10.08 Lee 第三批 start（Slate 構築前の呼び出しに備えてキャッシュしてから適用する）
	CachedBodyTexture = InTexture;
	if (BodyImage == nullptr || InTexture == nullptr)
	{
		return;
	}
	// 輪郭テクスチャをそのまま等比で表示する（UI 用にサイズ同期はしない）
	BodyImage->SetBrushFromTexture(InTexture, false);
	// 2026.10.08 Lee 第三批 end
}

void UTutorialGamepadWidget::SetHighlightedControl(FName ControlName)
{
	HighlightedControl = ControlName;
	ApplyHighlight();
}

// 2026.10.08 Lee start（Slate 構築前に Root を組み替えるため RebuildWidget を使用）
// 2026.10.08 Lee 返工 start（RebuildWidget の実際の署名は TSharedRef<SWidget> を返す）
TSharedRef<SWidget> UTutorialGamepadWidget::RebuildWidget()
{
	BuildNativeLayout();
	return Super::RebuildWidget();
}
// 2026.10.08 Lee 返工 end
// 2026.10.08 Lee end

void UTutorialGamepadWidget::BuildNativeLayout()
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

	// 2026.10.08 Lee 返工 start（根は USizeBox で 768x512 の設計 DesiredSize を確定し、その中に Canvas を置く）
	// 既存 Root（ブループリント側で設定されていた場合）は差し替え前に読み取って折り畳み保持する
	UWidget* OldRoot = Tree->RootWidget.Get();
	USizeBox* RootSizeBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("GamepadRootSizeBox"));
	RootSizeBox->SetWidthOverride(DesignWidth);
	RootSizeBox->SetHeightOverride(DesignHeight);
	Tree->RootWidget = RootSizeBox;

	UCanvasPanel* Canvas = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("GamepadRootCanvas"));
	RootSizeBox->SetContent(Canvas);

	if (OldRoot != nullptr && OldRoot != RootSizeBox)
	{
		if (UCanvasPanelSlot* OldSlot = Canvas->AddChildToCanvas(OldRoot))
		{
			OldSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			OldSlot->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 0.0f));
		}
		OldRoot->SetVisibility(ESlateVisibility::Collapsed);
	}
	// 2026.10.08 Lee 返工 end

	// 底図（輪郭）。テクスチャは SetBodyTexture で後から渡される
	BodyImage = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("GamepadBody"));
	if (UCanvasPanelSlot* BodySlot = Canvas->AddChildToCanvas(BodyImage))
	{
		// 2026.10.08 Lee 返工 start（全面拉伸アンカーでは Offsets を全 0 にする（右/下を残すと余白で 0 サイズへ縮む））
		BodySlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		BodySlot->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 0.0f));
		// 2026.10.08 Lee 返工 end
	}
	BodyImage->SetVisibility(ESlateVisibility::HitTestInvisible);

	// ── 指示レイヤ（輪郭源図 1536x1024 の中心座標を半分にした設計位置） ──
	// LB=(400,150)/2、RB=(1136,150)/2、十字キー中心=(460,540)/2、
	// Y=(1120,370)/2、A=(1120,570)/2、X=(1020,470)/2、B=(1220,470)/2
	// 2026.10.08 Lee 返工 start（ABXY は 64x44 に拡大し、字母 + 方向（↑↓←→）を併記する）
	// 2026.10.09 Lee start（境界返工：肩キー Marker を 116x54 へ拡大して 36 号文字を覆い、
	// ABXY Marker を 76x48 へ拡大して単行ラベルの背景を覆う。中心座標は不変）
	// 【旧実装（保持）】
	// LBMarker = AddMarkerToCanvas(Canvas, TEXT("LBMarker"), FVector2D(200.0f, 75.0f), FVector2D(96.0f, 44.0f));
	// RBMarker = AddMarkerToCanvas(Canvas, TEXT("RBMarker"), FVector2D(568.0f, 75.0f), FVector2D(96.0f, 44.0f));
	// 2026.10.09 Lee start（確定線稿：中心 LB(170,58) / RB(566,58)、肩 94x44。旧行はコメント保持）
	// LBMarker = AddMarkerToCanvas(Canvas, TEXT("LBMarker"), FVector2D(200.0f, 75.0f), FVector2D(116.0f, 54.0f));
	// RBMarker = AddMarkerToCanvas(Canvas, TEXT("RBMarker"), FVector2D(568.0f, 75.0f), FVector2D(116.0f, 54.0f));
	LBMarker = AddMarkerToCanvas(Canvas, TEXT("LBMarker"), FVector2D(170.0f, 58.0f), FVector2D(94.0f, 44.0f));
	RBMarker = AddMarkerToCanvas(Canvas, TEXT("RBMarker"), FVector2D(566.0f, 58.0f), FVector2D(94.0f, 44.0f));
	// 2026.10.09 Lee end

	// 2026.10.09 Lee start（確定線稿：十字中心 (270,303)、横 94x29 / 縦 29x94。旧行はコメント保持）
	// 十字キーは十字形（横腕 + 縦腕）で示す
	// DPadHBar = AddMarkerToCanvas(Canvas, TEXT("DPadHBar"), FVector2D(230.0f, 270.0f), FVector2D(60.0f, 20.0f));
	// DPadVBar = AddMarkerToCanvas(Canvas, TEXT("DPadVBar"), FVector2D(230.0f, 270.0f), FVector2D(20.0f, 60.0f));
	DPadHBar = AddMarkerToCanvas(Canvas, TEXT("DPadHBar"), FVector2D(270.0f, 303.0f), FVector2D(94.0f, 29.0f));
	DPadVBar = AddMarkerToCanvas(Canvas, TEXT("DPadVBar"), FVector2D(270.0f, 303.0f), FVector2D(29.0f, 94.0f));
	// 2026.10.09 Lee end

	// 2026.10.09 Lee start（確定線稿：ABXY 52x52 円形、中心 Y(571,139) / A(571,237) / X(524,189) / B(619,189)。
	// 旧行はコメントとして保持）
	// ABXY: 上段が字母、下段が対応方向
	// 【旧実装（保持）】
	// YMarker = AddMarkerToCanvas(Canvas, TEXT("YMarker"), FVector2D(560.0f, 185.0f), FVector2D(64.0f, 44.0f));
	// AMarker = AddMarkerToCanvas(Canvas, TEXT("AMarker"), FVector2D(560.0f, 285.0f), FVector2D(64.0f, 44.0f));
	// XMarker = AddMarkerToCanvas(Canvas, TEXT("XMarker"), FVector2D(510.0f, 235.0f), FVector2D(64.0f, 44.0f));
	// BMarker = AddMarkerToCanvas(Canvas, TEXT("BMarker"), FVector2D(610.0f, 235.0f), FVector2D(64.0f, 44.0f));
	// YMarker = AddMarkerToCanvas(Canvas, TEXT("YMarker"), FVector2D(560.0f, 185.0f), FVector2D(76.0f, 48.0f));
	// AMarker = AddMarkerToCanvas(Canvas, TEXT("AMarker"), FVector2D(560.0f, 285.0f), FVector2D(76.0f, 48.0f));
	// XMarker = AddMarkerToCanvas(Canvas, TEXT("XMarker"), FVector2D(510.0f, 235.0f), FVector2D(76.0f, 48.0f));
	// BMarker = AddMarkerToCanvas(Canvas, TEXT("BMarker"), FVector2D(610.0f, 235.0f), FVector2D(76.0f, 48.0f));
	YMarker = AddMarkerToCanvas(Canvas, TEXT("YMarker"), FVector2D(571.0f, 139.0f), FVector2D(52.0f, 52.0f));
	AMarker = AddMarkerToCanvas(Canvas, TEXT("AMarker"), FVector2D(571.0f, 237.0f), FVector2D(52.0f, 52.0f));
	XMarker = AddMarkerToCanvas(Canvas, TEXT("XMarker"), FVector2D(524.0f, 189.0f), FVector2D(52.0f, 52.0f));
	BMarker = AddMarkerToCanvas(Canvas, TEXT("BMarker"), FVector2D(619.0f, 189.0f), FVector2D(52.0f, 52.0f));
	// 2026.10.09 Lee end

	// 2026.10.09 Lee start（PIE 実機でボタン文字が小さすぎたため、ABXY/LB/RB の全ラベルを
	// 36 へ統一し、文字枠を中心固定のまま拡大する。字母と方向矢印の併記は維持）
	// 【旧実装（保持）】
	// LBText = AddTextToCanvas(Canvas, TEXT("LBText"), TEXT("LB"), 30.0f, FVector2D(200.0f, 75.0f), FVector2D(96.0f, 44.0f));
	// RBText = AddTextToCanvas(Canvas, TEXT("RBText"), TEXT("RB"), 30.0f, FVector2D(568.0f, 75.0f), FVector2D(96.0f, 44.0f));
	//
	// YLetter = AddTextToCanvas(Canvas, TEXT("YLetter"), TEXT("Y"), 22.0f, FVector2D(560.0f, 176.0f), FVector2D(64.0f, 22.0f));
	// ALetter = AddTextToCanvas(Canvas, TEXT("ALetter"), TEXT("A"), 22.0f, FVector2D(560.0f, 276.0f), FVector2D(64.0f, 22.0f));
	// XLetter = AddTextToCanvas(Canvas, TEXT("XLetter"), TEXT("X"), 22.0f, FVector2D(510.0f, 226.0f), FVector2D(64.0f, 22.0f));
	// BLetter = AddTextToCanvas(Canvas, TEXT("BLetter"), TEXT("B"), 22.0f, FVector2D(610.0f, 226.0f), FVector2D(64.0f, 22.0f));
	// YArrow = AddTextToCanvas(Canvas, TEXT("YArrow"), TEXT("↑"), 20.0f, FVector2D(560.0f, 198.0f), FVector2D(64.0f, 22.0f));
	// AArrow = AddTextToCanvas(Canvas, TEXT("AArrow"), TEXT("↓"), 20.0f, FVector2D(560.0f, 298.0f), FVector2D(64.0f, 22.0f));
	// XArrow = AddTextToCanvas(Canvas, TEXT("XArrow"), TEXT("←"), 20.0f, FVector2D(510.0f, 248.0f), FVector2D(64.0f, 22.0f));
	// BArrow = AddTextToCanvas(Canvas, TEXT("BArrow"), TEXT("→"), 20.0f, FVector2D(610.0f, 248.0f), FVector2D(64.0f, 22.0f));
	// 2026.10.09 Lee start（確定線稿：肩文字は 32 号・94x44、中心は肩中心と一致。旧行はコメント保持）
	// 肩キーの文字（中心はレイアウト固定座標のまま、枠のみ拡大）
	// LBText = AddTextToCanvas(Canvas, TEXT("LBText"), TEXT("LB"), 36.0f, FVector2D(200.0f, 75.0f), FVector2D(116.0f, 54.0f));
	// RBText = AddTextToCanvas(Canvas, TEXT("RBText"), TEXT("RB"), 36.0f, FVector2D(568.0f, 75.0f), FVector2D(116.0f, 54.0f));
	LBText = AddTextToCanvas(Canvas, TEXT("LBText"), TEXT("LB"), 32.0f, FVector2D(170.0f, 58.0f), FVector2D(94.0f, 44.0f));
	RBText = AddTextToCanvas(Canvas, TEXT("RBText"), TEXT("RB"), 32.0f, FVector2D(566.0f, 58.0f), FVector2D(94.0f, 44.0f));
	// 2026.10.09 Lee end

	// 2026.10.09 Lee start（境界返工：上下 2 行の 36 号ラベルは中心間 22 単位で必ず混雑するため、
	// ABXY は字母と方向矢印を 1 行に並べた単一ラベルへ変更する。
	// 36 号・76x48 枠・中心は各ボタン中心と一致。旧 2 行構築はコメントとして保持）
	// 【旧実装（保持）】
	// YLetter = AddTextToCanvas(Canvas, TEXT("YLetter"), TEXT("Y"), 36.0f, FVector2D(560.0f, 176.0f), FVector2D(76.0f, 44.0f));
	// ALetter = AddTextToCanvas(Canvas, TEXT("ALetter"), TEXT("A"), 36.0f, FVector2D(560.0f, 276.0f), FVector2D(76.0f, 44.0f));
	// XLetter = AddTextToCanvas(Canvas, TEXT("XLetter"), TEXT("X"), 36.0f, FVector2D(510.0f, 226.0f), FVector2D(76.0f, 44.0f));
	// BLetter = AddTextToCanvas(Canvas, TEXT("BLetter"), TEXT("B"), 36.0f, FVector2D(610.0f, 226.0f), FVector2D(76.0f, 44.0f));
	// YArrow = AddTextToCanvas(Canvas, TEXT("YArrow"), TEXT("↑"), 36.0f, FVector2D(560.0f, 198.0f), FVector2D(76.0f, 40.0f));
	// AArrow = AddTextToCanvas(Canvas, TEXT("AArrow"), TEXT("↓"), 36.0f, FVector2D(560.0f, 298.0f), FVector2D(76.0f, 40.0f));
	// XArrow = AddTextToCanvas(Canvas, TEXT("XArrow"), TEXT("←"), 36.0f, FVector2D(510.0f, 248.0f), FVector2D(76.0f, 40.0f));
	// BArrow = AddTextToCanvas(Canvas, TEXT("BArrow"), TEXT("→"), 36.0f, FVector2D(610.0f, 248.0f), FVector2D(76.0f, 40.0f));
	// 2026.10.09 Lee start（確定線稿：ABXY 単行ラベルは 32 号・58x42、中心は各ボタン中心と一致。
	// 旧行はコメントとして保持）
	// YLetter = AddTextToCanvas(Canvas, TEXT("YLabel"), TEXT("Y↑"), 36.0f, FVector2D(560.0f, 185.0f), FVector2D(76.0f, 48.0f));
	// ALetter = AddTextToCanvas(Canvas, TEXT("ALabel"), TEXT("A↓"), 36.0f, FVector2D(560.0f, 285.0f), FVector2D(76.0f, 48.0f));
	// XLetter = AddTextToCanvas(Canvas, TEXT("XLabel"), TEXT("X←"), 36.0f, FVector2D(510.0f, 235.0f), FVector2D(76.0f, 48.0f));
	// BLetter = AddTextToCanvas(Canvas, TEXT("BLabel"), TEXT("B→"), 36.0f, FVector2D(610.0f, 235.0f), FVector2D(76.0f, 48.0f));
	YLetter = AddTextToCanvas(Canvas, TEXT("YLabel"), TEXT("Y↑"), 32.0f, FVector2D(571.0f, 139.0f), FVector2D(58.0f, 42.0f));
	ALetter = AddTextToCanvas(Canvas, TEXT("ALabel"), TEXT("A↓"), 32.0f, FVector2D(571.0f, 237.0f), FVector2D(58.0f, 42.0f));
	XLetter = AddTextToCanvas(Canvas, TEXT("XLabel"), TEXT("X←"), 32.0f, FVector2D(524.0f, 189.0f), FVector2D(58.0f, 42.0f));
	BLetter = AddTextToCanvas(Canvas, TEXT("BLabel"), TEXT("B→"), 32.0f, FVector2D(619.0f, 189.0f), FVector2D(58.0f, 42.0f));
	// 2026.10.09 Lee end
	// 2 行目の方向矢印（YArrow など）は本方針では構築しない。メンバーは nullptr のまま残り、
	// ApplyHighlight 側の null 保護が既存のため追加改修は不要
	// 2026.10.09 Lee end
	// 2026.10.08 Lee 返工 end

	bNativeLayoutBuilt = true;
	// 2026.10.08 Lee 第三批 start（構築前に渡されていた底図があれば適用する）
	if (CachedBodyTexture != nullptr && BodyImage != nullptr)
	{
		BodyImage->SetBrushFromTexture(CachedBodyTexture, false);
	}
	// 2026.10.08 Lee 第三批 end
	ApplyHighlight();
}

UImage* UTutorialGamepadWidget::AddMarkerToCanvas(UCanvasPanel* Canvas, const FString& Name,
	const FVector2D& Center, const FVector2D& Size)
{
	// 2026.10.08 Lee 返工 start（GetWidgetTree は存在しないため保護メンバーの WidgetTree を直接読む）
	UWidgetTree* Tree = WidgetTree.Get();
	// 2026.10.08 Lee 返工 end
	UImage* Marker = Tree->ConstructWidget<UImage>(UImage::StaticClass(), FName(Name));
	// 2026.10.09 Lee start（C4458: UWidget::Slot メンバーとの遮蔽を避けるため局所名を MarkerSlot へ変更）
	if (UCanvasPanelSlot* MarkerSlot = Canvas->AddChildToCanvas(Marker))
	{
		MarkerSlot->SetPosition(Center - Size * 0.5f);
		MarkerSlot->SetSize(Size);
	}
	// 2026.10.09 Lee end

	// 2026.10.09 Lee start（確定線稿：角丸ハイライト。RoundedBox + FixedRadius を
	// Widget 名で区別する（ABXY = 52x52 の半分で正円 / 肩 = 半径 10 / 十字 = 半径 4）。
	// OutlineSettings・CornerRadii・RoundingType は SlateBrush.h の実 API。
	// 枠線は透明・幅 0 で塗りのみ。新テクスチャは追加しない）
	// 2026.10.09 Lee start（LNK2019 修正：FSlateBrush の既定構築はリンク失敗のため、
	// Marker の既存ブラシを GetBrush() で複製して流用する（圆角と機能は不変）。
	// 旧行はコメントとして保持）
	// FSlateBrush RoundedBrush;
	FSlateBrush RoundedBrush = Marker->GetBrush();
	// 2026.10.09 Lee end
	RoundedBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	RoundedBrush.OutlineSettings.Color = FSlateColor(FLinearColor::Transparent);
	RoundedBrush.OutlineSettings.Width = 0.0f;
	RoundedBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	float UniformRadius = 10.0f;
	if (Name == TEXT("YMarker") || Name == TEXT("AMarker") || Name == TEXT("XMarker") || Name == TEXT("BMarker"))
	{
		UniformRadius = 26.0f;
	}
	else if (Name.Contains(TEXT("DPad")))
	{
		UniformRadius = 4.0f;
	}
	RoundedBrush.OutlineSettings.CornerRadii = FVector4(UniformRadius, UniformRadius, UniformRadius, UniformRadius);
	Marker->SetBrush(RoundedBrush);
	// 2026.10.09 Lee end

	Marker->SetColorAndOpacity(MarkerDimColor);
	Marker->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Marker;
}

// 2026.10.08 Lee 返工 start（中央寄せ文字ブロックの生成ヘルパー）
UTextBlock* UTutorialGamepadWidget::AddTextToCanvas(UCanvasPanel* Canvas, const FString& Name,
	const TCHAR* Text, float FontSize, const FVector2D& Center, const FVector2D& Size)
{
	UWidgetTree* Tree = WidgetTree.Get();
	UTextBlock* TextBlock = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(Name));
	// 2026.10.09 Lee start（C4458: UWidget::Slot メンバーとの遮蔽を避けるため局所名を TextSlot へ変更）
	if (UCanvasPanelSlot* TextSlot = Canvas->AddChildToCanvas(TextBlock))
	{
		TextSlot->SetPosition(Center - Size * 0.5f);
		TextSlot->SetSize(Size);
	}
	// 2026.10.09 Lee end
	TextBlock->SetText(FText::FromString(Text));
	TextBlock->SetFontSize(FontSize);
	TextBlock->SetJustification(ETextJustify::Center);
	TextBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
	return TextBlock;
}
// 2026.10.08 Lee 返工 end

void UTutorialGamepadWidget::ApplyHighlight()
{
	// 2026.10.08 Lee 返工 start（十字腕の参照で構築済みを判定する）
	if (DPadHBar == nullptr)
	{
		return;
	}
	// 2026.10.08 Lee 返工 end

	// 強調対象の判定（ボタン名は大文字固定）
	const bool bAll = HighlightedControl == TEXT("All");
	const bool bDPad = bAll || HighlightedControl == TEXT("DPad");
	const bool bY = bAll || HighlightedControl == TEXT("Y");
	const bool bA = bAll || HighlightedControl == TEXT("A");
	const bool bX = bAll || HighlightedControl == TEXT("X");
	const bool bB = bAll || HighlightedControl == TEXT("B");
	const bool bLB = bAll || HighlightedControl == TEXT("LB");
	const bool bRB = bAll || HighlightedControl == TEXT("RB");

	// 背景の明暗で強調を表す（文字と位置は常に表示）
	// 2026.10.08 Lee 返工 start（単一 DPadMarker を十字の 2 腕へ置き換え）
	DPadHBar->SetColorAndOpacity(bDPad ? MarkerHighlightWhite : MarkerDimColor);
	DPadVBar->SetColorAndOpacity(bDPad ? MarkerHighlightWhite : MarkerDimColor);
	// 2026.10.08 Lee 返工 end
	YMarker->SetColorAndOpacity(bY ? MarkerHighlightY : MarkerDimColor);
	AMarker->SetColorAndOpacity(bA ? MarkerHighlightA : MarkerDimColor);
	XMarker->SetColorAndOpacity(bX ? MarkerHighlightX : MarkerDimColor);
	BMarker->SetColorAndOpacity(bB ? MarkerHighlightB : MarkerDimColor);
	LBMarker->SetColorAndOpacity(bLB ? MarkerHighlightShoulder : MarkerDimColor);
	RBMarker->SetColorAndOpacity(bRB ? MarkerHighlightShoulder : MarkerDimColor);

	// 2026.10.08 Lee 返工 start（文字色は強調時 深色 / 非強調時 白色 で对比を確保する）
	const FSlateColor LBTextColor = bLB ? TextOnHighlightColor : TextOnDimColor;
	const FSlateColor RBTextColor = bRB ? TextOnHighlightColor : TextOnDimColor;
	const FSlateColor YTextColor = bY ? TextOnHighlightColor : TextOnDimColor;
	const FSlateColor ATextColor = bA ? TextOnHighlightColor : TextOnDimColor;
	const FSlateColor XTextColor = bX ? TextOnHighlightColor : TextOnDimColor;
	const FSlateColor BTextColor = bB ? TextOnHighlightColor : TextOnDimColor;
	if (LBText != nullptr) { LBText->SetColorAndOpacity(LBTextColor); }
	if (RBText != nullptr) { RBText->SetColorAndOpacity(RBTextColor); }
	if (YLetter != nullptr) { YLetter->SetColorAndOpacity(YTextColor); }
	if (ALetter != nullptr) { ALetter->SetColorAndOpacity(ATextColor); }
	if (XLetter != nullptr) { XLetter->SetColorAndOpacity(XTextColor); }
	if (BLetter != nullptr) { BLetter->SetColorAndOpacity(BTextColor); }
	if (YArrow != nullptr) { YArrow->SetColorAndOpacity(YTextColor); }
	if (AArrow != nullptr) { AArrow->SetColorAndOpacity(ATextColor); }
	if (XArrow != nullptr) { XArrow->SetColorAndOpacity(XTextColor); }
	if (BArrow != nullptr) { BArrow->SetColorAndOpacity(BTextColor); }
	// 2026.10.08 Lee 返工 end
}
// 2026.10.08 Lee 返工 end
