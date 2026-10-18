CXX ?= g++
CXXFLAGS ?= -O2 -g -Wall -Wextra -Wpedantic
WX_CONFIG ?= wx-config

all: sk150

sk150: src/main.cpp src/modbus.cpp src/modbus.hpp src/display.hpp src/dialogs.hpp src/connection.hpp src/controls.hpp src/setpoints.hpp src/resources.hpp
	$(CXX) $(CXXFLAGS) -std=c++17 -pthread $$($(WX_CONFIG) --cxxflags) src/main.cpp src/modbus.cpp -o $@ $$($(WX_CONFIG) --libs)

src/resources.hpp: assets/MD_PCB_Logo.svg LICENSE NOTICE tools/embed_resources.py
	python3 tools/embed_resources.py

protocol-test: tests/protocol_test.cpp src/modbus.cpp src/modbus.hpp src/setpoints.hpp
	$(CXX) $(CXXFLAGS) -std=c++17 -pthread -Isrc tests/protocol_test.cpp src/modbus.cpp -o $@

display-test: tests/display_test.cpp src/display.hpp src/modbus.hpp
	$(CXX) $(CXXFLAGS) -std=c++17 -Isrc $$($(WX_CONFIG) --cxxflags) tests/display_test.cpp -o $@ $$($(WX_CONFIG) --libs base)

connection-test: tests/connection_test.cpp src/connection.hpp
	$(CXX) $(CXXFLAGS) -std=c++17 -Isrc tests/connection_test.cpp -o $@

gui-test: tests/gui_test.cpp src/main.cpp src/dialogs.hpp src/resources.hpp src/connection.hpp src/controls.hpp src/setpoints.hpp src/modbus.cpp
	$(CXX) $(CXXFLAGS) -std=c++17 -pthread -Isrc $$($(WX_CONFIG) --cxxflags) tests/gui_test.cpp src/modbus.cpp -o $@ $$($(WX_CONFIG) --libs)

test: protocol-test display-test connection-test
	./connection-test
	./display-test
	./protocol-test
	python3 tests/serial_test.py ./protocol-test
	python3 tests/output_serial_test.py ./protocol-test
	python3 tests/limit_mode_serial_test.py ./protocol-test
	python3 tests/protection_serial_test.py ./protocol-test
	python3 tests/serial_opp_test.py ./protocol-test
	python3 tests/targets_serial_test.py ./protocol-test

clean:
	rm -f sk150 protocol-test display-test connection-test gui-test
.PHONY: all test clean
