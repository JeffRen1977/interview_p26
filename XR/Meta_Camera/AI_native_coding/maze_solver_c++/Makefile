CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
IMPL ?= project

BUILD := build
BIN := $(BUILD)/test_$(IMPL)
SRCS := $(IMPL)/grid.cpp $(IMPL)/maze.cpp $(IMPL)/renderer.cpp $(IMPL)/solver.cpp
HDRS := $(wildcard $(IMPL)/*.h)

.PHONY: test test-project test-solution clean

test: $(BIN)
	$(BIN)

$(BIN): tests/test_maze_solver.cpp $(SRCS) $(HDRS)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I$(IMPL) tests/test_maze_solver.cpp $(SRCS) -o $(BIN)

test-project:
	$(MAKE) IMPL=project test

test-solution:
	$(MAKE) IMPL=solution test

clean:
	rm -rf $(BUILD)
