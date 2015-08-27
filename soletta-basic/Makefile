APPLICATION = soletta_basic
BOARD ?= samr21-xpro
RIOTBASE ?= $(CURDIR)/../../RIOT

USEMODULE += ng_netif_default
USEMODULE += auto_init_ng_netif
USEMODULE += ng_ipv6_default
USEMODULE += ng_udp

USEMODULE += uart0
USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += ps

USEPKG += libsoletta
# soletta deps?
USEMODULE += posix
CFLAGS += -D__LINUX_ERRNO_EXTENSIONS__=1

CFLAGS += -DDEVELHELP

QUIET ?= 1

include $(RIOTBASE)/Makefile.include
