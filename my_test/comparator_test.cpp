#include <iostream>
#include "leveldb/comparator.h"
#include "leveldb/slice.h"

int main() {
    const leveldb::Comparator* comparator = leveldb::BytewiseComparator();
    std::cout << comparator->Name() << std::endl;

    const leveldb::Slice a("a");
    const leveldb::Slice b("b");

    std::cout << comparator->Compare(a, b) << std::endl;//-1
    std::cout << comparator->Compare(a, a) << std::endl;//0
    std::cout << comparator->Compare(b, a) << std::endl;//1

    {
        std::string start("abcdefg");
        const leveldb::Slice slice("abcdxyz");

        std::cout << "start:" << start << " slice:" << slice.ToString() << std::endl;
        comparator->FindShortestSeparator(&start, slice);
        std::cout << "start:" << start << " size:" << start.size() << std::endl;//start:hellp size:5
    }

    {
        std::string start("abcdxyz");
        const leveldb::Slice slice("abcdefg");

        std::cout << "start:" << start << " slice:" << slice.ToString() << std::endl;
        comparator->FindShortestSeparator(&start, slice);
        std::cout << "start:" << start << " size:" << start.size() << std::endl;//start:hellp size:5
    }

    {
        std::string start("abcdefg");
        const leveldb::Slice slice("abcdffg");

        std::cout << "start:" << start << " slice:" << slice.ToString() << std::endl;
        comparator->FindShortestSeparator(&start, slice);
        std::cout << "start:" << start << " size:" << start.size() << std::endl;//start:hellp size:5
    }

    {
        std::string start("ace");
        std::cout << "start:" << start << std::endl;
        comparator->FindShortSuccessor(&start);
        std::cout << "start:" << start << std::endl;
    }

    {
        std::string start(1, 0xff);
        start.append("world");
        for (int i = 0; i < start.size(); ++i) {
            printf("%02x ", (unsigned char)start[i]);
        }
        printf("\n");
        comparator->FindShortSuccessor(&start);
        for (int i = 0; i < start.size(); ++i) {
            printf("%02x ", (unsigned char)start[i]);
        }
        printf("\n");
    }

    return 0;
}

/* vim: set ts=4 sw=4 sts=4 tw=100 */
