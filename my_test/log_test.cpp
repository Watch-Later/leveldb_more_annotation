/***************************************************************************
 * 
 * Copyright (c) 2018 Baidu.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 /**
 * @file log_test.cpp
 * @author zhangying21(zhangying21@baidu.com)
 * @date 2018/10/11 20:53:57
 * @version $Revision$ 
 * @brief 
 *  
 **/

#include <iostream>
#include "util/logging.h"
#include "leveldb/slice.h"

int main() {
    {
        leveldb::Slice in = "0012345.log";
        uint64_t value = 0;
        bool parsed = leveldb::ConsumeDecimalNumber(&in, &value);
        std::cout << "parsed:" << parsed << " value:" << value << std::endl;
        std::cout << "in:" << in.ToString() << std::endl;
    }

    {
        leveldb::Slice in = "18446744073709551615.suffix";
        uint64_t value = 0;
        bool parsed = leveldb::ConsumeDecimalNumber(&in, &value);
        std::cout << "parsed:" << parsed << " value:" << value << std::endl;
        std::cout << "in:" << in.ToString() << std::endl;
    }

    return 0;
}

/* vim: set ts=4 sw=4 sts=4 tw=100 */
