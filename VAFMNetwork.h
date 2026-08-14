/*
 *   Copyright (C) 2026 by Steve Miller KC1AWV
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 */

#ifndef VAFMNetwork_H
#define VAFMNetwork_H

#include "Network.h"
#include "RingBuffer.h"
#include "Timer.h"
#include "UDPSocket.h"

#include <opus/opus.h>
#include <samplerate.h>

#include <cstdint>
#include <string>

class CVAFMNetwork : public INetwork {
public:
	CVAFMNetwork(const std::string& callsign, const std::string& passphrase,
		const std::string& localAddress, uint16_t localPort,
		const std::string& remoteAddress, uint16_t remotePort,
		unsigned int keepalive, int opusBitrate, bool debug);
	virtual ~CVAFMNetwork();

	virtual bool open();
	virtual bool writeStart(const std::string& callsign);
	virtual bool writeData(const float* data, unsigned int nSamples);
	virtual bool writeEnd();
	virtual unsigned int readData(float* out, unsigned int nOut);
	virtual void reset();
	virtual void close();
	virtual void clock(unsigned int ms);

private:
	enum class STATE {
		CLOSED,
		WAIT_WELCOME,
		WAIT_AUTH_WELCOME,
		READY
	};

	CUDPSocket          m_socket;
	sockaddr_storage    m_addr;
	unsigned int        m_addrLen;
	std::string         m_callsign;
	std::string         m_txCallsign;
	std::string         m_passphrase;
	unsigned int        m_keepalive;
	int                 m_opusBitrate;
	bool                m_debug;
	STATE               m_state;
	OpusEncoder*        m_encoder;
	OpusDecoder*        m_decoder;
	SRC_STATE*          m_txResampler;
	SRC_STATE*          m_rxResampler;
	CRingBuffer<float>  m_txPCM;
	CRingBuffer<float>  m_txOpusPCM;
	CRingBuffer<float>  m_rxPCM;
	CTimer              m_keepaliveTimer;
	CTimer              m_handshakeTimer;
	uint32_t            m_txStreamId;
	uint32_t            m_txSequence;
	uint32_t            m_rxStreamId;
	bool                m_txActive;

	bool sendHello();
	bool sendAuth();
	bool sendKeepalive();
	bool sendControl(uint8_t type, const uint8_t* payload = nullptr, unsigned int length = 0U);
	bool sendPacket(uint8_t type, uint8_t flags, uint8_t codec, uint32_t streamId,
		uint32_t sequence, uint32_t timestamp, const std::string& callsign,
		const uint8_t* payload, unsigned int length);
	bool encodePending();
	bool flushTransmit();
	bool handlePacket(const uint8_t* data, unsigned int length);
	bool handleVoice(uint8_t flags, uint32_t streamId, const uint8_t* payload, unsigned int length);
	void resetTransmit();
	void resetReceive();
	uint32_t createStreamId() const;
};

#endif