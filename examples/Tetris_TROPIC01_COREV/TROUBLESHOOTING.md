# Troubleshooting Guide

## 1. Bluetooth Connection Failed

**Symptoms**: Central device cannot connect to peripheral

**Solutions**:
```bash
# Check Bluetooth status
sudo systemctl status bluetooth

# Restart Bluetooth service
sudo systemctl restart bluetooth

# Check device visibility
bluetoothctl scan on

# Trust and pair devices manually
bluetoothctl
[bluetooth]# trust XX:XX:XX:XX:XX:XX
[bluetooth]# pair XX:XX:XX:XX:XX:XX
```

## 2. Permission Denied

**Symptoms**: "Permission denied" when running executables

**Solutions**:
```bash
# Ensure executables have proper permissions
chmod +x bin/central bin/peripheral

# Run with sudo (required for Bluetooth access)
sudo ./bin/central
```

## 3. Build Errors

**Symptoms**: CMake or make fails

**Solutions**:
```bash
# Clean build directory
make clean
rm -rf build/

# Rebuild
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## 4. Hardware Not Found

**Symptoms**: Random number generation fails

**Solutions**:
- **CORE-V Mode**: Ensure CORE-V hardware is properly connected and drivers are installed
- **TROPIC01 Mode**: Ensure TROPIC01 hardware is properly connected and drivers are installed
- Check hardware connections and device permissions

## 5. CORE-V Mode Failures

**Symptoms**:
- "Failed to execute NSCP" error messages
- Application cannot generate random numbers in CORE-V mode
- `ex_se05x_crypto` not found errors

**Solutions**:
```bash
# Ensure ex_se05x_crypto is in the correct location
ls -la build/bin/ex_se05x_crypto

# If missing, copy it from the project root
cp ex_se05x_crypto build/bin/

# Make sure it's executable
chmod +x build/bin/ex_se05x_crypto

# Check if corev_random.txt exists and is readable
ls -la corev_random.txt

# If needed, delete the file to force regeneration
rm -f corev_random.txt
```

## 6. NSCP Session Issues

**Symptoms**: CORE-V mode works once but fails on subsequent runs

**Solutions**:
- This is expected behavior due to the current implementation limitation
- Perform a complete FPGA power cycle as described in the Important Notes section
- Delete `corev_random.txt` if you want to force fresh randomness generation
