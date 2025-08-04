# TROPIC01 Secure Bluetooth Communication

A lightweight, secure implementation for Bluetooth communication with the TROPIC01 secure element. This project demonstrates how to securely interface with TROPIC01 secure elements over Bluetooth using encrypted commands and responses, built on the **bb-protocol (BlueBrothers)** framework.

## 🏗️ Architecture

```
[User Console] → [Peripheral (Server, hci0)] ←encrypted→ [Central (Client, hci0)] → [TROPIC01 Secure Element]
```

- **Peripheral (peripheral.c)**: Command-line console, acts as a Bluetooth L2CAP server on hci0, enables scan mode for discoverability, and sends encrypted commands.
- **Central (central.c)**: Bluetooth L2CAP client on hci0, connects to the peripheral, performs all TROPIC01 secure element operations, and sends encrypted responses.
- **TROPIC01**: Secure element providing cryptographic operations, accessed only by the central.
- **Protocol**: Secure communication using **bb-protocol (BlueBrothers)** handshake + AEAD encryption, with pre-shared keys for authentication.

## 🚀 Quick Start

### Prerequisites

1. **Bluetooth Development Libraries**
   ```bash
   sudo apt-get install libbluetooth-dev
   ```

2. **TROPIC01 Hardware**
   - USB dongle with TROPIC01 secure element, OR
   - Raspberry Pi shield with TROPIC01 secure element

3. **For USB Dongle:**
- Connect TROPIC01 USB dongle
- Ensure user is in `dialout` group: `sudo adduser $USER dialout`
- Log out and back in

4. **For Raspberry Pi Shield:**
- Enable SPI: `sudo raspi-config` → Interfaces → SPI
- Install WiringPi: `sudo apt install wiringpi`
- Connect shield according to pinout diagram

5. **libtropic-util** (Optional - for real hardware)
   ```bash
   # Clone and build libtropic-util
   git clone --recurse-submodules https://github.com/tropicsquare/libtropic-util
   cd libtropic-util
   
   # For USB dongle:
   mkdir build && cd build && cmake -DUSB_DONGLE_TS1302=1 .. && make
   sudo cp lt-util /bin/ && sudo chmod +x /bin/lt-util
   
   # For Raspberry Pi shield:
   mkdir build && cd build && cmake -DHW_SPI=1 .. && make
   sudo cp lt-util /bin/ && sudo chmod +x /bin/lt-util
   ```

### 🔧 Build and Run

**To find your peripheral's MAC address:**

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

## 📋 Supported Commands

All commands are based on the [libtropic-util](https://github.com/tropicsquare/libtropic-util) functionality and are executed on the central device:

| Command | Description | Example | Response |
|---------|-------------|---------|----------|
| `random <count>` | Generate random bytes (1-255) | `random 32` | `OK - 32 bytes: a1b2c3d4...` |
| `ecc-gen <slot>` | Generate ECC key pair in slot | `ecc-gen 0` | `OK - ECC key generated` |
| `ecc-download <slot>` | Get public key from slot | `ecc-download 0` | `OK - Public key: 0449f8c1...` |
| `ecc-clear <slot>` | Clear ECC slot | `ecc-clear 0` | `OK - ECC slot cleared` |
| `ecc-sign <slot> <data>` | Sign data with key from slot | `ecc-sign 0 hello` | `OK - Signature: 30450221...` |
| `mem-store <slot> <data>` | Store data in memory slot | `mem-store 0 secret` | `OK - Data stored` |
| `mem-read <slot>` | Read data from memory slot | `mem-read 0` | `OK - Data: secret` |
| `mem-erase <slot>` | Erase memory slot | `mem-erase 0` | `OK - Memory slot erased` |
| `exit` | Exit the console | `exit` | Exits cleanly |

## 🔒 Security Features

- **bb-protocol (BlueBrothers) Handshake**: Mutual authentication between devices using pre-shared public/private keys
- **AEAD Encryption**: Authenticated encryption for all messages
- **Counter-based Nonces**: Prevents replay attacks
- **Hardware-backed Operations**: All cryptographic operations use TROPIC01 secure element (on central)
- **Secure Key Storage**: Private keys never leave the secure element

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

## 🛠️ Troubleshooting

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

## 🔗 Related Projects

- [libtropic-util](https://github.com/tropicsquare/libtropic-util) - Command-line utility for TROPIC01
- [TROPIC01 Documentation](https://tropicsquare.com) - Official TROPIC01 documentation 