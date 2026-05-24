// Inside src/FotaSender.cpp
#include "FotaSender.h"
#include <Arduino.h>
#include <cstring>

FotaSender::FotaSender() : 
    _file_size(0), 
    _crc32(0), 
    _total_chunks(0), 
    _current_chunk(0), 
    _is_active(false), 
    _last_send_time(0), 
    _target_node_id(0), 
    _verbose_mode(false), 
    _start_time_ms(0) 
{
    _staged_filename[0] = 0;
}
// Inside src/FotaSender.cpp
void FotaSender::startTransfer(uint64_t targetNodeId, const char* filename, uint32_t size, uint32_t crc32, bool verbose) {
    _verbose_mode = verbose;

    // Force immediate hardware print to prove we entered the function
    Serial.println("\n[DEBUG] Inside FotaSender::startTransfer framework...");
    Serial.flush();

    // Safety check: Ensure the filename has a leading forward slash if InternalFS requires it
    if (filename[0] != '/') {
        snprintf(_staged_filename, sizeof(_staged_filename), "/%s", filename);
    } else {
        strncpy(_staged_filename, filename, sizeof(_staged_filename) - 1);
    }

    // Now check if the dynamically passed file actually exists on the disk layout
    if (InternalFS.exists(filename)) {
        Serial.printf("[DEBUG] InternalFS confirmed file existence: %s\n", filename);
    } else {
        Serial.printf("[DEBUG] InternalFS reports file MISSING: %s\n", filename);
    }
    Serial.flush();

    _target_node_id = targetNodeId;
    _file_size = size;
    _crc32 = crc32;
    _is_active = true;
    _current_chunk = 0;
    
    uint16_t max_payload_len = 200;
    _total_chunks = (size + max_payload_len - 1) / max_payload_len;
    _start_time_ms = millis();

    if (_verbose_mode) {
        Serial.println("\n==================================================");
        Serial.printf("[FOTA] START TIME: %lu ms\n", _start_time_ms);
        Serial.printf("[FOTA] Dynamically Linked File: %s (%u bytes)\n", _staged_filename, _file_size);
        Serial.println("==================================================");
    }

    FotaStartCmd cmd;
    cmd.opcode = FOTA_CMD_START;
    cmd.total_size = _file_size;
    cmd.chunk_size = max_payload_len;
    cmd.total_chunks = _total_chunks;
    cmd.file_crc32 = _crc32;
    
    // MeshCoreApp::instance()->sendPacketToNode(_target_node_id, MESH_PORT_FOTA, (uint8_t*)&cmd, sizeof(cmd));
}

void FotaSender::handleIncomingPacket(const uint8_t* data, uint16_t len) {
    if (!_is_active || len == 0) return;
    uint8_t opcode = data[0];

    // Listen for status indicators returning from Target Repeater
    if (opcode == FOTA_STATUS && _verbose_mode) {
        const FotaStatusResp* resp = reinterpret_cast<const FotaStatusResp*>(data);
        if (resp->status_code == 2) { // Complete/Success Code
            unsigned long end_time = millis();
            unsigned long total_duration = end_time - _start_time_ms;
            
            Serial.println("\n==================================================");
            Serial.printf("[FOTA SUCCESS] Target finished image verification!\n");
            Serial.printf("[FOTA] END TIME: %lu ms\n", end_time);
            Serial.printf("[FOTA] Total Transfer Duration: %lu ms (%.2f seconds)\n", total_duration, (float)total_duration / 1000.0f);
            Serial.println("==================================================");
            _is_active = false;
        }
    }

    if (opcode == FOTA_REQ_NACK && len >= sizeof(FotaNackReq)) {
        const FotaNackReq* nack = reinterpret_cast<const FotaNackReq*>(data);
        if (_verbose_mode) {
            Serial.printf("[FOTA NACK] Dropped packet alert! Resending chunk index: %u\n", nack->missing_chunk_index);
        }
        sendChunk(nack->missing_chunk_index);
    }
}

void FotaSender::serviceLoop() {
    if (!_is_active) return;

    if (millis() - _last_send_time > 250) { 
        if (_current_chunk < _total_chunks) {
            sendChunk(_current_chunk);
            
            if (_verbose_mode) {
                // Calculate percentage complete based on chunk progress
                float progress = ((float)(_current_chunk + 1) / (float)_total_chunks) * 100.0f;
                unsigned long elapsed = millis() - _start_time_ms;
                Serial.printf("[FOTA PROGRESS] Sent chunk %u/%u | Complete: %.1f%% | Elapsed: %.2f sec\r", 
                              _current_chunk + 1, _total_chunks, progress, (float)elapsed / 1000.0f);
            }
            
            _current_chunk++;
            _last_send_time = millis();
        }
    }
}

void FotaSender::sendChunk(uint16_t index) {
    if (index >= _total_chunks) return;
    FotaDataChunk chunkPacket;
    chunkPacket.opcode = FOTA_DATA_CHUNK;
    chunkPacket.chunk_index = index;

    uint32_t flash_base_address = 0x000B0000; 
    uint32_t offset = index * 200;
    const uint8_t* source_flash_address = (const uint8_t*)(flash_base_address + offset);
    std::memcpy(chunkPacket.payload, source_flash_address, 200);
    
    // MeshCoreApp::instance()->sendPacketToNode(_target_node_id, MESH_PORT_FOTA, (uint8_t*)&chunkPacket, 3 + 200);
}

void FotaSender::stopTransfer() { _is_active = false; }
