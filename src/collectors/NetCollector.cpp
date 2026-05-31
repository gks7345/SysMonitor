#include "collectors/NetCollector.h"

void NetCollector::init(PDH_HQUERY& query) {
	PDH_STATUS sentStatus = PdhAddEnglishCounterW(
		query,
		L"\\Network Interface(*)\\Bytes Sent/sec",
		0,
		&sentCounter);
	if (sentStatus != ERROR_SUCCESS) {
		spdlog::error("Counter Error 0x{:X}", sentStatus);
	}

	PDH_STATUS recvStatus = PdhAddEnglishCounterW(
		query,
		L"\\Network Interface(*)\\Bytes Received/sec",
		0,
		&recvCounter);
	if (recvStatus != ERROR_SUCCESS) {
		spdlog::error("Counter Error 0x{:X}", recvStatus);
	}
}

double NetCollector::getSentKbps() const {
	DWORD bufSize = 0, count = 0;

	PdhGetFormattedCounterArray(sentCounter,
		PDH_FMT_DOUBLE,
		&bufSize,
		&count,
		nullptr);

	if (count == 0 || bufSize) return 0.0;

	std::vector<char> buf(bufSize);

	PDH_STATUS status = PdhGetFormattedCounterArray(sentCounter, PDH_FMT_DOUBLE,
		&bufSize, &count,
		reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM*>(buf.data()));

	if (status != ERROR_SUCCESS) return 0.0;

	auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM*>(buf.data());

	double total = 0.0;
	for (DWORD i = 0; i < count; ++i)
	{
		if (items[i].FmtValue.CStatus != PDH_CSTATUS_VALID_DATA &&
			items[i].FmtValue.CStatus != PDH_CSTATUS_NEW_DATA) {
			continue;
		}

		total += items[i].FmtValue.doubleValue;
	}

	double sentNetKbos = total * 8.0 / 1'000.0;
	return sentNetKbos;
}

double NetCollector::getRecvKbps() const {
	DWORD bufSize = 0, count = 0;

	PdhGetFormattedCounterArray(recvCounter,
		PDH_FMT_DOUBLE,
		&bufSize,
		&count,
		nullptr);

	if (count == 0 || bufSize) return 0.0;

	std::vector<char> buf(bufSize);

	PDH_STATUS status = PdhGetFormattedCounterArray(recvCounter, PDH_FMT_DOUBLE,
		&bufSize, &count,
		reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM*>(buf.data()));

	if (status != ERROR_SUCCESS) return 0.0;

	auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM*>(buf.data());

	double total = 0.0;
	for (DWORD i = 0; i < count; ++i)
	{
		if (items[i].FmtValue.CStatus != PDH_CSTATUS_VALID_DATA &&
			items[i].FmtValue.CStatus != PDH_CSTATUS_NEW_DATA)
			continue;

		total += items[i].FmtValue.doubleValue;
	}

	double getRecvKB = total * 8.0 / 1'000.0;
	return getRecvKB;
}

std::string NetCollector::formatNetSpeed(double kbps) {
	if (kbps >= 1000.0)
		return std::format("{:.1f} Mbps", kbps / 1000.0);
	else
		return std::format("{:.1f} Kbps", kbps);
}