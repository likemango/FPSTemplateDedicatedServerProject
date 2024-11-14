#pragma once

#include "CoreMinimal.h"
#include "GameLiftServerSDK.h"
#include "ShooterGameModeBase.h"
#include "ShooterGameMode.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(GameServerLog, Log, All)

/**
 * 
 */
UCLASS()
class FPSTEMPLATE_API AShooterGameMode : public AShooterGameModeBase
{
	GENERATED_BODY()

public:
	AShooterGameMode();

protected:
	virtual void BeginPlay() override;

private:
	// Process Parameters needs to remain in scope for the lifetime of the app
	FProcessParameters ProcessParameters;

	void InitGameLift();
	void InitServerParameters(FServerParameters& OutServerParameters);
};
