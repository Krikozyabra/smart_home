#include "OntologyTestSupport.h"
#include <limits>

using namespace ontology_test;

namespace {
void capabilityBasedDevices() {
    Fixture fixture;
    const OntologyDeviceRecord renamed{0, "Renamed ' }) MATCH (n) DETACH DELETE n //"};
    const std::vector<OntologyDeviceRecord> expected{renamed, {2, "Shared"}, {7, "Shared"}};
    {
        LadybugOntologyStorage storage(fixture.config);
        storage.addRoom({"room", "Room"});
        storage.addAbility({"temperature", "Temperature", "climate"});
        storage.addAbility({"brightness", "Brightness", "lighting"});
        // Identical names do not determine a device's role. Insert out of order.
        for (const auto id : {7, 0, 2}) {
            storage.addDevice({id, "Shared"});
            storage.assignDeviceToRoom(id, "room");
        }
        require(storage.getDevicesWithAbility("brightness").empty(), "no implicit abilities from name");
        storage.addDeviceAbility(7, "temperature");
        storage.addDeviceAbility(7, "brightness");
        storage.addDeviceAbility(0, "brightness");
        storage.addDeviceAbility(0, "brightness");
        const auto copy = storage.getDevice(0).value();
        storage.addDevice(renamed);
        require(copy == OntologyDeviceRecord{0, "Shared"}, "device reads are independent copies");
        require(!(copy == renamed) && !(copy == OntologyDeviceRecord{2, "Shared"}), "device equality uses key and name");
        require(storage.getDevice(0).value() == renamed && storage.getAllDevices() == expected, "two-field device reads ordered by key");
        require(storage.getDevicesInRoom("room") == expected, "room device projection preserves names and key order");
        require(storage.getDevicesWithAbility("brightness") == std::vector<OntologyDeviceRecord>{renamed, {7, "Shared"}},
                "capability query preserves names and key order without duplicates");
        require(storage.getDevicesWithAbility("temperature") == std::vector<OntologyDeviceRecord>{{7, "Shared"}},
                "device can have abilities from different categories");
        const auto details = storage.getDeviceDetails(0).value();
        require(details.device == renamed && details.room_id == "room" && details.ability_ids == std::vector<std::string>{"brightness"},
                "rename preserves room and capability links");
        require(storage.getDeviceDetails(7)->ability_ids == std::vector<std::string>{"brightness", "temperature"}, "ability keys are ordered");
        require(storage.getDeviceDetails(2)->ability_ids.empty(), "device without abilities remains readable");
        storage.removeDeviceAbility(7, "brightness");
        require(storage.getDevicesWithAbility("brightness") == std::vector<OntologyDeviceRecord>{renamed} &&
                storage.getDevicesWithAbility("temperature") == std::vector<OntologyDeviceRecord>{{7, "Shared"}},
                "roles follow current links");
        storage.dumpToFile(fixture.config.database_path);
        storage.close();
    }
    LadybugOntologyStorage reopened(fixture.config);
    require(reopened.getAllDevices() == expected && reopened.getDevicesInRoom("room") == expected, "two-field devices survive reopen");
    require(reopened.getDevicesWithAbility("brightness") == std::vector<OntologyDeviceRecord>{renamed} &&
            reopened.getDeviceDetails(7)->ability_ids == std::vector<std::string>{"temperature"}, "capability links survive reopen");
}

void rejectLegacyDeviceSchema() {
    Fixture fixture;
    rawDatabase(fixture.config.database_path, [](auto& conn) {
        rawQuery(conn, "CREATE NODE TABLE rooms(id STRING, name STRING, PRIMARY KEY(id))");
        rawQuery(conn, "CREATE NODE TABLE devices(id INT64, name STRING, type STRING, PRIMARY KEY(id))");
        rawQuery(conn, "CREATE NODE TABLE abilities(id STRING, name STRING, category STRING, PRIMARY KEY(id))");
        rawQuery(conn, "CREATE REL TABLE LOCATED_IN(FROM devices TO rooms)");
        rawQuery(conn, "CREATE REL TABLE HAS_FEATURE(FROM devices TO abilities)");
        rawQuery(conn, "CREATE (:devices {id: 0, name: 'Legacy light', type: 'light'})");
        rawQuery(conn, "CHECKPOINT");
    });
    const auto before = bytes(fixture.config.database_path);
    try {
        LadybugOntologyStorage storage(fixture.config);
        require(false, "legacy devices.type schema was accepted");
    } catch (const std::runtime_error& error) {
        require(std::string(error.what()).find("explicit migration required") != std::string::npos, "legacy schema requires explicit migration");
    }
    require(bytes(fixture.config.database_path) == before && fs::is_empty(fixture.root / "ram"),
            "legacy database preserved and failed startup workspace cleaned");
}
} // namespace

int main() {
    try {
        Fixture fixture;
        {
            LadybugOntologyStorage storage(fixture.config);
            require(storage.getAllRooms().empty() && storage.getAllDevices().empty() && storage.getAllAbilities().empty(), "fresh schema");
            seed(storage);
            require(storage.getAllRooms().size() == 2 && storage.getAllDevices().size() == 2 && storage.getAllAbilities().size() == 2, "seed");
            require(storage.getDeviceDetails(0)->ability_ids == std::vector<std::string>{"brightness"}, "device details");
            storage.addDeviceAbility(0, "brightness");
            require(storage.getDeviceAbilities(0).size() == 1 && storage.getDevicesWithAbility("brightness").size() == 1, "no duplicate edges");
            storage.assignDeviceToRoom(0, "bathroom");
            storage.assignDeviceToRoom(0, "bathroom");
            require(storage.getDevicesInRoom("living_room").empty() && storage.getDevicesInRoom("bathroom").size() == 2, "room reassignment");
            expectException<std::invalid_argument>([&] { storage.assignDeviceToRoom(0, "missing"); });
            expectException<std::invalid_argument>([&] { storage.assignDeviceToRoom(99, "bathroom"); });
            require(storage.getDeviceRoom(0)->id == "bathroom", "failed assignment preserves old room");
            expectException<std::invalid_argument>([&] { storage.addDeviceAbility(0, "missing"); });
            expectException<std::invalid_argument>([&] { storage.addDeviceAbility(99, "brightness"); });
            expectException<std::invalid_argument>([&] { storage.getDeviceAbilities(99); });
            expectException<std::invalid_argument>([&] { storage.getDevicesWithAbility("missing"); });
            expectException<std::invalid_argument>([&] { storage.getDevicesInRoom("missing"); });
            expectException<std::invalid_argument>([&] { storage.getDeviceRoom(99); });
            expectException<std::invalid_argument>([&] { storage.removeDeviceFromRoom(99); });
            expectException<std::invalid_argument>([&] { storage.removeDeviceAbility(0, "missing"); });
            expectException<std::invalid_argument>([&] { storage.addDevice({-1, "No"}); });
            expectException<std::invalid_argument>([&] { storage.addDevice({0, ""}); });
            expectException<std::invalid_argument>([&] { storage.addDevice({0, std::string("Bad\0name", 8)}); });
            expectException<std::invalid_argument>([&] { storage.addRoom({"", "No"}); });
            expectException<std::invalid_argument>([&] { storage.addAbility({"empty", "", "lighting"}); });
            expectException<std::invalid_argument>([&] { storage.getRoom(""); });
            expectException<std::invalid_argument>([&] { storage.getDevice(-1); });
            require(!storage.getDevice(99) && !storage.getRoom("missing") && !storage.getAbility("missing") && !storage.getDeviceDetails(99), "missing records");
            require(!storage.deleteDevice(99) && !storage.deleteRoom("missing") && !storage.deleteAbility("missing"), "missing deletion");
            storage.addRoom({"bathroom", "Ванная 'quoted' \\ $id"});
            storage.addDevice({0, "Updated light"});
            storage.addAbility({"brightness", "Updated brightness", "light"});
            require(storage.getRoom("bathroom")->name == "Ванная 'quoted' \\ $id" && storage.getAllRooms().size() == 2, "parameterized upsert");
            require(storage.getDeviceDetails(0)->room_id == "bathroom" && storage.getDeviceDetails(0)->ability_ids.size() == 1, "upsert preserves edges");
            storage.addAbility({"injection' }) MATCH (n) DETACH DELETE n //", "Safe", "test"});
            storage.addDeviceAbility(0, "injection' }) MATCH (n) DETACH DELETE n //");
            require(storage.getAllDevices().size() == 2 && storage.getDeviceAbilities(0).size() == 2, "bound injection text");
            const auto maximum = std::numeric_limits<smart_home::DeviceId>::max();
            storage.addDevice({maximum, "Large ID"});
            require(storage.getAllDevices().back().id == maximum, "int64 ID preserved");
            storage.removeDeviceAbility(0, "brightness");
            storage.removeDeviceAbility(0, "brightness");
            require(storage.getDevicesWithAbility("brightness").empty(), "unlink ability");
            storage.addDeviceAbility(0, "brightness");
            require(storage.deleteAbility("brightness") && storage.getDeviceAbilities(0).size() == 1, "delete ability detaches");
            storage.removeDeviceFromRoom(0);
            storage.removeDeviceFromRoom(0);
            require(!storage.getDeviceRoom(0) && storage.getDeviceDetails(0)->room_id.empty(), "remove room");
            storage.assignDeviceToRoom(0, "bathroom");
            require(storage.deleteRoom("bathroom") && !storage.getDeviceRoom(0) && !storage.getDeviceRoom(1), "room detach cascade");
            require(storage.deleteDevice(0) && storage.getAllAbilities().size() == 2, "device detach cascade");
            storage.close();
        }
        {
            LadybugOntologyStorage reopened(fixture.config);
            require(reopened.getAllDevices().size() == 2 && reopened.getAllRooms().size() == 1, "schema idempotent reopen");
        }
        // Existing incompatible databases must not be silently recreated.
        Fixture incompatible;
        rawDatabase(incompatible.config.database_path, [](auto& conn) { rawQuery(conn, "CREATE NODE TABLE rooms(id INT64 PRIMARY KEY, name STRING)"); });
        const auto before = bytes(incompatible.config.database_path);
        expectException<std::runtime_error>([&] { LadybugOntologyStorage storage(incompatible.config); });
        require(bytes(incompatible.config.database_path) == before, "incompatible database preserved");
        capabilityBasedDevices();
        rejectLegacyDeviceSchema();
        std::cout << "Ontology storage tests passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
