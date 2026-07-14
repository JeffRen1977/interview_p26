// SharedPtr — 手写共享所有权智能指针（引用计数版）。
//
// Whiteboard talking points:
// - 两个指针成员：被管理对象 m_ptr + 堆上的引用计数 m_ref_count。
//   计数必须在堆上：所有副本共享同一个计数器，任何一方增减都对全体可见。
// - Rule of Five：析构 / 拷贝构造 / 拷贝赋值 / 移动构造 / 移动赋值 缺一不可。
//   拷贝 = 共享所有权（计数 +1）；移动 = 转移所有权（计数不变，源置空）。
// - release() 是唯一的减计数出口：计数归零时同时 delete 对象和计数器。
// - 拷贝/移动赋值都要先 release() 旧资源，并防自赋值（if (this != &other)）。
// - 与 std::shared_ptr 的差距（面试主动说）：
//   1) 线程安全：std 用 atomic 计数（本版用普通 uint32_t，多线程会 race）；
//   2) 控制块：std 把 ref_count/weak_count/deleter 合并成一个 control block，
//      make_shared 还会把对象和控制块一次分配（省一次 new、cache 友好）；
//   3) weak_ptr：打破循环引用（A→B→A 时普通 shared 计数永不归零 → 泄漏）；
//   4) 自定义 deleter / allocator、aliasing constructor 等。

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>

template <typename T>
class SharedPtr {
 private:
    T* m_ptr = nullptr;
    // 引用计数放在堆上：所有拷贝共享同一个 uint32_t。
    // 若做成成员变量（每个对象一份），拷贝之间就无法同步计数。
    uint32_t* m_ref_count = nullptr;

    // 唯一的“放手”路径：减计数；归零则销毁对象和计数器。
    // 析构、拷贝赋值、移动赋值都复用它，保证逻辑只写一遍。
    void release() {
        if (m_ref_count) {
            (*m_ref_count)--;
            if (*m_ref_count == 0) {
                delete m_ptr;        // 先销毁被管理对象
                delete m_ref_count;  // 再销毁计数器本身
            }
        }
        // 注意：release() 后 m_ptr/m_ref_count 是悬垂的，
        // 调用方（赋值运算符）必须立刻覆盖它们；析构则无所谓。
    }

 public:
    // explicit：禁止 SharedPtr<int> p = raw; 这种隐式转换，
    // 避免同一裸指针被两个独立的 SharedPtr 接管（double free）。
    explicit SharedPtr(T* ptr = nullptr) : m_ptr(ptr) {
        if (m_ptr) {
            m_ref_count = new uint32_t(1);  // 第一个持有者，计数 = 1
        }
        // ptr == nullptr 时不分配计数器：空指针无需跟踪
    }

    ~SharedPtr() {
        release();
    }

    // ---- 拷贝语义：共享所有权 ----
    // 拷贝构造：指向同一对象、同一计数器，计数 +1。
    SharedPtr(const SharedPtr& other) : m_ptr(other.m_ptr), m_ref_count(other.m_ref_count) {
        if (m_ref_count) {
            (*m_ref_count)++;
        }
    }

    // 拷贝赋值：先放开自己原来持有的资源，再共享 other 的。
    SharedPtr& operator=(const SharedPtr& other) {
        if (this != &other) {   // 防自赋值：否则 release() 可能把资源提前删掉
            release();          // 放开旧资源（可能触发旧对象销毁）
            m_ptr = other.m_ptr;
            m_ref_count = other.m_ref_count;
            if (m_ref_count) {
                (*m_ref_count)++;
            }
        }
        return *this;
    }

    // ---- 移动语义：转移所有权，计数不变 ----
    // noexcept 很重要：容器（如 vector 扩容）只有在移动构造 noexcept 时
    // 才敢用移动代替拷贝。
    SharedPtr(SharedPtr&& other) noexcept
        : m_ptr(other.m_ptr), m_ref_count(other.m_ref_count) {
        // 源对象置空：它不再拥有任何东西，析构时 release() 走 no-op 分支
        other.m_ptr = nullptr;
        other.m_ref_count = nullptr;
    }

    SharedPtr& operator=(SharedPtr&& other) noexcept {
        if (this != &other) {
            release();  // 自己原来的份额要先归还
            m_ptr = other.m_ptr;
            m_ref_count = other.m_ref_count;
            other.m_ptr = nullptr;
            other.m_ref_count = nullptr;
        }
        return *this;
    }

    // ---- 指针操作符 ----
    // const 成员：不改变 SharedPtr 本身（改的是指向的对象）。
    T& operator*() const { return *m_ptr; }
    T* operator->() const { return m_ptr; }

    // ---- 工具函数 ----
    uint32_t use_count() const {
        return m_ref_count ? *m_ref_count : 0;
    }

    T* get() const { return m_ptr; }

    // 支持 if (sp) 判空
    explicit operator bool() const { return m_ptr != nullptr; }

    // 放弃当前资源，可选接管新裸指针（语义同 std::shared_ptr::reset）
    void reset(T* ptr = nullptr) {
        release();
        m_ptr = ptr;
        m_ref_count = ptr ? new uint32_t(1) : nullptr;
    }
};


// ---------------------------------------------------------------------------
// 测试：用一个能记录构造/析构次数的类型验证生命周期正确性
// ---------------------------------------------------------------------------

struct Tracked {
    static int alive;   // 当前存活实例数
    std::string name;

    explicit Tracked(std::string n) : name(std::move(n)) { ++alive; }
    ~Tracked() { --alive; }
};

int Tracked::alive = 0;

static void test_basic_lifetime() {
    assert(Tracked::alive == 0);
    {
        SharedPtr<Tracked> p(new Tracked("a"));
        assert(p.use_count() == 1);
        assert(Tracked::alive == 1);
        assert(p->name == "a");
        assert((*p).name == "a");
    }
    // 离开作用域：唯一持有者析构 → 对象销毁
    assert(Tracked::alive == 0);
}

static void test_copy_shares_ownership() {
    SharedPtr<Tracked> p1(new Tracked("shared"));
    {
        SharedPtr<Tracked> p2(p1);   // 拷贝构造：计数 1 → 2
        assert(p1.use_count() == 2);
        assert(p2.use_count() == 2);
        assert(p1.get() == p2.get());  // 指向同一对象

        SharedPtr<Tracked> p3(new Tracked("other"));
        assert(Tracked::alive == 2);
        p3 = p1;                     // 拷贝赋值：p3 放开 "other"（销毁），共享 "shared"
        assert(Tracked::alive == 1);
        assert(p1.use_count() == 3);
    }
    // p2/p3 析构：计数 3 → 1，对象仍存活
    assert(p1.use_count() == 1);
    assert(Tracked::alive == 1);
}

static void test_move_transfers_ownership() {
    SharedPtr<Tracked> p1(new Tracked("movable"));
    SharedPtr<Tracked> p2(std::move(p1));   // 移动构造：计数不变，p1 置空
    assert(p2.use_count() == 1);
    assert(p1.use_count() == 0);
    assert(p1.get() == nullptr);
    assert(!p1);
    assert(static_cast<bool>(p2));

    SharedPtr<Tracked> p3(new Tracked("victim"));
    assert(Tracked::alive == 2);
    p3 = std::move(p2);                     // 移动赋值：p3 原资源销毁，接管 movable
    assert(Tracked::alive == 1);
    assert(p3.use_count() == 1);
    assert(p2.get() == nullptr);
}

static void test_self_assignment() {
    SharedPtr<Tracked> p(new Tracked("self"));
    SharedPtr<Tracked>& alias = p;          // 通过引用别名绕开编译器的自赋值告警
    p = alias;                              // 自赋值：必须不销毁资源
    assert(p.use_count() == 1);
    assert(Tracked::alive == 1);
    assert(p->name == "self");
}

static void test_null_and_reset() {
    SharedPtr<Tracked> empty;               // 默认构造：空
    assert(empty.use_count() == 0);
    assert(!empty);

    SharedPtr<Tracked> p(new Tracked("x"));
    p.reset(new Tracked("y"));              // reset：旧对象销毁，接管新对象
    assert(Tracked::alive == 1);
    assert(p->name == "y");
    p.reset();                              // reset()：放开一切
    assert(Tracked::alive == 0);
    assert(p.use_count() == 0);
}

int main() {
    test_basic_lifetime();
    test_copy_shares_ownership();
    assert(Tracked::alive == 0);
    test_move_transfers_ownership();
    assert(Tracked::alive == 0);
    test_self_assignment();
    assert(Tracked::alive == 0);
    test_null_and_reset();
    assert(Tracked::alive == 0);

    std::cout << "shared_ptr: all tests passed\n";
    return 0;
}
