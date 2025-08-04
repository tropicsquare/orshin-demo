# TROPIC01 Protocol via BlueBrothers protocol

This document provides a detailed implementation of TROPIC01 via Bluetooth communication, built on the **bb-protocol (BlueBrothers)** framework. The implementation consists of two main components: `central.c` (TROPIC01 interface) and `peripheral.c` (user console interface).

## 🏗️ System Architecture

```
[User Console] → [Peripheral (Server, hci0)] ←encrypted→ [Central (Client, hci0)] → [TROPIC01 Secure Element]
```

### Component Roles

- **Peripheral (`peripheral.c`)**: 
  - Acts as a Bluetooth L2CAP server on hci0 (first Bluetooth adapter)
  - Enables scan mode for discoverability (so the central can find it)
  - Provides interactive console interface for user commands
  - Handles encrypted communication with the central device
  - Role: BB_ROLE_PERIPHERAL

- **Central (`central.c`)**:
  - Acts as a Bluetooth L2CAP client on hci0 (first Bluetooth adapter)
  - Connects to the peripheral's Bluetooth address and port
  - Performs all TROPIC01 secure element operations (random, ECC, memory, etc.)
  - Sends encrypted responses back to the peripheral
  - Role: BB_ROLE_CENTRAL

- **TROPIC01 Secure Element**:
  - Hardware security module providing cryptographic operations
  - Accessed only by the central device

**Note:**
- The public/private key pairs and Bluetooth addresses must match between the two devices for the handshake to succeed.
- All TROPIC01 operations are performed on the central, triggered by commands from the peripheral.

## 📨 Message Protocol

### Command Structure
```c
struct tropic_message {
    char command[128];    // Command string (e.g., "random 32", "ecc-gen 0")
    uint8_t data[256];    // Optional data payload
    uint16_t data_len;    // Length of data payload
};
```

### Response Structure
```c
struct tropic_response {
    char status[64];      // Status message ("OK" or "ERROR: message")
    uint8_t data[256];    // Response data (keys, signatures, etc.)
    uint16_t data_len;    // Length of response data
};
```

## 🔧 Implementation Details

### Peripheral Implementation (`peripheral.c`)

**Key Components:**

1. **Bluetooth Server Setup**
   ```c
   // Uses hci0 device for server role
   int dev_id = 0;              // hci0 device ID
   bdaddr_t local_bdaddr = {0}; // Store hci0 address
   
   // Open HCI device and get local address
   int hci_sock = hci_open_dev(dev_id);
   hci_read_bd_addr(hci_sock, &local_bdaddr, 0);
   ```

2. **Enable Scan Mode for Discoverability**
   ```c
   // Enable device discoverability so the central can find and connect
   uint8_t param = (SCAN_PAGE | SCAN_INQUIRY); // Enable both page and inquiry scan
   hci_send_cmd(hci_sock, OGF_HOST_CTL, OCF_WRITE_SCAN_ENABLE, 1, &param);
   close(hci_sock);
   ```

3. **L2CAP Server Configuration**
   ```c
   // Create L2CAP server socket and bind to hci0
   server_socket = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
   loc_addr.l2_family = AF_BLUETOOTH;
   bacpy(&loc_addr.l2_bdaddr, &local_bdaddr);
   loc_addr.l2_psm = htobs(L2CAP_SERVER_PORT_NUM);
   bind(server_socket, (struct sockaddr*)&loc_addr, sizeof(loc_addr));
   ```

4. **BB Protocol Handshake**
   ```c
   // Initialize BB state as peripheral
   bbstate_init(&state, BB_ROLE_PERIPHERAL, public_key, private_key, remote_public_key, NULL);
   // Receive handshake request from central
   recv(client_socket, buffer, sizeof(struct bb_session_start_req), 0);
   bb_session_start_rx(&state, buffer);
   // Send handshake response
   bb_session_start_rsp(&state, buffer);
   send(client_socket, buffer, sizeof(struct bb_session_start_req), 0);
   ```

5. **Console Interface**
   ```c
   void console_interface(int socket, bbstate* state) {
       // Interactive command loop for user
       while (fgets(input, sizeof(input), stdin)) {
           parse_command(input, &msg);
           // Encrypt and send command
           size_t enc_len = aead_encrypt(encrypted_msg, state->key, state->counter++, NULL, 0, (uint8_t*)&msg, sizeof(msg));
           send(socket, encrypted_msg, enc_len, 0);
           // Receive and decrypt response
           recv(socket, encrypted_msg, sizeof(encrypted_msg), 0);
           size_t dec_len = aead_decrypt(decrypted_resp, state->key, state->counter++, NULL, 0, encrypted_msg, recv_len);
           memcpy(&resp, decrypted_resp, sizeof(resp));
           printf("Response: %s", resp.status);
       }
   }
   ```

### Central Implementation (`central.c`)

**Key Components:**

1. **Bluetooth Client Setup**
   ```c
   // Uses hci0 device for client role
   int dev_id = 0;              // hci0 device ID
   bdaddr_t local_bdaddr = {0}; // Store hci0 address
   int hci_sock = hci_open_dev(dev_id);
   hci_read_bd_addr(hci_sock, &local_bdaddr, 0);
   ```

2. **L2CAP Client Connection**
   ```c
   // Create L2CAP client socket and bind to hci0
   sock = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
   local_addr.l2_family = AF_BLUETOOTH;
   bacpy(&local_addr.l2_bdaddr, &local_bdaddr);
   bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr));
   // Connect to peripheral server
   addr.l2_family = AF_BLUETOOTH;
   addr.l2_psm = htobs(L2CAP_SERVER_PORT_NUM);
   str2ba(L2CAP_SERVER_BLUETOOTH_ADDR, &addr.l2_bdaddr);
   connect(sock, (struct sockaddr*)&addr, sizeof(addr));
   ```

3. **BB Protocol Handshake**
   ```c
   // Initialize BB state as central
   bbstate_init(&state, BB_ROLE_CENTRAL, public_key, private_key, remote_public_key, NULL);
   // Send handshake request to peripheral
   bb_session_start_req(&state, buffer);
   send(sock, buffer, sizeof(struct bb_session_start_req), 0);
   // Receive handshake response
   recv(sock, buffer, sizeof(struct bb_session_start_req), 0);
   bb_session_start_rx(&state, buffer);
   ```

4. **Command Processing Loop**
   ```c
   void process_commands(int socket, bbstate* state) {
       while (1) {
           // Receive encrypted command from peripheral
           ssize_t recv_len = recv(socket, encrypted_msg, sizeof(encrypted_msg), 0);
           // Decrypt command using AEAD
           size_t dec_len = aead_decrypt(decrypted_msg, state->key, state->counter++, NULL, 0, encrypted_msg, recv_len);
           memcpy(&msg, decrypted_msg, sizeof(msg));
           // Execute TROPIC01 operation based on command
           // ... (see central.c for full command handling)
           // Encrypt and send response
           size_t enc_len = aead_encrypt(encrypted_resp, state->key, state->counter++, NULL, 0, (uint8_t*)&resp, sizeof(resp));
           send(socket, encrypted_resp, enc_len, 0);
       }
   }
   ```

## 🔗 TROPIC01 Integration

- All TROPIC01 operations (random, ECC, memory, etc.) are performed on the central device, triggered by commands from the peripheral.
- The central uses `lt-util` to interface with the TROPIC01 secure element for all cryptographic and memory operations.
- The peripheral is a secure, interactive console for the user; it does not access the TROPIC01 hardware directly.

**Note:**
- The public/private key pairs and Bluetooth addresses must match between the two devices for the handshake to succeed.
- All communication is encrypted and authenticated using the session key derived from the handshake.
