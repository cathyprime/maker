#!/usr/bin/env sh
# vim: ft=sh

c++ -DMAKER_TEST -o tests tests.cc && \
./tests $@
