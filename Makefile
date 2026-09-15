# Makefile - build, install, and distribution rules for wewm

include config.mk

SRC = wewm.c drw.c util.c
OBJ = $(SRC:.c=.o)

VERSION ?= 1.0
DISTNAME = $(BIN)-$(VERSION)
DISTFILE = $(DISTNAME).tar.gz

# Files needed to reconstruct and build the project.  config.h is deliberately
# excluded: it is a user-maintained/generated local configuration file.
DISTFILES = \
	wewm.c \
	drw.c \
	drw.h \
	util.c \
	util.h \
	config.def.h \
	config.mk \
	Makefile \
	wewm.1 \
	README \
	transient.c

.PHONY: all clean install uninstall dist

all: $(BIN)

# The current wewm.c does not include or otherwise consume config.h, so the
# object dependencies intentionally do not depend on CONFIG_H.  Keep this
# rule for the conventional config.def.h -> config.h workflow and for future
# integration without overwriting an existing user configuration.
$(CONFIG_H): $(CONFIG_DEF_H)
	@if test -e "$@"; then \
		:; \
	else \
		cp "$<" "$@"; \
	fi

$(BIN): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

wewm.o: wewm.c config.h drw.h util.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

drw.o: drw.c drw.h util.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

util.o: util.c util.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(BIN)

install: $(BIN)
	mkdir -p "$(DESTDIR)$(BINDIR)" "$(DESTDIR)$(MAN1DIR)"
	install -m 755 $(BIN) "$(DESTDIR)$(BINDIR)/$(BIN)"
	install -m 644 $(MAN) "$(DESTDIR)$(MAN1DIR)/$(notdir $(MAN))"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/$(BIN)" "$(DESTDIR)$(MAN1DIR)/$(notdir $(MAN))"

dist:
	rm -rf "$(DISTNAME)"
	mkdir -p "$(DISTNAME)"
	cp $(DISTFILES) "$(DISTNAME)/"
	tar -czf "$(DISTFILE)" "$(DISTNAME)"
	rm -rf "$(DISTNAME)"
