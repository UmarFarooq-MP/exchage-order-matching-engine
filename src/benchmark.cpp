#include "book/order_book.h"
#include "simple/simple_order.h"
#include <chrono>
#include <iostream>
#include <memory>
#include <random>

using namespace liquibook;

class BenchListener : public book::OrderListener<simple::SimpleOrderPtr> {
  public:
    void on_accept(const simple::SimpleOrderPtr& order) override {
        // std::cout << "Order " << order->order_id() << " accepted\n";
    }
    void on_reject(const simple::SimpleOrderPtr& order, const char* reason) override {
        std::cerr << "Order " << order->order_id() << " rejected: " << reason << "\n";
    }
    void on_fill(
        const simple::SimpleOrderPtr& order,
        const simple::SimpleOrderPtr& matched_order,
        book::Quantity fill_qty,
        book::Price fill_price) override {
        trades_++;
        filled_qty_ += fill_qty;
    }
    void on_cancel(const simple::SimpleOrderPtr&) override {}
    void on_cancel_reject(const simple::SimpleOrderPtr&, const char*) override {}
    void on_replace(const simple::SimpleOrderPtr&, const int64_t&, book::Price) override {}
    void on_replace_reject(const simple::SimpleOrderPtr&, const char*) override {}

    size_t trades() const {
        return trades_;
    }
    book::Quantity filled_qty() const {
        return filled_qty_;
    }

  private:
    size_t trades_ = 0;
    book::Quantity filled_qty_ = 0;
};

class DummyTradeListener : public book::TradeListener<book::OrderBook<simple::SimpleOrderPtr>> {
  public:
    void on_trade(
        const book::OrderBook<simple::SimpleOrderPtr>*,
        book::Quantity qty,
        book::Price price) override {
        // We can track trade-level stats here if needed
        (void) qty;
        (void) price;
    }
};

int main() {
    using OrderPtr = simple::SimpleOrderPtr;
    BenchListener listener;
    DummyTradeListener trade_listener;

    book::OrderBook<OrderPtr> order_book("BENCH");
    order_book.set_order_listener(&listener);
    order_book.set_trade_listener(&trade_listener);

    // RNG setup
    std::mt19937_64 rng(42);                                // fixed seed for reproducibility
    std::uniform_int_distribution<int> side_dist(0, 1);     // buy/sell
    std::uniform_int_distribution<int> price_dist(95, 105); // price range
    std::uniform_int_distribution<int> qty_dist(1, 10);     // qty range

    const size_t NUM_ORDERS = 100000;

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < NUM_ORDERS; ++i) {
        bool is_buy = side_dist(rng);
        auto price = price_dist(rng);
        auto qty = qty_dist(rng);

        auto order = std::make_shared<simple::SimpleOrder>(is_buy, price, qty);
        order_book.add(order);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double seconds = std::chrono::duration<double>(end - start).count();

    std::cout << "=== Benchmark Results ===\n";
    std::cout << "Orders processed: " << NUM_ORDERS << "\n";
    std::cout << "Trades executed: " << listener.trades() << "\n";
    std::cout << "Total filled qty: " << listener.filled_qty() << "\n";
    std::cout << "Elapsed time: " << seconds << " sec\n";
    std::cout << "Throughput: " << NUM_ORDERS / seconds << " orders/sec\n";

    return 0;
}
