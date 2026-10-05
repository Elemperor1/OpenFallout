#!/bin/bash

pushd $( dirname -- "$0"; )
docker build -t openfallout_doc .
popd
