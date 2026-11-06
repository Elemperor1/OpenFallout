# Building OpenFallout documentation

## Building in Docker (the recommended way)

### Preparing Docker image

Run the following commands from OpenFallout source directory to build a new Docker image `openfallout_doc`:
```bash
cd docs
docker build -t openfallout_doc .
cd ..
```

(or run script `docs/prepare_docker_image.sh`)

This step needs to be repeated only if any dependencies were changed.

The image and Read the Docs configuration use Python 3.12. The Documentation
workflow builds this image and the HTML when documentation dependencies change.

### Generating HTML

Both build methods stop if Lua page generation fails. The Docker image includes
LuaDocumentor; for a local build, install it as described below.

Run the following command from OpenFallout source directory to generate the documentation:
```bash
docker run --user "$(id -u)":"$(id -g)" --volume "$PWD":/openfallout openfallout_doc \
    sphinx-build /openfallout/docs/source /openfallout/docs/build
```

(or run script `docs/build_docs.sh`)

To view the generated documentation just open `docs/build/index.html` in a browser.

## Building without Docker (an alternative way)

Building documentation without Docker is more complicated as it requires multiple dependencies.

### Installation of required python packages

Use Python 3.12 from the OpenFallout source directory:
```bash
python3.12 -m venv .venv-docs
source .venv-docs/bin/activate
python -m pip install -r docs/requirements.txt
```

The OpenMW theme currently requires Sphinx below 9. Sphinx 8.2.3 and
Docutils 0.21.2 are pinned together; update these constraints and the theme
together before enabling Sphinx 9 or Docutils 0.22 and later.

### Installation of openmwluadocumentor:

**Debian/Ubuntu**

```bash
sudo apt install luarocks
git clone https://gitlab.com/ptmikheev/openmw-luadocumentor.git
cd openmw-luadocumentor/luarocks
luarocks --local pack openmwluadocumentor-0.2.0-1.rockspec
luarocks --local install openmwluadocumentor-0.2.0-1.src.rock
```

**Windows**

Use Docker as described above, or install
[WSL with Ubuntu](https://learn.microsoft.com/en-us/windows/wsl/install)
for a build without Docker. In the Ubuntu Bash shell, follow the Python 3.12
environment setup above and the Debian/Ubuntu LuaRocks instructions. Run
`sphinx-build` from that same environment, with `bash`, the Linux LuaRocks and
LuaDocumentor commands, and Sphinx available on `PATH`.

The native standalone Windows LuaRocks package emits CMD-style `SET` commands
from `luarocks path`; the Bash helper expects Unix shell assignments. Installing
Bash alongside that native package is insufficient for this build route.

### Generating HTML

Run the following command from OpenFallout source directory to generate the documentation:
```bash
sphinx-build docs/source docs/build
```
