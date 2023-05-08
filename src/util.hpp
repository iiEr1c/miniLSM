#pragma once

#include <bit>
#include <cstdint>
#include <vector>

namespace util {
void put_u16(std::vector<uint8_t> &buf, uint16_t value) {
  if constexpr (std::endian::native == std::endian::big) {
    buf.push_back(value >> 8);
    buf.push_back(value & 0xFF);
  } else {
    buf.push_back(value & 0xFF);
    buf.push_back(value >> 8);
  }
}

uint16_t get_u16(uint8_t first, uint8_t second) {
  if constexpr (std::endian::native == std::endian::big) {
    return (first << 8) + second;
  } else {
    return (second << 8) + first;
  }
}

void put_u32(std::vector<uint8_t> &buf, uint32_t value) {
  if constexpr (std::endian::native == std::endian::big) {
    buf.push_back((value >> 24) & 0xff);
    buf.push_back((value >> 16) & 0xff);
    buf.push_back((value >> 8) & 0xff);
    buf.push_back((value)&0xff);
  } else {
    buf.push_back((value)&0xff);
    buf.push_back((value >> 8) & 0xff);
    buf.push_back((value >> 16) & 0xff);
    buf.push_back((value >> 24) & 0xff);
  }
}

// assume [start, start + 4) is validity
uint32_t get_u32(uint8_t *start) {
  if constexpr (std::endian::native == std::endian::big) {
    uint32_t result = 0;
    uint32_t off = 24;
    for (size_t i = 0; i < 4; ++i) {
      result += (*start << off);
      ++start;
      off -= 8;
    }
    return result;
  } else {
    uint32_t result = 0;
    uint32_t off = 0;
    for (size_t i = 0; i < 4; ++i) {
      result += (*start << off);
      ++start;
      off += 8;
    }
    return result;
  }
}
} // namespace util
