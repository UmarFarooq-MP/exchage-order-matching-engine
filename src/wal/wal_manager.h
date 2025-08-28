//
// Created by Umar Farooq on 28/08/2025.
//

#ifndef OME_WAL_MANAGER_H
#define OME_WAL_MANAGER_H

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <nlohmann/json.hpp>

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace wal {

    // --- WAL record ---
    struct WalRecord {
        uint64_t id;           // sequence number
        std::string type;      // e.g. "add", "cancel", "trade"
        nlohmann::json payload;
    };

    class WalManager {
    public:
        explicit WalManager(const std::string& path);
        ~WalManager();

        WalManager(const WalManager&) = delete;
        WalManager& operator=(const WalManager&) = delete;

        /// Append inbound op before engine processes it
        uint64_t appendInbound(const std::string& type, const nlohmann::json& payload);

        /// Mark record as processed (after engine + broadcast OK)
        void markProcessed(uint64_t seq, const nlohmann::json& payload);

        /// Replay inbound ops starting from last snapshot or from 1
        std::vector<WalRecord> replayInbound(uint64_t fromSeq = 1);

        /// Check if a record has been processed (exists in WAL-Out)
        bool isProcessed(uint64_t seq);

        /// Save order book snapshot at given sequence
        void saveSnapshot(const std::string& symbol,
                          const nlohmann::json& snapshot,
                          uint64_t seq);

        /// Load latest snapshot for symbol
        nlohmann::json loadSnapshot(const std::string& symbol,
                                    uint64_t& lastSeq);

    private:
        std::string dbPath_;
        rocksdb::DB* db_;                // main RocksDB instance
        std::unique_ptr<rocksdb::DB> dbGuard_;

        uint64_t nextSeq_;               // monotonically increasing sequence

        // column families
        rocksdb::ColumnFamilyHandle* cfInbound_;
        rocksdb::ColumnFamilyHandle* cfOutbound_;
        rocksdb::ColumnFamilyHandle* cfSnapshot_;
    };

} // namespace wal

#endif // OME_WAL_MANAGER_H
