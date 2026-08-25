#pragma once

#include <stdint.h>

/**
 * @file seed_source.h
 * @brief Supplies a different maze seed on every power-up, without any user action.
 *
 * ---------------------------------------------------------------------------------
 * WHY THIS EXISTS
 * ---------------------------------------------------------------------------------
 * MazeCore::generate() is deterministic on purpose: the same seed always produces the
 * same maze, on this chip and on a PC alike, which is what makes the board's output
 * checkable against the host oracle. The randomness a player sees therefore comes
 * entirely from the ONE number handed to it - and an STM32F103 has nothing random to
 * offer at boot. It has no hardware RNG (that arrives in the F2/F4 families), and
 * HAL_GetTick() reads zero every time, because every reset starts the same way.
 *
 * ---------------------------------------------------------------------------------
 * WHAT THIS DOES INSTEAD
 * ---------------------------------------------------------------------------------
 * It remembers. A counter is kept in the last page of flash, advanced once per boot,
 * and stirred together with the chip's factory-programmed unique ID. Flash survives
 * power-off, so "different every time you switch it on" is a guarantee here rather
 * than a probability - which is exactly what analog tricks like sampling noise on a
 * floating pin cannot promise. A quiet board can return the same ADC reading every
 * boot and hand you the same maze while looking like it did something clever.
 *
 * The cost is one flash word written per boot. See the wear note in seed_source.cpp.
 *
 * The seed is reported in every maze message, so reproducibility is untouched: read
 * the seed off the wire, pass it to tools/host_json_dump, and you get the same maze
 * the board is showing.
 */

namespace SeedSource {

/** @brief What a word of flash reads as when it has been erased and not yet written. */
constexpr uint32_t ERASED_WORD = 0xFFFFFFFFu;

/** @brief Where the next boot counter goes, and what value it should hold. */
struct SlotPlan {
    uint32_t next_value;    /**< Counter value to store. */
    uint32_t slot;          /**< Index of the word to write it into. */
    bool     erase_first;   /**< True when the page is full and must be cleared first. */
};

/**
 * @brief Works out where the next boot counter goes. Pure - touches no hardware.
 *
 * @param page       The counter page, read as an array of words.
 * @param slot_count How many words the page holds.
 * @return Where to write, what to write, and whether the page must be erased first.
 *
 * Kept in the header, free of any HAL dependency, so the bookkeeping can be tested on a
 * PC against a plain array - the same code that runs on the board, not a copy of it.
 * The page is treated as write-once slots because flash can only be cleared a whole page
 * at a time; see the wear note in seed_source.cpp.
 */
inline SlotPlan planNextSlot(const uint32_t* page, uint32_t slot_count)
{
    uint32_t last_value = 0u;
    uint32_t free_slot  = slot_count;   // slot_count means "no free slot left"

    for (uint32_t i = 0u; i < slot_count; ++i) {
        if (page[i] == ERASED_WORD) {
            free_slot = i;
            break;
        }
        last_value = page[i];
    }

    SlotPlan plan;
    plan.next_value  = last_value + 1u;
    plan.erase_first = (free_slot == slot_count);
    plan.slot        = plan.erase_first ? 0u : free_slot;
    return plan;
}

/**
 * @brief Returns a seed that differs on every power-up.
 *
 * @return A non-zero seed for MazeCore::generate().
 *
 * Safe to call exactly once, early in main(). It never blocks indefinitely: if flash
 * cannot be written it still returns a usable seed, at the cost of that seed repeating
 * on the next boot.
 */
uint32_t next();

}   // namespace SeedSource
