/*
 * http-request-response.cc
 *
 *  Created on: 04-Apr-2020
 *      Author: abhijit
 */

#include "http-common-request-response.h"

#include <algorithm>
#include "ns3/log.h"

#define HRR_BUFFER_LENGTH 2048

NS_LOG_COMPONENT_DEFINE("HttpCommonRequestResponse");

namespace ns3 {


void AddToBuffer(std::queue<std::pair<char*, uint16_t> > &q, const char *buf,
		uint16_t len) {
	char *nbuf = new char[len];
	std::memcpy(nbuf, buf, len);
	std::pair<char*, uint16_t> pair(nbuf, len);
	q.push(pair);
}

uint16_t ReadFromBuffer(std::queue<std::pair<char*, uint16_t> > &q, char *buf,
		uint16_t len) {
	uint16_t ret = 0;
	while (len && !q.empty()) {
		auto pt = q.front();
		uint16_t mlen = std::min(len, pt.second);
		std::memcpy(buf, pt.first, mlen);
		buf += mlen;
		ret += mlen;
		len -= mlen;
		if (mlen < pt.second) {
			char *nbuf = new char[pt.second - mlen];
			std::memcpy(nbuf, pt.first + mlen, pt.second - mlen);
			delete[] pt.first;
			pt.first = nbuf;
			pt.second -= mlen;
			NS_ASSERT(len == 0);
		} else {
			delete[] pt.first;
			q.pop();
		}
	}
	return ret;
}


/****************************************************
 *                                                  *
 ****************************************************/

HttpCommonRequestResponse::HttpCommonRequestResponse(HttReqResState hrrs): m_tmpbuflen(0), m_state(hrrs) {
}

HttpCommonRequestResponse::~HttpCommonRequestResponse() {
	NS_LOG_FUNCTION(this);
}

void HttpCommonRequestResponse::ParseHeader(uint8_t *buf, uint16_t len) {
	NS_LOG_FUNCTION(this);
	NS_ASSERT(m_state != HTTP_REQ_RES_INVALID);

	if (m_state == HTTP_REQ_RES_READ_BODY) {
		AddToBuffer(m_body, (char*) buf, len);
		return;
	}

	std::string headerLine;
	uint8_t last = m_tmpbuflen > 0 ? m_tmpbuf[m_tmpbuflen - 1] : 0;
	uint16_t offset = 0;
	for (uint16_t i = 0; i < len; i++) {
		if (m_state == HTTP_REQ_RES_READ_BODY) {
			offset = i;
			break;
		}

		uint8_t cur = buf[i];
		m_tmpbuf[m_tmpbuflen++] = cur;
		if (last == '\r' and cur == '\n') { //Got a headerLine
			ProcessHeaderLine(); //tmpbuffer already have the buffer;
			m_tmpbuflen = 0;
		}
		last = cur;
	}
	if (m_state == HTTP_REQ_RES_READ_BODY)
		AddToBuffer(m_body, (char*) (buf + offset), len - offset);
}

void HttpCommonRequestResponse::AddHeader(std::string key, std::string value) {
	NS_LOG_FUNCTION(this);
	NS_ASSERT(m_state == HTTP_REQ_RES_INVALID);
	m_headers[key] = value;
}

void HttpCommonRequestResponse::EndHeader() {
	NS_LOG_FUNCTION(this);

	NS_ASSERT(m_state == HTTP_REQ_RES_INVALID);

	ReadFirstHeaderLine();
	for (auto it : m_headers) {
		auto sptr = std::string(it.first + ": " + it.second + "\r\n");
		const char *headerLine = sptr.c_str();
		AddToBuffer(m_request, headerLine, std::strlen(headerLine));

	}
	const char *headerLine = "\r\n";
	AddToBuffer(m_request, headerLine, std::strlen(headerLine));

	m_state = HTTP_REQ_RES_LOCK;

}

void HttpCommonRequestResponse::AddToHeaderBuffer(const char *buf, uint16_t len) {
	AddToBuffer(m_request, buf, len);
}

uint16_t HttpCommonRequestResponse::ReadHeader(uint8_t *buf, uint16_t len) {
	NS_LOG_FUNCTION(this);

	if (m_state != HTTP_REQ_RES_LOCK) {
		EndHeader();
	}

	return ReadFromBuffer(m_request, (char*) buf, len);
}


uint16_t HttpCommonRequestResponse::ReadBody(uint8_t *buf, uint16_t len) {
	NS_LOG_FUNCTION(this);

	NS_ASSERT(m_state == HTTP_REQ_RES_READ_BODY);

	return ReadFromBuffer(m_request, (char*) buf, len);
}

bool HttpCommonRequestResponse::IsHeaderReceived() {
	NS_LOG_FUNCTION(this);
	return m_state == HTTP_REQ_RES_READ_BODY;
}

void HttpCommonRequestResponse::ProcessHeaderLine() {
	NS_LOG_FUNCTION(this);
	NS_ASSERT(
			m_state != HTTP_REQ_RES_INVALID && m_state != HTTP_REQ_RES_READ_BODY
					&& m_tmpbuflen >= 2);
	if (m_tmpbuflen == 2) { //Header ended here
		m_state = HTTP_REQ_RES_READ_BODY;
		return;
	}

	std::stringstream ss;
	ss.write((char*) m_tmpbuf, m_tmpbuflen - 1); // I need \r
	if (m_state == HTTP_REQ_RES_PARSE_L1) {
		ProcessFirstHeaderLine(ss);
		m_state = HTTP_REQ_RES_PARSE_HEADER;
		return;
	}

	std::string key, value;
	std::getline(ss, key, ':');
	auto c = ss.get();
	while (c == ' ') {
		c = ss.get();
	};
	ss.unget();
	std::getline(ss, value, '\r');
	NS_LOG_INFO("key: " << key << " <=> value: " << value);
//	std::cout << "orig: " << m_tmpbuflen << " key: " << key << " <=> value: "
//			<< value << std::endl;
	m_headers[key] = value;
}

std::string HttpCommonRequestResponse::GetHeader(std::string name) {
	if(m_headers.find(name) == m_headers.end())
		return std::string();
	return m_headers[name];
}

/****************************************************
 *                                                  *
 ****************************************************/

HttpRequest::HttpRequest(): HttpCommonRequestResponse() {
	NS_LOG_FUNCTION(this);
}

HttpRequest::HttpRequest(std::string method, std::string path,
		std::string version):HttpCommonRequestResponse(HTTP_REQ_RES_INVALID) {
	NS_LOG_FUNCTION(this);
	m_method = method;
	m_path = path;
	m_version = version;
}

HttpRequest::~HttpRequest() {
	NS_LOG_FUNCTION(this);
}

void HttpRequest::ReadFirstHeaderLine(){
	auto sptr = std::string(m_method + " " + m_path + " " + m_version + "\r\n");
	auto ptr = sptr.c_str();
	AddToHeaderBuffer(ptr, std::strlen(ptr));
}

void HttpRequest::ProcessFirstHeaderLine(std::stringstream &ss) {
	ss >> m_method >> m_path >> m_version;
}

const std::string& HttpRequest::getMethod() const {
	return m_method;
}

const std::string& HttpRequest::GetPath() const {
	return m_path;
}

const std::string& HttpRequest::GetVersion() const {
	return m_version;
}

} /* namespace ns3 */

/****************************************************
 *                                                  *
 ****************************************************/

namespace ns3 {

HttpResponse::HttpResponse() : m_statusCode(200) {
	NS_LOG_FUNCTION(this);
}

HttpResponse::HttpResponse(std::string version, uint16_t statusCode,
		std::string statusText) : HttpCommonRequestResponse(HTTP_REQ_RES_INVALID), m_statusCode(statusCode) {
	NS_LOG_FUNCTION(this);

	m_version = version;
	m_statusText = statusText;
}

HttpResponse::~HttpResponse() {
	NS_LOG_FUNCTION(this);
}

void HttpResponse::ReadFirstHeaderLine(){
	auto sptr = std::string(m_version + " " + std::to_string(m_statusCode) + " " + m_statusText + "\r\n");
	auto ptr = sptr.c_str();
	AddToHeaderBuffer(ptr, std::strlen(ptr));
}

void HttpResponse::ProcessFirstHeaderLine(std::stringstream &ss) {
	ss >> m_version >> m_statusCode >> m_statusText;
}

uint8_t HttpResponse::GetStatusCode() const {
	return m_statusCode;
}

const std::string& HttpResponse::GetStatusText() const {
	return m_statusText;
}

const std::string& HttpResponse::GetVersion() const {
	return m_version;
}

void HttpResponse::SetStatusCode(uint16_t mStatusCode) {
	m_statusCode = mStatusCode;
}

void HttpResponse::SetStatusText(const std::string &mStatusText) {
	m_statusText = mStatusText;
}

void HttpResponse::SetVersion(const std::string &mVersion) {
	m_version = mVersion;
}

} /* namespace ns3 */
