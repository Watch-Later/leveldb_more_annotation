#include <iostream>

#include "db/log_format.h"
#include "util/crc32c.h"

int main() {
    std::string type(1, leveldb::log::kFullType);
    std::string value("hello world");

    uint32_t type_crc = leveldb::crc32c::Value(type.c_str(), type.size());
    std::cout << leveldb::crc32c::Extend(type_crc, value.c_str(), value.size()) << std::endl;

    std::string buffer = type + value;
    std::cout << leveldb::crc32c::Value(buffer.c_str(), buffer.size()) << std::endl;

    return 0;
}
/* vim: set ts=4 sw=4 sts=4 tw=100 */
