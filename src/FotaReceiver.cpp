// src/FotaReceiver.cpp
#include "FotaReceiver.h"
#include <Arduino.h> // Assuming framework dependencies used by MeshCore targets
#include <cstring>

FotaReceiver::FotaReceiver() : _state(IDLE), _total_size(0), _chunk_size(0), _total_chunks(0), _expected_crc(0), _last_packet_time(0) {}

void FotaReceiver::handleIncomingPacket(const uint8_t* data, uint16_t len) {
    if (len == 0) return;
    uint8_t opcode = data[0];
    _last_packet_time = millis();

    switch (opcode) {
        case FOTA_CMD_START:
            if (len >= sizeof(FotaStartCmd)) {
                initTransfer(*reinterpret_cast<const FotaStartCmd*>(data));
            }
            break;
        case FOTA_CMD_STOP:
            _state = IDLE;
            sendStatus(0);
            break;
        case FOTA_DATA_CHUNK:
            if (_state == RECEIVING && len > 3) {
                processChunk(*reinterpret_cast<const FotaDataChunk*>(data), len - 3);
            }
            break;
        default:
            break;
    }
}

void FotaReceiver::initTransfer(const FotaStartCmd& cmd) {
    _total_size = cmd.total_size;
    _chunk_size = cmd.chunk_size;
    _total_chunks = cmd.total_chunks;
    _expected_crc = cmd.file_crc32;
    
    // Allocate bitfield bitmap to verify chunk receipt state
    uint16_t bitmap_bytes = (_total_chunks + 7) / 8;
    _bitmap.assign(bitmap_bytes, 0);
    
    // In practice, clear out target Internal Flash blocks or external QSPI on nRF52 here
    _state = RECEIVING;
    sendStatus(1);
}

void FotaReceiver::processChunk(const FotaDataChunk& chunk, uint16_t data_len) {
    if (chunk.chunk_index >= _total_chunks) return;

    // Check if chunk is already stored
    uint16_t byte_idx = chunk.chunk_index / 8;
    uint8_t bit_mask = 1 << (chunk.chunk_index % 8);
    
    if (!(_bitmap[byte_idx] & bit_mask)) {
        // Write data to nRF52 flash at offset: (chunk.chunk_index * _chunk_size)
        // InternalFlash.write((chunk.chunk_index * _chunk_size), chunk.payload, data_len);
        
        _bitmap[byte_idx] |= bit_mask; // Mark chunk received
    }
    
    checkCompleteness();
}

void FotaReceiver::checkCompleteness() {
    bool complete = true;
    for (uint16_t i = 0; i < _total_chunks; i++) {
        uint16_t byte_idx = i / 8;
        uint8_t bit_mask = 1 << (i % 8);
        if (!(_bitmap[byte_idx] & bit_mask)) {
            complete = false;
            // Optionally request immediate chunk recovery if window slips too far
            break;
        }
    }

    if (complete) {
        _state = VERIFYING;
        // Read file back from hardware flash storage memory block to check integrity
        // uint32_t computed_crc = calculateCRC32(flash_start_addr, _total_size);
        uint32_t dummy_computed_crc = _expected_crc; // Replace with actual flash reading logic
        
        if (dummy_computed_crc == _expected_crc) {
            _state = COMPLETE;
            sendStatus(2);
            // Trigger actual bootloader jump / image switch execution flag
        } else {
            _state = RECEIVING;
            sendStatus(3); // Notify companion CRC integrity check failed
        }
    }
}

void FotaReceiver::updateTimeout() {
    // Audit chunk status maps if connection falls idle while transferring
    if (_state == RECEIVING && (millis() - _last_packet_time > 5000)) {
        for (uint16_t i = 0; i < _total_chunks; i++) {
            uint16_t byte_idx = i / 8;
            uint8_t bit_mask = 1 << (i % 8);
            if (!(_bitmap[byte_idx] & bit_mask)) {
                sendNack(i); // Solicit re-transmits for missing segment
                break;       // Throttles back requests to preserve LoRa duty cycles
            }
        }
        _last_packet_time = millis(); // Avoid flooding NACK requests
    }
}

void FotaReceiver::sendNack(uint16_t chunk_index) {
    FotaNackReq nack;
    nack.opcode = FOTA_REQ_NACK;
    nack.missing_chunk_index = chunk_index;
    // MeshCore::sendPacket(MESH_PORT_FOTA, (uint8_t*)&nack, sizeof(nack));
}

void FotaReceiver::sendStatus(uint8_t status) {
    FotaStatusResp resp;
    resp.opcode = FOTA_STATUS;
    resp.status_code = status;
    resp.next_expected_chunk = 0; // Populate index of first missing chunk if necessary
    // MeshCore::sendPacket(MESH_PORT_FOTA, (uint8_t*)&resp, sizeof(resp));
}
