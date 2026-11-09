#include <string>

#include "core/problem_codes.h"
#include "mini_test.h"

TEST(codes_table) {
  CHECK_EQ(problemCodeCount(), 26u);
  CHECK_EQ(std::string(findProblemCode(1)->sub), "Conveyor Infeed");
  CHECK_EQ(std::string(findProblemCode(13)->jenis), "Loss Time");
  CHECK_EQ(std::string(findProblemCode(13)->sub), "Dandory Produk");
  CHECK_EQ(std::string(findProblemCode(22)->mesin), "Utility");
  CHECK_EQ(std::string(findProblemCode(26)->jenis), "Performance Loss");
  CHECK(findProblemCode(0) == nullptr);
  CHECK(findProblemCode(27) == nullptr);
}

TEST(codes_every_row_matches_its_number) {
  for (int i = 1; i <= 26; ++i) {
    const ProblemCode* p = findProblemCode(i);
    CHECK(p != nullptr);
    if (p) CHECK_EQ(static_cast<int>(p->code), i);
  }
}

TEST(keypad) {
  CHECK_EQ(keypadAppend("", '0'), "");
  CHECK_EQ(keypadAppend("1", '3'), "13");
  CHECK_EQ(keypadAppend("13", '5'), "13");
  CHECK_EQ(keypadValue("13"), 13);
  CHECK_EQ(keypadValue(""), -1);
  CHECK_EQ(keypadValue("27"), -1);
  CHECK_EQ(keypadValue("9"), 9);
}
