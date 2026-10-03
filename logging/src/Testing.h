/** @file Testing.h
 * @brief Внутренний интерфейс подмены приемника только для тестовой сборки.
 */
#pragma once
#include <spdlog/sinks/sink.h>
#include <memory>
namespace smart_home::logging {
/** @brief Устанавливает приемник для проверки фильтрации и ошибок вывода.
 * @param sink Тестовый приемник.
 */
void installTestSink(std::shared_ptr<spdlog::sinks::sink> sink);
}
