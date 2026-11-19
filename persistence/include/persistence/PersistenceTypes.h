#pragma once

#include "common/DeviceId.h"
#include "devices/types/Color.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <variant>

namespace smart_home::persistence {

/** @brief Неотрицательная ревизия полного состояния. */
using Revision = std::int64_t;

/**
 * @brief Возвращает следующую ревизию для проверки до изменения состояния.
 * @throws std::runtime_error Отрицательная ревизия или переполнение.
 */
Revision nextRevision(Revision revision);

/** @brief Значение свойства; целые числа хранятся с точностью int64. */
using PropertyValue =
    std::variant<bool, std::int64_t, double, Color, std::string>;

/** @brief Свойства по локальному ID и ключу возможности. */
using DevicePropertiesMap =
    std::map<DeviceId, std::map<std::string, PropertyValue>>;

/**
 * @brief Единственный владелец приватного каталога снимка в RAM.
 * @details Вызывающий код записывает state.sqlite и ontology.lbdb в каталог,
 * закрывает файлы и передаёт снимок воркеру. После передачи изменять файлы
 * нельзя. Перемещение передаёт владение; деструктор удаляет каталог.
 */
class Snapshot {
  public:
    /**
     * @brief Создаёт пустой приватный каталог в существующем каталоге RAM.
     * @throws std::runtime_error Недоступный каталог, отсутствие RAM или
     * отрицательная ревизия.
     */
    Snapshot(const std::filesystem::path &ram_base, Revision revision);
    /** @brief Удаляет каталог, если снимок ещё владеет им. */
    ~Snapshot() noexcept;

    /** @brief Передаёт владение каталогом. */
    Snapshot(Snapshot &&other) noexcept;
    Snapshot &operator=(Snapshot &&) = delete; ///< Присваивание запрещено.

    /**
     * @brief Возвращает каталог; после перемещения путь пуст.
     * @return Ссылка действительна до перемещения или уничтожения снимка.
     */
    const std::filesystem::path &directory() const noexcept {
        return directory_;
    }
    /** @brief Возвращает ревизию снимка. */
    Revision revision() const noexcept { return revision_; }

  private:
    std::filesystem::path directory_;
    Revision revision_;
};

/** @brief Подтверждение попытки публикации. */
struct SaveResult {
    Revision revision; ///< Ревизия переданного снимка.
    bool success;      ///< true после fsync каталога с CURRENT.
};

/** @brief Данные поколения, выбранного указателем CURRENT. */
struct CommittedGeneration {
    std::filesystem::path path; ///< Абсолютный путь к комплекту файлов.
    Revision revision; ///< Ревизия, сверенная с именем, манифестом и SQLite.
    std::string name;  ///< Имя каталога внутри generations.
};

} // namespace smart_home::persistence
