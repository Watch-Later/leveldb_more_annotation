// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "leveldb/table_builder.h"

#include <assert.h>
#include "leveldb/comparator.h"
#include "leveldb/env.h"
#include "leveldb/filter_policy.h"
#include "leveldb/options.h"
#include "table/block_builder.h"
#include "table/filter_block.h"
#include "table/format.h"
#include "util/coding.h"
#include "util/crc32c.h"

namespace leveldb {

struct TableBuilder::Rep {
  Options options;
  Options index_block_options;
  WritableFile* file;
  uint64_t offset;//file当前写入大小
  Status status;
  //data block&&index block都采用相同的格式，通过BlockBuilder完成
  //不过block_restart_interval参数不同
  BlockBuilder data_block;
  BlockBuilder index_block;
  std::string last_key;
  int64_t num_entries;
  bool closed;          // Either Finish() or Abandon() has been called.
  FilterBlockBuilder* filter_block;

  // We do not emit the index entry for a block until we have seen the
  // first key for the next data block.  This allows us to use shorter
  // keys in the index block.  For example, consider a block boundary
  // between the keys "the quick brown fox" and "the who".  We can use
  // "the r" as the key for the index block entry since it is >= all
  // entries in the first block and < all entries in subsequent
  // blocks.
  //
  // Invariant: r->pending_index_entry is true only if data_block is empty.
  // FindShortestSeparator完成: ("the quick brown fox", "the who") -> "the r"
  // 当写入一个data block后，设置r->pending_index_entry = true，之后更新index block
  bool pending_index_entry;
  BlockHandle pending_handle;  // Handle to add to index block

  std::string compressed_output;

  Rep(const Options& opt, WritableFile* f)
      : options(opt),
        index_block_options(opt),
        file(f),
        offset(0),
        data_block(&options),
        index_block(&index_block_options),
        num_entries(0),
        closed(false),
        //default opt.filter_policy is nullptr
        filter_block(opt.filter_policy == nullptr ? nullptr
                     : new FilterBlockBuilder(opt.filter_policy)),
        pending_index_entry(false) {
    //index_block调用一次Add，同时更新restarts_
    index_block_options.block_restart_interval = 1;
  }
};

//传入file负责写入，文件打开和关闭在类外完成
TableBuilder::TableBuilder(const Options& options, WritableFile* file)
    : rep_(new Rep(options, file)) {
  if (rep_->filter_block != nullptr) {
    rep_->filter_block->StartBlock(0);
  }
}

TableBuilder::~TableBuilder() {
  assert(rep_->closed);  // Catch errors where caller forgot to call Finish()
  delete rep_->filter_block;
  delete rep_;
}

Status TableBuilder::ChangeOptions(const Options& options) {
  // Note: if more fields are added to Options, update
  // this function to catch changes that should not be allowed to
  // change in the middle of building a Table.
  if (options.comparator != rep_->options.comparator) {
    return Status::InvalidArgument("changing comparator while building table");
  }

  // Note that any live BlockBuilders point to rep_->options and therefore
  // will automatically pick up the updated options.
  rep_->options = options;
  rep_->index_block_options = options;
  rep_->index_block_options.block_restart_interval = 1;
  return Status::OK();
}

//更新数据到data_block，fiter_block，可能触发data_block的落盘以及index_block的更新
void TableBuilder::Add(const Slice& key, const Slice& value) {
  Rep* r = rep_;
  assert(!r->closed);
  if (!ok()) return;
  if (r->num_entries > 0) {
    //输入key有序，因此key > r->last_key
    assert(r->options.comparator->Compare(key, Slice(r->last_key)) > 0);
  }

  //刚写入了一个data block后设置为true
  if (r->pending_index_entry) {
    assert(r->data_block.empty());
    //计算满足>r->last_key && <= key的第一个字符串，存储到r->last_key
    //例如(abcdefg, abcdxyz) -> 1st_arg = abcdf
    r->options.comparator->FindShortestSeparator(&r->last_key, key);
    std::string handle_encoding;
    //pending_handle记录的是上个block写入前的offset及大小
    r->pending_handle.EncodeTo(&handle_encoding);
    r->index_block.Add(r->last_key, Slice(handle_encoding));
    r->pending_index_entry = false;
  }

  if (r->filter_block != nullptr) {
    r->filter_block->AddKey(key);
  }

  r->last_key.assign(key.data(), key.size());
  r->num_entries++;
  r->data_block.Add(key, value);

  const size_t estimated_block_size = r->data_block.CurrentSizeEstimate();
  //如果data_block大小超过了4K(default)，调用Flush
  if (estimated_block_size >= r->options.block_size) {
    Flush();
  }
}

//写入data block，更新pending handle, filter_block
void TableBuilder::Flush() {
  Rep* r = rep_;
  assert(!r->closed);
  if (!ok()) return;
  if (r->data_block.empty()) return;
  assert(!r->pending_index_entry);
  //写入r->data_block到r->file
  //更新pending_handle: size为r->data_block的大小，offset为写入data_block前的offset
  //因此pending_handle可以定位一个完整的data_block
  WriteBlock(&r->data_block, &r->pending_handle);
  if (ok()) {
    //标记需要写入一个index
    r->pending_index_entry = true;
    //r->file落盘
    r->status = r->file->Flush();
  }
  if (r->filter_block != nullptr) {
    r->filter_block->StartBlock(r->offset);
  }
}

//从block取出数据写入到文件，handle记录本次写入的数据size，以及写入前的offset
void TableBuilder::WriteBlock(BlockBuilder* block, BlockHandle* handle) {
  // File format contains a sequence of blocks where each block has:
  //    block_data: uint8[n]
  //    type: uint8
  //    crc: uint32
  assert(ok());
  Rep* r = rep_;
  //获取BlockBuilder内部格式组织的数据
  Slice raw = block->Finish();

  Slice block_contents;
  CompressionType type = r->options.compression;
  // TODO(postrelease): Support more compression options: zlib?
  switch (type) {
    //不采用任何压缩方式，直接取raw数据
    case kNoCompression:
      block_contents = raw;
      break;

    case kSnappyCompression: {
      std::string* compressed = &r->compressed_output;
      //调用snappy压缩raw，压缩结果存储到compressed
      //如果compressed.size < raw.size() * 7/8，即压缩后大小小于原来的87.5，则使用压缩后数据
      if (port::Snappy_Compress(raw.data(), raw.size(), compressed) &&
          compressed->size() < raw.size() - (raw.size() / 8u)) {
        block_contents = *compressed;
      } else {
        //压缩比例太小，则即使指定了kSnappyCompression，也不采用任何压缩方式
        // Snappy not supported, or compressed less than 12.5%, so just
        // store uncompressed form
        block_contents = raw;
        type = kNoCompression;
      }
      break;
    }
  }
  WriteRawBlock(block_contents, type, handle);
  r->compressed_output.clear();
  block->Reset();
}

//将 (block_contents, type, crc) 写入r->file，更新r->status r->offset
//[In]  block_contents: 写入内容
//[In]  type: block_contents的压缩方式
//[Out] handle: 更新size为block_contents大小 offset为写入r->file前的offset
void TableBuilder::WriteRawBlock(const Slice& block_contents,
                                 CompressionType type,
                                 BlockHandle* handle) {
  Rep* r = rep_;
  handle->set_offset(r->offset);
  handle->set_size(block_contents.size());
  //先写block contents
  r->status = r->file->Append(block_contents);
  if (r->status.ok()) {
    //5 bytes:|compression type  |crc(by block_contents)  |
    char trailer[kBlockTrailerSize];
    trailer[0] = type;
    uint32_t crc = crc32c::Value(block_contents.data(), block_contents.size());
    crc = crc32c::Extend(crc, trailer, 1);  // Extend crc to cover block type
    EncodeFixed32(trailer+1, crc32c::Mask(crc));
    //接着写5 bytes的block trailer(type + crc)
    r->status = r->file->Append(Slice(trailer, kBlockTrailerSize));
    if (r->status.ok()) {
      r->offset += block_contents.size() + kBlockTrailerSize;
    }
  }
}

Status TableBuilder::status() const {
  return rep_->status;
}

Status TableBuilder::Finish() {
  Rep* r = rep_;
  //更新未写入的block
  Flush();
  assert(!r->closed);
  r->closed = true;

  //注意接下来只调用了r->file->Append
  BlockHandle filter_block_handle, metaindex_block_handle, index_block_handle;

  // Write filter block
  // 一次性写入filter block
  if (ok() && r->filter_block != nullptr) {
    WriteRawBlock(r->filter_block->Finish(), kNoCompression,
                  &filter_block_handle);
  }

  // Write metaindex block
  // 写入index of filter block，这里称为meta_index_block
  if (ok()) {
    //meta_index_block只写入一条数据
    //key: filter.$filter_name
    //value: filter_block的起始位置和大小
    BlockBuilder meta_index_block(&r->options);
    if (r->filter_block != nullptr) {
      // Add mapping from "filter.Name" to location of filter data
      std::string key = "filter.";
      key.append(r->options.filter_policy->Name());
      std::string handle_encoding;
      filter_block_handle.EncodeTo(&handle_encoding);
      meta_index_block.Add(key, handle_encoding);
    }

    // TODO(postrelease): Add stats and other meta blocks
    WriteBlock(&meta_index_block, &metaindex_block_handle);
  }

  // Write index block
  // 写入Rep->index_block
  if (ok()) {
    if (r->pending_index_entry) {
      //第一个>r->last_key的字符串
      //例如r->last_key = "ace"，调用后r->last_key = "b"
      r->options.comparator->FindShortSuccessor(&r->last_key);
      std::string handle_encoding;
      r->pending_handle.EncodeTo(&handle_encoding);
      r->index_block.Add(r->last_key, Slice(handle_encoding));
      r->pending_index_entry = false;
    }
    WriteBlock(&r->index_block, &index_block_handle);
  }

  // Write footer
  if (ok()) {
    Footer footer;
    footer.set_metaindex_handle(metaindex_block_handle);
    footer.set_index_handle(index_block_handle);
    std::string footer_encoding;
    footer.EncodeTo(&footer_encoding);
    r->status = r->file->Append(footer_encoding);
    if (r->status.ok()) {
      r->offset += footer_encoding.size();
    }
  }
  return r->status;
}

void TableBuilder::Abandon() {
  Rep* r = rep_;
  assert(!r->closed);
  r->closed = true;
}

uint64_t TableBuilder::NumEntries() const {
  return rep_->num_entries;
}

uint64_t TableBuilder::FileSize() const {
  return rep_->offset;
}

}  // namespace leveldb
