include ../config.env

all: build $(OUTPUT)/yrw801.dat $(OUTPUT)/waves.dat $(OUTPUT)/STANDARD.BNK
ifeq ($(OS),Windows_NT)
	@copy $(OUTPUT)\*.dat $(DISK)
	@copy $(OUTPUT)\STANDARD.BNK $(DISK)
else
	cp $(OUTPUT)/*.dat $(DISK)
	cp $(OUTPUT)/STANDARD.BNK $(DISK)
endif

$(OUTPUT)/yrw801.dat: src/yrw801.c $(OUTPUT)/yrw801_index.rel
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(OUTPUT)/yrw801_index.rel --code-loc 0x8102 -o $(OUTPUT)/$(basename $(<F)).ihx
ifeq ($(OS),Windows_NT)
	@..\bin\hex2bin -e dat $(OUTPUT)\$(basename $(<F)).ihx
else
	../bin/hex2bin -e dat $(OUTPUT)/$(basename $(<F)).ihx
endif

$(OUTPUT)/waves.dat: src/waves.c $(OUTPUT)/wave_patches.rel
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(OUTPUT)/wave_patches.rel --code-loc 0x8000 -o $(OUTPUT)/$(basename $(<F)).ihx
ifeq ($(OS),Windows_NT)
	../bin/hex2bin -e dat $(OUTPUT)/$(basename $(<F)).ihx
else
endif

$(OUTPUT)/STANDARD.BNK: STANDARD.BNK
ifeq ($(OS),Windows_NT)
	@copy $< $(subst /,\,$@)
else
	cp $< $@
endif

$(OUTPUT)/%.rel: src/%.z80
	$(AS) -o $@ $<

$(OUTPUT)/%.rel: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

build:
ifeq ($(OS),Windows_NT)
	@mkdir $(OUTPUT)
else
	mkdir -p $(OUTPUT)
endif

.PHONY: clean
clean:
ifeq ($(OS),Windows_NT)
	@if exist $(OUTPUT) del /s/q $(OUTPUT)
	@if exist $(OUTPUT) rmdir $(OUTPUT)
else
	rm -rf $(OUTPUT)
endif
