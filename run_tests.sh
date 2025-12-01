#!/usr/bin/env sh
# vim: ft=sh

c++ -o tests tests.cc && \
./tests $@
