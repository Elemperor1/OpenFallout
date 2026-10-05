# Build OpenFallout using Docker

## Build Docker image

Replace `LINUX_VERSION` with the Linux distribution you wish to use.
```
docker build -f Dockerfile.LINUX_VERSION -t openfallout.LINUX_VERSION .
```

## Build OpenFallout using Docker

Labeling systems like SELinux require that proper labels are placed on volume content mounted into a container.
Without a label, the security system might prevent the processes running inside the container from using the content.
The Z option tells Docker to label the content with a private unshared label.
```
docker run -v /path/to/openfallout:/openfallout:Z -e NPROC=2 -it openfallout.LINUX_VERSION
```
