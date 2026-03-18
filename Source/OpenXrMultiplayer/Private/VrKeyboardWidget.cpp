#include "VrKeyboardWidget.h"

#include "Components/EditableTextBox.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

UVrKeyboardWidget::UVrKeyboardWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, MaxCharacters(128)
	, bStartShiftEnabled(false)
	, bShiftEnabled(false)
{
}

void UVrKeyboardWidget::NativeConstruct()
{
	Super::NativeConstruct();
	bShiftEnabled = bStartShiftEnabled;
}

TSharedRef<SWidget> UVrKeyboardWidget::RebuildWidget()
{
	return BuildKeyboardWidget();
}

void UVrKeyboardWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	PreviewTextBox.Reset();
}

void UVrKeyboardWidget::SetKeyboardText(const FString& NewText)
{
	CurrentText = NewText.Left(MaxCharacters);
	BroadcastTextChanged();
}

FString UVrKeyboardWidget::GetKeyboardText() const
{
	return CurrentText;
}

void UVrKeyboardWidget::ClearKeyboardText()
{
	CurrentText.Reset();
	BroadcastTextChanged();
}

TSharedRef<SWidget> UVrKeyboardWidget::BuildKeyboardWidget()
{
	const TArray<FKeyDefinition> Row1 = {
		{TEXT("1"), TEXT("!"), EKeyAction::Character, 1.0f},
		{TEXT("2"), TEXT("@"), EKeyAction::Character, 1.0f},
		{TEXT("3"), TEXT("#"), EKeyAction::Character, 1.0f},
		{TEXT("4"), TEXT("$"), EKeyAction::Character, 1.0f},
		{TEXT("5"), TEXT("%"), EKeyAction::Character, 1.0f},
		{TEXT("6"), TEXT("^"), EKeyAction::Character, 1.0f},
		{TEXT("7"), TEXT("&"), EKeyAction::Character, 1.0f},
		{TEXT("8"), TEXT("*"), EKeyAction::Character, 1.0f},
		{TEXT("9"), TEXT("("), EKeyAction::Character, 1.0f},
		{TEXT("0"), TEXT(")"), EKeyAction::Character, 1.0f},
		{TEXT("Back"), TEXT("Back"), EKeyAction::Backspace, 1.8f}
	};

	const TArray<FKeyDefinition> Row2 = {
		{TEXT("q"), TEXT("Q"), EKeyAction::Character, 1.0f},
		{TEXT("w"), TEXT("W"), EKeyAction::Character, 1.0f},
		{TEXT("e"), TEXT("E"), EKeyAction::Character, 1.0f},
		{TEXT("r"), TEXT("R"), EKeyAction::Character, 1.0f},
		{TEXT("t"), TEXT("T"), EKeyAction::Character, 1.0f},
		{TEXT("y"), TEXT("Y"), EKeyAction::Character, 1.0f},
		{TEXT("u"), TEXT("U"), EKeyAction::Character, 1.0f},
		{TEXT("i"), TEXT("I"), EKeyAction::Character, 1.0f},
		{TEXT("o"), TEXT("O"), EKeyAction::Character, 1.0f},
		{TEXT("p"), TEXT("P"), EKeyAction::Character, 1.0f}
	};

	const TArray<FKeyDefinition> Row3 = {
		{TEXT("a"), TEXT("A"), EKeyAction::Character, 1.0f},
		{TEXT("s"), TEXT("S"), EKeyAction::Character, 1.0f},
		{TEXT("d"), TEXT("D"), EKeyAction::Character, 1.0f},
		{TEXT("f"), TEXT("F"), EKeyAction::Character, 1.0f},
		{TEXT("g"), TEXT("G"), EKeyAction::Character, 1.0f},
		{TEXT("h"), TEXT("H"), EKeyAction::Character, 1.0f},
		{TEXT("j"), TEXT("J"), EKeyAction::Character, 1.0f},
		{TEXT("k"), TEXT("K"), EKeyAction::Character, 1.0f},
		{TEXT("l"), TEXT("L"), EKeyAction::Character, 1.0f},
		{TEXT("Enter"), TEXT("Enter"), EKeyAction::Enter, 1.8f}
	};

	const TArray<FKeyDefinition> Row4 = {
		{TEXT("Shift"), TEXT("Shift"), EKeyAction::Shift, 1.8f},
		{TEXT("z"), TEXT("Z"), EKeyAction::Character, 1.0f},
		{TEXT("x"), TEXT("X"), EKeyAction::Character, 1.0f},
		{TEXT("c"), TEXT("C"), EKeyAction::Character, 1.0f},
		{TEXT("v"), TEXT("V"), EKeyAction::Character, 1.0f},
		{TEXT("b"), TEXT("B"), EKeyAction::Character, 1.0f},
		{TEXT("n"), TEXT("N"), EKeyAction::Character, 1.0f},
		{TEXT("m"), TEXT("M"), EKeyAction::Character, 1.0f},
		{TEXT(","), TEXT("<"), EKeyAction::Character, 1.0f},
		{TEXT("."), TEXT(">"), EKeyAction::Character, 1.0f},
		{TEXT("?"), TEXT("/"), EKeyAction::Character, 1.0f}
	};

	const TArray<FKeyDefinition> Row5 = {
		{TEXT("Clear"), TEXT("Clear"), EKeyAction::Clear, 1.6f},
		{TEXT("Space"), TEXT("Space"), EKeyAction::Space, 5.0f}
	};

	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
		.BorderBackgroundColor(FLinearColor::Transparent)
		.Padding(8.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SAssignNew(PreviewTextBox, SEditableTextBox)
				.IsReadOnly(true)
				.Text_Lambda([this]() { return FText::FromString(CurrentText); })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[BuildKeyRow(Row1)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[BuildKeyRow(Row2)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[BuildKeyRow(Row3)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[BuildKeyRow(Row4)]
			+ SVerticalBox::Slot().AutoHeight()[BuildKeyRow(Row5)]
		];
}

TSharedRef<SWidget> UVrKeyboardWidget::BuildKeyRow(const TArray<FKeyDefinition>& KeyRow)
{
	TSharedRef<SHorizontalBox> RowWidget = SNew(SHorizontalBox);

	for (const FKeyDefinition& KeyDef : KeyRow)
	{
		RowWidget->AddSlot()
		.FillWidth(KeyDef.Width)
		.Padding(2.0f)
		[
			SNew(SButton)
			.OnClicked_Lambda([this, KeyDef]() { return HandleKeyPressed(KeyDef); })
			.ContentPadding(FMargin(8.0f, 6.0f))
			[
				SNew(STextBlock)
				.Justification(ETextJustify::Center)
				.Text_Lambda([this, KeyDef]() { return FText::FromString(GetDisplayLabel(KeyDef)); })
			]
		];
	}

	return RowWidget;
}

FReply UVrKeyboardWidget::HandleKeyPressed(FKeyDefinition KeyDef)
{
	switch (KeyDef.Action)
	{
	case EKeyAction::Character:
		if (CurrentText.Len() < MaxCharacters)
		{
			CurrentText += bShiftEnabled ? KeyDef.Shifted : KeyDef.Normal;
			if (bShiftEnabled)
			{
				bShiftEnabled = false;
			}
			BroadcastTextChanged();
		}
		break;
	case EKeyAction::Backspace:
		if (!CurrentText.IsEmpty())
		{
			CurrentText.LeftChopInline(1);
			BroadcastTextChanged();
		}
		break;
	case EKeyAction::Enter:
		BroadcastTextCommitted();
		break;
	case EKeyAction::Shift:
		bShiftEnabled = !bShiftEnabled;
		break;
	case EKeyAction::Space:
		if (CurrentText.Len() < MaxCharacters)
		{
			CurrentText += TEXT(" ");
			BroadcastTextChanged();
		}
		break;
	case EKeyAction::Clear:
		CurrentText.Reset();
		BroadcastTextChanged();
		break;
	default:
		break;
	}

	Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
	return FReply::Handled();
}

FString UVrKeyboardWidget::GetDisplayLabel(const FKeyDefinition& KeyDef) const
{
	if (KeyDef.Action == EKeyAction::Character)
	{
		return bShiftEnabled ? KeyDef.Shifted : KeyDef.Normal;
	}

	if (KeyDef.Action == EKeyAction::Shift)
	{
		return bShiftEnabled ? TEXT("Shift*") : TEXT("Shift");
	}

	return KeyDef.Normal;
}

void UVrKeyboardWidget::BroadcastTextChanged()
{
	if (PreviewTextBox.IsValid())
	{
		PreviewTextBox->SetText(FText::FromString(CurrentText));
	}

	OnKeyboardTextChanged.Broadcast(CurrentText);
}

void UVrKeyboardWidget::BroadcastTextCommitted()
{
	// Write the keyboard text into the targeted text box (if one is set)
	if (TargetTextBox.IsValid())
	{
		TargetTextBox->SetText(FText::FromString(CurrentText));
	}

	OnKeyboardTextCommitted.Broadcast(CurrentText);

	// Clear keyboard after committing so it's ready for the next input
	CurrentText.Reset();
	BroadcastTextChanged();
}


// ═══════════════════════════════════════════════════
// Target Text Box — Blueprint-driven focus tracking
// ═══════════════════════════════════════════════════

void UVrKeyboardWidget::SetTargetTextBox(UEditableTextBox* NewTarget)
{
	TargetTextBox = NewTarget;

	// Load the target's existing text into the keyboard so the player can edit it
	if (NewTarget)
	{
		CurrentText = NewTarget->GetText().ToString();
		BroadcastTextChanged();
	}
}

UEditableTextBox* UVrKeyboardWidget::GetTargetTextBox() const
{
	return TargetTextBox.Get();
}

bool UVrKeyboardWidget::HasTargetTextBox() const
{
	return TargetTextBox.IsValid();
}
