mbed-nrf51-gcc-test
===================

Test repository for compiling an mbed example project for NRF51 using gcc

Following the discussion in https://mbed.org/forum/mbed/topic/5050/

Run the following commands to pull the sub-repos:
```
git submodule init
git submodule update
```

To attempt to compile, ensure that https://launchpad.net/gcc-arm-embedded is in your path, then:
```
cd projects/BLE_HeartRate
cmake .
make
```
