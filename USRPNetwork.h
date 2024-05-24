/*
 *   Copyright (C) 2020,2021,2023,2024 by Jonathan Naylor G4KLX
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

#ifndef	USRPNetwork_H
#define	USRPNetwork_H

#include "RingBuffer.h"
#include "UDPSocket.h"
#include "Network.h"

#include <cstdint>
#include <string>

class CUSRPNetwork : public INetwork {
public:
	CUSRPNetwork(const std::string& localAddress, uint16_t localPort, const std::string& gatewayAddress, uint16_t gatewayPort, bool debug);
	virtual ~CUSRPNetwork();

	virtual bool open();

	virtual bool writeStart(const std::string& callsign);

	virtual bool writeData(const float* data, unsigned int nSamples);

	virtual bool writeEnd();

	virtual unsigned int readData(float* out, unsigned int nOut);

	virtual void reset();

	virtual void close();

	virtual void clock(unsigned int ms);

private:
	CUDPSocket          m_socket;
	sockaddr_storage    m_addr;
	unsigned int        m_addrLen;
	bool                m_debug;
	CRingBuffer<uint8_t> m_buffer;
	uint32_t            m_seqNo;
};

#endif

