/**
 * @file tilelinklib.h
 * @brief TileLink wire-format types and opcode enumerations.
 *
 * Defines the packed structs and enums used to represent TileLink A/D channel
 * messages as they appear on Switchboard queues (dw=416, 52 bytes per packet).
 * Supports TL-UL and TL-UH (A and D channels only); TL-C is not supported.
 *
 * This header is C-compatible (no C++ dependencies). String formatting helpers
 * for these types are provided in tilelinklib.hpp.
 */

#ifndef __TILELINKLIB_H__
#define __TILELINKLIB_H__

#include <stdint.h>

/**
 * @brief TileLink A-channel opcodes.
 *
 * Used in TLMessageA::opcode. Covers TL-UL, TL-UH, and atomic operations.
 */
enum TLAOpcode {
  PutFullData    = 0, /**< Write all bytes indicated by mask. */
  PutPartialData = 1, /**< Write bytes selectively indicated by mask. */
  ArithmeticData = 2, /**< Atomic arithmetic read-modify-write (TL-UH). */
  LogicalData    = 3, /**< Atomic logical read-modify-write (TL-UH). */
  Get            = 4, /**< Read request. */
  Hint           = 5, /**< Prefetch hint. */
};

/**
 * @brief TileLink D-channel opcodes.
 *
 * Used in TLMessageD::opcode. Covers responses to A-channel requests.
 */
enum TLDOpcode {
  AccessAck     = 0, /**< Acknowledgement for a write (no data). */
  AccessAckData = 1, /**< Acknowledgement for a read (carries data). */
  HintAck       = 2, /**< Acknowledgement for a Hint. */
};

/**
 * @brief Arithmetic atomic operation subtypes (TL-UH).
 *
 * Encoded in TLMessageA::param when opcode == ArithmeticData.
 */
enum TLArithmeticAtomics {
  TL_MIN  = 0, /**< Signed minimum. */
  TL_MAX  = 1, /**< Signed maximum. */
  TL_MINU = 2, /**< Unsigned minimum. */
  TL_MAXU = 3, /**< Unsigned maximum. */
  TL_ADD  = 4, /**< Addition. */
};

/**
 * @brief Logical atomic operation subtypes (TL-UH).
 *
 * Encoded in TLMessageA::param when opcode == LogicalData.
 */
enum TLLogicalAtomics {
  TL_XOR  = 0, /**< XOR. */
  TL_OR   = 1, /**< OR. */
  TL_AND  = 2, /**< AND. */
  TL_SWAP = 3, /**< Swap (unconditional). */
};

/**
 * @brief Hint subtypes.
 *
 * Encoded in TLMessageA::param when opcode == Hint.
 */
enum TLHints {
  PREFETCH_READ  = 0, /**< Prefetch for read. */
  PREFETCH_WRITE = 1, /**< Prefetch for write. */
};

/**
 * @brief TileLink A-channel message (request).
 *
 * Packed wire format: 52 bytes (416 bits), matching Switchboard queue width
 * (dw=416). Each instance represents one beat on the A channel.
 *
 * @note Only the first `TLBundleParams::data_bit_width / 8` bytes of @p data
 *       are valid per transaction; upper bytes are unused when the bus is
 *       narrower than 256 bits.
 * @note `__attribute__((packed))` is GCC/Clang-specific.
 */
typedef struct TLMessageA {
  uint8_t  opcode;   /**< A-channel opcode; cast to TLAOpcode. */
  uint8_t  param;    /**< Subtype: TLArithmeticAtomics, TLLogicalAtomics, or TLHints. */
  uint8_t  size;     /**< lg2(transfer_size_in_bytes). */
  uint8_t  corrupt;  /**< 1 if data is intentionally corrupted (poison). */
  uint32_t source;   /**< Source ID (initiator tag). */
  uint64_t address;  /**< Target byte address. */
  uint32_t mask;     /**< Byte-enable mask; bit i enables byte i of data. */
  uint8_t  data[32]; /**< Payload — one beat; valid bytes = data_bit_width / 8. */
} __attribute__((packed)) TLMessageA;

static_assert(sizeof(TLMessageA) == 52,
              "TLMessageA layout changed — verify Switchboard queue dw=416");

/**
 * @brief TileLink D-channel message (response).
 *
 * Packed wire format: 52 bytes (416 bits), matching Switchboard queue width
 * (dw=416). Each instance represents one beat on the D channel.
 *
 * @note Only the first `TLBundleParams::data_bit_width / 8` bytes of @p data
 *       are valid per transaction; upper bytes are unused when the bus is
 *       narrower than 256 bits.
 * @note `__attribute__((packed))` is GCC/Clang-specific.
 */
typedef struct TLMessageD {
  uint8_t  opcode;      /**< D-channel opcode; cast to TLDOpcode. */
  uint8_t  param;       /**< Always 0. */
  uint8_t  size;        /**< lg2(transfer_size_in_bytes). */
  uint8_t  corrupt;     /**< 1 if data is intentionally corrupted (poison). */
  uint32_t source;      /**< Echoed source ID from the corresponding A-channel request. */
  uint32_t sink;        /**< Sink ID (responder tag). */
  uint8_t  denied;      /**< 1 if the transaction was denied by the target. */
  uint8_t  data[32];    /**< Payload — one beat; valid bytes = data_bit_width / 8. */
  uint8_t  reserved[7]; /**< Padding to 52 bytes; always 0. */
} __attribute__((packed)) TLMessageD;

static_assert(sizeof(TLMessageD) == 52,
              "TLMessageD layout changed — verify Switchboard queue dw=416");

/**
 * @brief TileLink bundle configuration parameters.
 *
 * Describes the widths of each TileLink signal field for a given link instance.
 * Used by TLAgent to validate and construct messages.
 */
typedef struct TLBundleParams {
  uint8_t  address_bit_width;  /**< Width of the address field in bits. */
  uint8_t  source_bit_width;   /**< Width of the source ID field in bits. */
  uint8_t  sink_bit_width;     /**< Width of the sink ID field in bits. */
  uint8_t  size_bit_width;     /**< lg2(beat_size_in_bytes); e.g. 5 for a 32-byte beat. */
  uint32_t data_bit_width;     /**< Bus width in bits (must be ≤ 256). */
  uint32_t max_transfer_bytes; /**< Maximum burst transfer size in bytes. */
} TLBundleParams;

#endif // __TILELINKLIB_H__
