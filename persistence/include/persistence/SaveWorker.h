#pragma once

#include "persistence/Reservation.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace smart_home::persistence {

class StateLock;

/**
 * @brief Публикует полные RAM-снимки в одном фоновом потоке.
 * @details reserve(), collectResults() и stop() потокобезопасны.
 * Дескриптор событий использует один потребитель.
 */
class SaveWorker {
  public:
    /**
     * @brief Открывает EventPipe и запускает фоновый поток.
     * @param lock Блокировка каталога, живущая дольше воркера. Для неё
     * допускается один воркер.
     * @param capacity Максимум выданных слотов, включая очередь, активную
     * запись и результаты до collectResults().
     * @throws std::invalid_argument Нулевая ёмкость.
     * @throws std::runtime_error Ошибка запуска потока или открытия канала.
     */
    SaveWorker(const StateLock &lock, std::size_t capacity);
    /** @brief Завершает очередь и дожидается потока. */
    ~SaveWorker() noexcept;

    /** @brief Резервирует слот без ожидания; при заполнении или остановке токен
     * пуст. */
    Reservation reserve();

    /**
     * @brief Возвращает готовые результаты по порядку и освобождает их слоты.
     * @details Одновременно сбрасывает уведомления канала событий.
     */
    std::vector<SaveResult> collectResults();

    /**
     * @brief Возвращает читающий дескриптор для poll(POLLIN).
     * @return Дескриптор принадлежит воркеру и действителен до его уничтожения.
     */
    int eventFd() const noexcept;

    /** @brief Запрещает новые задания, завершает очередь и дожидается потока.
     */
    void stop();

  private:
    friend class Reservation;
    void release() noexcept;
    void submit(Snapshot &&snapshot);

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace smart_home::persistence
