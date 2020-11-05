# Install instructions

## Get the code from git project

```
$ git clone https://github.com/trabucayre/openFPGALoader.git
```

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

## Install

As root:

```
$ make install
```
