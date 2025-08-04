#ifndef TROPIC_SIMPLE_H
#define TROPIC_SIMPLE_H

#include <stdint.h>
#include <string.h>
#include <stdio.h>

// Simple command structure - just send the command string
struct tropic_message {
    char command[128];  // Command string (e.g., "random 10", "ecc-gen 1")
    uint8_t data[256];  // Optional data payload
    uint16_t data_len;  // Length of data
};

// Simple response structure
struct tropic_response {
    char status[64];    // "OK" or "ERROR: message"
    uint8_t data[256];  // Response data
    uint16_t data_len;  // Length of response data
};

// Helper functions
void parse_command(const char* input, struct tropic_message* msg);
void format_response(struct tropic_response* resp, const char* status, const uint8_t* data, uint16_t len);
void print_hex(const uint8_t* data, size_t len);

#endif 