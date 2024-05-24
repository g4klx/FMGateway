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

#include "FMNetwork.h"
#include "Utils.h"
#include "Log.h"

#include <cstdio>
#include <cassert>
#include <cstring>

#if !defined(_WIN32) && !defined(_WIN64)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

const unsigned int BUFFER_LENGTH = 1500U;

CFMNetwork::CFMNetwork(const std::string& localAddress, uint16_t localPort, const std::string& gatewayAddress, uint16_t gatewayPort, bool debug) :
m_socket(localAddress, localPort),
m_addr(),
m_addrLen(0U),
m_debug(debug),
m_buffer(2000U, "FM Network"),
m_timer(1000U, 5U)
{
	assert(gatewayPort > 0U);
	assert(!gatewayAddress.empty());

	if (CUDPSocket::lookup(gatewayAddress, gatewayPort, m_addr, m_addrLen) != 0)
		m_addrLen = 0U;
}

CFMNetwork::~CFMNetwork()
{
}

bool CFMNetwork::open()
{
	if (m_addrLen == 0U) {
		LogError("Unable to resolve the address of the FM Gateway");
		return false;
	}

	LogMessage("Opening FM network connection");

	bool ret = m_socket.open(m_addr);
	if (!ret)
		return false;

	m_timer.start();

	return true;
}

bool CFMNetwork::writeData(const float* data, unsigned int nSamples)
{
	assert(data != nullptr);
	assert(nSamples > 0U);

	uint8_t buffer[BUFFER_LENGTH];
	::memset(buffer, 0x00U, BUFFER_LENGTH);

	unsigned int length = 0U;

	buffer[length++] = 'F';
	buffer[length++] = 'M';
	buffer[length++] = 'D';

	for (unsigned int i = 0U; i < nSamples; i++) {
		short val = short(data[i] * 32767.0F + 0.5F);			// Changing audio format from float to S16LE

		buffer[length++] = (val >> 0) & 0xFFU;
		buffer[length++] = (val >> 8) & 0xFFU;
	}

	if (m_debug)
		CUtils::dump(1U, "FM Network Data Sent", buffer, length);

	return m_socket.write(buffer, length, m_addr, m_addrLen);
}

bool CFMNetwork::writePing()
{
	uint8_t buffer[5U];

	buffer[0U] = 'F';
	buffer[1U] = 'M';
	buffer[2U] = 'P';

	if (m_debug)
		CUtils::dump(1U, "FM Network Data Sent", buffer, 3U);

	return m_socket.write(buffer, 3U, m_addr, m_addrLen);
}

void CFMNetwork::clock(unsigned int ms)
{
	m_timer.clock(ms);
	if (m_timer.isRunning() && m_timer.hasExpired()) {
		writePing();
		m_timer.start();
	}

	uint8_t buffer[BUFFER_LENGTH];

	sockaddr_storage addr;
	unsigned int addrlen;
	int length = m_socket.read(buffer, BUFFER_LENGTH, addr, addrlen);
	if (length <= 0)
		return;

	// Check if the data is for us
	if (!CUDPSocket::match(addr, m_addr, IMT_ADDRESS_AND_PORT)) {
		LogMessage("FM packet received from an invalid source");
		return;
	}

	// Invalid packet type?
	if (::memcmp(buffer, "FM", 2U) != 0)
		return;

	if (::memcmp(buffer, "FMP", 3U) == 0)
		return;

	if (m_debug)
		CUtils::dump(1U, "FM Network Data Received", buffer, length);

	if (::memcmp(buffer, "FMD", 3U) == 0) {
		uint16_t len = length;
		m_buffer.addData((uint8_t*)&len, sizeof(uint16_t));
		m_buffer.addData(buffer, length);
	} else if (::memcmp(buffer, "FMS", 3U) == 0) {
		uint16_t len = length;
		m_buffer.addData((uint8_t*)&len, sizeof(uint16_t));
		m_buffer.addData(buffer, length);
	} else if (::memcmp(buffer, "FME", 3U) == 0) {
		uint16_t len = 3U;
		m_buffer.addData((uint8_t*)&len, sizeof(uint16_t));
		m_buffer.addData(buffer, 3U);
	}
}

NETWORK_TYPE CFMNetwork::readType() const
{
	unsigned int length = m_buffer.dataSize();
	if (length == 0U)
		return NT_NONE;

	uint16_t len = 0U;
	m_buffer.peek((uint8_t*)&len, sizeof(uint16_t));

	uint8_t buffer[5U];
	m_buffer.peek(buffer, 3U);

	if (::memcmp(buffer, "FMD", 3U) == 0)
		return NT_DATA;
	else if (::memcmp(buffer, "FMS", 3U) == 0)
		return NT_START;
	else if (::memcmp(buffer, "FME", 3U) == 0)
		return NT_END;

	return NT_DATA;		// ???
}

std::string CFMNetwork::readStart()
{
	unsigned int length = m_buffer.dataSize();
	if (length == 0U)
		return "";

	uint16_t len = 0U;
	m_buffer.getData((uint8_t*)&len, sizeof(uint16_t));

	uint8_t buffer[BUFFER_LENGTH];
	m_buffer.getData(buffer, len);

	if (::memcmp(buffer, "FMS", 3U) != 0)
		assert(false);

	char* callsign = (char*)(buffer + 3U);

	return std::string(callsign, ::strlen(callsign));
}

unsigned int CFMNetwork::readData(float* out, unsigned int nOut)
{
	assert(out != nullptr);
	assert(nOut > 0U);

	unsigned int length = m_buffer.dataSize();
	if (length == 0U)
		return 0U;

	uint16_t len = 0U;
	m_buffer.getData((uint8_t*)&len, sizeof(uint16_t));

	uint8_t buffer[BUFFER_LENGTH];
	m_buffer.getData(buffer, len);

	if (::memcmp(buffer, "FMD", 3U) != 0)
		assert(false);

	unsigned int nSamples = (len - 3U) / sizeof(uint16_t);

	if (nOut < nSamples)
		nSamples = nOut;

	const uint8_t* data = buffer + 3U;
	for (unsigned int i = 0U; i < nSamples; i++) {
		short val = ((data[i * 2U + 0U] & 0xFFU) << 0) + ((data[i * 2U + 1U] & 0xFFU) << 8);
		out[i] = float(val) / 65536.0F;
	}

	return nSamples;
}

void CFMNetwork::readEnd()
{
	unsigned int length = m_buffer.dataSize();
	if (length == 0U)
		return ;

	uint16_t len = 0U;
	m_buffer.getData((uint8_t*)&len, sizeof(uint16_t));

	uint8_t buffer[BUFFER_LENGTH];
	m_buffer.getData(buffer, len);

	if (::memcmp(buffer, "FME", 3U) != 0)
		assert(false);
}

void CFMNetwork::reset()
{
	m_buffer.clear();
}

void CFMNetwork::close()
{
	m_socket.close();

	LogMessage("Closing FM network connection");
}
