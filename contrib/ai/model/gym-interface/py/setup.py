# Copyright (c) 2023 Huazhong University of Science and Technology
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation;
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
# Author: Muyuan Shen <muyuan_shen@hust.edu.cn>

import os
import shutil
from setuptools import setup

# Copy the generated protobuf files to the package directory
def copy_generated_files(file_names, target_folder="ns3ai_gym_env"):
    base_dir = os.path.dirname(__file__)

    target_dir = os.path.join(base_dir, target_folder)

    files = os.listdir(base_dir)
    for file in files:
        if file.startswith(tuple(file_names)):
            source_file = os.path.join(base_dir, file)

            # Only copy if it's a file and not already present in the target directory
            if os.path.isfile(source_file):
                if os.path.exists(os.path.join(target_dir, file)):
                    os.unlink(os.path.join(target_dir, file))

                shutil.copy2(source_file, target_dir)

copy_generated_files(["messages_pb2", "ns3ai_gym_msg_py"])

# Set up the package
setup(
    name="ns3ai_gym_env",
    version="0.0.1",
    install_requires=["numpy", "gymnasium", "protobuf"],
)
