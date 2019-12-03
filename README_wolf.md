# IPOP-VPN with wolfSSL

Below describes the steps for building IPOP-VPN with wolfSSL.

## wolfSSL

```sh
git clone https://github.com/wolfSSL/wolfssl.git
cd wolfssl
./autogen.sh
./configure --enable-opensslall --enable-keygen --enable-rsapss --enable-aesccm \
    --enable-aesctr --enable-des3 --enable-camellia --enable-curve25519 --enable-ed25519 \
    --enable-dtls --enable-certgen --enable-certreq --enable-certext --enable-tlsv10 \
    CFLAGS="-DWOLFSSL_PUBLIC_MP -DWOLFSSL_DES_ECB"
make
make check
sudo make install
```

## Building IPOP-VPN TinCan

Build Instructions based on:
http://ipop-project.org/wiki/Build-IPOP-for-Ubuntu,-Raspberry-Pi-3-and-Raspberry-Pi-Zero

```sh
mkdir ipop-vpn
cd ipop-vpn

# To clone latest master
git clone https://github.com/dgarske/Tincan
git clone https://github.com/ipop-project/Controllers

# To clone Latest Stable Release
git clone -b bh1 --single-branch https://github.com/dgarske/Tincan
git clone -b bh1 --single-branch https://github.com/ipop-project/Controllers

# Get Ubuntu external 3rd-Party-Libs
git clone -b ubuntu-x64 --single-branch https://github.com/ipop-project/3rd-Party-Libs.git Tincan/external/3rd-Party-Libs

# Remove libboringssl.a and libboringssl_asm.a
rm Tincan/external/3rd-Party-Libs/debug/libboringssl*
rm Tincan/external/3rd-Party-Libs/release/libboringssl*
```

### Build WebRTC with wolfSSL

Based on instructions:
* https://github.com/ipop-project/ipop-project.github.io/wiki/Build-WebRTC-Libraries-for-Linux
* https://webrtc.org/native-code/development/

```sh
# from ipop-vpn dir
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH=`pwd`/depot_tools:"$PATH"
mkdir webrtc-checkout
cd webrtc-checkout
fetch --nohooks webrtc
cd src

# See releases
git branch -r

git checkout branch-heads/57
gclient sync # NOTE: this takes a long time
git rebase-update
gclient sync

git remote add wolf https://github.com/dgarske/webrtc.git
git fetch wolf
git checkout wolf_57

gn clean out/debug
gn gen out/debug "--args=enable_iterator_debugging=false is_component_build=false rtc_build_wolfssl=true rtc_build_ssl=false rtc_ssl_root=\"/usr/local/include\""
ninja -C out/debug/ -t clean
ninja -C out/debug/ protobuf_lite p2p base jsoncpp

gn clean out/release
gn gen out/release "--args=enable_iterator_debugging=false is_component_build=false is_debug=false rtc_build_wolfssl=true rtc_build_ssl=false rtc_ssl_root=\"/usr/local/include\""
ninja -C out/release/ -t clean
ninja -C out/release/ protobuf_lite p2p base jsoncpp
```

Note: Git may complain about existing files when switching to wolf_57 branch. To resolve delete the conflicting directories:

```sh
rm -rf gmock/include/gmock
rm -rf gtest/include/gtest
```

#### Testing WebRTC

```sh
cd webrtc/examples/
gn gen out/debug
ninja -C out/debug/ -t clean
ninja -C out/debug/

cd out/debug
./peerconnection_server &
./peerconnection_clients
```


### Copy the WebRTC Libraries to TinCan
These freshly built libraries will replace the existing libraries.
Currently, the libraries we need from out/Debug_x64 and out/Release_x64 are:

```sh
cd ipop-vpn

# Debug
ar -rcs Tincan/external/3rd-Party-Libs/debug/libjsoncpp.a webrtc-checkout/src/out/debug/obj/third_party/jsoncpp/jsoncpp/json_reader.o webrtc-checkout/src/out/debug/obj/third_party/jsoncpp/jsoncpp/json_value.o webrtc-checkout/src/out/debug/obj/third_party/jsoncpp/jsoncpp/json_writer.o

cp webrtc-checkout/src/out/debug/obj/webrtc/p2p/librtc_p2p.a Tincan/external/3rd-Party-Libs/debug
cp webrtc-checkout/src/out/debug/obj/webrtc/p2p/libstunprober.a Tincan/external/3rd-Party-Libs/debug
cp webrtc-checkout/src/out/debug/obj/webrtc/base/librtc_base_approved.a Tincan/external/3rd-Party-Libs/debug
cp webrtc-checkout/src/out/debug/obj/webrtc/base/librtc_task_queue.a Tincan/external/3rd-Party-Libs/debug
cp webrtc-checkout/src/out/debug/obj/webrtc/base/librtc_base.a Tincan/external/3rd-Party-Libs/debug
cp webrtc-checkout/src/out/debug/obj/webrtc/libwebrtc_common.a Tincan/external/3rd-Party-Libs/debug
cp webrtc-checkout/src/out/debug/obj/base/third_party/libevent/libevent.a Tincan/external/3rd-Party-Libs/debug
cp webrtc-checkout/src/out/debug/obj/third_party/protobuf/libprotobuf_lite.a Tincan/external/3rd-Party-Libs/debug

# Release
ar -rcs Tincan/external/3rd-Party-Libs/release/libjsoncpp.a webrtc-checkout/src/out/release/obj/third_party/jsoncpp/jsoncpp/json_reader.o webrtc-checkout/src/out/release/obj/third_party/jsoncpp/jsoncpp/json_value.o webrtc-checkout/src/out/release/obj/third_party/jsoncpp/jsoncpp/json_writer.o

cp webrtc-checkout/src/out/release/obj/webrtc/p2p/librtc_p2p.a Tincan/external/3rd-Party-Libs/release
cp webrtc-checkout/src/out/release/obj/webrtc/p2p/libstunprober.a Tincan/external/3rd-Party-Libs/release
cp webrtc-checkout/src/out/release/obj/webrtc/base/librtc_base_approved.a Tincan/external/3rd-Party-Libs/release
cp webrtc-checkout/src/out/release/obj/webrtc/base/librtc_task_queue.a Tincan/external/3rd-Party-Libs/release
cp webrtc-checkout/src/out/release/obj/webrtc/base/librtc_base.a Tincan/external/3rd-Party-Libs/release
cp webrtc-checkout/src/out/release/obj/webrtc/libwebrtc_common.a Tincan/external/3rd-Party-Libs/release
cp webrtc-checkout/src/out/release/obj/base/third_party/libevent/libevent.a Tincan/external/3rd-Party-Libs/release
cp webrtc-checkout/src/out/release/obj/third_party/protobuf/libprotobuf_lite.a Tincan/external/3rd-Party-Libs/release
```

### Build IPOP-VPN TinCan

```sh
# Build
cd Tincan
git checkout wolf

cd trunk/build/
make DEBUG=1 WOLF_DIR=/usr/local
cp -f ../out/debug/$(uname -m)/ipop-tincan ../../../../ipop-vpn/

make WOLF_DIR=/usr/local
cp -f ../out/release/$(uname -m)/ipop-tincan ../../../../ipop-vpn/

cd ../../../Controllers
cp -rf ./controller/ ../../ipop-vpn/

# Return to ipop-vpn dir
cd ../
```

You will need a valid configuration file (ipop-config.json) in ipop-vpn/config directory to run IPOP. You may find a sample config file in the Controllers repo root, which you have already cloned into your machine.

Setting up a sample configuration:

```sh
mkdir config
cd config
wget https://github.com/ipop-project/Controllers/raw/master/controller/template-config.json
mv template-config.json ipop-config.json
```


## IPOP-VPN Testing

Testing Guide:
http://ipop-project.org/wiki/IPOP-Scale-test-Walkthrough

```sh
git clone https://github.com/ipop-project/Release-Management
cd ./Release-Management/Test/ipop-scale-test
./scale_test.sh

Select vpn mode to test. Please input 1 for classic or 2 for switch.
1
Enter from the following options:
    install-support-serv           : install critical services used in both, classic and switch modes
    prep-def-container             : prepare default container (what goes in depends on the mode)
    containers-create              : create and start containers
    containers-update              : restart containers adding IPOP src changes
    containers-start               : start stopped containers
    containers-stop                : stop containers
    containers-del                 : delete containers
    ipop-start                     : to start IPOP processes
    ipop-stop                      : to stop IPOP processes
    ipop-tests                     : open scale test shell to test ipop
    ipop-status                    : show statuses of IPOP processes
    visualizer-start               : install and start up visualizer
    visualizer-stop                : stop visualizer processes
    visualizer-status              : show statuses of visualizer processes
    logs                           : aggregate ipop logs under ./logs
    mode                           : show or change ipop mode to test
    help                           : show this menu
    quit                           : quit

> install-support-serv
... Installs packages

> prep-def-container

> containers-create
10
```


## Support

For questions or issue please email support@wolfssl.com
