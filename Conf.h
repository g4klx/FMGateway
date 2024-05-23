/*
 *   Copyright (C) 2015,2016,2017,2018,2020,2021,2024 by Jonathan Naylor G4KLX
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

#if !defined(CONF_H)
#define	CONF_H

#include <string>

#include <cstdint>

class CConf
{
public:
	CConf(const std::string& file);
	~CConf();

	bool read();

	// The General section
	std::string  getCallsign() const;
	std::string  getProtocol() const;
	bool         getDebug() const;
	bool         getDaemon() const;

	// The Log section
	unsigned int getLogDisplayLevel() const;
	unsigned int getLogMQTTLevel() const;

	// The MQTT section
	std::string  getMQTTAddress() const;
	uint16_t     getMQTTPort() const;
	unsigned int getMQTTKeepalive() const;
	std::string  getMQTTName() const;

	// The Network section
	std::string  getNetworkLocalAddress() const;
	uint16_t     getNetworkLocalPort() const;
	std::string  getNetworkRptAddress() const;
	uint16_t     getNetworkRptPort() const;
	bool         getNetworkDebug() const;

	// The USRP Network section
	std::string  getUSRPLocalAddress() const;
	uint16_t     getUSRPLocalPort() const;
	std::string  getUSRPRemoteAddress() const;
	uint16_t     getUSRPRemotePort() const;
	bool         getUSRPDebug() const;

	// The RAW Network section
	std::string  getRAWLocalAddress() const;
	uint16_t     getRAWLocalPort() const;
	std::string  getRAWRemoteAddress() const;
	uint16_t     getRAWRemotePort() const;
	unsigned int getRAWSampleRate() const;
	std::string  getRAWSquelchFile() const;
	bool         getRAWDebug() const;

	// The IAX Network section
	std::string  getIAXLocalAddress() const;
	uint16_t     getIAXLocalPort() const;
	std::string  getIAXRemoteAddress() const;
	uint16_t     getIAXRemotePort() const;
	std::string  getIAXUsername() const;
	std::string  getIAXPassword() const;
	std::string  getIAXNode() const;
	bool         getIAXDebug() const;

private:
	std::string  m_file;
	std::string  m_callsign;
	std::string  m_protocol;
	bool         m_debug;
	bool         m_daemon;

	unsigned int m_logDisplayLevel;
	unsigned int m_logMQTTLevel;

	std::string  m_mqttAddress;
	uint16_t     m_mqttPort;
	unsigned int m_mqttKeepalive;
	std::string  m_mqttName;

	std::string  m_networkLocalAddress;
	uint16_t     m_networkLocalPort;
	std::string  m_networkRptAddress;
	uint16_t     m_networkRptPort;
	bool         m_networkDebug;

	std::string  m_usrpLocalAddress;
	uint16_t     m_usrpLocalPort;
	std::string  m_usrpRemoteAddress;
	uint16_t     m_usrpRemotePort;
	bool         m_usrpDebug;

	std::string  m_rawLocalAddress;
	uint16_t     m_rawLocalPort;
	std::string  m_rawRemoteAddress;
	uint16_t     m_rawRemotePort;
	unsigned int m_rawSampleRate;
	std::string  m_rawSquelchFile;
	bool         m_rawDebug;

	std::string  m_iaxLocalAddress;
	uint16_t     m_iaxLocalPort;
	std::string  m_iaxRemoteAddress;
	uint16_t     m_iaxRemotePort;
	std::string  m_iaxUsername;
	std::string  m_iaxPassword;
	std::string  m_iaxNode;
	bool         m_iaxDebug;
};

#endif
