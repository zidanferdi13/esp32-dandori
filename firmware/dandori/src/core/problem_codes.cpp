#include "problem_codes.h"

namespace {

const ProblemCode kCodes[] = {
    {1, "Downtime", "Palletizer", "Conveyor Infeed"},
    {2, "Downtime", "Palletizer", "Turning Belt"},
    {3, "Downtime", "Palletizer", "Cross Inverter"},
    {4, "Downtime", "Palletizer", "Grouping Belt"},
    {5, "Downtime", "Palletizer", "Row Pusher"},
    {6, "Downtime", "Palletizer", "Stripper Plate"},
    {7, "Downtime", "Palletizer", "Pressing Plat BCD"},
    {8, "Downtime", "Palletizer", "Plat B"},
    {9, "Downtime", "Palletizer", "Plat C"},
    {10, "Downtime", "Palletizer", "Plat D"},
    {11, "Downtime", "Palletizer", "Elevator"},
    {12, "Downtime", "Palletizer", "Lain-Lain"},
    {13, "Loss Time", "Palletizer", "Dandory Produk"},
    {14, "Loss Time", "Palletizer", "Preparation Awal Shift"},
    {15, "Loss Time", "Palletizer", "Pasir Kurang / Habis"},
    {16, "Loss Time", "Palletizer", "Kemasan Kurang / Habis"},
    {17, "Loss Time", "Palletizer", "Aditif Kurang / Habis"},
    {18, "Loss Time", "Palletizer", "Semen Kurang / Habis"},
    {19, "Loss Time", "Palletizer", "Menunggu Forklift"},
    {20, "Loss Time", "Palletizer", "Cek Lab"},
    {21, "Loss Time", "Palletizer", "Lain-Lain"},
    {22, "Loss Time", "Utility", "Angin"},
    {23, "Loss Time", "Utility", "Listrik"},
    {24, "Loss Time", "Utility", "Lain-Lain"},
    {25, "Performance Loss", "Palletizer", "Small Stop"},
    {26, "Performance Loss", "Palletizer", "Reduce Speed"},
};

constexpr size_t kCount = sizeof(kCodes) / sizeof(kCodes[0]);

}  // namespace

const ProblemCode* findProblemCode(int code) {
  if (code < 1 || code > static_cast<int>(kCount)) return nullptr;
  return &kCodes[code - 1];
}

size_t problemCodeCount() { return kCount; }

std::string keypadAppend(const std::string& cur, char digit) {
  if (digit < '0' || digit > '9') return cur;
  if (cur.size() >= 2) return cur;
  if (cur.empty() && digit == '0') return cur;
  return cur + digit;
}

int keypadValue(const std::string& digits) {
  if (digits.empty() || digits.size() > 2) return -1;
  int value = 0;
  for (char c : digits) {
    if (c < '0' || c > '9') return -1;
    value = value * 10 + (c - '0');
  }
  return findProblemCode(value) ? value : -1;
}
