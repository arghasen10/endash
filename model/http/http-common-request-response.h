/*
 * http-request-response.h
 *
 *  Created on: 04-Apr-2020
 *      Author: abhijit
 */

#ifndef SRC_SPDASH_MODEL_HTTP_REQUEST_RESPONSE_H_
#define SRC_SPDASH_MODEL_HTTP_REQUEST_RESPONSE_H_

#include "ns3/object.h"
#include "http-common.h"
#include <map>
#include <queue>

enum HttReqResState{
	HTTP_REQ_RES_PARSE_L1,
	HTTP_REQ_RES_PARSE_HEADER,
	HTTP_REQ_RES_READ_BODY,
	HTTP_REQ_RES_LOCK,
	HTTP_REQ_RES_INVALID //during creation
};


namespace ns3 {

struct CaseInsensitiveCompare{
  bool operator()(const std::string &a, const std::string &b){return false;};
};

class HttpCommonRequestResponse: public Object {
public:
	HttpCommonRequestResponse(HttReqResState hrrs = HTTP_REQ_RES_PARSE_L1);
	virtual ~HttpCommonRequestResponse();
	virtual void ParseHeader(uint8_t *buf, uint16_t len);
	virtual void AddHeader(std::string key, std::string value);
	virtual uint16_t ReadHeader(uint8_t *buf, uint16_t len);
	virtual uint16_t ReadBody(uint8_t *buf, uint16_t len);
	virtual bool IsHeaderReceived();
	virtual void EndHeader();
	virtual std::string GetHeader(std::string name);
protected:
	virtual void ProcessFirstHeaderLine(std::stringstream &) = 0;
	virtual void ReadFirstHeaderLine() = 0;
	virtual void AddToHeaderBuffer(const char *buf, uint16_t len);
private:
	void ProcessHeaderLine();

	uint8_t m_tmpbuf[2048]; //I do not expect a line to go beyond
	uint16_t m_tmpbuflen;
	HttReqResState m_state;
	std::queue<std::pair<char *, uint16_t> > m_body;
	std::queue<std::pair<char *, uint16_t> > m_request;

	//=============
	std::map<std::string, std::string> m_headers;

};
}

/****************************************************
 *                                                  *
 ****************************************************/

namespace ns3 {

class HttpRequest: public HttpCommonRequestResponse {
public:
	HttpRequest();
	HttpRequest(std::string method, std::string path, std::string version);
	virtual ~HttpRequest();
	const std::string& getMethod() const;
	const std::string& GetPath() const;
	const std::string& GetVersion() const;

protected:
	virtual void ProcessFirstHeaderLine(std::stringstream &ss);
	virtual void ReadFirstHeaderLine();
private:

	std::string m_method, m_path, m_version;
};


} /* namespace ns3 */


/****************************************************
 *                                                  *
 ****************************************************/


namespace ns3 {

class HttpResponse: public HttpCommonRequestResponse {
public:
	HttpResponse();
	HttpResponse(std::string version, uint16_t statusCode, std::string statusText);
	~HttpResponse();


	uint8_t GetStatusCode() const;
	const std::string& GetStatusText() const;
	const std::string& GetVersion() const;
	void SetStatusCode(uint16_t mStatusCode);
	void SetStatusText(const std::string &mStatusText);
	void SetVersion(const std::string &mVersion);

protected:
	virtual void ProcessFirstHeaderLine(std::stringstream &ss);
	virtual void ReadFirstHeaderLine();
private:
//==============================================
	std::string m_version;
	uint16_t m_statusCode;
	std::string m_statusText;
};

} /* namespace ns3 */
#endif /* SRC_SPDASH_MODEL_HTTP_REQUEST_RESPONSE_H_ */

