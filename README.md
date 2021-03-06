## MQTT-ify
### Jake's final project for Advanced Embedded Software Development, Spring 2022
### University of Colorado, Boulder

See the overall project [Wiki](https://github.com/cu-ecen-aeld/final-project-jmichael16/wiki/Project-Overview) for project details and [schedule](https://github.com/cu-ecen-aeld/final-project-jmichael16/wiki/Project-Overview). 

### Brief
[Custom Yocto image](https://github.com/cu-ecen-aeld/final-project-jmichael16/) on Raspberry Pi acts as an Edge device, communicating with legacy hardware over UART.  Tx/Rx communication is transmitted via MQTT pub/sub protocol to a cloud hosted broker. A seperate client, on a different network, running the Python GUI can transmit and receive messages from the legacy hardware. 

### Repository Contents
This repository contains two applications: 
- [Desktop GUI application](desktop-gui) written in Python3 using [Tkinter](https://docs.python.org/3/library/tkinter.html)
- [Embedded target](embedded-target) application written in C using [Paho](https://www.eclipse.org/paho/)


#### Embedded target application
Example usage of the mqttify program:
```
usage: mqttify [-f serial_port] [-d]
   -f, --file : a serial port device to read/write data to (required)
   -d, --daemon :  run this process as a daemon
example:
   mqttify --file /dev/ttyAMA0 --daemon
```

The application for the embedded target contains a Makefile which will build the executable. Presence or absence of the TARGET_BUILD flag will change the location that the program expects a `passwd.txt` file as follows:
- TARGET_BUILD is unset, passwd.txt is expected in the same directory that you are launching the executable from.
- TARGET_BUILD is set, passwd.txt is expected at `etc/mqttify/passwd.txt`. 

The passwd.txt file should contain the following contents:
```
<username>
<password>
```

The embedded target application is included as a part of this [custom Linux distribution](https://github.com/cu-ecen-aeld/final-project-jmichael16) which is built with Yocto for the Raspberry Pi 4B. Further details for the overall project are included in that repository. 

#### Desktop application
I have provided a pipfile which outlines the dependencies. If you use `pipenv`, then you can simply navigate to the location you have cloned this git repository and:
```
cd ./desktop-gui
pipenv install 
pipenv shell
./mqttify-gui-app.py
```

If you don't use `pipenv`, you can install the dependencies system-wide with pip: `pip3 install paho-mqtt`. Then run with `./mqttify-gui-app.py`. 
