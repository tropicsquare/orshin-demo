# ORSHIN Demonstrator Platform

This repository contains the ORSHIN demonstrator platform integrating following ORSHIN deliverables and related assets:
  - [BB-protocols](https://github.com/sacca97/bb-sec-protocols)
  - [New Secure Channel Protocol 03](https://doi.org/10.23919/DATE64628.2025.10992943)
  - [AttackDefense Framework (ADF)](https://doi.org/10.1145/3698396)
  - [TROPIC01 Secure Element](https://github.com/tropicsquare/tropic01)

## Architecture


```mermaid
flowchart LR
    A[RPi – Terminal<br/>Peripheral Device]
    A <-.->|encrypted BB| B[RPi<br/>Central Device]
    B <-.->|Noise/NSCP03| C[TROPIC01 Secure Element<br/>OR<br/>CORE-V Emulated Secure Element]

    style A fill:#e1f5fe
    style B fill:#f3e5f5
    style C fill:#fff3e0
```

![Hardware Setup of the Demo Platform](img/hw_setup_lowQ.png)
<sub>[High Resolution view of the Hardware Setup of the Demo Platform →](img/hw_setup.png)</sub>


The Demonstrator platform consists of two RPi devices: the first RPi acts as a terminal, and the second RPi acts as a remote host with a connected secure element that provides remote secure services to the terminal RPi.

Wireless communication between the RPi devices is secured by the **BB-protocol** over BLE.

Two secure elements can be used interchangeably on the second RPi:
- the TROPIC01 physical chip (as an RPi extension shield or a USB plug)
- the RISC-V-emulated Secure Element implementing the **New Secure Channel Protocol 03** over I2C

## Repository Structure

- **examples** – software examples for the demo platform
- **img** – images
- **orshin-sc-fpga** – dependencies for enabling the RISC-V-emulated Secure Element
- **orshin-sc** – dependencies for enabling the ORSHIN Secure Channel on RPi
- **threat-model** – threat model for the demo platform using the **AttackDefense Framework (ADF)** and **MITRE EM3ED**
- **tropic01-se** – dependencies for enabling the TROPIC01 physical Secure Element


## Get Started!

**Note:** Not all submodules are public yet. The repository opening is currently a work in progress.

```bash
git clone --recurse-submodules https://github.com/tropicsquare/orshin-demo
```

**Demo Bringup:**

- Follow the [Bill of Materials](BOM.md) to acquire hardware needed for the complete demo setup.
- Follow the [Interconnect guide](WIRING.md) to connect particular boards together.
- Follow the [BB-protocols guide](BB_BRINGUP.md) to enable secure communication between two RPis.
- Follow:
  * [TROPIC01 guide](TROPIC01_BRINGUP.md) to configure both RPis with the TROPIC01 physical Secure Element,
  * or [ORSHIN Secure Channel guide](ORSHIN_SC_BRINGUP.md) to configure both RPis with the RISC-V-emulated Secure Element.


## Examples

Several software examples are available for the demonstrator platform, and several software components need to be orchestrated to bring-up the platform.

To enable the BB-protocol serving for secure communication, follow [BB-protocols over L2CAP for Bluetooth Classic (BR/EDR)](BB_BRINGUP.md) instructions.
To enable TROPIC01 secure element, providing cryptografic functions in certain demo configurations, follow [TROPIC01 Secure Element Enablement](TROPIC01_BRINGUP.md) instructions.

Following examples are available for the ORSHIN demo:

  - [Secure Bluetooth L2CAP communication using the BB protocol with AEAD encryption](examples/BB-Communication_Example/)
  - [A lightweight, secure implementation for Bluetooth communication with the TROPIC01 secure element.](examples/BB-TP01_Integration/)
  - [Tetris Game with the TROPIC01 Secure Element](examples/Tetris_TP01/)
  - [Tetris Game with selectable SE TP01/CORE-V](examples/Tetris_TP01_COREV/)


## Demonstrator Threat Model

The AttackDefense Framework (ADF) is used to model threats to a device in the following phases:

  1. System and Attacker Modeling
  2. Threat Identification
  3. Threat Ranking
  4. Defense Strategy

For the Demonstrator Platform model, we use the [extended](https://tropicsquare.github.io/orshin-adf/) approach, interconnecting the ADF-based threat model with [MITRE EMB3D](https://emb3d.mitre.org) and [MITRE CVE](https://www.cve.org/), where MITRE EMB3D serves for threat model definition using *Device Properties* (PID), and ADF improves the detail of the model by providing a wide, custom set of threats.

For details, see the [Threat Model](threat_model/) of the demonstrator, including coverage of the four phases mentioned above.

# About ORSHIN
It is common wisdom that cyber security is only as strong as the weakest link in a chain. Therefore, the main challenge is to identify the critical points of IoT infrastructure. To address this issue, ORSHIN is creating the first generic and integrated methodology, called trusted lifecycle, to develop secure network devices based on open-source components while managing their entire lifecycle. ORSHIN's trustworthy lifecycle consists of different phases (design, implementation, evaluation, installation, maintenance and retirement) that form a chain of trust. This lifecycle defines how the safety objectives are translated into policies for defined phases. Using this holistic view, ORSHIN will address critical links, reduce threats and improve security of open-source devices.

# About Tropic Square
[Tropic Square](https://tropicsquare.com) develops open-architecture secure elements on the foundation of Security through Transparency. We invite testing and evaluation to verify the reliability of our products.

[TROPIC01](https://github.com/tropicsquare/tropic01)  is the first product in its lineup, serving as a cryptographic coprocessor that establishes a hardware root of trust, ensuring the protection of the system’s most sensitive assets while offloading cryptographic functions from the host MCU.

# License
See the [LICENSE.md](LICENSE.md) file in the root of this repository or consult license information at Tropic Square website.

Examples in this repository are based on BB-protocols distributed under the terms of the MIT License. See the [LICENSE_BB](LICENSE_BB) file.

# Acknowledgement
Funded by the European Union under grant agreement no. 101070008. Views and opinions expressed are however those of the author(s) only and do not necessarily reflect those of the European Union. Neither the European Union nor the granting authority can be held responsible for them.
