// Minimal host-side test harness: TEST(name), CHECK(cond), CHECK_EQ(a, b).
#pragma once
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace mini_test {

struct TestCase {
  const char* name;
  void (*fn)();
};

inline std::vector<TestCase>& registry() {
  static std::vector<TestCase> tests;
  return tests;
}

inline int& currentFailures() {
  static int failures = 0;
  return failures;
}

struct Registrar {
  Registrar(const char* name, void (*fn)()) { registry().push_back({name, fn}); }
};

template <typename T>
std::string show(const T& v) {
  std::ostringstream os;
  os << v;
  return os.str();
}
inline std::string show(const std::string& v) { return "\"" + v + "\""; }
inline std::string show(const char* v) { return v ? "\"" + std::string(v) + "\"" : "nullptr"; }
inline std::string show(bool v) { return v ? "true" : "false"; }

inline void fail(const char* file, int line, const std::string& msg) {
  ++currentFailures();
  std::cout << "  " << file << ":" << line << ": " << msg << "\n";
}

}  // namespace mini_test

#define TEST(name)                                                   \
  static void name();                                                \
  static mini_test::Registrar name##_registrar(#name, name);         \
  static void name()

#define CHECK(cond)                                                  \
  do {                                                               \
    if (!(cond)) mini_test::fail(__FILE__, __LINE__, "CHECK(" #cond ")"); \
  } while (0)

#define CHECK_EQ(a, b)                                               \
  do {                                                               \
    const auto va_ = (a);                                            \
    const auto vb_ = (b);                                            \
    if (!(va_ == vb_))                                               \
      mini_test::fail(__FILE__, __LINE__,                            \
                      "CHECK_EQ(" #a ", " #b "): " + mini_test::show(va_) + \
                          " != " + mini_test::show(vb_));            \
  } while (0)
