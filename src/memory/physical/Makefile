# Sub folder makefile for URIX kernel
include ../../../rules.mk

# All C sources in this folder
SRC := $(wildcard *.c)

# Object files in build dir
OBJ := $(patsubst %.c, $(BUILDDIR)/%.o, $(SRC))

# Final combined object
LIB_OBJ := $(BUILDDIR)/lib.o

.PHONY: all clean

all: $(LIB_OBJ)

# Compile .c -> build/.o
$(BUILDDIR)/%.o: %.c
	@mkdir -p $(BUILDDIR)/
	$(CC) $(CFLAGS) -c -o $@ $<

# Link all .o files into one .o file
$(LIB_OBJ): $(OBJ)
	$(LD) -r -o $@ $(OBJ)

clean:
	rm -rf $(BUILDDIR) $(LIB_OBJ) 
