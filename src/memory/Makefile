include ../../rules.mk

LIBS = physical

LIB_OBJS = $(foreach lib,$(LIBS),$(BUILDDIR)/$(notdir $(lib)).o)

SRC := $(wildcard *.c)

OBJ := $(patsubst %.c, $(BUILDDIR)/%.o, $(SRC))

ALL_OBJS := $(OBJ) $(LIB_OBJS)

LIB_OBJ := $(BUILDDIR)/lib.o

.PHONY: all clean

all: $(LIB_OBJ)

$(LIB_OBJS):
	@for lib in $(LIBS); do \
		$(MAKE) -C $$lib CFLAGS="$(CFLAGS)"; \
		cp $$lib/build/lib.o $(BUILDDIR)/$$(basename $$lib).o; \
	done

$(BUILDDIR)/%.o: %.c
	@mkdir -p $(BUILDDIR)/
	$(CC) $(CFLAGS) -c -o $@ $<

$(LIB_OBJ): $(ALL_OBJS)
	$(LD) -r -o $@ $(ALL_OBJS)

clean:
	rm -rf $(BUILDDIR) $(LIB_OBJ)
	@for lib in $(LIBS); do \
		$(MAKE) -C $$lib clean; \
	done