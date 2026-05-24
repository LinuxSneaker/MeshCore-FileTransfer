#pragma once
#include <stdint.h>

#define MESH_PORT_FOTA 0x46 // 'F' for FOTA

enum FotaOpcode : uint8_t {
    FOTA_CMD_START  = 0x01,
    FOTA_CMD_STOP   = 0x02,
    FOTA_DATA_CHUNK = 0x03,
    FOTA_REQ_NACK   = 0x04,
    FOTA_STATUS     = 0x05
};

#pragma pack(push, 1)

struct FotaStartCmd {
    uint8_t opcode;       // FOTA_CMD_START
    uint32_t total_size;  // In bytes
    uint16_t chunk_size;  // Size of each data packet payload
    uint16_t total_chunks;
    uint32_t file_crc32;
};

struct FotaStopCmd {
    uint8_t opcode;       // FOTA_CMD_STOP
};

struct FotaDataChunk {
    uint8_t opcode;       // FOTA_DATA_CHUNK
    uint16_t chunk_index;
    uint8_t payload[200]; // Tailored for typical LoRa MTUs safely under 256 bytes
};

struct FotaNackReq {
    uint8_t opcode;       // FOTA_REQ_NACK
    uint16_t missing_chunk_index;
};

struct FotaStatusResp {
    uint8_t opcode;       // FOTA_STATUS
    uint8_t status_code;  // 0: Idle, 1: Transferring, 2: Complete, 3: CRC Error
    uint16_t next_expected_chunk;
};

#pragma pack(pop)