#include "seed_source.h"

#include "stm32f1xx_hal.h"

namespace SeedSource {

namespace {

/** @brief Address of the page holding the boot counter.
 *
 *  The last 1KB page of the 64KB declared in STM32F103XX_FLASH.ld (0x08000000 + 64K -
 *  1K). The linker fills flash from the bottom and the image is currently around 7.5KB,
 *  so this page is far from any code.
 *
 *  ASSUMPTION, worth re-checking if the firmware ever grows past ~63KB: nothing reserves
 *  this page in the linker script, so a large enough image would eventually collide with
 *  it. `arm-none-eabi-size build/main.elf` is the check.
 */
constexpr uint32_t COUNTER_PAGE = 0x08000000u + (64u * 1024u) - FLASH_PAGE_SIZE;

/** @brief Words available in the page. FLASH_PAGE_SIZE is 0x400 on medium-density F1. */
constexpr uint32_t SLOT_COUNT = FLASH_PAGE_SIZE / sizeof(uint32_t);

uint32_t readWord(uint32_t address)
{
    return *reinterpret_cast<volatile uint32_t*>(address);
}

/**
 * @brief Scrambles a counter into something that looks nothing like its neighbours.
 *
 * Boot 7 and boot 8 must not produce similar mazes. This is the finalising mix from
 * MurmurHash3 - two multiply-and-xor-shift rounds - chosen because every input bit ends
 * up affecting every output bit, and it costs three multiplications rather than a table.
 *
 * xorshift32 inside MazeCore would eventually diverge on its own, but its first outputs
 * for adjacent seeds start close together, and the maze is built from the first few
 * dozen values. Mixing here keeps that from showing.
 */
uint32_t avalanche(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x85EBCA6Bu;
    x ^= x >> 13;
    x *= 0xC2B2AE35u;
    x ^= x >> 16;
    return x;
}

/**
 * @brief Finds the boot counter, advances it, and stores it again.
 *
 * @return The new boot count, or 0 if flash could not be written.
 *
 * The page is used as SLOT_COUNT write-once slots rather than one rewritten word,
 * because flash can only be cleared a whole page at a time. Each boot fills the next
 * empty slot, so a page erase happens once every 256 boots instead of every boot -
 * flash is rated for about 10,000 erase cycles, which at one erase per 256 boots is
 * roughly 2.5 million power-ups. Writing a single word every boot instead would have
 * spent that budget after 10,000.
 */
uint32_t advanceBootCounter()
{
    const SlotPlan plan = planNextSlot(
        reinterpret_cast<const uint32_t*>(COUNTER_PAGE), SLOT_COUNT);

    if (HAL_FLASH_Unlock() != HAL_OK) return 0u;

    if (plan.erase_first) {
        FLASH_EraseInitTypeDef erase = {};
        erase.TypeErase   = FLASH_TYPEERASE_PAGES;
        erase.PageAddress = COUNTER_PAGE;
        erase.NbPages     = 1u;

        uint32_t page_error = 0u;
        if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK) {
            HAL_FLASH_Lock();
            return 0u;
        }
    }

    const HAL_StatusTypeDef written = HAL_FLASH_Program(
        FLASH_TYPEPROGRAM_WORD,
        COUNTER_PAGE + plan.slot * sizeof(uint32_t),
        plan.next_value);

    HAL_FLASH_Lock();

    return (written == HAL_OK) ? plan.next_value : 0u;
}

}   // namespace

uint32_t next()
{
    const uint32_t boot = advanceBootCounter();

    // 96-bit factory-programmed unique device ID (RM0008 section 30.2, base 0x1FFFF7E8).
    // Constant for a given chip, so it cannot vary the maze between boots - it is here so
    // that two boards running the same boot number still show different mazes.
    const uint32_t uid0 = readWord(UID_BASE + 0u);
    const uint32_t uid1 = readWord(UID_BASE + 4u);
    const uint32_t uid2 = readWord(UID_BASE + 8u);

    // HAL_GetTick() is near zero here and adds nothing on its own; it is folded in so that
    // a later move of this call - after a delay, or to a button press - starts varying the
    // seed without any other change.
    const uint32_t mixed = avalanche(boot)
                         ^ avalanche(uid0 ^ uid1 ^ uid2)
                         ^ HAL_GetTick();

    // generate() rewrites a zero seed to 1 anyway; done here too so the value reported on
    // the wire is the value actually used.
    return (mixed == 0u) ? 1u : mixed;
}

}   // namespace SeedSource
