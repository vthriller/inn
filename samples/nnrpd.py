# This module supplies stub Python functions corresponding to the ones
# provided by nnrpd.  It is not used by the server; it is only here so
# that you can test your auth and filter scripts before loading.

from types import *

def set_auth_hook(anObject):
    if type(anObject) == InstanceType:
        print "** set_auth_hook for " + repr(anObject)
    else:
        print "** <Your object is not a class instance.>"

def syslog(level, message):
    print "-- syslog level: %s message: %s" % (level, message)