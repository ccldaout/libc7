#
# Makefile
#
# Copyright (c) 2019 ccldaout@gmail.com
#
# This software is released under the MIT License.
# http://opensource.org/licenses/mit-license.php
#


C7_TARGET_BASE = libc7
C7_CC_IPATH = .

include ../Makefile.common

CFLAGS += -fPIC
C7_LD_LPATH =
C7_LD_RPATH =
C7_NOLIBC7 = y

#CFLAGS += -Wno-unused-value

$(C7_OUT_OBJDIR)/c7coroutine.o: CFLAGS_PROTO=no-

SRCS := $(wildcard *.c)
OBJS := $(addprefix $(C7_OUT_OBJDIR)/,$(patsubst %.c,%.o,$(SRCS)))

.PHONY: build
build: init headers $(C7_TARGET_LIB)

$(C7_TARGET_LIB): $(OBJS) .
	$(call cmd_o,cc -shared $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS))

.PHONY: headers
headers:
	@mkdir -p $(C7_OUT_INCDIR) >/dev/null 2>&1
	@cp -p c7*.h $(C7_OUT_INCDIR)/
