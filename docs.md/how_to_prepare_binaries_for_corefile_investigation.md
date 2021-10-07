# How to prepare binaries for corefile investigation

1. Create CMake build with **RelWithDebInfo** configuration (gcc options: ```-O2 -g -DNDEBUG```)

```bash
mkdir bin
cd bin
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSERVER_CLIB_BUILD_EXAMPLES:BOOL=ON ..
make
```

2. Move debug info to seprate file. **Fo instance if you application name _any_service_**

```bash
APP=any_service
cp -f $APP $APP.debug
strip --only-keep-debug $APP.debug
objcopy --strip-debug $APP
```

These commands must go in folder with binary. They remove debug symbols from _any_service_
to depoy it and save _any_service.debug_ to store as artifact

3. To load corefile in GDB:

```bash
gdb any_service -c core
```

_any_service_ and _any_service.debug_ must be in the same directory

* _core_ is tipical name for corefile. Additional info about corefile creation you can read in documentation of certain system service. For *Apport* (one of common) [here](https://wiki.ubuntu.com/Apport)

4. To load corefile with IDE sometimes you need in this command before:


```bash
objcopy --add-gnu-debuglink any_service.debug any_service
```


> (!) Don't forget about conformity between binary, debug symbols (_any_service.debug_ - at above example), corefile and source code
