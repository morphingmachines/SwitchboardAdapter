#ifndef __TILELINKLIB_H__
#define __TILELINKLIB_H__

#include <stdint.h>
#include <string>
#include <unistd.h>

enum TLAOpcode {
  PutFullData = 0,
  PutPartialData = 1,
  ArithmeticData = 2,
  LogicalData = 3,
  Get = 4,
  Hint = 5,
};

static inline std::string get_opcodeA_str(TLAOpcode opcode) {
  switch (opcode) {
  case PutFullData:    return "PutFullData";
  case PutPartialData: return "PutPartialData";
  case ArithmeticData: return "ArithmeticData";
  case LogicalData:    return "LogicalData";
  case Get:            return "Get";
  case Hint:           return "Hint";
  default:             return "Unknown";
  }
}

enum TLDOpcode {
  AccessAck = 0,
  AccessAckData = 1,
  HintAck = 2,
};

static inline std::string get_opcodeD_str(TLDOpcode opcode) {
  switch (opcode) {
  case AccessAck:     return "AccessAck";
  case AccessAckData: return "AccessAckData";
  case HintAck:       return "HintAck";
  default:            return "Unknown";
  }
}

enum TLArithmeticAtomics {
  TL_MIN  = 0,
  TL_MAX  = 1,
  TL_MINU = 2,
  TL_MAXU = 3,
  TL_ADD  = 4,
};

enum TLLogicalAtomics {
  TL_XOR  = 0,
  TL_OR   = 1,
  TL_AND  = 2,
  TL_SWAP = 3,
};

enum TLHints {
  PREFETCH_READ = 0,
  PREFETCH_WRITE = 1,
};

typedef struct TLMessageA {
  uint8_t opcode;
  uint8_t param; // TLArithmeticAtomics, TLLogicalAtomics, TLHints
  uint8_t size;
  uint8_t corrupt;
  uint32_t source;
  uint64_t address;
  uint32_t mask;
  uint8_t data[32];
} __attribute__((packed)) TLMessageA;

typedef struct TLMessageD {
  uint8_t opcode;
  uint8_t param; // Always: 0
  uint8_t size;
  uint8_t corrupt;
  uint32_t source;
  uint32_t sink;
  uint32_t denied;
  uint8_t data[32];
  uint32_t reserved; // Always: 0
} __attribute__((packed)) TLMessageD;

typedef struct TLBundleParams {
  uint8_t address_bit_width;
  uint8_t source_bit_width;
  uint8_t sink_bit_width;
  uint8_t size_bit_width;
  uint32_t data_bit_width;
  uint32_t max_transfer_bytes;
} TLBundleParams;

#endif // __TILELINKLIB_H__