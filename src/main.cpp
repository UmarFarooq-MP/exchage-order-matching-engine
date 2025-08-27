#include "book/order_book.h"
#include "simple/simple_order.h"
#include "simple/simple_order_book.h"
#include <iostream>

using namespace liquibook;

int main() {
    // Create a simple order book for one symbol
    book::OrderBook<simple::SimpleOrder*> order_book("AAPL");

    // Create two simple orders (buy and sell)
    simple::SimpleOrder* buy_order = new simple::SimpleOrder("BUY1", true, 100, 10); // buy 10 @ 100
    simple::SimpleOrder* sell_order =
        new simple::SimpleOrder("SELL1", false, 95, 10); // sell 10 @ 95

    // Add them to the order book
    order_book.add(buy_order);
    order_book.add(sell_order);

    // Print results using correct API
    std::cout << "Buy order: " << buy_order->order_id() << " filled: " << buy_order->filled_qty()
              << "\n";

    std::cout << "Sell order: " << sell_order->order_id() << " filled: " << sell_order->filled_qty()
              << "\n";

    delete buy_order;
    delete sell_order;

    return 0;
}
