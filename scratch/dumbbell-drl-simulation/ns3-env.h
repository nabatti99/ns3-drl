#include <ns3/ai-module.h>
#include <ns3/my-custom-module.h>
#include <ns3/core-module.h>

using namespace ns3;

class Ns3Env : public OpenGymEnv
{
  public:
    Ns3Env();
    ~Ns3Env() override;
    static TypeId GetTypeId();
    void DoDispose() override;

    // States
    double m_queueDelay = 0.0;
    double m_queueSize = 0.0;
    double m_enqueueRate = 0.0;
    double m_dequeueRate = 0.0;

    // Actions
    double m_dropProbability = 0.5;
    default_map<int32_t, double> m_adjustDropProbability{
        {-2, -0.2},
        {-1, -0.1},
        {0, 0.0},
        {1, 0.1},
        {2, 0.2},
    };

    // OpenGym interfaces:
    Ptr<OpenGymSpace> GetActionSpace() override;
    Ptr<OpenGymSpace> GetObservationSpace() override;
    bool GetGameOver() override;
    Ptr<OpenGymDataContainer> GetObservation() override;
    float GetReward() override;
    std::string GetExtraInfo() override;
    bool ExecuteActions(Ptr<OpenGymDataContainer> action) override;
};
