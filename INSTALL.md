# Install instructions

## Compile from source

```
$ mkdir build
$ cd build
$ cmake ../ # add -DBUILD_STATIC=ON to build a static version
            # add -DENABLE_UDEV=OFF to disable udev support and -d /dev/xxx
$ cmake --build .
or
$ make -j$(nproc)
```
