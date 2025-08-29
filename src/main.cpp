#include <iostream>
#include <random>
#include <thread>
#include <chrono>

#include "engine/matching_engine.h"
#include "wal/wal_manager.h"

using namespace engine;

int main() {
    {
        std::cout << "=== Starting Order Matching Engine with WAL ===\n";
        wal::WalManager wal("db/wal");
        Broadcaster broadcaster;
        MatchingEngine engine("usdtbtc", &wal, &broadcaster);

        // insert ~100 orders
        for (int i = 0; i < 1000000; ++i) {
            bool isBuy = (rand() % 2 == 0);
            uint64_t price = 95 + rand() % 10;
            uint64_t qty   = 1 + rand() % 20;
            engine.addOrder(isBuy, price, qty);

            if ((i + 1) % 25 == 0) {
                std::cout << "--- Inserted " << (i + 1) << " orders ---\n";
            }
        }

        engine.takeSnapshot();
    } // <-- wal + engine destructed here, RocksDB lock released

    std::cout << "\n=== Simulating restart... ===\n";

    {
        wal::WalManager wal("db/wal");
        Broadcaster broadcaster;
        engine::MatchingEngine engine("usdtbtc", &wal, &broadcaster);

        engine.recover();   // should succeed now
    }

    return 0;
}
