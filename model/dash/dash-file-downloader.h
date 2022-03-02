/*
 * dash-file-downloader.h
 *
 *  Created on: 15-Apr-2020
 *      Author: abhijit
 */

#ifndef SRC_SPDASH_MODEL_DASH_DASH_FILE_DOWNLOADER_H_
#define SRC_SPDASH_MODEL_DASH_DASH_FILE_DOWNLOADER_H_

#include "dash-common.h"
#include "ns3/application.h"
#include "ns3/http-client-basic.h"
#include <ns3/internet-module.h>

#include <vector>

namespace ns3 {

class DashFileDownloader: public Application {
public:
	static TypeId GetTypeId(void);
	DashFileDownloader();
	virtual ~DashFileDownloader();

private:
	virtual void StopApplication();
	virtual void StartApplication();

	virtual void EndApplication();

	void StartDash();
	void DownloadNextSegment();
	void Downloaded();
	void DownloadedCB(Ptr<Object> obj);
	void DownloadFailedCB(Ptr<Object> obj);

	void PeriodicDataCollection();

	std::string CollectNodeTrace(Ptr<Node> node, bool firstLine=false);

	void TxTracedCallback(Ptr< const Packet > packet, Ptr< Ipv4 > ipv4, uint32_t interface);
	void RxTracedCallback(Ptr< const Packet > packet, Ptr< Ipv4 > ipv4, uint32_t interface);

	void LogTrace();

	bool m_running;
	Address m_serverAddress;
	uint16_t m_serverPort;
	uint16_t m_numDownloaded;
	uint16_t m_count;
	clen_t m_clen;
	Ptr<HttpClientBasic> m_httpDownloader;
	std::vector<HttpTrace> m_httpTrace;
	std::string m_tracePath;
	std::map<uint32_t, uint64_t> m_rxData;
	std::map<uint32_t, uint64_t> m_txData;


	Time m_timeout;
	int32_t m_traceId;
	std::string m_nodeTracePath;
	std::ofstream m_nodeTraceFp;
	Time m_nodeTraceInterval;
	Time m_nodeTraceLastTime;
	std::map<uint32_t, uint64_t> m_rxNodeTraceLastData;
	std::map<uint32_t, uint64_t> m_txNodeTraceLastData;
	Callback<std::string, Ptr<Node>, bool > m_nodeTraceCB;
	Callback<void> m_onStartClient;
	Callback<void> m_onStopClient;
};

} /* namespace ns3 */

#endif /* SRC_SPDASH_MODEL_DASH_DASH_FILE_DOWNLOADER_H_ */
