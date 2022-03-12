#pragma once
template <typename F>
struct Defer {
    F f;
    Defer(F f) : f(f) { }
    ~Defer() { f(); }
};

struct defer_code {
    template<typename F>
    Defer<F> operator=(F f) {
        return Defer<F>(f);
    }
};

#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x)    DEFER_2(x, __COUNTER__)
#define defer auto DEFER_3(_defer_) = defer_code() = [&]()