LIB := drivers.lib

include ../config.env

all: build $(OBJS_C) $(OBJS_Z80)
	$(AR) -rcD $(OUTPUT)/$(LIB) $(OBJS_C) $(OBJS_Z80)

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
