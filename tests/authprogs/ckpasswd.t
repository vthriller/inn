#! /bin/sh
# $Id$
#
# Test suite for ckpasswd.

# The count starts at 1 and is updated each time ok is printed.  printcount
# takes "ok" or "not ok".
count=1
printcount () {
    echo "$1 $count $2"
    count=`expr $count + 1`
}

# Run ckpasswd, expecting it to succeed.  Takes a username, a password, any
# additional options, and the expected output string.
runsuccess () {
    output=`$ckpasswd -u "$1" -p "$2" $3 2>&1`
    status=$?
    if test $status = 0 && test x"$output" = x"$4" ; then
        printcount "ok"
    else
        printcount "not ok"
        echo "  saw: $output"
        echo "  not: $4"
    fi
}

# Run ckpasswd, expecting it to fail, and make sure it fails with status 1 and
# prints out the right error message.  Takes a username, a password, any
# additional options, and the expected output string.
runfailure() {
    output=`$ckpasswd -u "$1" -p "$2" $3 2>&1`
    status=$?
    if test $status = 1 && test x"$output" = x"$4" ; then
        printcount "ok"
    else
        printcount "not ok"
        echo "  saw: $output"
        echo "  not: $4"
    fi
}

# Make sure we're in the right directory.
for dir in . authprogs tests/authprogs ; do
    test -f "$dir/passwd" && cd $dir
done
ckpasswd=../../authprogs/ckpasswd

# Print the test count.
echo 6

# First, run the tests that we expect to succeed.
runsuccess "foo" "foopass" "-f passwd" "User:foo"
runsuccess "bar" "barpass" "-f passwd" "User:bar"
runsuccess "baz" ""        "-f passwd" "User:baz"

# Now, run the tests that we expect to fail.
runfailure "foo" "barpass" "-f passwd" \
    "ckpasswd: invalid password for user foo"
runfailure "who" "foopass" "-f passwd" \
    "ckpasswd: user who unknown"
runfailure "" "foopass" "-f passwd" \
    "ckpasswd: null username"
