//
// Created by Umar Farooq on 28/08/2025.
//

#include "wal_manager.h"

#include <rocksdb/utilities/checkpoint.h>
#include <rocksdb/status.h>
#include <iostream>

namespace wal {

    using json = nlohmann::json;

    WalManager::WalManager(const std::string& path)
        : dbPath_(path), db_(nullptr), nextSeq_(1),
          cfInbound_(nullptr), cfOutbound_(nullptr), cfSnapshot_(nullptr) {

        rocksdb::Options options;
        options.create_if_missing = true;
        options.create_missing_column_families = true;

        std::vector<std::string> cfNames;
        rocksdb::Status s = rocksdb::DB::ListColumnFamilies(options, path, &cfNames);

        std::vector<rocksdb::ColumnFamilyDescriptor> cfDescriptors;
        if (s.ok() && !cfNames.empty()) {
            for (auto& name : cfNames) {
                cfDescriptors.emplace_back(name, options);
            }
        } else {
            // First time init
            cfDescriptors = {
                {rocksdb::kDefaultColumnFamilyName, options},
                {"in", options},
                {"out", options},
                {"snap", options}
            };
        }

        std::vector<rocksdb::ColumnFamilyHandle*> handles;
        s = rocksdb::DB::Open(options, path, cfDescriptors, &handles, &db_);
        if (!s.ok()) {
            throw std::runtime_error("Failed to open RocksDB: " + s.ToString());
        }
        dbGuard_.reset(db_);

        // assign handles
        for (size_t i = 0; i < handles.size(); i++) {
            if (cfDescriptors[i].name == "in") cfInbound_ = handles[i];
            else if (cfDescriptors[i].name == "out") cfOutbound_ = handles[i];
            else if (cfDescriptors[i].name == "snap") cfSnapshot_ = handles[i];
            else if (cfDescriptors[i].name == rocksdb::kDefaultColumnFamilyName) {
                // ignore default
            }
        }

        // discover nextSeq_
        std::unique_ptr<rocksdb::Iterator> it(db_->NewIterator(rocksdb::ReadOptions(), cfInbound_));
        it->SeekToLast();
        if (it->Valid()) {
            nextSeq_ = std::stoull(it->key().ToString()) + 1;
        }
    }

    WalManager::~WalManager() {
        delete cfInbound_;
        delete cfOutbound_;
        delete cfSnapshot_;
    }

    uint64_t WalManager::appendInbound(const std::string& type, const json& payload) {
        uint64_t seq = nextSeq_++;

        json record = {{"type", type}, {"payload", payload}};
        rocksdb::Status s = db_->Put(rocksdb::WriteOptions(),
                                     cfInbound_,
                                     std::to_string(seq),
                                     record.dump());
        if (!s.ok()) {
            throw std::runtime_error("WAL appendInbound failed: " + s.ToString());
        }
        return seq;
    }

    void WalManager::markProcessed(uint64_t seq, const json& payload) {
        rocksdb::Status s = db_->Put(rocksdb::WriteOptions(),
                                     cfOutbound_,
                                     std::to_string(seq),
                                     payload.dump());
        if (!s.ok()) {
            throw std::runtime_error("WAL markProcessed failed: " + s.ToString());
        }
    }

    std::vector<WalRecord> WalManager::replayInbound(uint64_t fromSeq) {
        std::vector<WalRecord> result;
        std::unique_ptr<rocksdb::Iterator> it(db_->NewIterator(rocksdb::ReadOptions(), cfInbound_));
        for (it->Seek(std::to_string(fromSeq)); it->Valid(); it->Next()) {
            uint64_t seq = std::stoull(it->key().ToString());
            auto val = it->value().ToString();
            json record = json::parse(val);

            WalRecord rec;
            rec.id = seq;
            rec.type = record["type"];
            rec.payload = record["payload"];
            result.push_back(rec);
        }
        return result;
    }

    bool WalManager::isProcessed(uint64_t seq) {
        std::string val;
        auto s = db_->Get(rocksdb::ReadOptions(), cfOutbound_, std::to_string(seq), &val);
        return s.ok();
    }

    void WalManager::saveSnapshot(const std::string& symbol,
                                  const json& snapshot,
                                  uint64_t seq) {
        json snapRecord = {{"seq", seq}, {"snapshot", snapshot}};
        rocksdb::Status s = db_->Put(rocksdb::WriteOptions(),
                                     cfSnapshot_,
                                     symbol,
                                     snapRecord.dump());
        if (!s.ok()) {
            throw std::runtime_error("WAL saveSnapshot failed: " + s.ToString());
        }
    }

    json WalManager::loadSnapshot(const std::string& symbol,
                                  uint64_t& lastSeq) {
        std::string val;
        auto s = db_->Get(rocksdb::ReadOptions(), cfSnapshot_, symbol, &val);
        if (!s.ok()) {
            lastSeq = 0;
            return {};
        }
        json snapRecord = json::parse(val);
        lastSeq = snapRecord["seq"];
        return snapRecord["snapshot"];
    }

} // namespace wal
