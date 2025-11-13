# Compiler and flags
CC = gcc
CFLAGS = -Wall -Iinclude -fPIC -Werror -pthread
LDFLAGS =

# Directories
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
DIST_DIR = dist

LIBRARY_NAME = p2pipe
SO_NAME = lib$(LIBRARY_NAME).so
A_NAME = lib$(LIBRARY_NAME).a

# Target and version info
TARGET = p2pipe
version_file = include/version.h
VERSION_MAJOR = $(shell sed -n -e 's/\#define VERSION_MAJOR \([0-9]*\)/\1/p' $(version_file))
VERSION_MINOR = $(shell sed -n -e 's/\#define VERSION_MINOR \([0-9]*\)/\1/p' $(version_file))
VERSION_PATCH = $(shell sed -n -e 's/\#define VERSION_PATCH \([0-9]*\)/\1/p' $(version_file))
VERSION = $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)

DEFINES = 
SANITIZERS = -fsanitize=address,undefined -fno-omit-frame-pointer -fno-optimize-sibling-calls

# Determine the build type
ifeq ($(type), RELEASE)
	CFLAGS += -O3
else
	DEFINES += -DMETRICS_ENABLED -DDEBUG
	CFLAGS  += $(DEFINES) -ggdb 
	CFLAGS  += -Wall -Wextra -Wno-unused-parameter -Wno-unused-function
	CFLAGS  += $(SANITIZERS)
	LDFLAGS += $(SANITIZERS)
endif

# Source and object files
SRC_LIB_FILES := $(shell find $(SRC_DIR)/p2pipe -name '*.c')
SRC_FILES := $(shell find $(SRC_DIR) -name '*.c')
OBJ_LIB_FILES = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC_LIB_FILES))
OBJ_FILES = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC_FILES))

# Default target
.DEFAULT_GOAL := help

# Total source file count
TOTAL_FILES := $(words $(SRC_LIB_FILES))

# Counter to track progress
counter = 0

# Targets

.PHONY: all
all: check_tools $(BUILD_DIR) static shared $(TARGET)## Build the project
	@echo "Build complete."

.PHONY: check_tools
check_tools: ## Check if necessary tools are available
	@command -v gcc >/dev/null 2>&1 || { echo >&2 "[ERRO] gcc is not installed."; exit 1; }
	@command -v bear >/dev/null 2>&1 || { echo >&2 "[WARN] bear is not installed. Skipping compile_commands.json target."; }

$(BUILD_DIR): ## Create the build directory if it doesn't exist
	@echo "[INFO] Creating build directory"
	mkdir -p $(BUILD_DIR)/p2pipe

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c ## Compile source files with progress
	$(eval counter=$(shell echo $$(($(counter)+1))))
	@echo "[$(counter)/$(TOTAL_FILES)] Compiling $< -> $@"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(BUILD_DIR) static ## Build executable using static library
	@echo "[INFO] Building executable: $(TARGET)"
	@$(CC) $(SRC_FILES) -o $(TARGET) -L. -l:$(A_NAME) $(LDFLAGS) -Iinclude $(DEFINES)

.PHONY: shared
shared: $(BUILD_DIR) $(OBJ_LIB_FILES) ## Build shared library
	@echo "[INFO] Building shared library: $(SO_NAME)"
	@$(CC) -shared $(CFLAGS) -o $(SO_NAME) $(OBJ_LIB_FILES)

.PHONY: static
static: $(BUILD_DIR) $(OBJ_LIB_FILES) ## Build static library
	@echo "[INFO] Building static library: $(A_NAME)"
	@$(AR) rcs $(A_NAME) $(OBJ_LIB_FILES)

.PHONY: clean
clean: ## Remove all build files and the executable
	@echo "[INFO] Cleaning up build directory and executable."
	rm -rf $(BUILD_DIR) $(TARGET) $(SO_NAME) $(A_NAME) sender receiver

.PHONY: distclean
distclean: clean ## Perform a full clean, including backup and temporary files
	@echo "[INFO] Performing full clean, removing build directory, dist files, and editor backups."
	rm -f *~ core $(SRC_DIR)/*~ $(DIST_DIR)/*.tar.gz

.PHONY: dist
dist: $(SRC_FILES) ## Create a tarball of the project
	@echo "[INFO] Creating a tarball for version $(VERSION)"
	mkdir -p $(DIST_DIR)
	tar -czvf $(DIST_DIR)/$(TARGET)-$(VERSION).tar.gz $(SRC_DIR) $(INCLUDE_DIR) Makefile README.md

.PHONY: install
install: all ## Installs p2pipe system-wide
	@echo "[INFO] Installing p2pipe..."
	cp p2pipe /usr/local/bin
	cp libp2pipe.* /usr/lib
	cp -r include/p2pipe /usr/local/include/
	cp docs/autocomplete/p2pipe.zsh /usr/share/zsh/functions/Completion/_p2pipe
	# TODO: Install other scripts


## Generate compile_commands.json
.PHONY: compile_commands.json
compile_commands.json: $(SRC_FILES) ## Generate compile_commands.json
	@echo "[INFO] Generating compile_commands.json"
	bear -- make all

## Show this help message
.PHONY: help
help: ## Show this help message
	@echo "Available commands:"
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-20s\033[0m %s\n", $$1, $$2}'

## Enable verbose output for debugging
.PHONY: verbose
verbose: CFLAGS += -DVERBOSE
verbose: all ## Build the project in verbose mode

.PHONY: autocomplete
autocomplete: ## Generate autocomplete scripts for bash, zsh and fish
	complgen  --zsh ./docs/autocomplete/p2pipe.zsh  ./docs/autocomplete/p2pipe.usage
	complgen --bash ./docs/autocomplete/p2pipe.bash ./docs/autocomplete/p2pipe.usage
	complgen --fish ./docs/autocomplete/p2pipe.fish ./docs/autocomplete/p2pipe.usage


METRICS_DEFINES = -DMETRICS_ENABLED -DDEBUG
.PHONY: metrics 
metrics: metrics/sender.c metrics/receiver.c libp2pipe.a
	$(CC) metrics/sender.c -o sender -Iinclude $(SANITIZERS) $(METRICS_DEFINES) $(LDFLAGS) -L. -lp2pipe -pthread
	$(CC) metrics/receiver.c -o receiver -Iinclude $(SANITIZERS) $(METRICS_DEFINES) $(LDFLAGS) -L. -lp2pipe -pthread
