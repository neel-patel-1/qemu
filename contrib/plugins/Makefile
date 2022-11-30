# -*- Mode: makefile -*-
#
# This Makefile example is fairly independent from the main makefile
# so users can take and adapt it for their build. We only really
# include config-host.mak so we don't have to repeat probing for
# cflags that the main configure has already done for us.
#

BUILD_DIR := $(CURDIR)/../../build

include $(BUILD_DIR)/config-host.mak

VPATH += $(SRC_PATH)/contrib/plugins

NAMES :=
NAMES += execlog
NAMES += hotblocks
NAMES += hotpages
NAMES += howvec
NAMES += lockstep
NAMES += hwprofile
NAMES += cache
NAMES += drcov
NAMES += inst-trace
NAMES += cache-inclusive
NAMES += cache-inclusive-parallel
NAMES += cache-inclusive-parallel-1
NAMES += cache-inclusive-parallel-1-3
NAMES += empty-callback
NAMES += byte-size-dist
NAMES += print-specific-byte
NAMES += byte-size-dist-1
NAMES += byte-size-dist-1-3
NAMES += inst-trace-arm
NAMES += inst-trace-bytes
NAMES += inst-trace-bytes-3


SONAMES := $(addsuffix .so,$(addprefix lib,$(NAMES)))

# The main QEMU uses Glib extensively so it's perfectly fine to use it
# in plugins (which many example do).
CFLAGS = $(GLIB_CFLAGS)
CFLAGS += -fPIC -Wall $(filter -W%, $(QEMU_CFLAGS))
CFLAGS += $(if $(findstring no-psabi,$(QEMU_CFLAGS)),-Wpsabi)
CFLAGS += -I$(SRC_PATH)/include/qemu

all: cache-sim-naked.o cache-inclusive-parallel.o $(SONAMES)

cache-sim-naked.o : cache-sim-naked.c cache-parallel.h
	$(CC) -pthread -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -Wall -fPIC -c -o $@ $<

cache-inclusive-parallel.o : cache-inclusive-parallel.c cache-parallel.h
	$(CC) $(CFLAGS) -c -o $@ $<

libcache-inclusive-parallel.so: cache-inclusive-parallel.o cache-sim-naked.o
	$(CC) -shared -Wl,-soname,$@ -o $@ $^ $(LDLIBS)

cache-inclusive-parallel-4-7.o : cache-inclusive-parallel.c cache-parallel.h
	$(CC) $(CFLAGS) -DCONFIG_4_7 -c -o $@ $<

libcache-inclusive-parallel-4-7.so: cache-inclusive-parallel-4-7.o cache-sim-naked.o
	$(CC) -shared -DCONFIG_4_7 -Wl,-soname,$@ -o $@ $^ $(LDLIBS)

cache-inclusive-parallel-1-3.o : cache-inclusive-parallel.c cache-parallel.h
	$(CC) $(CFLAGS) -DCONFIG_1_3 -c -o $@ $<

libcache-inclusive-parallel-1-3.so: cache-inclusive-parallel-1-3.o cache-sim-naked.o
	$(CC) -shared -DCONFIG_1_3 -Wl,-soname,$@ -o $@ $^ $(LDLIBS)

cache-inclusive-parallel-1.o : cache-inclusive-parallel.c cache-parallel.h
	$(CC) $(CFLAGS) -DCONFIG_1 -c -o $@ $<

libcache-inclusive-parallel-1.so: cache-inclusive-parallel-1.o cache-sim-naked.o
	$(CC) -shared -DCONFIG_1 -Wl,-soname,$@ -o $@ $^ $(LDLIBS)

byte-size-dist-1.o : byte-size-dist.c
	$(CC) $(CFLAGS) -DCONFIG_1 -c -o $@ $<

libbyte-size-dist-1.so: byte-size-dist-1.o
	$(CC) -shared -DCONFIG_1 -Wl,-soname,$@ -o $@ $^ $(LDLIBS)

byte-size-dist-1-3.o : byte-size-dist.c
	$(CC) $(CFLAGS) -DCONFIG_1_3 -c -o $@ $<

libbyte-size-dist-1-3.so: byte-size-dist-1-3.o
	$(CC) -shared -DCONFIG_1_3 -Wl,-soname,$@ -o $@ $^ $(LDLIBS)


inst-trace-bytes-3.o : inst-trace-bytes.c
	$(CC) $(CFLAGS) -DCONFIG_3 -c -o $@ $<

inst-trace-bytes-3.so: byte-size-dist-3.o
	$(CC) -shared -DCONFIG_3 -Wl,-soname,$@ -o $@ $^ $(LDLIBS)


%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

lib%.so: %.o
	$(CC) -shared -Wl,-soname,$@ -o $@ $^ $(LDLIBS)

clean:
	rm -f *.o *.so *.d
	rm -Rf .libs

.PHONY: all clean
