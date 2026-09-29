#include "Core/EisenwalGameInstance.h"
#include "Eisenwal.h"

void UEisenwalGameInstance::Init()
{
	Super::Init();
	UE_LOG(LogEisenwal, Log, TEXT("EISENWAL gestartet."));
}
