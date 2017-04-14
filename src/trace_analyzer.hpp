#ifndef __RESULT_WRITER_HPP__
#define __RESULT_WRITER_HPP__

#include <iostream>
#include <string>
#include <fstream>
#include <vector>

#include "json.hpp"

namespace oxton {

enum event_type
{
    EV_BEGIN_SEND = 0,
    EV_END_SEND,
    EV_BEGIN_RECV,
    EV_END_RECV
};

class trace_analyzer
{
public:
    trace_analyzer() : n_events_(0)
    {
    }

    void feed_event(event_type type, int peer, int len, int tag)
    {
        n_events_++;

        switch (type) {
        case EV_BEGIN_SEND:
            sent_traffic_[peer] += len;
            break;
        case EV_BEGIN_RECV:
            received_traffic_[peer] += len;
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
        sent_traffic_.resize(n_procs);
        received_traffic_.resize(n_procs);
    }

    void write_result(const std::string& path)
    {
        nlohmann::json j;
        j["processor_name"] = processor_name_;
        j["rank"] = rank_;
        j["n_procs"] = n_procs_;
        j["description"] = description_;
        j["n_events"] = n_events_;
        j["sent_traffic"] = sent_traffic_;
        j["received_traffic"] = received_traffic_;

        std::ofstream ofs(path);
        ofs << std::setw(4) << j << std::endl;
    }
private:
    std::string processor_name_;
    int rank_;
    int n_procs_;
    std::string description_;

    int n_events_;

    std::vector<uint64_t> sent_traffic_;
    std::vector<uint64_t> received_traffic_;
};

}

#endif
