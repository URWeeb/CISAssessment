# Шорткаты над CMake/CTest.
BUILD_DIR := build
STRESS_FILTER := BoxAppendIsConcurrencySafe|SubmitContributionConcurrentSubmissionsPublishExactlyOnce
STRESS_REPEATS := 30

.PHONY: configure compile build test stress-test format format-check run-server run-client clean

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release

compile:
	cmake --build $(BUILD_DIR) -j

build: configure compile

test:
	ctest --test-dir $(BUILD_DIR) --output-on-failure

stress-test:
	ctest --test-dir $(BUILD_DIR) --output-on-failure -R "$(STRESS_FILTER)" --repeat until-fail:$(STRESS_REPEATS)

format:
	find include src tests -name "*.hpp" -o -name "*.cpp" | xargs clang-format -i

format-check:
	find include src tests -name "*.hpp" -o -name "*.cpp" | xargs clang-format --dry-run --Werror

run-server:
	./$(BUILD_DIR)/publisher_server --port=7878 --roster=configs/clients.csv

run-client:
	./$(BUILD_DIR)/publisher_client 127.0.0.1 7878 $(ARGS)

clean:
	rm -rf $(BUILD_DIR)