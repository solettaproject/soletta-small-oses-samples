CONTIKI_PROJECT = srv
all: $(CONTIKI_PROJECT)

APPDIRS += $(CURDIR)/apps

APPS += soletta

#TARGET ?= quark-se-devboard
TARGET ?= native
CONTIKI_WITH_RPL=0
CONTIKI_WITH_RIME=0
MODULES += core/net/ipv6/multicast

SOLETTA_CONF=$(PWD)/sol.conf

CONTIKI ?= $(PWD)/contiki
include $(CONTIKI)/Makefile.include
