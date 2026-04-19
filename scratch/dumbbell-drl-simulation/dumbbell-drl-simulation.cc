#include "ns3-env.h"

#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellDrlSimulation");

int
main(int argc, char* argv[])
{
    LogComponentEnableAll(LOG_PREFIX_ALL);
    LogComponentEnable("OpenGymEnv", LOG_LEVEL_LOGIC);
    LogComponentEnable("DumbbellDrlSimulation", LOG_LEVEL_LOGIC);

    Ptr<Ns3Env> ns3Env = CreateObject<Ns3Env>();

    // Run 10 loops of the environment to test
    for (int i = 0; i < 10; ++i)
    {
        // Push the current state and wait for the Agent to provide the next action
        ns3Env->Notify();

        NS_LOG_UNCOND("Current drop probability: " << ns3Env->m_dropProbability);
    }

    ns3Env->NotifySimulationEnd();

    return 0;
}
