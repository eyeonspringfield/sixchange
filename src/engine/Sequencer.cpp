#include "engine/Sequencer.h"

namespace sixchange {

    Sequencer::Sequencer(const SequenceNumber first_sequence) noexcept : next_sequence_{first_sequence} {}

    SequenceNumber Sequencer::next() noexcept {
        return next_sequence_++;
    }

} // namespace sixchange

