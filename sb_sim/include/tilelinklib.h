#ifndef __TILELINKLIB_H__
#define __TILELINKLIB_H__

#include <unistd.h>
#include <stdint.h>

enum TLAOpcode {
  PutFullData = 0,
  PutPartialData = 1,
  ArithmeticData = 2,
  LogicalData = 3,
  Get = 4,
  Hint = 5,
};

enum TLDOpcode {
    AccessAck = 0, 
    AccessAckData = 1,
    HintAck = 2, 
};

enum TLArithmeticAtomics {
    MIN = 0,
    MAX = 1,
    MINU = 2,
    MAXU = 3,
    ADD = 4,
};

enum TLLogicalAtomics {
    XOR = 0,
    OR = 1,
    AND = 2,
    SWAP = 3,
};    

enum TLHints {
    PREFETCH_READ = 0,
    PREFETCH_WRITE = 1,
};


typedef struct TLMessageA {
    uint8_t opcode;
    uint8_t param; //TLArithmeticAtomics, TLLogicalAtomics, TLHints
    uint8_t size;
    uint8_t corrupt;
    uint32_t source;
    uint64_t address;
    uint32_t mask;
    uint8_t data[32];
} __attribute__((packed)) TLMessageA;

typedef struct TLMessageD {
    uint8_t opcode;
    uint8_t param;      //Always: 0
    uint8_t size;
    uint8_t corrupt;
    uint32_t source;
    uint32_t sink;
    uint32_t denied;
    uint8_t data[32];
    uint32_t reserved;  //Always: 0
} __attribute__((packed)) TLMessageD;

typedef struct TLBundleParams {
    uint8_t address_bit_width;
    uint8_t source_bit_width;
    uint8_t sink_bit_width;
    uint8_t size_bit_width;
    uint32_t data_bit_width;
} TLBundleParams;


#endif // __TILELINKLIB_H__