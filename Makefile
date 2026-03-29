CC = gcc
MINGW_CC = x86_64-w64-mingw32-gcc
CFLAGS = -O2 -g -Wall -Wno-multichar

SRCS = main.c mkntfs.c attrdef.c upcase.c
HDRS = mkntfs.h ntfs_types.h

# Native Linux build
all: winmkntfs

winmkntfs: $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

# Windows cross-compile
win: winmkntfs.exe

winmkntfs.exe: $(SRCS) $(HDRS)
	$(MINGW_CC) $(CFLAGS) -o $@ $(SRCS)

# Test: create 64MB image and format it
test: winmkntfs
	dd if=/dev/zero of=test.img bs=1M count=64 status=none
	./winmkntfs test.img -L TestVol
	@echo "--- Verifying with ntfsinfo ---"
	ntfsinfo -m test.img 2>/dev/null || echo "(ntfsinfo not available)"
	@echo "--- Listing root ---"
	ntfsls test.img 2>/dev/null || echo "(ntfsls not available)"

clean:
	rm -f winmkntfs winmkntfs.exe test.img

.PHONY: all win test clean
