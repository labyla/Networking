#include <iostream>
#include <chrono>
#include "fmt/format.h"

#include "Client.h"

Client* Client::RunningInstance = nullptr;

/////////////////////////////////////////////////////////////
///					PUBLIC FUNCTIONS				     ////
/////////////////////////////////////////////////////////////

Client::~Client() {
	if (m_NetworkThread.joinable())
		m_NetworkThread.join();
}

void Client::ConnectToServer(const std::string& ServerAddress) {
	if (m_Running)
		return;

	if (m_NetworkThread.joinable())
		m_NetworkThread.join();

	m_ServerAddress = ServerAddress;
	m_NetworkThread = std::thread([this]() { NetworkThreadFunction(); });
}

void Client::Disconnect() {
	m_Running = false;

	if (m_NetworkThread.joinable())
		m_NetworkThread.join();
}

void Client::SetDataReceivedCallback(const DataReceivedCallback& NewCallbackFunction) {
	m_DataReceivedCallback = NewCallbackFunction;
}

void Client::SetServerConnectedCallback(const ServerConnectedCallback& NewCallbackFunction) {
	m_ServerConnectedCallback = NewCallbackFunction;
}

void Client::SetServerDisconnectedCallback(const ServerDisconnectedCallback& NewCallbackFunction) {
	m_ServerDisconnectedCallback = NewCallbackFunction;
}

bool Client::isRunning() {
	return m_Running;
}

Client::ConnectionStatus Client::GetConnectionStatus() {
	return m_ConnectionStatus;
}

const std::string& Client::GetConnectionDebugMessage() {
	return m_ConnectionDebugMessage;
}

/////////////////////////////////////////////////////////////
///					 PRIVATE FUNCTIONS				     ////
/////////////////////////////////////////////////////////////

void Client::NetworkThreadFunction() {
	RunningInstance = this;
	m_Running = true;
	m_ConnectionStatus = ConnectionStatus::Connecting;

	SteamDatagramErrMsg errMsg;
	if (!GameNetworkingSockets_Init(nullptr, errMsg))
	{
		FatalError(fmt::format("GameNetworkingSockets_Init failed: {}", errMsg));
		m_ConnectionStatus = ConnectionStatus::FailedToConnect;
		m_ConnectionDebugMessage = fmt::format("GameNetworkingSockets_Init failed: {}", errMsg);
		return;
	}

	m_pInterface = SteamNetworkingSockets();

	SteamNetworkingIPAddr address;
	if (!address.ParseString(m_ServerAddress.c_str()))
	{
		FatalError(fmt::format("Invalid IP address - could not parse {}", m_ServerAddress));
		m_ConnectionStatus = ConnectionStatus::FailedToConnect;
		m_ConnectionDebugMessage = fmt::format("Invalid IP address - could not parse {}", m_ServerAddress);
		return;
	}

	SteamNetworkingConfigValue_t options;
	options.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)ConnectionStatusChangedCallback);
	m_Connection = m_pInterface->ConnectByIPAddress(address, 1, &options);
	if (m_Connection == k_HSteamNetConnection_Invalid)
	{
		FatalError("Failed to create connection");
		m_ConnectionStatus = ConnectionStatus::FailedToConnect;
		m_ConnectionDebugMessage = "Failed to create connection";
		return;
	}


	while (m_Running) {
		PollIncomingMessages();
		PollConnectionStateChanges();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	m_pInterface->CloseConnection(m_Connection, 0, nullptr, false);
	m_ConnectionStatus = ConnectionStatus::Disconnected;
}

void Client::ConnectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t* pInfo) {
	RunningInstance->OnConnectionStatusChanged(pInfo);
}

void Client::OnConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo) {
	switch (pInfo->m_info.m_eState)
	{
		case k_ESteamNetworkingConnectionState_None:
			break;

		case k_ESteamNetworkingConnectionState_ClosedByPeer:
		case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
		{
			m_Running = false;
			m_ConnectionStatus = ConnectionStatus::FailedToConnect;
			m_ConnectionDebugMessage = pInfo->m_info.m_szEndDebug;

			if (pInfo->m_eOldState == k_ESteamNetworkingConnectionState_Connecting) {
				std::cout << "Couldn't connect to the server: " << pInfo->m_info.m_szEndDebug << std::endl;
			}
			else if (pInfo->m_eOldState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally) {
				std::cout << "Lost Connection with the server: " << pInfo->m_info.m_szEndDebug << std::endl;
			}
			else {
				std::cout << "Disconnected from the server: " << pInfo->m_info.m_szEndDebug << std::endl;
			}

			m_Connection = k_HSteamNetConnection_Invalid;
			m_pInterface->CloseConnection(m_Connection, 0, nullptr, false);
			m_ConnectionStatus = ConnectionStatus::Disconnected;

			if ((bool)m_ServerDisconnectedCallback)
				m_ServerDisconnectedCallback();

			break;
		}

		case k_ESteamNetworkingConnectionState_Connecting:
			break;

		case k_ESteamNetworkingConnectionState_Connected:
		{
			m_ConnectionStatus = ConnectionStatus::Connected;
			std::cout << "Connected to the server" << std::endl;

			if ((bool)m_ServerConnectedCallback)
				m_ServerConnectedCallback();
		}

		default:
			break;
	}
}

void Client::PollIncomingMessages() {
	while (m_Running) {
		ISteamNetworkingMessage* pIncomingMessage = nullptr;
		int numMsgs = m_pInterface->ReceiveMessagesOnConnection(m_Connection, &pIncomingMessage, 1);

		if (numMsgs == 0)
			return;

		if (numMsgs < 0) {
			m_Running = false;
			return;
		}

		if ((bool)m_DataReceivedCallback)
			m_DataReceivedCallback(Buffer(pIncomingMessage->m_pData, pIncomingMessage->m_cbSize));

		pIncomingMessage->Release();
	}
}

void Client::PollConnectionStateChanges() {
	m_pInterface->RunCallbacks();
}

void Client::FatalError(const std::string& message) {
	std::cout << message << std::endl;
	m_Running = false;
}