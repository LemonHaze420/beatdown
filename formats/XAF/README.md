# XAF

Inspects and extracts Beat Down's XAF archives, and replaces individual files while rebuilding the archive layout.

```text
./xaf input.xaf
./xaf extract input.xaf output_directory
./xaf replace input.xaf archive_path replacement_file output.xaf
```

An archive is inspected when no command is specified. `extract` also accepts a
directory of XAF archives. Build with `make`.

Other projects can include `xaf.h` and link `xaf_bin.cpp`.
