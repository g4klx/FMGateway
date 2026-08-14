/*
 *   Copyright (C) 2026 by Steve Miller KC1AWV
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 */

#include "VAFMNetwork.h"
#include "Log.h"
#include "Utils.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cctype>
#include <cstring>

namespace {
const uint8_t VAFM_VERSION = 0x01U;
const uint8_t VAFM_HELLO = 0x01U;
const uint8_t VAFM_WELCOME = 0x02U;
const uint8_t VAFM_AUTH = 0x03U;
const uint8_t VAFM_KEEPALIVE = 0x04U;
const uint8_t VAFM_VOICE = 0x05U;
const uint8_t VAFM_END = 0x06U;
const uint8_t VAFM_ERROR = 0x07U;
const uint8_t VAFM_DISCONNECT = 0x09U;
const uint8_t VAFM_FLAG_EOT = 0x01U;
const uint8_t VAFM_CODEC_NONE = 0x00U;
const uint8_t VAFM_CODEC_OPUS = 0x01U;

const unsigned int VAFM_HEADER_LENGTH = 32U;
const unsigned int VAFM_MAX_PAYLOAD = 4096U;
const unsigned int MMDVM_FRAME_SAMPLES = 160U;
const unsigned int OPUS_FRAME_SAMPLES = 960U;
const unsigned int OPUS_MAX_PACKET = 1276U;

uint16_t getUInt16(const uint8_t* p)
{
	return (uint16_t(p[0U]) << 8) | uint16_t(p[1U]);
}

uint32_t getUInt32(const uint8_t* p)
{
	return (uint32_t(p[0U]) << 24) | (uint32_t(p[1U]) << 16) |
		(uint32_t(p[2U]) << 8) | uint32_t(p[3U]);
}

void putUInt16(uint8_t* p, uint16_t value)
{
	p[0U] = uint8_t(value >> 8);
	p[1U] = uint8_t(value);
}

void putUInt32(uint8_t* p, uint32_t value)
{
	p[0U] = uint8_t(value >> 24);
	p[1U] = uint8_t(value >> 16);
	p[2U] = uint8_t(value >> 8);
	p[3U] = uint8_t(value);
}

std::string normaliseCallsign(const std::string& callsign)
{
	std::string value;
	for (std::string::const_iterator it = callsign.begin(); it != callsign.end() && value.size() < 10U; ++it) {
		if (*it == ' ')
			break;
		value.push_back(char(::toupper(static_cast<unsigned char>(*it))));
	}
	return value;
}
}

CVAFMNetwork::CVAFMNetwork(const std::string& callsign, const std::string& passphrase,
	const std::string& localAddress, uint16_t localPort,
	const std::string& remoteAddress, uint16_t remotePort,
	unsigned int keepalive, int opusBitrate, bool debug) :
m_socket(localAddress, localPort),
m_addr(),
m_addrLen(0U),
m_callsign(normaliseCallsign(callsign)),
m_txCallsign(m_callsign),
m_passphrase(passphrase),
m_keepalive(keepalive),
m_opusBitrate(opusBitrate),
m_debug(debug),
m_state(STATE::CLOSED),
m_encoder(nullptr),
m_decoder(nullptr),
m_txResampler(nullptr),
m_rxResampler(nullptr),
m_txPCM(8000U, "VAFM transmit PCM"),
m_txOpusPCM(4800U, "VAFM transmit Opus PCM"),
m_rxPCM(8000U, "VAFM receive PCM"),
m_keepaliveTimer(1000U, keepalive),
m_handshakeTimer(1000U, 5U),
m_txStreamId(0U),
m_txSequence(0U),
m_rxStreamId(0U),
m_txActive(false)
{
	assert(!m_callsign.empty());
	assert(remotePort > 0U);
	assert(!remoteAddress.empty());
	assert(keepalive > 0U);

	if (CUDPSocket::lookup(remoteAddress, remotePort, m_addr, m_addrLen) != 0)
		m_addrLen = 0U;
}

CVAFMNetwork::~CVAFMNetwork()
{
	close();
}

bool CVAFMNetwork::open()
{
	if (m_addrLen == 0U) {
		LogError("Unable to resolve the VAFM reflector address");
		return false;
	}

	int error = OPUS_OK;
	m_encoder = ::opus_encoder_create(48000, 1, OPUS_APPLICATION_VOIP, &error);
	if (m_encoder == nullptr || error != OPUS_OK) {
		LogError("Unable to create the VAFM Opus encoder - %s", ::opus_strerror(error));
		return false;
	}

	error = ::opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE(m_opusBitrate));
	if (error != OPUS_OK) {
		LogError("Unable to set the VAFM Opus bitrate - %s", ::opus_strerror(error));
		close();
		return false;
	}

	m_decoder = ::opus_decoder_create(48000, 1, &error);
	if (m_decoder == nullptr || error != OPUS_OK) {
		LogError("Unable to create the VAFM Opus decoder - %s", ::opus_strerror(error));
		close();
		return false;
	}

	m_txResampler = ::src_new(SRC_SINC_FASTEST, 1, &error);
	if (m_txResampler == nullptr) {
		LogError("Unable to create the VAFM transmit resampler - %s", ::src_strerror(error));
		close();
		return false;
	}

	m_rxResampler = ::src_new(SRC_SINC_FASTEST, 1, &error);
	if (m_rxResampler == nullptr) {
		LogError("Unable to create the VAFM receive resampler - %s", ::src_strerror(error));
		close();
		return false;
	}

	if (!m_socket.open(m_addr)) {
		close();
		return false;
	}

	LogMessage("Opening VAFM UDP network connection");
	m_state = STATE::WAIT_WELCOME;
	m_handshakeTimer.start();
	return sendHello();
}

bool CVAFMNetwork::writeStart(const std::string& callsign)
{
	resetTransmit();
	m_txCallsign = normaliseCallsign(callsign);
	if (m_txCallsign.empty())
		m_txCallsign = m_callsign;
	m_txStreamId = createStreamId();
	m_txActive = true;
	return true;
}

bool CVAFMNetwork::writeData(const float* data, unsigned int nSamples)
{
	assert(data != nullptr);
	assert(nSamples > 0U);

	if (m_state != STATE::READY)
		return true;

	if (!m_txActive && !writeStart(m_callsign))
		return false;

	if (!m_txPCM.addData(data, nSamples))
		return false;

	return encodePending();
}

bool CVAFMNetwork::encodePending()
{
	while (m_txPCM.hasData()) {
		float in[MMDVM_FRAME_SAMPLES];
		float resampled[OPUS_FRAME_SAMPLES + 256U];
		unsigned int nIn = std::min(m_txPCM.dataSize(), MMDVM_FRAME_SAMPLES);
		m_txPCM.peek(in, nIn);

		SRC_DATA src;
		::memset(&src, 0x00U, sizeof(src));
		src.data_in = in;
		src.data_out = resampled;
		src.input_frames = nIn;
		src.output_frames = OPUS_FRAME_SAMPLES + 256U;
		src.src_ratio = 6.0;

		int error = ::src_process(m_txResampler, &src);
		if (error != 0) {
			LogError("VAFM transmit resampler error - %s", ::src_strerror(error));
			return false;
		}
		if (src.input_frames_used > 0)
			m_txPCM.getData(in, unsigned(src.input_frames_used));
		if (!m_txOpusPCM.addData(resampled, unsigned(src.output_frames_gen)))
			return false;
		if (src.input_frames_used == 0 && src.output_frames_gen == 0)
			break;
	}

	while (m_txOpusPCM.dataSize() >= OPUS_FRAME_SAMPLES) {
		float pcm[OPUS_FRAME_SAMPLES];
		uint8_t encoded[OPUS_MAX_PACKET];
		m_txOpusPCM.getData(pcm, OPUS_FRAME_SAMPLES);

		int length = ::opus_encode_float(m_encoder, pcm, OPUS_FRAME_SAMPLES, encoded, OPUS_MAX_PACKET);
		if (length < 0) {
			LogError("VAFM Opus encode error - %s", ::opus_strerror(length));
			return false;
		}

		if (!sendPacket(VAFM_VOICE, 0U, VAFM_CODEC_OPUS, m_txStreamId,
			m_txSequence, m_txSequence * 20U, m_txCallsign, encoded, unsigned(length)))
			return false;
		m_txSequence++;
	}

	return true;
}

bool CVAFMNetwork::flushTransmit()
{
	if (!encodePending())
		return false;

	for (;;) {
		float input = 0.0F;
		float resampled[OPUS_FRAME_SAMPLES];
		SRC_DATA src;
		::memset(&src, 0x00U, sizeof(src));
		src.data_in = &input;
		src.data_out = resampled;
		src.output_frames = OPUS_FRAME_SAMPLES;
		src.end_of_input = 1;
		src.src_ratio = 6.0;
		int error = ::src_process(m_txResampler, &src);
		if (error != 0) {
			LogError("VAFM transmit resampler flush error - %s", ::src_strerror(error));
			return false;
		}
		if (src.output_frames_gen == 0)
			break;
		if (!m_txOpusPCM.addData(resampled, unsigned(src.output_frames_gen)))
			return false;
	}

	unsigned int remaining = m_txOpusPCM.dataSize();
	if (remaining > 0U && remaining < OPUS_FRAME_SAMPLES) {
		float silence[OPUS_FRAME_SAMPLES];
		::memset(silence, 0x00U, sizeof(silence));
		if (!m_txOpusPCM.addData(silence, OPUS_FRAME_SAMPLES - remaining))
			return false;
	}

	while (m_txOpusPCM.dataSize() >= OPUS_FRAME_SAMPLES) {
		float pcm[OPUS_FRAME_SAMPLES];
		uint8_t encoded[OPUS_MAX_PACKET];
		m_txOpusPCM.getData(pcm, OPUS_FRAME_SAMPLES);
		int length = ::opus_encode_float(m_encoder, pcm, OPUS_FRAME_SAMPLES, encoded, OPUS_MAX_PACKET);
		if (length < 0) {
			LogError("VAFM Opus encode error - %s", ::opus_strerror(length));
			return false;
		}
		if (!sendPacket(VAFM_VOICE, 0U, VAFM_CODEC_OPUS, m_txStreamId,
			m_txSequence, m_txSequence * 20U, m_txCallsign, encoded, unsigned(length)))
			return false;
		m_txSequence++;
	}
	return true;
}

bool CVAFMNetwork::writeEnd()
{
	if (!m_txActive)
		return true;

	bool result = true;
	if (m_state == STATE::READY) {
		result = flushTransmit();
		if (result)
			result = sendPacket(VAFM_END, 0U, VAFM_CODEC_NONE, m_txStreamId,
				m_txSequence, m_txSequence * 20U, m_txCallsign, nullptr, 0U);
	}

	resetTransmit();
	return result;
}

unsigned int CVAFMNetwork::readData(float* out, unsigned int nOut)
{
	assert(out != nullptr);
	assert(nOut > 0U);

	unsigned int available = m_rxPCM.dataSize();
	if (available == 0U)
		return 0U;
	if (nOut > available)
		nOut = available;
	m_rxPCM.getData(out, nOut);
	return nOut;
}

void CVAFMNetwork::clock(unsigned int ms)
{
	m_handshakeTimer.clock(ms);
	m_keepaliveTimer.clock(ms);

	if (m_state == STATE::READY && m_keepaliveTimer.hasExpired()) {
		sendKeepalive();
		m_keepaliveTimer.start();
	} else if ((m_state == STATE::WAIT_WELCOME || m_state == STATE::WAIT_AUTH_WELCOME) && m_handshakeTimer.hasExpired()) {
		m_state = STATE::WAIT_WELCOME;
		sendHello();
		m_handshakeTimer.start();
	}

	uint8_t buffer[VAFM_HEADER_LENGTH + VAFM_MAX_PAYLOAD];
	for (;;) {
		sockaddr_storage addr;
		unsigned int addrLen = 0U;
		int length = m_socket.read(buffer, sizeof(buffer), addr, addrLen);
		if (length <= 0)
			break;

		if (!CUDPSocket::match(addr, m_addr, IPMATCHTYPE::ADDRESS_AND_PORT)) {
			LogMessage("VAFM packet received from an invalid source");
			continue;
		}
		if (m_debug)
			CUtils::dump(1U, "VAFM UDP packet received", buffer, unsigned(length));
		handlePacket(buffer, unsigned(length));
	}
}

bool CVAFMNetwork::handlePacket(const uint8_t* data, unsigned int length)
{
	if (length < VAFM_HEADER_LENGTH || ::memcmp(data, "VAFM", 4U) != 0 || data[4U] != VAFM_VERSION)
		return false;

	unsigned int payloadLength = getUInt16(data + 30U);
	if (payloadLength > VAFM_MAX_PAYLOAD || length != VAFM_HEADER_LENGTH + payloadLength) {
		LogError("Invalid VAFM UDP packet length");
		return false;
	}

	uint8_t type = data[5U];
	uint8_t flags = data[6U];
	uint8_t codec = data[7U];
	uint32_t streamId = getUInt32(data + 8U);
	const uint8_t* payload = data + VAFM_HEADER_LENGTH;

	switch (type) {
	case VAFM_WELCOME:
		if (m_state == STATE::WAIT_WELCOME && !m_passphrase.empty()) {
			if (!sendAuth())
				return false;
			m_state = STATE::WAIT_AUTH_WELCOME;
			m_handshakeTimer.start();
		} else {
			m_state = STATE::READY;
			m_handshakeTimer.stop();
			m_keepaliveTimer.start();
			LogMessage("VAFM UDP session established");
		}
		return true;
	case VAFM_KEEPALIVE:
		return true;
	case VAFM_VOICE:
		if (m_state != STATE::READY || codec != VAFM_CODEC_OPUS)
			return false;
		return handleVoice(flags, streamId, payload, payloadLength);
	case VAFM_END:
		if (streamId == m_rxStreamId)
			resetReceive();
		return true;
	case VAFM_ERROR: {
		std::string error(reinterpret_cast<const char*>(payload), payloadLength);
		LogError("VAFM reflector error - %s", error.c_str());
		return false;
	}
	case VAFM_DISCONNECT:
		LogMessage("VAFM reflector disconnected the session");
		m_state = STATE::WAIT_WELCOME;
		m_handshakeTimer.start();
		return sendHello();
	default:
		return false;
	}
}

bool CVAFMNetwork::handleVoice(uint8_t flags, uint32_t streamId, const uint8_t* payload, unsigned int length)
{
	if (streamId != m_rxStreamId) {
		resetReceive();
		m_rxStreamId = streamId;
	}

	float decoded[OPUS_FRAME_SAMPLES];
	int samples = ::opus_decode_float(m_decoder, payload, int(length), decoded, OPUS_FRAME_SAMPLES, 0);
	if (samples < 0) {
		LogError("VAFM Opus decode error - %s", ::opus_strerror(samples));
		return false;
	}

	float resampled[MMDVM_FRAME_SAMPLES + 64U];
	SRC_DATA src;
	::memset(&src, 0x00U, sizeof(src));
	src.data_in = decoded;
	src.data_out = resampled;
	src.input_frames = samples;
	src.output_frames = MMDVM_FRAME_SAMPLES + 64U;
	src.src_ratio = 1.0 / 6.0;

	int error = ::src_process(m_rxResampler, &src);
	if (error != 0) {
		LogError("VAFM receive resampler error - %s", ::src_strerror(error));
		return false;
	}
	if (!m_rxPCM.addData(resampled, unsigned(src.output_frames_gen)))
		return false;

	if ((flags & VAFM_FLAG_EOT) != 0U)
		resetReceive();
	return true;
}

bool CVAFMNetwork::sendHello()
{
	return sendControl(VAFM_HELLO);
}

bool CVAFMNetwork::sendAuth()
{
	return sendControl(VAFM_AUTH, reinterpret_cast<const uint8_t*>(m_passphrase.data()), unsigned(m_passphrase.size()));
}

bool CVAFMNetwork::sendKeepalive()
{
	return sendControl(VAFM_KEEPALIVE);
}

bool CVAFMNetwork::sendControl(uint8_t type, const uint8_t* payload, unsigned int length)
{
	return sendPacket(type, 0U, VAFM_CODEC_NONE, 0U, 0U, 0U, m_callsign, payload, length);
}

bool CVAFMNetwork::sendPacket(uint8_t type, uint8_t flags, uint8_t codec, uint32_t streamId,
	uint32_t sequence, uint32_t timestamp, const std::string& callsign,
	const uint8_t* payload, unsigned int length)
{
	if (length > VAFM_MAX_PAYLOAD)
		return false;

	uint8_t buffer[VAFM_HEADER_LENGTH + VAFM_MAX_PAYLOAD];
	::memset(buffer, 0x00U, sizeof(buffer));
	::memcpy(buffer, "VAFM", 4U);
	buffer[4U] = VAFM_VERSION;
	buffer[5U] = type;
	buffer[6U] = flags;
	buffer[7U] = codec;
	putUInt32(buffer + 8U, streamId);
	putUInt32(buffer + 12U, sequence);
	putUInt32(buffer + 16U, timestamp);
	::memset(buffer + 20U, ' ', 10U);
	std::string value = normaliseCallsign(callsign);
	::memcpy(buffer + 20U, value.data(), value.size());
	putUInt16(buffer + 30U, uint16_t(length));
	if (length > 0U && payload != nullptr)
		::memcpy(buffer + VAFM_HEADER_LENGTH, payload, length);

	if (m_debug) {
		// Never include the AUTH payload (the passphrase) in a debug dump.
		unsigned int dumpLength = type == VAFM_AUTH ? VAFM_HEADER_LENGTH : VAFM_HEADER_LENGTH + length;
		CUtils::dump(1U, "VAFM UDP packet sent", buffer, dumpLength);
	}
	return m_socket.write(buffer, VAFM_HEADER_LENGTH + length, m_addr, m_addrLen);
}

void CVAFMNetwork::resetTransmit()
{
	m_txPCM.clear();
	m_txOpusPCM.clear();
	m_txStreamId = 0U;
	m_txSequence = 0U;
	m_txActive = false;
	if (m_txResampler != nullptr)
		::src_reset(m_txResampler);
	if (m_encoder != nullptr)
		::opus_encoder_ctl(m_encoder, OPUS_RESET_STATE);
}

void CVAFMNetwork::resetReceive()
{
	m_rxStreamId = 0U;
	if (m_rxResampler != nullptr)
		::src_reset(m_rxResampler);
if (m_decoder != nullptr)
		::opus_decoder_ctl(m_decoder, OPUS_RESET_STATE);
}

void CVAFMNetwork::reset()
{
	resetTransmit();
	resetReceive();
	m_rxPCM.clear();
}

void CVAFMNetwork::close()
{
	if (m_state != STATE::CLOSED)
		m_socket.close();
	m_state = STATE::CLOSED;
	m_handshakeTimer.stop();
	m_keepaliveTimer.stop();
	reset();

	if (m_encoder != nullptr) {
		::opus_encoder_destroy(m_encoder);
		m_encoder = nullptr;
	}
	if (m_decoder != nullptr) {
		::opus_decoder_destroy(m_decoder);
		m_decoder = nullptr;
	}
	if (m_txResampler != nullptr) {
		::src_delete(m_txResampler);
		m_txResampler = nullptr;
	}
	if (m_rxResampler != nullptr) {
		::src_delete(m_rxResampler);
		m_rxResampler = nullptr;
	}
}

uint32_t CVAFMNetwork::createStreamId() const
{
	uint64_t now = uint64_t(std::chrono::steady_clock::now().time_since_epoch().count());
	uint32_t value = uint32_t(now) ^ uint32_t(now >> 32);
	for (std::string::const_iterator it = m_txCallsign.begin(); it != m_txCallsign.end(); ++it)
		value = (value * 33U) ^ uint8_t(*it);
	return value == 0U ? 1U : value;
}