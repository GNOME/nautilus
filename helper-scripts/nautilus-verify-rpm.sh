#!/bin/sh

# This is a script meant for RPM based systems.
#
# It verifies that the currently installed Nautilus
# RPM has the correct version of Bonobo installed.
#
# We will probably add more sanity checks here
# as we learn of specific problems that break the
# Nautilus RPM.
#

# This script is meant to be call by the script that
# launched Nautilus.  Currently that is run-nautilus.
#
# The result value of this script is:
#
# 0: Nautilus launch may continue.  This happens if
#    one of the following occured:
#
#    a.  No error was detected
#    b.  An unknown error was detected but the user
#        chose to continue.
#    c.  This is not an RPM based system.
#    d.  Either Bonobo or Nautilus RPMS were not
#        found in the system.  This is a special
#        case which occurs in debug builds.  
#        Lots of people use run-nautilus with
#        with debug builds, so we dont want to 
#        break that for them.  Its possible we
#        might handle this case differently in
#        the future.  For example, we could have
#        separate launch scripts for debug and 
#        rpm installations of Nautilus.
# 
# 1: Nautilus launch should be aborted.  An error
#    known to break Nautilus was detected.
#

# Check for RPM bases systems only
if [ ! -f /etc/redhat-release ]
then
    exit 0
fi

# check for nautilus
rpm -q nautilus > /dev/null 2>&1
if [ $? -ne 0 ]
then
    exit 0
fi

# check for bonobo
rpm -q bonobo > /dev/null 2>&1
if [ $? -ne 0 ]
then
    exit 0
fi

# Verify the nautilus rpm.  The idea here is to detect
# whether the Nautilus rpm has been broken by something
# else.  One possibility is a forced installed of a newer
# bonobo.
log=/tmp/run-nautilus-log-$$
rm -f $log
rpm --verify nautilus > $log 2>&1

if [ $? -eq 0 ]
then
    rm -f $log
    exit 0
fi

grep "Unsatisfied dependencies" $log | grep bonobo > /dev/null 2>&1

if [ $? -eq 0 ]
then
    bonobo_version=`rpm -qi bonobo | grep "Version" | awk '{ print $3; }'`

    if [ "$bonobo_version" != "0.26" ]
    then
	title="Problem Running Nautilus"

	message=`printf "This version of Nautilus requires Bonobo 0.26.  This computer has Bonobo version %s installed.  There might be a newer version of Nautilus available that will work with this version of Bonobo.

Please check our download site at http://www.eazel.com/download" $bonobo_version`

	nautilus-error-dialog --message "$message" --title "$title" 
	exit 1
    fi
fi

title="Problem Running Nautilus"
button_one="Yes"
button_two="Cancel"

message="Nautilus or some library it uses is damaged or missing. It might work, but more
likely it will not. You could try to reinstall Nautilus from:
http://www.eazel.com/download.

Do you want to try to run Nautilus anyway?
"

nautilus-error-dialog --message "$message" --title "$title" --button-one $button_one --button-two $button_two
rv=$?

if [ $rv -eq 0 ]
then
    exit 0
fi

exit 1

