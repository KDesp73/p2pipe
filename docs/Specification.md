# P2Pipe Protocol Specification

<!--toc:start-->
- [Overview](#overview)
- [1. Protocol Roles](#1-protocol-roles)
- [2. Connection Establishment](#2-connection-establishment)
  - [2.1 Handshake](#21-handshake)
  - [2.2 Handshake Flow](#22-handshake-flow)
- [3. Packet Structure](#3-packet-structure)
- [4. Reliable Delivery](#4-reliable-delivery)
  - [4.1 ACK Mechanism](#41-ack-mechanism)
  - [4.2 Retransmission](#42-retransmission)
  - [4.3 Buffer Management](#43-buffer-management)
- [5. Flow Control](#5-flow-control)
- [6. Threading Model](#6-threading-model)
- [7. Packet Serialization](#7-packet-serialization)
- [8. Error Handling](#8-error-handling)
- [9. Metrics](#9-metrics)
- [10. Summary Diagram](#10-summary-diagram)
<!--toc:end-->

## Overview

P2Pipe is a custom UDP-based peer-to-peer reliable data transmission protocol.  
It allows sending and receiving arbitrary payloads between peers with:

- Reliable delivery via ACKs and retransmissions
- Packet sequencing
- Handshake negotiation
- End-of-stream signaling
- Multithreaded operation

---

## 1. Protocol Roles

P2Pipe defines two modes:

| Role       | Description |
|------------|-------------|
| **Sender*- (`MODE_SND`) | Initiates connection, sends data, manages retransmissions, waits for ACKs. |
| **Receiver*- (`MODE_RCV`) | Receives data, acknowledges packets, maintains storage buffer for application reads. |

---

## 2. Connection Establishment

### 2.1 Handshake

Before sending data, peers exchange a handshake:

**Handshake Structure**

| Field          | Type   | Size  | Description |
|----------------|--------|-------|-------------|
| `buffer_cap`   | `uint32_t` | 4 B | Receiver buffer capacity |
| `payload_len`  | `uint32_t` | 4 B | Total payload length (sender only) |
| `hash`         | `uint64_t` | 8 B | FNV-1a hash of payload (sender only) |

**Serialization**

- Fields are serialized in order: `buffer_cap` → `payload_len` → `hash`
- Total handshake packet size: 16 bytes

### 2.2 Handshake Flow

1. Sender/Receiver creates a UDP socket on an ephemeral port.
2. Client sends `HANDSHAKE` message:  

```
HANDSHAKE VERSION=<VERSION> TYPE=SND|RCV
```
3. Server may reply with:
- `WAIT`: client waits for a peer to connect
- `PEER <ip> <port> <extra_info>`: provides peer IP, port, and metadata
4. Sender sends handshake packet as a `SIGNAL_HANDSHAKE | SIGNAL_PAYLOAD` packet.

---

## 3. Packet Structure

Packets carry both control signals and payload data.

| Field            | Type       | Size          | Description |
|------------------|------------|---------------|-------------|
| `signals`        | `uint8_t`  | 1 B           | Bitmask of PacketSignal flags |
| `seq`            | `uint32_t` | 4 B           | Packet sequence number |
| `len`            | `uint32_t` | 4 B           | Length of `data` payload |
| `data`           | `uint8_t[]`| 1024 B max    | Payload data |

**Packet Signals**

| Signal                 | Value | Description |
|------------------------|-------|-------------|
| `SIGNAL_PAYLOAD`       | 1 << 0 | Data packet |
| `SIGNAL_ACK`           | 1 << 1 | Acknowledgment of a packet |
| `SIGNAL_RESEND`        | 1 << 2 | Request retransmission (currently unused) |
| `SIGNAL_END`           | 1 << 3 | Marks end of transmission |
| `SIGNAL_TERMINATE`     | 1 << 4 | Abort transmission |
| `SIGNAL_HANDSHAKE`     | 1 << 5 | Handshake packet |

---

## 4. Reliable Delivery

### 4.1 ACK Mechanism

- Every received payload packet triggers an ACK.
- ACK packets carry the sequence number of the acknowledged packet:

  ```c
  Packet ack = PACKET_ACK(received_seq);
  ```
- Sender removes packets from buffer when ACKs arrive.

### 4.2 Retransmission

- Sender maintains a buffer of unacknowledged packets.
- Retransmission thread periodically checks for packets exceeding `RETRANSMISSION_TIMEOUT_MS = 500ms` and resends them.
- Multiple retransmissions continue until packet is acknowledged or connection closes.

### 4.3 Buffer Management

- Sender blocks if buffer is full until ACKs free space.
- Receiver maintains a storage buffer for application reads.

---

## 5. Flow Control

- Sender waits for handshake to complete before sending data.
- Data is sent in `PACKET_BUFFER_SIZE = 1024` chunks.
- `pipe_flush()` ensures all buffered packets are acknowledged before closing.

---

## 6. Threading Model

| Thread Type           | Purpose                                              |
| --------------------- | ---------------------------------------------------- |
| Packet Listener       | Receives UDP packets and dispatches them             |
| Retransmission Thread | Periodically retransmits unacknowledged packets      |
| Thread Pool Workers   | Handles sending ACKs and asynchronous packet sending |

---

## 7. Packet Serialization

- All packet fields are serialized in network byte order where applicable.
- Payload is copied verbatim up to 1024 bytes per packet.
- Sequence numbers allow reordering detection.

---

## 8. Error Handling

- Socket errors are logged and may terminate the pipe.
- Retransmission continues for timed-out packets.
- Duplicate or stale ACKs are ignored.
- Handshake failures abort connection setup.

---

## 9. Metrics

- `metrics.packets_sent` – total packets sent
- `metrics.packets_received` – total packets received
- `metrics.packets_acked` – total packets acknowledged

---

## 10. Summary Diagram

```
[SENDER] ---> HANDSHAKE ---> [RECEIVER]
   |                             |
   | <--- PEER info / WAIT ---   |
   |                             |
[SENDER] ---> DATA PACKET ---> [RECEIVER]
[SENDER] <--- ACK ------------ [RECEIVER]
[SENDER] ---> END -----------> [RECEIVER]
```
