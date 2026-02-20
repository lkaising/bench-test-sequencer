# ============================================================================
# Makefile — bench_test_sequencer
# NIR Vein-Mapping System — Bench Test LED Sequencer
#
# Standard targets: setup, list, build, upload, monitor, clean
# Configuration override: command-line > config/local.mk > defaults below
# ============================================================================

# --- Per-machine overrides (gitignored) ---
-include config/local.mk

# --- Defaults ---
FQBN            ?= arduino:avr:uno
PORT            ?= /dev/ttyACM0
BAUD            ?= 115200
AVR_CORE_VERSION ?= 1.8.6
SKETCH          ?= bench_test_sequencer.ino
CONFIG          ?= config/arduino-cli.yaml
BUILD_DIR       ?= build

# --- Tool ---
CLI             ?= arduino-cli

# ============================================================================
# Targets
# ============================================================================

.PHONY: setup list build upload monitor clean help

## setup : Install/verify pinned AVR core
setup:
	$(CLI) core install arduino:avr@$(AVR_CORE_VERSION) --config-file $(CONFIG)
	@echo "✓ AVR core $(AVR_CORE_VERSION) installed"

## list : Detect connected boards and ports
list:
	$(CLI) board list

## build : Compile sketch to build/
build:
	$(CLI) compile --fqbn $(FQBN) --build-path $(BUILD_DIR) \
		--config-file $(CONFIG) .
	@echo "✓ Build complete → $(BUILD_DIR)/"

## upload : Compile + flash to board
upload:
	$(CLI) compile --fqbn $(FQBN) --build-path $(BUILD_DIR) \
		--config-file $(CONFIG) .
	$(CLI) upload --fqbn $(FQBN) --port $(PORT) \
		--input-dir $(BUILD_DIR)
	@echo "✓ Uploaded to $(PORT)"

## monitor : Open serial monitor
monitor:
	$(CLI) monitor --port $(PORT) --config baudrate=$(BAUD)

## clean : Remove build artifacts
clean:
	rm -rf $(BUILD_DIR)
	@echo "✓ Clean"

## help : Show available targets
help:
	@echo "bench_test_sequencer — Bench Test LED Sequencer"
	@echo ""
	@echo "Targets:"
	@echo "  make setup    Install/verify AVR core (pinned $(AVR_CORE_VERSION))"
	@echo "  make list     Detect connected boards and ports"
	@echo "  make build    Compile sketch to $(BUILD_DIR)/"
	@echo "  make upload   Compile + flash to board (PORT=$(PORT))"
	@echo "  make monitor  Open serial monitor ($(BAUD) baud)"
	@echo "  make clean    Remove build artifacts"
	@echo ""
	@echo "Override examples:"
	@echo "  make upload PORT=/dev/ttyACM1"
	@echo "  make build FQBN=arduino:avr:mega:cpu=atmega2560"
