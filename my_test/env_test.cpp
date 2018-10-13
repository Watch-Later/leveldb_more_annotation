/***************************************************************************
 * 
 * Copyright (c) 2018 Baidu.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 /**
 * @file my_test/env_test.cpp
 * @author zhangying21(zhangying21@baidu.com)
 * @date 
 * @version $Revision$ 
 * @brief 
 *  
 **/

#include <iostream>

#include "leveldb/env.h"

void test_GetChildren() {
    const std::string dir= "./data/sample.db";
    std::vector<std::string> result;
    leveldb::Status s = leveldb::Env::Default()->GetChildren(dir, &result);

    std::cout << s.ToString() << std::endl;
    for (const auto file : result) {
        std::cout << file << std::endl;
    }
}

int main() {
    std::cout << leveldb::Env::Default() << std::endl;

    test_GetChildren();

    return 0;
}

/* vim: set ts=4 sw=4 sts=4 tw=100 */
