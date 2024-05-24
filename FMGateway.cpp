/*
*   Copyright (C) 2016,2017,2018,2020,2021,2024 by Jonathan Naylor G4KLX
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "MQTTConnection.h"
#include "USRPNetwork.h"
#include "RAWNetwork.h"
#include "IAXNetwork.h"
#include "FMNetwork.h"
#include "UDPSocket.h"
#include "FMGateway.h"
#include "StopWatch.h"
#include "Network.h"
#include "Version.h"
#include "Thread.h"
#include "Timer.h"
#include "Utils.h"
#include "Conf.h"
#include "Log.h"
#include "GitVersion.h"

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <pwd.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
const char* DEFAULT_INI_FILE = "FMGateway.ini";
#else
const char* DEFAULT_INI_FILE = "/etc/FMGateway.ini";
#endif

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <ctime>
#include <cstring>

// In Log.cpp
extern CMQTTConnection* m_mqtt;

const unsigned int BUFFER_LENGTH = 500U;

static bool m_killed = false;
static int  m_signal = 0;

#if !defined(_WIN32) && !defined(_WIN64)
static void sigHandler(int signum)
{
	m_killed = true;
	m_signal = signum;
}
#endif

int main(int argc, char** argv)
{
	const char* iniFile = DEFAULT_INI_FILE;
	if (argc > 1) {
		for (int currentArg = 1; currentArg < argc; ++currentArg) {
			std::string arg = argv[currentArg];
			if ((arg == "-v") || (arg == "--version")) {
				::fprintf(stdout, "FMGateway version %s git #%.7s\n", VERSION, gitversion);
				return 0;
			} else if (arg.substr(0, 1) == "-") {
				::fprintf(stderr, "Usage: FMGateway [-v|--version] [filename]\n");
				return 1;
			} else {
				iniFile = argv[currentArg];
			}
		}
	}

#if !defined(_WIN32) && !defined(_WIN64)
	::signal(SIGINT,  sigHandler);
	::signal(SIGTERM, sigHandler);
	::signal(SIGHUP,  sigHandler);
#endif

	int ret = 0;

	do {
		m_signal = 0;

		CFMGateway* gateway = new CFMGateway(std::string(iniFile));
		ret = gateway->run();

		delete gateway;

		if (m_signal == 2)
			::LogInfo("FMGateway-%s exited on receipt of SIGINT", VERSION);

		if (m_signal == 15)
			::LogInfo("FMGateway-%s exited on receipt of SIGTERM", VERSION);

		if (m_signal == 1)
			::LogInfo("FMGateway-%s restarted on receipt of SIGHUP", VERSION);

	} while (m_signal == 1);

	::LogFinalise();

	return ret;
}

CFMGateway::CFMGateway(const std::string& file) :
m_file(file)
{
	CUDPSocket::startup();
}

CFMGateway::~CFMGateway()
{
	CUDPSocket::shutdown();
}

int CFMGateway::run()
{
	CConf conf(m_file);
	bool ret = conf.read();
	if (!ret) {
		::fprintf(stderr, "FMGateway: cannot read the .ini file\n");
		return 1;
	}

#if !defined(_WIN32) && !defined(_WIN64)
	bool m_daemon = conf.getDaemon();
	if (m_daemon) {
		// Create new process
		pid_t pid = ::fork();
		if (pid == -1) {
			::fprintf(stderr, "Couldn't fork() , exiting\n");
			return -1;
		} else if (pid != 0) {
			exit(EXIT_SUCCESS);
		}

		// Create new session and process group
		if (::setsid() == -1) {
			::fprintf(stderr, "Couldn't setsid(), exiting\n");
			return -1;
		}

		// Set the working directory to the root directory
		if (::chdir("/") == -1) {
			::fprintf(stderr, "Couldn't cd /, exiting\n");
			return -1;
		}

		// If we are currently root...
		if (getuid() == 0) {
			struct passwd* user = ::getpwnam("mmdvm");
			if (user == nullptr) {
				::fprintf(stderr, "Could not get the mmdvm user, exiting\n");
				return -1;
			}

			uid_t mmdvm_uid = user->pw_uid;
			gid_t mmdvm_gid = user->pw_gid;

			// Set user and group ID's to mmdvm:mmdvm
			if (setgid(mmdvm_gid) != 0) {
				::fprintf(stderr, "Could not set mmdvm GID, exiting\n");
				return -1;
			}

			if (setuid(mmdvm_uid) != 0) {
				::fprintf(stderr, "Could not set mmdvm UID, exiting\n");
				return -1;
			}

			// Double check it worked (AKA Paranoia)
			if (setuid(0) != -1) {
				::fprintf(stderr, "It's possible to regain root - something is wrong!, exiting\n");
				return -1;
			}
		}
	}
#endif

	::LogInitialise(conf.getLogDisplayLevel(), conf.getLogMQTTLevel());

	std::vector<std::pair<std::string, void (*)(const unsigned char*, unsigned int)>> subscriptions;

	m_mqtt = new CMQTTConnection(conf.getMQTTAddress(), conf.getMQTTPort(), conf.getMQTTName(), subscriptions, conf.getMQTTKeepalive());
	ret = m_mqtt->open();
	if (!ret)
		return 1; 

#if !defined(_WIN32) && !defined(_WIN64)
	if (m_daemon) {
		::close(STDIN_FILENO);
		::close(STDOUT_FILENO);
		::close(STDERR_FILENO);
	}
#endif

	CFMNetwork localNetwork(conf.getNetworkLocalAddress(), conf.getNetworkLocalPort(), conf.getNetworkRptAddress(), conf.getNetworkRptPort(), conf.getNetworkDebug());
	ret = localNetwork.open();
	if (!ret)
		return -1;

	INetwork* network = nullptr;
	if (conf.getProtocol() == "USRP") {
		network = new CUSRPNetwork(conf.getUSRPLocalAddress(), conf.getUSRPLocalPort(), conf.getUSRPRemoteAddress(), conf.getUSRPRemotePort(), conf.getUSRPDebug());
	} else if (conf.getProtocol() == "RAW") {
		network = new CRAWNetwork(conf.getRAWLocalAddress(), conf.getRAWLocalPort(), conf.getRAWRemoteAddress(), conf.getRAWRemotePort(), conf.getRAWSampleRate(), conf.getRAWSquelchFile(), conf.getRAWDebug());
	} else if (conf.getProtocol() == "IAX") {
		network = new CIAXNetwork(conf.getCallsign(), conf.getIAXUsername(), conf.getIAXPassword(), conf.getIAXNode(), conf.getIAXLocalAddress(), conf.getIAXLocalPort(), conf.getIAXRemoteAddress(), conf.getIAXRemotePort(), conf.getIAXDebug());
	} else {
		LogError("Invalid FM network protocol specified - %s", conf.getProtocol().c_str());
		return false;
	}

	ret = network->open();
	if (!ret)
		return -1;

	CStopWatch stopWatch;
	stopWatch.start();

	LogMessage("FMGateway-%s is starting", VERSION);
	LogMessage("Built %s %s (GitID #%.7s)", __TIME__, __DATE__, gitversion);

	while (!m_killed) {
		float buffer[BUFFER_LENGTH];

		NETWORK_TYPE type = localNetwork.readType();

		switch (type) {
		case NT_START: {
				std::string callsign = localNetwork.readStart();
				network->writeStart(callsign);
			}
			break;

		case NT_DATA: {
				unsigned int n = localNetwork.readData(buffer, BUFFER_LENGTH);
				network->writeData(buffer, n);
			}
			break;

		case NT_END: {
				localNetwork.readEnd();
				network->writeEnd();
			}
			break;

		default:
			break;
		}

		unsigned int n = network->readData(buffer, BUFFER_LENGTH);
		if (n > 0U)
			localNetwork.writeData(buffer, n);

		unsigned int ms = stopWatch.elapsed();
		stopWatch.start();

		localNetwork.clock(ms);

		network->clock(ms);

		if (ms < 10U)
			CThread::sleep(10U);
	}

	LogInfo("FMGateway is stopping");

	localNetwork.close();

	network->close();
	delete network;

	return 0;
}
