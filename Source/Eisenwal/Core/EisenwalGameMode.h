#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "EisenwalGameMode.generated.h"

class AEisenwalSky;
class AEisenwalOcean;

/**
 * Haupt-Spielmodus. Sorgt dafuer, dass Ozean und Himmel existieren (auch in einem leeren Level)
 * und startet die Welt-Systeme.
 */
UCLASS()
class EISENWAL_API AEisenwalGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AEisenwalGameMode();

	virtual void StartPlay() override;

	UFUNCTION(BlueprintPure, Category = "Eisenwal")
	AEisenwalSky* GetSky() const { return Sky; }

	UFUNCTION(BlueprintPure, Category = "Eisenwal")
	AEisenwalOcean* GetOcean() const { return Ocean; }

	/** Start-Beaufort beim Spielstart. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eisenwal")
	float StartBeaufort = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eisenwal")
	float StartWindDirection = 30.f;

protected:
	void EnsureWorldActors();

	UPROPERTY(Transient)
	TObjectPtr<AEisenwalSky> Sky;

	UPROPERTY(Transient)
	TObjectPtr<AEisenwalOcean> Ocean;
};
