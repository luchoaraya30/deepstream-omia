# deepstream-omia

## Install libconfig++
```
wget https://hyperrealm.github.io/libconfig/dist/libconfig-1.7.3.tar.gz
tar -xzvf libconfig-1.7.3.tar.gz
cd libconfig-1.7.3
./configure
make
make check
make install
cd ../lib
cp /usr/local/lib/libconfig++.so.11 .
```

## Compile nvmsgconv
From  folder *deepstream-5.1/*
```
cd sources/libs/small_payload_test/
make install
```
## Compile app
From folder *deepstream-5.1/*
```
cd sources/apps/sample_apps/small_payload/
export CUDA_VER=11.1
make
```

