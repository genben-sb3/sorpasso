#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GTOGameMode.generated.h"

/** Puts the player straight into the car, no menu. */
UCLASS()
class GTO_API AGTOGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGTOGameMode();
};
