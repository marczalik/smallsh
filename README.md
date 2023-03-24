# Smallsh
### Author: Marc Zalik

### QUICK INSTRUCTIONS:

Run 'make all' and use the smallsh executable created in the present directory.

### FULL INSTRUCTIONS:

You can compile Smallsh using the provided Make commands:

    - make all
    - make release
    - make debug

- make all will build both release and debug builds and store them in their respective directories, as well store as a copy of the release build in the present directory.

- make release will build a copy of the release build in the release directory.

- make debug will build a copy of the debug build in the debug directory. Note that this is NOT to be used for general usage or for grading. This is for debugging only, and will not work as intended if used in any other way.


Alternatively, you may build the source object files and link them directly using the following commands:

    - gcc -std=gnu99 -Wall -Wextra -Wpedantic -Werror UTL_smallsh.c -o UTL_smallsh.o -c
    - gcc -std=gnu99 -Wall -Wextra -Wpedantic -Werror smallsh.c -o smallsh.o -c
    - gcc -std=gnu99 -o smallsh smallsh.o UTL_smallsh.o
