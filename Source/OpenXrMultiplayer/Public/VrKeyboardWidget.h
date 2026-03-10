#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VrKeyboardWidget.generated.h"

class SEditableTextBox;
class UEditableTextBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVrKeyboardTextChanged, const FString&, Text);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVrKeyboardTextCommitted, const FString&, Text);

UCLASS(BlueprintType, Blueprintable)
class OPENXRMULTIPLAYER_API UVrKeyboardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UVrKeyboardWidget(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Keyboard")
	int32 MaxCharacters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Keyboard")
	bool bStartShiftEnabled;

	UPROPERTY(BlueprintAssignable, Category = "VR Keyboard")
	FVrKeyboardTextChanged OnKeyboardTextChanged;

	UPROPERTY(BlueprintAssignable, Category = "VR Keyboard")
	FVrKeyboardTextCommitted OnKeyboardTextCommitted;

	UFUNCTION(BlueprintCallable, Category = "VR Keyboard")
	void SetKeyboardText(const FString& NewText);

	UFUNCTION(BlueprintPure, Category = "VR Keyboard")
	FString GetKeyboardText() const;

	UFUNCTION(BlueprintCallable, Category = "VR Keyboard")
	void ClearKeyboardText();

	// ─────────────────────────────────────────────
	// Target Text Box — Blueprint tells us which box to write to
	// ─────────────────────────────────────────────

	/**
	 * Set which EditableTextBox the keyboard writes to on Enter.
	 *
	 * Call this from Blueprint whenever a text box gains focus:
	 *   TextBox_ServerName → OnFocused → SetTargetTextBox(TextBox_ServerName)
	 *   TextBox_Password   → OnFocused → SetTargetTextBox(TextBox_Password)
	 *
	 * When Enter is pressed, the keyboard text is written into this box
	 * and the keyboard is cleared for the next input.
	 */
	UFUNCTION(BlueprintCallable, Category = "VR Keyboard|Target")
	void SetTargetTextBox(UEditableTextBox* NewTarget);

	/** Returns the text box the keyboard will write to (may be null) */
	UFUNCTION(BlueprintPure, Category = "VR Keyboard|Target")
	UEditableTextBox* GetTargetTextBox() const;

	/** Returns true if a target text box is set and still valid */
	UFUNCTION(BlueprintPure, Category = "VR Keyboard|Target")
	bool HasTargetTextBox() const;

protected:
	virtual void NativeConstruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	enum class EKeyAction : uint8
	{
		Character,
		Backspace,
		Enter,
		Shift,
		Space,
		Clear
	};

	struct FKeyDefinition
	{
		FString Normal;
		FString Shifted;
		EKeyAction Action;
		float Width;
	};

	FString CurrentText;
	bool bShiftEnabled;
	TSharedPtr<SEditableTextBox> PreviewTextBox;

	/** Weak ref to the text box Blueprint told us to target */
	TWeakObjectPtr<UEditableTextBox> TargetTextBox;

	TSharedRef<SWidget> BuildKeyboardWidget();
	TSharedRef<SWidget> BuildKeyRow(const TArray<FKeyDefinition>& KeyRow);
	FReply HandleKeyPressed(FKeyDefinition KeyDef);
	FString GetDisplayLabel(const FKeyDefinition& KeyDef) const;
	void BroadcastTextChanged();
	void BroadcastTextCommitted();
};
