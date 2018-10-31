// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "db/memtable.h"
#include "db/dbformat.h"
#include "leveldb/comparator.h"
#include "leveldb/env.h"
#include "leveldb/iterator.h"
#include "util/coding.h"

namespace leveldb {

//data由两部分组成：|encode(value_size)  |value  |
//解析长度后，返回value
static Slice GetLengthPrefixedSlice(const char* data) {
  uint32_t len;
  const char* p = data;
  //解析internal_key的长度
  //5: varint32编码后int最多占5个bytes
  p = GetVarint32Ptr(p, p + 5, &len);  // +5: we assume "p" is not corrupted
  //返回internal_key
  return Slice(p, len);
}

MemTable::MemTable(const InternalKeyComparator& cmp)
    : comparator_(cmp),
      refs_(0),
      table_(comparator_, &arena_) {
}

MemTable::~MemTable() {
  assert(refs_ == 0);
}

size_t MemTable::ApproximateMemoryUsage() { return arena_.MemoryUsage(); }

int MemTable::KeyComparator::operator()(const char* aptr, const char* bptr)
    const {
  // Internal keys are encoded as length-prefixed strings.
  // 分别获取internal_key并比较
  Slice a = GetLengthPrefixedSlice(aptr);
  Slice b = GetLengthPrefixedSlice(bptr);
  return comparator.Compare(a, b);
}

// Encode a suitable internal key target for "target" and return it.
// Uses *scratch as scratch space, and the returned pointer will point
// into this scratch space.
static const char* EncodeKey(std::string* scratch, const Slice& target) {
  scratch->clear();
  PutVarint32(scratch, target.size());
  scratch->append(target.data(), target.size());
  return scratch->data();
}

class MemTableIterator: public Iterator {
 public:
  explicit MemTableIterator(MemTable::Table* table) : iter_(table) { }

  virtual bool Valid() const { return iter_.Valid(); }
  virtual void Seek(const Slice& k) { iter_.Seek(EncodeKey(&tmp_, k)); }
  virtual void SeekToFirst() { iter_.SeekToFirst(); }
  virtual void SeekToLast() { iter_.SeekToLast(); }
  virtual void Next() { iter_.Next(); }
  virtual void Prev() { iter_.Prev(); }
  //存储到skiplist的格式为:
  //|encode(internal_key.size)  |internal_key  |encode(value.size())  |value  |
  //通过GetLengthPrefixedSlice获取internal_key返回
  virtual Slice key() const { return GetLengthPrefixedSlice(iter_.key()); }
  //首先通过GetLengthPrefixedSlice获取internal_key
  //然后通过GetLengthPrefixedSlice解析剩余字符串得到value返回
  virtual Slice value() const {
    Slice key_slice = GetLengthPrefixedSlice(iter_.key());
    return GetLengthPrefixedSlice(key_slice.data() + key_slice.size());
  }

  virtual Status status() const { return Status::OK(); }

 private:
  MemTable::Table::Iterator iter_;
  std::string tmp_;       // For passing to EncodeKey

  // No copying allowed
  MemTableIterator(const MemTableIterator&);
  void operator=(const MemTableIterator&);
};

Iterator* MemTable::NewIterator() {
  return new MemTableIterator(&table_);
}

void MemTable::Add(SequenceNumber s, ValueType type,
                   const Slice& key,
                   const Slice& value) {
  // Format of an entry is concatenation of:
  //  key_size     : varint32 of internal_key.size()
  //  key bytes    : char[internal_key.size()]
  //  value_size   : varint32 of value.size()
  //  value bytes  : char[value.size()]
  size_t key_size = key.size();
  size_t val_size = value.size();
  //InternalKey长度 = UserKey长度 + 8bytes(存储SequenceNumber + ValueType)
  size_t internal_key_size = key_size + 8;
  //用户写入的key value在内部实现里增加了sequence type
  //而到了MemTable实际按照以下格式组成一段buffer
  //|encode(internal_key_size)  |key  |sequence  type  |encode(value_size)  |value  |
  //这里先计算大小，申请对应大小的buffer
  const size_t encoded_len =
      VarintLength(internal_key_size) + internal_key_size +
      VarintLength(val_size) + val_size;
  char* buf = arena_.Allocate(encoded_len);
  //append key_size
  char* p = EncodeVarint32(buf, internal_key_size);
  //append key bytes
  memcpy(p, key.data(), key_size);
  p += key_size;
  //append sequence_number && type
  //64bits = 8bytes，前7个bytes存储s，最后一个bytes存储type.
  //这里8bytes对应前面 internal_key_size = key_size + 8
  //也是Fixed64而不是Varint64的原因
  EncodeFixed64(p, (s << 8) | type);
  p += 8;
  //append value_size
  p = EncodeVarint32(p, val_size);
  //append value bytes
  memcpy(p, value.data(), val_size);
  assert(p + val_size == buf + encoded_len);
  //写入table_的buffer包含了key/value及附属信息
  table_.Insert(buf);
}

bool MemTable::Get(const LookupKey& key, std::string* value, Status* s) {
  Slice memkey = key.memtable_key();
  Table::Iterator iter(&table_);
  iter.Seek(memkey.data());
  if (iter.Valid()) {
    // entry format is:
    //    klength  varint32
    //    userkey  char[klength]
    //    tag      uint64
    //    vlength  varint32
    //    value    char[vlength]
    // Check that it belongs to same user key.  We do not check the
    // sequence number since the Seek() call above should have skipped
    // all entries with overly large sequence numbers.
    const char* entry = iter.key();
    uint32_t key_length;
    //解析出internal_key的长度存储到key_length
    //key_ptr指向internal_key
    const char* key_ptr = GetVarint32Ptr(entry, entry+5, &key_length);
    //Seek返回的是第一个>=key的Node(>= <=> InternalKeyComparator::Compare)
    //因此先判断下userkey是否相等
    if (comparator_.comparator.user_comparator()->Compare(
            Slice(key_ptr, key_length - 8),
            key.user_key()) == 0) {
      // Correct user key
      // tag = (s << 8) | type
      const uint64_t tag = DecodeFixed64(key_ptr + key_length - 8);
      //type存储在最后一个字节
      switch (static_cast<ValueType>(tag & 0xff)) {
        case kTypeValue: {
          Slice v = GetLengthPrefixedSlice(key_ptr + key_length);
          value->assign(v.data(), v.size());
          return true;
        }
        case kTypeDeletion:
          *s = Status::NotFound(Slice());
          return true;
      }
    }
  }
  return false;
}

}  // namespace leveldb
