CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
IMPL ?= project

BUILD := build
BIN := $(BUILD)/test_$(IMPL)

.PHONY: test test-project test-solution test-phase1 test-phase2 test-phase3 clean

test: $(BIN)
	$(BIN)

$(BIN): tests/test_ttl_kv_index.cpp $(IMPL)/store.cpp $(IMPL)/store.h $(IMPL)/clock.h
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I$(IMPL) tests/test_ttl_kv_index.cpp $(IMPL)/store.cpp -o $(BIN)

test-project:
	$(MAKE) IMPL=project test

test-solution:
	$(MAKE) IMPL=solution test

# Filter by printing only matching names is not built in; run full suite per IMPL.
test-phase1 test-phase2 test-phase3: test

clean:
	rm -rf $(BUILD)
