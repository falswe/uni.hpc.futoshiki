#!/bin/bash
find . -type f -name "*.sh.e*" -delete
find . -type f -name "*.sh.o*" -delete
find . -type f -name "*.o" -delete
find . -type f ! -name "*.*" -not \( -path '*/.*' \) -delete
find . -type f -name "*.o32*" -delete
find . -type f -name "*.e32*" -delete