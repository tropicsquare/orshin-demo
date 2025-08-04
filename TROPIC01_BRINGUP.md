# TROPIC01 Secure Element Enablement

This introductory readme descibes how to enable TROPIC01 with the ORSHIN demo.

## Prerequisites

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

## 🔗 Resources

- [libtropic-util](https://github.com/tropicsquare/libtropic-util) - Command-line utility for TROPIC01
- [TROPIC01 Documentation](https://tropicsquare.com/tropic01) - Official TROPIC01 documentation
