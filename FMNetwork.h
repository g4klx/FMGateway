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

#ifndef	FMNetwork_H
#define	FMNetwork_H

#include "RingBuffer.h"
#include "UDPSocket.h"
#include "Timer.h"

#include <cstdint>
#include <string>

enum NETWORK_TYPE {
	NT_NONE,
	NT_START,
	NT_DATA,
	NT_END
};

class CFMNetwork {
public:
	CFMNetwork(const std::string& localAddress, uint16_t localPort, const std::string& rptAddress, uint16_t rptPort, bool debug);
	~CFMNetwork();

	bool open();

	bool writeData(const float* data, unsigned int nSamples);

	NETWORK_TYPE readType() const;

	std::string readStart();

	unsigned int readData(float* out, unsigned int nOut);

	void readEnd();

	void reset();

	void close();

	void clock(unsigned int ms);

private:
	CUDPSocket          m_socket;
	sockaddr_storage    m_addr;
	unsigned int        m_addrLen;
	bool                m_debug;
	CRingBuffer<uint8_t> m_buffer;
	CTimer              m_timer;

	bool writePing();
};

#endif
