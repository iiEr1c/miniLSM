#pragma once

#include "util.hpp"

#include <cassert>
#include <cstdint>
#include <span>
#include <vector>

using namespace util;

struct BlockMeta {
  uint32_t offset; // 这里要求单个sst小于4GB
  // first key of the data block
  std::vector<uint8_t> first_key;

  void encode_block_meta(std::vector<BlockMeta> &block_meta,
                         std::vector<uint8_t> &buf) {
    size_t estimated_size = 0;
    /**
     * 序列化BlockMeta时, 4字节offset, 2字节用于描述first_key的长度, key
     */
    for (auto &meta : block_meta) {
      estimated_size += sizeof(uint32_t);
      estimated_size += sizeof(uint16_t);
      estimated_size += meta.first_key.size();
    }
    size_t origin_len = buf.size();
    buf.reserve(estimated_size + origin_len);
    for (auto &meta : block_meta) {
      put_u32(buf, meta.offset);
      put_u16(buf, static_cast<uint16_t>(meta.first_key.size()));
      buf.insert(buf.end(), meta.first_key.begin(), meta.first_key.end());
    }
    assert(estimated_size == buf.size() - origin_len);
  }

  // 假设buff是std::span<uint8_t>
  std::vector<BlockMeta> decode_block_meta(std::span<uint8_t> buf) {
    std::vector<BlockMeta> block_meta;
    while (buf.size() != 0) {
      assert(buf.size() >= 4);
      uint32_t offset = get_u32(static_cast<uint8_t *>(buf.data()));
      buf = buf.subspan(sizeof(uint32_t));
      assert(buf.size() >= 2);
      uint16_t first_key_len = get_u16(buf[0], buf[1]);
      buf = buf.subspan(sizeof(uint16_t));
      assert(buf.size() >= first_key_len);
      BlockMeta block{.offset = offset,
                      .first_key = std::vector<uint8_t>(
                          buf.begin(), buf.begin() + first_key_len)};
      buf = buf.subspan(first_key_len);
    }
  }
};
