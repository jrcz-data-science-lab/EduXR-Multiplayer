#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VrKeyboard.generated.h"

class UUserWidget;
class UWidgetComponent;

UCLASS()
class OPENXRMULTIPLAYER_API AVrKeyboard : public AActor
{
	GENERATED_BODY()
	
public:	
	AVrKeyboard();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VR Keyboard")
	TSubclassOf<UUserWidget> KeyboardWidgetClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR Keyboard")
	UWidgetComponent* KeyboardWidgetComponent;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	USceneComponent* Root;
};
