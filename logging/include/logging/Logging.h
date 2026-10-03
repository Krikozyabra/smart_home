/**
 * @file Logging.h
 * @brief Потокобезопасное диагностическое логирование приложения.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <limits>
#include <string>
#include <string_view>

namespace smart_home::logging {

/** @brief Важность сообщения; Off отключает обычные записи. */
enum class Level {
    Trace, ///< Подробная трассировка.
    Debug, ///< Отладочные события.
    Info, ///< События жизненного цикла.
    Warn, ///< Обработанные отклонения.
    Error, ///< Ошибка операции с продолжением работы.
    Critical, ///< Фатальное завершение приложения.
    Off ///< Отключение записей.
};

/** @brief Параметры синхронного логирования в stderr и файл. */
struct Config {
    Level level = Level::Info; ///< Минимальная важность.
    std::string file_path = "logs/smart_home.log"; ///< Путь; пустой отключает файл.
    std::size_t max_file_size = 5 * 1024 * 1024; ///< Порог ротации в байтах.
    std::size_t max_rotated_files = 3; ///< Количество архивных файлов.
};

/** @brief Контекст операции без физических адресов или полезной нагрузки.
 * @details Строка operation должна существовать до возврата синхронного log().
 */
struct Context {
    std::int64_t device_id = -1; ///< Локальный идентификатор; -1 означает отсутствие.
    std::string_view operation; ///< Идентификатор операции.
    std::size_t source_index = std::numeric_limits<std::size_t>::max(); ///< Индекс источника; max означает отсутствие.
};

/**
 * @brief Настраивает вывод; ошибка файла оставляет вывод в stderr.
 * @param config Параметры; одинаковый повторный вызов ничего не меняет.
 * @throws std::invalid_argument Некорректные параметры.
 * @throws std::logic_error Повторная инициализация с другими параметрами.
 */
void initialize(const Config& config);
/** @brief Проверяет порог; до инициализации доступны Warn и выше.
 * @param level Проверяемая важность.
 * @return true, если запись разрешена.
 */
bool enabled(Level level) noexcept;
/** @brief Записывает сообщение без исключений; ошибки вывода идут напрямую в stderr.
 * @param level Важность.
 * @param component Имя компонента.
 * @param message Безопасный текст без секретов.
 * @param context Контекст устройства, операции или источника.
 */
void log(Level level, std::string_view component, std::string_view message,
         Context context = {}) noexcept;
/** @brief Изменяет порог во время выполнения.
 * @param level Новый порог.
 */
void setLevel(Level level) noexcept;
/** @brief Сбрасывает буферы без исключений. */
void flush() noexcept;
/** @brief Освобождает собственный логгер; глобальный реестр spdlog не затрагивается. */
void shutdown() noexcept;

/** @brief Разбирает CLI и окружение: CLI > окружение > значения по умолчанию.
 * @param argc Количество аргументов.
 * @param argv Аргументы с именем программы в позиции 0.
 * @param warnings Поток предупреждений о некорректном окружении.
 * @return Проверенная конфигурация.
 * @throws std::invalid_argument Неизвестный аргумент или некорректное значение CLI.
 */
Config resolveConfig(int argc, const char* const* argv, std::ostream& warnings);

/** @brief Владелец логирования в main; должен жить дольше объектов приложения. */
class Session {
public:
    /** @brief Инициализирует логирование.
     * @param config Настройки.
     * @throws std::invalid_argument Некорректные настройки.
     * @throws std::logic_error Конфликт инициализации.
     */
    explicit Session(const Config& config);
    /** @brief Сбрасывает буферы и освобождает логгер. */
    ~Session();
    Session(const Session&) = delete; ///< Копирование запрещено.
    Session& operator=(const Session&) = delete; ///< Присваивание запрещено.
};
} // namespace smart_home::logging
