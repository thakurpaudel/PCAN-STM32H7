# PCAN-USB Pro FD Adapter Firmware (Cross-Platform)

A high-performance, open-source firmware implementation for **STM32H7** and **ESP32-S3** microcontrollers that emulates the PEAK PCAN-USB Pro FD adapter. This project enables compatible hardware to interface seamlessly with standard PCAN software tools (PCAN-View, PCAN-Explorer, etc.) via USB.

## 🚀 Features

*   **Multi-Platform**: Unified protocol core (`pcan_common`) running on both STM32 and ESP32 architectures.
*   **Dual/Single Channel Support**: Supports CAN interfaces mapped to respective hardware peripherals (FDCAN for STM32, TWAI for ESP32).
*   **CAN-FD & Classic CAN**: Supports Flexible Data-rate (FD) and standard CAN 2.0B depending on the underlying hardware capabilities.
*   **USB 2.0 High/Full Speed**: Utilizes STM32 USB OTG or ESP32 TinyUSB stacks for low-latency communication.
*   **Standard Compatibility**: 
    *   Fully compatible with official PEAK drivers (Linux `peak_usb` & Windows).
    *   Works out-of-the-box with PCAN-View and PCAN-Basic API.
*   **Precise Timing**: Hardware-based timestamping for accurate message logging.

## 📦 Project Structure

```
PCAN/
├── pcan_common/           # Unified Cross-Platform PCAN Protocol Core
│   ├── pcanpro_protocol.c # Shared USB-CAN packet processing logic
│   └── pcanpro_fd_...     # Shared CAN-FD packet definitions
├── esp32s3/               # ESP-IDF specific project
│   └── main/
│       ├── can/           # TWAI (CAN) ESP32 driver interface
│       └── usb/           # TinyUSB descriptors and hardware configuration
├── stm32/                 # STM32Cube specific project
│   ├── Core/
│   │   ├── can/           # STM32 FDCAN driver interface
│   │   └── usb/           # STM32 HAL USB device configuration
│   ├── Drivers/           # STM32 HAL & Low-Layer Drivers
│   └── Middlewares/       # STM32 USB Device Library
└── README.md
```

## 🛠 Hardware Specifications

### STM32H743ZIT6 (Nucleo-144 or Custom Board)
| Feature | Pin | Description |
| :--- | :--- | :--- |
| **USB** | `PA11` (DM), `PA12` (DP) | USB OTG FS (used as device) |
| **CAN1** | `PD0` (RX), `PD1` (TX) | FDCAN1 Interface |
| **CAN2** | `PB5` (RX), `PB6` (TX) | FDCAN2 Interface |
| **LED 1** | `PC7` | Activity Indicator (Channel 1) |
| **LED 2** | `PE2` | Activity Indicator (Channel 2) |
| **Debug** | `PA9` (TX), `PA10` (RX) | USART1 for printf logs (115200 8N1) |

### STM32H745I-DISCO (Discovery Board)
| Feature | Pin | Description |
| :--- | :--- | :--- |
| **USB** | `PA11` (DM), `PA12` (DP) | USB OTG FS (USB_FS2 connector) |
| **CAN1** | `PH14` (RX), `PH13` (TX) | FDCAN1 Interface (routes to U22 Transceiver) |
| **CAN2** | `PB5` (RX), `PB13` (TX) | FDCAN2 Interface (routes to U21 Transceiver) |
| **LED 1** | `PJ2` (Green, LD7) | Status / USB Link Indicator (Active Low) |
| **LED 2** | `PI13` (Red, LD6) | CAN Activity Indicator (Active Low) |
| **Debug** | `PA9` (TX), `PA10` (RX) | USART1 for printf logs (115200 8N1) |

### ESP32-S3
*Note: Pin mappings for ESP32-S3 can be configured dynamically via `idf.py menuconfig` under the PCAN settings menu.*
| Feature | Default Pin | Description |
| :--- | :--- | :--- |
| **USB** | `GPIO19` (D-), `GPIO20` (D+) | Built-in native USB JTAG/Serial |
| **CAN/TWAI** | Configurable | TWAI Interface |

---

## 🔨 Build Instructions

### Building for STM32

**Prerequisites:** CMake (3.22+), ARM GCC Toolchain (`arm-none-eabi-gcc`), Make or Ninja.

1. Navigate to the STM32 project:
   ```bash
   cd stm32
   ```
2. Configure with CMake:
   * **For STM32H743 (Nucleo):**
     ```bash
     cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake -DPCAN_DISCO_H745=OFF
     ```
   * **For STM32H745 (Discovery):**
     ```bash
     cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake -DPCAN_DISCO_H745=ON
     ```
3. Compile:
   ```bash
   cmake --build build -j$(nproc)
   ```
4. Flash using STM32CubeProgrammer or OpenOCD:
   ```bash
   openocd -f interface/stlink.cfg -f target/stm32h7x.cfg -c "program build/PCAN.elf verify reset exit"
   ```

### Building for ESP32-S3

**Prerequisites:** ESP-IDF v5.x installed and sourced.

1. Navigate to the ESP32 project:
   ```bash
   cd esp32s3
   ```
2. Set the target architecture:
   ```bash
   idf.py set-target esp32s3
   ```
3. (Optional) Configure CAN pins and USB parameters:
   ```bash
   idf.py menuconfig
   ```
4. Build and Flash:
   ```bash
   idf.py build flash monitor
   ```

---

## 🔌 Usage

1.  **Connect** the board to your PC via the designated USB data port.
2.  **Verify Enumeration**:
    *   **Linux**: Run `lsusb` and `dmesg`. You should see `PEAK-System Technik GmbH PCAN-USB Pro FD` attaching to the `peak_usb` kernel module.
    *   **Windows**: Device Manager should show "PCAN-USB Pro FD" (assuming PEAK drivers are installed).
3.  **Open PCAN-View**:
    *   Select the emulated device from the hardware list.
    *   Initialize the bitrate (e.g., 500 kbit/s).
    *   You should now see bidirectional TX/RX CAN traffic!

## 🤝 Contributing
Contributions to extend hardware support, improve stability, or enhance the shared `pcan_common` core are highly welcome.

## 📄 License
This software component relies on vendor-specific abstractions (STMicroelectronics HAL, Espressif ESP-IDF). The PCAN protocol emulation logic is provided for educational, debugging, and interoperability purposes. Please refer to individual library licenses.
