.PHONY: all test art
all:
	python3 tools/build.py
art:
	python3 tools/art.py
test: all
	python3 tools/check_rom.py
	python3 tests/playtest.py
	python3 tests/edgecases.py
