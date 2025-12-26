# Sub folder makefile for URIX kernel - interrupts
include ../../rules.mk

# All C sources in this folder
C_SRC := $(wildcard *.c)
# All assembly sources in this folder
ASM_SRC := $(wildcard *.S)

# Object files in build dir
C_OBJ := $(patsubst %.c, $(BUILDDIR)/%.o, $(C_SRC))
ASM_OBJ := $(patsubst %.S, $(BUILDDIR)/%.o, $(ASM_SRC))

# All objects combined
OBJ := $(C_OBJ) $(ASM_OBJ)

# Final combined object
LIB_OBJ := $(BUILDDIR)/lib.o

.PHONY: all clean

all: $(LIB_OBJ)

# Compile .c -> build/.o
$(BUILDDIR)/%.o: %.c
	@mkdir -p $(BUILDDIR)/
	$(CC) $(CFLAGS) -c -o $@ $<

# Compile .S -> build/.o
$(BUILDDIR)/%.o: %.S
	@mkdir -p $(BUILDDIR)/
	$(AS) $(ASFLAGS) -c -o $@ $<

# Link all .o files into one .o file
$(LIB_OBJ): $(OBJ)
	$(LD) -r -o $@ $(OBJ)

clean:
	rm -rf $(BUILDDIR) $(LIB_OBJ)