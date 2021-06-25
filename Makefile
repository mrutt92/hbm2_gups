all: exe

BASEJUMP_STL_DIR=$(abspath $(PWD)/../basejump_stl)
BSG_MANYCORE_DIR=$(abspath $(PWD)/../bsg_manycore)

CXXFLAGS += -O2
CXXFLAGS += -std=c++11 -D_GNU_SOURCE -Wall
CXXFLAGS += -I$(BASEJUMP_STL_DIR)/imports/DRAMSim3/src
CXXFLAGS += -I$(BASEJUMP_STL_DIR)/imports/DRAMSim3/ext/headers
CXXFLAGS += -I$(BASEJUMP_STL_DIR)/imports/DRAMSim3/ext/fmt/include
CXXFLAGS += -DFMT_HEADER_ONLY=1
CXXFLAGS += -DCMD_TRACE
CXXFLAGS += -DBASEJUMP_STL_DIR=$(BASEJUMP_STL_DIR)
CXXFLAGS += -DBLOOD_GRAPH

exe: $(BASEJUMP_STL_DIR)/imports/DRAMSim3/src/bankstate.cc
exe: $(BASEJUMP_STL_DIR)/imports/DRAMSim3/src/channel_state.cc
exe: $(BASEJUMP_STL_DIR)/imports/DRAMSim3/src/command_queue.cc
exe: $(BASEJUMP_STL_DIR)/imports/DRAMSim3/src/common.cc
exe: $(BASEJUMP_STL_DIR)/imports/DRAMSim3/src/configuration.cc
exe: $(BASEJUMP_STL_DIR)/imports/DRAMSim3/src/controller.cc
exe: $(BASEJUMP_STL_DIR)/imports/DRAMSim3/src/dram_system.cc
exe: $(BASEJUMP_STL_DIR)/imports/DRAMSim3/src/hmc.cc
exe: $(BASEJUMP_STL_DIR)/imports/DRAMSim3/src/memory_system.cc
exe: $(BASEJUMP_STL_DIR)/imports/DRAMSim3/src/refresh.cc
exe: $(BASEJUMP_STL_DIR)/imports/DRAMSim3/src/simple_stats.cc
exe: $(BASEJUMP_STL_DIR)/imports/DRAMSim3/src/timing.cc
exe: $(BASEJUMP_STL_DIR)/imports/DRAMSim3/src/blood_graph.cc
exe: main.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(filter %.cpp,$^) $(filter %.cc,$^)

blood_graph_ch0.log: exe
	./$<

blood.png: blood_graph_ch0.log
	python3 $(BSG_MANYCORE_DIR)/software/py/dramsim3_blood_graph.py  $< $@

clean:
	rm -f *.o *~ exe
