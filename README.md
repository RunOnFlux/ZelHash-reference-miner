# ZelCash ZelHash (Based on Equihash 125/4) OpenCL Miner


# Usage
## One Liner Examples
```
Linux: ./zelhash-opencl-miner --server <hostName>:<portNumer> --user <walletAddr or userName> --devices <deviceList>
Windows: ./zelhash-opencl-miner.exe --server <hostName>:<portNumer> --user <walletAddr or userName>  --devices <deviceList>
```

## Parameters
### --server 
Passes the address and port of the pool the miner will mine on to the miner.
The pool address can be an IP or any other valid server address.

### --user
This parameter is used to pass the wallet to mine on or the pool user name to the miner.

### --pass (Optional)
If the pool requires a password to the username, this can be used to pass it to the miner.

### --devices (Optional)
Selects the devices to mine on. If not specified the miner will run on all devices found on the system. 
For example to mine on GPUs 0,1 and 3 but not number 2 use --devices 0,1,3
To list all devices that are present in the system and get their order start the miner with --devices -2 .
Then all devices will be listed, but none selected for mining. The miner closes when no devices were 
selected for mining or all selected miner fail in the compatibility check.

# How to build
## Windows
1. Install Visual Studio >= 2017 with CMake support.
1. Download and install Boost prebuilt binaries https://sourceforge.net/projects/boost/files/boost-binaries/1.68.0/boost_1_68_0-msvc-14.1-64.exe, also add `BOOST_ROOT` to the _Environment Variables_.
1. Download and install CUDA Toolkit https://developer.nvidia.com/cuda-downloads
1. Add `.../boost_1_68_0/lib64-msvc-14.1` to the _System Path_.
1. Open project folder in Visual Studio, select your target (`Release-x64` for example, if you downloaded 64bit Boost and OpenSSL) and select `CMake -> Build All`.
1. Go to `CMake -> Cache -> Open Cache Folder -> zel-opencl-miner` (you'll find `zel-opencl-miner.exe`).

## Linux (Ubuntu 16.04)
1. Install `gcc7` `boost` packages.
```
  sudo add-apt-repository ppa:ubuntu-toolchain-r/test
  sudo apt update
  sudo apt install g++-7 libboost-all-dev -y
```
2. Set it up so the symbolic links `gcc`, `g++` point to the newer version:
```
  sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 60 \
                           --slave /usr/bin/g++ g++ /usr/bin/g++-7 
  sudo update-alternatives --config gcc
  gcc --version
  g++ --version
```
3. Install OpenCL
```
  sudo apt install ocl-icd-* opencl-headers
```
4. Install latest CMake 
```
  wget "https://cmake.org/files/v3.12/cmake-3.12.0-Linux-x86_64.sh"
  sudo sh cmake-3.12.0-Linux-x86_64.sh --skip-license --prefix=/usr
```
5. Go to zel-opencl-miner project folder and call `cmake -DCMAKE_BUILD_TYPE=Release . && make -j4`.
6. You'll find _zel-opencl-miner_ binary in `bin` folder.
