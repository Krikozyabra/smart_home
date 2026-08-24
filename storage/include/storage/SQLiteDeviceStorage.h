#pragma once

#include "storage/DeviceRecord.h"
#include "storage/interfaces/IDeviceStorage.h"
#include "sqlite3.h"

#include <optional>
#include <string>
#include <vector>

namespace smart_home {

class SQLiteDeviceStorage final : public IDeviceStorage {
  private:
    sqlite3* database = nullptr;

    void createSchema();

  public:
    explicit SQLiteDeviceStorage(const std::string &database_path);
    ~SQLiteDeviceStorage() override;

    SQLiteDeviceStorage(const SQLiteDeviceStorage &) = delete;
    SQLiteDeviceStorage &operator=(const SQLiteDeviceStorage &) = delete;

    std::optional<DeviceRecord>
    findByPhysicalId(const std::string &physical_id,
                     const std::string &device_id) const override;

    DeviceRecord insert(const std::string &physical_id,
                        const std::string &device_id,
                        const std::string &default_name) override;

    std::vector<DeviceRecord> getAll() const override;
};

} // namespace smart_home
