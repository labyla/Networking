#pragma once

#include <thread>
#include <string>
#include <map>
#include <functional>

#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#ifndef STEAMNETWORKINGSOCKETS_OPENSOURCE
#include <steam/steam_api.h>
#endif

#include "buffer/Buffer.h"

class Server {
public:
	struct ClientInfo {
		HSteamNetConnection Connection;
		std::string ConnectionDescription;
	};

	using DataReceivedCallback = std::function<void(const ClientInfo&, const Buffer&)>;
	using ClientConnectedCallback = std::function<void(const ClientInfo&)>;
	using ClientDisconnectedCallback = std::function<void(const ClientInfo&)>;

	static Server* RunningInstance;

public:

	Server();

	explicit Server(uint16 ServerPort);

	~Server();

	void Start();

	void Stop();

	bool isRunning();

	EResult SendBufferToClient(const ClientInfo& Client, const Buffer& buffer, bool reliable = true);
	void SendBufferToAllClients(const Buffer& buffer, const ClientInfo& ExcludeClient, bool reliable = true);

	template <typename T>
	EResult SendDataToClient(const ClientInfo& Client, const T& Data, bool reliable = true) {
		return SendBufferToClient(Client, Buffer(Data, sizeof(T)), reliable);
	}
	template <typename T>
	void SendDataToAllClients(const T& Data, const ClientInfo& ExcludeClient, bool reliable = true) {
		SendBufferToAllClients(Buffer(Data, sizeof(T)), ExcludeClient, reliable);
	}

	void SetDataReceivedCallback(const DataReceivedCallback& NewCallbackFunction);
	void SetClientConnectedCallback(const ClientConnectedCallback& NewCallbackFunction);
	void SetClientDisconnectedCallback(const ClientDisconnectedCallback& NewCallbackFunction);

	std::map<HSteamNetConnection, ClientInfo> GetConnectedClients();

private:
	/////////////////////////////////////////////////////////////
	///						 VARIABLES						 ////
	/////////////////////////////////////////////////////////////

	uint16 nPort;
	std::thread m_NetworkThread;
	bool m_Running = false;

	HSteamListenSocket m_ListenSocket;
	HSteamNetPollGroup m_PollGroup;
	ISteamNetworkingSockets* m_pInterface;

	std::map<HSteamNetConnection, ClientInfo> m_ConnectedClients;

	DataReceivedCallback m_DataReceivedCallback;
	ClientConnectedCallback m_ClientConnectedCallback;
	ClientDisconnectedCallback m_ClientDisconnectedCallback;


	/////////////////////////////////////////////////////////////
	///						 FUNCTIONS						 ////
	/////////////////////////////////////////////////////////////

	void NetworkThreadFunction();

	static void ConnectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t* pInfo);

	void OnConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo);

	void PollIncomingMessages();

	void PollConnectionStateChanges();

	void FatalError(const std::string& message);
};