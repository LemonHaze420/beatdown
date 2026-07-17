# SCRIPT_BIN

`SCRIPT_BIN` editor/disassembler/assembler utility. 

```text
./script_bin --disassemble input.bin output.script
./script_bin --assemble input.script output.bin
```

`-d` and `-a` are the short forms. Without a mode, `.bin` inputs are disassembled and other inputs are assembled. 

Other projects can include `script.h` and link `script_bin.cpp` and `source_parser.cpp`.

## Status

All known samples are able to be decoded and encoded/edited with one exception that uses an empty bank.

Semantic coverage is complete. 

## Contributors

- LemonHaze420