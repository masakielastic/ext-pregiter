PHP_ARG_ENABLE([pregiter],
  [whether to enable pregiter support],
  [AS_HELP_STRING([--enable-pregiter], [Enable pregiter support])],
  [yes])

if test "$PHP_PREGITER" != "no"; then
  PHP_ADD_EXTENSION_DEP(pregiter, pcre)
  PHP_NEW_EXTENSION([pregiter], [pregiter.c pregiter_iterator.c], [$ext_shared])
fi
