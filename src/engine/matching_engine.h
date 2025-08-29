#ifndef OME_MATCHING_ENGINE_H
#define OME_MATCHING_ENGINE_H

#include <book/order_book.h>
#include <simple/simple_order.h>
#include <nlohmann/json.hpp>
#include "../wal/wal_manager.h"
#include <memory>
#include <string>
#include <iostream>

// Minimal Broadcaster stub
class Broadcaster {
public:
    bool publish(const std::string& topic, const nlohmann::json& msg) {
        std::cout << "[BROADCAST] topic=" << topic << " msg=" << msg.dump() << "\n";
        return true;
    }
};

namespace engine {
    class MatchingEngine final
        : public liquibook::book::OrderListener<liquibook::simple::SimpleOrderPtr>,
          public liquibook::book::TradeListener<liquibook::book::OrderBook<liquibook::simple::SimpleOrderPtr>> {

    public:
        MatchingEngine() = delete;
        MatchingEngine(const MatchingEngine&) = delete;
        explicit MatchingEngine(const std::string& symbol, wal::WalManager* wal, Broadcaster* broadcaster);
        virtual ~MatchingEngine() = default;

        void addOrder(bool isBuy, uint64_t price, uint64_t qty, bool fromReplay = false);
        void removeOrder(uint32_t orderId, bool fromReplay = false);

        void takeSnapshot();
        void recover();

        // --- Listener methods ---
        void on_accept(const liquibook::simple::SimpleOrderPtr& order) override;
        void on_reject(const liquibook::simple::SimpleOrderPtr& order, const char* reason) override;
        void on_fill(const liquibook::simple::SimpleOrderPtr& order,
                     const liquibook::simple::SimpleOrderPtr& matched_order,
                     liquibook::book::Quantity qty,
                     liquibook::book::Price price) override;
        void on_cancel(const liquibook::simple::SimpleOrderPtr& order) override;
        void on_cancel_reject(const liquibook::simple::SimpleOrderPtr& order, const char* reason) override;
        void on_replace(const liquibook::simple::SimpleOrderPtr& order,
                        const int64_t& size_delta,
                        liquibook::book::Price new_price) override;
        void on_replace_reject(const liquibook::simple::SimpleOrderPtr& order, const char* reason) override;
        void on_trade(const liquibook::book::OrderBook<liquibook::simple::SimpleOrderPtr>* book,
                      liquibook::book::Quantity qty,
                      liquibook::book::Price price) override;

    private:
        typedef liquibook::book::OrderBook<liquibook::simple::SimpleOrderPtr> OrderBookT;
        typedef liquibook::simple::SimpleOrderPtr OrderPtr;

        OrderBookT orderBook_;
        wal::WalManager* wal_;
        Broadcaster* broadcaster_;

        uint64_t processedCount_{0};
    };
}

#endif // OME_MATCHING_ENGINE_H
