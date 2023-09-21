#!/bin/bash
set -ex

# Build (and test) tiledbvcf-py assuming source code directory is
# ./TileDB-VCF/apis/python

OS=$(uname)
echo "OS: $OS"
if [[ $OS == Linux ]]
then
  export LD_LIBRARY_PATH=$GITHUB_WORKSPACE/install/lib:${LD_LIBRARY_PATH-}
  echo "LD_LIBRARY_PATH: $LD_LIBRARY_PATH"
elif [[ $OS == Darwin ]]
then
  export DYLD_LIBRARY_PATH=$GITHUB_WORKSPACE/install/lib:${DYLD_LIBRARY_PATH-}
  echo "DYLD_LIBRARY_PATH: $DYLD_LIBRARY_PATH"
else
  # assume Windows, which searches PATH for shared libraries
  export PATH=$GITHUB_WORKSPACE/install/bin:${PATH-}
  echo "PATH: $PATH"
fi

cd TileDB-VCF/apis/python
python setup.py develop --libtiledbvcf=$GITHUB_WORKSPACE/install/
python -c "import tiledbvcf; print(tiledbvcf.version)"

pytest
