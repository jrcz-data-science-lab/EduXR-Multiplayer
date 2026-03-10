#include "VrKeyboard.h"
#include "VrKeyboardWidget.h"

#include "Components/WidgetComponent.h"

AVrKeyboard::AVrKeyboard()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	KeyboardWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("KeyboardWidget"));
	KeyboardWidgetComponent->SetupAttachment(RootComponent);
	KeyboardWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	KeyboardWidgetComponent->SetDrawSize(FIntPoint(1800, 700));
	KeyboardWidgetComponent->SetTwoSided(true);
	KeyboardWidgetComponent->SetPivot(FVector2D(0.5f, 0.5f));

	KeyboardWidgetClass = UVrKeyboardWidget::StaticClass();
	KeyboardWidgetComponent->SetWidgetClass(KeyboardWidgetClass);
}

void AVrKeyboard::BeginPlay()
{
	Super::BeginPlay();

	if (KeyboardWidgetClass)
	{
		KeyboardWidgetComponent->SetWidgetClass(KeyboardWidgetClass);
	}
}

void AVrKeyboard::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
