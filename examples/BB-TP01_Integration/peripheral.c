#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include "bbstate.h"
#include <bluetooth/hci.h>     // For HCI functions
#include <bluetooth/hci_lib.h> // For HCI functions
#include "tropic_simple.h"

/* L2CAP server channel - must match client configuration */
#define L2CAP_SERVER_PORT_NUM 0x0235

// Pre-shared cryptographic keys for secure handshake
// These keys are used for the initial key exchange protocol
static uint8_t private_key[32] = {
    0x60, 0x58, 0xce, 0x93, 0x0d, 0xf3, 0xdd, 0x2e, 0xa0, 0x0e, 0x1a,
    0x62, 0x6b, 0xc7, 0x0b, 0xa4, 0x17, 0x74, 0xea, 0x11, 0xe9, 0xa2,
    0xef, 0x28, 0x7e, 0xd2, 0x5a, 0x59, 0xca, 0x04, 0x03, 0x6b};

static uint8_t public_key[32] = {
    0x49, 0x5a, 0xfa, 0xd7, 0xaf, 0x39, 0xae, 0x9f, 0x18, 0xdb, 0x1a,
    0x69, 0xd6, 0x88, 0xe3, 0xd2, 0x4a, 0xa1, 0xec, 0xa5, 0x49, 0xac,
    0x95, 0xc0, 0x46, 0x80, 0x3d, 0x22, 0x03, 0x59, 0x65, 0x73};

// Remote device's public key - must match central device's public key
static uint8_t remote_public_key[32] = {
    0x76, 0x8a, 0x58, 0x2e, 0x1f, 0x44, 0x21, 0xc9, 0xb7, 0x55, 0xf3,
    0x70, 0xdf, 0xe9, 0x44, 0x98, 0x5f, 0x31, 0xe9, 0x54, 0x77, 0x7e,
    0xb9, 0xba, 0xd6, 0x3d, 0xa0, 0xec, 0xf7, 0x4f, 0x6f, 0x61};

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
 * Console interface for user interaction with TROPIC01 hardware
 * Provides a command-line interface to send encrypted commands to the central device
 * The central device will execute these commands on the actual TROPIC01 hardware
 *
 * @param socket - Connected L2CAP socket to central device
 * @param state - Secure session state containing encryption keys and counters
 */
void console_interface(int socket, bbstate* state) {
    char input[256];
    struct tropic_message msg;
    struct tropic_response resp;
    uint8_t encrypted_msg[2048];
    uint8_t decrypted_resp[2048];

    // Display available TROPIC01 hardware commands
    printf("\nTROPIC01 Console Interface\n");
    printf("Available commands:\n");
    printf("  random <count>         - Generate random bytes\n");
    printf("  ecc-gen <slot>         - Generate ECC key\n");
    printf("  ecc-download <slot>    - Download public key\n");
    printf("  ecc-clear <slot>       - Clear ECC slot\n");
    printf("  ecc-sign <slot> <data> - Sign data\n");
    printf("  mem-store <slot> <data> - Store data\n");
    printf("  mem-read <slot>        - Read data\n");
    printf("  mem-erase <slot>       - Erase slot\n");
    printf("  exit                   - Exit\n");
    printf("\nTROPIC01> ");
    fflush(stdout);

    // Main command processing loop
    while (fgets(input, sizeof(input), stdin)) {
        // Handle exit command
        if (strncmp(input, "exit", 4) == 0) {
            printf("Exiting...\n");
            break;
        }

        // Parse user input into command structure
        parse_command(input, &msg);

        // Skip empty commands
        if (strlen(msg.command) == 0) {
            printf("TROPIC01> ");
            fflush(stdout);
            continue;
        }

        printf("Sending command: %s\n", msg.command);

        // Encrypt command using AEAD with session key and counter
        // Counter is incremented to prevent replay attacks
        size_t enc_len = aead_encrypt(encrypted_msg, state->key, state->counter++,
                                     NULL, 0, (uint8_t*)&msg, sizeof(msg));
        if (send(socket, encrypted_msg, enc_len, 0) <= 0) {
            perror("Failed to send command");
            break;
        }

        // Receive encrypted response from central device
        ssize_t recv_len = recv(socket, encrypted_msg, sizeof(encrypted_msg), 0);
        if (recv_len <= 0) {
            perror("Failed to receive response");
            break;
        }

        // Decrypt response using session key and incremented counter
        size_t dec_len = aead_decrypt(decrypted_resp, state->key, state->counter++,
                                     NULL, 0, encrypted_msg, recv_len);

        if (dec_len < sizeof(resp)) {
            printf("Invalid response size\n");
            printf("TROPIC01> ");
            fflush(stdout);
            continue;
        }

        // Extract response structure from decrypted data
        memcpy(&resp, decrypted_resp, sizeof(resp));

        // Display command result to user
        printf("Response: %s", resp.status);
        if (resp.data_len > 0) {
            printf(" - Data (%d bytes): ", resp.data_len);
            print_hex(resp.data, resp.data_len);
        } else {
            printf("\n");
        }

        printf("TROPIC01> ");
        fflush(stdout);
    }
}

/**
 * Main function - implements Bluetooth L2CAP server for TROPIC01 hardware interface
 * This device acts as a peripheral that:
 * 1. Sets up L2CAP server on hci0 Bluetooth adapter
 * 2. Performs secure handshake with central device
 * 3. Provides console interface for sending commands to TROPIC01 hardware
 */
int
main(int argc, char** argv)
{
    struct sockaddr_l2 loc_addr = {0}, rem_addr = {0};
    uint8_t buffer[128] = {0};
    int server_socket, client_socket, bytes_read;
    unsigned int opt = sizeof(rem_addr);

    bdaddr_t local_bdaddr = {0}; // Local Bluetooth address storage
    int dev_id = 0;              // Use hci0 device (first Bluetooth adapter)

    printf("Start Bluetooth L2CAP server...\n");

    // Get the local Bluetooth address of hci0 adapter
    int hci_sock = hci_open_dev(dev_id);
    if (hci_sock < 0) {
        perror("failed to open HCI device hci0");
        exit(1);
    }

    if (hci_read_bd_addr(hci_sock, &local_bdaddr, 0) < 0) {
        perror("failed to read local Bluetooth address for hci0 device");
        close(hci_sock);
        exit(1);
    }

    char local_addr_str[18];
    ba2str(&local_bdaddr, local_addr_str);
    printf("Using local HCI device: hci%d (%s)\n", dev_id, local_addr_str);

    // Enable device discoverability by setting scan mode
    // This allows the central device to find and connect to this peripheral
    uint8_t param = (SCAN_PAGE | SCAN_INQUIRY); // Enable both page and inquiry scan

    // Send HCI command to enable scanning
    if (hci_send_cmd(hci_sock, OGF_HOST_CTL, OCF_WRITE_SCAN_ENABLE, 1, &param) < 0) {
        perror("Failed to send HCI_Write_Scan_Enable command");
        return -1;
    }
    close(hci_sock);

    // Create L2CAP socket for server
    server_socket = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
    if (server_socket < 0) {
        perror("failed to create socket");
        exit(1);
    }

    // Bind socket to local Bluetooth adapter (hci0) and specified port
    loc_addr.l2_family = AF_BLUETOOTH;
    bacpy(&loc_addr.l2_bdaddr, &local_bdaddr); // Use hci0 address
    loc_addr.l2_psm = htobs(L2CAP_SERVER_PORT_NUM); // Set port number

    printf("binding\n");
    if (bind(server_socket, (struct sockaddr*)&loc_addr, sizeof(loc_addr)) < 0) {
        perror("failed to bind");
        close(server_socket);
        exit(1);
    }

    printf("listening\n");
    // Put socket into listening mode - wait for incoming connections
    listen(server_socket, 2);

    // Accept connection from central device
    client_socket = accept(server_socket, (struct sockaddr*)&rem_addr, &opt);
    if (client_socket < 0) {
        perror("accept");
        close(server_socket);
        return client_socket;
    }

    // Display connected client information
    char buf[18];
    ba2str(&rem_addr.l2_bdaddr, buf);
    printf("connected from %s\n", buf);

    // Initialize secure session state as peripheral device
    // This prepares the cryptographic state for the handshake
    bbstate_init(&state, BB_ROLE_PERIPHERAL, public_key, private_key,
                 remote_public_key, NULL);

    // Receive handshake initiation from central device
    bytes_read = recv(client_socket, buffer, sizeof(struct bb_session_start_req), 0);
    if (bytes_read <= 0) {
        perror("Error handshake initiation\n");
        close(client_socket);
        close(server_socket);
        exit(1);
    }

    // Process the received handshake request
    // This validates the central device and computes shared session key
    bb_session_start_rx(&state, buffer);

    // Prepare handshake response
    bb_session_start_rsp(&state, buffer);

    // Send handshake response to complete key exchange
    int rc = send(client_socket, buffer, sizeof(struct bb_session_start_req), 0);
    if (rc <= 0) {
        perror("Error sending");
        close(client_socket);
        close(server_socket);
        exit(1);
    }

    // Handshake complete - display derived session key
    printf("Handshake complete, key:\n");
    print_buf(state.key, 32);

    // Start interactive console for sending commands to TROPIC01 hardware
    // User can now enter commands that will be securely transmitted to
    // the central device for execution on the actual hardware
    console_interface(client_socket, &state);

    // Cleanup connections
    close(client_socket);
    close(server_socket);
    return 0;
}