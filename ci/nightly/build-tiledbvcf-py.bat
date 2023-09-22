@echo on

rem Build (and test) tiledbvcf-py assuming source code directory is
rem .\TileDB-VCF\apis\python

set PATH=%GITHUB_WORKSPACE%\install\bin;%PATH%
echo "PATH: $PATH"

cd TileDB-VCF\apis\python
python setup.py develop --libtiledbvcf=$GITHUB_WORKSPACE\install\
python -c "import tiledbvcf; print(tiledbvcf.version)"

pytest
