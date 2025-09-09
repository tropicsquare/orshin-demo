# TROPIC01 Secure Bluetooth Communication

A lightweight, secure implementation for Bluetooth communication with the TROPIC01 secure element. This project demonstrates how to securely interface with TROPIC01 secure elements over Bluetooth Physical Channel using encrypted commands and responses, built on the **BB-protocol** framework.

## 🏗️ Architecture

```mermaid
flowchart LR
    A[User Console] --> B[Peripheral<br/>Server, hci0]
    B -.->|encrypted| C[Central<br/>Client, hci0]
    C --> D[TROPIC01<br/>Secure Element]
    
    style A fill:#e1f5fe
    style B fill:#f3e5f5
    style C fill:#f3e5f5
    style D fill:#fff3e0
```

- **Peripheral (peripheral.c)**: Command-line console, acts as a Bluetooth L2CAP server on hci0, enables scan mode for discoverability, and sends encrypted commands.
- **Central (central.c)**: Bluetooth L2CAP client on hci0, connects to the peripheral, performs all TROPIC01 secure element operations, and sends encrypted responses.
- **TROPIC01**: Secure element providing cryptographic operations, accessed only by the central.
- **Protocol**: Secure communication using the **BB-protocol** handshake + AEAD encryption, with pre-shared keys for authentication.

## 🔒 Security Features

- **BB-protocol Handshake**: Mutual authentication between devices using pre-shared public/private keys
- **AEAD Encryption**: Authenticated encryption for all messages
- **Counter-based Nonces**: Prevents replay attacks
- **Hardware-backed Operations**: All cryptographic operations use TROPIC01 secure element (on central)
- **Secure Key Storage**: Private keys never leave the secure element

## Security Note

⚠️ **Warning**: This demo is for educational and demonstration purposes only. Keys are hardcoded and should not be used in production.
- The public/private key pairs and Bluetooth addresses must match between the two devices for the handshake to succeed.
- All communication is encrypted and authenticated using the session key derived from the handshake.

## 🚀 Get Started!

### Prerequisites

To enable the BB-protocol on RPi, follow [BB-protocols over L2CAP for Bluetooth Classic (BR/EDR)](../BB_BRINGUP.md) instructions.
To enable TROPIC01 secure element on the peripheral device, follow [TROPIC01 Secure Element Enablement](../TROPIC01_BRINGUP.md) instructions.

### Build and Run

**To find your peripheral's MAC address from the central device:**

```bash
hcitool dev
```
This will display all available Bluetooth devices and their addresses in the format `AA:BB:CC:DD:EE:FF`.

**Update the server address in `central.c`:**

```c
#define L2CAP_SERVER_BLUETOOTH_ADDR "AA:BB:CC:DD:EE:FF"  // Replace with your peripheral's address
#define L2CAP_SERVER_PORT_NUM 0x0235
```

1. **Compile the project**
   ```bash
   cmake .
   make central
   make peripheral
   ```

2. **Run Peripheral (Server, hci0) first**
   ```bash
   sudo ./peripheral
   ```
   - The peripheral will print its Bluetooth address and wait for a connection.
   - It enables scan mode for discoverability.
   - Use this address in the central's configuration.

3. **Run Central (Client, hci0) second**
   ```bash
   sudo ./central
   ```
   - The central connects to the peripheral's address and port.
   - All TROPIC01 operations are performed on the central, triggered by commands from the peripheral.

## Supported Commands

All commands are based on the [libtropic-util](https://github.com/tropicsquare/libtropic-util) functionality and are executed on the central device:

| lt-util Command | Description | Example | Response |
|-----------------|-------------|---------|----------|
| `./lt-util <serialport> -r <count> <file>` | Random - Get 1-255 random bytes and store them into file | `./lt-util /dev/ttyUSB0 -r 32 random.bin` | `OK - 32 bytes generated` |
| `./lt-util <serialport> -e -g <slot>` | ECC key - Generate private key in a given slot | `./lt-util /dev/ttyUSB0 -e -g 0` | `OK - ECC key generated in slot 0` |
| `./lt-util <serialport> -e -d <slot> <file>` | ECC key - Download public key from given slot into file | `./lt-util /dev/ttyUSB0 -e -d 0 pubkey.bin` | `OK - Public key downloaded` |
| `./lt-util <serialport> -e -c <slot>` | ECC key - Clear given ECC slot | `./lt-util /dev/ttyUSB0 -e -c 0` | `OK - ECC slot cleared` |
| `./lt-util <serialport> -e -s <slot> <file1> <file2>` | ECC key - Sign content of file1 (max 4095B) with key from slot | `./lt-util /dev/ttyUSB0 -e -s 0 data.txt sig.bin` | `OK - Signature created` |
| `./lt-util <serialport> -m -s <slot> <file>` | Memory - Store content of filename (max 444B) into memory slot | `./lt-util /dev/ttyUSB0 -m -s 0 secret.txt` | `OK - Data stored in slot 0` |
| `./lt-util <serialport> -m -r <slot> <file>` | Memory - Read content of memory slot (max 444B) into filename | `./lt-util /dev/ttyUSB0 -m -r 0 output.txt` | `OK - Data read from slot 0` |
| `./lt-util <serialport> -m -e <slot>` | Memory - Erase content of memory slot | `./lt-util /dev/ttyUSB0 -m -e 0` | `OK - Memory slot 0 erased` |
| `exit` | Exit the console | `exit` | Exits cleanly |


## 🎮 Example Session
```
TROPIC01> random 16
Response: OK - 16 bytes: a1b2c3d4e5f6789012345678abcdef01

TROPIC01> ecc-gen 0
Response: OK - ECC key generated in slot 0

TROPIC01> ecc-download 0
Response: OK - Public key: 0449f8c1b2a3d4e5f6789012345678...

TROPIC01> ecc-sign 0 "Hello, TROPIC01!"
Response: OK - Signature: 3045022100a1b2c3d4e5f6789012345678...

TROPIC01> mem-store 0 "My secret data"
Response: OK - Data stored

TROPIC01> mem-read 0
Response: OK - Data: My secret data

TROPIC01> exit
```

## Troubleshooting

### Common Issues

1. **Bluetooth Connection Fails**
   - Check if Bluetooth is enabled: `hciconfig`
   - Verify device addresses match
   - Ensure devices are paired

2. **Permission Denied**
   - Run with `sudo` for Bluetooth operations
   - Check user is in `dialout` group (USB dongle)

3. **TROPIC01 Not Found**
   - Verify hardware connection
   - Check if `lt-util` is installed and working
   - Test with: `lt-util -r 16 /tmp/test`

### Debug Mode

Enable debug output by setting environment variable:
```bash
export TROPIC_DEBUG=1
./central
```

## 🔗 TROPIC01 SE Integration

- All TROPIC01 Secure Element operations (random, ECC, memory, etc.) are performed on the central device, triggered by commands from the peripheral.
- The central uses `lt-util` to interface with the TROPIC01 secure element for all cryptographic and memory operations.
- The peripheral is a secure, interactive console for the user; it does not access the TROPIC01 hardware directly.

