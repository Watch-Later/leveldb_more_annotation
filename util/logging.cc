// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "util/logging.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits>
#include "leveldb/env.h"
#include "leveldb/slice.h"

namespace leveldb {

void AppendNumberTo(std::string* str, uint64_t num) {
  char buf[30];
  snprintf(buf, sizeof(buf), "%llu", (unsigned long long) num);
  str->append(buf);
}

void AppendEscapedStringTo(std::string* str, const Slice& value) {
  for (size_t i = 0; i < value.size(); i++) {
    char c = value[i];
    if (c >= ' ' && c <= '~') {
      str->push_back(c);
    } else {
      char buf[10];
      snprintf(buf, sizeof(buf), "\\x%02x",
               static_cast<unsigned int>(c) & 0xff);
      str->append(buf);
    }
  }
}

std::string NumberToString(uint64_t num) {
  std::string r;
  AppendNumberTo(&r, num);
  return r;
}

std::string EscapeString(const Slice& value) {
  std::string r;
  AppendEscapedStringTo(&r, value);
  return r;
}

//解析字符串in里的整数值，直到字符串结束或者碰到非整数字符[^0~9]
//解析后的整数存储到val，in记录解析后剩余的字符串
//正常解析返回true, 如果解析出的整数值超过uint64_t最大值，那么返回false
bool ConsumeDecimalNumber(Slice* in, uint64_t* val) {
  // Constants that will be optimized away.
  constexpr const uint64_t kMaxUint64 = std::numeric_limits<uint64_t>::max();
  constexpr const char kLastDigitOfMaxUint64 =
      '0' + static_cast<char>(kMaxUint64 % 10);

  //解析出的整数暂时存在value
  uint64_t value = 0;

  // reinterpret_cast-ing from char* to unsigned char* to avoid signedness.
  const unsigned char* start =
      reinterpret_cast<const unsigned char*>(in->data());

  const unsigned char* end = start + in->size();
  const unsigned char* current = start;
  for (; current != end; ++current) {
    const unsigned char ch = *current;
    if (ch < '0' || ch > '9')
      break;

    // Overflow check.
    // kMaxUint64 / 10 is also constant and will be optimized away.
    // (value * 10 > kMaxUint64) or (value * 10 + ch > kMaxUint64) 情况下，再对value赋值都会导致超过最大值
    if (value > kMaxUint64 / 10 ||
        (value == kMaxUint64 / 10 && ch > kLastDigitOfMaxUint64)) {
      return false;
    }

    value = (value * 10) + (ch - '0');
  }

  *val = value;
  const size_t digits_consumed = current - start;
  in->remove_prefix(digits_consumed);
  return digits_consumed != 0;
}

}  // namespace leveldb
