## Tutorial
Created by following tutorial of:
- https://devblog.qnx.com/how-to-use-macos-to-develop-for-qnx-8-0/
- https://gitlab.com/qnx/projects/docker-mac
## Modifications
### Dockerfile
```dockerfile
# Download base image ubuntu 20.04
FROM ubuntu:22.04
```
- Change to Ubuntu 22.04 due to Python issues
### Wifi
To configure cellular hotspot from iPhone, I first changed my phone's name to a one word (this prevents issues from formatting of `'` and spacing).

I then modified the `wpa_supplicant.conf` file with my hotspot details.

After inserting the MicroSD card, I let the Pi boot up. I didn't know the IP address of the Pi, so I ssh'd into the next IP address after my Mac (since my Mac was connected to the hotspot first).
- i.e. if my Mac had an IP of `172.20.10.11`, I tried to ssh to `172.20.10.12`

May also be possible to find the Pi's address using [nmap](https://nmap.org/book/inst-macosx.html) or just plug Pi into a monitor which shows the IP address in bottom left.

## To Run
1. Install QNX extension on VSCode
1. Run the docker container using `./docker-create-container.sh` (may need to `chmod +x` to give execute permission)
1. Right click on `my-project` directory in VSCode and under `QNX`, select `Build Active Project`
1. After binary is created (file called `my-project`) can scp to the Pi
1. Run `scp ./my-project qnxuser@172.20.10.12:~`
1. Can then ssh into the Pi and run `./my_project`

## To Stop
1. If not logged in as root, `su root`
1. Run `shutdown -b`