#pragma once

#include "persistence/PersistenceTypes.h"

#include <optional>

namespace smart_home::persistence {

class StateLock;

/** @brief Разрешает и проверяет зафиксированное поколение при запуске. */
class GenerationManager {
  public:
    /**
     * @brief Использует каталог, защищённый блокировкой.
     * @param lock Блокировка должна существовать до уничтожения менеджера.
     */
    explicit GenerationManager(const StateLock &lock) noexcept;

    /**
     * @brief Читает CURRENT и проверяет комплект, SHA-256 и ревизию.
     * @pre Вызывающий код завершает проверку и копирование до запуска записи.
     * @return Поколение по CURRENT или std::nullopt при отсутствии указателя.
     * @throws std::runtime_error Зафиксированное поколение повреждено или
     * недоступно.
     */
    std::optional<CommittedGeneration> resolveCommitted() const;

  private:
    const StateLock &lock_;
};

} // namespace smart_home::persistence
