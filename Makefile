.POSIX:

include config.mk

inc_fonts_0 =
inc_fonts_1 = -I/usr/include/freetype2 -I$(PREFIX)/include/freetype2
lib_fonts_0 =
lib_fonts_1 = -lXft -lfontconfig
lib_exif_0 =
lib_exif_1 = -lexif

nsxiv_cppflags = -D_XOPEN_SOURCE=700 \
  -DHAVE_LIBEXIF=$(HAVE_LIBEXIF) -DHAVE_LIBFONTS=$(HAVE_LIBFONTS) \
  -DHAVE_INOTIFY=$(HAVE_INOTIFY) $(inc_fonts_$(HAVE_LIBFONTS)) \
  $(CPPFLAGS)

nsxiv_ldlibs = -lImlib2 -lX11 \
  $(lib_exif_$(HAVE_LIBEXIF)) $(lib_fonts_$(HAVE_LIBFONTS)) \
  $(LDLIBS)

include_dir := ./include
build_dir := ./build
src_dir := ./src
sources := $(shell find $(src_dir) -iname '*.c' -printf '%p\n')
objects = $(patsubst $(src_dir)/%.c,$(build_dir)/%.o,$(sources))

all: nsxiv

nsxiv: $(objects)
	@echo "===> LD $@"
	@echo $(objects)
	$(CC) $(LDFLAGS) -o $@ $(objects) $(nsxiv_ldlibs)

$(build_dir):
	@mkdir -p $(build_dir)

$(build_dir)/%.o: $(src_dir)/%.c | $(build_dir)
	@echo "===> CC $@"
	$(CC) $(CFLAGS) $(nsxiv_cppflags) -c $< -o $@

$(objects): Makefile config.mk $(include_dir)/nsxiv.h $(include_dir)/config.h $(include_dir)/commands.h

$(build_dir)/options.o: $(include_dir)/version.h $(include_dir)/optparse.h
$(build_dir)/window.o: $(include_dir)/icon_data.h $(include_dir)/utf8.h
$(include_dir)/icon_data.h: $(include_dir)/icon_data.gen.h

$(include_dir)/icon_data.gen.h:
	@echo "===> GEN $@"
	make -C ./icon
	mv ./icon/data.gen.h $@

$(include_dir)/config.h: config.def.h
	@echo "===> GEN $@"
	cp config.def.h $@

$(include_dir)/version.h: config.mk .git/index
	@echo "===> GEN $@"
	v="$$(git describe 2>/dev/null || true)"; \
	echo "#define VERSION \"$${v:-$(VERSION)}\"" >$@

.git/index:

dump_cppflags:
	@echo $(nsxiv_cppflags)

clean:
	@rm -rf $(build_dir) $(include_dir)/version.h $(include_dir)/icon_data.gen.h
	@echo "Cleaned!"

install-all: install install-desktop install-icon

install-desktop:
	@echo "INSTALL nsxiv.desktop"
	mkdir -p $(DESTDIR)$(PREFIX)/share/applications
	cp etc/nsxiv.desktop $(DESTDIR)$(PREFIX)/share/applications

install-icon:
	@echo "INSTALL icon"
	for f in $(ICONS); do \
		dir="$(DESTDIR)$(PREFIX)/share/icons/hicolor/$${f%.png}/apps"; \
		mkdir -p "$$dir"; \
		cp "icon/$$f" "$$dir/nsxiv.png"; \
		chmod 644 "$$dir/nsxiv.png"; \
	done

uninstall-icon:
	@echo "REMOVE icon"
	for f in $(ICONS); do \
		dir="$(DESTDIR)$(PREFIX)/share/icons/hicolor/$${f%.png}/apps"; \
		rm -f "$$dir/nsxiv.png"; \
	done

install: all
	@echo "INSTALL bin/nsxiv"
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp $(build_dir)/nsxiv $(DESTDIR)$(PREFIX)/bin/
	chmod 755 $(DESTDIR)$(PREFIX)/bin/nsxiv
	@echo "INSTALL nsxiv.1"
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	sed "s!EGPREFIX!$(EGPREFIX)!g; s!PREFIX!$(PREFIX)!g; s!VERSION!$(VERSION)!g" \
		etc/nsxiv.1 >$(DESTDIR)$(MANPREFIX)/man1/nsxiv.1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/nsxiv.1
	@echo "INSTALL share/nsxiv/"
	mkdir -p $(DESTDIR)$(EGPREFIX)
	cp etc/examples/* $(DESTDIR)$(EGPREFIX)
	chmod 755 $(DESTDIR)$(EGPREFIX)/*

uninstall: uninstall-icon
	@echo "REMOVE bin/nsxiv"
	rm -f $(DESTDIR)$(PREFIX)/bin/nsxiv
	@echo "REMOVE nsxiv.1"
	rm -f $(DESTDIR)$(MANPREFIX)/man1/nsxiv.1
	@echo "REMOVE nsxiv.desktop"
	rm -f $(DESTDIR)$(PREFIX)/share/applications/nsxiv.desktop
	@echo "REMOVE share/nsxiv/"
	rm -rf $(DESTDIR)$(EGPREFIX)

.PHONY: dev

dev: compile_commands.json

compile_commands.json:
	make clean
	bear -- make

