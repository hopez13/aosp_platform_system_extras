/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/in.h>
#include <linux/unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "android-base/unique_fd.h"
#include "netdutils/Slice.h"
#include "netdutils/StatusOr.h"

#define ptr_to_u64(x) ((uint64_t)(uintptr_t)x)
#define DEFAULT_LOG_LEVEL 1

/* instruction set for bpf program */

#define MEM_LD(SIZE) (BPF_LDX | BPF_SIZE(SIZE) | BPF_MEM)
#define MEM_SET_BY_REG(SIZE) (BPF_STX | BPF_SIZE(SIZE) | BPF_MEM)
#define MEM_SET_BY_VAL(SIZE) (BPF_ST | BPF_SIZE(SIZE) | BPF_MEM)
#define PROG_EXIT (BPF_JMP | BPF_EXIT)
#define REG_ALU64(OP) (BPF_ALU64 | BPF_OP(OP) | BPF_X)
#define REG_ALU32(OP) (BPF_ALU | BPF_OP(OP) | BPF_X)
#define REG_ALU_JMP(OP) (BPF_JMP | BPF_OP(OP) | BPF_X)
#define REG_ATOMIC_ADD(SIZE) (BPF_STX | BPF_SIZE(SIZE) | BPF_XADD)
#define REG_MOV64 (BPF_ALU64 | BPF_MOV | BPF_X)
#define REG_MOV32 (BPF_ALU | BPF_MOV | BPF_X)
#define SKB_LD(SIZE) (BPF_LD | BPF_SIZE(SIZE) | BPF_ABS)
#define VAL_ALU64(OP) (BPF_ALU64 | BPF_OP(OP) | BPF_K)
#define VAL_ALU32(OP) (BPF_ALU | BPF_OP(OP) | BPF_K)
#define VAL_ALU_JMP(OP) (BPF_JMP | BPF_OP(OP) | BPF_K)
#define VAL_MOV64 (BPF_ALU64 | BPF_MOV | BPF_K)
#define VAL_MOV32 (BPF_ALU | BPF_MOV | BPF_K)

/* Raw code statement block */

#define BPF_INS_BLK(CODE, DST, SRC, OFF, IMM) \
    ((struct bpf_insn){                       \
        .code = (CODE), .dst_reg = (DST), .src_reg = (SRC), .off = (OFF), .imm = (IMM)})

#ifndef BPF_PSEUDO_MAP_FD
#define BPF_PSEUDO_MAP_FD 1
#endif

#define LOAD_MAP_FD(DST, MAP_FD)                                                                 \
    BPF_INS_BLK(BPF_LD | BPF_DW | BPF_IMM, DST, BPF_PSEUDO_MAP_FD, 0, (__s32)((__u32)(MAP_FD))), \
        BPF_INS_BLK(0, 0, 0, 0, (__s32)(((__u64)(MAP_FD)) >> 32))

namespace android {
namespace bpf {

struct UidTag {
    uint32_t uid;
    uint32_t tag;
};

struct StatsKey {
    uint32_t uid;
    uint32_t tag;
    uint32_t counterSet;
    uint32_t ifaceIndex;
};

// TODO: verify if framework side still need the detail number about TCP and UDP
// traffic. If not, remove the related tx/rx bytes and packets field to save
// space and simplify the eBPF program.
struct StatsValue {
    uint64_t rxPackets;
    uint64_t rxBytes;
    uint64_t txPackets;
    uint64_t txBytes;
};

struct Stats {
    uint64_t rxBytes;
    uint64_t rxPackets;
    uint64_t txBytes;
    uint64_t txPackets;
    uint64_t tcpRxPackets;
    uint64_t tcpTxPackets;
};

#ifndef DEFAULT_OVERFLOWUID
#define DEFAULT_OVERFLOWUID 65534
#endif

#define BPF_PATH "/sys/fs/bpf"

constexpr const char* BPF_EGRESS_PROG_PATH = BPF_PATH "/egress_prog";
constexpr const char* BPF_INGRESS_PROG_PATH = BPF_PATH "/ingress_prog";

constexpr const char* CGROUP_ROOT_PATH = "/dev/cg2_bpf";

constexpr const char* COOKIE_UID_MAP_PATH = BPF_PATH "/traffic_cookie_uid_map";
constexpr const char* UID_COUNTERSET_MAP_PATH = BPF_PATH "/traffic_uid_counterSet_map";
constexpr const char* UID_STATS_MAP_PATH = BPF_PATH "/traffic_uid_stats_map";
constexpr const char* TAG_STATS_MAP_PATH = BPF_PATH "/traffic_tag_stats_map";

const StatsKey NONEXISTENT_STATSKEY = {
    .uid = DEFAULT_OVERFLOWUID,
};

int createMap(bpf_map_type map_type, uint32_t key_size, uint32_t value_size,
              uint32_t max_entries, uint32_t map_flags);
int writeToMapEntry(const base::unique_fd& map_fd, void* key, void* value, uint64_t flags);
int findMapEntry(const base::unique_fd& map_fd, void* key, void* value);
int deleteMapEntry(const base::unique_fd& map_fd, void* key);
int getNextMapKey(const base::unique_fd& map_fd, void* key, void* next_key);
int bpfProgLoad(bpf_prog_type prog_type, netdutils::Slice bpf_insns, const char* license,
                uint32_t kern_version, netdutils::Slice bpf_log);
int mapPin(const base::unique_fd& map_fd, const char* pathname);
int mapRetrieve(const char* pathname, uint32_t flags);
int attachProgram(bpf_attach_type type, uint32_t prog_fd, uint32_t cg_fd);
int detachProgram(bpf_attach_type type, uint32_t cg_fd);
uint64_t getSocketCookie(int sockFd);
netdutils::StatusOr<base::unique_fd> setUpBPFMap(uint32_t key_size, uint32_t value_size,
                                                 uint32_t map_size, const char* path,
                                                 bpf_map_type map_type);
bool hasBpfSupport();
}  // namespace bpf
}  // namespace android
