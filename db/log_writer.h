// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_LOG_WRITER_H_
#define STORAGE_LEVELDB_DB_LOG_WRITER_H_

#include <stdint.h>
#include "db/log_format.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"

namespace leveldb {

class WritableFile;

namespace log {

//Write负责组织数据有格式的写入，成员变量dest_负责真正写入数据到文件系统
class Writer {
 public:
  // Create a writer that will append data to "*dest".
  // "*dest" must be initially empty.
  // "*dest" must remain live while this Writer is in use.
  explicit Writer(WritableFile* dest);

  // Create a writer that will append data to "*dest".
  // "*dest" must have initial length "dest_length".
  // "*dest" must remain live while this Writer is in use.
  Writer(WritableFile* dest, uint64_t dest_length);

  ~Writer();

  //接收数据并调用dest完成写入
  //实现细节：
  //根据slice 及 block剩余大小，可能分成一个或者多个fragment分别写入
  //对于一个fragment格式如下：
  //|crc(4B)  |length(2B)  |type(1B)  |ptr(nB)...  |
  //|--------------header-------------|----data----|
  //其中 type 是一个枚举类型：RecordType
  //{kZeroType kFullType kFirstType kMiddleType kLastType}
  Status AddRecord(const Slice& slice);

 private:
  WritableFile* dest_;
  //每kBlockSize记为一个block，block_offset_记录当前block已经写入的偏移量
  int block_offset_;       // Current offset in block

  // crc32c values for all supported record types.  These are
  // pre-computed to reduce the overhead of computing the crc of the
  // record type stored in the header.
  uint32_t type_crc_[kMaxRecordType + 1];

  Status EmitPhysicalRecord(RecordType type, const char* ptr, size_t length);

  // No copying allowed
  Writer(const Writer&);
  void operator=(const Writer&);
};

}  // namespace log
}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_LOG_WRITER_H_
