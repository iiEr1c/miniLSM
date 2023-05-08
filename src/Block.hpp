#pragma once

#include "util.hpp"

#include <algorithm>
#include <bitset>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <list>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

#include <fmt/core.h>

using namespace util;

/**
 * + k_len + k + v_len + v + offsets(n对kv则n个offset) + number of kv +
 * + k_len && v_len use uint16_t(2B)+
 */
// 要求该block的offset不能超过2^16次幂

struct Block {

  struct kv {
    std::span<uint8_t> key;
    std::span<uint8_t> value;
    // 这里其实默认key的类型像std::string类似的, 是能顺序比较的类型
    // 否则如果key是数字类型, 那么则要将其转换成整型再比较
    // 或者直接将整型转换为string?
    friend auto operator<=>(const kv &lhs, const kv &rhs) noexcept {
      return std::lexicographical_compare_three_way(
          lhs.key.begin(), lhs.key.end(), rhs.key.begin(), rhs.key.end());
    }

    friend bool operator==(const kv &lhs, const kv &rhs) noexcept {
      if (lhs.key.size() != rhs.key.size()) {
        return false;
      }
      for (size_t i = 0; i < lhs.key.size(); ++i) {
        if (lhs.key[i] != rhs.key[i]) {
          return false;
        }
      }
      return true;
    }
  };

  std::vector<uint8_t> data_; // k_len k v_len v
  std::vector<uint16_t> offsets_;

  std::vector<uint8_t> encode() {
    auto buf = data_;
    uint16_t offsets_len = static_cast<uint16_t>(offsets_.size());
    for (auto &offset : offsets_) {
      put_u16(buf, offset);
    }

    put_u16(buf, offsets_len);
    return buf;
  }

  Block &decode(std::vector<uint8_t> data) {
    uint16_t offsets_len =
        *reinterpret_cast<uint16_t *>(&data[data.size() - sizeof(uint16_t)]);
    uint16_t data_end =
        data.size() - sizeof(uint16_t) - offsets_len * sizeof(uint16_t);
    auto offsetSpan = std::span<uint8_t>(data.begin() + data_end,
                                         data.end() - sizeof(uint16_t));
    for (size_t i = 0; i < offsetSpan.size(); i += 2) {
      uint16_t offset = get_u16(offsetSpan[i], offsetSpan[i + 1]);
      this->offsets_.push_back(offset);
    }
    std::copy(data.begin(), data.begin() + data_end, this->data_.begin());
    return *this;
  }

  struct iterator {
    using iterator_type = iterator;
    using iterator_category = std::random_access_iterator_tag;
    using value_type = kv;
    using difference_type = std::ptrdiff_t;
    using reference = kv &;
    using pointer = kv *;

    Block *ptr;
    difference_type index;

    iterator() noexcept : ptr(nullptr), index(0) {}
    iterator(const iterator &i) noexcept : ptr(i.ptr), index(i.index) {}

    iterator(Block *ptr, difference_type index) noexcept
        : ptr(ptr), index(index) {}

    kv operator*() const noexcept {
      assert(index >= 0);
      uint16_t off = ptr->offsets_[index];
      uint16_t k_len = get_u16(ptr->data_[off], ptr->data_[off + 1]);
      off += sizeof(uint16_t);
      std::span<uint8_t> key(ptr->data_.begin() + off, k_len);
      off += k_len;
      uint16_t v_len = get_u16(ptr->data_[off], ptr->data_[off + 1]);
      off += sizeof(uint16_t);
      std::span<uint8_t> value(ptr->data_.begin() + off, v_len);
      return {key, value};
    }

    kv operator[](difference_type n) const noexcept {
      assert(n >= 0);
      uint16_t off = ptr->offsets_[n];
      uint16_t k_len = get_u16(ptr->data_[off], ptr->data_[off + 1]);
      off += sizeof(uint16_t);
      std::span<uint8_t> key(ptr->data_.begin() + off, k_len);
      off += k_len;
      uint16_t v_len = get_u16(ptr->data_[off], ptr->data_[off + 1]);
      off += sizeof(uint16_t);
      std::span<uint8_t> value(ptr->data_.begin() + off, v_len);
      return {key, value};
    }

    iterator &operator++() noexcept {
      ++index;
      return *this;
    }

    iterator operator++(int) noexcept {
      iterator tmp = *this;
      ++index;
      return tmp;
    }

    iterator &operator--() noexcept {
      --index;
      return *this;
    }

    iterator operator--(int) noexcept {
      iterator tmp = *this;
      --index;
      return tmp;
    }

    iterator &operator+=(difference_type n) noexcept {
      index += n;
      return *this;
    }

    iterator &operator-=(difference_type n) noexcept {
      index -= n;
      return *this;
    }

    iterator operator+(difference_type n) const noexcept {
      return iterator{this->ptr, this->index + n};
    }

    iterator operator-(difference_type n) const noexcept {
      return iterator{this->ptr, this->index - n};
    }

    friend difference_type operator-(const iterator &x,
                                     const iterator &y) noexcept {
      assert(x.ptr == y.ptr);
      assert(x.ptr != nullptr && y.ptr != nullptr);
      return x.index - y.index;
    }

    friend bool operator==(const iterator &x, const iterator &y) noexcept {
      return x.ptr == y.ptr && x.index == y.index;
    }

    friend bool operator!=(const iterator &x, const iterator &y) noexcept {
      return x.ptr == y.ptr && x.index != y.index;
    }
  };

  auto begin() { return iterator{this, 0}; }

  auto end() {
    return iterator{
        this, static_cast<iterator::difference_type>(this->offsets_.size())};
  }

  // 暂时无法提供像lower_bound一样的接口...
  // 将key转换为K然后再比较?
  // 这里只比较key
  template <typename K> iterator lower_bound(K value) {
    iterator first = begin(), last = end();
    iterator it;
    auto count = last - first;
    assert(count == this->offsets_.size());
    iterator::difference_type step;
    while (count > 0) {
      it = first;
      step = count / 2;
      it += step;
      // 如何比较key的大小呢?
      auto kvSpan = *it;
      // need convert to K from std::span<uint8_t>]
      if constexpr (std::is_same_v<K, std::string_view>) {
        auto keySV =
            std::string_view(reinterpret_cast<const char *>(kvSpan.key.data()),
                             kvSpan.key.size());
        fmt::print("keySV = {}\n", keySV);
        if (keySV < value) {
          first = ++it;
          count -= step + 1;
        } else {
          count = step;
        }
      } else {
        // 后续需要支持uint32_t/uint64_t等?
        fmt::print("todo: support other type\n");
      }
    }

    return first;
  }
};

struct BlockBuilder {
  std::vector<uint8_t> data_;
  std::vector<uint16_t> offsets_;
  uint16_t block_size_;

  BlockBuilder(uint16_t block_size) : block_size_(block_size) {}

  uint16_t estimated_size() {
    return offsets_.size() * sizeof(uint16_t) + data_.size() + sizeof(uint16_t);
  }

  // 退化到uint8_t?
  bool add(std::span<uint8_t> key, std::span<uint8_t> value) {
    assert(key.empty() == false);
    if (estimated_size() + key.size() + value.size() + sizeof(uint16_t) * 3 >
            block_size_ &&
        !empty()) {
      // k_len/v_len/add a offset(all uint16_t)
      return false;
    }

    offsets_.emplace_back(static_cast<uint16_t>(data_.size()));
    put_u16(data_, static_cast<uint16_t>(key.size()));
    for (auto &u8 : key) {
      data_.emplace_back(u8);
    }
    put_u16(data_, static_cast<uint16_t>(value.size()));
    for (auto &u8 : value) {
      data_.emplace_back(u8);
    }
    return true;
  }

  bool empty() { return offsets_.empty(); }

  Block build() {
    if (empty()) {
      std::abort();
    }

    return Block{this->data_, this->offsets_};
  }
};

/**
 * 迭代器:参考std::list和std::vector的++/+=之类的操作
 */