# sclang websocket

WebSocket support for SuperCollider using [boost beast](https://www.boost.org/doc/libs/latest/libs/beast/doc/html/index.html) with the new [gluon interface](https://github.com/supercollider/supercollider/pull/7189) for sclang.

A good introduction to Boost Beast by its author can be found on [YouTube](https://www.youtube.com/watch?v=7FQwAjELMek).

## Building

Currently the gluon interface is still under development and is not merged in the development branch of SuperCollider.
Therefore it is necessary to obtain the SuperCollider source code from the PR via

```shell
git clone -b topic/ffi https://github.com/JordanHendersonMusic/supercollider.git supercollider-ffi
```

Afterwards clone the code of this repo via

```shell
git clone https://github.com/capital-G/sclang_websocket.git
```

and build the project via where `DSC_SRC_PATH` should be replaced with the ffi fork from above.

```shell
cd sclang_websocket

cmake \
    -S . \
    -B build \
    -DSC_SRC_PATH=/Users/scheiba/github/supercollider \
    -DCMAKE_INSTALL_PREFIX=./install
cmake --build build --config Debug 
cmake --install build --config Debug
```

Copy the folder within the `install` folder to `Platform.userExtensionDir` and restart the interpreter.

## License

GPL-3.0
