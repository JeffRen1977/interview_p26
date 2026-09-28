CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
IMPL ?= project

BUILD := build
BIN := $(BUILD)/test_$(IMPL)
SRCS := $(IMPL)/wordlist.cpp $(IMPL)/solver.cpp
HDRS := $(IMPL)/wordlist.h $(IMPL)/solver.h

.PHONY: test test-project test-solution clean

test: $(BIN)
	$(BIN)

$(BIN): tests/test_max_unique_chars.cpp $(SRCS) $(HDRS)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I$(IMPL) tests/test_max_unique_chars.cpp $(SRCS) -o $(BIN)

test-project:
	$(MAKE) IMPL=project test

test-solution:
	$(MAKE) IMPL=solution test

clean:
	rm -rf $(BUILD)
