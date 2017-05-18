#!/bin/bash

set -e

git pull

PROJECTS=". experiment importers/caffe importers/keras"

for proj in $PROJECTS; do
    pushd $proj
    lein deploy clojars
    popd
done
