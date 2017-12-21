#!/bin/bash
dd if=/dev/urandom of=test1 bs=50K count=1
dd if=/dev/urandom of=test2 bs=100K count=1
dd if=/dev/urandom of=test3 bs=1M count=1
dd if=/dev/urandom of=test4 bs=5M count=1
