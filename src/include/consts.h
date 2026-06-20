#ifndef CONSTS_H
#define CONSTS_H

/* Item type codes (item_effect.type) */
#define ITEM_TYPE_SWORD         0x01
#define ITEM_TYPE_BLADE         0x02
#define ITEM_TYPE_SPEAR         0x03
#define ITEM_TYPE_AXE           0x04
#define ITEM_TYPE_BOW           0x05
#define ITEM_TYPE_STAFF         0x06
#define ITEM_TYPE_CLAW          0x07
#define ITEM_TYPE_MECH_ARM      0x08
#define ITEM_TYPE_ARMOR         0x10
#define ITEM_TYPE_ACCESSORY     0x18
#define ITEM_TYPE_CONSUMABLE    0x20

/* Item count */
#define ITEM_COUNT              215
#define ITEM_ID_MAX             0xD6

/* Spell count */
#define SPELL_COUNT             36
#define SPELL_ID_MAX            0x23

/* Job / class count */
#define JOB_COUNT               27
#define JOB_ID_MAX              0x1A

/* Character tables */
#define CHARACTER_BASE_COUNT    32
#define CHARACTER_GROWTH_COUNT  68
#define ENEMY_DATA_COUNT        68
#define SPELL_LEARNING_COUNT    20

/* Chapter count */
#define CHAPTER_COUNT           30
#define CHAPTER_INTRO_COUNT     26

/* runtime_char constants */
#define RUNTIME_CHAR_SIZE       0x50
#define INVENTORY_SLOT_COUNT    8

/* runtime_char.flags bits */
#define CHARFLAG_DEAD           0x01
#define CHARFLAG_CANNOT_ACT     0x04
#define CHARFLAG_ACTED          0x80

/* runtime_char.team values */
#define TEAM_ENEMY              0
#define TEAM_NPC                1
#define TEAM_PLAYER             2

/* inventory slot flag bits */
#define SLOT_EQUIPPED           0x40
#define SLOT_EMPTY              0x80

/* Portrait ID range */
#define PORTRAIT_ID_MAX         0x41

/* .object3 table addresses (LE runtime) */
#define ADDR_ITEM_EFFECT_TABLE          0x602ACuL
#define ADDR_SPELL_EFFECT_TABLE         0x619FDuL
#define ADDR_ENEMY_DATA_TABLE           0x61AF9uL
#define ADDR_CHARACTER_BASE_TABLE       0x61DA1uL
#define ADDR_CHARACTER_GROWTH_TABLE     0x620A1uL
#define ADDR_CHAPTER_INTRO_TABLE        0x6238DuL
#define ADDR_SPELL_LEARNING_TABLE       0x626B3uL

/* Dispatch table sizes */
#define CHAPTER_INIT_HANDLER_SLOTS      28
#define CHAPTER_END_HANDLER_SLOTS       30
#define CHAPTER_POST_ACTION_SLOTS       30
#define CHAPTER_EVENT_HANDLER_SLOTS     90
#define SPELL_DISPATCH_SLOTS            36

/* BIOS Data Area addresses (DOS/4G flat model, real-mode mem mapped linearly) */
#define BIOS_KBD_HEAD   (*(volatile uint16 *)0x41AuL)
#define BIOS_KBD_TAIL   (*(volatile uint16 *)0x41CuL)
#ifdef FD2_REPLAY
/* Deterministic virtual clock for replay (tests/play/replay.c). Every BIOS-tick
 * READ advances a counter, so animation phases derived from the tick (palette
 * cycle, portrait blink, dialog typewriter pacing) become a function of the
 * deterministic call sequence rather than wall-clock -- making framebuffer
 * captures byte-stable across runs. The tick-wait loops (fd2_wait_one_bios_tick /
 * fd2_wait_n_bios_ticks) still terminate because each read advances the counter.
 * Production (no FD2_REPLAY) reads the real BDA tick verbatim. */
uint32 fd2_replay_tick(void);
#define BIOS_TICK_COUNT  (fd2_replay_tick())
#define BIOS_TICK_WORD   ((uint16)fd2_replay_tick())
#else
#define BIOS_TICK_COUNT  (*(volatile uint32 *)0x46CuL)
#define BIOS_TICK_WORD   (*(volatile uint16 *)0x46CuL)
#endif

#endif /* CONSTS_H */
