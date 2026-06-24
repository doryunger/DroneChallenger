#include "DroneHUDServer.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Common/TcpSocketBuilder.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Misc/Base64.h"

FDroneHUDServer::FDroneHUDServer(const FString& InServeDir)
	: ServeDir(InServeDir)
{}

FDroneHUDServer::~FDroneHUDServer()
{
	Shutdown();
}

bool FDroneHUDServer::Start(int32 Port)
{
	FIPv4Endpoint Endpoint(FIPv4Address::Any, Port);
	ListenSocket = FTcpSocketBuilder(TEXT("DroneHUDServer"))
		.AsReusable()
		.BoundToEndpoint(Endpoint)
		.Listening(8);

	if (!ListenSocket)
	{
		UE_LOG(LogTemp, Error, TEXT("HUDServer: failed to bind port %d"), Port);
		return false;
	}

	FFileHelper::SaveStringToFile(TEXT(""), *(FPaths::ProjectLogDir() / TEXT("minimap_debug.log")),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	bListening = true;
	bRunning   = true;
	Thread     = FRunnableThread::Create(this, TEXT("DroneHUDServer"), 0, TPri_Normal);
	UE_LOG(LogTemp, Log, TEXT("HUDServer: listening on port %d, serving from %s"), Port, *ServeDir);
	return Thread != nullptr;
}

void FDroneHUDServer::Shutdown()
{
	bRunning   = false;
	bListening = false;
	if (ListenSocket)
		ListenSocket->Close();
	if (Thread)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
	if (ListenSocket)
	{
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenSocket);
		ListenSocket = nullptr;
	}
}

void FDroneHUDServer::SetState(const FHUDState& InState)
{
	FScopeLock Lock(&StateLock);
	State = InState;
}

void FDroneHUDServer::SetBTTreeJson(const FString& Json)
{
	FScopeLock Lock(&BTLock);
	BTTreeJson = Json;
}

void FDroneHUDServer::SetBTHistoryJson(const FString& Json)
{
	FScopeLock Lock(&BTLock);
	BTHistoryJson = Json;
}

uint32 FDroneHUDServer::Run()
{
	constexpr double kSendInterval = 1.0 / 60.0;
	double LastSendSec = FPlatformTime::Seconds();

	while (bRunning)
	{
		bool bPending = false;
		if (ListenSocket->HasPendingConnection(bPending) && bPending)
		{
			FSocket* NewClient = ListenSocket->Accept(TEXT("hud-client"));
			if (NewClient)
			{
				if (HandleClient(NewClient))
				{
					if (WSClient)
					{
						WSClient->Close();
						ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(WSClient);
					}
					WSClient = NewClient;
					WSClient->SetNonBlocking(true);
				}
				else
				{
					NewClient->Close();
					ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(NewClient);
				}
			}
		}

		if (WSClient)
		{
			const double Now = FPlatformTime::Seconds();
			if (Now - LastSendSec >= kSendInterval)
			{
				if (!SendWSTextFrame(WSClient, MakeStateJSON()))
				{
					WSClient->Close();
					ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(WSClient);
					WSClient = nullptr;
				}
				LastSendSec = Now;
			}
		}

		FPlatformProcess::Sleep(0.001f);
	}

	if (WSClient)
	{
		WSClient->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(WSClient);
		WSClient = nullptr;
	}
	return 0;
}

FString FDroneHUDServer::ReadFirstLine(FSocket* Client)
{
	FString Out;
	uint8 Ch = 0;
	int32 Read = 0;
	while (Client->Recv(&Ch, 1, Read) && Read > 0)
	{
		if (Ch == '\n') break;
		if (Ch != '\r') Out.AppendChar((TCHAR)Ch);
	}
	return Out;
}

void FDroneHUDServer::SendBytes(FSocket* Client, const TArray<uint8>& Data)
{
	int32 Sent = 0;
	int32 Offset = 0;
	while (Offset < Data.Num())
	{
		if (!Client->Send(Data.GetData() + Offset, Data.Num() - Offset, Sent)) break;
		Offset += Sent;
	}
}

void FDroneHUDServer::SendText(FSocket* Client, int32 Code,
                               const FString& ContentType, const FString& Body)
{
	FTCHARToUTF8 BodyUtf8(*Body);
	FString Header = FString::Printf(
		TEXT("HTTP/1.0 %d OK\r\nContent-Type: %s\r\nContent-Length: %d\r\n"
		     "Access-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n"),
		Code, *ContentType, BodyUtf8.Length());
	FTCHARToUTF8 HeaderUtf8(*Header);

	TArray<uint8> Out;
	Out.Append((uint8*)HeaderUtf8.Get(), HeaderUtf8.Length());
	Out.Append((uint8*)BodyUtf8.Get(),  BodyUtf8.Length());
	SendBytes(Client, Out);
}

void FDroneHUDServer::SendBinary(FSocket* Client, int32 Code,
                                  const FString& ContentType, const TArray<uint8>& Body)
{
	FString Header = FString::Printf(
		TEXT("HTTP/1.0 %d OK\r\nContent-Type: %s\r\nContent-Length: %d\r\n"
		     "Access-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n"),
		Code, *ContentType, Body.Num());
	FTCHARToUTF8 HeaderUtf8(*Header);

	TArray<uint8> Out;
	Out.Append((uint8*)HeaderUtf8.Get(), HeaderUtf8.Length());
	Out.Append(Body);
	SendBytes(Client, Out);
}

void FDroneHUDServer::Send404(FSocket* Client)
{
	SendText(Client, 404, TEXT("text/plain"), TEXT("Not found"));
}

TMap<FString, FString> FDroneHUDServer::ReadHeaders(FSocket* Client)
{
	TMap<FString, FString> Result;
	FString Line;
	uint8 Ch = 0; int32 R = 0;
	while (Client->Recv(&Ch, 1, R) && R > 0)
	{
		if (Ch == '\r') continue;
		if (Ch == '\n')
		{
			if (Line.IsEmpty()) break;
			int32 Colon;
			if (Line.FindChar(':', Colon))
				Result.Add(Line.Left(Colon).TrimStartAndEnd().ToLower(),
				           Line.RightChop(Colon + 1).TrimStartAndEnd());
			Line.Reset();
		}
		else Line.AppendChar(static_cast<TCHAR>(Ch));
	}
	return Result;
}

FString FDroneHUDServer::MakeStateJSON()
{
	FHUDState S;
	{ FScopeLock Lk(&StateLock); S = State; }
	return FString::Printf(
		TEXT("{\"dx\":%.1f,\"dy\":%.1f,\"dz\":%.1f,\"yaw\":%.4f,"
		     "\"altM\":%.2f,\"tx\":%.1f,\"ty\":%.1f,\"tz\":%.1f,\"inFov\":%s,"
		     "\"fovDeg\":%.1f,\"detRangeCm\":%.0f}"),
		S.DroneX, S.DroneY, S.DroneZ,
		S.DroneYaw, S.AltM,
		S.TargetX, S.TargetY, S.TargetZ,
		S.bTargetInFOV ? TEXT("true") : TEXT("false"),
		S.FovDeg, S.DetRangeCm);
}

bool FDroneHUDServer::HandleWSUpgrade(FSocket* Client, const TMap<FString, FString>& Headers)
{
	const FString* KeyPtr = Headers.Find(TEXT("sec-websocket-key"));
	if (!KeyPtr) return false;

	const FString Combined = *KeyPtr + TEXT("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
	FTCHARToUTF8 CombinedUtf8(*Combined);
	uint8 Hash[20];
	FSHA1::HashBuffer(CombinedUtf8.Get(), static_cast<uint64>(CombinedUtf8.Length()), Hash);
	TArray<uint8> HashArray;
	HashArray.Append(Hash, 20);
	const FString Accept = FBase64::Encode(HashArray);

	const FString Response = FString::Printf(
		TEXT("HTTP/1.1 101 Switching Protocols\r\n"
		     "Upgrade: websocket\r\nConnection: Upgrade\r\n"
		     "Sec-WebSocket-Accept: %s\r\n\r\n"), *Accept);
	FTCHARToUTF8 ResponseUtf8(*Response);
	TArray<uint8> Bytes;
	Bytes.Append(reinterpret_cast<const uint8*>(ResponseUtf8.Get()), ResponseUtf8.Length());
	SendBytes(Client, Bytes);
	return true;
}

bool FDroneHUDServer::SendWSTextFrame(FSocket* Client, const FString& Text)
{
	FTCHARToUTF8 Utf8(*Text);
	const int32 Len = Utf8.Length();

	TArray<uint8> Frame;
	Frame.Add(0x81);
	if (Len < 126)
		Frame.Add(static_cast<uint8>(Len));
	else if (Len < 65536)
	{
		Frame.Add(126);
		Frame.Add(static_cast<uint8>((Len >> 8) & 0xFF));
		Frame.Add(static_cast<uint8>(Len & 0xFF));
	}
	else
	{
		Frame.Add(127);
		for (int32 i = 7; i >= 0; --i)
			Frame.Add(static_cast<uint8>((Len >> (8 * i)) & 0xFF));
	}
	Frame.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Len);

	int32 Sent = 0;
	return Client->Send(Frame.GetData(), Frame.Num(), Sent) && Sent == Frame.Num();
}

bool FDroneHUDServer::HandleClient(FSocket* Client)
{
	const FString Line = ReadFirstLine(Client);
	if (Line.IsEmpty()) return false;

	const TMap<FString, FString> Headers = ReadHeaders(Client);

	TArray<FString> Parts;
	Line.ParseIntoArrayWS(Parts);
	if (Parts.Num() < 2) return false;

	FString Path = Parts[1];
	int32 QIdx = 0;
	if (Path.FindChar('?', QIdx)) Path = Path.Left(QIdx);

	if (Path == TEXT("/ws"))
	{
		const FString* UpgradeVal = Headers.Find(TEXT("upgrade"));
		if (UpgradeVal && UpgradeVal->Equals(TEXT("websocket"), ESearchCase::IgnoreCase))
			return HandleWSUpgrade(Client, Headers);
	}

	if (Path == TEXT("/state"))
	{
		SendText(Client, 200, TEXT("application/json"), MakeStateJSON());
		return false;
	}

	if (Path == TEXT("/tree"))
	{
		FString Json;
		{ FScopeLock Lock(&BTLock); Json = BTTreeJson; }
		if (Json.IsEmpty()) Json = TEXT("{}");
		SendText(Client, 200, TEXT("application/json"), Json);
		return false;
	}

	if (Path == TEXT("/history"))
	{
		FString Json;
		{ FScopeLock Lock(&BTLock); Json = BTHistoryJson; }
		if (Json.IsEmpty()) Json = TEXT("[]");
		SendText(Client, 200, TEXT("application/json"), Json);
		return false;
	}

	if (Path == TEXT("/log"))
	{
		const FString* LenStr = Headers.Find(TEXT("content-length"));
		const int32 BodyLen   = LenStr ? FCString::Atoi(**LenStr) : 0;
		if (BodyLen > 0 && BodyLen < 8192)
		{
			TArray<uint8> Body;
			Body.SetNumZeroed(BodyLen + 1);
			int32 TotalRead = 0;
			while (TotalRead < BodyLen)
			{
				int32 R = 0;
				if (!Client->Recv(Body.GetData() + TotalRead, BodyLen - TotalRead, R) || R <= 0) break;
				TotalRead += R;
			}
			const FString Msg = UTF8_TO_TCHAR(reinterpret_cast<const ANSICHAR*>(Body.GetData()));
			UE_LOG(LogTemp, Log, TEXT("MiniMap JS: %s"), *Msg);
			FFileHelper::SaveStringToFile(Msg + TEXT("\n"), *(FPaths::ProjectLogDir() / TEXT("minimap_debug.log")),
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
				&IFileManager::Get(), FILEWRITE_Append);
		}
		SendText(Client, 204, TEXT("text/plain"), TEXT(""));
		return false;
	}

	FString FileName = (Path == TEXT("/")) ? TEXT("minimap.html") : Path.RightChop(1);
	if (FileName.Contains(TEXT("..")) || FileName.Contains(TEXT(":")))
	{
		Send404(Client); return false;
	}

	const FString FilePath = ServeDir / FileName;
	TArray<uint8> FileBytes;
	if (!FFileHelper::LoadFileToArray(FileBytes, *FilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("HUDServer: 404 %s"), *FilePath);
		Send404(Client); return false;
	}

	FString CT = TEXT("application/octet-stream");
	if (FileName.EndsWith(TEXT(".html"))) CT = TEXT("text/html; charset=utf-8");
	else if (FileName.EndsWith(TEXT(".js"))) CT = TEXT("application/javascript");
	else if (FileName.EndsWith(TEXT(".css"))) CT = TEXT("text/css");

	SendBinary(Client, 200, CT, FileBytes);
	return false;
}
