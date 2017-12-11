Hacking
=======

Building
--------

To enable debugging closer to the metal, compile a debugging-enabled
binary by passing `-DCMAKE_BUILD_TYPE=Debug` on the cmake line.

Running
-------

Check for leaks with:

    G_SLICE=always-malloc \
    G_DEBUG=gc-friendly   \
    G_MESSAGES_DEBUG=all  \
      valgrind --leak-check=full --show-leak-kinds=definite xsuspender