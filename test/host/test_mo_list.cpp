#include "core/mo_list.h"
#include "mini_test.h"
#include "test_helpers.h"

TEST(mo_path) {
  CHECK_EQ(moListPath(wib(2026, 11, 10, 2, 0, 0)), "/api/mo?tgl=2026-11-09");
  CHECK_EQ(moListPath(0), "/api/mo");
}

TEST(mo_parse) {
  std::vector<MoItem> v;
  CHECK(parseMoList(R"({"tgl":"2026-11-09","data":[{"mo":"MO-1","produk":"Bata"},{"mo":"MO-2"}]})", v));
  CHECK_EQ(v.size(), 2u);
  CHECK_EQ(v[0].mo, "MO-1");
  CHECK_EQ(v[0].produk, "Bata");
  CHECK_EQ(v[1].produk, "");
  CHECK(parseMoList(R"({"data":[]})", v));
  CHECK_EQ(v.size(), 0u);
}

TEST(mo_parse_rejects_bad_bodies) {
  std::vector<MoItem> v{{"keep", ""}};
  CHECK(!parseMoList("<html>500</html>", v));
  CHECK(!parseMoList(R"({"x":1})", v));
  CHECK(!parseMoList("", v));
}

TEST(mo_parse_skips_items_without_mo) {
  std::vector<MoItem> v;
  CHECK(parseMoList(R"({"data":[{"produk":"x"},{"mo":""},{"mo":"MO-3"}]})", v));
  CHECK_EQ(v.size(), 1u);
  CHECK_EQ(v[0].mo, "MO-3");
}
