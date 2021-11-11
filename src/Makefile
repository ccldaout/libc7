#
# Makefile
#
# Copyright (c) 2019 ccldaout@gmail.com
#
# This software is released under the MIT License.
# http://opensource.org/licenses/mit-license.php
#


C7_TARGET_BASE = libc7
C7_VER_MAJOR = 2
C7_VER_MINOR = 1
C7_VER_PATCH = 1
C7_CC_IPATH = .

include ../Makefile.common

CFLAGS += -fPIC
C7_LD_LPATH =
C7_LD_RPATH =
C7_NOLIBC7 = y

C7_SONAME      = $(C7_TARGET_BASE).$(C7_LIBSFX).$(C7_VER_MAJOR)
C7_REAL_NAME   = $(C7_TARGET_BASE).$(C7_LIBSFX).$(C7_VER_MAJOR).$(C7_VER_MINOR).$(C7_VER_PATCH)
C7_LINKER_NAME = $(C7_TARGET_BASE).$(C7_LIBSFX)

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
