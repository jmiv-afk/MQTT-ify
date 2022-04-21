## MQTT-ify
### Jake's final project for Advanced Embedded Software Development, Spring 2022
### University of Colorado, Boulder

See the overall project [Wiki](https://github.com/cu-ecen-aeld/final-project-jmichael16/wiki/Project-Overview) for project details and [schedule](https://github.com/cu-ecen-aeld/final-project-jmichael16/wiki/Project-Overview). 

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
- TARGET_BUILD is unset, passwd.txt should be in the same folder that you are launching the executable from.
- TARGET_BUILD is set, passwd.txt is expected at `etc/mqttify/passwd.txt`. 

The passwd.txt file should contain the following contents:
```
<username>
<password>
```

#### Desktop application
I have provided a pipfile which outlines the dependencies. If you use `pipenv`, then you can simply navigate to the location you have cloned this git repository and:
```
cd ./desktop-gui
pipenv install 
pipenv shell
./mqttify-gui-app.py
```

If you don't use `pipenv`, you can install the dependencies system-wide with pip: `pip3 install paho-mqtt`. Then run with `./mqttify-gui-app.py`. 
