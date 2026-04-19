from ns3ai_gym_env.ns3_environment import Ns3Env
from gymnasium.envs.registration import register

register(
    id="ns3ai_gym_env/Ns3-v0",
    entry_point="ns3ai_gym_env.ns3_environment:Ns3Env",
)
