APPLICATION ?= $(notdir $(WORKING_TOPDIR))
BOARD ?= native

USEMODULE += gnrc_netdev_default
USEMODULE += auto_init_gnrc_netif
USEMODULE += gnrc_ipv6_default
USEMODULE += gnrc_udp

USEPKG += libsoletta

SOLETTA_CONF = $(realpath $(CURDIR)/sol_$(BOARD).conf)
ifeq (,$(SOLETTA_CONF))
SOLETTA_CONF = $(realpath $(CURDIR)/sol.conf)
endif
export SOLETTA_CONF

FLOW_CONFIG = $(realpath $(CURDIR)/sol-flow_$(BOARD).json)
ifeq (,$(FLOW_CONFIG))
FLOW_CONFIG = $(realpath $(CURDIR)/sol-flow.json)
endif
export FLOW_CONFIG

ifdef MAIN_STACK_SIZE
CFLAGS += -DTHREAD_STACKSIZE_MAIN=$(MAIN_STACK_SIZE)
endif

CFLAGS += -DDEVELHELP

QUIET ?= 1

include $(RIOTBASE)/Makefile.include

