DISCONTINUATION OF PROJECT

This project will no longer be maintained by Intel.

Intel has ceased development and contributions including, but not limited to, maintenance, bug fixes, new releases, or updates, to this project.  

Intel no longer accepts patches to this project.

If you have an ongoing need to use this project, are interested in independently developing it, or would like to maintain patches for the open source software community, please create your own fork of this project.  
DISCONTINUATION OF PROJECT

This project will no longer be maintained by Intel.

Intel has ceased development and contributions including, but not limited to, maintenance, bug fixes, new releases, or updates, to this project.  

Intel no longer accepts patches to this project.

If you have an ongoing need to use this project, are interested in independently developing it, or would like to maintain patches for the open source software community, please create your own fork of this project.  
# DirtyCoW /proc/self/mem vDSO Container Escape

## Setup

This is intended to be run on a Ubuntu 16.04.1 host with no updates applied. If
it has been updated it will no longer contain an exploitable kernel. The
setup.sh script should install the necessary packages, including docker, clear
containers, and an unpatched kernel.

```
sudo ./setup.sh
```

## Building

If you are running from behind a proxy server you will need to add the proxy
environment variables to the Dockerfile.

```
make
docker build -t proc .
# To exploit and escape run in the docker default, runc, runtime.
docker run --rm -ti --runtime=runc proc
# To exploit and be prevented from escape to host run with Clear Containers
docker run --rm -ti --runtime=cor proc
```

If you want to see the quick demo then add `demo` to the end of the docker run
commands. `docker run --rm -ti --runtime=cor proc demo`.

### Legal

> This software is subject to the U.S. Export Administration Regulations and
> other U.S. law, and may not be exported or re-exported to certain countries
> (Cuba, Iran, North Korea, Sudan, and Syria) or to persons or entities
> prohibited from receiving U.S. exports (including Denied Parties, Specially
> Designated Nationals, and entities on the Bureau of Export Administration
> Entity List or involved with missile technology or nuclear, chemical or
> biological weapons).
