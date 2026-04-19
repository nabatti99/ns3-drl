# The Network Simulator, Version 3 (ns-3) with Deep Reinforcement Learning (DRL) Support

[![Latest Release](https://gitlab.com/nsnam/ns-3-dev/-/badges/release.svg)](https://gitlab.com/nsnam/ns-3-dev/-/releases)

> **NOTE**: Much more substantial information about ns-3 can be found at
<https://www.nsnam.org>

## License

This software is licensed under the terms of the GNU General Public License v2.0 only (GPL-2.0-only).
See the LICENSE file for more details.

## Quick setup

The quickest way to get started with ns-3 and the DRL support.

### Install Python

Install `uv` and set default Python version to 3.13:

https://docs.astral.sh/uv/getting-started/installation/

```bash
uv python pin 3.13 --global
```

Create a virtual environment with `pip`:

```bash
uv venv ns-3-drl --seed
```

Activate the virtual environment:

```bash
source .venv/bin/activate
```

### Build ns-3 with DRL support

**Note:** Make sure the `venv` is activated before running the following commands.

Configure `ns-3` project with Python bindings:

```bash
bash configure.sh

# If you want to clean the build and start fresh, use:
bash configure.sh --clean
```

Build the whole project:

```bash
./ns3 build
```

Install the python bindings for the `ai` module:

```bash
pip install -e contrib/ai/python_utils
pip install -e contrib/ai/model/gym-interface/py
```

### Run the experiment

The sample experiment can be found in `scratch/drl-simulation/`. You can run it with:

```bash
python3 scratch/drl-simulation/drl-simulation.py
```

Or, you can config the `run.sh` script to run the experiment with different parameters:

```bash
# Edit the run.sh script to set the desired parameters for the experiment
code run.sh

# Then run the script
bash run.sh
```

## ns-3 Documentation

All of the full documentation should always be available from
the ns-3 website: <https://www.nsnam.org/documentation/>.

## ns3-ai Contribution Library

The `ns3-ai` library is a contribution to the ns-3 project that provides support for integrating deep reinforcement learning (DRL) algorithms with ns-3 simulations. It includes Python bindings for the `ai` module, which allows users to easily create and train DRL agents using popular libraries such as TensorFlow and PyTorch.

You can find more information about the `ns3-ai` library and how to use it in the documentation: <https://github.com/hust-diangroup/ns3-ai/tree/main>.