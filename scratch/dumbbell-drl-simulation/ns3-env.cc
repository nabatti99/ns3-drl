#include "ns3-env.h"

#include <vector>

Ns3Env::Ns3Env()
{
    SetOpenGymInterface(OpenGymInterface::Get());
}

Ns3Env::~Ns3Env()
{
}

TypeId
Ns3Env::GetTypeId()
{
    static TypeId tid = TypeId("ns3::Ns3Env").SetParent<OpenGymEnv>().SetGroupName("OpenGym");
    return tid;
}

void
Ns3Env::DoDispose()
{
}

Ptr<OpenGymSpace>
Ns3Env::GetActionSpace()
{
    std::vector<uint32_t> shape = {1};
    std::string dtype = TypeNameGet<int32_t>();
    Ptr<OpenGymBoxSpace> box = CreateObject<OpenGymBoxSpace>(-2, 2, shape, dtype);
    return box;
}

Ptr<OpenGymSpace>
Ns3Env::GetObservationSpace()
{
    std::vector<uint32_t> shape = {4};
    std::string dtype = TypeNameGet<double>();
    Ptr<OpenGymBoxSpace> box = CreateObject<OpenGymBoxSpace>(0, 0, shape, dtype);
    return box;
}

bool
Ns3Env::GetGameOver()
{
    return false;
}

Ptr<OpenGymDataContainer>
Ns3Env::GetObservation()
{
    std::vector<uint32_t> shape = {4};
    Ptr<OpenGymBoxContainer<double>> box = CreateObject<OpenGymBoxContainer<double>>(shape);

    box->AddValue(m_queueDelay);
    box->AddValue(m_queueSize);
    box->AddValue(m_enqueueRate);
    box->AddValue(m_dequeueRate);

    return box;
}

float
Ns3Env::GetReward()
{
    return 0.0;
}

std::string
Ns3Env::GetExtraInfo()
{
    return "";
}

bool
Ns3Env::ExecuteActions(Ptr<OpenGymDataContainer> action)
{
    Ptr<OpenGymBoxContainer<int32_t>> box = DynamicCast<OpenGymBoxContainer<int32_t>>(action);
    int32_t actionValue = box->GetValue(0);

    m_dropProbability += m_adjustDropProbability[actionValue];
    m_dropProbability = std::max(0.0, std::min(1.0, m_dropProbability));

    return true;
}
