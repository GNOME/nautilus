#!/bin/sh

make -s test-nautilus-font && rm -f font_test.png && ./test-nautilus-font && eog font_test.png

