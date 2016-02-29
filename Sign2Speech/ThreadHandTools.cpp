#include "ThreadHandTools.h"

// WebSocket
//#ifdef _WIN32
//#pragma comment( lib, "ws2_32" )
//#include <WinSock2.h>
//#endif
//#include <assert.h>
//#include <stdio.h>
//#include <string>

WebSocket::pointer ThreadHandTools::webSock = NULL;

ThreadHandTools::ThreadHandTools(mutex* mP, mutex *mBR, mutex *mBW, bool* pg, vector<long>* bR, vector<vector<pair<string, long>>>* bW, int ac, const char* av[]) : ThreadApp(mP, mBR, mBW, pg, bR, bW) {
	argv = av;
	argc = ac;
}

void ThreadHandTools::handle_message(const std::string & message){
	printf(">>> %s\n", message.c_str());
	if (message == "world") {
		ThreadHandTools::webSock->close();
	}
}

bool ThreadHandTools::CtrlHandler(DWORD fdwCtrlType, bool g_stop, ConsoleTools *ct ) 
{
	switch (fdwCtrlType)
	{
		// Handle the CTRL-C signal. 
	case CTRL_C_EVENT:

		// confirm that the user wants to exit. 
	case CTRL_CLOSE_EVENT:
		g_stop = true;
		Sleep(1000);
		ct->releaseAll();
		//*(program_on) = false;
		return(TRUE);

	default:
		return FALSE;
	}
}

void ThreadHandTools::run() {

	ConsoleTools ct;
	mProgram_on->lock();

	#ifdef _WIN32
		INT rc;
		WSADATA wsaData;

		rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (rc) {
			printf("WSAStartup Failed.\n");
			return;
		}
	#endif

	ThreadHandTools::webSock = WebSocket::from_url("ws://localhost:9000/ws/subtitle");
	assert(ThreadHandTools::webSock);

	while (*program_on) {

		mProgram_on->unlock();

		HandTools h;

		if (argc < 2)
		{
			Definitions::WriteHelpMessage();
			return;
		}

		// Setup
		ct.setSession(PXCSession::CreateInstance());
		if (!(ct.getSession()))
		{
			std::printf("Failed Creating PXCSession\n");
			return;
		}

		ct.setSenseManager(ct.getSession()->CreateSenseManager());
		if (!(ct.getSenseManager()))
		{
			ct.releaseAll(); //TODO : Fonction dans HandTools.cpp . Attention, il faut lui passer des paramètres maintenant. A discuter
			std::printf("Failed Creating PXCSenseManager\n");
			return;
		}

		if ((ct.getSenseManager())->EnableHand() != PXC_STATUS_NO_ERROR)
		{
			ct.releaseAll();
			std::printf("Failed Enabling Hand Module\n");
			return;
		}

		g_handModule = (ct.getSenseManager())->QueryHand();
		if (!g_handModule)
		{
			ct.releaseAll();
			std::printf("Failed Creating PXCHandModule\n");
			return;
		}

		g_handDataOutput = g_handModule->CreateOutput();
		if (!g_handDataOutput)
		{
			ct.releaseAll();
			std::printf("Failed Creating PXCHandData\n");
			return;
		}

		g_handConfiguration = g_handModule->CreateActiveConfiguration();
		if (!g_handConfiguration)
		{
			ct.releaseAll();
			std::printf("Failed Creating PXCHandConfiguration\n");
			return;
		}

		// Iterating input parameters
		for (int i = 1; i < argc; i++)
		{
			if (strcmp(argv[i], "-skeleton") == 0)
			{
				std::printf("-Skeleton Information Enabled-\n");
				g_skeleton = true;
			}
		}

		g_handConfiguration->EnableStabilizer(true);
		g_handConfiguration->SetTrackingMode(PXCHandData::TRACKING_MODE_FULL_HAND);

		// Apply configuration setup
		g_handConfiguration->ApplyChanges();


		if (g_handConfiguration)
		{
			g_handConfiguration->Release();
			g_handConfiguration = NULL;
		}

		pxcI32 numOfHands = 0;

		// First Initializing the sense manager
		if ((ct.getSenseManager())->Init() == PXC_STATUS_NO_ERROR)
		{
			std::printf("\nPXCSenseManager Initializing OK\n========================\n");

			mProgram_on->lock();
			// Acquiring frames from input device
			while ((ct.getSenseManager())->AcquireFrame(true) == PXC_STATUS_NO_ERROR && (*program_on))
			{
				mProgram_on->unlock();
				// Get current hand outputs
				if (g_handDataOutput->Update() == PXC_STATUS_NO_ERROR)
				{

					// Display joints
					if (g_skeleton)
					{
						PXCHandData::IHand *hand;
						for (int i = 0; i < g_handDataOutput->QueryNumberOfHands(); ++i)
						{
							g_handDataOutput->QueryHandData(PXCHandData::ACCESS_ORDER_BY_TIME, i, hand);
							std::string handSide = "Unknown Hand";
							handSide = hand->QueryBodySide() == PXCHandData::BODY_SIDE_LEFT ? "Left Hand" : "Right Hand";
							//std::printf("%s\n==============\n", handSide.c_str());
							//printFold(hand);
							h.analyseGesture(hand); //TODO : Fonction dans HandTools
						}
					}

				} // end if update
				(ct.getSenseManager())->ReleaseFrame();
				mProgram_on->lock();
			} // end while acquire frame
			mProgram_on->unlock();
		} // end if Init

		else
		{
			ct.releaseAll();
			std::printf("Failed Initializing PXCSenseManager\n");
			return;
		}

		ct.releaseAll();

		delete ThreadHandTools::webSock;

		#ifdef _WIN32
			WSACleanup();
		#endif

		mProgram_on->lock();
	}
	mProgram_on->unlock();
}