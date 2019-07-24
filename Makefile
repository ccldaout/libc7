MAKE_JOBS ?= 8

UNITS = src tools
.PHONY: rebuild clean all doxygen $(UNITS)

all: $(UNITS) doxygen
$(UNITS):
	@for _ in `seq 20`; do echo; done
	$(MAKE) --directory=$@ --no-print-directory clean
	$(MAKE) --directory=$@ --no-print-directory -j$(MAKE_JOBS)
