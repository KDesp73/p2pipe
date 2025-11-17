# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).


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

- Written Specification.md
- Completed metrics setup
- Added optional onread/onwrite handlers
- Added metrics.py script
- Using session id
- Logging to file

### Changed

- Using VERSION= instead of PROTO=
- Bumped version


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

- Sending / Receiving acks
- Flush
- Multithreaded operations

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


[0.1.0]: https://github.com/KDesp73/p2pipe/releases/tag/v0.1.0
[0.1.1]: https://github.com/KDesp73/p2pipe/releases/tag/v0.1.1
[0.2.0]: https://github.com/KDesp73/p2pipe/releases/tag/v0.2.0
[0.3.0]: https://github.com/KDesp73/p2pipe/releases/tag/v0.3.0
[1.0.0]: https://github.com/KDesp73/p2pipe/releases/tag/v1.0.0
[1.1.0]: https://github.com/KDesp73/p2pipe/releases/tag/v1.1.0

