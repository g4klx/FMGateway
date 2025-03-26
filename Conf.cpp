/*
 *   Copyright (C) 2015,2016,2017,2018,2020,2021,2024,2025 by Jonathan Naylor G4KLX
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

#include "Conf.h"
#include "Log.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

const int BUFFER_SIZE = 500;

enum class SECTION {
	NONE,
	GENERAL,
	LOG,
	MQTT,
	NETWORK,
	USRP_NETWORK,
	RAW_NETWORK,
	IAX_NETWORK
};

CConf::CConf(const std::string& file) :
m_file(file),
m_callsign(),
m_protocol("USRP"),
m_debug(false),
m_daemon(false),
m_logDisplayLevel(0U),
m_logMQTTLevel(0U),
m_mqttAddress("127.0.0.1"),
m_mqttPort(1883U),
m_mqttKeepalive(60U),
m_mqttName("fm-gateway"),
m_mqttAuthEnabled(false),
m_mqttUsername(),
m_mqttPassword(),
m_networkLocalAddress("127.0.0.1"),
m_networkLocalPort(0U),
m_networkRptAddress("127.0.0.1"),
m_networkRptPort(0U),
m_networkDebug(false),
m_usrpLocalAddress("127.0.0.1"),
m_usrpLocalPort(0U),
m_usrpRemoteAddress("127.0.0.1"),
m_usrpRemotePort(0U),
m_usrpDebug(false),
m_rawLocalAddress("127.0.0.1"),
m_rawLocalPort(0U),
m_rawRemoteAddress("127.0.0.1"),
m_rawRemotePort(0U),
m_rawSampleRate(8000U),
m_rawSquelchFile(),
m_rawDebug(false),
m_iaxLocalAddress("127.0.0.1"),
m_iaxLocalPort(0U),
m_iaxRemoteAddress("127.0.0.1"),
m_iaxRemotePort(0U),
m_iaxUsername(),
m_iaxPassword(),
m_iaxNode(),
m_iaxDebug(false)
{
}

CConf::~CConf()
{
}

bool CConf::read()
{
	FILE* fp = ::fopen(m_file.c_str(), "rt");
	if (fp == nullptr) {
		::fprintf(stderr, "Couldn't open the .ini file - %s\n", m_file.c_str());
		return false;
	}

	SECTION section = SECTION::NONE;

	char buffer[BUFFER_SIZE];
	while (::fgets(buffer, BUFFER_SIZE, fp) != nullptr) {
		if (buffer[0U] == '#')
			continue;

		if (buffer[0U] == '[') {
			if (::strncmp(buffer, "[General]", 9U) == 0)
				section = SECTION::GENERAL;
			else if (::strncmp(buffer, "[Log]", 5U) == 0)
				section = SECTION::LOG;
			else if (::strncmp(buffer, "[MQTT]", 6U) == 0)
				section = SECTION::MQTT;
			else if (::strncmp(buffer, "[Network]", 9U) == 0)
				section = SECTION::NETWORK;
			else if (::strncmp(buffer, "[USRP Network]", 14U) == 0)
				section = SECTION::USRP_NETWORK;
			else if (::strncmp(buffer, "[RAW Network]", 13U) == 0)
				section = SECTION::RAW_NETWORK;
			else if (::strncmp(buffer, "[IAX Network]", 13U) == 0)
				section = SECTION::IAX_NETWORK;
			else
				section = SECTION::NONE;

			continue;
		}

		char* key = ::strtok(buffer, " \t=\r\n");
		if (key == nullptr)
			continue;

		char* value = ::strtok(nullptr, "\r\n");
		if (value == nullptr)
			continue;

		// Remove quotes from the value
		size_t len = ::strlen(value);
		if (len > 1U && *value == '"' && value[len - 1U] == '"') {
			value[len - 1U] = '\0';
			value++;
		} else {
			char *p;

			// if value is not quoted, remove after # (to make comment)
			if ((p = strchr(value, '#')) != nullptr)
				*p = '\0';

			// Remove trailing tab/space
			for (p = value + strlen(value) - 1U; p >= value && (*p == '\t' || *p == ' '); p--)
				*p = '\0';
		}

		if (section == SECTION::GENERAL) {
			if (::strcmp(key, "Callsign") == 0)
				m_callsign = value;
			else if (::strcmp(key, "Protocol") == 0)
				m_protocol = value;
			else if (::strcmp(key, "Debug") == 0)
				m_debug = ::atoi(value) == 1;
			else if (::strcmp(key, "Daemon") == 0)
				m_daemon = ::atoi(value) == 1;
		} else if (section == SECTION::LOG) {
			if (::strcmp(key, "DisplayLevel") == 0)
				m_logDisplayLevel = (unsigned int)::atoi(value);
			else if (::strcmp(key, "MQTTLevel") == 0)
				m_logMQTTLevel = (unsigned int)::atoi(value);
		} else if (section == SECTION::MQTT) {
			if (::strcmp(key, "Address") == 0)
				m_mqttAddress = value;
			else if (::strcmp(key, "Port") == 0)
				m_mqttPort = uint16_t(::atoi(value));
			else if (::strcmp(key, "Keepalive") == 0)
				m_mqttKeepalive = (unsigned int)::atoi(value);
			else if (::strcmp(key, "Name") == 0)
				m_mqttName = value;
			else if (::strcmp(key, "Auth") == 0)
				m_mqttAuthEnabled = ::atoi(value) == 1;
			else if (::strcmp(key, "Username") == 0)
				m_mqttUsername = value;
			else if (::strcmp(key, "Password") == 0)
				m_mqttPassword = value;
		} else if (section == SECTION::NETWORK) {
			if (::strcmp(key, "LocalAddress") == 0)
				m_networkLocalAddress = value;
			else if (::strcmp(key, "LocalPort") == 0)
				m_networkLocalPort = uint16_t(::atoi(value));
			else if (::strcmp(key, "RptAddress") == 0)
				m_networkRptAddress = value;
			else if (::strcmp(key, "RptPort") == 0)
				m_networkRptPort = uint16_t(::atoi(value));
			else if (::strcmp(key, "Debug") == 0)
				m_networkDebug = ::atoi(value) == 1;
		} else if (section == SECTION::USRP_NETWORK) {
			if (::strcmp(key, "LocalAddress") == 0)
				m_usrpLocalAddress = value;
			else if (::strcmp(key, "LocalPort") == 0)
				m_usrpLocalPort = uint16_t(::atoi(value));
			else if (::strcmp(key, "RemoteAddress") == 0)
				m_usrpRemoteAddress = value;
			else if (::strcmp(key, "RemotePort") == 0)
				m_usrpRemotePort = uint16_t(::atoi(value));
			else if (::strcmp(key, "Debug") == 0)
				m_usrpDebug = ::atoi(value) == 1;
		} else if (section == SECTION::RAW_NETWORK) {
			if (::strcmp(key, "LocalAddress") == 0)
				m_rawLocalAddress = value;
			else if (::strcmp(key, "LocalPort") == 0)
				m_rawLocalPort = uint16_t(::atoi(value));
			else if (::strcmp(key, "RemoteAddress") == 0)
				m_rawRemoteAddress = value;
			else if (::strcmp(key, "RemotePort") == 0)
				m_rawRemotePort = uint16_t(::atoi(value));
			else if (::strcmp(key, "SampleRate") == 0)
				m_rawSampleRate = (unsigned int)::atoi(value);
			else if (::strcmp(key, "SquelchFile") == 0)
				m_rawSquelchFile = value;
			else if (::strcmp(key, "Debug") == 0)
				m_rawDebug = ::atoi(value) == 1;
		} else if (section == SECTION::IAX_NETWORK) {
			if (::strcmp(key, "LocalAddress") == 0)
				m_iaxLocalAddress = value;
			else if (::strcmp(key, "LocalPort") == 0)
				m_iaxLocalPort = uint16_t(::atoi(value));
			else if (::strcmp(key, "RemoteAddress") == 0)
				m_iaxRemoteAddress = value;
			else if (::strcmp(key, "RemotePort") == 0)
				m_iaxRemotePort = uint16_t(::atoi(value));
			else if (::strcmp(key, "Username") == 0)
				m_iaxUsername = value;
			else if (::strcmp(key, "Password") == 0)
				m_iaxPassword = value;
			else if (::strcmp(key, "Node") == 0)
				m_iaxNode = value;
			else if (::strcmp(key, "Debug") == 0)
				m_iaxDebug = ::atoi(value) == 1;
		}
	}

	::fclose(fp);

	return true;
}

std::string CConf::getCallsign() const
{
	return m_callsign;
}

std::string CConf::getProtocol() const
{
	return m_protocol;
}

bool CConf::getDebug() const
{
	return m_debug;
}

bool CConf::getDaemon() const
{
	return m_daemon;
}

unsigned int CConf::getLogDisplayLevel() const
{
	return m_logDisplayLevel;
}

unsigned int CConf::getLogMQTTLevel() const
{
	return m_logMQTTLevel;
}

std::string CConf::getMQTTAddress() const
{
	return m_mqttAddress;
}

uint16_t CConf::getMQTTPort() const
{
	return m_mqttPort;
}

unsigned int CConf::getMQTTKeepalive() const
{
	return m_mqttKeepalive;
}

std::string CConf::getMQTTName() const
{
	return m_mqttName;
}

bool CConf::getMQTTAuthEnabled() const
{
	return m_mqttAuthEnabled;
}

std::string CConf::getMQTTUsername() const
{
	return m_mqttUsername;
}

std::string CConf::getMQTTPassword() const
{
	return m_mqttPassword;
}

std::string CConf::getNetworkLocalAddress() const
{
	return m_networkLocalAddress;
}

uint16_t CConf::getNetworkLocalPort() const
{
	return m_networkLocalPort;
}

std::string CConf::getNetworkRptAddress() const
{
	return m_networkRptAddress;
}

uint16_t CConf::getNetworkRptPort() const
{
	return m_networkRptPort;
}

bool CConf::getNetworkDebug() const
{
	return m_networkDebug;
}

std::string CConf::getUSRPLocalAddress() const
{
	return m_usrpLocalAddress;
}

uint16_t CConf::getUSRPLocalPort() const
{
	return m_usrpLocalPort;
}

std::string CConf::getUSRPRemoteAddress() const
{
	return m_usrpRemoteAddress;
}

uint16_t CConf::getUSRPRemotePort() const
{
	return m_usrpRemotePort;
}

bool CConf::getUSRPDebug() const
{
	return m_usrpDebug;
}

std::string CConf::getRAWLocalAddress() const
{
	return m_rawLocalAddress;
}

uint16_t CConf::getRAWLocalPort() const
{
	return m_rawLocalPort;
}

std::string CConf::getRAWRemoteAddress() const
{
	return m_rawRemoteAddress;
}

uint16_t CConf::getRAWRemotePort() const
{
	return m_rawRemotePort;
}

unsigned int CConf::getRAWSampleRate() const
{
	return m_rawSampleRate;
}

std::string CConf::getRAWSquelchFile() const
{
	return m_rawSquelchFile;
}

bool CConf::getRAWDebug() const
{
	return m_rawDebug;
}

std::string CConf::getIAXLocalAddress() const
{
	return m_iaxLocalAddress;
}

uint16_t CConf::getIAXLocalPort() const
{
	return m_iaxLocalPort;
}

std::string CConf::getIAXRemoteAddress() const
{
	return m_iaxRemoteAddress;
}

uint16_t CConf::getIAXRemotePort() const
{
	return m_iaxRemotePort;
}

std::string CConf::getIAXUsername() const
{
	return m_iaxUsername;
}

std::string CConf::getIAXPassword() const
{
	return m_iaxPassword;
}

std::string CConf::getIAXNode() const
{
	return m_iaxNode;
}

bool CConf::getIAXDebug() const
{
	return m_iaxDebug;
}
