# vim: foldmethod=marker foldlevel=0

.POSIX:

include config.mk

inc_fonts_0 =
inc_fonts_1 = -I/usr/include/freetype2 -I$(PREFIX)/include/freetype2
lib_fonts_0 =
lib_fonts_1 = -lXft -lfontconfig
lib_exif_0 =
lib_exif_1 = -lexif

nsxiv_cflags = -D_XOPEN_SOURCE=700 \
  -DHAVE_LIBEXIF=$(HAVE_LIBEXIF) -DHAVE_LIBFONTS=$(HAVE_LIBFONTS) \
  -DHAVE_INOTIFY=$(HAVE_INOTIFY) $(inc_fonts_$(HAVE_LIBFONTS))

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
	$(CC) $(LDFLAGS) $(nsxiv_ldflags) -o $@ $(objects) $(nsxiv_ldlibs)

$(build_dir):
	@mkdir -p $(build_dir)

$(build_dir)/%.o: $(src_dir)/%.c | $(build_dir)
	@echo "===> CC $@"
	$(CC) $(CFLAGS) $(nsxiv_cflags) -c $< -o $@


$(objects): config.mk $(include_dir)/nsxiv.h $(include_dir)/config.h $(include_dir)/commands.h
$(build_dir)/options.o: $(include_dir)/version.h $(include_dir)/optparse.h
$(build_dir)/window.o: $(include_dir)/icon_data.h $(include_dir)/utf8.h


# Header generation {{{

$(include_dir)/icon_data.gen.h:
	@echo "===> GEN $@"
	make -C ./icon
	mv ./icon/data.gen.h $@

$(include_dir)/icon_data.h: $(include_dir)/icon_data.gen.h

$(include_dir)/config.h: config.def.h
	@echo "===> GEN $@"
	cp config.def.h $@

$(include_dir)/version.h: config.mk .git/index
	@echo "===> GEN $@"
	v="$$(git describe 2>/dev/null || true)"; \
	echo "#define VERSION \"$${v:-$(VERSION)}\"" >$@

.git/index:

# }}}


# Targets for Installing and Uninstalling {{{

.PHONY: install-all
install-all: install install-desktop install-icon

.PHONY: install-desktop
install-desktop:
	@echo "INSTALL nsxiv.desktop"
	mkdir -p $(DESTDIR)$(PREFIX)/share/applications
	cp etc/nsxiv.desktop $(DESTDIR)$(PREFIX)/share/applications

.PHONY: install-icon
install-icon:
	@echo "INSTALL icon"
	for f in $(ICONS); do \
		dir="$(DESTDIR)$(PREFIX)/share/icons/hicolor/$${f%.png}/apps"; \
		mkdir -p "$$dir"; \
		cp "icon/$$f" "$$dir/nsxiv.png"; \
		chmod 644 "$$dir/nsxiv.png"; \
	done

.PHONY: uninstall-icon
uninstall-icon:
	@echo "REMOVE icon"
	for f in $(ICONS); do \
		dir="$(DESTDIR)$(PREFIX)/share/icons/hicolor/$${f%.png}/apps"; \
		rm -f "$$dir/nsxiv.png"; \
	done

.PHONY: install
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

.PHONY: uninstall
uninstall: uninstall-icon
	@echo "REMOVE bin/nsxiv"
	rm -f $(DESTDIR)$(PREFIX)/bin/nsxiv
	@echo "REMOVE nsxiv.1"
	rm -f $(DESTDIR)$(MANPREFIX)/man1/nsxiv.1
	@echo "REMOVE nsxiv.desktop"
	rm -f $(DESTDIR)$(PREFIX)/share/applications/nsxiv.desktop
	@echo "REMOVE share/nsxiv/"
	rm -rf $(DESTDIR)$(EGPREFIX)

# }}}


# Phony helper targets {{{

.PHONY: dev
dev: compile_commands.json ctags

compile_commands.json: $(sources)
	make clean
	bear -- make

.PHONY: ctags
ctags: $(sources)
	ctags -R

.PHONY: dump_cflags
dump_cflags:
	@echo $(nsxiv_cflags)

.PHONY: clean
clean:
	@rm -rf $(build_dir) $(include_dir)/version.h $(include_dir)/icon_data.gen.h
	@rm -f ./nsxiv ./tags ./compile_commands.json
	@echo "Cleaned!"

.PHONY: lint
lint: compile_commands.json
	@# warning: Enable warning messages
	@# style: Enable all coding style checks. Implies 'performance' and 'portability'
	@# performance: Enable performance messages
	@# portability: Enable portability messages
	@# information: Enable information messages
	@# unusedFunction: Check for unused functions.
	@# missingInclude: Warn if there are missing includes
	cppcheck --project=compile_commands.json --check-level=exhaustive --inline-suppr \
		--enable=warning,style --platform=unix64 --language=c -j $(shell nproc)
# }}}
