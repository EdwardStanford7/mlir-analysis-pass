BUILD_DIR ?= build
PLUGIN ?= SignAnalysis
INPUT ?= input.mlir
REDUCTIONS_DIR ?= reductions
CONFIG_STAMP := $(BUILD_DIR)/.configured

all: build

# Show useful inferred facts. Constants and the type-level fact that an LLVM
# comparison is zero-or-positive are hidden.
run: build $(INPUT)
	./run.sh $(INPUT) | awk '/is (zero|negative|positive)/ && !/llvm\.mlir\.constant/ && !(/llvm\.icmp/ && /is zero-or-positive$$/)'

# Show every fact emitted by the analysis, including constants.
run-all: build $(INPUT)
	./run.sh $(INPUT) | grep -E 'is (zero|negative|positive)'

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

.PHONY: all build check-reductions clean reduce1 reduce2 reduce3 reduce-all run run-all
