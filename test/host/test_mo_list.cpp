#include "core/mo_list.h"
#include "mini_test.h"
#include "test_helpers.h"

TEST(mo_path) {
  CHECK_EQ(moListPath(wib(2026, 11, 10, 2, 0, 0)), "/kanban/getMOPalletizer?tgl=2026-11-09");
  CHECK_EQ(moListPath(0), "/kanban/getMOPalletizer");
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

TEST(mo_parse_palletizer) {
  std::vector<MoItem> v;
  CHECK(parseMoList(
      R"({"respone_code":200,"respone_desc":"Success","data":[)"
      R"({"nomor_mo":"WAN/MO/26/11/13287","product_name":"WAN Premix","isactive":1},)"
      R"({"nomor_mo":"WAN/MO/26/11/13288","product_name":null,"isactive":0}]})",
      v));
  CHECK_EQ(v.size(), 1u);
  CHECK_EQ(v[0].mo, "WAN/MO/26/11/13287");
  CHECK_EQ(v[0].produk, "WAN Premix");
}

TEST(mo_parse_rejects_bad_respone_code) {
  std::vector<MoItem> v;
  CHECK(!parseMoList(R"({"respone_code":500,"data":[]})", v));
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
