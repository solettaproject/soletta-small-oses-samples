KERNEL_TYPE ?= nano
BOARD ?= qemu_x86
MDEF_FILE := prj.mdef

ifneq (,$(realpath $(BUILD_DIR)/prj_$(BOARD).conf))
CONF_FILE := prj_$(BOARD).conf
else
ifneq (,$(realpath $(BUILD_DIR)/prj.conf))
CONF_FILE := prj.conf
endif
endif

SOLETTA_CONF = $(realpath $(BUILD_DIR)/sol_$(BOARD).conf)
ifeq (,$(SOLETTA_CONF))
SOLETTA_CONF = $(realpath $(BUILD_DIR)/sol.conf)
endif
export SOLETTA_CONF

FLOW_CONFIG = $(realpath $(BUILD_DIR)/sol-flow_$(BOARD).json)
ifeq (,$(FLOW_CONFIG))
FLOW_CONFIG = $(realpath $(BUILD_DIR)/sol-flow.json)
endif
export FLOW_CONFIG

include $(ZEPHYR_BASE)/Makefile.inc

