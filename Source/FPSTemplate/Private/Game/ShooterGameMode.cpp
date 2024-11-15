#include "Game/ShooterGameMode.h"


DEFINE_LOG_CATEGORY(GameServerLog)

AShooterGameMode::AShooterGameMode()
{
	
}

void AShooterGameMode::BeginPlay()
{
	Super::BeginPlay();

#if WITH_GAMELIFT
	InitGameLift();
#endif
}

void AShooterGameMode::InitServerParameters(FServerParameters& OutServerParameters)
{
	//AuthToken returned from the "aws gamelift get-compute-auth-token" API. Note this will expire and require a new call to the API after 15 minutes.
	if (FParse::Value(FCommandLine::Get(), TEXT("-authtoken="), OutServerParameters.m_authToken))
	{
		UE_LOG(GameServerLog, Log, TEXT("AUTH_TOKEN: %s"), *OutServerParameters.m_authToken)
	}
	//The Host/compute-name of the GameLift Anywhere instance.
	if (FParse::Value(FCommandLine::Get(), TEXT("-hostid="), OutServerParameters.m_hostId))
	{
		UE_LOG(GameServerLog, Log, TEXT("HOST_ID: %s"), *OutServerParameters.m_hostId)
	}
	//The Anywhere Fleet ID.
	if (FParse::Value(FCommandLine::Get(), TEXT("-fleetid="), OutServerParameters.m_fleetId))
	{
		UE_LOG(GameServerLog, Log, TEXT("FLEET_ID: %s"), *OutServerParameters.m_fleetId)
	}
	//The WebSocket URL (GameLiftServiceSdkEndpoint).
	if (FParse::Value(FCommandLine::Get(), TEXT("-websocketurl="), OutServerParameters.m_webSocketUrl))
	{
		UE_LOG(GameServerLog, Log, TEXT("WEBSOCKET_URL: %s"), *OutServerParameters.m_webSocketUrl)
	}
	//The PID of the running process
	OutServerParameters.m_processId = FString::Printf(TEXT("%d"), GetCurrentProcessId());
	UE_LOG(GameServerLog, Log, TEXT("PID: %s"), *OutServerParameters.m_processId);
}

void AShooterGameMode::GetPortFromCommandLine(int32& OutPort)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	FCommandLine::Parse(FCommandLine::Get(), Tokens, Switches);
	for(const FString& Switch : Switches)
	{
		FString Key;
		FString Value;
		if(Switch.Split(FString("="), &Key, &Value, ESearchCase::IgnoreCase))
		{
			if(Key.Equals(FString("port"), ESearchCase::IgnoreCase))
			{
				OutPort = FCString::Atoi(*Value);
				return;
			}
		}
	}
}

void AShooterGameMode::InitGameLift()
{
	UE_LOG(GameServerLog, Log, TEXT("Initializing the GameLift Server"));

	FGameLiftServerSDKModule* GameLiftServerSDKModule = &FModuleManager::LoadModuleChecked<FGameLiftServerSDKModule>(FName("GameLiftServerSDK"));
	//Define the server parameters for a GameLift Anywhere fleet. These are not needed for a GameLift managed EC2 fleet.
	FServerParameters serverParameters;
	
	InitServerParameters(serverParameters);
	
	//InitSDK establishes a local connection with GameLift's agent to enable further communication.
	//Use InitSDK(serverParameters) for a GameLift Anywhere fleet. 
	//Use InitSDK() for a GameLift managed EC2 fleet.
	GameLiftServerSDKModule->InitSDK(serverParameters);

	//Implement callback function onStartGameSession
	//GameLift sends a game session activation request to the game server
	//and passes a game session object with game properties and other settings.
	//Here is where a game server takes action based on the game session object.
	//When the game server is ready to receive incoming player connections, 
	//it invokes the server SDK call ActivateGameSession().
	auto OnGameSession = [=](Aws::GameLift::Server::Model::GameSession InGameSession)->void
	{
		FString gameSessionId = FString(InGameSession.GetGameSessionId());
		UE_LOG(GameServerLog, Log, TEXT("GameSession Initializing: %s"), *gameSessionId);
		GameLiftServerSDKModule->ActivateGameSession();
	};
	ProcessParameters.OnStartGameSession.BindLambda(OnGameSession);
	
	//Implement callback function OnProcessTerminate
	//GameLift invokes this callback before shutting down the instance hosting this game server.
	//It gives the game server a chance to save its state, communicate with services, etc., 
	//and initiate shut down. When the game server is ready to shut down, it invokes the 
	//server SDK call ProcessEnding() to tell GameLift it is shutting down.
	auto onProcessTerminate = [=]() 
	{
		UE_LOG(GameServerLog, Log, TEXT("Game Server Process is terminating"));
		GameLiftServerSDKModule->ProcessEnding();
	};
	ProcessParameters.OnTerminate.BindLambda(onProcessTerminate);

	//Implement callback function OnHealthCheck
	//GameLift invokes this callback approximately every 60 seconds.
	//A game server might want to check the health of dependencies, etc.
	//Then it returns health status true if healthy, false otherwise.
	//The game server must respond within 60 seconds, or GameLift records 'false'.
	//In this example, the game server always reports healthy.
	auto OnHealthCheck = []() 
	{
		UE_LOG(GameServerLog, Log, TEXT("Performing Health Check"));
		return true;
	};
	ProcessParameters.OnHealthCheck.BindLambda(OnHealthCheck);

	// default as 7777
	int32 Port = FURL::UrlConfig.DefaultPort;
	GetPortFromCommandLine(Port);
	
	//The game server gets ready to report that it is ready to host game sessions
	//and that it will listen on port 7777 for incoming player connections.
	ProcessParameters.port = 7777;

	//Here, the game server tells GameLift where to find game session log files.
	//At the end of a game session, GameLift uploads everything in the specified 
	//location and stores it in the cloud for access later.
	TArray<FString> logfiles;
	logfiles.Add(TEXT("GameLift426Test/Saved/Logs/GameLift426Test.log"));
	ProcessParameters.logParameters = logfiles;

	//The game server calls ProcessReady() to tell GameLift it's ready to host game sessions.
	UE_LOG(GameServerLog, Log, TEXT("Calling Process Ready"));

	GameLiftServerSDKModule->ProcessReady(ProcessParameters);
}
