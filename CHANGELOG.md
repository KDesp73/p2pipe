# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).


## [1.2.0] - 2026-06-26 

### Fixed

- Replace busy-wait spinlock in pipe_write with proper pthread_cond_wait on new handshake_cond
- Bound retransmission loop to buffer.count instead of buffer.capacity to avoid reading uninitialized packet slots
- Fix negative capacity validation (&& -> ||) to prevent SIZE_MAX allocation
- Add mutex lock to storage_export and change signature from `const Storage*` to `Storage*`
- Fix use-after-free/double-free in pipe_write_packet_async and packet_listener
- Replace non-portable Linux-internal headers (bits/getopt_core.h, bits/pthreadtypes.h, asm-generic/errno.h) with standard equivalents
- Fix pipe_read to actually wait for end_received; add proper pipe_rcv_close wait using condition variable instead of usleep
- Replace inet_ntoa with inet_ntop for thread safety
- Add session timeout cleanup (60s idle) to bootstrap server
- Add NULL checks for all strdup calls in main.c
- Fix help text displaying "p2p" instead of "p2pipe"
- Add missing #include <time.h> in log.c
- Replace thread_pool_wait sched_yield busy-loop with condition variable (add signal in worker_main on task completion)
- Add bounds checking to cli_generate_format_string and increase format buffer from 128 to 1024


## [1.1.0] - 2025-11-17 

### Added

- Metrics-focused executables
- plot.py script
- organize.py script
- LICENSE

### Changed

- Bumped version

### Fixed

- Thread-safe storage
- Help messages


## [1.0.0] - 2025-11-12 

### Added

- Using session id
- Written Specification.md
- Completed metrics setup
- Added optional onread/onwrite handlers
- Added metrics.py script
- Logging to file

### Changed

- Bumped version
- Using VERSION= instead of PROTO=


## [0.3.0] - 2025-11-06 

### Added

- Seperate handshake packet
- Multiple thread handlers for each operation
- Streaming file if over 100MB

### Changed

- Writing file gradually
- Bumped version

### Fixed

- Shutting down all threads and exiting normally
- Resolved `packet_listener` crash
- Autocomplete scripts


## [0.2.0] - 2025-11-04 

### Added

- Multithreaded operations
- Sending / Receiving acks
- Flush

### Changed

- Storage in each pipe
- Bumped version


## [0.1.1] - 2025-11-03 

### Added

- Autocomplete scripts
- Identifing sessions
- Implemented storage

### Changed

- Renamed thread_pool_t to match established naming convention
- Bumped version


## [0.1.0] - 2025-11-02 

### Added

- Cli interface
- Bootstrap server
- Threading framework
- Sending and receiving payloads in packets


[0.1.0]: https://github.com/KDesp73/p2pipe//releases/tag/v0.1.0
[0.1.1]: https://github.com/KDesp73/p2pipe//releases/tag/v0.1.1
[0.2.0]: https://github.com/KDesp73/p2pipe//releases/tag/v0.2.0
[0.3.0]: https://github.com/KDesp73/p2pipe//releases/tag/v0.3.0
[1.0.0]: https://github.com/KDesp73/p2pipe//releases/tag/v1.0.0
[1.1.0]: https://github.com/KDesp73/p2pipe//releases/tag/v1.1.0
[1.2.0]: https://github.com/KDesp73/p2pipe//releases/tag/v1.2.0

