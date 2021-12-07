#include <vector>
#include <cstdio>
#include <cstdint>
#include <string>
#include <memory>
#include <map>
#include <queue>
#include <deque>
#include <algorithm>
#include <inttypes.h>
#include "memory_system.h"
#include "configuration.h"

#ifdef DEBUG
#define pr_dbg(fmt, ...)                        \
    do { printf(fmt, ##__VA_ARGS__); fflush(NULL); } while (0)
#else
#define pr_dbg(...)
#endif

#define N_UPDATES (64*1024)
#define TABLE_SIZE (256*1024*1024)

#define __stringify(x)                          \
    #x
#define stringify(x)                            \
    __stringify(x)

using namespace std;
string config_path = stringify(BASEJUMP_STL_DIR) "/imports/DRAMSim3/configs/HBM2_8Gb_x128.ini";

using dramsim3::MemorySystem;
using dramsim3::Config;
using dramsim3::Address;

MemorySystem *memsys = nullptr;

//////////////////////////
// GUPS data structures //
//////////////////////////
typedef enum {
    READ_READY = 1,
    READ_ISSUED,
    WRITE_READY,
    WRITE_ISSUED,
    WRITE_DONE,
} gups_update_status_t;

const char *gups_update_status_to_string(gups_update_status_t st)
{
    switch(st) {
    case READ_READY:   return "read ready";
    case READ_ISSUED:  return "read issued";
    case WRITE_READY:  return "write ready";
    case WRITE_ISSUED: return "write issued";
    case WRITE_DONE:   return "write done";
    default:           return "unknown";
    }
}


typedef struct gups_update
{
    uint64_t             address;
    gups_update_status_t status;
} gups_update_t;

static vector<gups_update> gups_updates;
static map<uint64_t, int> gups_addr_to_update;
static deque<int> read_ready;
static deque<int> write_ready;

std::uniform_int_distribution<int> dist(0, TABLE_SIZE);
std::default_random_engine gen;

static uint64_t gups_make_random_address()
{
    const Config *cfg = memsys->GetConfig();
    while (true) {
        // generate a random address on channel 0, rank 0
        uint64_t addr = dist(gen);
        // clear rank and channel
        uint64_t mask = ((cfg->ra_mask << cfg->ra_pos)|(cfg->ch_mask << cfg->ch_pos)) << cfg->shift_bits;
        addr &= ~mask;

        // try again if address is already in map
        auto it = gups_addr_to_update.find(addr);
        if (it == gups_addr_to_update.end()) {
            Address cfg_addr = cfg->AddressMapping(addr);
            pr_dbg("addr  = %08" PRIx64 ": ch = %d\n", addr, cfg_addr.channel);
            return addr;
        }
    }
}

static void gups_setup_updates()
{

    for (int i = 0; i < N_UPDATES; i++) {
        // generate a random address for channel 0

        uint64_t addr = gups_make_random_address();

        // create an update struct
        gups_update_t update;
        update.address = addr;
        update.status = READ_READY;

        gups_updates.push_back(update);
        gups_addr_to_update[addr] = i;
        read_ready.push_back(i);
    }
}

///////////////////////
// GUPS control flow //
///////////////////////
static int gups_done()
{
    for (auto & update : gups_updates)
        if (update.status != WRITE_DONE)
            return 0;

    // all done
    return 1;
}

// return request sent successfully
static int gups_try_send_requests()
{
    int count = 0;
    // do all ready-writes
    while (!write_ready.empty()) {
        int idx = write_ready.front();
        gups_update_t *update = &gups_updates[idx];

        if (update->status != WRITE_READY) {
            pr_dbg("write issue error: %d\n", idx);
        }

        if (!memsys->WillAcceptTransaction(update->address, true))
            break;

        pr_dbg("write issued: %010" PRIx64 "\n", update->address);
        memsys->AddTransaction(update->address, true);
        write_ready.pop_front();
        update->status = WRITE_ISSUED;
        count++;
    }
    // do all ready-reads
    while (!read_ready.empty()) {
        int idx = read_ready.front();
        gups_update_t *update = &gups_updates[idx];

        if (update->status != READ_READY) {
            pr_dbg("read issue error: %d\n", idx);
        }

        if (!memsys->WillAcceptTransaction(update->address, false))
            break;

        pr_dbg("read issued: %010" PRIx64 "\n", update->address);
        pr_dbg("read issued: status = %s\n",
               gups_update_status_to_string(update->status));
        memsys->AddTransaction(update->address, false);
        update->status = READ_ISSUED;
        read_ready.pop_front();
        count++;
    }

    return count;
}

/////////////////////////////////
// DRAMSim3 callback functions //
/////////////////////////////////
static void gups_read_done(uint64_t addr)
{
    pr_dbg("read complete: %010" PRIx64 "\n", addr);
    int idx = gups_addr_to_update[addr];
    gups_updates[idx].status = WRITE_READY;
    write_ready.push_back(idx);
}

static void gups_write_done(uint64_t addr)
{
    pr_dbg("write complete: %010" PRIx64 "\n", addr);
    int idx = gups_addr_to_update[addr];
    gups_updates[idx].status = WRITE_DONE;
}

int main()
{
    memsys = new MemorySystem(config_path, ".", gups_read_done, gups_write_done);

    gups_setup_updates();

    while (!gups_done()) {
        gups_try_send_requests();
        memsys->ClockTick();
    }

    pr_dbg("done! printing stats\n");
    memsys->PrintStats();
    delete memsys;
    return 0;
}
