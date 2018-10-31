#include <iostream>
#include "table/block_builder.h"
#include "leveldb/options.h"

int main() {
    leveldb::Options option;
    option.block_restart_interval = 4;

    leveldb::BlockBuilder block_builder(&option);

    //|shared key len  |non-shared key len  | value len  |non-shared key  |value  |
    //restarts_ = [0]
    //0x00 07 05 c o n f u s e v a l u e
    block_builder.Add("confuse", "value");
    //0x03 04 05 t e n d v a l u e
    block_builder.Add("contend", "value");
    //0x02 02 05 p e v a l u e
    block_builder.Add("cope", "value");
    //0x03 01 05 y v a l u e
    block_builder.Add("copy", "value");
    //当前buufer大小为2e
    //restarts_ = [0, 2e]
    //0x00 04 05 c o r n v a l u e
    block_builder.Add("corn", "value");

    //0x00 00 00 00 2e 00 00 00 02 00 00 00
    leveldb::Slice block_builder_buffer = block_builder.Finish();

    // 00000000: 0007 0563 6f6e 6675 7365 7661 6c75 6503  ...confusevalue.
    // 00000010: 0405 7465 6e64 7661 6c75 6502 0205 7065  ..tendvalue...pe
    // 00000020: 7661 6c75 6503 0105 7976 616c 7565 0004  value...yvalue..
    // 00000030: 0563 6f72 6e76 616c 7565 0000 0000 2e00  .cornvalue......
    // 00000040: 0000 0200 0000                           ......
    std::cout << block_builder_buffer.ToString();

    return 0;
}

/* vim: set ts=4 sw=4 sts=4 tw=100 */
