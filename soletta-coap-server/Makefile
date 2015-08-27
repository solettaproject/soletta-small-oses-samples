APPLICATION = soletta_coap_server
BOARD ?= samr21-xpro
SERIAL ?= ATML2127031800004863
RIOTBASE ?= $(CURDIR)/../../RIOT

USEMODULE += gnrc_netif_default
USEMODULE += auto_init_gnrc_netif
USEMODULE += gnrc_ipv6_default
USEMODULE += gnrc_udp
USEMODULE += ps

USEPKG += libsoletta
# soletta deps?
#USEMODULE += posix
CFLAGS += -D__LINUX_ERRNO_EXTENSIONS__=1

CFLAGS += -DDEVELHELP -DTHREAD_STACKSIZE_MAIN=3072

QUIET ?= 1

include $(RIOTBASE)/Makefile.include
