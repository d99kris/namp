bin_PROGRAMS                 = namp

namp_SOURCES                 = main.c                                        \
                               main.h                                        \
                               model.c                                       \
                               model.h                                       \
                               uiview.c                                      \
                               uiview.h                                      \
                               uictrl.c                                      \
                               uictrl.h                                      \
                               play.c                                        \
                               play.h                                        \
                               mpeg.c                                        \
                               mpeg.h                                        \
                               mixer.c                                       \
                               mixer.h                                       \
                               list.c                                        \
                               list.h                                        \
                               utils.c                                       \
                               utils.h                                       \
                               conf.c                                        \
                               conf.h                                        \
                               scrobbler.c                                   \
                               scrobbler.h

namp_LDFLAGS                 = -lmpg123                                      \
                               -lao                                          \
                               -lasound                                      \
                               -lcurl                                        \
                               -lcrypto                                      \
                               -ltag_c                                       \
                               -lncursesw                                    \
                               `pkg-config --libs glib-2.0`

namp_CFLAGS                  = -O2                                           \
                               `pkg-config --cflags glib-2.0`                \
                               -I/usr/include/ncursesw                       \
                               -I/usr/include/taglib                         \
                               -Wall -Wextra -Wpedantic -std=gnu99           \
                               -Wshadow -Wpointer-arith -Wcast-qual          \
                               -Wstrict-prototypes -Wmissing-prototypes      \
                               -Wno-missing-braces -Wswitch-default          \
                               -Wcast-align -Wunreachable-code -Wundef       \
                               -Wuninitialized

dist_man_MANS                = namp.1

namp.1: $(namp_SOURCES) $(top_srcdir)/configure.ac
	-$(HELP2MAN) -n "ncurses audio media player" -N -o $@ $(builddir)/namp

