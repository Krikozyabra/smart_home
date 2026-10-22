#include "OntologyTestSupport.h"
#include <limits>

using namespace ontology_test;
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
            expectException<std::invalid_argument>([&] { storage.addDevice({-1, "No", "light"}); });
            expectException<std::invalid_argument>([&] { storage.addRoom({"", "No"}); });
            expectException<std::invalid_argument>([&] { storage.addAbility({"empty", "", "lighting"}); });
            expectException<std::invalid_argument>([&] { storage.getRoom(""); });
            expectException<std::invalid_argument>([&] { storage.getDevice(-1); });
            require(!storage.getDevice(99) && !storage.getRoom("missing") && !storage.getAbility("missing") && !storage.getDeviceDetails(99), "missing records");
            require(!storage.deleteDevice(99) && !storage.deleteRoom("missing") && !storage.deleteAbility("missing"), "missing deletion");
            storage.addRoom({"bathroom", "Ванная 'quoted' \\ $id"});
            storage.addDevice({0, "Updated light", "lamp"});
            storage.addAbility({"brightness", "Updated brightness", "light"});
            require(storage.getRoom("bathroom")->name == "Ванная 'quoted' \\ $id" && storage.getAllRooms().size() == 2, "parameterized upsert");
            require(storage.getDeviceDetails(0)->room_id == "bathroom" && storage.getDeviceDetails(0)->ability_ids.size() == 1, "upsert preserves edges");
            storage.addAbility({"injection' }) MATCH (n) DETACH DELETE n //", "Safe", "test"});
            storage.addDeviceAbility(0, "injection' }) MATCH (n) DETACH DELETE n //");
            require(storage.getAllDevices().size() == 2 && storage.getDeviceAbilities(0).size() == 2, "bound injection text");
            const auto maximum = std::numeric_limits<smart_home::DeviceId>::max();
            storage.addDevice({maximum, "Large ID", "sensor"});
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
        rawDatabase(incompatible.config.persistent_path, [](auto& conn) { rawQuery(conn, "CREATE NODE TABLE rooms(id INT64 PRIMARY KEY, name STRING)"); });
        const auto before = bytes(incompatible.config.persistent_path);
        expectException<std::runtime_error>([&] { LadybugOntologyStorage storage(incompatible.config); });
        require(bytes(incompatible.config.persistent_path) == before, "incompatible database preserved");
        std::cout << "Ontology storage tests passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
