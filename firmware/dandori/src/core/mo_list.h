// GET MO list request path and response parsing (spec §7.1).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct MoItem {
  std::string mo;
  std::string produk;  // optional, "" when absent
};

// "/kanban/getMOPalletizer?tgl=<production date>" with a valid clock, without the query otherwise.
std::string moListPath(int64_t nowEpoch);
// Accepts both the Palletizer service (nomor_mo/product_name, respone_code) and the
// plain contract (mo/produk, data). False on malformed JSON, a non-2xx respone_code,
// or a missing "data" array; `out` is left untouched then.
bool parseMoList(const std::string& body, std::vector<MoItem>& out);
