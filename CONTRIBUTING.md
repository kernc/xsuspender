Hacking
=======

Building
--------

To enable debugging closer to the metal, compile a debugging-enabled
binary by passing `-DCMAKE_BUILD_TYPE=Debug` on the cmake line.


### Building for Debian

To prepare Debian/APT source and binary packages, the following steps work:

    release_tag=1.2

    cd "$(mktemp -d)"
    wget "https://github.com/kernc/xsuspender/archive/${release_tag}.tar.gz"
    mv "${release_tag}.tar.gz" "xsuspender_${release_tag}.orig.tar.gz"
    tar xf "xsuspender_${release_tag}.orig.tar.gz"

    wget "https://github.com/kernc/xsuspender/archive/debian.tar.gz"
    tar xf debian.tar.gz -C "xsuspender-${release_tag}" --strip-components=1

    cd "xsuspender-${release_tag}"
    debuild --no-sign

    cat ../xsuspender_*.dsc


Running
-------

Check for leaks with:

    G_SLICE=always-malloc \
    G_DEBUG=gc-friendly   \
    G_MESSAGES_DEBUG=xsuspender  \
      valgrind --leak-check=full --show-leak-kinds=definite xsuspender
