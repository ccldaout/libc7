MAKE_JOBS ?= 8

include Makefile.version

UNITS = src tools
.PHONY: rebuild clean all doxygen $(UNITS)

GITTAG_F=r$(C7_VER_MAJOR).$(C7_VER_MINOR).$(C7_VER_PATCH)
GITTAG_S=r$(C7_VER_MAJOR).$(C7_VER_MINOR)

all: $(UNITS) doxygen
$(UNITS):
	$(MAKE) --directory=$@ --no-print-directory clean
	$(MAKE) --directory=$@ --no-print-directory -j$(MAKE_JOBS)

tag_move:
	git tag -a -f -m $(GITTAG_F) $(GITTAG_F)
	git tag -a -f -m $(GITTAG_S) $(GITTAG_S)
	git push -f --tags x22
	git push -f --tags github

push:
	git push -f x22
	git push -f --tags x22
	git push -f github
	git push -f --tags github

pull:
	git pull; git fetch github; git fetch -f --tags
