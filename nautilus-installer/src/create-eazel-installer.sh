#!/bin/sh

echo "* Packing"

cp eazel-installer hest
gzexe hest

echo "* Patching"

(
	echo "#!/bin/sh"

	# Curses to GNU grep and its quest to protect the world
	# from having to look at the innards of binary files.  If
	# it matches, tell me where!  Just watch, they'll make sed
	# detect binaries too and break this again.

	skip=$(sed -n '/skip=/s/skip=//p' hest)

	# And Red Hat 7's gzexe is broken and puts the wrong line
	# count in.  I'm going to hope that other versions do the
	# right thing rather than hardwiring more mysterious numbers.

	case "`cat /etc/redhat-release`" in
		*" 7.0 "*)
			skip=26
		;;
	esac

	extraskip=$(expr $skip + $(wc -l < prescript))
	echo "skip=$extraskip"

	cat prescript

	sed -e '1,2d' -e 's/set -C//' hest
) > eazel-installer.sh
