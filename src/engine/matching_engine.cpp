#include "matching_engine.h"

namespace engine {

    using namespace liquibook;

    MatchingEngine::MatchingEngine(const std::string& symbol, wal::WalManager* wal, Broadcaster* broadcaster)
        : orderBook_(symbol), wal_(wal), broadcaster_(broadcaster) {
        orderBook_.set_order_listener(this);
        orderBook_.set_trade_listener(this);
    }

    void MatchingEngine::addOrder(bool isBuy, uint64_t price, uint64_t qty, bool fromReplay) {
        auto order = std::make_shared<simple::SimpleOrder>(isBuy, price, qty);

        if (!fromReplay) {
            nlohmann::json payload = {
                {"id", order->order_id()},
                {"side", isBuy ? "BUY" : "SELL"},
                {"price", price},
                {"qty", qty}
            };
            uint64_t seq = wal_->appendInbound("add", payload);
            std::cout << "[ENGINE] Adding order " << order->order_id()
                      << " (" << (isBuy ? "BUY" : "SELL")
                      << " qty=" << qty << " @ price=" << price
                      << " seq=" << seq << ")\n";
        }
        orderBook_.add(order);
    }

    void MatchingEngine::removeOrder(uint32_t orderId, bool fromReplay) {
        auto findOrder = [&](const auto& container) -> simple::SimpleOrderPtr {
            for (const auto& entry : container) {
                if (entry.second.ptr()->order_id() == orderId) {
                    return entry.second.ptr();
                }
            }
            return nullptr;
        };

        auto order = findOrder(orderBook_.bids());
        if (!order) order = findOrder(orderBook_.asks());

        if (order) {
            if (!fromReplay) {
                nlohmann::json payload = {{"id", orderId}};
                uint64_t seq = wal_->appendInbound("cancel", payload);
                std::cout << "[ENGINE] Canceling order " << orderId << " seq=" << seq << "\n";
            }
            orderBook_.cancel(order);
        } else {
            std::cout << "[ENGINE] Order " << orderId << " not found\n";
        }
    }

    void MatchingEngine::takeSnapshot() {
        nlohmann::json snapshot;
        snapshot["bids"] = nlohmann::json::array();
        snapshot["asks"] = nlohmann::json::array();

        for (const auto& entry : orderBook_.bids()) {
            snapshot["bids"].push_back({
                {"orderId", entry.second.ptr()->order_id()},
                {"price", entry.second.ptr()->price()},
                {"qty", entry.second.ptr()->order_qty()}
            });
        }
        for (const auto& entry : orderBook_.asks()) {
            snapshot["asks"].push_back({
                {"orderId", entry.second.ptr()->order_id()},
                {"price", entry.second.ptr()->price()},
                {"qty", entry.second.ptr()->order_qty()}
            });
        }

        wal_->saveSnapshot(orderBook_.symbol(), snapshot, processedCount_);
        std::cout << "[SNAPSHOT] Saved at seq=" << processedCount_ << "\n";
    }

    // --- Listeners ---
    void MatchingEngine::on_accept(const simple::SimpleOrderPtr& order) {
        std::cout << "[LISTENER] Order " << order->order_id() << " accepted\n";
    }

    void MatchingEngine::on_reject(const simple::SimpleOrderPtr& order, const char* reason) {
        std::cout << "[LISTENER] Order " << order->order_id() << " rejected: " << reason << "\n";
    }

    void MatchingEngine::on_fill(const simple::SimpleOrderPtr& order,
                                 const simple::SimpleOrderPtr& matched_order,
                                 book::Quantity qty,
                                 book::Price price) {
        nlohmann::json trade = {
            {"orderId", order->order_id()},
            {"matchedId", matched_order->order_id()},
            {"qty", qty},
            {"price", price}
        };

        if (broadcaster_->publish("trades", trade)) {
            processedCount_++;
            wal_->markProcessed(processedCount_, trade);

            if (processedCount_ % 1000 == 0) {
                takeSnapshot();
            }
        }
    }

    void MatchingEngine::on_cancel(const simple::SimpleOrderPtr& order) {
        std::cout << "[LISTENER] Order " << order->order_id() << " canceled\n";
    }

    void MatchingEngine::on_cancel_reject(const simple::SimpleOrderPtr& order, const char* reason) {
        std::cout << "[LISTENER] Cancel reject for " << order->order_id()
                  << " reason=" << reason << "\n";
    }

    void MatchingEngine::on_replace(const simple::SimpleOrderPtr& order,
                                    const int64_t& size_delta,
                                    book::Price new_price) {
        std::cout << "[LISTENER] Order " << order->order_id()
                  << " replaced size_delta=" << size_delta
                  << " new_price=" << new_price << "\n";
    }

    void MatchingEngine::on_replace_reject(const simple::SimpleOrderPtr& order, const char* reason) {
        std::cout << "[LISTENER] Replace rejected for " << order->order_id()
                  << " reason=" << reason << "\n";
    }

    void MatchingEngine::on_trade(const book::OrderBook<simple::SimpleOrderPtr>* book,
                                  book::Quantity qty,
                                  book::Price price) {
        std::cout << "[TRADE] Executed qty=" << qty
                  << " @ " << price
                  << " on " << book->symbol() << "\n";
    }

    void MatchingEngine::recover() {
        uint64_t lastSnapshotSeq = 0;
        auto snapshot = wal_->loadSnapshot(orderBook_.symbol(), lastSnapshotSeq);

        if (!snapshot->empty()) {
            std::cout << "[RECOVERY] Restored snapshot seq=" << lastSnapshotSeq << "\n";
            for (const auto& bid : snapshot.value()["bids"]) {
                auto order = std::make_shared<simple::SimpleOrder>(true, bid["price"], bid["qty"]);
                orderBook_.add(order);
            }
            for (const auto& ask : snapshot.value()["asks"]) {
                auto order = std::make_shared<simple::SimpleOrder>(false, ask["price"], ask["qty"]);
                orderBook_.add(order);
            }
        } else {
            std::cout << "[RECOVERY] No snapshot, starting fresh\n";
        }

        auto entries = wal_->replayInbound(lastSnapshotSeq + 1);
        for (auto& rec : entries) {
            if (wal_->isProcessed(rec.id)) continue;

            if (rec.type == "add") {
                bool isBuy = (rec.payload["side"] == "BUY");
                addOrder(isBuy, rec.payload["price"], rec.payload["qty"], true);
            } else if (rec.type == "cancel") {
                removeOrder(rec.payload["id"], true);
            }
        }
        std::cout << "[RECOVERY] Replay complete\n";
    }

} // namespace engine
