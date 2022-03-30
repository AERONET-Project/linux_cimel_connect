<img align="right" width="100" height="100" src="https://cdn.iconscout.com/icon/free/png-256/linux-8-202409.png"><br/>


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
## Downloading and compiling the software ##
In the top right you can select the drop down Code and select, _download zip_ and put this data on a USB drive to port to your Linux machine

OR

From your linux machine run this command with git installed on this system,

```bash
sudo git clone https://github.com/anthony-larosa/linux_cimel_connect
```
You can compile the software as follows,

**&#9723; Model 5 &#9723;**
```bash
cc -o model5_connect_silent model5_connect_silent.c model5_port.c -lm -lcurl
```

**&#x1F536; Model T &#x1F536;**
```bash
cc -o modelT_connect_silent modelT_connect_silent.c modelT_port.c -lm -lcurl 
```
## Running the Software ##
The program will create backup files at the directory `$HOME/backup`, therefore a backup directory must exist prior to starting the program.
```bash
mkdir $HOME/backup
```

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

You can specify a different backup directory by starting the program as:
```bash
./modelT_connect_silent USB0  dir=BACKUP_NAME
```

Where `BACKUP_NAME` is the full path to other directory. 

## Questions ##

Please send an email to anthony.d.larosa@nasa.gov
