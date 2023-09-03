// CppTests.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <vector>
#include <map>
#include <unordered_map>
#include <queue>
#include <limits>

#include <iostream>

class Orderbook {
public:
	using OrderId = unsigned long;
	struct Order {
		double avg_price;
		double asking_price;
		double quantity;
		double quantity_filled;
		double quantity_left;
		bool buy;
	};

	double best_bid() {
		if (m_bids.empty()) {
			return 0;
		}

		return m_bids.rbegin()->first;
	}

	inline double best_offer() {
		if (m_offers.empty()) {
			return std::numeric_limits<double>::max();
		}

		return m_offers.begin()->first;
	}

	inline double spread() { 
		return best_offer() - best_bid(); 
	}

	inline double market_price() { 
		return m_last_market_price; 
	}

	OrderId limit_order(bool buy, double price, double quantity) {
		return _create_order(buy, price, quantity, true);
	}

	OrderId market_order(bool buy, double quantity) {
		return _create_order(buy, buy ? best_offer() : best_bid(), quantity, false);
	}

	Order get_order(OrderId order_id) {
		return m_orders[order_id];
	}

	void cancel_order(OrderId order_id) {
		Order& order = m_orders[order_id];
		order.quantity_left = 0;

		auto& order_pair = *(order.buy ? m_bids : m_offers).find(order.avg_price);
		order_pair.second.remove_if([=](OrderId id) { return id == order_id; });
		if (order_pair.second.empty()) {
			(order.buy ? m_bids : m_offers).erase(order.avg_price);
		}
	}
private:
	OrderId _create_order(bool buy, double asking_price, double quantity, bool limit) {
		OrderId order_id = m_orders.size();
		m_orders.push_back(Order());

		Order& order = m_orders.back();
		order.buy = buy;
		order.quantity = quantity;
		order.quantity_left = quantity;
		order.asking_price = asking_price;

		auto& opp_orders = buy ? m_offers : m_bids;
		auto& orders = buy ? m_bids : m_offers;

		while (!opp_orders.empty() && order.quantity_left > 0) {
			auto& [price, best_opp_orders] = buy ? *m_offers.begin() : *m_bids.rbegin();

			if (limit && buy && price > asking_price || limit && !buy && price < asking_price) {
				break;
			}

			while (!best_opp_orders.empty() && order.quantity_left > 0) {
				Order best_opp_order = m_orders[best_opp_orders.front()];

				double to_take = std::min(order.quantity_left, best_opp_order.quantity_left);

				order.quantity_left -= to_take;
				order.quantity_filled += to_take;
				order.avg_price += price * to_take;

				best_opp_order.quantity_left -= to_take;
				best_opp_order.quantity_filled += to_take;

				m_last_market_price = price;

				if (best_opp_order.quantity_left == 0) {
					best_opp_orders.pop_front();
				}
			}

			if (best_opp_orders.empty()) {
				opp_orders.erase(price);
			}
		}

		if (order.quantity_filled > 0) {
			order.avg_price /= order.quantity_filled;
		}

		if (limit) {
			orders[asking_price].push_back(order_id);
		}

		return order_id;
	}

	double m_last_market_price;
	std::map<double, std::list<OrderId>> m_offers;
	std::map<double, std::list<OrderId>> m_bids;
	std::vector<Order> m_orders;
};

int main() {
	Orderbook book;

	book.limit_order(true, 100, 1000);
	book.limit_order(true, 100, 1000);
	book.limit_order(true, 102, 1000);

	book.limit_order(false, 200, 700);
	book.limit_order(false, 201, 600);
	book.limit_order(false, 202, 500);
	
	Orderbook::OrderId orderId = book.market_order(false, 10000);

	Orderbook::Order order = book.get_order(orderId);

	std::cout << "Market price " << book.market_price() << std::endl;
	std::cout << "Avg price filled " << order.avg_price << " quantity filled " << order.quantity_filled << " quantity left " << order.quantity_left << std::endl;
	std::cout << "Best offer " << book.best_offer() << " Best bid " << book.best_bid() << std::endl;
}