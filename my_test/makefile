LIBS=../build/libleveldb.a
CPPFLAGS=-I ../build/include -I ../include -I .. -std=c++11 -g -DLEVELDB_PLATFORM_POSIX -DLEVELDB_HAS_PORT_CONFIG_H
LDFLAGS=-lpthread

sample:sample.cpp
	$(CXX) -o $@ $< $(LIBS) $(CPPFLAGS) $(LDFLAGS)

version_edit_test:version_edit_test.cpp
	$(CXX) -o $@ $< $(LIBS) $(CPPFLAGS) $(LDFLAGS)

version_set_test:version_set_test.cpp
	$(CXX) -o $@ $< $(LIBS) $(CPPFLAGS) $(LDFLAGS)

posix_writable_file_test:posix_writable_file_test.cpp
	$(CXX) -o $@ $< $(LIBS) $(CPPFLAGS) $(LDFLAGS)

log_writer_test:log_writer_test.cpp
	$(CXX) -o $@ $< $(LIBS) $(CPPFLAGS) $(LDFLAGS)

filename_test:filename_test.cpp
	$(CXX) -o $@ $< $(LIBS) $(CPPFLAGS) $(LDFLAGS)

env_test:env_test.cpp
	$(CXX) -o $@ $< $(LIBS) $(CPPFLAGS) $(LDFLAGS)

log_test:log_test.cpp
	$(CXX) -o $@ $< $(LIBS) $(CPPFLAGS) $(LDFLAGS)

crc32c_test:crc32c_test.cpp
	$(CXX) -o $@ $< $(LIBS) $(CPPFLAGS) $(LDFLAGS)
