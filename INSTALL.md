# Install instructions

## Compile from source

```
$ mkdir build
$ cd build
$ cmake ../ # add -DBUILD_STATIC=ON to build a static version
$ cmake --build .
or
$ make -j$(nproc)
```
