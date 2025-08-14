# UI makefile for URIX kernel
include ../../rules.mk

# All C sources in this folder
SRC := $(wildcard *.c)

# Object files in build dir
OBJ := $(patsubst %.c, $(BUILDDIR)/ui/%.o, $(SRC))

# Final combined object
UI_OBJ := $(BUILDDIR)/ui.o

.PHONY: all clean

all: $(UI_OBJ)

# Compile .c -> build/ui/.o
$(BUILDDIR)/ui/%.o: %.c
	@mkdir -p $(BUILDDIR)/ui
	$(CC) $(CFLAGS) -c -o $@ $<

# Link all UI .o into one ui.o
$(UI_OBJ): $(OBJ)
	$(LD) -r -o $@ $(OBJ)

clean:
	rm -rf $(BUILDDIR) $(UI_OBJ) 
