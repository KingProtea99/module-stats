// Stats module tests: the fixed evaluation order, clamping, and bit-stable integer results.
#include "stats/sim/Stats.h"

#include "mealkit/core/Hash.h"

#include <cstdio>

static int g_failed = 0;
#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            std::printf("%s(%d): FAIL %s\n", __FILE__, __LINE__, #x);                                                  \
            ++g_failed;                                                                                                \
        }                                                                                                              \
    } while (0)

using namespace stats;

int main() {
    Stats s;
    define(s, 0, fromWhole(100), 0, fromWhole(500)); // health-like
    define(s, 1, fromWhole(10));                     // speed-like
    CHECK(value(s, 0) == 100000);

    // No modifiers: value equals base
    recompute(s, nullptr, 0);
    CHECK(value(s, 1) == 10000);

    // Add -> PercentAdd -> Multiply regardless of list order
    const Modifier a[] = {
        mod(1, ModOp::Multiply, 500),        // x0.5
        mod(1, ModOp::PercentAdd, 100),      // +10%
        mod(1, ModOp::Add, fromWhole(5)),    // +5
        mod(1, ModOp::PercentAdd, 200),      // +20%
    };
    recompute(s, a, 4);
    // (10 + 5) = 15 -> x1.3 = 19.5 -> x0.5 = 9.75
    CHECK(value(s, 1) == 9750);
    // Other stat untouched
    CHECK(value(s, 0) == 100000);

    // Same modifiers in another order give the same result
    const Modifier b[] = {a[2], a[3], a[1], a[0]};
    Stats s2 = s;
    recompute(s2, b, 4);
    CHECK(value(s2, 1) == value(s, 1));

    // Override replaces the result; the last one wins; clamp still applies
    const Modifier o[] = {mod(1, ModOp::Override, 0), mod(1, ModOp::Add, 7000), mod(1, ModOp::Override, 3000)};
    recompute(s, o, 3);
    CHECK(value(s, 1) == 3000);
    const Modifier big[] = {mod(0, ModOp::Add, fromWhole(1000)), mod(0, ModOp::Add, -fromWhole(700))};
    recompute(s, big, 2);
    CHECK(value(s, 0) == fromWhole(400));
    const Modifier over[] = {mod(0, ModOp::Add, fromWhole(1000))};
    recompute(s, over, 1);
    CHECK(value(s, 0) == fromWhole(500)); // clamped to max
    const Modifier under[] = {mod(0, ModOp::Add, -fromWhole(1000))};
    recompute(s, under, 1);
    CHECK(value(s, 0) == 0); // clamped to min

    // Truncation toward zero, bad stat index ignored, recompute is idempotent
    Stats t;
    define(t, 0, 1, -1000000, 1000000);
    const Modifier tr[] = {mod(0, ModOp::Multiply, 500), mod(200, ModOp::Add, 5)};
    recompute(t, tr, 2);
    CHECK(value(t, 0) == 0); // 1 * 0.5 truncates to 0
    Stats u = t;
    recompute(u, tr, 2);
    mealkit::Hasher h1, h2;
    mealkit::reflect::hashStruct(mealkit::reflect::typeOf<Stats>(), &t, h1);
    mealkit::reflect::hashStruct(mealkit::reflect::typeOf<Stats>(), &u, h2);
    CHECK(h1.value == h2.value);

    // No overflow with large values (64-bit intermediates)
    Stats big2;
    define(big2, 0, 2000000000);
    const Modifier m3[] = {mod(0, ModOp::Multiply, 3000), mod(0, ModOp::PercentAdd, 5000)};
    recompute(big2, m3, 2);
    CHECK(value(big2, 0) == 2000000000);

    std::printf(g_failed ? "stats tests: %d failed\n" : "stats tests: all passed\n", g_failed);
    return g_failed ? 1 : 0;
}
