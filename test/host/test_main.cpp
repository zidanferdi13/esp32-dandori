#include "mini_test.h"

int main() {
  int passed = 0;
  int failed = 0;
  for (const auto& t : mini_test::registry()) {
    const int before = mini_test::currentFailures();
    t.fn();
    if (mini_test::currentFailures() == before) {
      ++passed;
    } else {
      ++failed;
      std::cout << "FAIL " << t.name << "\n";
    }
  }
  std::cout << passed << " passed, " << failed << " failed\n";
  return failed == 0 ? 0 : 1;
}
