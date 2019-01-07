#!/bin/bash

# If man had changed, rebuild its HTML and push to gh-pages

set -eu

[ "$GH_PASSWORD" ] || exit 12

head=$(git rev-parse HEAD)

git clone -b gh-pages "https://kernc:$GH_PASSWORD@github.com/$TRAVIS_REPO_SLUG.git" gh-pages
groff -wall -mandoc -Thtml doc/xsuspender.1 > gh-pages/xsuspender.1.html
cd gh-pages

ANALYTICS="<script>window.ga=window.ga||function(){(ga.q=ga.q||[]).push(arguments)};ga.l=+new Date;ga('create','UA-43663477-3','auto');ga('require','cleanUrlTracker',{indexFilename:'index.html',trailingSlash:'add'});ga('require','outboundLinkTracker',{events:['click','auxclick','contextmenu']});ga('require', 'maxScrollTracker');ga('require', 'pageVisibilityTracker');ga('send', 'pageview');setTimeout(function(){ga('send','event','pageview','view')},15000);</script><script async src='https://www.google-analytics.com/analytics.js'></script><script async src='https://cdnjs.cloudflare.com/ajax/libs/autotrack/2.4.1/autotrack.js'></script>"
sed -i "s#</body>#$ANALYTICS</body>#i" xsuspender.1.html

git add *
git diff --staged --quiet && echo "$0: No changes to commit." && exit 0
git commit -am "CI: Update xsuspender.1.html from $TRAVIS_TAG ($head)"
git push
