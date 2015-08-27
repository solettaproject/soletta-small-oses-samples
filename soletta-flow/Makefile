APPLICATION = soletta_flow
BOARD ?= samr21-xpro
RIOTBASE ?= $(CURDIR)/../../RIOT

USEMODULE += ng_netif_default
USEMODULE += auto_init_ng_netif
USEMODULE += ng_ipv6_default

USEPKG += libsoletta
CFLAGS += -D__LINUX_ERRNO_EXTENSIONS__=1

CFLAGS += -DDEVELHELP -DTHREAD_STACKSIZE_MAIN=3072

QUIET ?= 1

include $(RIOTBASE)/Makefile.include
