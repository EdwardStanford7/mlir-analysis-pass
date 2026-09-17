BUILD_DIR ?= build
PLUGIN ?= SignAnalysis
INPUT ?= input.mlir

all: build

run: build $(INPUT)
	./run.sh $(INPUT) | grep -E 'is (zero|negative|positive)'

run-no-constants: build $(INPUT)
	./run.sh $(INPUT) | awk '/is (zero|negative|positive)/ && !/llvm\.mlir\.constant/'

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

input.mlir: sqlite3.c
	clang -S -emit-llvm -o - sqlite3.c | mlir-translate --import-llvm > input.mlir

build: $(BUILD_DIR)/CMakeCache.txt
	cmake --build $(BUILD_DIR) --target $(PLUGIN)

$(BUILD_DIR)/CMakeCache.txt: CMakeLists.txt
	cmake -S . -B $(BUILD_DIR)

clean:
	@if test -f "$(BUILD_DIR)/CMakeCache.txt"; then \
		cmake --build $(BUILD_DIR) --target clean; \
	fi

.PHONY: all build clean run run-no-constants test
