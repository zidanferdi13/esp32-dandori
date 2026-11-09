// Problem code table (spec §6) and keypad input parsing for the code entry screen.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

struct ProblemCode {
  uint8_t code;
  const char* jenis;
  const char* mesin;
  const char* sub;
};

// Returns nullptr when code is outside 1..26.
const ProblemCode* findProblemCode(int code);
size_t problemCodeCount();

// Appends a keypad digit: at most 2 digits, a leading '0' on empty input is ignored.
std::string keypadAppend(const std::string& cur, char digit);
// Returns 1..26, or -1 when empty or out of range.
int keypadValue(const std::string& digits);
