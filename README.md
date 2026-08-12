# infrastructure_embstrin

Infrastructure module for the Embedded Streaming Interpreter (EmbStrIn) project — 
a framework to orchestrate, distribute, and run jobs across heterogeneous embedded 
devices (XIAO ESP32C6, MAX78000FTHR) connected to a host over USB.

This repo covers the **Infrastructure** section of the main project:
- Core data structures (tasks, programs, devices, host)
- Host-side API for managing devices, tasks, and program queues
- Device/link topology tracking
- Logging (host + device side)
- Firmware running on development boards

Main project: [Embedded Streaming Interpreter](https://github.com/codingclub-iitdh/Embedded-Streaming-Interpreter)

## Structure

| File | Description |
|---|---|
| `embstrin_types.h` | Core structs — `EMBSTRIN_Task`, `EMBSTRIN_Program`, `EMBSTRIN_Device`, `EMBSTRIN_Host` |
| `embstrin_api.h` / `embstrin_api.c` | Host-side API for program/task/device/queue management |
| `embstrin_logging.h` / `embstrin_logging.c` | Host + device logging with dual timestamps |
| `firmware/main.c` | Firmware running on ESP32C6 boards |
| `topology.h` / `topology.c` | Device/link graph and path-finding (in progress) |

## Status

- [x] Core types and API
- [x] Logging
- [x] Firmware skeleton (ESP32C6)
- [ ] Topology / path finding
- [ ] MAX78000 firmware (MaximSDK)
