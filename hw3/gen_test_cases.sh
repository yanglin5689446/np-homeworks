#!/bin/bash
dd if=/dev/urandom of=test1 bs=5K count=1
dd if=/dev/urandom of=test2 bs=1M count=1
dd if=/dev/urandom of=test3 bs=5M count=1
