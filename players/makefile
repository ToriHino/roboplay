include ../config.env

CODE := 0x4100
DATA := 0x0000

ifeq ($(OS),Windows_NT)
	LIBS := -l..\support\build\support.lib
else
	LIBS := -l../support/build/support.lib
endif

PLAYERS := $(patsubst src/%.c,$(OUTPUT)/%.ply,$(SOURCES_C))

all: build $(PLAYERS)

$(OUTPUT)/%.ply: $(OUTPUT)/%.rel
ifeq ($(OS),Windows_NT)
	@..\bin\hex2bin -e ply $(OUTPUT)\$(basename $(<F)).ihx
	@copy $(OUTPUT)\$(basename $(<F)).ply $(DISK)
else
	../bin/hex2bin -e ply $(OUTPUT)/$(basename $(<F)).ihx
	cp $(OUTPUT)/$(basename $(<F)).ply $(DISK)
endif

$(OUTPUT)/player_base.rel: src/player_base.z80
	$(AS) -o $@ $<

$(OUTPUT)/%.rel: src/%.c $(OUTPUT)/player_base.rel
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) --code-loc $(CODE) --data-loc $(DATA) $^ -o $(OUTPUT)/$(basename $(<F)).ihx

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
