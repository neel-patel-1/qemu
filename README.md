# DevteroFlex QEMU

This is the instrumented version of QEMU used by the the DevteroFlex simulator. DevteroFlex brings FPGA-accelerated simulation to the QFlex family.

DevteroFlex is composed of three main components: a modified version of QEMU, an instrumented ARM softcore (DevteroFlex) in FPGA, and a driver that handles the communication between QEMU and DevteroFlex. 

QEMU is written in C and can be developed on most Linux machines. DevteroFlex is written in Chisel, and while basic testing can be done on most Linux machines, fully simulating and synthesizing the softcore requires an extensive toolchain.

In the following section, we will describe the process to build QEMU for DevteroFlex

# Building QEMU

This guide assumes Ubuntu 20.04 platform.

To build QEMU, first download all the dependencies by executing the script:

```
$ bash scripts/qflex/install-deps.sh
```

Now, configure and build QEMU:
```
$ ./configure --target-list=aarch64-softmmu --enable-werror --disable-docs --disable-containers --enable-devteroflex
$ make -j6
```

After the build process is complete, you are ready to experiment with QEMU. 
We use git-lfs to store and share QEMU images. Download an image [here](https://github.com/parsa-epfl/images/tree/ubuntu20.04-aarch64) and give it a test!
