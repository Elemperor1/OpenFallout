#!/bin/bash

pushd $( dirname -- "$0"; )
docker run --user "$(id -u)":"$(id -g)" --volume "$PWD/..":/openfallout openfallout_doc \
    sphinx-build /openfallout/docs/source /openfallout/docs/build
popd
