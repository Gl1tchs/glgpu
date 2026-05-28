#!/bin/bash

# Usage: ./bump_version.sh 1.2.3
NEW_VERSION=$1

if [ -z "$NEW_VERSION" ]; then
    echo "Usage: ./bump_version.sh <major.minor.patch>"
    exit 1
fi

# Update CMakeLists.txt
# This searches for the project() line and replaces the version number
if [[ "$OSTYPE" == "darwin"* ]]; then
    sed -i '' "/project(/,/)/ s/VERSION [0-9.]*/VERSION $NEW_VERSION/" CMakeLists.txt
else
    sed -i "/project(/,/)/ s/VERSION [0-9.]*/VERSION $NEW_VERSION/" CMakeLists.txt
fi

echo "Updated CMakeLists.txt to version $NEW_VERSION"

# Git Workflow
git add CMakeLists.txt
git commit -m "chore: bump version to $NEW_VERSION"
git tag -a "v$NEW_VERSION" -m "Version $NEW_VERSION"

echo "Created git commit and tag v$NEW_VERSION"

# Push to Remote
read -p "Push to origin? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    git push origin master --tags
fi

echo "Done!"
