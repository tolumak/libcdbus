libcdbus
========

libcdbus is a library that can be used to generate light weight D-Bus bindings for
C programs. 

Build the test app
==================

You need cmake 2.6+ and python 3.2+.

cd libcdbus
mkdir build
cd build
cmake -DBUILD_TEST_APP=yes -DUSE_DBUS_SYSTEM_BUS=no ..
make

Launch the service:
./test-service

And try to send a message to it:
../test-client.py

You can introspect the service thanks to qdbusviewer from the Qt packages
The test service is called fr.sise.test

How-to use the library and generate bindings
============================================

Documentation should be written soon.

By now, just read test.c, test_introspect.xml and CMakeLists.txt and guess how it works... Sorry

