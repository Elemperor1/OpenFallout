#ifndef GAME_MWWORLD_SPELLCASTSTATE_H
#define GAME_MWWORLD_SPELLCASTSTATE_H

namespace OFWorld
{
    enum class SpellCastState
    {
        Success = 0,
        InsufficientMagicka = 1,
        PowerAlreadyUsed = 2
    };
}

#endif
