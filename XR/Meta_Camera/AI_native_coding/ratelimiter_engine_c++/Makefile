CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pthread
IMPL ?= project

BUILD := build
SRCS := $(IMPL)/memory_store.cpp $(IMPL)/ratelimiter.cpp
HDRS := $(wildcard $(IMPL)/*.h)

.PHONY: test demo clean

test: $(BUILD)/test_ratelimiter
	$(BUILD)/test_ratelimiter

demo: $(BUILD)/demo
	$(BUILD)/demo

$(BUILD)/test_ratelimiter: tests/test_ratelimiter.cpp $(SRCS) $(HDRS)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I$(IMPL) tests/test_ratelimiter.cpp $(SRCS) -o $@

$(BUILD)/demo: $(IMPL)/main.cpp $(SRCS) $(HDRS)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I$(IMPL) $(IMPL)/main.cpp $(SRCS) -o $@

clean:
	rm -rf $(BUILD)
