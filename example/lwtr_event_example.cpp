/*******************************************************************************
 * Copyright 2023 MINRES Technologies GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *******************************************************************************/

#include <chrono>
#include <list>
#if defined(HAS_SCC)
#include <scc/report.h>
#define SCP_INFO(X) SCCINFO(X)
#else
#include <scp/report.h>
#endif

#include "lwtr/lwtr.h"

using namespace sc_core;
using namespace sc_dt;

// Pipeline stages for RISC-V
enum class PipelineStage { FETCH, DECODE, EXECUTE, MEMORY, WRITEBACK };

const char* stage_name(PipelineStage stage) {
    switch (stage) {
        case PipelineStage::FETCH:     return "IF";
        case PipelineStage::DECODE:    return "ID";
        case PipelineStage::EXECUTE:   return "EX";
        case PipelineStage::MEMORY:    return "MEM";
        case PipelineStage::WRITEBACK: return "WB";
    }
    return "UNKNOWN";
}

// Memory fabric stages
enum class MemoryStage { REQUEST, ARBITRATE, ROUTE, ACCESS, RESPONSE };

const char* mem_stage_name(MemoryStage stage) {
    switch (stage) {
        case MemoryStage::REQUEST:   return "REQ";
        case MemoryStage::ARBITRATE: return "ARB";
        case MemoryStage::ROUTE:     return "ROUTE";
        case MemoryStage::ACCESS:    return "ACCESS";
        case MemoryStage::RESPONSE:  return "RESP";
    }
    return "UNKNOWN";
}

class test : public sc_module {
public:
    SC_HAS_PROCESS(test);
    test(::sc_core::sc_module_name) {
        SC_THREAD(main);
    }
    void main();
};

void test::main() {
    // Create fibers: CPU Core and Memory
    // Fiber IDs: 0 = CPU Core, 1 = Memory
    // Generator IDs: 0 = cpu_tx, 1 = cpu_events, 2 = mem_tx, 3 = mem_events
    lwtr::tx_fiber cpu_fiber((std::string(name()) + ".CPU_Core").c_str(), "transactions");
    lwtr::tx_fiber mem_fiber((std::string(name()) + ".Memory").c_str(), "transactions");
    lwtr::tx_generator<char const*, char const*> instruction_gen("instruction", cpu_fiber,"mnemonic", "result", true);
    lwtr::tx_generator<char const*> bus_transaction_gen("bus_transaction", mem_fiber, "op", true);

    uint64_t tx_id = 1;
    uint64_t cycle = 0;
    const sc_core::sc_time CYCLE_TIME = 10_ns;  // 10 ns per cycle
    const uint64_t STAGE_LATENCY = 1;  // 1 cycle per pipeline stage

    // Simulate dual-issue CPU executing instructions
    struct InstructionInfo {
        const char* mnemonic;
        const char* type;
        uint64_t addr;
        bool is_memory_op;
        uint64_t mem_addr;
        const char* mem_type;
    };

    std::vector<InstructionInfo> program = {
        {"ADD",   "R-type", 0x1000, false, 0, nullptr},
        {"SUB",   "R-type", 0x1004, false, 0, nullptr},
        {"LW",    "I-type", 0x1008, true,  0x2000, "load"},
        {"ADDI",  "I-type", 0x100C, false, 0, nullptr},
        {"SW",    "S-type", 0x1010, true,  0x2004, "store"},
        {"BEQ",   "B-type", 0x1014, false, 0, nullptr},
        {"LW",    "I-type", 0x1018, true,  0x2008, "load"},
        {"AND",   "R-type", 0x101C, false, 0, nullptr},
        {"OR",    "R-type", 0x1020, false, 0, nullptr},
        {"SW",    "S-type", 0x1024, true,  0x200C, "store"},
    };

    // Process instructions in pairs (dual issue)
    for (size_t i = 0; i < program.size(); i += 2) {
        auto issue_time = cycle * CYCLE_TIME;

        // Issue up to 2 instructions simultaneously
        for (size_t j = 0; j < 2; ++j) {
            if(i+j>=program.size()) continue;
            const auto& insn = program[i+j];
            uint64_t insn_tx_id = tx_id++;

            // Create instruction transaction
            auto insn_tx = instruction_gen.begin_tx_delayed(issue_time, insn.mnemonic);
            insn_tx.record_attribute("type", insn.type);
            insn_tx.record_attribute("pc", insn.addr);
            insn_tx.record_attribute("issue_slot", static_cast<uint64_t>(j - i));
            // Record pipeline stage events
            auto stage_time = issue_time;
            for (int s = 0; s < 5; ++s) {
                PipelineStage stage = static_cast<PipelineStage>(s);
                insn_tx.record_event(stage_name(stage),stage_time,
                                     "stage_id", static_cast<uint64_t>(s),
                                     "pc", insn.addr);
                stage_time += STAGE_LATENCY * CYCLE_TIME;
            }

            // If it's a memory operation, create child memory transaction
            if (insn.is_memory_op) {
                uint64_t mem_tx_id = tx_id++;
                auto mem_start = issue_time + 3 * STAGE_LATENCY * CYCLE_TIME;  // MEM stage

                // Create memory transaction as child
                auto mem_tx = bus_transaction_gen.begin_tx_delayed(mem_start, insn.mem_type);
                mem_tx.record_attribute("addr", insn.mem_addr);
                mem_tx.record_attribute("size", static_cast<uint64_t>(4));
                mem_tx.add_relation("parent_of", insn_tx);
                // Record memory fabric progression events
                auto mem_stage_time = mem_start;
                for (int s = 0; s < 5; ++s) {
                    MemoryStage stage = static_cast<MemoryStage>(s);
                    mem_tx.record_event(mem_stage_name(stage), mem_stage_time,
                                        "fabric_node", static_cast<uint64_t>(s),
                                        "addr", insn.mem_addr);
                    mem_stage_time += 2 * CYCLE_TIME;  // Memory fabric has 2-cycle latency per stage
                }

                // Memory transaction ends after all fabric stages
                mem_tx.end_tx_delayed(mem_stage_time);

                // Instruction takes longer due to memory access
                insn_tx.end_tx_delayed<char const*>(mem_stage_time, "ok");
            } else {
                // Regular instruction ends after pipeline
                insn_tx.end_tx_delayed<char const*>(stage_time, "ok");
            }
        }
        // Advance cycle for next pair
        cycle += 5;  // Basic pipeline throughput
    }
    wait(cycle);
}

int sc_main(int argc, char* argv[]) {
    auto start = std::chrono::system_clock::now();
#if defined(HAS_SCC)
    scc::init_logging(scc::log::DEBUG);
#else
    scp::init_logging(scp::log::DEBUG);
#endif
    lwtr::tx_ftr_init(true);
    lwtr::tx_db db("my_db");
    lwtr::tx_db::set_default_db(&db);
    // create modules/channels
    test i_test("i_test");
    // run the simulation
    sc_start(10.0, SC_US);
    auto int_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start);
    SCP_INFO("sc_main") << "simulation duration " << int_us.count() << "µs";
    return 0;
}
