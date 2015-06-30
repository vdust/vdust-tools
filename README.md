vdust's tools
=============

Set of unrelated utility scripts and small applications.
Some may be useful.
Others were wrote for fun.

Build C programs
----------------

This repository uses the [Waf build system](http://waf.io/ "Waf official website").
Full documentation is available on the website.

To configure:

    $ ./waf configure

You can change the installation path prefix (default to /usr/local/).

    $ ./waf configure --prefix=${HOME}/.local/

To build:

    $ ./waf

programs are built in _build/src/. To install them:

    $ ./waf install


