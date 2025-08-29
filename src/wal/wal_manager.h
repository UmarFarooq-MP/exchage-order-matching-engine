#ifndef OME_WAL_MANAGER_H
#define OME_WAL_MANAGER_H

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>
#include <rocksdb/utilities/checkpoint.h>
#include <nlohmann/json.hpp>

#include <string>
#include <vector>
#include <optional>
#include <atomic>

namespace wal {

    struct WalRecord {
        uint64_t id;
        std::string type;
        nlohmann::json payload;
    };

    class WalManager {
    public:
        explicit WalManager(const std::string& path);
        ~WalManager();

        // Write operations
        uint64_t appendInbound(const std::string& type, const nlohmann::json& payload);
        void markProcessed(uint64_t seq, const nlohmann::json& payload);

        // Snapshot
        void saveSnapshot(const std::string& symbol, const nlohmann::json& snapshot, uint64_t seq);
        std::optional<nlohmann::json> loadSnapshot(const std::string& symbol, uint64_t& lastSeq);

        // Recovery
        std::vector<WalRecord> replayInbound(uint64_t from = 1);
        bool isProcessed(uint64_t seq);

    private:
        std::string dbPath_;
        rocksdb::DB* db_{nullptr};
        rocksdb::ColumnFamilyHandle* inboundCF_{nullptr};
        rocksdb::ColumnFamilyHandle* outboundCF_{nullptr};
        rocksdb::ColumnFamilyHandle* snapshotCF_{nullptr};
        std::vector<rocksdb::ColumnFamilyHandle*> handles_;
        std::atomic<uint64_t> seq_{0};
    };

} // namespace wal

#endif // OME_WAL_MANAGER_H
