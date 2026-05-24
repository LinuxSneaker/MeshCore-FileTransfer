#ifndef FOTA_SENDER_H
#define FOTA_SENDER_H

#include <Arduino.h>
#include <InternalFileSystem.h> 

// Pull definitions directly from your global protocol file instead of creating copies
#include "FotaProtocol.h" 

class FotaSender {
public:
    FotaSender();
    
    void startTransfer(uint64_t targetNodeId, const char* filename, uint32_t size, uint32_t crc32, bool verbose = false);
    void stopTransfer();
    void handleIncomingPacket(const uint8_t* data, uint16_t len);
    void serviceLoop();

private:
    void sendChunk(uint16_t index);

    uint64_t _target_node_id;
    char _staged_filename[32]; 
    uint32_t _file_size;
    uint32_t _crc32;
    uint16_t _total_chunks;
    uint16_t _current_chunk;
    bool _is_active;
    uint32_t _last_send_time;
    bool _verbose_mode;
    unsigned long _start_time_ms;
};

#endif // FOTA_SENDER_H
