PKG_NAME=soletta
PKG_URL=https://github.com/solettaproject/soletta
PKG_VERSION=master
PKG_DIR=$(CURDIR)/$(PKG_NAME)

export CFLAGS += $(INCLUDES)
export LDFLAGS += $(LINKFLAGS)
export TOOLCHAIN_PREFIX = $(PREFIX)

ifneq (,$(SOLETTA_CONF))
MERGE_APP_CONF = merge_app_conf
endif

export PKG_CONFIG_LIBDIR=
unexport BINDIR

.PHONY: all clean

all: $(PKG_DIR)/.config $(MERGE_APP_CONF)
	"$(MAKE)" -C $(PKG_DIR)
	cp $(PKG_DIR)/build/soletta_sysroot/usr/lib/libsoletta.a $(BINDIR)/libsoletta.a

merge_app_conf: $(SOLETTA_CONF) $(PKG_DIR)/.config
	@cd $(PKG_DIR) && \
	tools/kconfig/merge_config.sh .config $(SOLETTA_CONF)

$(PKG_DIR)/.config: $(PKG_DIR)/Makefile
	"$(MAKE)" -C $(PKG_DIR) alldefconfig

$(PKG_DIR)/Makefile: $(PKG_DIR)/.git/config

$(PKG_DIR)/.git/config:
	test -d "$(PKG_DIR)" || git clone "$(PKG_URL)" "$(PKG_DIR)"; \
		cd "$(PKG_DIR)" && git checkout -f "$(PKG_VERSION)"

clean::
	@echo "Cleaning up soletta package..."
	@cd "$(PKG_DIR)" 2> /dev/null > /dev/null && \
		$(MAKE) clean


distclean::
	rm -rf "$(PKG_DIR)"

Makefile.include:
	@true
