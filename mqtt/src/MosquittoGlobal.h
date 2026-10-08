#pragma once

namespace smart_home::mqtt::detail {

// Must outlive every transport, including the mosquittopp base destructor.
class MosquittoGlobal {
public:
    MosquittoGlobal();
    ~MosquittoGlobal();
    MosquittoGlobal(const MosquittoGlobal&) = delete;
    MosquittoGlobal& operator=(const MosquittoGlobal&) = delete;
};

} // namespace smart_home::mqtt::detail
