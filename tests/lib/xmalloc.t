#! /bin/sh
# $Id$
#
# Test suite for xmalloc and friends.

# The count starts at 1 and is updated each time ok is printed.  printcount
# takes "ok" or "not ok".
count=1
printcount () {
    echo "$1 $count $2"
    count=`expr $count + 1`
}

# Run a program expected to succeed, and print ok if it does.
runsuccess () {
    output=`$xmalloc "$1" "$2" "$3" 2>&1 >/dev/null`
    status=$?
    if test $status = 0 && test -z "$output" ; then
        printcount "ok"
    else
        if test $status = 2 ; then
            printcount "ok" "# skip - no data limit support"
        else
            printcount "not ok"
            echo "  $output"
        fi
    fi
}

# Run a program expected to fail and make sure it fails with an exit status
# of 2 and the right failure message.  Strip the colon and everything after
# it off the error message since it's system-specific.
runfailure () {
    output=`$xmalloc "$1" "$2" "$3" 2>&1 >/dev/null`
    status=$?
    output=`echo "$output" | sed 's/:.*//'`
    if test $status = 1 && test x"$output" = x"$4" ; then
        printcount "ok"
    else
        if test $status = 2 ; then
            printcount "ok" "# skip - no data limit support"
        else
            printcount "not ok"
            echo "  saw: $output"
            echo "  not: $4"
        fi
    fi
}

# Find where the helper program is.
xmalloc=xmalloc
for file in ./xmalloc lib/xmalloc ../xmalloc ; do
    [ -x $file ] && xmalloc=$file
done

# Total tests.
echo 26

# First run the tests expected to succeed.
runsuccess "m" "21"     "0"
runsuccess "m" "128000" "0"
runsuccess "m" "0"      "0"
runsuccess "r" "21"     "0"
runsuccess "r" "128000" "0"
runsuccess "s" "21"     "0"
runsuccess "s" "128000" "0"
runsuccess "n" "21"     "0"
runsuccess "n" "128000" "0"
runsuccess "c" "24"     "0"
runsuccess "c" "128000" "0"

# Now limit our memory to 96KB and then try the large ones again, all of
# which should fail.
runfailure "m" "128000" "96000" \
    "failed to malloc 128000 bytes at lib/xmalloc.c line 28"
runfailure "r" "128000" "96000" \
    "failed to realloc 128000 bytes at lib/xmalloc.c line 49"
runfailure "s" "64000"  "96000" \
    "failed to strdup 64000 bytes at lib/xmalloc.c line 73"
runfailure "n" "64000"  "96000" \
    "failed to strndup 64000 bytes at lib/xmalloc.c line 95"
runfailure "c" "128000" "96000" \
    "failed to calloc 128000 bytes at lib/xmalloc.c line 116"

# Check our custom error handler.
runfailure "M" "128000" "96000" "malloc 128000 lib/xmalloc.c 28"
runfailure "R" "128000" "96000" "realloc 128000 lib/xmalloc.c 49"
runfailure "S" "64000"  "96000" "strdup 64000 lib/xmalloc.c 73"
runfailure "N" "64000"  "96000" "strndup 64000 lib/xmalloc.c 95"
runfailure "C" "128000" "96000" "calloc 128000 lib/xmalloc.c 116"

# Check the smaller ones again just for grins.
runsuccess "m" "21" "96000"
runsuccess "r" "32" "96000"
runsuccess "s" "64" "96000"
runsuccess "n" "20" "96000"
runsuccess "c" "24" "96000"
