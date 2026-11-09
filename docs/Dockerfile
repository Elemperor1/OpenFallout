FROM python:3.15-slim-bookworm
ENV HOME="/home/docs"
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential ca-certificates curl git libreadline-dev unzip wget \
    && rm -rf /var/lib/apt/lists/* \
    && mkdir -p /home/docs \
    && touch /home/docs/omw_luadoc_docker

COPY requirements.txt ./
RUN pip3 install -r requirements.txt

COPY source/install_luadocumentor_in_docker.sh ./
RUN bash ./install_luadocumentor_in_docker.sh
ENV PATH="/home/docs/luarocks/bin:/home/docs/lua-5.1.5/src:${PATH}"

VOLUME /openfallout
