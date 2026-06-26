# P2Pipe Library API

P2Pipe is a lightweight, multithreaded, peer-to-peer reliable data transfer
protocol built on UDP. It provides reliable delivery, sequencing, flow control,
and automatic retransmission without TCP.

## Building with the library

Compile and link against the static or shared library:

```text
gcc -Iinclude -L. -lp2pipe -pthread my_prog.c -o my_prog
# or directly against sources:
gcc -Iinclude -pthread src/p2pipe/*.c my_prog.c -o my_prog
```

Your application must define two global symbols the library expects:

```c
Metrics metrics;
FILE* log_file = NULL;
```

## Core types

### `Packet` (`p2pipe/packet.h`)

The wire-format data unit. Every send and receive revolves around packets.

```c
#define PACKET_BUFFER_SIZE 1024

typedef struct {
    uint8_t signals;       // bitwise OR of PacketSignal flags
    uint32_t seq;          // sequence number
    uint32_t len;          // payload length (bytes)
    uint32_t last_sent_ms; // timestamp of last send (for retransmission)
    uint8_t data[PACKET_BUFFER_SIZE]; // payload
} Packet;
```

Signal flags:

| Flag                | Meaning                    |
|---------------------|----------------------------|
| `SIGNAL_PAYLOAD`    | Contains user data         |
| `SIGNAL_ACK`        | Acknowledgement            |
| `SIGNAL_RESEND`     | Request retransmission     |
| `SIGNAL_END`        | Transfer complete          |
| `SIGNAL_TERMINATE`  | Abort transfer             |
| `SIGNAL_HANDSHAKE`  | Handshake message          |

Convenience constructors:

```c
Packet ack = PACKET_ACK(seq);      // SIGNAL_ACK with given seq
Packet end = PACKET_END;           // SIGNAL_END (len=0)
```

### `Pipe` (`p2pipe/pipe.h`)

The central object representing one side of a connection. All fields are
internal; zero-initialise before use.

```c
typedef struct {
    PipeMode mode;          // MODE_SND or MODE_RCV
    Buffer buffer;          // send buffer (pending unacked packets)
    int sock_fd;            // UDP socket
    struct sockaddr_in peer_addr; // resolved peer address
    Storage storage;        // receive-side ordered storage
    uint32_t seq;           // next sequence number to assign
    uint64_t hash;          // payload hash
    size_t payload_len;     // total payload length
    bool running;           // listener threads keep running
    bool retransmit_running;
    bool handshake_completed;
    bool end_received;
    // Internal synchronisation (mutexes, condvars)
    TPTaskFn onread;        // optional callback per received packet
    TPTaskFn onwrite;       // optional callback per sent packet
} Pipe;
```

### `Buffer` (`p2pipe/buffer.h`)

The sender-side retransmission buffer. Holds packets that have been sent but
not yet acknowledged.

```c
typedef struct {
    Packet* items;
    size_t count;
    size_t capacity;
} Buffer;

bool buffer_init(Buffer* buffer, size_t capacity);
bool buffer_append(Buffer* buffer, Packet packet);
bool buffer_remove(Buffer* buffer, uint32_t seq);
void buffer_free(Buffer* buffer);
```

### `Storage` (`p2pipe/storage.h`)

The receiver-side ordered storage. Delivered data is written to the output file
as in-order sequences arrive. Out-of-order packets are buffered until their
predecessors arrive.

```c
typedef struct {
    Packet* packets;
    size_t capacity;
    size_t count;
    bool ready;
    FILE* file_out;
    uint32_t next_expected_seq;
    bool stream_data;       // write to file as packets arrive
    pthread_mutex_t lock;
} Storage;

bool storage_init(Storage* storage, size_t capacity,
                  const char* dest_path, uint32_t initial_seq);
void storage_free(Storage* storage);
void storage_append(Storage* storage, const Packet* packet);
void storage_resize(Storage* storage, size_t capacity);
void storage_export(Storage* storage);
```

### `Handshake` (`p2pipe/handshake.h`)

Session parameters exchanged during connection setup.

```c
typedef struct {
    uint32_t buffer_cap;
} Handshake;
```

### `ThreadPool` (`p2pipe/threads.h`)

The internal thread pool (16 workers). Automatically initialised on first pipe
open; shut down at program exit with `threads_shutdown()`.

```c
ThreadPool *thread_pool_create(size_t num_threads);
bool thread_pool_submit(ThreadPool *p, TPTaskFn fn, void *arg);
void thread_pool_wait(ThreadPool *p);
void thread_pool_join(ThreadPool *p);
void thread_pool_wake_all(ThreadPool *p);
void thread_pool_shutdown(ThreadPool *p);
void thread_pool_destroy(ThreadPool *p);
void thread_pool_free(ThreadPool *p);
```

## Sender workflow

### 1. Open a sending pipe

```c
Pipe pipe = {0};
int fd = pipe_snd_open(&pipe, "127.0.0.1", 8080, 25, NULL);
if (fd <= 0) { /* error */ }
```

Parameters: `ip` (bootstrap server), `port`, `capacity` (buffer depth), `onwrite`
callback (nullable).

This initiates the handshake with the bootstrap server, opens a UDP socket, and
starts three packet listener threads plus three retransmission threads.

### 2. Write data

`pipe_write` splits the payload into `PACKET_BUFFER_SIZE` chunks, assigns
increasing sequence numbers, buffers each chunk, and submits send jobs to the
thread pool. Flow control is automatic: if the send buffer is full, `pipe_write`
blocks until ACKs arrive and free space.

```c
uint8_t data[] = "Hello, world!";
if (!pipe_write(&pipe, data, sizeof(data))) {
    /* error */
}
```

### 3. Flush and close

`pipe_flush` blocks until all buffered packets are acknowledged (with a 5-second
timeout). `pipe_snd_close` sends an END signal and releases resources.

```c
pipe_flush(&pipe);
pipe_snd_close(&pipe);
```

### Full sender example

```c
#include "p2pipe/pipe.h"

Metrics metrics;
FILE* log_file = NULL;

int main() {
    Pipe pipe = {0};
    if (pipe_snd_open(&pipe, "192.168.1.100", 8080, 50, NULL) <= 0)
        return 1;

    const char* msg = "Hello over UDP!";
    pipe_write(&pipe, (void*)msg, strlen(msg) + 1);

    pipe_snd_close(&pipe);
    threads_shutdown();
    return 0;
}
```

## Receiver workflow

### 1. Open a receiving pipe

```c
Pipe pipe = {0};
int fd = pipe_rcv_open(&pipe, "192.168.1.100", 8080, "session-id-abc", 25, NULL);
if (fd <= 0) { /* error */ }
```

Parameters: `ip` (bootstrap server), `port`, `id` (session ID from the server),
`capacity`, `onread` callback (nullable).

### 2. Initialise storage

```c
storage_init(&pipe.storage, 50, "output.bin", 0);
pipe.storage.stream_data = true;
```

This opens the output file and preparess to write in-order data as it arrives.
Set `stream_data = true` to enable immediate writing; without it, all packets
are buffered and must be exported manually with `storage_export`.

### 3. Read (block until complete)

`pipe_read` blocks until the END signal is received or the pipe is closed.

```c
if (!pipe_read(&pipe, PACKET_BUFFER_SIZE * pipe.buffer.capacity)) {
    /* error */
}
```

The `n_bytes` parameter is reserved for future chunked reads; currently the
function waits for the full transfer to finish.

### 4. Close

```c
pipe_rcv_close(&pipe);
```

### Full receiver example

```c
#include "p2pipe/pipe.h"
#include "p2pipe/storage.h"

Metrics metrics;
FILE* log_file = NULL;

int main() {
    Pipe pipe = {0};
    if (pipe_rcv_open(&pipe, "192.168.1.100", 8080, "abc123", 50, NULL) <= 0)
        return 1;

    storage_init(&pipe.storage, 50, "received.bin", 0);
    pipe.storage.stream_data = true;

    pipe_read(&pipe, 0);
    pipe_rcv_close(&pipe);
    threads_shutdown();
    return 0;
}
```

## Bootstrap server

The server coordinates sessions between senders and receivers.

```c
#include "p2pipe/bootstrap.h"
int server(int port);   // blocks forever
```

Usage in the CLI:

```text
./p2pipe serve -P 8080
```

As a library call:

```c
server(8080);  // never returns
```

Session lifecycle:

1. Sender connects with `TYPE=SND`, receives an `ID=<base56> WAIT`.
2. Receiver connects with `TYPE=RCV ID=<base56>`, receives sender's address.
3. Both peers are notified of each other's address and begin direct P2P
   communication. The session is removed from the server.

Stale sessions (no activity for 60 seconds) are automatically cleaned up.

## Optional callbacks

### `onread` callback

Called for every received data packet, **after** it has been appended to
storage and before the packet/RecvJob is freed. Set during `pipe_rcv_open`.

```c
void my_onread(void* arg) {
    RecvJob* job = (RecvJob*)arg;
    printf("Got packet #%u (%u bytes)\n", job->packet->seq, job->packet->len);
}

Pipe pipe = {0};
pipe_rcv_open(&pipe, "127.0.0.1", 8080, "id", 25, my_onread);
```

### `onwrite` callback

Called for every packet **after** successful send and before the SendJob is
freed. Set during `pipe_snd_open`.

```c
void my_onwrite(void* arg) {
    SendJob* job = (SendJob*)arg;
    printf("Sent packet #%u\n", job->packet.seq);
}

Pipe pipe = {0};
pipe_snd_open(&pipe, "127.0.0.1", 8080, 25, my_onwrite);
```

**Warning:** The job pointer is freed immediately after the callback returns.
Do not store it.

## Low-level packet I/O

You can send and receive raw `Packet` structs directly, bypassing the write
buffer and flow control.

```c
// Synchronous (blocking send):
Packet custom = { .signals = SIGNAL_PAYLOAD, .seq = 42 };
memcpy(custom.data, payload, len);
custom.len = len;
pipe_write_packet_sync(&pipe, &custom, NULL);

// Asynchronous (queued to thread pool):
pipe_write_packet_async(&pipe, &custom, NULL);
```

Pass a `struct sockaddr_in*` as the third argument to send to a specific
address instead of the default peer.

## Serialisation

```c
uint8_t wire[sizeof(Packet)];
size_t n = packet_serialize(&packet, wire, sizeof(wire));
// send wire[0..n] over UDP

Packet decoded;
packet_deserialize(&decoded, wire, n);
```

Uses big-endian wire format with a 9-byte header (signals, seq, len) followed
by the payload.

## Metrics

The library records transfer statistics to a global `Metrics` struct.

```c
#include "p2pipe/metrics.h"

Metrics metrics;  // must be defined globally

metrics_init(&metrics, "metrics.csv");
metrics_start(&metrics);   // set start timestamp
metrics_end(&metrics);     // set end timestamp
metrics_write(&metrics, "metrics.csv");  // append CSV row
metrics_free(&metrics);    // free id string
```

The CSV columns are:

```text
id, packets_sent, packets_received, packets_lost, packets_duplicate,
packets_discarded, acks_sent, acks_received, acks_lost, retransmits,
sender_paused, start, end, buffer_capacity, payload_len, type
```

## Logging

```c
#include "extern/logging.h"  // must be included FIRST
#include "p2pipe/log.h"

logging_set_file();  // creates logs/<timestamp>.log
```

Log macros (all write to both stderr/stdout and the log file):

| Macro                          | Output            |
|--------------------------------|-------------------|
| `INFO(format, ...)`            | stdout + log file |
| `ERRO(format, ...)`            | stderr + log file |
| `WARN(format, ...)`            | stderr + log file |
| `DEBU(format, ...)`            | stderr + log file (debug builds only) |
| `PANIC(format, ...)`           | stderr + log file + `exit(1)` |

## Utility functions

```c
// Timestamps
long long current_time_ms();
uint64_t  current_time_us();

// FNV-1a hash (64-bit)
uint64_t compute_fnv1a_hash(const void* payload, size_t len);

// Session ID generation (base-56, 16 chars)
#include "p2pipe/id.h"
char id[17];
srand(time(NULL));           // caller must seed rand()
generate_base56_id(id, 16); // id now holds "Ab3Xy9..."

// Thread pool lifecycle
bool threads_init(void);     // idempotent, called automatically
void threads_shutdown(void); // joins and destroys pool (call at exit)
```

## Compile-time configuration

| Define              | Effect                                       |
|---------------------|----------------------------------------------|
| `-DMETRICS_ENABLED` | Enable metrics increment/set macros          |
| `-DDEBUG`           | Enable debug log output and file+console log |
