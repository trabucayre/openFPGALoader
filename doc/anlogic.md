# Anlogic notes

## Sipeed Lichee Tang

For this target, *openFPGALoader* support *svf* and *bit*

__bit file load (memory)__

```bash
openFPGALoader -m -b licheeTang /somewhere/project/prj/*.bit
```

Since *-m* is the default, this argument is optional

__bit file load (spi flash)__

```bash
openFPGALoader -f -b licheeTang /somewhere/project/prj/*.bit
```

__svf file load__

It's possible to produce this file by using *TD*:
* Tools->Device Chain
* Add your bit file
* Option : Create svf

or by using [prjtang project](https://github.com/mmicko/prjtang)

```bash
mkdir build
cd build
cmake ../
make
```

Now a file called *tangbit* is present in current directory and has to be used as
follow:
```bash
tangbit --input /somewhere.bit --svf bitstream.svf
```

```bash
openFPGALoader -b licheeTang /somewhere/*.svf
```