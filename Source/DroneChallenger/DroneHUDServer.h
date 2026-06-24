#pragma once
#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/CriticalSection.h"

struct FHUDState
{
	float DroneX = 0.f, DroneY = 0.f, DroneZ = 0.f;
	float DroneYaw = 0.f;
	float AltM  = 0.f;
	float TargetX = 0.f, TargetY = 0.f, TargetZ = 0.f;
	bool  bTargetInFOV = false;
	float FovDeg     = 90.f;
	float DetRangeCm = 20000.f;
};

class FDroneHUDServer : public FRunnable
{
public:
	explicit FDroneHUDServer(const FString& InServeDir);
	~FDroneHUDServer() override;

	bool Start(int32 Port = 8081);
	void Shutdown();
	void SetState(const FHUDState& InState);
	void SetBTTreeJson(const FString& Json);
	void SetBTHistoryJson(const FString& Json);
	[[nodiscard]] bool IsRunning() const { return bListening; }

private:
	virtual uint32 Run() override;

	bool           HandleClient(class FSocket* Client);
	static FString ReadFirstLine(class FSocket* Client);
	static TMap<FString, FString> ReadHeaders(class FSocket* Client);
	bool           HandleWSUpgrade(class FSocket* Client, const TMap<FString, FString>& Headers);
	bool           SendWSTextFrame(class FSocket* Client, const FString& Text);
	FString        MakeStateJSON();
	void           SendBytes(class FSocket* Client, const TArray<uint8>& Data);
	void           SendText(class FSocket* Client, int32 Code,
	                        const FString& ContentType, const FString& Body);
	void           SendBinary(class FSocket* Client, int32 Code,
	                          const FString& ContentType, const TArray<uint8>& Body);
	void           Send404(class FSocket* Client);

	FString          ServeDir;
	class FSocket*   ListenSocket = nullptr;
	class FSocket*   WSClient     = nullptr;
	FRunnableThread* Thread       = nullptr;
	bool             bRunning     = false;
	bool             bListening   = false;

	FCriticalSection StateLock;
	FHUDState        State;

	FCriticalSection BTLock;
	FString          BTTreeJson;
	FString          BTHistoryJson;
};
