# Stop on error
set -e

# Check python installation
pythonPath=$(which python)
if [[ -z "$pythonPath" ]]; then
    echo "Python is not installed. Please install Python by 'venv' and try again."
    exit 1
fi

if [[ "$pythonPath" != *".venv"* ]]; then
    echo "python is not using a .venv interpreter: $pythonPath"
    echo "Please activate the .venv environment and try again."
    exit 1
fi

# Prepare and configure ns-3
if [[ "$1" == "--clean" ]]; then
    echo "🧹 Cleaning ns-3 build..."
    ./ns3 clean
else
    echo "🔧 Using existing build of ns-3. Use --clean to clean the build and start fresh."
fi

./ns3 configure \
    --enable-python-bindings
    # --enable-examples \