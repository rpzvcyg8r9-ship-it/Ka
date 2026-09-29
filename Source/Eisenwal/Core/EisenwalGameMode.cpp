#include "Core/EisenwalGameMode.h"
#include "Eisenwal.h"
#include "World/EisenwalSky.h"
#include "Ocean/EisenwalOcean.h"
#include "Ocean/EisenwalOceanSubsystem.h"
#include "EngineUtils.h"
#include "GameFramework/DefaultPawn.h"

AEisenwalGameMode::AEisenwalGameMode()
{
	DefaultPawnClass = ADefaultPawn::StaticClass();
}

void AEisenwalGameMode::StartPlay()
{
	EnsureWorldActors();

	if (UEisenwalOceanSubsystem* OceanSystem = GetWorld()->GetSubsystem<UEisenwalOceanSubsystem>())
	{
		OceanSystem->SetTargetSeaState(StartBeaufort, StartWindDirection, 0.01f);
	}

	Super::StartPlay();
}

void AEisenwalGameMode::EnsureWorldActors()
{
	UWorld* World = GetWorld();

	for (TActorIterator<AEisenwalSky> It(World); It; ++It)
	{
		Sky = *It;
		break;
	}
	for (TActorIterator<AEisenwalOcean> It(World); It; ++It)
	{
		Ocean = *It;
		break;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (!Sky)
	{
		Sky = World->SpawnActor<AEisenwalSky>(AEisenwalSky::StaticClass(), FTransform::Identity, Params);
		UE_LOG(LogEisenwal, Log, TEXT("Kein Himmel im Level gefunden - AEisenwalSky automatisch erzeugt."));
	}
	if (!Ocean)
	{
		Ocean = World->SpawnActor<AEisenwalOcean>(AEisenwalOcean::StaticClass(), FTransform::Identity, Params);
		UE_LOG(LogEisenwal, Log, TEXT("Kein Ozean im Level gefunden - AEisenwalOcean automatisch erzeugt."));
	}
}
