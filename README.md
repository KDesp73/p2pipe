# P2Pipe

**P2Pipe** is a lightweight, multithreaded, peer-to-peer data transfer protocol built on top of **UDP**.
It provides reliable delivery, sequencing, and flow control — all without requiring TCP.

The project includes both the **protocol implementation** and a **CLI tool** to start, connect, and manage P2Pipe sessions.

---

## Table of Contents

* [Overview](#overview)
* [Installation](#installation)
* [Usage](#usage)

  * [Commands](#commands)
  * [Options](#options)
  * [Examples](#examples)
* [Protocol Summary](#protocol-summary)
* [License](#license)

---

## Overview

P2Pipe enables two peers to exchange arbitrary binary data reliably over UDP.
It can operate in three modes:

| Command  | Role     | Description                                 |
| -------- | -------- | ------------------------------------------- |
| `serve`  | Server   | Waits for peers and coordinates connections |
| `listen` | Receiver | Receives and writes incoming data           |
| `talk`   | Sender   | Reads and sends data to a peer              |


## Installation

```bash
git clone https://github.com/KDesp73/p2pipe.git
cd p2pipe
make all
sudo make install
```

You can then run `p2pipe` directly from your terminal.


## Usage

```
p2pipe <COMMAND> [<OPTIONS>...]
```

### Commands

| Command  | Description                                                   |
| -------- | ------------------------------------------------------------- |
| `serve`  | Act as a P2Pipe server — coordinate connections between peers |
| `listen` | Receive data from a peer (receiver mode)                      |
| `talk`   | Send data to a peer (sender mode)                             |


### Global Options

| Option            | Description           |
| ----------------- | --------------------- |
| `-h`, `--help`    | Print help message    |
| `-v`, `--version` | Print project version |


### Server Options (`serve`)

| Option                       | Description                            |
| ---------------------------- | -------------------------------------- |
| `-P <PORT>`, `--port <PORT>` | Specify the port to bind the server to |
| `--help`                     | Print help message for `serve`         |

Example:

```bash
p2pipe serve -P 9000
```


### Client Options (used by `listen` and `talk`)

| Option                           | Description                              |
| -------------------------------- | ---------------------------------------- |
| `-I <ADDRESS>`, `--ip <ADDRESS>` | Server IP address                        |
| `-P <PORT>`, `--port <PORT>`     | Server port                              |
| `-C <N>`, `--capacity <N>`       | Set the internal buffer capacity (bytes) |


### Receiver Options (`listen`)

| Option                      | Description                             |
| --------------------------- | --------------------------------------- |
| `-d <PATH>`, `--dst <PATH>` | Destination file to write received data |
| `-i <ID>`, `--id <ID>`      | Session identifier (for handshake)      |

Example:

```bash
p2pipe listen -I 127.0.0.1 -P 9000 -d output.bin -i session123
```


### Sender Options (`talk`)

| Option                      | Description                         |
| --------------------------- | ----------------------------------- |
| `-s <PATH>`, `--src <PATH>` | Source file to send to the receiver |

Example:

```bash
p2pipe talk -I 127.0.0.1 -P 9000 -s input.bin
```


## Protocol Summary

P2Pipe uses a **custom UDP-based reliable transport protocol** defined in [`docs/Protocol.md`](./docs/Specification.md).
It ensures reliability and order without TCP by using:

* **Handshake negotiation** (connection setup)
* **Sequenced packets** with unique `seq` IDs
* **Acknowledgments (ACKs)** for each received packet
* **Retransmissions** for unacknowledged data
* **Flow control** and buffer capacity awareness
* **End-of-stream signaling** (`SIGNAL_END` and `SIGNAL_TERMINATE`)

### Simplified Flow

```
[SENDER] <---> HANDSHAKE <---> [RECEIVER]
[SENDER] ---> DATA(seq=n) --> [RECEIVER]
[SENDER] <--- ACK(seq=n) --- [RECEIVER]
[SENDER] ---> END ----------> [RECEIVER]
```


## Example Session

### 1. Start the server

```bash
p2pipe serve -P 9000
```

### 2. Start the sender

```bash
p2pipe talk -I 127.0.0.1 -P 9000 -s message.txt
```
> The session id is being printed on stdout

### 3. Start the receiver

```bash
p2pipe listen -I 127.0.0.1 -P 9000 -d received.txt --id <session_id>
```

After transmission completes, `received.txt` will contain the same data as `message.txt`.


## Design Goals

* **Reliable UDP** with no TCP overhead
* **Low latency** via parallel sending and ACK threads
* **Session isolation** using unique handshake IDs
* **Extensible** protocol (custom signals and flow control parameters)


## License

This project is licensed under the **MIT License**.
See [LICENSE](./LICENSE) for details.
