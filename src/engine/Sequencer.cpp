#include "engine/Sequencer.h"

namespace sixchange {

Sequencer& Sequencer::instance() noexcept
{
    static Sequencer sequencer;
    return sequencer;
}

SequenceNumber Sequencer::next() noexcept
{
    return next_sequence_++;
}

} // namespace sixchange