#ifndef MY_MAPPING_H
#define MY_MAPPING_H

#include "../tags/flow-type-tag.h"

#include <unordered_map>

template <typename K, typename V>
class default_map : public std::unordered_map<K, V>
{
  public:
    // Inherit std::unordered_map constructors
    using std::unordered_map<K, V>::unordered_map;

    default_map(V defaultValue)
        : std::unordered_map<K, V>(),
          m_defaultValue(defaultValue)
    {
    }

    bool contains(const K& key) const
    {
        return this->find(key) != this->end();
    }

    V& operator[](const K& key)
    {
        auto it = this->find(key);
        if (it == this->end())
        {
            // Insert default value
            auto result = this->emplace(key, V(m_defaultValue));
            it = result.first;
        }

        return it->second;
    }

  private:
    V m_defaultValue;
};

static const std::unordered_map<std::string, FlowType> APP_TYPE_TO_FLOW_TYPE = {
    {"ns3::RealtimeApplication", FlowType::REALTIME_FLOW},
    {"ns3::NonRealtimeApplication", FlowType::NON_REALTIME_FLOW},
    {"ns3::OtherApplication", FlowType::OTHER_FLOW}};

#endif // MY_MAPPING_H