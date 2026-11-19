#include "persistence/Reservation.h"
#include "persistence/SaveWorker.h"

#include <stdexcept>
#include <utility>

namespace smart_home::persistence {

Reservation::Reservation(Reservation &&other) noexcept
    : worker_(std::exchange(other.worker_, nullptr)) {}

Reservation::~Reservation() noexcept {
    if (worker_)
        worker_->release();
}

void Reservation::submit(Snapshot &&snapshot) {
    if (!worker_)
        throw std::logic_error("Snapshot submission requires a reserved slot.");
    worker_->submit(std::move(snapshot));
    worker_ = nullptr;
}

} // namespace smart_home::persistence
