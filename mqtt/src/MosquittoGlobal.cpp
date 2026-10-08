#include "MosquittoGlobal.h"
#include "MosquittoApi.h"
#include "mqtt/MqttErrors.h"

#include <mutex>

namespace smart_home::mqtt::detail {
namespace {
std::mutex runtimeMutex;
std::size_t runtimeUsers = 0;
}

MosquittoGlobal::MosquittoGlobal() {
    std::lock_guard<std::mutex> lock(runtimeMutex);
    if (runtimeUsers == 0) {
        const int rc = mosqpp::lib_init();
        if (rc != MOSQ_ERR_SUCCESS)
            throw MqttException("Cannot initialize the MQTT library.", rc);
    }
    ++runtimeUsers;
}

MosquittoGlobal::~MosquittoGlobal() {
    std::lock_guard<std::mutex> lock(runtimeMutex);
    if (--runtimeUsers == 0)
        mosqpp::lib_cleanup();
}

} // namespace smart_home::mqtt::detail
