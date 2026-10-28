include config.env

all: application

ifeq ($(OS),Windows_NT)
export PATH := $(PWD)\bin:$(PATH)
else
export PATH := $(PWD)/bin:$(PATH)
endif

ifeq ($(OS),Windows_NT)
bin\hex2bin.exe:
	if not exist bin mkdir bin
	if not exist hex2bin-2.0\bin\ mkdir hex2bin-2.0\bin
else
bin/hex2bin:
	mkdir -p bin
	mkdir -p hex2bin-2.0/bin
endif
	make -C hex2bin-2.0
	make -C hex2bin-2.0 cleanall

support:
	make -C support

drivers:
	make -C drivers

players:
	make -C players

data:
	make -C data

ifeq ($(OS),Windows_NT)
application: bin\hex2bin.exe dsk support drivers data players
else
application: bin/hex2bin dsk support drivers data players
endif
	make -C application

dsk:
ifeq ($(OS),Windows_NT)
	mkdir $(OUTPUT)\$(DISK)
else
	mkdir $(OUTPUT)/$(DISK)
endif

.PHONY: clean cleanall support drivers players data application
clean:
	make -C hex2bin-2.0 cleanall
	make -C support clean
	make -C drivers clean
	make -C players clean
	make -C data clean
	make -C application clean
ifeq ($(OS),Windows_NT)
	@if exist bin del /s/q bin
	@if exist bin rmdir bin
	@if exist $(OUTPUT) del /s/q $(OUTPUT)
	@if exist $(OUTPUT) rmdir $(OUTPUT)
else
	rm -rf bin
	rm -rf $(OUTPUT)
endif

