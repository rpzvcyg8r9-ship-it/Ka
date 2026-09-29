#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "EisenwalGameInstance.generated.h"

/** Lebt ueber Levelwechsel hinweg; haelt spaeter Spielstand, Fortschritt und Einstellungen. */
UCLASS()
class EISENWAL_API UEisenwalGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
};
