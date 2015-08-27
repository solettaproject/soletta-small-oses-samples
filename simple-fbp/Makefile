APPLICATION = simple_fbp
BOARD ?= samr21-xpro
RIOTBASE ?= $(CURDIR)/../../RIOT

USEMODULE += ng_netif_default
USEMODULE += auto_init_ng_netif

USEPKG += libsoletta
CFLAGS += -D__LINUX_ERRNO_EXTENSIONS__=1

CFLAGS += -DTHREAD_STACKSIZE_MAIN=3072

QUIET ?= 1

include $(RIOTBASE)/Makefile.include
