#ifndef OPENFALLOUT_MWMECHANICS_AISTATEFWD_H
#define OPENFALLOUT_MWMECHANICS_AISTATEFWD_H

namespace OFMechanics
{
    template <class Base>
    class DerivedClassStorage;

    struct AiTemporaryBase;

    /// \brief Container for AI package status.
    using AiState = DerivedClassStorage<AiTemporaryBase>;
}

#endif
