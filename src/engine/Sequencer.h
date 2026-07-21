#pragma once

#include <sixchange/core/Types.h>

namespace sixchange {

class Sequencer final {
public:
    [[nodiscard]]
    static Sequencer& instance() noexcept;

    [[nodiscard]]
    SequenceNumber next() noexcept;

    Sequencer(const Sequencer&) = delete;
    Sequencer& operator=(const Sequencer&) = delete;

    Sequencer(Sequencer&&) = delete;
    Sequencer& operator=(Sequencer&&) = delete;

private:
    Sequencer() noexcept = default;

    SequenceNumber next_sequence_{1};
};

} // namespace sixchange