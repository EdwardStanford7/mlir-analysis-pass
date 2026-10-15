BUILD_DIR ?= build
PLUGIN ?= SignAnalysis
INPUT ?= input.mlir
REDUCTIONS_DIR ?= reductions
CONFIG_STAMP := $(BUILD_DIR)/.configured
TEST ?= *
TEST_OUTPUT_DIR ?= $(BUILD_DIR)/instrumentation-tests
TEST_TIMEOUT ?= 5

all: build

# Show useful inferred facts. Constants and the type-level fact that an LLVM
# comparison is zero-or-positive are hidden.
run: build $(INPUT)
	./run.sh $(INPUT) | awk '/is (zero|negative|positive)/ && !/llvm\.mlir\.constant/ && !(/llvm\.icmp/ && /is zero-or-positive$$/)'

# Show every fact emitted by the analysis, including constants.
run-all: build $(INPUT)
	./run.sh $(INPUT) | grep -E 'is (zero|negative|positive)'

# Keep generated IR, annotations, executables, and logs for debugging.
test:
	@build_log=$$(mktemp); \
	trap 'rm -f "$$build_log"' EXIT; \
	$(MAKE) --no-print-directory build > "$$build_log" 2>&1 || { cat "$$build_log"; exit 1; }
	@python3 tests/run.py --build-dir "$(BUILD_DIR)" --output-dir "$(TEST_OUTPUT_DIR)" --case '$(TEST)' --timeout "$(TEST_TIMEOUT)"

check-reductions: build
	@for id in 1 2 3; do \
		echo "Checking input$$id.ll..."; \
		$(REDUCTIONS_DIR)/test$$id.sh $(REDUCTIONS_DIR)/input$$id.ll || exit $$?; \
	done

reduce1 reduce2 reduce3: reduce%: build
	@echo "Checking that input$*.ll is interesting..."
	$(REDUCTIONS_DIR)/test$*.sh $(REDUCTIONS_DIR)/input$*.ll
	llvm-reduce \
		--test=$(abspath $(REDUCTIONS_DIR)/test$*.sh) \
		--output=$(REDUCTIONS_DIR)/.reduced$*.ll \
		$(REDUCTIONS_DIR)/input$*.ll
	mlir-translate --import-llvm $(REDUCTIONS_DIR)/.reduced$*.ll > $(REDUCTIONS_DIR)/reduced$*.mlir

reduce-all: reduce1 reduce2 reduce3

input.mlir: sqlite3.c
	clang -S -emit-llvm -o - sqlite3.c | mlir-translate --import-llvm > input.mlir

build: $(CONFIG_STAMP)
	cmake --build $(BUILD_DIR) --target $(PLUGIN)

$(CONFIG_STAMP): CMakeLists.txt
	cmake -S . -B $(BUILD_DIR)
	@touch $(CONFIG_STAMP)

clean:
	@if test -f "$(BUILD_DIR)/CMakeCache.txt"; then \
		cmake --build $(BUILD_DIR) --target clean; \
	fi

.PHONY: all build check-reductions clean reduce1 reduce2 reduce3 reduce-all run run-all test
