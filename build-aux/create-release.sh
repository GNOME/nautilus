#!/bin/bash
set -e

# Usage: create_release.sh [optional next version]

meson_file="meson.build"
metainfo_file="data/org.gnome.Nautilus.metainfo.xml.in.in"
news_file="NEWS"

### Version ###

meson_version=$(grep " version: '" ${meson_file} | cut -d\' -f2 | head -n 1)
release_version=${1:-"${meson_version}"}

current_branch=$(git rev-parse --abbrev-ref HEAD)
latest_commit=$(git rev-parse HEAD)
previous_tag=$(git describe --tags --abbrev=0)

# Calculate next version
IFS='.' read -r major minor patch <<< "${release_version}"
case "$minor" in
	"alpha")
		next_version="${major}.beta"
		;;
	"beta")
		next_version="${major}.rc"
		;;
	"rc")
		next_version="${major}.0"
		;;
	*)
		if [[ ! "$minor" =~ ^[0-9]+$ ]]; then
			echo "Error: Don't know how to handle version '${release_version}'"
			exit 1
		elif [[ ${current_branch} = "main" ]] ; then
			# if branch is main increase major number
			next_version="$((major+1)).alpha"
		else
			# otherwise increase minor
			next_version="${major}.$((minor + 1))"
		fi
		;;
esac

### Commits ###

bump_commit_msg="Post[\ -]release version bump"
translation_filter="^Update.*translation\$"
commits=$(git log \
	--pretty=format:"%h %s (%an)" \
	${previous_tag}..${latest_commit} \
	--invert-grep --grep="${bump_commit_msg}" --grep="${translation_filter}")
t10n_commits=$(git log --pretty=oneline \
	${previous_tag}..${latest_commit} \
	--grep="${translation_filter}")

num_commits=$(echo "${commits}" | sed '/^\s*$/d' | wc -l)
num_t10n_commits=$(echo "${t10n_commits}" | sed '/^\s*$/d' | wc -l)

meson_version_str=""
if [ ! ${release_version} = ${meson_version} ] ; then
	meson_version_str=" (meson has ${meson_version})"
fi
echo "Creating release ${release_version}${meson_version_str} (Bumping to ${next_version})"
echo "on top of commit ${latest_commit}"
echo ""
echo "There were ${num_commits} code change(s) and ${num_t10n_commits} translation change(s) since ${previous_tag}."
echo ""

### NEWS ###

header="Major Changes in ${release_version}"
underline=$(printf '%*s\n' "${#header}" '' | tr ' ' '=')
translations=""
if [ ! ${num_t10n_commits} = 0 ] ; then
	translations="* Translation updates (GNOME Translation Project contributors)"
fi
news_update="\
${header}
${underline}
* Enhancements:
- 

* Bugfixes:
- 

* Cleanups:
- 

${commits}

${translations}
"

echo "${news_update}" | cat - NEWS > temp && mv temp NEWS

echo "Edit the NEWS file then continue with [Enter]"
read

release_notes=$(git diff --unified=0 --color=never NEWS | grep -E '^\+[^+]' | sed 's/^\+//')

### metainfo.xml ###

date=$(date --iso-8601)
next_minor="${next_version##*.}"
type=$([[ "$next_minor" =~ ^[1-9][0-9]*$ ]] && echo "stable" || echo "development")
metainfo_release_version=$(echo ${release_version} | sed -E "s/\.([a-z]+)/~\1/")

snapshot="<release version=\"@release-version@\" type=\"snapshot\"\/>"
placeholder="<!--NEXT_RELEASE_PLACEHOLDER-->"
new_entry="<release version=\"${metainfo_release_version}\" type=\"${type}\" date=\"${date}\"\/>"
replace="s/${snapshot}/${placeholder}\n    ${new_entry}/"
sed -i "${replace}" "${metainfo_file}"

### Release commit ###
release_message="Release ${release_version}"
git add "${metainfo_file}" "${news_file}"
git commit -m "${release_message}"
release_commit=$(git rev-parse HEAD)

### Post-Release bump commit ###
sed -ri "s/  version: [^,],/  version: '${next_version}',/" "${meson_file}"

replace="s/${placeholder}/${snapshot}/"
sed -i "${replace}" "${metainfo_file}"

git add "${metainfo_file}" "${meson_file}"
git commit -m "Post-release version bump"

### Note about release tagging ###
release_notes="${release_message}
${release_notes}"
echo Release commits created. Tag release with:
echo "git tag -s ${release_version} ${release_commit} -m \"${release_notes}\""
echo "git push --tags"
