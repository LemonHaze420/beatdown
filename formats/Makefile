.PHONY: all clean xaf lgp dat mrg ol csv scn script text_table

all: xaf lgp dat mrg ol csv scn script text_table

xaf:
	$(MAKE) -C XAF

lgp:
	$(MAKE) -C LGP

dat:
	$(MAKE) -C DAT

mrg:
	$(MAKE) -C MRG

ol:
	$(MAKE) -C OL

csv:
	$(MAKE) -C CSV

scn:
	$(MAKE) -C SCN

script:
	$(MAKE) -C SCRIPT_BIN

text_table:
	$(MAKE) -C TEXT_TABLE

clean:
	$(MAKE) -C LGP clean
	$(MAKE) -C DAT clean
	$(MAKE) -C MRG clean
	$(MAKE) -C OL clean
	$(MAKE) -C CSV clean
	$(MAKE) -C SCN clean
	$(MAKE) -C SCRIPT_BIN clean
	$(MAKE) -C TEXT_TABLE clean
	$(MAKE) -C XAF clean
