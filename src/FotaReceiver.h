// src/FotaReceiver.h
#pragma once
#include "FotaProtocol.h"
#include <vector>

class FotaReceiver {
public:
    FotaReceiver();
    void handleIncomingPacket(const uint8_t* data, uint16_t len);
    void updateTimeout();

private:
    void initTransfer(const FotaStartCmd& cmd);
    void processChunk(const FotaDataChunk& chunk, uint16_t data_len);
    void checkCompleteness();
    void sendNack(uint16_t chunk_index);
    void sendStatus(uint8_t status);
    uint32_t calculateCRC32(const uint8_t* data, uint32_t length);

    enum State { IDLE, RECEIVING, VERIFYING, COMPLETE };
    State _state;
    
    uint32_t _total_size;
    uint16_t _chunk_size;
    uint16_t _total_chunks;
    uint32_t _expected_crc;
    
    std::vector<uint8_t> _bitmap; // Tracks received chunks bit-by-bit
    uint32_t _last_packet_time;
};
