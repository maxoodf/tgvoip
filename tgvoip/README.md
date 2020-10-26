# **TGWOIP: VoIP library and test tools based on WebRTC framework (M80 release version)**

## Building
1. Install required tools and utilities
    ```bash
   sudo apt update
   sudo apt upgrade -y
   sudo apt install -y git g++ pkg-config cmake libssl-dev libx11-dev
    ```
2. Install `libwebsockets` library (3.2.2)
    ```bash
    git clone https://github.com/warmcat/libwebsockets.git
    cd ./libwebsockets
    git checkout tags/v3.2.2
    mkdir build-release
    cd build-release
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS=-fPIC -DLWS_WITH_SHARED=OFF -DLWS_WITHOUT_TESTAPPS=ON -DLWS_WITHOUT_TEST_SERVER=ON -DLWS_WITHOUT_TEST_PING=ON -DLWS_WITHOUT_TEST_CLIENT=ON ../
    make -j 8
    sudo make install
    cd ../../
    rm -rf ./libwebsockets
    ```
3. Install the latest `rapidjson` library:
    ```bash
    git clone https://github.com/Tencent/rapidjson.git
    cd rapidjson
    mkdir build-release
    cd build-release
    cmake -DCMAKE_BUILD_TYPE=Release ../
    make -j 8
    sudo make install
    cd ../../
    rm -rf rapidjson            
    ```
4. Install `webrtc` library (m81 release version is provided with the submission)
    ```bash
   cd src/webrtc
   sudo cp -r ./include /usr/local/include/webrtc
   sudo cp ./lib/libwebrtc.a /usr/local/lib
    ```
5. Build `tgvoip`
    ```bash
    cd src/tgvoip
    sudo patch /usr/local/lib/cmake/libwebsockets/LibwebsocketsConfig.cmake < ./patch/lws_static.patch
    mkdir build-release
    cd build-release
    cmake -DCMAKE_BUILD_TYPE=Release ../
    make -j 8
    ```
