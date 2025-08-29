#include "wal_manager.h"
#include <rocksdb/utilities/options_util.h>
#include <iostream>

namespace wal {

    // wal_manager.cpp
    WalManager::WalManager(const std::string& path) : dbPath_(path) {
        rocksdb::Options options;
        options.create_if_missing = true;
        options.create_missing_column_families = true;

        const std::vector<rocksdb::ColumnFamilyDescriptor> cfDescriptors = {
            {rocksdb::kDefaultColumnFamilyName, rocksdb::ColumnFamilyOptions()},
            {"in", rocksdb::ColumnFamilyOptions()},
            {"out", rocksdb::ColumnFamilyOptions()},
            {"snap", rocksdb::ColumnFamilyOptions()}
        };

        rocksdb::Status s = rocksdb::DB::Open(options, dbPath_, cfDescriptors, &handles_, &db_);
        if (!s.ok()) {
            throw std::runtime_error("Failed to open RocksDB: " + s.ToString());
        }

        inboundCF_  = handles_[1];
        outboundCF_ = handles_[2];
        snapshotCF_ = handles_[3];

        std::cout << "[WAL] Opened RocksDB at " << dbPath_ << "\n";
    }

    WalManager::~WalManager() {
        if (db_) {
            for (auto* h : handles_) {
                db_->DestroyColumnFamilyHandle(h);
            }
            handles_.clear();
            delete db_;
            db_ = nullptr;
        }
    }


uint64_t WalManager::appendInbound(const std::string& type, const nlohmann::json& payload) {
    uint64_t id = ++seq_;
    const nlohmann::json record = {{"id", id}, {"type", type}, {"payload", payload}};
    const std::string key = std::to_string(id);
    auto s = db_->Put(rocksdb::WriteOptions(), inboundCF_, key, record.dump());
    if (!s.ok()) throw std::runtime_error("appendInbound failed: " + s.ToString());
    return id;
}

void WalManager::markProcessed(const uint64_t seq, const nlohmann::json& payload) const {
    const std::string key = std::to_string(seq);
    auto s = db_->Put(rocksdb::WriteOptions(), outboundCF_, key, payload.dump());
    if (!s.ok()) throw std::runtime_error("markProcessed failed: " + s.ToString());
}

void WalManager::saveSnapshot(const std::string& symbol,
                              const nlohmann::json& snapshot,
                              const uint64_t seq) const {
    const std::string key = symbol + ":" + std::to_string(seq);
    auto s = db_->Put(rocksdb::WriteOptions(), snapshotCF_, key, snapshot.dump());
    if (!s.ok()) throw std::runtime_error("saveSnapshot failed: " + s.ToString());
}

std::optional<nlohmann::json> WalManager::loadSnapshot(const std::string& symbol,
                                                       uint64_t& lastSeq) const {
    const std::unique_ptr<rocksdb::Iterator> it(db_->NewIterator(rocksdb::ReadOptions(), snapshotCF_));
    const std::string prefix = symbol + ":";
    it->Seek(prefix);
    nlohmann::json result;
    bool found = false;
    for (; it->Valid(); it->Next()) {
        if (it->key().starts_with(prefix)) {
            found = true;
            lastSeq = std::stoull(it->key().ToString().substr(prefix.size()));
            result = nlohmann::json::parse(it->value().ToString());
        }
    }
    return found ? std::optional<nlohmann::json>(result) : std::nullopt;
}

std::vector<WalRecord> WalManager::replayInbound(const uint64_t from) const {
    std::vector<WalRecord> records;
    const std::unique_ptr<rocksdb::Iterator> it(db_->NewIterator(rocksdb::ReadOptions(), inboundCF_));
    it->Seek(std::to_string(from));
    for (; it->Valid(); it->Next()) {
        auto val = nlohmann::json::parse(it->value().ToString());
        WalRecord rec{val["id"], val["type"], val["payload"]};
        records.push_back(std::move(rec));
    }
    return records;
}

bool WalManager::isProcessed(const uint64_t seq) const {
    std::string val;
    auto s = db_->Get(rocksdb::ReadOptions(), outboundCF_, std::to_string(seq), &val);
    return s.ok();
}

} // namespace wal
