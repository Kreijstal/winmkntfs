# winmkntfs

Portable NTFS formatter for Windows and Linux. Creates NTFS 3.1 volumes that can be verified by ntfs-3g tools.

Written to provide ReactOS with an open-source `NtfsFormat` implementation, since the existing stub in `sdk/lib/fslib/ntfslib/ntfslib.c` is unimplemented.

## Status

**Work in progress.** Volumes pass `ntfsinfo` validation but ntfs-3g cannot fully mount them yet (see [issues](https://github.com/Kreijstal/winmkntfs/issues)).

What works:
- Boot sector, MFT, MFTMirr, $LogFile, $Bitmap, $UpCase, $AttrDef written correctly
- `ntfsfix` confirms MFT/MFTMirr integrity
- `ntfsinfo` reads volume name, version (3.1), geometry, all metadata

What doesn't work yet:
- ntfs-3g mount fails ($Secure lookup issue, [#1](https://github.com/Kreijstal/winmkntfs/issues/1))
- File creation via `ntfscp` fails ([#4](https://github.com/Kreijstal/winmkntfs/issues/4))

## Building

```bash
# Native Linux build
make

# Windows cross-compile (requires mingw-w64)
make win
```

## Usage

```bash
# Create an empty image and format it
dd if=/dev/zero of=disk.img bs=1M count=64
./winmkntfs disk.img -L MyVolume

# Options
#   -s <size>   Sector size in bytes (default: 512)
#   -c <size>   Cluster size in bytes (default: 4096)
#   -L <label>  Volume label
```

## Architecture

The formatter is split into a platform-independent core and platform-specific I/O:

| File | Purpose |
|------|---------|
| `mkntfs.c` | Core formatting logic (I/O-independent) |
| `mkntfs.h` | Public API with I/O callback interface |
| `ntfs_types.h` | NTFS on-disk structure definitions |
| `attrdef.c` | Attribute definition table ($AttrDef) |
| `upcase.c` | Unicode uppercase table ($UpCase) |
| `main.c` | CLI with POSIX and Win32 I/O backends |

The core accepts an `MKNTFS_IO` struct with `read`/`write`/`flush` callbacks, making it easy to integrate into other projects (e.g., ReactOS's `NtfsFormat`).

## System files created

$MFT, $MFTMirr, $LogFile, $Volume, $AttrDef, root directory, $Bitmap, $Boot, $BadClus, $Secure, $UpCase, $Extend (records 0-11), plus 4 reserved empty records (12-15).

## Related

- [ReactOS NTFS driver](https://github.com/reactos/reactos/tree/master/drivers/filesystems/ntfs)
- [ntfs-3g mkntfs](https://github.com/tuxera/ntfs-3g/blob/edge/ntfsprogs/mkntfs.c) (reference implementation)
- [ReactOS PR #7427](https://github.com/reactos/reactos/pull/7427) (earlier attempt at NTFS format support)

## License

GPL-2.0-or-later
