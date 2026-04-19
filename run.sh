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

# Run the ns-3 simulation
./ns3 build
# python scratch/drl-simulation/drl-simulation.py
python scratch/dumbbell-drl-simulation/dumbbell-drl-simulation.py
