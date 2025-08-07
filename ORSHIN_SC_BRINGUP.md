# Secure Channel Bringup

The [New Secure Channel Protocol](https://github.com/securitypattern/orshin-STM32-client-scp03-nscp) (NSCP) implementation runs on the RISC-V CPU, representing a Secure Element, emulated on the Nexys A7 FPGA.

To bring up demo platform configurations involving the NSCP03, Nexys A7 must be configured properly.
To communicate with the Secure Element from the RPi computer, the NSCP03 support must be enabled on the RPi side.

When software running on the RISC-V CPU needs to be modified, a development environment must be prepared on a separate workstation.

## Prerequisites

The cloned dependency submodule repositories in **../orshin-sc-fpga** and **../orshin-sc**.

The setup was tested with the [Raspberry Pi OS 12 (bookworm)](https://www.raspberrypi.com/software/operating-systems/), the Lite image was used, and the following additional packages need to be installed:

- [libssl1.1_1.1.1w-0+deb11u1_arm64](http://ftp.debian.org/debian/pool/main/o/openssl/libssl1.1_1.1.1w-0+deb11u1_arm64.deb)
- [libssl-dev_1.1.1w-0+deb11u1_arm64](http://ftp.debian.org/debian/pool/main/o/openssl/libssl-dev_1.1.1w-0+deb11u1_arm64.deb)

```shell
cd ~/Downloads
wget http://ftp.debian.org/debian/pool/main/o/openssl/libssl1.1_1.1.1w-0+deb11u1_arm64.deb
wget wget http://ftp.debian.org/debian/pool/main/o/openssl/libssl-dev_1.1.1w-0+deb11u1_arm64.deb

sudo dpkg -i ./libssl1.1_1.1.1w-0+deb11u1_arm64.deb libssl-dev_1.1.1w-0+deb11u1_arm64.deb
```

Install OpenOCD on RPi via package manager (version 0.12.0).
```
sudo apt-get install openocd
```

## Flashing Bitstream and Firmware to FPGA
You can load CoreV onto the Nexys A7 either temporarily or permanently. 
  
Choose the method that fits your needs in the sections below.

### Temporary FPGA Configuration

In this case, the bitstream is loaded into FPGA but cleared after reset.

Go to the cloned [core-v-mcu](https://github.com/openhwgroup/core-v-mcu) repository folder and run.

```shell
make downloadn NEXYSA7_BITMAP=emulation/quickstart/core_v_mcu_nexys.bit
```

### Permanent FPGA Configuration

In this case, the bitstream is stored in QSPI and reloaded on every reset.

For permanent flashing to QSPI:
- Ensure Vivado is installed on a separate workstation (see instructions below).
- Set JP1 and JP2 switches as described in the [Nexys A7 reference manual](https://digilent.com/reference/programmable-logic/nexys-a7/reference-manual) (Chapter 2: FPGA Configuration).
- In Vivado, generate the .bin memory configuration file:
```
Flow > Hardware Manager
Tools > Generate Memory Configuration File ...
```
In the Generate Memory Configuration File dialog:
- Set Format: BIN
- Set Custom Memory Size: 16MB
- Set Filename: path_to_your_file.bin
- Set Interface: SPIx1
- Check Load Bitstream File and select the bitstream from the quickstart folder in the core-v-mcu repository (emulation/quickstart/core_v_mcu_nexys.bit)
- Enable Overwrite
Vivado will then display the corresponding command in the bottom field.
```
write_cfgmem  -format bin -size 16 -interface SPIx1 -loadbit {up 0x00000000 "/home/your_name/path_to_coreV/core-v-mcu/emulation/quickstart/core_v_mcu_nexys.bit" } -force -file "/home/your_name/path_to_coreV/core-v-mcu/emulation/quickstart/core_v_mcu_nexys.bin"
```
Generate the .bin file, then open the target in Vivado. In the main window, navigate to:
```
Tools > Autoconnect
```
Load the .bin file to QSPI memory:
In the main Vivado window, click:
```
Tools > Add Configuration Memory Device > xc7a100t_0
```
- In the Program Flash dialog, search for s25fl128l-spi-x1_x2_x4 and click OK.
- Set the path to the .bin configuration file you generated.
- Ensure Erase, Program, and Verify checkboxes are selected.
Your QSPI programming is now complete!

## Automatic RISC-V Firmware Upload

To enable the demo, firmware needs to be loaded into an FPGA-emulated RISC-V CPU.

The *cli_test* demo is launched automatically via the *systemd* rpi_riscv_loader.service, which uploads it to the
RISC-V core. 

First, connect Nexys A7 to the Raspberry Pi via UART (serial console), and the HS-2 programmer (OpenOCD channel).

Then install the new system service and its dependencies:
```
cd ../orshin-sc
sudo ./install
systemctl status rpi_riscv_loader
```
The install script creates the **/opt/rpi-rpi-loader** path, where all service dependencies are installed, then it installs and starts a new *systemd* service.


## Firmware Development Environment Installation

The development environment must be prepared on a separate workstation (a more powerful workstation is needed; RPi cannot be used due to a lack of system resources) if you
need to modify the demo FW (or FPGA configuration). You can skip this part if you only need to get the demo running.

The following tools were used for the current demo development:

- [corev-openhw-gcc-rocky8-20230622](https://buildbot.embecosm.com/job/corev-gcc-rocky8/lastSuccessfulBuild/artifact/build-sources.txt)
- [openocd-0.12.0](https://deac-fra.dl.sourceforge.net/project/openocd/openocd/0.12.0/openocd-0.12.0.zip?viasf=1)
- [Eclipse 2025-06 (4.36.0)](https://download.eclipse.org/eclipse/downloads/drops4/R-4.36-202505281830/)
- [Vivado v2023.2](https://www.xilinx.com/support/download.html)

Follow each package’s official installation guide, and install all software packages on your workstation.

### Firmware Development Environment Setup

Run Eclipse version 2025.06. In Eclipse, import the project:

```
File > Import > Git > Project from Git > Existing local repository
```

Then pick the repository **orshin-corev-secure-element** from **../orshin-sc-fpga** and click next. Since
there are more eclipse projects, you will see a bunch of projects. Leave only the __cli_test__ checked.

Then you need to load the launch configuration file.
```
File > Import > Run/Debug > Launch Configurations
```

Browse to the **launch** folder in the **orshin-corev-secure-element** repository, and check
only the **cli_test Default.launch** file, then press Finish.

In the top toolbar, click on the bug icon, then select Debug Configurations.
You will see your loaded launch files in the left-hand panel under GDB OpenOCD
Debugging. Click on **cli_test Default** and go to the Startup tab.

At the bottom, in the Run/Restart Commands section, there is an option called
Pre-run/Restart reset. Make sure this option is unchecked — otherwise, CoreV
will be erased every time you reload your program.

Install and configure the correct toolchain paths in your workspace. Go to
Preferences, and configure OpenOCD according to your setup, by default use:

```
Window > Preferences > Workspace OpenOCD Path
Executable: openocd
Folder: /usr/local/bin 
```

A similar process applies to the GCC toolchain. Set the toolchain folder:

```
Window > Preferences > Workspace RISC-V Toolchains Paths
Toolchain folder: PATH_TO_GCC_TOOLCHAIN/corev-openhw-gcc-rocky8-20230622/bin
```

More detailed Eclipse configuration instructions can be found
[here](https://github.com/openhwgroup/core-v-mcu-cli-test).

#### Serial Console

In Eclipse, add a serial console.
```
Window > Show view > Terminal
```

Select the serial port to which you connected the UART cable and leave all
other settings at their defaults. If you have already flashed the FPGA, you
should be periodically receiving the message "BOOTME".

Now, click the hammer icon in the top toolbar to compile the cli_test project.
Once the build succeeds, click the play icon to start the slave. You should see
"SecPat Receive" printed in the console, indicating that the system is ready to
receive data.

