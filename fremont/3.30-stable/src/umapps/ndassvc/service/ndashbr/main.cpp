#include "stdafx.h"
#include "ndasdevhb.h"
#include "lpxcomm.h"

#define XDBG_MAIN_MODULE
#include "xdebug.h"
#include <iostream>
#include <iomanip>

using namespace ximeta;

static std::ostream& operator << (std::ostream& os, LPX_ADDRESS& addr)
{
	os << std::setbase(16) << 
		std::setw(2) << std::setfill('0') << (WORD) addr.Node[0] << ":" << 
		std::setw(2) << std::setfill('0') << (WORD) addr.Node[1] << ":" <<
		std::setw(2) << std::setfill('0') << (WORD) addr.Node[2] << ":" <<
		std::setw(2) << std::setfill('0') << (WORD) addr.Node[3] << ":" <<
		std::setw(2) << std::setfill('0') << (WORD) addr.Node[4] << ":" <<
		std::setw(2) << std::setfill('0') << (WORD) addr.Node[5];
	return os;
}

class MyObserver : public CObserver
{
	PCNdasDeviceHeartbeatListener m_pListener;

public:
	MyObserver(PCNdasDeviceHeartbeatListener pListener) :
		m_pListener(pListener)
	{
	}

	virtual ~MyObserver()
	{
	}

	virtual void Update(PCSubject pChangedSubject)
	{
		if (pChangedSubject == m_pListener) {
			NDAS_DEVICE_HEARTBEAT_DATA data;
			m_pListener->GetHeartbeatData(&data);

			std::cout << "Device : " << data.RemoteAddress <<
				" at " << data.LocalAddress << 
				" Type: " << (INT) data.ucType <<
				" Version: " << (INT) data.ucVersion <<
				std::endl;
		}	
	}
	
};

int __cdecl wmain()
{
	WSADATA wsaData;

	std::cout << "Initializing WSA..." << std::endl;

	INT iError = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (0 != iError) {
		std::cerr << "Socket initialization failed - Error " << iError << std::endl;
		return -1;
	}

	std::cout << "Decl..." << std::endl;
	PCNdasDeviceHeartbeatListener listener;

	std::cout << "Inst..." << std::endl;
	listener = new CNdasDeviceHeartbeatListener();

	std::cout << "Initializing Listener..." << std::endl;

	BOOL success = listener->Initialize();

	if (!success) {
		std::cerr << "Initialization failed - Error " << GetLastError() << std::endl;
		return -1;
	}

	std::cout << "Initializing Listener..." << std::endl;
	success = listener->Run();
	if (!success) {
		std::cerr << "Failed to run the listener - Error " << GetLastError() << std::endl;
		return -1;
	}

	MyObserver o(listener);
	listener->Attach(&o);

	std::cout << "Press Enter to stop..." << std::endl;

	CHAR buffer[255];
	std::cin.getline(buffer, 256);

	listener->Stop();

	delete listener;

	WSACleanup();

	return 0;
}
