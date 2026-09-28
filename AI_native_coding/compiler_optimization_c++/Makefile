CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
IMPL ?= project

BUILD := build
BIN := $(BUILD)/test_$(IMPL)
SRCS := $(IMPL)/optimizer_utils.cpp $(IMPL)/compiler_optimizer.cpp
HDRS := $(IMPL)/optimizer_utils.h $(IMPL)/compiler_optimizer.h

.PHONY: test test-project test-solution clean

test: $(BIN)
	$(BIN)

$(BIN): tests/test_compiler_optimizer.cpp $(SRCS) $(HDRS)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I$(IMPL) tests/test_compiler_optimizer.cpp $(SRCS) -o $(BIN)

test-project:
	$(MAKE) IMPL=project test

test-solution:
	$(MAKE) IMPL=solution test

clean:
	rm -rf $(BUILD)
