#pragma once

#include "common/DeviceId.h"
#include "storage/DeviceRecord.h"
#include "storage/interfaces/IDeviceStorage.h"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace smart_home {

class InMemoryDeviceStorage final : public IDeviceStorage {
  private:
    using DeviceKey = std::pair<std::string, std::string>;
    std::map<DeviceKey, DeviceRecord> records;
    DeviceId next_local_id = 0;

  public:
    DeviceRecord insert(const std::string &physical_id,
                        const std::string &driver_id,
                        const std::string &name) override;

    std::optional<DeviceRecord>
    findByPhysicalId(const std::string &driver_id,
                     const std::string &physical_id) const override;

    std::vector<DeviceRecord> getAll() const override;
};

} // namespace smart_home
