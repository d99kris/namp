# Init
AC_INIT([namp], [1.0], [kristofer.berggren@gmail.com], [], 
        [https://github.com/d99kris/namp])
AM_INIT_AUTOMAKE([
  -Wall -Werror foreign
])

# Deps - glib
AM_PATH_GLIB_2_0(2.0.0,,
             AC_MSG_ERROR(Required library glib >= 2.0 not found.))

# Deps - ncursesw
AC_CHECK_LIB([ncursesw], [waddnwstr], ,
             AC_MSG_ERROR(Required library ncursesw not found.))
AC_CHECK_HEADER([ncursesw/ncurses.h], ,
             AC_MSG_ERROR(Headers for ncursesw not found.))

# Deps - libmpg123
AC_CHECK_LIB([mpg123], [mpg123_new], ,
             AC_MSG_ERROR(Required library mpg123 not found.))
AC_CHECK_HEADER([mpg123.h], ,
             AC_MSG_ERROR(Headers for mpg123 not found.))

# Deps - libao
AC_CHECK_LIB([ao], [ao_open_live], ,
             AC_MSG_ERROR(Required library ao not found.))
AC_CHECK_HEADER([ao/ao.h], ,
             AC_MSG_ERROR(Headers for ao not found.))

# Deps - libasound
AC_CHECK_LIB([asound], [snd_mixer_open], ,
             AC_MSG_ERROR(Required library asound not found.))
AC_CHECK_HEADER([alsa/asoundlib.h], ,
             AC_MSG_ERROR(Headers for asound not found.))

# Deps - taglib
AC_CHECK_LIB([tag_c], [taglib_file_new], ,
             AC_MSG_ERROR(Required library tag not found.))
AC_CHECK_HEADER([taglib/tag_c.h], ,
             AC_MSG_ERROR(Headers for tag not found.))

# Deps - libcurl
AC_CHECK_LIB([curl], [curl_easy_init], ,
             AC_MSG_ERROR(Required library curl not found.))
AC_CHECK_HEADER([curl/curl.h], ,
             AC_MSG_ERROR(Headers for curl not found.))

# Deps - crypto
AC_CHECK_LIB([crypto], [MD5], ,
             AC_MSG_ERROR(Required library crypto not found.))
AC_CHECK_HEADER([openssl/md5.h], ,
             AC_MSG_ERROR(Headers for crypto not found.))

# Deps - help2man
AC_CHECK_PROG([HELP2MAN], [help2man], [help2man])
test -z "$HELP2MAN" && AC_MSG_ERROR(Required tool help2man not found.)

# Prog
AC_PROG_CC
AM_PROG_AR
AC_PROG_LIBTOOL

# Config
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_FILES([
  Makefile
  src/Makefile
])

# Output
AC_OUTPUT

