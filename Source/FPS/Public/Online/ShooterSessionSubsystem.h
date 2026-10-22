#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Loadout/LoadoutTypes.h"
#include "ShooterSessionSubsystem.generated.h"

class FOnlineSessionSearch;

// 房间列表里的一项
USTRUCT(BlueprintType)
struct FRoomBrowserEntry
{
	GENERATED_BODY()

	// 搜索结果下标（JoinRoom 用）
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Session") int32 ResultIndex = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Session") FString RoomCode;
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Session") EShooterGameMode Mode = EShooterGameMode::TDM;
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Session") int32 Players = 0;
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Session") int32 MaxPlayers = 0;
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Session") int32 PingMs = 0;
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Session") FString HostName;
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Session") bool bInGame = false;
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Session") bool bDedicated = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRoomsFound, bool, bSuccess, const TArray<FRoomBrowserEntry>&, Rooms);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoomJoinFailed, const FText&, Reason);

// 房间会话（与在线服务无关的封装）。现在走 OnlineSubsystem Null（局域网），
// 以后换 Steam/EOS：改 ini 的 DefaultPlatformService + 插件，这里的接口不变。
// 服务器（listen 或专用）在大厅 GameMode 里 AdvertiseRoom；客户端 FindRooms / JoinRoom / JoinByCode。
UCLASS()
class FPS_API UShooterSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ---- 服务器 ----
	// 发布/更新房间信息（没有会话就创建）
	void AdvertiseRoom(const FString& RoomCode, EShooterGameMode Mode, int32 Players, int32 MaxPlayers, const FString& HostName, bool bInGame);

	UFUNCTION(BlueprintCallable, Category = "FPS|Session")
	void DestroyRoomSession();

	// ---- 客户端 ----
	UFUNCTION(BlueprintCallable, Category = "FPS|Session")
	void FindRooms(EShooterGameMode Mode, bool bAnyMode = false);

	UFUNCTION(BlueprintCallable, Category = "FPS|Session")
	bool JoinRoom(int32 ResultIndex);

	// 按房间号找并加入
	UFUNCTION(BlueprintCallable, Category = "FPS|Session")
	void JoinByCode(const FString& RoomCode);

	UFUNCTION(BlueprintPure, Category = "FPS|Session")
	bool IsSearching() const { return bSearching; }

	UFUNCTION(BlueprintPure, Category = "FPS|Session")
	const TArray<FRoomBrowserEntry>& GetLastRooms() const { return LastRooms; }

	UFUNCTION(BlueprintPure, Category = "FPS|Session")
	FString GetOnlineServiceName() const;

	UPROPERTY(BlueprintAssignable, Category = "FPS|Session")
	FOnRoomsFound OnRoomsFound;

	UPROPERTY(BlueprintAssignable, Category = "FPS|Session")
	FOnRoomJoinFailed OnJoinFailed;

	static const FName KeyRoomCode;
	static const FName KeyMode;
	static const FName KeyPlayers;
	static const FName KeyHost;
	static const FName KeyInGame;

protected:
	virtual void Deinitialize() override;

private:
	IOnlineSessionPtr GetSessions() const;
	bool IsLAN() const;
	void FillSettings(class FOnlineSessionSettings& S, const FString& RoomCode, EShooterGameMode Mode, int32 Players, int32 MaxPlayers, const FString& HostName, bool bInGame) const;

	void HandleCreateComplete(FName SessionName, bool bOk);
	void HandleFindComplete(bool bOk);
	void HandleJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroyComplete(FName SessionName, bool bOk);
	void DoJoin(int32 ResultIndex);
	void FailJoin(const FText& Reason);

	TSharedPtr<FOnlineSessionSearch> Search;
	TArray<FRoomBrowserEntry> LastRooms;
	EShooterGameMode SearchMode = EShooterGameMode::TDM;
	bool bSearchAnyMode = false;
	bool bSearching = false;
	bool bCreating = false;
	FString PendingJoinCode;
	int32 PendingJoinIndex = INDEX_NONE;

	FDelegateHandle CreateHandle, FindHandle, JoinHandle, DestroyHandle;
};
