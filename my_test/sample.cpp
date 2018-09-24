/***************************************************************************
 * 
 * Copyright (c) 2018 Baidu.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 /**
 * @file sample.cpp
 * @author zhangying21(zhangying21@baidu.com)
 * @date 2018/08/05 14:00:53
 * @version $Revision$ 
 * @brief 
 *  
 **/

#include <assert.h>
#include <iostream>
#include "leveldb/db.h"

int main() {
    leveldb::DB* db = NULL;
    leveldb::Options options;
    options.create_if_missing = true;

    leveldb::Status status = leveldb::DB::Open(options, "./data/new_sample.db", &db);
    assert(status.ok());

    // std::string key = "hello";
    // std::string value = "izual";

    // status = db->Put(leveldb::WriteOptions(), key, value);
    // assert(status.ok());

    // std::string db_value;
    // status = db->Get(leveldb::ReadOptions(), key, &db_value);
    // assert(status.ok());

    // std::cout << key << " : " << db_value << std::endl;

    return 0;
}
/* vim: set ts=4 sw=4 sts=4 tw=100 */
