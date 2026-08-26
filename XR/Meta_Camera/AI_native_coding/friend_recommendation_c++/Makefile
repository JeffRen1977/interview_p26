CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
IMPL ?= project

BUILD := build
BIN := $(BUILD)/test_$(IMPL)
SRCS := $(IMPL)/social_graph.cpp $(IMPL)/recommenders.cpp
HDRS := $(IMPL)/social_graph.h $(IMPL)/recommenders.h

.PHONY: test test-project test-solution clean

test: $(BIN)
	$(BIN)

$(BIN): tests/test_friend_recommendation.cpp $(SRCS) $(HDRS)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I$(IMPL) tests/test_friend_recommendation.cpp $(SRCS) -o $(BIN)

test-project:
	$(MAKE) IMPL=project test

test-solution:
	$(MAKE) IMPL=solution test

clean:
	rm -rf $(BUILD)
