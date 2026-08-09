$ErrorActionPreference = 'Stop'

if (-not $env:GEODE_SDK) {
    throw 'GEODE_SDK is not set. Example: $env:GEODE_SDK = "C:\path\to\geode"'
}

python tools/bootstrap_deps.py
python -m unittest discover -s tests -p 'test_*.py' -v
python tools/verify_bindings.py
python tools/verify_upstream_bindings.py
python tools/bootstrap_deps.py --validate-only

cmake -S . -B build -A x64
cmake --build build --config Release --parallel

cmake -S companion -B companion/build -A x64
cmake --build companion/build --config Release --parallel
