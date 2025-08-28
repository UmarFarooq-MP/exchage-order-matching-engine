#include <iostream>
#include <random>
#include <thread>
#include <chrono>

#include "engine/matching_engine.h"
#include "wal/wal_manager.h"

// Dummy broadcaster for now
class ConsoleBroadcaster : public engine::Broadcaster {
public:
    bool publish(const std::string& topic, const nlohmann::json& msg) override {
        std::cout << "[BROADCAST] topic=" << topic
                  << " msg=" << msg.dump() << "\n";
        return true; // simulate success
    }
};

using namespace engine;
using namespace liquibook;

int main() {
    try {
        std::cout << "=== Starting Order Matching Engine with WAL ===\n";

        // --- Init WAL ---
        wal::WalManager wal("db/wal");   // persistent directory

        // --- Init Broadcaster ---
        ConsoleBroadcaster broadcaster;

        // --- Init Engine ---
        MatchingEngine engine("usdtbtc", &wal, &broadcaster);

        // --- RNG for orders ---
        std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> sideDist(0, 1);      // 0=buy, 1=sell
        std::uniform_int_distribution<int> priceDist(95, 105);  // price range
        std::uniform_int_distribution<int> qtyDist(1, 20);      // qty range

        // --- Insert ~200 orders ---
        for (int i = 1; i <= 200; i++) {
            bool isBuy = (sideDist(rng) == 0);
            uint64_t price = priceDist(rng);
            uint64_t qty = qtyDist(rng);

            // 1. Write to WAL-In
            nlohmann::json j = {
                {"side", isBuy ? "BUY" : "SELL"},
                {"price", price},
                {"qty", qty}
            };
            uint64_t seq = wal.appendInbound("add", j);

            // 2. Process in engine
            engine.addOrder(isBuy, price, qty);

            // 3. Mark WAL-Out (processed after publish success)
            wal.markProcessed(seq, j);

            if (i % 50 == 0) {
                std::cout << "--- Inserted " << i << " orders ---\n";
            }
        }

        std::cout << "\n=== Simulate restart (replay from WAL) ===\n";

        // On restart: reload snapshot + replay WAL
        auto inbound = wal.replayInbound();
        for (auto& rec : inbound) {
            if (!wal.isProcessed(rec.id)) {
                std::cout << "[RECOVERY] Replaying order seq=" << rec.id
                          << " payload=" << rec.payload.dump() << "\n";

                if (rec.type == "add") {
                    bool isBuy = (rec.payload["side"] == "BUY");
                    uint64_t price = rec.payload["price"];
                    uint64_t qty = rec.payload["qty"];
                    engine.addOrder(isBuy, price, qty);
                } else if (rec.type == "cancel") {
                    engine.removeOrder(rec.payload["id"]);
                }

                wal.markProcessed(rec.id, rec.payload);
            }
        }

        std::cout << "\n=== Done ===\n";

    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}
