#! /bin/sh

set -e

aclocal
#autoheader
libtoolize --automake --force --copy
automake --add-missing --force --copy
autoconf
