#ifndef __TRACE_HPP__
#define __TRACE_HPP__

#include <iostream>
#include <string>
#include <fstream>
#include <unordered_map>
#include <vector>

#include "json.hpp"

namespace pfprof {

enum event_type
{
    EV_BEGIN_SEND = 0,
    EV_END_SEND,
    EV_BEGIN_RECV,
    EV_END_RECV
};

class trace
{
public:
    trace() : duration_(0.0), n_events_(0)
    {
    }

    void feed_event(event_type type, int peer, int len, int tag)
    {
        n_events_++;

        switch (type) {
        case EV_BEGIN_SEND:
            tx_bytes_[peer] += len;
            tx_messages_[peer]++;
            tx_message_sizes_[len]++;
            break;
        case EV_BEGIN_RECV:
            rx_bytes_[peer] += len;
            rx_messages_[peer]++;
            rx_message_sizes_[len]++;
            break;
        default:
            break;
        }
    }

    void set_processor_name(const std::string& processor_name)
    {
        processor_name_ = processor_name;
    }

    void set_rank(int rank)
    {
        rank_ = rank;
    }

    void set_description(const std::string& description)
    {
        description_ = description;
    }

    void set_n_procs(int n_procs)
    {
        n_procs_ = n_procs;
        tx_bytes_.resize(n_procs);
        rx_bytes_.resize(n_procs);
        tx_messages_.resize(n_procs);
        rx_messages_.resize(n_procs);
    }

    void set_duration(double duration)
    {
        duration_ = duration;
    }

    void write_result(const std::string& path)
    {
        nlohmann::json j;

        // Meta data
        j["processor_name"] = processor_name_;
        j["rank"] = rank_;
        j["n_procs"] = n_procs_;
        j["description"] = description_;
        j["n_events"] = n_events_;
        j["duration"] = duration_;

        j["tx_bytes"] = tx_bytes_;
        j["rx_bytes"] = rx_bytes_;
        j["tx_messages"] = tx_messages_;
        j["rx_messages"] = rx_messages_;

        j["tx_message_sizes"] = nlohmann::json::array();
        for (const auto& kv : tx_message_sizes_) {
            j["tx_message_sizes"].push_back({
                {"message_size", kv.first},
                {"frequency", kv.second},
            });
        }

        j["rx_message_sizes"] = nlohmann::json::array();
        for (const auto& kv : rx_message_sizes_) {
            j["rx_message_sizes"].push_back({
                {"message_size", kv.first},
                {"frequency", kv.second},
            });
        }

        std::ofstream ofs(path);
        ofs << std::setw(4) << j << std::endl;
    }
private:
    std::string processor_name_;
    int rank_;
    int n_procs_;
    std::string description_;
    double duration_;
    int n_events_;

    std::vector<uint64_t> tx_bytes_;
    std::vector<uint64_t> rx_bytes_;
    std::vector<uint64_t> tx_messages_;
    std::vector<uint64_t> rx_messages_;
    std::unordered_map<int, uint64_t> tx_message_sizes_;
    std::unordered_map<int, uint64_t> rx_message_sizes_;
};

}

#endif
