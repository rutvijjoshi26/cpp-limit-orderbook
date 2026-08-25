#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

// Minimal zero-dependency test harness: register tests, run them all,
// report pass/fail counts and failure locations.

namespace testkit {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(const std::string& name, std::function<void()> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

inline int& failure_count() {
    static int count = 0;
    return count;
}

inline const char*& current_test_name() {
    static const char* name = "";
    return name;
}

inline void report_failure(const char* file, int line, const std::string& message) {
    failure_count()++;
    std::fprintf(stderr, "  FAILED [%s] %s:%d: %s\n", current_test_name(), file, line, message.c_str());
}

inline int run_all() {
    int passed = 0;
    for (auto& tc : registry()) {
        current_test_name() = tc.name.c_str();
        int before = failure_count();
        tc.fn();
        if (failure_count() == before) {
            passed++;
        }
    }
    int total = static_cast<int>(registry().size());
    int failed = total - passed;
    std::printf("\n%d passed, %d failed, %d total\n", passed, failed, total);
    return failed == 0 ? 0 : 1;
}

}  // namespace testkit

#define TEST(name)                                                        \
    static void name();                                                  \
    static ::testkit::Registrar registrar_##name(#name, name);           \
    static void name()

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            ::testkit::report_failure(__FILE__, __LINE__, "CHECK failed: " #cond); \
        }                                                                  \
    } while (0)

#define CHECK_EQ(a, b)                                                    \
    do {                                                                  \
        auto va = (a);                                                    \
        auto vb = (b);                                                    \
        if (!(va == vb)) {                                                \
            ::testkit::report_failure(__FILE__, __LINE__, "CHECK_EQ failed: " #a " != " #b); \
        }                                                                  \
    } while (0)
