.PHONY: check build test clean

check: build test

build:
	cmake -B build
	cmake --build build

test:
	ctest --test-dir build --output-on-failure

clean:
	rm -rf build
