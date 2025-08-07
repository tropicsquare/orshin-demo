#!/usr/bin/bash

echo "[INF] WAITING FOR THE HS2 ..."
while ! lsusb | grep -q "ID 0403:6014"; do
    sleep 0.5
done

echo "[INF] FOUND FTDI OPENOCD PORT"

MEMORY_OFFSET=0x1c000880
OPENOCD=/usr/local/bin/openocd
OPENOCD_CFG=/opt/orshin-rpi-loader/openocd-nexys-hs2.cfg
SREC_FILE=/opt/orshin-rpi-loader/cli_test.srec

$OPENOCD -f $OPENOCD_CFG -c "init; halt; load_image $SREC_FILE; resume $MEMORY_OFFSET; shutdown"

echo "[INF] IMAGE LOADED IN THE RISC-V CORE"
