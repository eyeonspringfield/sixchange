#pragma once

#include <sixchange/core/Types.h>

namespace sixchange {

class Sequencer {
public:
    explicit Sequencer(
        SequenceNumber first_sequence = SequenceNumber{1}
    ) noexcept;

    [[nodiscard]] SequenceNumber next() noexcept;

private:
    SequenceNumber next_sequence_;
};

} // namespace sixch
