// DataStore.h
#pragma once
#include <string>
#include <filesystem>
#include "datastore/RingBuffer.h"
#include "duckdb.hpp"
#include "models/SnapShotData.h"

class DataStore {
private:

    RingBuffer<SnapShotProcData> procsData;
    RingBuffer<SnapShotSysData>  sysData;

    static std::string makeDailyDbPath();

    std::string currentDbPath;
    duckdb::DuckDB     db;
    duckdb::Connection con;
    

    void initDB();

    // timestamp ∫Ø»Ø «Ô∆€ (chrono °Ê microseconds)
    static int64_t toTimestamp(
        const std::chrono::system_clock::time_point& tp) {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            tp.time_since_epoch()).count();
    }

public:
    DataStore(size_t procCap = 120, size_t sysCap = 120);
    ~DataStore() = default;

    void pushProcsData(const SnapShotProcData& data);
    void pushSysData(const SnapShotSysData& data);

    void flushProcsToDB();
    void flushSysToDB();

    std::string queryReport(const std::string& sql);

    
    void checkDailyRotation();

    size_t getProcsSize() const { return procsData.getSize(); }
};