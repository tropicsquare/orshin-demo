#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include "bbstate.h"
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include "tropic_simple.h"

#include <sys/wait.h>  // For WEXITSTATUS
#include <time.h>      // For time()
#include <unistd.h>    // For getpid(), unlink()

// Target server configuration - must match peripheral device
#define L2CAP_SERVER_BLUETOOTH_ADDR "88:A2:9E:0B:0A:B7" // Replace with your peripheral's Bluetooth address
#define L2CAP_SERVER_PORT_NUM 0x0235

// Pre-shared cryptographic keys for secure handshake
// These keys are used for the initial key exchange protocol with the peripheral device
static uint8_t private_key[32] = {
    0x00, 0xee, 0x2a, 0xfc, 0x36, 0xe9, 0x58, 0xdf, 0x08, 0x94, 0x3b,
    0xc9, 0x96, 0x88, 0x3a, 0x44, 0x2c, 0x0b, 0xc5, 0x40, 0x41, 0x0d,
    0x40, 0x0a, 0x1e, 0x38, 0x27, 0x22, 0x1e, 0x0f, 0x26, 0x4e};

static uint8_t public_key[32] = {
    0x76, 0x8a, 0x58, 0x2e, 0x1f, 0x44, 0x21, 0xc9, 0xb7, 0x55, 0xf3,
    0x70, 0xdf, 0xe9, 0x44, 0x98, 0x5f, 0x31, 0xe9, 0x54, 0x77, 0x7e,
    0xb9, 0xba, 0xd6, 0x3d, 0xa0, 0xec, 0xf7, 0x4f, 0x6f, 0x61};

// Remote device's public key - must match peripheral device's public key
static uint8_t remote_public_key[32] = {
    0x49, 0x5a, 0xfa, 0xd7, 0xaf, 0x39, 0xae, 0x9f, 0x18, 0xdb, 0x1a,
    0x69, 0xd6, 0x88, 0xe3, 0xd2, 0x4a, 0xa1, 0xec, 0xa5, 0x49, 0xac,
    0x95, 0xc0, 0x46, 0x80, 0x3d, 0x22, 0x03, 0x59, 0x65, 0x73};

// Global state for the secure communication session
static bbstate state;

/**
 * Print buffer contents in hexadecimal format
 * Used for debugging and displaying cryptographic data
 */
void print_buf(void* buf, size_t buf_len)
{
    uint8_t* bufr = (uint8_t*)buf;
    for (int i = 0; i < buf_len; i++) {
        printf("%02x", bufr[i]);
    }
    printf("\n");
}

/**
 * Print data in hexadecimal format with proper formatting
 * Used for displaying binary data from TROPIC01 hardware operations
 */
void print_hex(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
        if ((i + 1) % 16 == 0) printf("\n");
        else if ((i + 1) % 4 == 0) printf(" ");
    }
    if (len % 16 != 0) printf("\n");
}

/**
 * Parse incoming command string into message structure
 * Extracts command from user input and prepares it for transmission
 */
void parse_command(const char* input, struct tropic_message* msg) {
    strncpy(msg->command, input, sizeof(msg->command) - 1);
    msg->command[sizeof(msg->command) - 1] = '\0';
    msg->data_len = 0;

    // Remove trailing newline
    char* newline = strchr(msg->command, '\n');
    if (newline) *newline = '\0';
}

/**
 * Format response structure with status and optional data
 * Prepares response to be sent back to the peripheral device
 */
void format_response(struct tropic_response* resp, const char* status, const uint8_t* data, uint16_t len) {
    strncpy(resp->status, status, sizeof(resp->status) - 1);
    resp->status[sizeof(resp->status) - 1] = '\0';

    if (data && len > 0 && len <= sizeof(resp->data)) {
        memcpy(resp->data, data, len);
        resp->data_len = len;
    } else {
        resp->data_len = 0;
    }
}

/**
 * Generate random bytes using TROPIC01 hardware
 * Interfaces with lt-util to generate cryptographically secure random numbers
 *
 * @param out - Buffer to store generated random bytes
 * @param count - Number of random bytes to generate
 * @return 0 on success, -1 on failure
 */
int tropic_random(uint8_t* out, uint8_t count) {
    char cmd[256];
    char temp_file[64];

    // Create unique temporary file to avoid conflicts
    snprintf(temp_file, sizeof(temp_file), "/tmp/random_%d_%ld", getpid(), time(NULL));
    snprintf(cmd, sizeof(cmd), "lt-util -r %d %s", count, temp_file);

    // Execute lt-util command to generate random bytes
    if (system(cmd) == 0) {
        FILE* f = fopen(temp_file, "rb");
        if (f) {
            size_t bytes_read = fread(out, 1, count, f);
            fclose(f);
            unlink(temp_file);
            return bytes_read == count ? 0 : -1;
        }
    }
    unlink(temp_file);
    return -1;
}

/**
 * Generate ECC key pair in specified slot using TROPIC01 hardware
 * Creates a new ECC key pair and stores it in the secure hardware slot
 *
 * @param slot - Hardware slot number for key storage
 * @return 0 on success, -1 on failure
 */
int tropic_ecc_generate(uint8_t slot) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "lt-util -e -g %d", slot);

    int result = system(cmd);
    return WEXITSTATUS(result) == 0 ? 0 : -1;
}

/**
 * Download public key from specified ECC slot using TROPIC01 hardware
 * Retrieves the public key portion of an ECC key pair from hardware
 *
 * @param slot - Hardware slot number containing the key
 * @param pubkey - Buffer to store the downloaded public key
 * @return Length of public key on success, -1 on failure
 */
int tropic_ecc_download(uint8_t slot, uint8_t* pubkey) {
    char cmd[256];
    char temp_file[64];

    // Create unique temporary file
    snprintf(temp_file, sizeof(temp_file), "/tmp/pubkey_%d_%ld", getpid(), time(NULL));
    snprintf(cmd, sizeof(cmd), "lt-util -e -d %d %s", slot, temp_file);

    // Execute lt-util command to download public key
    if (system(cmd) == 0) {
        FILE* f = fopen(temp_file, "rb");
        if (f) {
            int len = fread(pubkey, 1, 64, f);
            fclose(f);
            unlink(temp_file);
            return len > 0 ? len : -1;
        }
    }
    unlink(temp_file);
    return -1;
}

/**
 * Clear ECC key slot using TROPIC01 hardware
 * Securely erases the key pair stored in the specified slot
 *
 * @param slot - Hardware slot number to clear
 * @return 0 on success, -1 on failure
 */
int tropic_ecc_clear(uint8_t slot) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "lt-util -e -c %d", slot);

    int result = system(cmd);
    return WEXITSTATUS(result) == 0 ? 0 : -1;
}

/**
 * Sign data using ECC private key in specified slot
 * Uses the TROPIC01 hardware to create a digital signature
 *
 * @param slot - Hardware slot number containing the signing key
 * @param data - Data to be signed
 * @param data_len - Length of data to sign
 * @param signature - Buffer to store the generated signature
 * @return Length of signature on success, -1 on failure
 */
int tropic_ecc_sign(uint8_t slot, const uint8_t* data, size_t data_len, uint8_t* signature) {
    char cmd[256];
    char input_file[64];
    char output_file[64];

    // Create unique temporary files
    snprintf(input_file, sizeof(input_file), "/tmp/sign_input_%d_%ld", getpid(), time(NULL));
    snprintf(output_file, sizeof(output_file), "/tmp/sign_output_%d_%ld", getpid(), time(NULL));

    // Write data to input file
    FILE* f = fopen(input_file, "wb");
    if (!f) return -1;

    if (fwrite(data, 1, data_len, f) != data_len) {
        fclose(f);
        unlink(input_file);
        return -1;
    }
    fclose(f);

    // Execute lt-util command for signing
    snprintf(cmd, sizeof(cmd), "lt-util -e -s %d %s %s", slot, input_file, output_file);

    int result = -1;
    if (system(cmd) == 0) {
        FILE* f = fopen(output_file, "rb");
        if (f) {
            int len = fread(signature, 1, 64, f);
            fclose(f);
            result = len > 0 ? len : -1;
        }
    }

    // Cleanup temporary files
    unlink(input_file);
    unlink(output_file);
    return result;
}

/**
 * Store data in memory slot using TROPIC01 hardware
 * Writes data to the secure memory storage in the specified slot
 *
 * @param slot - Memory slot number for data storage
 * @param data - Data to be stored
 * @param data_len - Length of data to store
 * @return 0 on success, -1 on failure
 */
int tropic_mem_store(uint8_t slot, const uint8_t* data, size_t data_len) {
    char cmd[256];
    char temp_file[64];

    // Create unique temporary file
    snprintf(temp_file, sizeof(temp_file), "/tmp/mem_store_%d_%ld", getpid(), time(NULL));

    // Write data to temporary file
    FILE* f = fopen(temp_file, "wb");
    if (!f) return -1;

    if (fwrite(data, 1, data_len, f) != data_len) {
        fclose(f);
        unlink(temp_file);
        return -1;
    }
    fclose(f);

    // Execute lt-util command for memory storage
    snprintf(cmd, sizeof(cmd), "lt-util -m -s %d %s", slot, temp_file);

    int result = system(cmd);
    unlink(temp_file);
    return WEXITSTATUS(result) == 0 ? 0 : -1;
}

/**
 * Read data from memory slot using TROPIC01 hardware
 * Retrieves data from the secure memory storage in the specified slot
 *
 * @param slot - Memory slot number to read from
 * @param data - Buffer to store read data
 * @param max_len - Maximum length of data to read
 * @return Length of data read on success, -1 on failure
 */
int tropic_mem_read(uint8_t slot, uint8_t* data, size_t max_len) {
    char cmd[256];
    char temp_file[64];

    // Create unique temporary file
    snprintf(temp_file, sizeof(temp_file), "/tmp/mem_read_%d_%ld", getpid(), time(NULL));
    snprintf(cmd, sizeof(cmd), "lt-util -m -r %d %s", slot, temp_file);

    // Execute lt-util command for memory reading
    if (system(cmd) == 0) {
        FILE* f = fopen(temp_file, "rb");
        if (f) {
            int len = fread(data, 1, max_len, f);
            fclose(f);
            unlink(temp_file);
            return len > 0 ? len : -1;
        }
    }
    unlink(temp_file);
    return -1;
}

/**
 * Erase memory slot using TROPIC01 hardware
 * Securely erases data stored in the specified memory slot
 *
 * @param slot - Memory slot number to erase
 * @return 0 on success, -1 on failure
 */
int tropic_mem_erase(uint8_t slot) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "lt-util -m -e %d", slot);

    int result = system(cmd);
    return WEXITSTATUS(result) == 0 ? 0 : -1;
}

/**
 * Process incoming encrypted commands from peripheral device
 * This is the main command processing loop that:
 * 1. Receives encrypted commands via L2CAP
 * 2. Decrypts commands using the established session key
 * 3. Executes TROPIC01 hardware operations
 * 4. Encrypts and sends responses back to peripheral
 *
 * @param socket - L2CAP socket connected to peripheral device
 * @param state - Secure session state containing keys and counters
 */
void process_commands(int socket, bbstate* state) {
    uint8_t encrypted_msg[2048];
    uint8_t decrypted_msg[2048];
    struct tropic_message msg;
    struct tropic_response resp;

    printf("Central: Waiting for commands...\n");

    // Main command processing loop
    while (1) {
        // Receive encrypted command from peripheral device
        ssize_t recv_len = recv(socket, encrypted_msg, sizeof(encrypted_msg), 0);
        if (recv_len <= 0) {
            printf("Connection closed or error\n");
            break;
        }

        // Decrypt command using session key and counter
        // Counter is incremented to prevent replay attacks
        size_t dec_len = aead_decrypt(decrypted_msg, state->key, state->counter++,
                                     NULL, 0, encrypted_msg, recv_len);

        if (dec_len < sizeof(msg)) {
            printf("Invalid message size\n");
            continue;
        }

        // Extract command structure from decrypted data
        memcpy(&msg, decrypted_msg, sizeof(msg));
        printf("Received command: %s\n", msg.command);

        // Process specific TROPIC01 hardware commands
        if (strncmp(msg.command, "random ", 7) == 0) {
            // Generate random bytes command
            int count = atoi(msg.command + 7);
            uint8_t random_data[256];

            if (count > 0 && count <= 255) {
                tropic_random(random_data, count);
                format_response(&resp, "OK", random_data, count);
            } else {
                format_response(&resp, "ERROR: Invalid count", NULL, 0);
            }

        } else if (strncmp(msg.command, "ecc-gen ", 8) == 0) {
            // Generate ECC key pair command
            int slot = atoi(msg.command + 8);

            if (tropic_ecc_generate(slot) == 0) {
                format_response(&resp, "OK - ECC key generated", NULL, 0);
            } else {
                format_response(&resp, "ERROR: Key generation failed", NULL, 0);
            }

        } else if (strncmp(msg.command, "ecc-download ", 13) == 0) {
            // Download public key command
            int slot = atoi(msg.command + 13);
            uint8_t pubkey[64];

            int len = tropic_ecc_download(slot, pubkey);
            if (len > 0) {
                format_response(&resp, "OK - Public key", pubkey, len);
            } else {
                format_response(&resp, "ERROR: Key download failed", NULL, 0);
            }

        } else if (strncmp(msg.command, "ecc-clear ", 10) == 0) {
            // Clear ECC key slot command
            int slot = atoi(msg.command + 10);

            if (tropic_ecc_clear(slot) == 0) {
                format_response(&resp, "OK - ECC slot cleared", NULL, 0);
            } else {
                format_response(&resp, "ERROR: Clear failed", NULL, 0);
            }

        } else if (strncmp(msg.command, "ecc-sign ", 9) == 0) {
            // Sign data command
            char* space = strchr(msg.command + 9, ' ');
            if (space) {
                int slot = atoi(msg.command + 9);
                char* data_str = space + 1;
                uint8_t signature[64];

                int len = tropic_ecc_sign(slot, (uint8_t*)data_str, strlen(data_str), signature);
                if (len > 0) {
                    format_response(&resp, "OK - Signature", signature, len);
                } else {
                    format_response(&resp, "ERROR: Signing failed", NULL, 0);
                }
            } else {
                format_response(&resp, "ERROR: Invalid sign command", NULL, 0);
            }

        } else if (strncmp(msg.command, "mem-store ", 10) == 0) {
            // Store data in memory command
            char* space = strchr(msg.command + 10, ' ');
            if (space) {
                int slot = atoi(msg.command + 10);
                char* data_str = space + 1;
                
                if (tropic_mem_store(slot, (uint8_t*)data_str, strlen(data_str)) == 0) {
                    format_response(&resp, "OK - Data stored", NULL, 0);
                } else {
                    format_response(&resp, "ERROR: Store failed", NULL, 0);
                }
            } else {
                format_response(&resp, "ERROR: Invalid store command", NULL, 0);
            }
            
        } else if (strncmp(msg.command, "mem-read ", 9) == 0) {
            // Read data from memory command
            int slot = atoi(msg.command + 9);
            uint8_t data[444];
            
            int len = tropic_mem_read(slot, data, sizeof(data));
            if (len > 0) {
                format_response(&resp, "OK - Memory data", data, len);
            } else {
                format_response(&resp, "ERROR: Read failed", NULL, 0);
            }
            
        } else if (strncmp(msg.command, "mem-erase ", 10) == 0) {
            // Erase memory slot command
            int slot = atoi(msg.command + 10);
            
            if (tropic_mem_erase(slot) == 0) {
                format_response(&resp, "OK - Memory slot erased", NULL, 0);
            } else {
                format_response(&resp, "ERROR: Erase failed", NULL, 0);
            }
            
        } else {
            // Unknown command
            format_response(&resp, "ERROR: Unknown command", NULL, 0);
        }
        
        // Encrypt response using session key and incremented counter
        uint8_t encrypted_resp[2048];
        size_t enc_len = aead_encrypt(encrypted_resp, state->key, state->counter++,
                                     NULL, 0, (uint8_t*)&resp, sizeof(resp));
        send(socket, encrypted_resp, enc_len, 0);
    }
}

/**
 * Main function - implements Bluetooth L2CAP client for TROPIC01 hardware interface
 * This device acts as a central that:
 * 1. Connects to peripheral device via L2CAP
 * 2. Performs secure handshake to establish session key
 * 3. Processes encrypted commands and executes TROPIC01 hardware operations
 * 4. Sends encrypted responses back to peripheral
 */
int
main(int argc, char** argv)
{
    uint8_t buffer[128];
    struct sockaddr_l2 addr = {0};
    int sock;
    const char* sample_text = "L2CAP Simple";
    bdaddr_t local_bdaddr = {0}; // Local Bluetooth address storage
    int dev_id = 0;              // Use hci0 device (first Bluetooth adapter)

    printf("Start Bluetooth L2CAP client, server addr %s\n",
           L2CAP_SERVER_BLUETOOTH_ADDR);

    // Get local Bluetooth address from hci0 adapter
    int hci_sock = hci_open_dev(dev_id);
    if (hci_sock < 0) {
        perror("failed to open HCI device");
        exit(1);
    }

    if (hci_read_bd_addr(hci_sock, &local_bdaddr, 0) < 0) {
        perror("failed to read local Bluetooth address");
        close(hci_sock);
        exit(1);
    }
    close(hci_sock);

    char local_addr_str[18];
    ba2str(&local_bdaddr, local_addr_str);
    printf("Using local HCI device: hci%d (%s)\n", dev_id, local_addr_str);

    // Create L2CAP socket for client connection
    sock = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
    if (sock < 0) {
        perror("failed to create socket");
        exit(1);
    }

    // Bind socket to local address (hci0)
    struct sockaddr_l2 local_addr = {0};
    local_addr.l2_family = AF_BLUETOOTH;
    bacpy(&local_addr.l2_bdaddr, &local_bdaddr); // Set to hci0's address

    if (bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        perror("failed to bind local socket");
        close(sock);
        exit(1);
    }

    // Configure connection parameters for target server
    addr.l2_family = AF_BLUETOOTH;
    addr.l2_psm = htobs(L2CAP_SERVER_PORT_NUM); // Server's port number
    str2ba(L2CAP_SERVER_BLUETOOTH_ADDR, &addr.l2_bdaddr); // Server's Bluetooth address

    // Connect to peripheral device
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("failed to connect");
        close(sock);
        exit(1);
    }

    printf("connected...\n");

    // Initialize secure session state as central device
    // This prepares the cryptographic state for the handshake
    bbstate_init(&state, BB_ROLE_CENTRAL, public_key, private_key,
                 remote_public_key, NULL);

    // Initiate secure handshake with peripheral device
    bb_session_start_req(&state, buffer);

    // Send handshake request
    if (send(sock, buffer, sizeof(struct bb_session_start_req), 0) <= 0) {
        perror("failed to send data");
        close(sock);
        exit(1);
    }

    // Receive handshake response
    if (recv(sock, buffer, sizeof(struct bb_session_start_req), 0) <= 0) {
        perror("failed to receive data");
        close(sock);
        exit(1);
    }

    // Process handshake response and derive session key
    bb_session_start_rx(&state, buffer);
    printf("Handshake complete, key:\n");
    print_buf(state.key, sizeof(state.key));

    // Start processing commands from peripheral device
    // This is the main loop that handles encrypted TROPIC01 commands
    process_commands(sock, &state);

    close(sock);
    return 0;
}