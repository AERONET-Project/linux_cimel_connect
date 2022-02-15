# linux_cimel_connect
Linux software to retrieve data from a Cimel sun photometer and upload to NASA servers for processing.

## About ##
This software package can be used for Model 5 and Model T Cimel sun photometers, and their accompanying versions. It will automatically download data from the respective instrument and upload it to NASA for processing.

## Requirements ##
This software has been tested on Raspberry Pi's and similar single board Debian Linux CPUs. It is written in C, making it portable to hopefully most Linux flavors. Some pre-installed libraries should be updated, which is outlined below.

## Getting Started ##
Please update your systems cURL development environment, this library includes the OpenSSL packages,
```bash
sudo apt-get install -y libcurl4-openssl-dev
```

Set your time zone to UTC,
```bash
sudo timedatectl set-timezone Etc/UTC
```

You can compile the software as follows,

*Model 5 instruments*
```bash
cc -o model5_connect_silent model5_connect_silent.c model5_port.c -lm -lcurl
```

[![](https://img.shields.io/badge/Crates.io-orange?style=for-the-badge)](https://crates.io/crates/redant)
```bash
cc -o modelT_connect_silent modelT_connect_silent.c modelT_port.c -lm -lcurl 
```
## Running the Software ##
To run the software you will need to specify the port the Cimel is connected to on your Linux device. This is done by identifying the device path. To determine the device path utiize the following commands:
```bash
ls /dev/ttyUSB* && ls /dev /ttyS*
```
or
```bash
dmesg | grep serial
```
This will return a device path: /dev/ttyUSB0, /dev/ttyUSB1, etc.

For example if you are using a USB to Serial adapter you still start the program as
```bash
./modelT_connect_silent USB0 #For Model T
```
If you are using a UART connection,
```bash
./modelT_connect_silent S0 #For Model T
```
