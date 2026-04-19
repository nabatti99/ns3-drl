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

from typing import *
import sys
import traceback
import ns3ai_gym_env
import gymnasium as gym

env = gym.make("ns3ai_gym_env/Ns3-v0", targetName="dumbbell-drl-simulation", ns3Path=".")
ob_space = env.observation_space
ac_space = env.action_space
print("Observation space: ", ob_space, ob_space.dtype)
print("Action space: ", ac_space, ac_space.dtype)


class MyAgent:
    def __init__(self, env: gym.Env):
        self.env = env

    def get_action(self, obs: Tuple[float, float, float, float]) -> int:

        queue_delay = obs[0]
        queue_size = obs[1]
        enqueue_rate = obs[2]
        dequeue_rate = obs[3]

        action = self.env.action_space.sample()

        return action


try:
    my_agent = MyAgent(env)

    obs, info = env.reset()
    reward = 0
    done = False

    while True:
        action = my_agent.get_action(obs)
        print("---action: ", action)

        obs, reward, done, _, info = env.step(action)
        print("---obs, reward, done, info: ", obs, reward, done, info)

        if done:
            break

except Exception as e:
    exc_type, exc_value, exc_traceback = sys.exc_info()
    print("Exception occurred: {}".format(e))
    print("Traceback:")
    traceback.print_tb(exc_traceback)
    exit(1)

finally:
    print("Finally exiting...")
    env.close()
