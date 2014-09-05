/*
 * Copyright (c) 2014, Hewlett-Packard Development Company, LP.
 * The license and distribution terms for this file are placed in LICENSE.txt.
 */
#ifndef FOEDUS_ASSORTED_FIXED_STRING_HPP_
#define FOEDUS_ASSORTED_FIXED_STRING_HPP_
#include <stdint.h>

#include <algorithm>
#include <cstring>
#include <cwchar>
#include <ostream>
#include <string>

#include "foedus/assert_nd.hpp"
#include "foedus/cxx11.hpp"

namespace foedus {
namespace assorted {

/**
 * @brief An embedded string object of fixed max-length, which uses no external memory.
 * @ingroup ASSORTED
 * @details
 * This header-only object behaves like std::string in many ways.
 * The key difference is that this object is essentially a fixed array while std::string allocates
 * memory in heap (with some exception for optimized stack-allocation, but that's not the point).
 *
 * This implies a few crucial characteristics.
 *  \li We can copy/overwrite a piece of memory containing this object without taking care of
 * heap-allocated memory (which makes the handling of shared memory much easier).
 *  \li We have limit on the length of the string, determined at compile time (a template param).
 *  \li We always consume that much memory regardless the actual content.
 *
 * @section FIXEDSTRING_USE Usecases
 * As mentioned above, this object is used where we can't use std::string that points to somewhere
 * else. For example, use it as follows.
 * @code{.cpp}
 * struct MyPage {
 *   FixedString<12>  str1_;  // +4+12
 *   FixedString<8>   str2_;  // +4+8+4 (Anyway 8 byte aligned..)
 *   char             other_data_[4096-32];
 * };
 * // This page can be simply memcpy-ed unlike a struct that contains std::string.
 * @endcode
 *
 * @section FIXEDSTRING_LIMITS Limitations
 * No char traits for exotic comparison rules.
 * Not more than 2^32-1 chars. Length 2^32-1 is reserved for npos.
 */
template <uint MAXLEN, typename CHAR = char>
class FixedString {
 public:
  /** Constructs an empty string. */
  FixedString() CXX11_NOEXCEPT : length_(0) {}

  /** Copy constructor for all FixedString objects. Note that too-long strings are truncated. */
  template <uint MAXLEN2>
  explicit FixedString(const FixedString<MAXLEN2, CHAR>& other) CXX11_NOEXCEPT { assign(other); }

  /** Copy constructor for std::string. Note that too-long strings are truncated. */
  explicit FixedString(const std::basic_string<CHAR>& str) CXX11_NOEXCEPT { assign(str); }

  /** Copy constructor for char* and len. Note that too-long strings are truncated. */
  FixedString(const CHAR* str, uint32_t len) CXX11_NOEXCEPT { assign(str, len); }

  /** Copy constructor for null-terminated char*. Note that too-long strings are truncated. */
  FixedString(const CHAR* str) CXX11_NOEXCEPT {  // NOLINT(runtime/explicit) follows std::string
    assign(str, strlen(str));
  }

  static uint32_t strlen(const char* str) CXX11_NOEXCEPT { return std::strlen(str); }
  static uint32_t strlen(const wchar_t* str) CXX11_NOEXCEPT { return std::wcslen(str); }

  /** Assign operator for all FixedString objects. Note that too-long strings are truncated. */
  template <uint MAXLEN2>
  FixedString& operator=(const FixedString<MAXLEN2, CHAR>& other) CXX11_NOEXCEPT {
    assign(other);
    return *this;
  }
  /** Assign operator for std::string. Note that too-long strings are truncated. */
  FixedString& operator=(const std::basic_string<CHAR>& str) CXX11_NOEXCEPT {
    assign(str);
    return *this;
  }

  template <uint MAXLEN2>
  bool operator==(const FixedString<MAXLEN2, CHAR>& other) const CXX11_NOEXCEPT {
    if (length_ == 0) {
      return other.length() == 0;
    }
    return length_ == other.length() &&
      std::memcmp(data_, other.data(), length_ * sizeof(CHAR)) == 0;
  }
  template <uint MAXLEN2>
  bool operator!=(const FixedString<MAXLEN2, CHAR>& other) const CXX11_NOEXCEPT {
    return !operator==(other);
  }
  template <uint MAXLEN2>
  bool operator<(const FixedString<MAXLEN2, CHAR>& other) const CXX11_NOEXCEPT {
    uint32_t min_len = std::min<uint32_t>(length_, other.length());
    if (min_len == 0) {
      return length_ < other.length();
    }
    int result = std::memcmp(data_, other.data(), min_len * sizeof(CHAR));
    if (result != 0) {
      return result < 0;
    }
    return length_ < other.length();
  }

  bool operator==(const std::basic_string<CHAR>& str) const CXX11_NOEXCEPT {
    if (length_ == 0) {
      return str.size() == 0;
    }
    return length_ == str.size() && std::memcmp(data_, str.data(), length_ * sizeof(CHAR)) == 0;
  }
  bool operator!=(const std::basic_string<CHAR>& str) const CXX11_NOEXCEPT {
    return !operator==(str);
  }


  /** Assign operator for all FixedString objects. Note that too-long strings are truncated. */
  template <uint MAXLEN2>
  void      assign(const FixedString<MAXLEN2, CHAR>& other) CXX11_NOEXCEPT {
    ASSERT_ND(other.length() < MAXLEN2);
    length_ = other.length() > MAXLEN ? MAXLEN : other.length();
    std::memcpy(data_, other.data(), length_ * sizeof(CHAR));
  }
  /** Assign operator for std::string. Note that too-long strings are truncated. */
  void      assign(const std::basic_string<CHAR>& str) CXX11_NOEXCEPT {
    length_ = str.size() > MAXLEN ? MAXLEN : str.size();
    std::memcpy(data_, str.data(), length_ * sizeof(CHAR));
  }

  /** Assign operator for char* and length. Note that too-long strings are truncated. */
  void      assign(const CHAR* str, uint32_t len) CXX11_NOEXCEPT {
    length_ = len > MAXLEN ? MAXLEN : len;
    std::memcpy(data_, str, length_ * sizeof(CHAR));
  }

  // the following methods imitate std::string signatures.

  /** Returns the length of this string. */
  uint32_t  length() const CXX11_NOEXCEPT { return length_; }
  /** Returns the length of this string. */
  uint32_t  size() const CXX11_NOEXCEPT { return length_; }
  /** Return size of allocated storage. Actually a constexpr. */
  uint32_t  capacity() const CXX11_NOEXCEPT { return MAXLEN; }
  /** Return maximum size of string. Actually a constexpr. */
  uint32_t  max_size() const CXX11_NOEXCEPT { return MAXLEN; }
  /** Clear string */
  void      clear() CXX11_NOEXCEPT { length_ = 0; }
  /** Test if string is empty */
  bool      empty() const CXX11_NOEXCEPT { return length_ == 0; }

  /** Get string data  */
  const CHAR* data() const CXX11_NOEXCEPT { return data_; }
  /** Convert to a std::string object. */
  std::basic_string<CHAR> str() const {
    return std::basic_string<CHAR>(data_, length_);
  }

  /**
   * npos is a static member constant value with the greatest possible value for uint32_t.
   * This value, when used as the value for a len (or sublen) parameter in this object,
   * means "until the end of the string". As a return value, it is usually used to indicate no
   * matches. This constant is defined with a value of -1, which because uint32_t is an unsigned
   * integral type, it is the largest possible representable value for this type.
   */
  static const uint32_t npos = -1;

  friend std::ostream& operator<<(std::ostream& o, const FixedString& v) {
    o << v.str();
    return o;
  }

 private:
  /** String length. 0 means an empty string. */
  uint32_t  length_;        // +4
  /** Content of this string. data_[len_] and later are undefined. We don't bother clear them. */
  CHAR      data_[MAXLEN];  // +MAXLEN
};

}  // namespace assorted
}  // namespace foedus
#endif  // FOEDUS_ASSORTED_FIXED_STRING_HPP_
