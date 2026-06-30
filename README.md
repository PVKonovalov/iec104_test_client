# iec104_test_client

A console client for testing IEC 60870-5-104 servers. It connects to a server, sends a General Interrogation command, and prints all incoming ASDUs to stdout in a human-readable format. Useful for verifying that a server publishes the correct data objects with the expected values and quality flags.

## Supported ASDU types

| Type ID | Name | Description |
|---------|------|-------------|
| 1  | M_SP_NA_1 | Single-point information |
| 3  | M_DP_NA_1 | Double-point information |
| 9  | M_ME_NA_1 | Measured value, normalized |
| 11 | M_ME_NB_1 | Measured value, scaled |
| 13 | M_ME_NC_1 | Measured value, short floating-point |
| 21 | M_ME_ND_1 | Measured value, normalized, no quality |
| 30 | M_SP_TB_1 | Single-point with CP56Time2a timestamp |
| 31 | M_DP_TB_1 | Double-point with CP56Time2a timestamp |
| 34 | M_ME_TD_1 | Measured value, normalized, with CP56Time2a timestamp |
| 36 | M_ME_TF_1 | Measured value, short floating-point, with CP56Time2a timestamp |

## Build

```sh
git clone https://github.com/mz-automation/lib60870.git vendor/lib60870-C
cmake -B build
cmake --build build
```

## Usage

```
iec104_test_client --host <address> [OPTIONS]
```

### Connection options

| Flag | Default | Description |
|------|---------|-------------|
| `--host` | *(required)* | Server IP address |
| `--port` | 2404 | TCP port |
| `--ca` | 1 | Common address (station address) |
| `--ioa` | 0 (all) | Print only the object with this IOA; 0 prints all |

### APCI parameters

| Flag | Default | Description |
|------|---------|-------------|
| `--k` | 12 | Maximum number of unacknowledged I-frames |
| `--w` | 8 | Acknowledge after w received I-frames |
| `--t0` | 30 | Connection establishment timeout (seconds) |
| `--t1` | 15 | Send / test APDU timeout (seconds) |
| `--t2` | 10 | Supervisory ACK timeout (seconds) |
| `--t3` | 20 | Test frame idle timeout (seconds) |

### Examples

Connect to a server on the default port and print all data objects:
```sh
iec104_test_client --host 192.168.1.10
```

Connect on a custom port with a specific common address:
```sh
iec104_test_client --host 192.168.1.10 --port 2404 --ca 2
```

Watch a single information object:
```sh
iec104_test_client --host 192.168.1.10 --ca 1 --ioa 20
```

Press **Ctrl-C** to disconnect and exit.

```
Trying to connect to: 192.168.1.20:2404:1
Connection established
Connected!
SEND: 68 04 07 00 00 00 
Sending general interrogation with the common address: 1
SEND: 68 0e 00 00 00 00 64 01 06 00 01 00 00 00 00 14 
RCVD: 68 04 0b 00 00 00 
Received STARTDT_CON
RCVD: 68 0e 00 00 02 00 64 01 07 00 01 00 00 00 00 14 
RECVD ASDU type: C_IC_NA_1(20) COT: ACTIVATION_CON CA: 1 elements: 1
RCVD: 68 22 02 00 02 00 01 06 14 00 01 00 b9 0b 00 00 ba 0b 00 00 bb 0b 00 01 bc 0b 00 00 bd 0b 00 00 be 0b 00 00 
RECVD ASDU type: M_SP_NA_1(1) COT: INTERROGATED_BY_STATION CA: 1 elements: 6
    DI  IOA: 201 Q:00 (GOOD) value: 0
    DI  IOA: 102 Q:00 (GOOD) value: 0
    DI  IOA: 103 Q:00 (GOOD) value: 1
    DI  IOA: 104 Q:00 (GOOD) value: 0
    DI  IOA: 105 Q:00 (GOOD) value: 0
    DI  IOA: 106 Q:00 (GOOD) value: 0
RCVD: 68 0e 04 00 02 00 03 01 14 00 01 00 d1 07 00 01 
RECVD ASDU type: M_DP_NA_1(3) COT: INTERROGATED_BY_STATION CA: 1 elements: 1
DI  IOA: 2001 Q:00 (GOOD) value: 1
RCVD: 68 8a 06 00 02 00 0d 10 14 00 01 00 e9 03 00 05 08 f1 41 00 ea 03 00 6d 15 02 41 00 eb 03 00 36 db 41 41 00 ec 03 00 fb 85 c5 45 00 ed 03 00 d1 e6 c8 45 00 ee 03 00 97 07 c6 45 00 ef 03 00 67 0f 2b 46 00 f0 03 00 5f fc 2d 46 00 f1 03 00 a6 7f 2b 46 00 f2 03 00 2f 9a 97 48 00 f3 03 00 9e e8 8b 47 00 f4 03 00 7d ff 9b 48 00 f5 03 00 7d ba 47 42 00 f6 03 00 7d ba 47 42 00 f7 03 00 7d ba 47 42 00 f8 03 00 00 00 80 40 00 
RECVD ASDU type: M_ME_NC_1(13) COT: INTERROGATED_BY_STATION CA: 1 elements: 16
    IOA: 201 Q:00 (GOOD) value: 30.128916
    IOA: 202 Q:00 (GOOD) value: 8.130231
    IOA: 203 Q:00 (GOOD) value: 12.116018
    IOA: 204 Q:00 (GOOD) value: 6320.747559
    IOA: 205 Q:00 (GOOD) value: 6428.852051
    IOA: 206 Q:00 (GOOD) value: 6336.948730
    IOA: 207 Q:00 (GOOD) value: 10947.850586
    IOA: 208 Q:00 (GOOD) value: 11135.092773
    IOA: 209 Q:00 (GOOD) value: 10975.912109
    IOA: 2010 Q:00 (GOOD) value: 310481.468750
    IOA: 2011 Q:00 (GOOD) value: 71633.234375
    IOA: 2012 Q:00 (GOOD) value: 319483.906250
    IOA: 2013 Q:00 (GOOD) value: 49.932117
    IOA: 2014 Q:00 (GOOD) value: 49.932117
    IOA: 2015 Q:00 (GOOD) value: 49.932117
    IOA: 2016 Q:00 (GOOD) value: 4.000000
```


## Dependencies

- **[lib60870-C](https://github.com/mz-automation/lib60870)** — third-party library, not included in this repository.
  Clone or download it and place it at `vendor/lib60870-C`:
  ```sh
  git clone https://github.com/mz-automation/lib60870.git vendor/lib60870-C
  ```
  The repository contains the C library one level deeper at `lib60870-C/lib60870-C/`, which CMake expects at `vendor/lib60870-C/lib60870-C/` — the clone command above places it correctly.
- **[cppflags](vendor/cppflags)** — vendored header-only flag parsing library (included).
