// GET /api/mo request path and response parsing (spec §7.1).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct MoItem {
  std::string mo;
  std::string produk;  // optional, "" when absent
};

// "/api/mo?tgl=<production date>" with a valid clock, "/api/mo" otherwise.
std::string moListPath(int64_t nowEpoch);
// False on malformed JSON or a missing "data" array; `out` is left untouched then.
bool parseMoList(const std::string& body, std::vector<MoItem>& out);
