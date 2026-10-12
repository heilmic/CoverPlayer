# Library scanning and large collections

CoverPlayer stores a library index for each configured collection. The index
contains the folder hierarchy, track tags, estimated durations and artwork
paths. It is separate from saved playback progress and collection settings.

## Every startup

For each available collection, CoverPlayer walks the directory tree and builds
a fingerprint from relevant file paths, sizes and modification times. These
records are sorted and hashed. This does not hash MP3 contents, but it still
visits the whole tree and queries filesystem metadata on every startup.

If the fingerprint and cache format match, the stored library is loaded. MP3
tags, durations and embedded images do not need to be extracted again.

## First scan or changed collection

When an index is missing, outdated or invalidated, the entire collection is
scanned again. For each MP3, CoverPlayer:

1. Reads ID3 tags for the title, artist, album and track number. ID3v2 tags are
   limited to 8 MiB; the reader also reads up to 64 KiB beyond the tag to
   validate MP3 data, and consults the final ID3v1 tag for missing fields.
2. Extracts embedded artwork into the application preference directory. A full
   rescan currently rewrites these images, even if most tracks are unchanged.
3. Opens the MP3 again to estimate duration from Xing/Info/VBRI information or
   bitrate and file size, reading at most 256 KiB after the ID3 tag. It does not
   decode the complete audio file.
4. Sorts the tracks, constructs the folder hierarchy and saves a new index.

The work runs synchronously. Progress counts visited directories rather than
processed tracks, so one folder with many MP3s can spend a long time on the
same displayed directory.

## Efficiency and practical expectations

The cached path avoids repeated MP3 parsing, and file reads for duration are
bounded. However, there is no per-file incremental index: one changed MP3 or
cover invalidates its whole collection. Large ID3 images, many small filesystem
operations and cover writes can make a first scan take minutes on a handheld
SD card. Concurrent synchronization can compete for storage access; files
added during synchronization can also cause another full scan on the next
launch. A duration cannot be predicted from total library size alone.

If possible, let synchronization finish before scanning. Smaller, separately
configured collections limit the scope of a rebuild, although each collection
still needs a fingerprint check on startup. Overlapping collection roots can
cause the same files to be checked or scanned more than once.

For very large libraries, the next performance improvement should be an
incremental per-file index that reuses unchanged tags, durations and artwork.
Phase timings, track-based progress and background scanning would also make
long operations easier to diagnose and use. These are proposed improvements;
v1.0.1 changes the Now Playing metadata handling, not the scanning strategy.

## Updating to v1.0.1

Cache format v6 distinguishes actual folder artwork from artwork borrowed for
the library preview. Older indexes rebuild once after updating, so the first
launch can take longer. Playback progress and collection settings are kept.
