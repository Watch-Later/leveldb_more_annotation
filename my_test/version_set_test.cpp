/***************************************************************************
 * 
 * Copyright (c) 2018 Baidu.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 /**
 * @file my_test/version_set_test.cpp
 * @author zhangying21(zhangying21@baidu.com)
 * @date 2018/08/18 22:38:48
 * @version $Revision$ 
 * @brief 
 *  
 **/

#include <iostream>
#include "leveldb/db.h"
#include "db/version_set.h"

int main() {
    leveldb::DB* db = NULL;
    leveldb::Options options;
    options.create_if_missing = true;
    // std::string dbname = "./data/sample.db";
    std::string dbname = "./data/new_sample.db";
    leveldb::InternalKeyComparator internal_key_comparator(options.comparator);

    auto versions_ = new leveldb::VersionSet(
            dbname,
            &options,
            NULL,
            &internal_key_comparator);
    bool save_manifest = true;
    leveldb::Status status = versions_->Recover(&save_manifest);

    std::cout << status.ToString() << std::endl;
    std::cout << versions_->current()->DebugString() << std::endl;
    std::cout << "LogNumber:" << versions_->LogNumber() << std::endl;
    std::cout << "PrevLogNumber:" << versions_->PrevLogNumber() << std::endl;
    std::cout << "LastSequence:" << versions_->LastSequence() << std::endl;
    std::cout << "ManifestFileNumber:" << versions_->ManifestFileNumber() << std::endl;
    std::cout << "NumLevelFiles(0):" << versions_->NumLevelFiles(0) << std::endl;
    std::cout << "NewFileNumber:" << versions_->NewFileNumber() << std::endl;

    return 0;
}

/* vim: set ts=4 sw=4 sts=4 tw=100 */
