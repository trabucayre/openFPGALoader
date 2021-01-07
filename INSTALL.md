# Install instructions

## Get the code from git project

```
$ git clone https://github.com/trabucayre/openFPGALoader.git
```
## For FX2 cable support add the following steps

```
$ git clone https://github.com/makestuff/fpgalink.git
```

```
$ build.sh
```
Add the install/bin directory to the path (for building and running OpenFPGALoader)

## Compile from source

```
$ mkdir build
$ cd build
$ cmake ../ # add -DBUILD_STATIC=ON to build a static version
            # add -DENABLE_UDEV=OFF to disable udev support and -d /dev/xxx
            # add -DENABLE_FX2=OFF to disable FX2 cable support 
$ cmake --build .
or
$ make -j$(nproc)
```



## Install

As root:

```
$ make install
```
