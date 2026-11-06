#!/bin/bash
set -e

DOCS_SOURCE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
FILES_DIR=$DOCS_SOURCE_DIR/../../files
OUTPUT_DIR=$DOCS_SOURCE_DIR/reference/lua-scripting/generated_html
export PATH="$PATH:$HOME/luarocks/bin"

if ! command -v openmwluadocumentor &> /dev/null
then
    if [ -f /.dockerenv ] || [ -f /home/docs/omw_luadoc_docker ]; then
        bash "$DOCS_SOURCE_DIR/install_luadocumentor_in_docker.sh"
    fi
fi

luarocks_paths=$(luarocks path)
eval "$luarocks_paths"

if ! command -v openmwluadocumentor &> /dev/null
then
    echo "openmwluadocumentor not found; See README.md for installation instructions." >&2
    exit 1
fi

if ! compgen -G "$FILES_DIR/lua_api/openfallout/*lua" > /dev/null; then
    echo "No Lua API files found matching '$FILES_DIR/lua_api/openfallout/*lua'." >&2
    exit 1
fi

rm -f "$OUTPUT_DIR"/*.html

data_paths=$("$DOCS_SOURCE_DIR/luadoc_data_paths.sh")

cd "$FILES_DIR/lua_api"
openmwluadocumentor -f doc -d "$OUTPUT_DIR" openfallout/*lua
cd "$FILES_DIR/data"
for path in $data_paths
do
  openmwluadocumentor -f doc -d "$OUTPUT_DIR" "$path"
done
