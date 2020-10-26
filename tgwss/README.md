# **TGWSS: WebRTC signaling server based on websockets protocol**

## Building
1. Install required tools and utilities
    ```bash
   sudo apt update
   sudo apt upgrade -y
   sudo apt install -y git g++ pkg-config cmake libssl-dev
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
3. Install `libfmt` library (6.1.2)
    ```bash
    git clone https://github.com/fmtlib/fmt.git
    cd ./fmt
    git checkout tags/6.1.2
    mkdir build-release
    cd build-release
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-fPIC ../
    make -j 8
    sudo make install
    cd ../../
    rm -rf ./fmt
    ```
4. Instal `rapidjson` library:
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
5. Build `tgwss`
    ```bash
    cd src/tgwss
    sudo patch /usr/local/lib/cmake/libwebsockets/LibwebsocketsConfig.cmake < ./patch/lws_static.patch
    mkdir build-release
    cd build-release
    cmake -DCMAKE_BUILD_TYPE=Release ../
    make -j 8
    ```
 
## Configuration
`tgwss` configuration file:
```
    {
        "network": {
            "bind_port": 8080,
            "conn_limit": 4,
            "io_timeout": 30,
            "ssl": true,
            "cert_file": "/etc/tgwss/cert.pem",
            "pkey_file": "/etc/tgwss/pkey.pem"
        },    
        "log": {
            "destination": "/var/log/tgwss.log",
            "level": "notice"
        }
    }
```

- `network.bind_port`: listen on port (all interfaces)
- `network.conn_limit`: max number of incoming connections
- `network.io_timeout`: max connections inactivity timeout (sec)
- `network.ssl`: `true` to use secure connection (SSl/TLS)
- `network.cert_file`: certificate file location
- `network.pkey_file`: private key file location
- `log.destination`: "console" - output logging information on console, "syslog" - output logging information to syslog, "some_file_name" - output logging information to file with name "some_file_name"
- `log.level`: logging level, one of the following: "debug", "info", "notice", "warning", "error", "critical"
