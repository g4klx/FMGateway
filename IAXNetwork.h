/*
 *   Copyright (C) 2020,2021,2023,2024,2025 by Jonathan Naylor G4KLX
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

#ifndef	IAXNetwork_H
#define	IAXNetwork_H

#include "RingBuffer.h"
#include "UDPSocket.h"
#include "StopWatch.h"
#include "Network.h"
#include "Timer.h"

#include <cstdint>
#include <string>

#if defined(_WIN32) || defined(_WIN64)
#include <wincrypt.h>
#else
#include <md5.h>
#endif

enum class IAX_STATUS {
	DISCONNECTED,
	CONNECTING,
	REGISTERNG,
	CONNECTED
};

class CIAXNetwork : public INetwork {
public:
	CIAXNetwork(const std::string& callsign, const std::string& username, const std::string& password, const std::string& node, const std::string& localAddress, uint16_t localPort, const std::string& gatewayAddress, uint16_t gatewayPort, bool debug);
	virtual ~CIAXNetwork();

	virtual bool open();

	virtual bool writeStart(const std::string& callsign);

	virtual bool writeData(const float* data, unsigned int nSamples);

	virtual bool writeEnd();

	virtual unsigned int readData(float* out, unsigned int nOut);

	virtual void reset();

	virtual void close();

	virtual void clock(unsigned int ms);

private:
	std::string         m_callsign;
	std::string         m_username;
	std::string         m_password;
	std::string         m_node;
	CUDPSocket          m_socket;
	sockaddr_storage    m_addr;
	unsigned int        m_addrLen;
	bool                m_debug;
	CRingBuffer<uint8_t> m_buffer;
	IAX_STATUS          m_status;
	CTimer              m_retryTimer;
	CTimer              m_pingTimer;
	std::string         m_seed;
	CStopWatch          m_timestamp;
	uint16_t            m_sCallNo;
	uint16_t            m_dCallNo;
	uint8_t             m_iSeqNo;
	uint8_t             m_oSeqNo;
	uint32_t            m_rxJitter;
	uint32_t            m_rxLoss;
	uint32_t            m_rxFrames;
	uint16_t            m_rxDelay;
	uint32_t            m_rxDropped;
	uint32_t            m_rxOOO;
	bool                m_keyed;
#if defined(_WIN32) || defined(_WIN64)
	HCRYPTPROV          m_provider;
#endif

	bool writeNew(bool retry);
	bool writeAuthRep();
	bool writeKey(bool key);
	bool writePing();
	bool writePong(uint32_t ts);
	bool writeAck(uint32_t ts);
	bool writeLagRq();
	bool writeLagRp(uint32_t ts);
	bool writeHangup();
	bool writeRegReq(bool retry);
	bool writeAudio(const int16_t* audio, unsigned int length);

	void uLawEncode(const int16_t* audio, uint8_t* buffer, unsigned int length) const;
	void uLawDecode(const uint8_t* buffer, int16_t* audio, unsigned int length) const;

	bool compareFrame(const uint8_t* buffer, uint8_t type1, uint8_t type2) const;
};

#endif

