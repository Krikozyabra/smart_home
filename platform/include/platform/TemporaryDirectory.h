/**
 * @file TemporaryDirectory.h
 * @brief Эксклюзивное владение временным каталогом Linux.
 */
#pragma once

#include <filesystem>
#include <string_view>

namespace smart_home::platform {

/**
 * @brief Создаёт приватный временный каталог и рекурсивно удаляет его при
 * разрушении.
 * @details Сохраняет каталог по явному запросу. Перемещение передаёт путь и
 * режим очистки. Доступ к одному объекту синхронизирует вызывающий код.
 */
class TemporaryDirectory {
  public:
    /**
     * @brief Создаёт каталог с правами 0700 через mkdtemp().
     * @param base Существующий родительский каталог.
     * @param prefix Префикс имени временного каталога без символа '/'.
     * @throws std::invalid_argument Пустая база, символ NUL в пути или
     * недопустимый префикс.
     * @throws std::system_error Ошибка получения абсолютного пути или создания
     * каталога.
     */
    TemporaryDirectory(const std::filesystem::path &base,
                       std::string_view prefix);
    /** @brief Рекурсивно удаляет каталог при включённой очистке, без
     * исключений. */
    ~TemporaryDirectory() noexcept;

    TemporaryDirectory(const TemporaryDirectory &) = delete;
    TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;
    /** @brief Передаёт каталог и его режим очистки. */
    TemporaryDirectory(TemporaryDirectory &&other) noexcept;
    /** @brief Завершает владение прежним каталогом и принимает новый. */
    TemporaryDirectory &operator=(TemporaryDirectory &&other) noexcept;

    /** @return Абсолютный путь до перемещения или разрушения, либо пустой путь
     * после перемещения. */
    const std::filesystem::path &path() const noexcept { return path_; }
    /** @brief Оставляет каталог и его содержимое после разрушения объекта. */
    void preserve() noexcept { setPreserve(true); }
    /** @param preserve true для сохранения объекта на диске, false для удаления
     * при разрушении. */
    void setPreserve(bool preserve) noexcept { preserve_ = preserve; }

  private:
    void cleanup() noexcept;

    std::filesystem::path path_;
    bool preserve_ = false;
};

} // namespace smart_home::platform
