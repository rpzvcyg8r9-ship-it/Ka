#include "Ocean/EisenwalOcean.h"
#include "Ocean/EisenwalOceanSubsystem.h"
#include "Core/EisenwalSettings.h"
#include "Eisenwal.h"
#include "Async/ParallelFor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	UMaterialInterface* GetFallbackOceanMaterial()
	{
		static const TCHAR* Path = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
		return LoadObject<UMaterialInterface>(nullptr, Path);
	}
}

AEisenwalOcean::AEisenwalOcean()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("OzeanMesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->bUseAsyncCooking = true;
	Mesh->SetMobility(EComponentMobility::Movable);

	// Innen fein, aussen grob bis zum Horizont.
	auto MakeLOD = [](float CellSize, int32 Cells)
	{
		FEisenwalOceanLOD Lod;
		Lod.CellSizeCm = CellSize;
		Lod.CellsPerSide = Cells;
		return Lod;
	};
	LODs.Add(MakeLOD(400.f, 160));     //  640 m
	LODs.Add(MakeLOD(2000.f, 96));     //  1,9 km
	LODs.Add(MakeLOD(10000.f, 64));    //  6,4 km
	LODs.Add(MakeLOD(60000.f, 64));    // 38 km
	LODs.Add(MakeLOD(400000.f, 24));   // 96 km (Horizont)
}

void AEisenwalOcean::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	States.Reset();
	Mesh->ClearAllMeshSections();
	ApplyMaterial();
	UpdateSurface();
}

void AEisenwalOcean::BeginPlay()
{
	Super::BeginPlay();
	ApplyMaterial();
}

void AEisenwalOcean::ApplyMaterial()
{
	ActiveMaterial = OceanMaterialOverride;
	if (!ActiveMaterial)
	{
		ActiveMaterial = UEisenwalSettings::Get()->OceanMaterial.LoadSynchronous();
	}
	if (!ActiveMaterial)
	{
		// Platzhalter, solange M_Ozean noch nicht vom Setup-Skript erzeugt wurde.
		if (UMaterialInterface* Fallback = GetFallbackOceanMaterial())
		{
			UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Fallback, this);
			Mid->SetFlags(RF_Transient);
			Mid->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.01f, 0.05f, 0.08f));
			ActiveMaterial = Mid;
		}
	}
	for (int32 i = 0; i < Mesh->GetNumSections(); ++i)
	{
		Mesh->SetMaterial(i, ActiveMaterial);
	}
}

void AEisenwalOcean::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (!World->IsGameWorld() && !bAnimateInEditor)
	{
		return;
	}
	UpdateSurface();
}

FVector AEisenwalOcean::GetViewLocation() const
{
	const UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			if (PC->PlayerCameraManager)
			{
				return PC->PlayerCameraManager->GetCameraLocation();
			}
		}
	}
	if (World && World->ViewLocationsRenderedLastFrame.Num() > 0)
	{
		return World->ViewLocationsRenderedLastFrame[0];
	}
	return GetActorLocation();
}

void AEisenwalOcean::UpdateSurface()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const UEisenwalOceanSubsystem* Ocean = World->GetSubsystem<UEisenwalOceanSubsystem>();
	if (!Ocean)
	{
		return;
	}

	if (States.Num() != LODs.Num())
	{
		States.Reset();
		States.SetNum(LODs.Num());
		Mesh->ClearAllMeshSections();
	}

	const FVector View = GetViewLocation();
	for (int32 i = 0; i < LODs.Num(); ++i)
	{
		UpdateLOD(i, View, *Ocean);
	}
}

void AEisenwalOcean::UpdateLOD(int32 Index, const FVector& ViewLocation, const UEisenwalOceanSubsystem& Ocean)
{
	const FEisenwalOceanLOD& Lod = LODs[Index];
	FLODState& State = States[Index];

	const int32 Cells = FMath::Max(2, Lod.CellsPerSide & ~1);
	const int32 VertsPerSide = Cells + 1;
	const double Cell = FMath::Max(Lod.CellSizeCm, 10.f);
	const double Snap = Cell * 2.0;

	// Gitter rastet in Doppelzellen ein, damit Wellen beim Fahren nicht "schwimmen".
	const FVector2D Center(FMath::FloorToDouble(ViewLocation.X / Snap) * Snap, FMath::FloorToDouble(ViewLocation.Y / Snap) * Snap);
	const double Half = Cells * Cell * 0.5;
	const FVector2D Start = Center - FVector2D(Half, Half);

	// Loch fuer den naechst-feineren Ring.
	bool bHasHole = false;
	FVector2D HoleMin, HoleMax;
	FIntPoint HoleKey(0, 0);
	if (Index > 0)
	{
		const FLODState& Inner = States[Index - 1];
		const FEisenwalOceanLOD& InnerLod = LODs[Index - 1];
		const double InnerHalf = FMath::Max(2, InnerLod.CellsPerSide & ~1) * InnerLod.CellSizeCm * 0.5;
		HoleMin = Inner.Center - FVector2D(InnerHalf, InnerHalf);
		HoleMax = Inner.Center + FVector2D(InnerHalf, InnerHalf);
		bHasHole = true;
		HoleKey = FIntPoint(
			FMath::RoundToInt32((Inner.Center.X - Center.X) / FMath::Max(InnerLod.CellSizeCm, 1.f)),
			FMath::RoundToInt32((Inner.Center.Y - Center.Y) / FMath::Max(InnerLod.CellSizeCm, 1.f)));
	}

	const int32 NumVerts = VertsPerSide * VertsPerSide;
	State.Vertices.SetNumUninitialized(NumVerts);
	State.Normals.SetNumUninitialized(NumVerts);
	State.UVs.SetNumUninitialized(NumVerts);
	State.Colors.SetNumUninitialized(NumVerts);
	if (State.Tangents.Num() != NumVerts)
	{
		State.Tangents.Init(FProcMeshTangent(1.f, 0.f, 0.f), NumVerts);
	}

	const FVector ActorOrigin = GetActorLocation();
	const float MinWavelength = static_cast<float>(Cell) * MinWavelengthCellRatio;
	const float DepthOffset = LODDepthOffsetCm * Index;

	ParallelFor(VertsPerSide, [&](int32 Y)
	{
		for (int32 X = 0; X < VertsPerSide; ++X)
		{
			const int32 V = Y * VertsPerSide + X;
			const FVector2D Grid = Start + FVector2D(X * Cell, Y * Cell);
			float Foam = 0.f, Crest = 0.f;
			FVector P = Ocean.DisplaceGridPoint(Grid, MinWavelength, Foam, Crest);
			P.Z -= DepthOffset;
			State.Vertices[V] = P - ActorOrigin;
			State.UVs[V] = FVector2D(Grid.X / 1000.0, Grid.Y / 1000.0);
			State.Colors[V] = FColor(
				static_cast<uint8>(FMath::Clamp(Foam, 0.f, 1.f) * 255.f),
				static_cast<uint8>(FMath::Clamp(Crest, 0.f, 1.f) * 255.f),
				static_cast<uint8>(FMath::Clamp(Index / 4.f, 0.f, 1.f) * 255.f),
				255);
		}
	});

	// Normalen aus den fertig verschobenen Nachbarpunkten.
	ParallelFor(VertsPerSide, [&](int32 Y)
	{
		const int32 Y0 = FMath::Max(Y - 1, 0);
		const int32 Y1 = FMath::Min(Y + 1, VertsPerSide - 1);
		for (int32 X = 0; X < VertsPerSide; ++X)
		{
			const int32 X0 = FMath::Max(X - 1, 0);
			const int32 X1 = FMath::Min(X + 1, VertsPerSide - 1);
			const FVector Dx = State.Vertices[Y * VertsPerSide + X1] - State.Vertices[Y * VertsPerSide + X0];
			const FVector Dy = State.Vertices[Y1 * VertsPerSide + X] - State.Vertices[Y0 * VertsPerSide + X];
			FVector N = FVector::CrossProduct(Dx, Dy).GetSafeNormal();
			if (N.Z < 0.f)
			{
				N = -N;
			}
			State.Normals[Y * VertsPerSide + X] = N;
		}
	});

	const bool bTopologyChanged = !State.bSectionCreated || State.HoleKey != HoleKey || State.Triangles.Num() == 0;
	State.Center = Center;

	if (bTopologyChanged)
	{
		State.HoleKey = HoleKey;
		State.Triangles.Reset(Cells * Cells * 6);
		for (int32 Y = 0; Y < Cells; ++Y)
		{
			for (int32 X = 0; X < Cells; ++X)
			{
				if (bHasHole)
				{
					const double X0 = Start.X + X * Cell;
					const double Y0 = Start.Y + Y * Cell;
					if (X0 >= HoleMin.X && X0 + Cell <= HoleMax.X && Y0 >= HoleMin.Y && Y0 + Cell <= HoleMax.Y)
					{
						continue;
					}
				}
				const int32 V00 = Y * VertsPerSide + X;
				const int32 V10 = V00 + 1;
				const int32 V01 = V00 + VertsPerSide;
				const int32 V11 = V01 + 1;
				if (!bFlipWinding)
				{
					State.Triangles.Append({ V00, V01, V10, V10, V01, V11 });
				}
				else
				{
					State.Triangles.Append({ V00, V10, V01, V10, V11, V01 });
				}
			}
		}
		Mesh->CreateMeshSection(Index, State.Vertices, State.Triangles, State.Normals, State.UVs, State.Colors, State.Tangents, false);
		State.bSectionCreated = true;
		if (!ActiveMaterial)
		{
			ApplyMaterial();
		}
		Mesh->SetMaterial(Index, ActiveMaterial);
	}
	else
	{
		Mesh->UpdateMeshSection(Index, State.Vertices, State.Normals, State.UVs, State.Colors, State.Tangents);
	}
}
