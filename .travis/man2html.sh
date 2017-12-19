#!/bin/bash

# If man had changed, rebuild its HTML and push to gh-pages

set -e

[ "$TRAVIS" = true ] || exit 0
[ "$TRAVIS_PULL_REQUEST" = false ] || exit 0
[ "$GH_PASSWORD" ] || exit 12

parent=$(git show --pretty=raw HEAD | awk '/^parent /{ print $2; exit }')
head=$(git rev-parse HEAD)

if ! git diff --name-only "$parent" "$head" | grep -Pq "doc/.*\.[1-8]$"; then
    exit 0
fi

git clone -b gh-pages --depth 3 "https://kernc:$GH_PASSWORD@github.com/$TRAVIS_REPO_SLUG.git" gh-pages
groff -wall -mandoc -Thtml doc/xsuspender.1 > gh-pages/xsuspender.1.html
cd gh-pages
git commit -m "CI: Update xsuspender.1.html from $head" -- xsuspender.1.html
git push
