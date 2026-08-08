#include <iostream>
#include <string>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <cassert>
#include <vector>
#include <stdexcept>

// 订单状态枚举
enum class OrderStatus {
    PENDING,
    RESERVED,
    CANCELLED
};

// 状态转字符串辅助函数
std::string statusToString(OrderStatus status) {
    switch (status) {
        case OrderStatus::PENDING:   return "PENDING";
        case OrderStatus::RESERVED:  return "RESERVED";
        case OrderStatus::CANCELLED: return "CANCELLED";
    }
    return "UNKNOWN";
}

// 订单结构体（包含版本号用于乐观锁/原子更新）
struct Order {
    std::string order_id;
    OrderStatus status = OrderStatus::PENDING;
    int version = 0; // 版本号，解决并发更新写回旧值问题
};

// 幂等结果记录
struct ReserveResult {
    bool success;
    std::string message;
};

class OrderRoutingService {
private:
    std::mutex mtx; // 模拟分布式数据库事务锁或内存并发锁
    int stock_quantity;
    
    std::unordered_map<std::string, Order> db_orders;
    std::unordered_map<std::string, ReserveResult> idempotency_store;

public:
    explicit OrderRoutingService(int initial_stock) : stock_quantity(initial_stock) {}

    // 1. 预定扣减（带 idempotency_key 和 乐观锁控制）
    ReserveResult reserve(const std::string& idempotency_key, const std::string& order_id) {
        std::lock_guard<std::mutex> lock(mtx);

        // 【检查 1：幂等性校验】如果 idempotency_key 已处理，直接返回首次结果，不重复扣库存
        if (idempotency_store.find(idempotency_key) != idempotency_store.end()) {
            return idempotency_store[idempotency_key];
        }

        // 初始化或获取订单
        if (db_orders.find(order_id) == db_orders.end()) {
            db_orders[order_id] = Order{order_id, OrderStatus::PENDING, 0};
        }
        Order current_order = db_orders[order_id];

        // 【检查 2：状态机校验】合法转换：仅允许 PENDING -> RESERVED
        if (current_order.status == OrderStatus::CANCELLED) {
            ReserveResult res{false, "Order is already CANCELLED"};
            idempotency_store[idempotency_key] = res;
            return res;
        }
        if (current_order.status == OrderStatus::RESERVED) {
            ReserveResult res{true, "Order is already RESERVED"};
            idempotency_store[idempotency_key] = res;
            return res;
        }

        // 【检查 3：库存校验与写状态原子更新】
        if (stock_quantity <= 0) {
            ReserveResult res{false, "Insufficient stock"};
            idempotency_store[idempotency_key] = res; // 扣减失败也记录幂等结果
            return res;
        }

        // 模拟乐观锁更新：核对 version 是否未被更改
        if (db_orders[order_id].version != current_order.version) {
            // 版本冲突，执行重试或回滚
            ReserveResult res{false, "Concurrent modification conflict, retry needed"};
            return res;
        }

        // 执行原子操作：扣库存 + 变更状态 + 版本号加1
        stock_quantity--;
        db_orders[order_id].status = OrderStatus::RESERVED;
        db_orders[order_id].version++;

        ReserveResult success_res{true, "Reserved successfully"};
        // 绑定 key 与 最终结果
        idempotency_store[idempotency_key] = success_res;
        return success_res;
    }

    // 2. 取消订单（支持并发操作与状态校验）
    bool cancel(const std::string& order_id) {
        std::lock_guard<std::mutex> lock(mtx);

        if (db_orders.find(order_id) == db_orders.end()) {
            db_orders[order_id] = Order{order_id, OrderStatus::PENDING, 0};
        }

        Order& order = db_orders[order_id];

        // 如果已经是 CANCELLED，无需重复取消
        if (order.status == OrderStatus::CANCELLED) {
            return true;
        }

        // 如果之前成功 RESERVED，取消时需退还库存
        if (order.status == OrderStatus::RESERVED) {
            stock_quantity++;
        }

        // 状态变更为 CANCELLED，更新版本号
        order.status = OrderStatus::CANCELLED;
        order.version++;
        return true;
    }

    // 辅助查询方法
    int getStock() {
        std::lock_guard<std::mutex> lock(mtx);
        return stock_quantity;
    }

    OrderStatus getOrderStatus(const std::string& order_id) {
        std::lock_guard<std::mutex> lock(mtx);
        return db_orders[order_id].status;
    }
};
