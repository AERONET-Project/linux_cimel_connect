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

*Model T instruments*
```bash
cc -o modelT_connect_silent modelT_connect_silent.c modelT_port.c -lm -lcurl 
```
