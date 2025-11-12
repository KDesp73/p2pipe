# P2Pipe Communication Protocol Specification

<!--toc:start-->
- [1. Overview](#1-overview)
- [2. Roles and Modes](#2-roles-and-modes)
- [3. Connection Establishment](#3-connection-establishment)
  - [3.1 Handshake Initiation](#31-handshake-initiation)
    - [Handshake Packet Format](#handshake-packet-format)
    - [Example Flow](#example-flow)
- [4. Packet Structure](#4-packet-structure)
  - [4.1 Common Header](#41-common-header)
  - [4.2 Signal Flags](#42-signal-flags)
- [5. Reliability Layer](#5-reliability-layer)
  - [5.1 Acknowledgment Model](#51-acknowledgment-model)
  - [5.2 Retransmission](#52-retransmission)
- [6. Flow Control](#6-flow-control)
- [7. Serialization Rules](#7-serialization-rules)
- [8. Error Handling and Recovery](#8-error-handling-and-recovery)
- [9. Example Communication Flow](#9-example-communication-flow)
<!--toc:end-->

## 1. Overview

The **P2Pipe Communication Protocol** defines a reliable, UDP-based, peer-to-peer data exchange mechanism.
It is designed to provide **connection-oriented reliability** over an inherently unreliable transport, using:

* Connection negotiation via handshake
* Sequenced data transmission
* Acknowledgment (ACK)-based reliability
* Retransmission and flow control
* Explicit termination signaling

This document specifies the structure, message types, and operational semantics of the P2Pipe communication protocol.

---

## 2. Roles and Modes

Each peer operates in one of two roles during a session:

| Role         | Identifier | Description                                                                                                              |
| ------------ | ---------- | ------------------------------------------------------------------------------------------------------------------------ |
| **Sender**   | `MODE_SND` | Initiates connection and transmits data packets. Responsible for maintaining retransmission state and interpreting ACKs. |
| **Receiver** | `MODE_RCV` | Waits for incoming connections, acknowledges received packets, and buffers data for application consumption.             |

---

## 3. Connection Establishment

### 3.1 Handshake Initiation

A peer must complete a **handshake exchange** before any data transmission occurs.
This establishes mutual configuration and readiness between sender and receiver.

#### Handshake Packet Format

| Field         | Type       | Size | Description                           |
| ------------- | ---------- | ---- | ------------------------------------- |
| `buffer_cap`  | `uint32_t` | 4 B  | Receiver’s advertised buffer capacity |

All fields are serialized in network byte order.

#### Example Flow

```
Sender -> "HANDSHAKE VERSION=<v> TYPE=SND" # Server provides a session id
Receiver -> "HANDSHAKE VERSION=<v> TYPE=RCV ID=<SESSION_ID>"

# Both peers receive the following from the server
PEER <IP> <PORT> VERSION=<v> TYPE=SND|RCV ID=<SESSION_ID>
```

Upon successful handshake acknowledgment, both peers transition to the **connected** state.

---

## 4. Packet Structure

### 4.1 Common Header

All protocol packets follow a shared binary header layout:

| Field     | Type        | Size    | Description                              |
| --------- | ----------- | ------- | ---------------------------------------- |
| `signals` | `uint8_t`   | 1 B     | Bitmask of control flags (see below)     |
| `seq`     | `uint32_t`  | 4 B     | Sequence number (monotonic, per session) |
| `len`     | `uint32_t`  | 4 B     | Length of `data` field                   |
| `data`    | `uint8_t[]` | ≤1024 B | Payload data or control content          |

### 4.2 Signal Flags

| Flag               | Bit      | Description                                        |
| ------------------ | -------- | -------------------------------------------------- |
| `SIGNAL_PAYLOAD`   | `1 << 0` | Standard data packet                               |
| `SIGNAL_ACK`       | `1 << 1` | Acknowledges receipt of a specific sequence number |
| `SIGNAL_RESEND`    | `1 << 2` | Requests retransmission (reserved for future use)  |
| `SIGNAL_END`       | `1 << 3` | End of stream — all data sent                      |
| `SIGNAL_TERMINATE` | `1 << 4` | Connection terminated unexpectedly                 |
| `SIGNAL_HANDSHAKE` | `1 << 5` | Marks handshake payload                            |

---

## 5. Reliability Layer

### 5.1 Acknowledgment Model

* Every data packet (`SIGNAL_PAYLOAD`) must trigger an acknowledgment.
* ACK packets contain the **sequence number** of the received packet:

```
signals = SIGNAL_ACK
seq = <acknowledged_sequence>
```

* The sender frees corresponding buffered packets upon receipt of an ACK.
* Duplicate or stale ACKs are ignored.

### 5.2 Retransmission

* Unacknowledged packets are retained in a retransmission buffer.
* Packets older than `RETRANSMISSION_TIMEOUT_MS` (default: 500 ms) are resent.
* Retransmissions continue until acknowledgment or session termination.
* The retransmission window is dynamically managed based on buffer capacity and congestion feedback.

---

## 6. Flow Control

To prevent buffer exhaustion and excessive retransmissions:

* The **sender** halts new transmissions when the receiver’s advertised buffer is full.
* The **receiver** processes packets sequentially and maintains an application buffer.
* The handshake defines both `buffer_cap` and `payload_len` — these values must be respected throughout the session.
* `END` and `TERMINATE` signals mark logical and abrupt session closures respectively.

---

## 7. Serialization Rules

* Multi-byte integers are encoded in **network byte order (big-endian)**.
* The `data` section is binary-safe and unmodified (no compression or framing).
* Packet size must not exceed `HEADER_SIZE + payload_len`.
* Sequence numbers start at 0 and increment monotonically.

---

## 8. Error Handling and Recovery

* Invalid or malformed packets are discarded silently.
* Repeated handshake failures abort the session.
* Timeouts on both sides trigger retransmission or termination depending on role.
* After a `SIGNAL_TERMINATE` message, peers must close sockets and release buffers immediately.

---

## 9. Example Communication Flow

```
   [Sender]                              [Receiver]
       |                                       |
       |--- HANDSHAKE (TYPE=SND, ID=abc) ----->|
       |<--------- WAIT / PEER ----------------|
       |--- [SIGNAL_HANDSHAKE + payload] ----->|
       |<----------- ACK(seq=0) ---------------|
       |--- DATA(seq=1, len=1024) ------------>|
       |<----------- ACK(seq=1) ---------------|
       |--- END(seq=N) ----------------------->|
       |<----------- ACK(seq=N) ---------------|
       |--- TERMINATE ------------------------>|
       |<--- ACK(seq=TERMINATE) ---------------|
```
