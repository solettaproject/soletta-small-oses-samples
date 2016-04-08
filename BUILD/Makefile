WORKING_TOPDIR := $(PWD)
MAKEFILE_TOPDIR := $(dir $(CURDIR)/$(lastword $(MAKEFILE_LIST)))

TARGETS_DIR := $(MAKEFILE_TOPDIR)targets
APPLICATION_SOURCES_DIR := $(WORKING_TOPDIR)/src

AVAILABLE_TARGETS := $(sort $(patsubst $(TARGETS_DIR)/Makefile.%,%,$(wildcard $(TARGETS_DIR)/Makefile.*)))

TARGET = $(filter $(AVAILABLE_TARGETS),$(MAKECMDGOALS))
ifeq (,$(TARGET))
all:
	@echo "Please run with one of the available targets: $(AVAILABLE_TARGETS)"
else
PASSTHROUGH_TARGETS = $(filter-out $(AVAILABLE_TARGETS),$(MAKECMDGOALS))
BUILD_DIR ?= $(WORKING_TOPDIR)/$(TARGET)_stage
TARGET_DIR := $(TARGETS_DIR)/$(TARGET)

include $(WORKING_TOPDIR)/Makefile.application
include $(TARGETS_DIR)/Makefile.$(TARGET)

ifneq (,$(LOG_LEVEL))
export LOGGING_LEVEL = -DSOL_LOG_LEVEL=\"$(LOG_LEVEL)\"
endif

ifneq (,$(MACHINE_ID))
export MACHINE_IDENTIFICATION = -DSOL_MACHINE_ID=\\\"$(MACHINE_ID)\\\"
$(info setting machine id to $(MACHINE_IDENTIFICATION))
endif

APP_SOURCES := $(addprefix $(WORKING_TOPDIR)/src/, $(C_SOURCES) $(FBP_SOURCES))

TARGET_CONFIG := $(realpath $(WORKING_TOPDIR)/config.$(TARGET))
ifneq (,$(TARGET_CONFIG))
copy_config_target = copy_config
copy_config: prepare
	@cp $(TARGET_CONFIG)/* $(BUILD_DIR)
endif

prepare:
	@mkdir -p $(SOURCES_DIR)
	@cp $(TARGET_DIR)/* $(BUILD_DIR)/
	@cp $(APP_SOURCES) $(SOURCES_DIR)/

ifeq ("yes",$(vars_missing))
$(TARGET): missing_env_vars
	@exit 1
else
$(TARGET): prepare $(copy_config_target)
	$(MAKE) -C $(BUILD_DIR) $(PASSTHROUGH_TARGETS) MAKEFILE_TOPDIR=$(MAKEFILE_TOPDIR) WORKING_TOPDIR=$(WORKING_TOPDIR) BUILD_DIR=$(BUILD_DIR) FBP_SOURCES="$(FBP_SOURCES)"
endif

$(PASSTHROUGH_TARGETS):
endif
