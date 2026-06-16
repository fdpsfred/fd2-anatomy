#ifndef TYPES_H
#define TYPES_H

typedef unsigned char   uint8;
typedef signed char     int8;
typedef unsigned short  uint16;
typedef signed short    int16;
typedef unsigned long   uint32;
typedef signed long     int32;

/* ========================================================================
 * runtime_char  (80 bytes = 0x50)
 *
 * Per-unit runtime state. Array at 0x53A45, indexed by unit_id.
 * Layout from program_info/overview.md.
 * ======================================================================== */
#pragma pack(1)

typedef struct {
    uint8  pos_x;                       /* +0x00 */
    uint8  pos_y;                       /* +0x01 */
    uint8  sprite_state[3];             /* +0x02  [0]=cache_idx [1]=facing [2]=walk_phase */
    uint8  flags;                       /* +0x05  bit0=dead bit2=cannot_act bit7=acted */
    uint8  team;                        /* +0x06  0=enemy 1=npc 2=player */
    uint8  portrait_id;                 /* +0x07 */
    uint8  char_id;                     /* +0x08 */
    uint8  reserved_09;                 /* +0x09 */
    uint8  inventory_slots[16];         /* +0x0A  8 slots x (flag, item_id) */
    uint8  spells_known_bitmap[5];      /* +0x1A  40 spells x 1 bit */
    uint8  archetype_flag;              /* +0x1F */
    uint8  job_id;                      /* +0x20 */
    uint8  status_flags_block[5];       /* +0x21  [0]=level [1]=ap_buff [2]=dp_buff [3]=dx_buff [4]=poison */
    uint8  status_sleep_flag;           /* +0x26 */
    uint8  combat_aux_block[21];        /* +0x27  [0]=silence ... [0xD]=ai_class ... see KB */
    uint8  movement_order;              /* +0x3C  0=moved 0xFF=not_moved */
    uint8  ai_target_and_dx_block[3];   /* +0x3D  [0]=ai_target_id [1..2]=dx_total */
    uint16 hp_current;                  /* +0x40 */
    uint16 hp_max;                      /* +0x42 */
    uint16 mp_current;                  /* +0x44 */
    uint16 mp_max;                      /* +0x46 */
    uint16 ap;                          /* +0x48 */
    uint16 dp;                          /* +0x4A */
    uint16 dx_current;                  /* +0x4C */
    uint16 stat4_current;               /* +0x4E */
} runtime_char;

/* ========================================================================
 * item_effect  (23 bytes)
 *
 * .object3 table @ 0x602AC, 215 entries.
 * Layout from assets/tables/item_effect.md.
 * ======================================================================== */
typedef struct {
    uint8  unknown_00;          /* +0  prefix byte */
    uint8  type;                /* +1  item type (01=sword..20h=consumable) */
    uint16 ap;                  /* +2  attack bonus */
    uint16 ht;                  /* +4  hit rate bonus */
    uint16 dp;                  /* +6  defense bonus */
    uint16 ev;                  /* +8  evasion bonus */
    uint8  special_type;        /* +10 02=poison 03=double 04=crit */
    uint8  special_chance;      /* +11 % */
    uint8  range_min;           /* +12 */
    uint8  range_max;           /* +13 */
    uint8  use_effect;          /* +14 */
    uint8  use_param_lo;        /* +15 */
    uint8  use_param_hi;        /* +16 */
    uint8  cast_range_flags;    /* +17 bit4=line, low4=distance */
    uint8  target_side;         /* +18 0=enemy 1=self */
    uint8  area;                /* +19 max 3 */
    uint16 price;               /* +20 */
    uint8  trailing_22;         /* +22 */
} item_effect;

/* ========================================================================
 * spell_effect  (7 bytes)
 *
 * .object3 table @ 0x619FD, 36 entries.
 * Layout from assets/tables/spell_effect.md.
 * ======================================================================== */
typedef struct {
    uint16 damage;              /* +0  max damage / heal power */
    uint8  hit_rate;            /* +2  % */
    uint8  cast_range_flags;    /* +3  bit4=line, low4=distance */
    uint8  area;                /* +4  max 3 */
    uint8  mp_cost;             /* +5 */
    uint8  target_side;         /* +6  0=enemy 1=self */
} spell_effect;

/* ========================================================================
 * enemy_data  (10 bytes)
 *
 * .object3 table @ 0x61AF9, 68 entries.
 * Layout from assets/tables/enemy_data.md.
 * ======================================================================== */
typedef struct {
    uint8  race_id;             /* +0 */
    uint8  class_id;            /* +1 */
    uint16 hp;                  /* +2 */
    uint8  mp;                  /* +4 */
    uint8  ap;                  /* +5 */
    uint8  dp;                  /* +6 */
    uint8  dx;                  /* +7 */
    uint8  mv;                  /* +8 */
    uint8  exp_reward;          /* +9 */
} enemy_data;

/* ========================================================================
 * character_base  (24 bytes)
 *
 * .object3 table @ 0x61DA1, 32 entries.
 * Layout from assets/tables/character_base.md.
 * ======================================================================== */
typedef struct {
    uint8  race_id;             /* +0  */
    uint8  class_id;            /* +1  */
    uint8  level;               /* +2  starting level */
    uint16 hp;                  /* +3  base HP */
    uint16 mp;                  /* +5  base MP */
    uint8  mv;                  /* +7  movement */
    uint8  initial_spells[4];   /* +8  spell IDs (0=none) */
    uint8  initial_items[6];    /* +12 item IDs (0xFF=empty) */
    uint16 ap;                  /* +18 base attack */
    uint16 dp;                  /* +20 base defense */
    uint16 dx;                  /* +22 base evasion */
} character_base;

/* ========================================================================
 * character_growth  (11 bytes)
 *
 * .object3 table @ 0x620A1, 68 entries.
 * Layout from assets/tables/character_growth.md.
 * ======================================================================== */
typedef struct {
    uint8  ap_min;              /* +0  per-level AP growth min */
    uint8  ap_max;              /* +1  exclusive upper bound */
    uint8  dp_min;              /* +2  */
    uint8  dp_max;              /* +3  */
    uint8  dx_min;              /* +4  */
    uint8  dx_max;              /* +5  */
    uint8  hp_min;              /* +6  */
    uint8  hp_max;              /* +7  */
    uint8  mp_min;              /* +8  */
    uint8  mp_max;              /* +9  */
    uint8  spell_learning_idx;  /* +10 index into spell_learning_table; 0xFF=none */
} character_growth;

/* glyph_blit_state @ 0x627A3 -- shared scratch render-state for the 1bpp glyph
 * blitter (fd2_blit_glyph_2bpp_with_outline) and the stride-blit pair
 * (fd2_blit_sprite_with_stride_setup / _loop). Inside the packed region, so it
 * is 17B alignment-1, byte-identical to the Ghidra struct layout. */
typedef struct {
    uint16 wPitch;          /* +0  destination row stride (bytes) */
    uint8  bFill_color;     /* +2  glyph body palette index */
    uint8  bBg_color;       /* +3  background fill palette index (0 = skip) */
    uint8  bOutline_color;  /* +4  drop-shadow palette index */
    uint8 *pDst_buf;        /* +5  destination base linear address */
    uint8 *pFont_data;      /* +9  1bpp font sheet base */
    int32  nGlyph_idx;      /* +13 glyph index into the font sheet */
} glyph_blit_state;

#pragma pack()

#endif /* TYPES_H */
