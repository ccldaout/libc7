#
# Makefile
#
# Copyright (c) 2019 ccldaout@gmail.com
#
# This software is released under the MIT License.
# http://opensource.org/licenses/mit-license.php
#


C7_TARGET_BASE = libc7$(C7_TARGET_DBGSFX)
C7_CC_IPATH = .

include ../Makefile.version
include ../Makefile.common

CFLAGS += -fPIC
C7_LD_LPATH =
C7_LD_RPATH =
C7_NOLIBC7 = y

C7_SONAME      = $(C7_TARGET_BASE).$(C7_LIBSFX).$(C7_VER_MAJOR)
C7_REAL_NAME   = $(C7_TARGET_BASE).$(C7_LIBSFX).$(C7_VER_MAJOR).$(C7_VER_MINOR).$(C7_VER_PATCH)
C7_LINKER_NAME = $(C7_TARGET_BASE).$(C7_LIBSFX)
C7_VERSION_H   = $(C7_OUT_INCDIR)/_c7version.h

LDFLAGS += -Wl,-soname,$(C7_SONAME)

$(C7_OUT_OBJDIR)/c7coroutine.o: CFLAGS_PROTO=no-

SRCS := $(wildcard *.c)
OBJS := $(addprefix $(C7_OUT_OBJDIR)/,$(patsubst %.c,%.o,$(SRCS)))

.PHONY: build
build: init headers $(C7_TARGET_LIB)

$(C7_TARGET_LIB): $(OBJS) .
	$(call cmd_o,cc -shared $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS))
	@ln -sf $(C7_REAL_NAME) $(C7_OUT_LIBDIR)/$(C7_SONAME)
	@ln -sf $(C7_REAL_NAME) $(C7_OUT_LIBDIR)/$(C7_LINKER_NAME)

.PHONY: headers
headers:
	@mkdir -p $(C7_OUT_INCDIR) >/dev/null 2>&1
	@cp -p c7*.h $(C7_OUT_INCDIR)/
	@echo "#define C7_VERSION_MAJOR ($(C7_VER_MAJOR)UL)" >| $(C7_VERSION_H)
	@echo "#define C7_VERSION_MINOR ($(C7_VER_MINOR)UL)" >> $(C7_VERSION_H)
	@echo "#define C7_VERSION_PATCH ($(C7_VER_PATCH)UL)" >> $(C7_VERSION_H)
	@echo "#define C7_VERSION (C7_VERSION_MAJOR*1000000 + C7_VERSION_MINOR*1000 + C7_VERSION_PATCH)" >> $(C7_VERSION_H)
