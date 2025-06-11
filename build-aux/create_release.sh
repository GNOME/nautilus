#!/bin/bash

# Pass the release version as a parameter to the script

version="${1}"
current_branch="$(git rev-parse --abbrev-ref HEAD)"
release_version="$(grep \" version: \" meson.build | cut -d":" -f2)"
echo ${release_version}

# Check that input is sane version number
#if [[ "$version" =~ ^[0-9]\+.[0-9]+(\.[0-9]+|)$ ]]; then
if [[ ! "$release_version" =~ ^[0-9]+\.[0-9]+(\.[0-9])?$ ]]; then
	echo "Invalid version set! Detected ${release_version}"
fi

next_version=$release_version

exit


commit=$(git rev-parse HEAD)

git tag -s ${release_version} -m (git show -s --format='%s' ${commit}) ${commit}
git push --tags
