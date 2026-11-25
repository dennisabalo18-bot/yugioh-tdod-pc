/*
 * mascota.c - the duel mascots (idea 9 of the Master Duel list, 2026-11-14,
 * the user: "una mascota para cada jugador ... encima del deck extra"): a 3D
 * monster of the game standing over each player's Extra Deck, with its own
 * idle animation, made with the very "viewer" the battle uses. First step:
 * one model for both (Kuriboh, m057), the opponent's mirrored like the
 * attacked monster of a battle. F2 / Configuración "Mascotas en el duelo".
 *
 * All read from the game's code (analysis/disasm/asm/text.asm):
 * - The duel scene sub_000444B0 (this = scene, [0x656F88]) builds the two
 *   battle viewers at +0x74 / +0x78 (0x44AF0..0x44B97): an object of 0xD0
 *   (sub_00057040), the viewer of 0x138 (sub_00185210(size, sub_00192370(
 *   scene)) for memory), sub_0005B7E0(viewer; [scene+0x7C], that object,
 *   [scene], side == 1) and sub_00058760(viewer; 0). The same is done here
 *   for each mascot right after the scene is built.
 * - The battle loads a model (0x76041): "monster/%03.3d/m%03.3d.igb" with
 *   n / 100 * 100 and n, sub_0001DB20(n, 1) (n's bit in the use map),
 *   sub_0001ADF0(path, 1, 0, 0, 0) -> handle; ready when sub_00018FE0() and
 *   sub_000199C0(handle) say so (sub_00075E00); the node: sub_00019AF0(
 *   handle) -> root, sub_00013610(&ptr; root) (sub_00076350). The viewer
 *   takes it with sub_0005A670(viewer; node, n) and is switched on with
 *   sub_00057020(viewer; 1) (sub_0007D230 at 0x7D56E..0x7D5C4).
 * - Its camera: sub_000583A0(viewer; position, target, up), 9 floats by
 *   value; the battle's per-model table at 0x42A718 (0x98 bytes = 19
 *   doubles: 0, target A, position A, up A, target B, position B, up B).
 *   The mascot takes the A one (where the battle's camera starts).
 * - Each frame the duel draws the battle viewers with sub_00056FC0(viewer;
 *   r) at 0x46111 / 0x46129 (only a switched-on one renders); the mascots
 *   are drawn there too, after the second one.
 * - On screen: sub_00057D60 builds the viewer's quad (8 strips of 4 points),
 *   at rest (the battle's wipe over, see cuadro()). While it runs for a
 *   mascot, each point's texture coordinate call (vtable +0xB0) is caught
 *   (recomp_lookup_manual sees every indirect call) and the point is placed
 *   from it into the mascot's box over the Extra Deck.
 * Lines "[MASCOTA] ..." in registro.txt for each step.
 */
#include "mods_carpeta.h"
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <windows.h>
#include "recomp/gen/recomp_types.h"
void xbox_TscX1(int on);                /* kit: a speed-1 clock while on (kernel_pacing.h) */

typedef void (*mfunc_t)(void);
recomp_func_t recomp_lookup(uint32_t xbox_va);
/* the kit (nv2a_pb_d3d11.c): alpha 1 into the 480x480 targets. 2026-11-14
 * 20:58 capture: even the mascot at the battle's own size showed only the
 * Blue-Eyes' claws and edges. In the battle an opaque background goes into
 * each viewer's picture first (the attribute's effect), so the low alpha a
 * model writes on its body never shows; a mascot has no background. */
extern int g_nv2a_alpha_uno_480;

void sub_00057D60_pad(void);
void sub_00057020_pad(void);
void sub_000583A0_pad(void);
void sub_000444B0_pad(void);
void sub_00056FC0_pad(void);
void sub_00057D60(void);
void sub_00057020(void);
void sub_000583A0(void);
void sub_00192370(void);
void sub_00185210(void);
void sub_00057040(void);
void sub_0005B7E0(void);
void sub_00058760(void);
void sub_00018FE0(void);
void sub_0001DB20(void);
void sub_0001ADF0(void);
void sub_000199C0(void);
void sub_00019700(void);
void sub_00019AF0(void);
void sub_00013610(void);
void sub_0005A670(void);
void sub_00043800_pad(void);
void sub_00044360_pad(void);
void sub_00058420(void);

/* ---- options ----------------------------------------------------------- */
static volatile LONG s_on = 1;
int ygo_mascota_get(void) { return s_on ? 0 : 1; }
void ygo_mascota_set(int i) { InterlockedExchange(&s_on, i == 0); }

/* The models (monster/%03d/m%03d.igb numbers): the first five the user's
 * (2026-11-14: Kuriboh, Blue-Eyes, Dark Magician, "suma jinzo y craneo
 * convocado"), ten more when asked "se puede poner mas?". Each number from
 * the game's card -> model table (0x44FBB0, tools/cartas_mod.py modelo_de),
 * each .igb checked in Game\monster: Red-Eyes 4088, Dark Magician Girl 4766,
 * Gaia the Fierce Knight 4044, Celtic Guardian 4047, Harpie Lady 4068, Time
 * Wizard 4022, Baby Dragon 4010, Flame Swordsman 4021, Black Luster Soldier
 * 4370, Buster Blader 4983. 2026-11-15 (the user: "faltaria Exodia" and
 * each character's own monster): Exodia 4027, Barrel Dragon 4749 (Bandit
 * Keith), Two-Headed King Rex 4038 (Rex), Perfectly Ultimate Great Moth
 * 4073 (Weevil), The Legendary Fisherman 5069 (Mako), Thousand-Eyes
 * Restrict 4740 (Pegasus), Dark Necrofear 5222 (Yami Bakura), The Masked
 * Beast 5094 (Lumis & Umbra), Cyber Commander 4427 (Tristan), Dragon Zombie
 * 4103 (Bonz). Strings' (Slifer), Ishizu's and Odion's have no 3D model in
 * the game. 2026-11-15 (the user: Exodia "sale solo la cabeza", the 3D one
 * of the Exodia win): the whole Exodia is the game's model 737, kept apart as
 * effect/special_igb/se002.igb (sub_000662A0 loads it by sub_000625C0(9):
 * sub_0001ADF0(path, 1, 0, 1, 0), no sub_0001DB20; its node is named "737"
 * and the camera table has an entry 737), with arms and legs; it takes the
 * place of the head (20). Bakura's own (his cf_anim/bg/cfbg07.png
 * background): Maha Vailo 4499, model 492; Shadi's (cfbg25, the user):
 * Millennium Golem 4459, model 452. Shimon's, Marik's and
 * Yami Marik's backgrounds are spells / traps (Pot of Greed, Mystical Space
 * Typhoon, Dark Hole): no model, they keep theirs. The player picks one (F2 / Configuración "Tu mascota",
 * Blue-Eyes by default; main.c's k_mascota_tuya in this order); each
 * opponent has its own (k_rival). */
#define NMODELOS 100
static const int k_modelo[NMODELOS] = {
    0,   /* Blue-Eyes */       57,  /* Kuriboh */          34,  /* Dark Magician */ 960, /* Dark Magician II (16:2x "al lado de mago oscuro") */
    854, /* Jinzo */           21,  /* Summoned Skull */   81,  /* Red-Eyes */
    774, /* Dark Magician Girl */ 37, /* Gaia */           40,  /* Celtic Guardian */
    61,  /* Harpie Lady */     15,  /* Time Wizard */      3,   /* Baby Dragon */
    14,  /* Flame Swordsman */ 363, /* Black Luster Soldier */ 860, /* Buster Blader */
    20,  /* Exodia's head; 16:1x the whole one (737) taken out: "deja la cabeza como antes" */  739, /* Barrel Dragon */    31,  /* Two-Headed King Rex */
    66,  /* Great Moth */      921, /* Legendary Fisherman */ 903, /* Thousand-Eyes Restrict */
    930, /* Dark Necrofear */  902, /* The Masked Beast */ 722, /* 24 Slot Machine (card 4729; was Cyber Commander 420, 2026-11-21) */
    96,  /* Dragon Zombie */   492, /* Maha Vailo */       452, /* Millennium Golem */
    /* 2026-11-15 (the user: "si, sumalos"): more of the game's 3D models,
     * the strong and known ones (each .igb checked in Game\monster) */
    379, /* Blue-Eyes Ultimate Dragon */
    216, /* B. Skull Dragon */
    721, /* Magician of Black Chaos */
    712, /* Meteor B. Dragon */
    863, /* Valkyrion the Magna Warrior */
    900, /* Blue-Eyes Toon Dragon */
    36, /* Gaia the Dragon Champion */
    931, /* Dark Sage */
    612, /* Twin-Headed Thunder Dragon */
    704, /* Tri-Horned Dragon */
    707, /* Cosmo Queen */
    359, /* Zera the Mant */
    706, /* Skull Knight */
    /* 2026-11-15 (the user: "si todas"): 25 more, up to 65 */
    385, /* Harpie's Pet Dragon */
    373, /* Gate Guardian */
    901, /* Relinquished */
    728, /* Toon Summoned Skull */
    38, /* Curse of Dragon */
    68, /* Thousand Dragon */
    855, /* Alpha The Magnet Warrior */
    856, /* Beta The Magnet Warrior */
    857, /* Gamma The Magnet Warrior */
    711, /* Meteor Dragon */
    1, /* Mystical Elf */
    573, /* Witch of the Black Forest */
    918, /* Mystic Tomato */
    47, /* Sangan */
    908, /* Big Shield Gardna */
    97, /* 56 Clown Zombie (card 4104; was Masaki 292, 2026-11-21) */
    135, /* Witty Phantom */
    427, /* Magician of Faith */
    946, /* 59 Humanoid Slime (card 5060; was Machine King 406, 2026-11-21) */
    926, /* Fire Princess */
    62, /* Harpie Lady Sisters */
    370, /* Sanga of the Thunder */
    371, /* Kazejin */
    372, /* Suijin */
    /* 2026-11-16 (the user: "acomoda la mascota de todos", from each one's
     * deck in the game's reward lists; "1 modelo mas para quedar en 70") */
    718, /* 65 Dokurorider (Tristan) */
    859, /* 66 Dunames Dark Witch (Ishizu) */
    845, /* 67 Slate Warrior (Odion) */
    905, /* 68 Humanoid Worm Drake (Strings) */
    945, /* 69 Vorse Raider (Seto's, Marik's decks) */
    /* 2026-11-19 (the user: "vamos a agregar ... 100"): 30 more, each with its
     * sleeve and tapete (the full combo); every .igb checked in Game\monster */
    924, /* 70 Goblin Attack Force (card 5145) */
    531, /* 71 Gemini Elf (card 4538) */
    906, /* 72 St. Joan (card 5208) */
    390, /* 73 Zoa (card 4397) */
    953, /* 74 Melchid the Four-Face Beast (card 5095; was Ryu-Ran 725, 2026-11-21) */
    384, /* 75 Bickuribox (card 4391) */
    550, /* 76 Dark Elf (card 4557) */
    709, /* 77 Crab Turtle (card 4716) */
    715, /* 78 Garma Sword (card 4722) */
    716, /* 79 Javelin Beetle (card 4723) */
    369, /* 80 Labyrinth Tank (card 4376) */
    717, /* 81 Fortress Whale (card 4724) */
    705, /* 82 Serpent Night Dragon (card 4712) */
    708, /* 83 Chakra (card 4715) */
    907, /* 84 The Fiend Megacyber (card 4761) */
    500, /* 85 Man-Eater Bug (card 4507) */
    911, /* 86 Cyber Jar (card 4913) */
    762, /* 87 Airknight Parshath (card 5023) */
    601, /* 88 Penguin Soldier (card 4608) */
    590, /* 89 Morphing Jar (card 4597) */
    515, /* 90 Muka Muka (card 4522) */
    73, /* 91 Giant Soldier of Stone (card 4080) */
    365, /* 92 Labyrinth Wall (card 4372) */
    468, /* 93 Armed Ninja (card 4475) */
    627, /* 94 Witch's Apprentice (card 4634) */
    933, /* 95 Gilasaurus (card 5256) */
    553, /* 96 Lava Battleguard (card 4560) */
    357, /* 97 Seiyaryu (card 4364) */
    746, /* 98 Panther Warrior (card 4751) */
    366, /* 99 Jirai Gumo (card 4373) */
};
static volatile LONG s_tuya = 0;
int ygo_mascota_tuya_get(void) { return (int)s_tuya; }
void ygo_mascota_tuya_set(int i) { InterlockedExchange(&s_tuya, i >= 0 && i < NMODELOS ? i : 0); }

/* Each opponent's, by character (0x66DCDC + 4, what the opponent select
 * hands to the duel: sub_001037D0), from the monster each one is known for:
 * 0 Blue-Eyes, 1 Kuriboh, 2 Dark Magician, 3 Jinzo, 4 Summoned Skull,
 * 5 Red-Eyes, 6 Dark Magician Girl, 7 Gaia, 8 Celtic Guardian, 9 Harpie
 * Lady, 10 Time Wizard, 11 Baby Dragon, 12 Flame Swordsman, 13 Black Luster
 * Soldier, 14 Buster Blader, 15 Exodia, 16 Barrel Dragon, 17 Two-Headed King
 * Rex, 18 Great Moth, 19 Legendary Fisherman, 20 Thousand-Eyes Restrict, 21
 * Dark Necrofear, 22 The Masked Beast, 23 Cyber Commander, 24 Dragon Zombie,
 * 25 Maha Vailo, 26 Millennium Golem, 27.. the 2026-11-15 list (only for "Tu
 * mascota").
 * Grandpa (Trusdale) and the Rare Hunter: Exodia, as in the series.
 * Characters 0..26 are the names texts 86..112 (the default.xbe list: Yami
 * Yugi .. Shimon, the "Quién da" portraits cfsNN). From 27 on the select's
 * characters go their own way (s8 0x423D60 -> duelist: 27..30, 34, 35 -> 27;
 * 31 -> 29; 32 -> 30; 33 -> 31): the Duel Computer is character 32
 * (2026-11-14 log: "[PREMIOS] slot 9 personaje 32 rival 30"; it got a
 * Kuriboh instead of its Jinzo). The others there: Kuriboh. */
/* 2026-11-21 (the user: "usamos cada fondo de personaje para saber que
 * mascota tiene"): each one's dialog background (Game/cf_anim/bg/cfbgNN.png,
 * NN = the character) shows its monster; Yugi, Tristan and Pegasus had
 * another one */
static const signed char k_rival[36] = {
    2, /* Yami Yugi */      0, /* Seto */           6, /* Joey */          1, /* Yugi: Kuriboh (his cfbg03) */
   60, /* Tea: Fire Princess */ 70, /* Tristan: Goblin Attack Force (cfbg05) */ 16, /* Trusdale */ 26, /* Bakura */
   22, /* Yami Bakura */   43, /* Pegasus: Relinquished (cfbg09) */        10, /* Mai */           24, /* Bandit Keith: Slot Machine (cfbg11) */
   20, /* Mako */          18, /* Rex Raptor */    19, /* Weevil */        56, /* Bonz: Clown Zombie (cfbg15) */
    4, /* Espa Roba */     16, /* Rare Hunter */    3, /* Arkana: Dark Magician II (2026-11-22, the user: "debe llevar el mago oscuro 2") */ 23, /* Lumis & Umbra */
   59, /* Strings: Humanoid Slime (cfbg20) */ 66, /* Ishizu: Dunames */ 67, /* Odion: Slate Warrior */ 5, /* Marik */
    5, /* Yami Marik */    27, /* Shadi */         53, /* Shimon: Mystic Tomato */         4, /* 27 (named Duel Computer) */
    1, 1, 1, 1,                                                             /* 28..31 */
    4, /* 32 Duel Computer */ 1, 1, 1,                                      /* 33..35 */
};

/* the opponent's, as k_modelo's index (fundas.c: their sleeve) */
int ygo_mascota_rival(void)
{
    uint32_t ch = MEM32(0x0066DCDCu + 4u);
    return ch < 36u ? k_rival[ch] : 1;
}

/* carta_as.c: the model of each side's mascot (0 yours, 1 the opponent's) */
/* perfil.c's "Ver Invocación": the model of mascot i */
int ygo_mascota_modelo_de(int i) { return i >= 0 && i < NMODELOS ? k_modelo[i] : -1; }

/* 2026-11-20 (the user: "si uso el deck prestado tambien tenga su mascota,
 * su funda y tapete"): with an opponent's deck (prestado.c) your side's
 * mascot is that opponent's */
int ygo_prestado_en_duelo(void);
int ygo_mascota_de_personaje(int ch) { return ch >= 0 && ch < 36 ? k_rival[ch] : 1; }
static int tuya_idx(void)
{
    int ch = ygo_prestado_en_duelo();
    if (ch >= 0)
        return ygo_mascota_de_personaje(ch);
    return s_tuya >= 0 && s_tuya < NMODELOS ? (int)s_tuya : 0;
}

int ygo_mascota_modelo_lado(int lado)
{
    uint32_t ch;
    if (lado == 0)
        return k_modelo[tuya_idx()];
    ch = MEM32(0x0066DCDCu + 4u);
    return k_modelo[ch < 36u ? k_rival[ch] : 1];
}

/* the opponent's second mascot (k_modelo index), -1 none */
/* (2026-11-21 01:40: a forced test put four in two duels; the log had the
 * four viewers destroyed and the four models let go each time) */
static int segunda(uint32_t ch)
{
    return ch == 19u ? 74 : -1;         /* Lumis & Umbra: Melchid the Four-Face Beast (cfbg19 shows the Masked Beast) */
}

static int modelo_de(int i)
{
    uint32_t ch;
    if (i == 2) {
        int k = segunda(MEM32(0x0066DCDCu + 4u));
        return k >= 0 ? k_modelo[k] : -1;
    }
    if (i == 3) {                       /* yours, with a loaned deck (prestado.c) */
        int ch = ygo_prestado_en_duelo(), k = ch >= 0 ? segunda((uint32_t)ch) : -1;
        return k >= 0 ? k_modelo[k] : -1;
    }
    if (i == 0) {
        /* 2026-11-20 23:5x: this runs every frame; the 23:41 log had 66499
         * copies of the line, so it's logged only when it changes */
        static int s_ult = -1;
        int t = tuya_idx();
        int p = t != (int)s_tuya ? t : -1;
        if (p != s_ult) {
            s_ult = p;
            if (p >= 0)
                fprintf(stderr, "[MASCOTA] tuya: la del mazo prestado (%d)\n", t);
        }
        return k_modelo[t];
    }
    ch = MEM32(0x0066DCDCu + 4u);
    fprintf(stderr, "[MASCOTA] rival: personaje %u\n", ch);
    return k_modelo[ch < 36u ? k_rival[ch] : 1];
}

/* ---- where they stand ----------------------------------------------------- */
/* 2026-11-14 (the user, 23:00: "de tamaño y ubicacion creo que es perfecto"):
 * the 640x480 frame is stretched over the window (column x 3, row x 2.25 on
 * 1920x1080); the name plates: "Dennis" columns 27..153, rows 240..271, the
 * opponent's columns 483..613, rows 118..149. The Blue-Eyes as liked:
 * camera 3x as far as the battle's camera A, its flying reach u 48..288,
 * v 98..346 of the picture, 0.5 frame px per picture px: 120 x 124 frame px,
 * the player's standing on row 236 centred on column 90, the opponent's
 * hanging from row 155 centred on column 565.
 * Any model (23:40): the kit measures where the model is in its picture
 * (g_nv2a_mascota, while it is hidden during the duel's opening) and it is
 * put there the same way: the largest scale that keeps it within 120 x 124;
 * cut by the picture's edge, the camera goes 1.35x farther and it is measured
 * again; small in it, 1.3x nearer (more detail). Kept per model for the
 * next duels (s_encuadre). */
#define LEJOS0 3.0
/* The whole Exodia (model 737, se002.igb): 2026-11-15 01:10 run, at 20 x its
 * camera's distance it took u 238..251, v 236..259 of the 480 picture (13 x
 * 23 px: blown up 9 x, the user's capture all pixels). The others fill
 * ~200 rows: 20 x 23 / 200 = 2.3. The measure taken at 20 is dropped
 * (encuadres_leer). */
#define EXODIA_ENTERO 737
#define LEJOS_EXODIA 2.3
/* 2026-11-15 (the user: the 19 s loop "se nota" and it ran over the life
 * points; "dejarlo quieto ... que se mueva poco como cuando respira"): the
 * mascot loads se002_mascota.igb, its own animation held at 7.6 s with a
 * +-0.12 s sway (tools/modelos/exodia_mascota.py; se002.igb stays for the
 * win). Its measures go under their own key (CLAVE), so the ones taken
 * while it moved are not used. */
/* 2026-11-15 (the user: the punch "solo para cuando ganas con exodia, no en
 * cada ataque porque cansa visualmente"): the Exodia win's scene
 * (sub_000662A0: it loads se000 / se002 / se001) sets s_exodia_gana; a
 * mascot that is the whole Exodia, placed, then loads se002_ataque.igb (the
 * punch again and again, same measure) until the duel ends. */
static volatile LONG s_exodia_gana;
static ULONGLONG s_crear_t;              /* the duel scene was built then (crear) */
/* the kit: a golden glow and ring over the final picture (present_blit), per
 * mascot: frame x, y, glow radius, glow strength, ring radius, ring strength */
extern volatile float g_nv2a_destello[2][6];
/* "Entrada del duelo" (F2 / Configuración, 2026-11-15, the user's video: a
 * Master Duel-like arrival, Egyptian): 0 Completa, as the doors open (state
 * 3) the whole picture starts VIAJE_ZOOM x and settles to its size in
 * VIAJE_MS, and the mascots rise in with the plates (state 5); 1 Corta, only the
 * mascots' rise; 2 Original, as the game was (the mascots at state 6, no
 * rise). The lens is the matrix at [this+0xC] that sub_0005C3F0 copies to
 * the temple's camera each frame: 2.414 / 3.219 on its diagonal (05:16 log),
 * the same in states 3..6; scaling those two widens the view. */
#define VIAJE_ENTRA_MS 160u               /* the push in, while the doors are shut */
#define VIAJE_MS 1300u
#define VIAJE_ZOOM 1.6f                   /* the picture at the start, x its size */
extern volatile float g_nv2a_zoom;       /* the kit: the whole picture enlarged (present_blit) */
static volatile LONG s_entrada_modo;
int ygo_entrada_get(void) { return (int)s_entrada_modo; }
void ygo_entrada_set(int i) { InterlockedExchange(&s_entrada_modo, i >= 0 && i <= 2 ? i : 0); }
static ULONGLONG s_viaje_t0;              /* state 3 first seen this duel (0: not yet) */
void sub_000662A0_pad(void);
void sub_000662A0(void)
{
    if (!s_exodia_gana) {
        InterlockedExchange(&s_exodia_gana, 1);
        fprintf(stderr, "[MASCOTA] escena de Exodia: golpea\n");
    }
    sub_000662A0_pad();
}
/* HARPIAS: its own camera angle (camara()), so its own measures too */
#define HARPIAS 62
#define CLAVE(n, i) ((n) * 2 + (i) + ((n) == EXODIA_ENTERO ? 100000 : 0) + ((n) == HARPIAS ? 200000 : 0))
#define ANCHO 120.f
#define ALTO 124.f
#define TOPE_RIVAL 162.f                /* the opponent's top row (was k_pie's 155) */
/* 2026-11-15 ("como sale la maga oscura desde ahi"): each one starts where
 * her left edge is (column 60) and grows right, the wide ones moved left off
 * the life points (TOPE_DER); floating over the name as she does (bottom row
 * 226, not 236 on the plate). The opponent's the same way mirrored: its
 * right edge at column 620, its left no further than 508 (the purple
 * diamond ends at 503). */
#define PIE_JUGADOR 226.f
/* 2026-11-15 00:40 ("siguen saliendo de lados distintos como el craneo que
 * sale mas hacia la derecha en vez del centro como la maga"): centred where
 * she is (column 82, her left edge at 60), not grown right from her edge;
 * the opponent's centred on the mirrored spot. */
/* 2026-11-15 (the user: "cada mascota sale en un lado diferente ... todas
 * centradas arriba del nombre"): each one centred on its name plate, the
 * player's columns 27..153, the opponent's 483..613; no more moving the wide
 * ones left off the life points (TOPE_DER) nor by-eye shifts. */
#define CENTRO_JUGADOR 90.f
#define CENTRO_RIVAL 548.f
#define IZQ_RIVAL 508.f
#define DER_RIVAL_MAX 632.f

struct nv2a_mascota_kit {               /* nv2a_pb_d3d11.c: keep both alike */
    volatile float x0, x1, y0, y1;
    volatile int activa, ocultar, pedido, medido;
    volatile uint32_t off;
    volatile int bb[4];
    volatile int pies;
};
/* 2026-11-21 (the user: Lumis & Umbra "pueden llevar 2 mascotas solo ellos",
 * mock-up B): a third viewer, the opponent's second mascot, only for a
 * character in segunda(); on the opponent's side (LADO 1), smaller, in
 * front of and left of the first. Index 2 below; the kit's flash, aura and
 * tone stay for 0 and 1 only. */
/* 2026-11-21 01:5x (the user: "falta que se usen las 2 cuando el jugador
 * pida prestado el deck"): a fourth, index 3, your side's second, with a
 * loaned deck whose owner has one (segunda()); the pair mirrored on your
 * side (muestras/lumis_prestado.png) */
#define NMASC 4
#define LADO(i) ((i) == 1 || (i) == 2)
#define SEGUNDA(i) ((i) >= 2)
/* i is one of a pair: a second, or a first with its second there */
#define PAREJA(i) (SEGUNDA(i) || ((i) == 1 && s_m[2].v) || ((i) == 0 && s_m[3].v))
extern struct nv2a_mascota_kit g_nv2a_mascota[NMASC];

/* n: model * 2 + side. Seeded (2026-11-15 00:00 run, the user went through
 * the whole list: each took a measure of 2..10 s while hidden, "demoran unos
 * segundos en cargar (10s)") with the player's side measured at camera x3;
 * the rest is measured the first time it shows and kept. */
/* 2026-11-15 02:09 (the user: the new mascots took long the first time and
 * again each time after): the table held 40 and the file had 40 lines, so
 * from the 41st on a measure was neither kept nor saved and each model was
 * measured anew every time. 41 models x 2 sides, with room to spare. */
#define NENCUADRE 200
/* bb[4]: the feet's column (2026-11-15), -1 not measured (the older lines) */
static struct { int n; double lejos; int bb[5]; } s_encuadre[NENCUADRE] = {
    /* 2026-11-15: every model at the Dark Magician Girl's camera angle
     * (camara): the measures taken at each one's own angle no longer hold;
     * hers does (same angle). The others are measured once more (~1 s the
     * first time each shows) and kept in the file. */
    { 774 * 2, 3.0, { 158, 156, 265, 465, -1 } },  /* Dark Magician Girl */
};
static int s_nencuadre = 1;

enum { M_NADA, M_CARGA, M_LISTA, M_ERROR };
static struct {
    uint32_t v, h, d0;                  /* viewer, model handle, its 2D renderer */
    uint32_t viejo;                     /* the model it replaces, let go once the new one is on */
    int estado;
    int dibujada;                       /* its picture rendered once */
    int n;                              /* model */
    int encuadrada;                     /* placed from a measure (or given up) */
    int intentos, alejada;
    int ataque;                         /* Exodia: the punching one (se002_ataque.igb) */
    ULONGLONG entrada;                  /* its entrance began then (0: not yet this duel / model) */
    float vx, vy, vh;                   /* the model's middle and height on the frame (the flash) */
    float vw;                           /* and its width (the click on it) */
    int entrada_hecha;
    double lejos;
    float esc, cx, cy;                  /* picture px -> frame px, its centre */
    ULONGLONG desde;
} s_m[NMASC];
static uint32_t s_escena;
static ULONGLONG s_fin_t;               /* the duel decided then (fin_duelo), 0: not yet */
static int s_fin_gana = -1, s_fin_hecho[2];
static void fin_limpiar(void);

/* ---- calling the game -------------------------------------------------- */
static uint32_t gcall(mfunc_t f, uint32_t self, int n, const uint32_t *a)
{
    uint32_t sp = g_esp, r;
    int i;
    for (i = n - 1; i >= 0; i--)
        PUSH32(g_esp, a[i]);
    PUSH32(g_esp, 0u);
    g_ecx = self;
    f();
    r = g_eax;
    g_esp = sp;
    return r;
}

/* 2026-11-21 (the 04:22 whole-process profile: NtQueryVirtualMemory was 5.3 %
 * of the game's main thread, from these checks, several a frame): the
 * readable regions already seen are kept 250 ms (8 of them), so most checks
 * ask the system nothing. A region found readable stays so for that long. */
static int rd_ok(uint32_t a, uint32_t len)
{
    static struct { uintptr_t lo, hi; ULONGLONG t; } vistas[8];
    static unsigned prox;
    MEMORY_BASIC_INFORMATION mi;
    uintptr_t p = (uintptr_t)XBOX_PTR(a);
    ULONGLONG now;
    int i;
    if (a < 0x10000u)
        return 0;
    now = GetTickCount64();
    for (i = 0; i < 8; i++)
        if (p >= vistas[i].lo && p + len <= vistas[i].hi && now - vistas[i].t < 250u)
            return 1;
    if (!VirtualQuery((void *)p, &mi, sizeof mi) || mi.State != MEM_COMMIT
            || (mi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
        return 0;
    i = (int)(prox++ & 7u);
    vistas[i].lo = (uintptr_t)mi.BaseAddress;
    vistas[i].hi = (uintptr_t)mi.BaseAddress + mi.RegionSize;
    vistas[i].t = now;
    return p + len <= vistas[i].hi;
}

static float fb(uint32_t v) { float f; memcpy(&f, &v, 4); return f; }
static uint32_t bf(float f) { uint32_t v; memcpy(&v, &f, 4); return v; }

static int es_mascota(uint32_t v)
{
    return v && (v == s_m[0].v || v == s_m[1].v || v == s_m[2].v || v == s_m[3].v);
}

/* ---- the quad: sub_00057D60's strips ------------------------------------ */
/* 2026-11-14 21:21, the [T480] pictures (diagnostico_480\): the mascot's
 * 480x480 picture is a whole, opaque Blue-Eyes, as good as the battle's. What
 * cut it is the layer that puts it on screen (sub_0005B4A0): its stage 1 is
 * duel/mask_00.png (0x4299AC, sub_00059060(layer; 1, tex)), white on top,
 * black below with a spiky edge between: the battle's wipe. sub_00057D60
 * builds 8 strips of 4 points; each point's position and its stage-0
 * coordinate come from one table: rows 0, 230 - s, 405 - s, 480 - s, 480 with
 * s = (c4 - c0) / c4 * 480 when [v+0x45] = 1 (0x57D9D), columns 0..480; the
 * position is x = -(column + skew + side), y = row (the coordinate the
 * column and row as they are, 0x581A0). The mask's coordinates are fixed per
 * point. c0 counts down to 0 in the battle (sub_0005A460): at rest s = 480
 * and the picture is the strip of rows 0..480, the mask's white part. The
 * mascots had c0 = c4 (s = 0): the start of the wipe, only the spikes of
 * the mask over the monster -- the "strokes" and the "purple grass".
 * Now: c0 = 0, c4 = 20 (the battle at rest), and each point's place comes
 * from its stage-0 coordinate (vtable +0xB0, set 0): the picture's 480 x 480
 * into the mascot's box, coordinates outside the picture clamped to its
 * edge (those strips fold to nothing). Positions and colours as the game
 * makes them. */
static uint32_t s_setpos_va;            /* the vertex array's +0x5C (this; index, &vec3) */
static uint32_t s_setuv_va;             /* its +0xB0 (this; set, index, &vec2) */
static int s_modo;                      /* 0 off, 2 place */
static float s_caja[2], s_esc;          /* box centre (frame px), picture -> box */
static float s_kx = 1.f, s_ky = 1.f;    /* the reactions: across / up (negative across: turned round) */
static int s_puntos;

static float recorta(float t)
{
    return t < 0.f ? 0.f : t > 480.f ? 480.f : t;
}

static void setuv_mascota(void)
{
    uint32_t self = g_ecx, set = MEM32(g_esp + 4), idx = MEM32(g_esp + 8), p = MEM32(g_esp + 12);
    recomp_func_t real = recomp_lookup(s_setuv_va), pos = recomp_lookup(s_setpos_va);
    float u = 0.f, v = 0.f;
    int ok = set == 0 && rd_ok(p, 8) && pos;
    if (ok) {
        u = recorta(fb(MEM32(p)));
        v = recorta(fb(MEM32(p + 4)));
        MEM32(p) = bf(u);
        MEM32(p + 4) = bf(v);
    }
    if (real)
        real();
    if (ok) {
        uint32_t ra = g_eax, rc = g_ecx, rd = g_edx, a[2], sp = g_esp;
        float col = s_caja[0] + (u - 240.f) * s_esc * s_kx, row = s_caja[1] + (v - 240.f) * s_esc * s_ky;
        g_esp -= 0x10u;
        MEM32(g_esp) = bf(-col);        /* x = -column, as the game stores it */
        MEM32(g_esp + 4) = bf(row);
        MEM32(g_esp + 8) = bf(10.f);    /* the game's z (0x41200000) */
        a[0] = idx;
        a[1] = g_esp;
        gcall((mfunc_t)pos, self, 2, a);
        g_esp = sp;
        g_eax = ra; g_ecx = rc; g_edx = rd;
        s_puntos++;
    }
}

/* recomp_manual.c asks first on every indirect call */
recomp_func_t ygo_mascota_lookup(uint32_t va)
{
    if (s_modo == 2 && va == s_setuv_va)
        return setuv_mascota;
    return NULL;
}

/* The strips of mascot i, at rest, into its box. */
static int s_callado;                   /* cuadro() without its log line (the entrance, each frame) */

static void cuadro(int i)
{
    uint32_t v = s_m[i].v, s, g, va;
    s = MEM32(v + 0xC);
    if (!rd_ok(s, 0x14) || !rd_ok(g = MEM32(s + 0x10), 0x14) || !rd_ok(va = MEM32(g + 0x10), 4)
            || !rd_ok(MEM32(va), 0xB4)) {
        fprintf(stderr, "[MASCOTA] %d: las tiras del cuadro no se pueden leer (tira %08X)\n", i, s);
        return;
    }
    s_setpos_va = MEM32(MEM32(va) + 0x5C);
    s_setuv_va = MEM32(MEM32(va) + 0xB0);
    MEM8(v + 0x45) = 1;
    MEM32(v + 0xC0) = 0;                /* the wipe over: the battle at rest */
    MEM32(v + 0xC4) = 20;
    s_esc = s_m[i].esc;
    s_caja[0] = s_m[i].cx;
    s_caja[1] = s_m[i].cy;
    s_puntos = 0;
    s_modo = 2;
    gcall(sub_00057D60, v, 0, NULL);
    s_modo = 0;
    if (!s_callado)
    fprintf(stderr, "[MASCOTA] %d: cuadro en reposo (c0 0 / c4 20) centro %.0f,%.0f escala %.2f,"
            " %d puntos ubicados (+0x5C %08X, +0xB0 %08X)\n", i, s_caja[0], s_caja[1], s_esc,
            s_puntos, s_setpos_va, s_setuv_va);
    {
        struct nv2a_mascota_kit *m = &g_nv2a_mascota[i];
        float k = fabsf(s_kx) > s_ky ? fabsf(s_kx) : s_ky, h = 240.f * s_esc * (k > 1.f ? k : 1.f);
        m->x0 = -(s_caja[0] + h);
        m->x1 = -(s_caja[0] - h);
        m->y0 = s_caja[1] - h;
        m->y1 = s_caja[1] + h;
        m->activa = 1;
    }
}

/* ---- building, loading, drawing ------------------------------------------ */
/* model n for mascot i, as the battle asks for it (0x76041) */
void xbox_ContiguousUsageKB(uint32_t *in_use, uint32_t *peak);
static void soltar(uint32_t h)
{
    /* as the battle lets its two go when it ends (sub_00075DB0) */
    uint32_t a[1], u0 = 0, u1 = 0, pk = 0;
    if (h == 0xFFFFFFFFu)
        return;
    xbox_ContiguousUsageKB(&u0, &pk);
    gcall(sub_00018FE0, 0, 0, NULL);
    a[0] = h;
    gcall(sub_00019700, 0, 1, a);
    xbox_ContiguousUsageKB(&u1, &pk);
    fprintf(stderr, "[MASCOTA] modelo soltado (manija %08X): texturas %u -> %u KB\n", h, u0, u1);
}

/* 2026-11-20 (registro 20:37, bloque por bloque): los 21 bloques (~1,9 MB)
 * que quedaban de cada duelo son el modelo de TU mascota (m716): al soltar
 * su manija no vuelve nada; el de la del rival (m081, m000) si. m716 es
 * tambien una carta de tu mazo: el juego trae por su cuenta los modelos de
 * los monstruos de los mazos por la MISMA ruta (sub_0001D5F0 -> cola ->
 * sub_0001A4A0 / sub_0001AA20, tabla 0x631A50) y se cruza con la carga de la
 * mascota. 20:43 se probo guardarlo y reaprovecharlo: el juego lo solto por
 * su lado, la manija quedo apuntando a otra cosa y el juego se cerro en un
 * ataque (registro 20:48; la mascota 0 nunca quedo lista en el duelo 2).
 * Ahora la mascota se carga desde una copia propia del archivo con otro
 * nombre (monster/NNN/mNNNm.igb, copiada una vez en Game junto al original):
 * el juego nunca usa esa ruta, no hay cruce, y se suelta entera como la del
 * rival. Si no se puede copiar, la ruta de siempre. */
static int copia_mascota(int n, char *path, size_t cap)
{
    char exe[MAX_PATH], src[MAX_PATH], dst[MAX_PATH], *barra;
    DWORD k = GetModuleFileNameA(NULL, exe, MAX_PATH);
    if (!k || k >= MAX_PATH || !(barra = strrchr(exe, '\\')))
        return 0;
    *barra = 0;
    /* the card mod's model first (Mods-Extras\cartas\archivos, as the game
     * would read it), else the game's */
    snprintf(src, sizeof src, "%s%s\\cartas\\archivos\\monster\\%03d\\m%03d.igb", exe, ygo_mods_a(), n / 100 * 100, n);
    /* 2026-11-25 (review for the package): the folder that has default.xbe
     * (main.c's g_ygo_game_dir), which may be a subfolder of Game\; before,
     * exe\Game was assumed and a game pasted one folder deeper fell back to
     * the shared path that crashed (see above) */
    {
        extern const char *g_ygo_game_dir;
        char jd[MAX_PATH];
        if (g_ygo_game_dir && *g_ygo_game_dir)
            snprintf(jd, sizeof jd, "%s", g_ygo_game_dir);
        else
            snprintf(jd, sizeof jd, "%s\\Game", exe);
        if (GetFileAttributesA(src) == INVALID_FILE_ATTRIBUTES)
            snprintf(src, sizeof src, "%s\\monster\\%03d\\m%03d.igb", jd, n / 100 * 100, n);
        snprintf(dst, sizeof dst, "%s\\monster\\%03d\\m%03dm.igb", jd, n / 100 * 100, n);
    }
    {
        WIN32_FILE_ATTRIBUTE_DATA a, b;
        int nueva = !GetFileAttributesExA(dst, GetFileExInfoStandard, &b);
        if (!nueva && GetFileAttributesExA(src, GetFileExInfoStandard, &a)
                && (a.nFileSizeLow != b.nFileSizeLow || CompareFileTime(&a.ftLastWriteTime, &b.ftLastWriteTime) > 0))
            nueva = 1;                  /* the source changed (a mod update): copied again */
        if (!nueva)
            goto lista_copia;
    }
    {
        if (!CopyFileA(src, dst, FALSE)) {
            fprintf(stderr, "[MASCOTA] no pude copiar %s (error %lu): ruta de siempre\n", src, GetLastError());
            return 0;
        }
        fprintf(stderr, "[MASCOTA] copia propia del modelo %d: %s (de %s)\n", n, dst, src);
    }
lista_copia:
    snprintf(path, cap, "monster/%03d/m%03dm.igb", n / 100 * 100, n);
    return 1;
}

/* the duel is over (a new duel scene, or another scene): its mascots'
 * models let go too (each duel kept two loaded until the heap ran out) */
static void soltar_todo(void)
{
    int i;
    for (i = 0; i < NMASC; i++) {
        if (s_m[i].estado == M_LISTA || s_m[i].estado == M_CARGA)
            soltar(s_m[i].h);
        if (s_m[i].viejo)
            soltar(s_m[i].viejo);
    }
    memset(s_m, 0, sizeof s_m);
}

static void cargar(int i, int n)
{
    uint32_t a[5], buf;
    char path[64];
    int k;
    /* the model shown stays until the new one takes its place (lista); one
     * still loading is dropped now */
    if (s_m[i].estado == M_LISTA)
        s_m[i].viejo = s_m[i].h;
    else if (s_m[i].estado == M_CARGA)
        soltar(s_m[i].h);
    s_m[i].dibujada = 0;                /* not shown until its picture has the new model */
    s_m[i].n = n;
    if (n == EXODIA_ENTERO)
        snprintf(path, sizeof path, s_m[i].ataque ? "effect/special_igb/se002_ataque.igb"
                                                  : "effect/special_igb/se002_mascota.igb");   /* tools/modelos/exodia_mascota.py */
    else if (!copia_mascota(n, path, sizeof path))
        snprintf(path, sizeof path, "monster/%03d/m%03d.igb", n / 100 * 100, n);
    g_esp -= 0x40u;
    buf = g_esp;
    for (k = 0; path[k]; k++)
        MEM8(buf + (uint32_t)k) = (uint8_t)path[k];
    MEM8(buf + (uint32_t)k) = 0;
    gcall(sub_00018FE0, 0, 0, NULL);
    if (n != EXODIA_ENTERO) {
        a[0] = (uint32_t)n; a[1] = 1;
        gcall(sub_0001DB20, 0, 2, a);
    }
    /* the special one as the Exodia win loads it (sub_000625C0) */
    a[0] = buf; a[1] = 1; a[2] = 0; a[3] = n == EXODIA_ENTERO; a[4] = 0;
    s_m[i].h = gcall(sub_0001ADF0, 0, 5, a);
    g_esp += 0x40u;
    s_m[i].estado = s_m[i].h == 0xFFFFFFFFu ? M_ERROR : M_CARGA;
    s_m[i].desde = GetTickCount64();
    fprintf(stderr, "[MASCOTA] %d: %s -> manija %08X\n", i, path, s_m[i].h);
}

static void encuadres_leer(void);
void ygo_as_duelo_nuevo(void);

static void crear(uint32_t esc)
{
    uint32_t heap, a[5];
    int i;
    soltar_todo();
    memset((void *)g_nv2a_mascota, 0, sizeof g_nv2a_mascota);
    InterlockedExchange(&s_exodia_gana, 0);
    s_fin_t = 0;
    s_fin_gana = -1;
    fin_limpiar();
    memset(s_m, 0, sizeof s_m);         /* (soltar_todo did; every field from 0) */
    s_crear_t = GetTickCount64();
    s_viaje_t0 = 0;
    ygo_as_duelo_nuevo();               /* carta_as.c: "Primera vez por duelo" starts over */
    memset((void *)g_nv2a_destello, 0, sizeof g_nv2a_destello);
    g_nv2a_zoom = 1.f;
    encuadres_leer();
    s_escena = esc;
    if (!s_on || !rd_ok(esc, 0x80) || !MEM32(esc + 0x7C))
        return;
    {
        /* 2026-11-20 seguro (el usuario: "las semanas de trabajo son en vano
         * si el juego se tranca"): las mascotas usan ~2,7 MB de la memoria
         * contigua (64 MB, la de la Xbox) por duelo. Con mas de 40 MB ya en
         * uso al empezar el duelo, este duelo va sin mascotas: si quedara
         * alguna perdida, el juego no llega a llenarla por ellas. */
        extern void xbox_ContiguousUsageKB(uint32_t *in_use, uint32_t *peak);
        uint32_t uso = 0, pico = 0;
        xbox_ContiguousUsageKB(&uso, &pico);
        if (uso > 40u * 1024u) {
            fprintf(stderr, "[MASCOTA] memoria de texturas alta (%u KB de 65408): este duelo sin mascotas\n", uso);
            return;
        }
    }
    heap = gcall(sub_00192370, esc, 0, NULL);
    for (i = 0; i < NMASC; i++) {
        uint32_t d0, v;
        int n = modelo_de(i);
        if (n < 0)
            continue;                   /* no second mascot for this opponent */
        a[0] = 0xD0; a[1] = heap;
        d0 = gcall(sub_00185210, 0, 2, a);
        if (!d0) break;
        gcall(sub_00057040, d0, 0, NULL);
        a[0] = 0x138; a[1] = heap;
        v = gcall(sub_00185210, 0, 2, a);
        if (!v) break;
        a[0] = MEM32(esc + 0x7C); a[1] = d0; a[2] = MEM32(esc); a[3] = (uint32_t)LADO(i);   /* the game's side: the second is the opponent's too */
        gcall(sub_0005B7E0, v, 4, a);
        a[0] = 0;
        gcall(sub_00058760, v, 1, a);
        s_m[i].v = v;
        s_m[i].d0 = d0;
        cargar(i, n);
        fprintf(stderr, "[MASCOTA] %d: visor %08X (memoria %08X)\n", i, v, heap);
    }
    fflush(stderr);
}

/* the battle's camera A for the model (2026-11-14 20:23: with B, the
 * attack's close-up, Kuriboh's camera stood ~6 from it and only a bit of its
 * fur showed, "pasto morado que se mueve"; A, where the attack starts, is
 * ~25 away), s_m[i].lejos times as far from its target */
/* 2026-11-15 (the user: all of them "del mismo lado", "como sale la maga
 * oscura"; "las 3": the same height, the same place, facing the same way):
 * the battle's cameras A look from 19..59 degrees aside and -16..+22 up
 * (default.xbe table), so each model came out turned its own way. Every
 * mascot now takes the Dark Magician Girl's angle (az -35.4, el -2.1: from
 * the model's front-left, she looks toward the field), keeping its own
 * target and distance; the opponent's mirrored (az +35.4: toward the field
 * from the other side). */
#define CAM_AZ -35.4
#define CAM_EL -2.1

static void camara(int i)
{
    uint32_t a[9];
    double d[19], dx, dy, dz, dist, az, el;
    int k;
    memcpy(d, (const void *)XBOX_PTR(0x0042A718u + 0x98u * (uint32_t)s_m[i].n), sizeof d);
    dx = d[4] - d[1];
    dy = d[5] - d[2];
    dz = d[6] - d[3];
    dist = sqrt(dx * dx + dy * dy + dz * dz) * s_m[i].lejos;
    /* the same angle on both sides: the opponent's picture comes out mirrored
     * by the viewer itself (2026-11-15 00:40: at +35.4 the opponent's Jinzo
     * looked away from the field, "mirando hacia la nada"; at its camera A,
     * -46, it had looked toward it) */
    az = CAM_AZ * 3.14159265358979 / 180.0;
    /* Harpie Lady Sisters (16:2x, the user: "son 3 pero se ven 2 medio juntas"):
     * they stand side by side, so from 35 degrees aside one hides behind
     * another; almost from the front they show the three */
    if (s_m[i].n == HARPIAS)
        az = -8.0 * 3.14159265358979 / 180.0;
    el = CAM_EL * 3.14159265358979 / 180.0;
    a[0] = bf((float)(d[1] + dist * sin(az) * cos(el)));
    a[1] = bf((float)(d[2] + dist * sin(el)));
    a[2] = bf((float)(d[3] + dist * cos(az) * cos(el)));
    for (k = 0; k < 3; k++) {
        a[3 + k] = bf((float)d[1 + k]);
        a[6 + k] = bf((float)d[7 + k]);
    }
    gcall(sub_000583A0, s_m[i].v, 9, a);
}

/* the model fills bb (u0, v0, u1, v1) of its picture: scale and place it */
/* By eye (2026-11-15 captures, the user): "kuriboh hacerlo un mas chico (ya
 * que es chico)"; "el pescador ... subelo un poco". Scale on top of the fit,
 * rows up. */
/* 2026-11-15 01:55 captures (the user): "agrandar los 3" (Blue-Eyes Ultimate
 * Dragon, B. Skull Dragon, Sengenjin: their wings / spear are ~2.5 x as wide
 * as tall, so the fit by width left the body small: 216 at 0.356, 702 at
 * 0.460) and Kuriboh "mas chico (parece que quedo como antes)" (0.7 x a fit of
 * 1.446 still filled the 120 x 124). The big ones go 1.45 x and may run off
 * the window's left edge (izq: their wing or spear end cut there, the body
 * left of the life points). */
/* 2026-11-15 (the user, one by one with captures; Jinzo "perfectamente
 * centrado" is the reference): esc x the fit; arriba rows higher; dx columns
 * to the left (the opponent's mirrored); ancho the widest it may be (0: the
 * free space centred on the plate, 84 / 80); pies: 0 centred by its feet
 * (the measured column of its lowest rows; 16:0x, the user: "siguen
 * desentrados los otros tomá sus pies como referencia": every one), -1 by
 * its whole extent (the flying dragons). Wider than the centred space it moves away from
 * the life points, as far as the window's edge (TOPE_IZQ_DEF / DER_RIVAL_MAX). */
#define TOPE_IZQ_DEF 8.f
#define DRAGON 124.f                    /* "mas grande": the whole free space, 8..132 */
#define MEDIANO 110.f                   /* bigger than the centred 84, still near the middle */
/* der: how far right it may reach (0: TOPE_DER); past the diamond only for
 * a wing high above the life points (the opponent's mirrored) */
static const struct { int n; float esc, arriba, dx, ancho; int pies; float der; } k_retoque[] = {
    { 57, 0.5f, 0.f, 0.f, 0.f, 0 },     /* Kuriboh: smaller */
    { 921, 1.f, 12.f, 0.f, 0.f, 0 },    /* Legendary Fisherman: higher */
    { 15, 1.f, 0.f, 8.f, 0.f, -1 },      /* Time Wizard: "un poco mas hacia la izquierda" */
    { 902, 1.f, 0.f, 0.f, 114.f, 0 },   /* Masked Beast: "mas grande ... a la izquierda (como antes)" */
    /* 420 Cyber Commander's (0.8) went with it, 2026-11-21 */
    /* 15:5x captures, "ahi que agrandar estos ... centra cualquiera que puedas":
     * MEDIANO wide, as near the plate's middle as the diamond lets (22..132) */
    { 21, 1.3f, 0.f, 0.f, MEDIANO, 0 }, /* Summoned Skull: "sus pies ... donde jinzo", bigger */
    { 737, 1.3f, 0.f, 0.f, MEDIANO, 0 },/* Exodia (whole) */
    { 739, 1.3f, 0.f, 0.f, MEDIANO, -1 },/* Barrel Dragon (its cannons, no feet) */
    /* Thousand-Eyes Restrict, 16:1x "centralo bien": centred on its extent
     * (35..145), its right claws high (rows ~170) left of the life points */
    { 903, 1.3f, 0.f, 0.f, MEDIANO, -1, 146.f },
    { 452, 0.75f, 0.f, 6.f, 0.f, 0 },  /* Millennium Golem: 16:1x "achica a golem", "centrarlo un poco mas" */
    { 492, 1.f, 0.f, 10.f, 0.f, -1 },  /* 16:1x "move a maha mas a la izquierda": 38..122 */     /* Maha Vailo: "dejalo como estaba de tamaño pero centrado": by its robe's bottom, not the cape */
    { 40, 1.f, 0.f, 0.f, 0.f, 0 },      /* Celtic Guardian: "los pies centrado ... no la capa" */
    { 379, 1.45f, 0.f, 0.f, DRAGON, -1 },/* Blue-Eyes Ultimate Dragon: "mas grande" */
    /* B. Skull Dragon; 16:0x "esta chico", 16:1x "se va demasiado para la
     * izquierda, centrado lo maximo posible sin que choque": centred on the
     * plate (6..176), its right wing at rows ~144, above the life points
     * (186..220); its claws stay left of the diamond (column ~104) */
    { 216, 1.45f, 0.f, 0.f, 170.f, -1, 176.f },
    { 0, 1.45f, 10.f, 0.f, DRAGON, -1 },
    { 36, 1.45f, 0.f, 0.f, DRAGON, -1 },/* Gaia the Dragon Champion: 16:1x "mas grande" */
    { 854, 1.f, 0.f, 0.f, 0.f, -1 },    /* Jinzo: "ya estaba centrado", by its extent */ /* Blue-Eyes; 15:5x "solo subirlo un poco" */
    { 81, 1.45f, 0.f, 0.f, DRAGON, -1 }, /* Red-Eyes */
    { 712, 1.45f, 0.f, 0.f, 100.f, -1 },/* Meteor B. Dragon; 16:0x "muy grande" (was 124) */
    { 359, 1.3f, 0.f, 0.f, MEDIANO, 0 },/* Zera the Mant: "no lo agrandaste" */
    /* 16:2x captures of the new ones */
    { 918, 1.f, 10.f, 0.f, 0.f, 0 },     /* Mystic Tomato: "levantalo un poco para que cuando pique no toque nada" */
    { 62, 1.3f, 0.f, 0.f, MEDIANO, 0 }, /* Harpie Lady Sisters, from the front (camara()): 16:3x "un poco mas grandes" (were 84 wide, 48..132) */
    { 926, 1.f, 6.f, 0.f, 0.f, 0 },      /* Fire Princess: "levantarla muy poquito para que no toque" */
    { 385, 1.45f, 0.f, 0.f, DRAGON, -1 },/* Harpie's Pet Dragon: "mas grandes ya que son dragones" */
    { 38, 1.45f, 0.f, 0.f, DRAGON, -1 }, /* Curse of Dragon: same */
    { 371, 1.f, 0.f, 0.f, 0.f, -1 },     /* Kazejin: "mas hacia la derecha pero sin tocar los pv" (was 24..108; by its extent 48..132) */
    /* 16:2x captures of the opponents' new ones */
    { 905, 1.45f, 0.f, 0.f, DRAGON, -1 },/* Humanoid Worm Drake: "mas grande" (wings and tail: by its extent) */
    { 945, 1.f, 0.f, 10.f, 0.f, 0 },     /* Vorse Raider: "mas centrado, un poco mas hacia la izquierda" */
    { 845, 1.f, 0.f, 10.f, 0.f, 0 },     /* Slate Warrior: the same */
    /* 2026-11-19 captures of the new ones */
    { 601, 0.75f, 0.f, 0.f, 0.f, 0 },    /* Penguin Soldier: "mas chico" */
    { 590, 0.75f, 12.f, 0.f, 0.f, 0 },   /* Morphing Jar: "mas chico", "mas arriba" */
    { 365, 1.f, 16.f, 0.f, 0.f, -1 },    /* Labyrinth Wall: "subi la muralla", "mas arriba" */
    { 357, 1.45f, 16.f, 0.f, DRAGON, -1 },/* Seiyaryu: "mas grande y levantalo como que vuela", "se ve chico aun" */
    { 390, 1.3f, 0.f, 0.f, MEDIANO, 0 }, /* Zoa: "mas grande" */
    { 907, 1.f, 0.f, 10.f, 0.f, 0 },     /* The Fiend Megacyber: "centralo un poco mas (hacia la izquierda)" */
    /* Cyber Jar: "mas hacia la izquierda centrado (se mueve hacia la derecha
     * esa es su animacion)": measured at rest u 168..247 (84 wide, 44..128),
     * its roll takes it right to the diamond; by its extent, 18 columns left */
    { 911, 1.f, 0.f, 18.f, 0.f, -1 },
    { 500, 1.45f, 0.f, 0.f, DRAGON, 0 }, /* Man-Eater Bug: "hacerlo mas grande" (was cut to 84 wide; now 124) */
};
/* The player's must end left of column 132: the life points plate starts at
 * 147 and its blue diamond at ~135 (rows 207..233); the opponent's right of
 * 508 (the purple diamond ends at 503). */
#define TOPE_DER 132.f
/* the opponent's second mascot (mock-up B): this much of the fit, its left
 * edge this far right of IZQ_RIVAL, its feet on this row (the first's
 * bottom is TOPE_RIVAL + ALTO = 286) */
/* 2026-11-21 01:0x (the user's capture: "pone a la bestia bien hacia la
 * derecha (que entre completa) y en lo que queda acomoda a la cabeza", not
 * touching each other nor the life points): measured on it (1920 x 1080 =
 * 3 x 2.25 per game px) the first went 518..617, rows 162..276, the second
 * 510..570, rows 233..298 (on the first and on your board's right edge,
 * ~515). Now the first ends at column 638 (the window is 640) and the second
 * goes under its left: from column 518, feet on row 316 (your hand's cards
 * start at ~322), ~44 rows tall (0.55 x 44 / 65) */
/* 01:0x the log's measures: the first is 114 x 116 (now 524..638, rows
 * 162..278, its staff at the left down to ~278); the second 62 x 68 at
 * 0.55, so 0.34 makes it ~42 x 42: columns 518..560, rows 278..320 */
/* 01:2x (the user's capture: "achica a la bestia, tiene que verse mas o menos
 * en el mismo lado, no pongas el otro abajo"): side by side, feet on one
 * row. Log measures: the Beast's picture 143 x 145, Melchid's 85 x 93 (fit
 * 1.333). Beast 73 wide: columns 565..638, rows ~171..245; Melchid 0.40 of
 * its fit (~45 x 50): columns 518..563 (right of your board's edge, ~515),
 * rows ~195..245 */
/* 01:2x ("tiene que verse como se ven ellos, no 1 en una punta y otro en la
 * otra"): together as Lumis & Umbra in their portrait: the Beast behind on
 * the right (80 wide: columns 558..638, rows ~159..240), Melchid in front of
 * its left a bit lower, overlapping it ~10 columns (0.44 of its fit, ~50 x 55:
 * columns 518..568, rows ~195..250) */
/* 01:3x (capture: Melchid at 518..573 over the Beast's legs at 567..627;
 * "aun se chocan entre ellos, tiene que parecer que estan juntos"): no
 * overlap, one group: both on one feet row, each given its width (their
 * measured extents, the whole idle animation): Melchid 48 (518..566), a
 * 4 column gap, the Beast 66 (570..636) */
#define SEGUNDA_ESC 0.44f                /* (only until measured) */
/* 01:4x: the user picked mock-up 2 ("Melchid atras, asomando", sizes and
 * height as shown, not touching): modding/muestras/lumis_pareja_final.png.
 * Melchid 44 wide, columns 504..548 (the purple diamond ends at 503), feet
 * on row 226 (your board's edge starts at 229); the Beast 88 wide, columns
 * 550..638, feet on row 248 (its top ~159, the plate ends at 149) */
#define SEGUNDA_DX 8.f                  /* 01:5x "muy alejados, un poco mas a la derecha la cabeza": the user picked +12 of the mock-up (516..560), muestras/lumis_melchid_derecha.png */
#define SEGUNDA_ANCHO 44.f
#define SEGUNDA_PIE 226.f
#define PRIMERA_PIE 248.f
#define PRIMERA_DER 638.f
#define PRIMERA_ANCHO 88.f

static int por_pies(int n)
{
    int r;
    for (r = 0; r < (int)(sizeof k_retoque / sizeof k_retoque[0]); r++)
        if (k_retoque[r].n == n)
            return k_retoque[r].pies >= 0;
    return 1;
}

static void ubicar(int i, const int bb[5])
{
    static const int be[5] = { 48, 98, 288, 346, -1 };
    float w = (float)(bb[2] - bb[0]), h = (float)(bb[3] - bb[1]), e, arriba = 0.f, dx = 0.f, ancho = 0.f;
    float centro, libre, izq, uc, der = TOPE_DER, izq_r = IZQ_RIVAL;
    int r, pies = 1;
    if (w < 8.f || h < 8.f) {
        /* nothing seen: as the Blue-Eyes */
        bb = be;
        w = 240.f;
        h = 248.f;
    }
    e = ANCHO / w < ALTO / h ? ANCHO / w : ALTO / h;
    for (r = 0; r < (int)(sizeof k_retoque / sizeof k_retoque[0]); r++)
        if (k_retoque[r].n == s_m[i].n) {
            e *= k_retoque[r].esc;
            arriba = k_retoque[r].arriba;
            dx = k_retoque[r].dx;
            ancho = k_retoque[r].ancho;
            pies = k_retoque[r].pies >= 0;
            if (k_retoque[r].der > 0.f) {
                der = k_retoque[r].der;
                izq_r = IZQ_RIVAL - (der - TOPE_DER);
            }
        }
    centro = !LADO(i) ? CENTRO_JUGADOR : CENTRO_RIVAL;
    libre = 2.f * (!LADO(i) ? TOPE_DER - CENTRO_JUGADOR : CENTRO_RIVAL - IZQ_RIVAL);
    if (ancho <= 0.f)
        ancho = libre;
    if (w * e > ancho) {
        fprintf(stderr, "[MASCOTA] %d: modelo %d ancho %.0f, achicado a %.0f\n", i, s_m[i].n, w * e, ancho);
        e = ancho / w;
    }
    if (PAREJA(i))
        e = (SEGUNDA(i) ? SEGUNDA_ANCHO : PRIMERA_ANCHO) / w;   /* the pair: each its width */
    s_m[i].esc = e;
    /* the column it is centred on: its extent's middle, or its feet's */
    uc = (float)(bb[0] + bb[2]) / 2.f;
    if (pies && bb[4] >= 0)
        uc = (float)bb[4];
    izq = centro - (!LADO(i) ? dx : -dx) - (uc - (float)bb[0]) * e;
    /* clear of the life points and the diamond, inside the window */
    if (!LADO(i)) {
        if (izq + w * e > der)
            izq = der - w * e;
        if (izq < TOPE_IZQ_DEF)
            izq = TOPE_IZQ_DEF < der - w * e ? TOPE_IZQ_DEF : der - w * e;
    } else {
        if (izq < izq_r)
            izq = izq_r;
        if (izq + w * e > DER_RIVAL_MAX)
            izq = DER_RIVAL_MAX - w * e > izq_r ? DER_RIVAL_MAX - w * e : izq_r;
    }
    if (PAREJA(i)) {
        /* the opponent's: the second from IZQ_RIVAL + SEGUNDA_DX, the first
         * ending at PRIMERA_DER; yours the same mirrored (the window is 640) */
        float a = SEGUNDA(i) ? IZQ_RIVAL + SEGUNDA_DX : PRIMERA_DER - w * e;
        izq = LADO(i) ? a : 640.f - a - w * e;
    }
    s_m[i].cx = izq - ((float)bb[0] - 240.f) * e;
    s_m[i].vx = izq + w * e / 2.f;
    if (PAREJA(i)) {
        /* the pair: each on its own feet row; yours the first on your plate,
         * the second as much higher as the opponent's */
        float pie = LADO(i) ? (SEGUNDA(i) ? SEGUNDA_PIE : PRIMERA_PIE)
                            : PIE_JUGADOR - (SEGUNDA(i) ? PRIMERA_PIE - SEGUNDA_PIE : 0.f);
        s_m[i].cy = pie - ((float)bb[3] - 240.f) * e;
        s_m[i].vy = pie - h * e / 2.f;
        arriba = 0.f;
    } else if (!LADO(i)) {
        s_m[i].cy = PIE_JUGADOR - ((float)bb[3] - 240.f) * e;
        s_m[i].vy = PIE_JUGADOR - h * e / 2.f - arriba;
    } else {
        /* 2026-11-16 (the user's capture, Fire Princess against Tea: "que no
         * toque el nombre las mascotas del rival, bajarlas un poco"): the
         * touches' "arriba" are the player's (lifted off its plate below);
         * the opponent's hangs under its plate (rows 118..149), so it is not
         * lifted into it, and its top is 7 rows lower than before (155) */
        arriba = 0.f;
        s_m[i].cy = TOPE_RIVAL - ((float)bb[1] - 240.f) * e;
        s_m[i].vy = TOPE_RIVAL + h * e / 2.f;
    }
    s_m[i].vh = h * e;
    s_m[i].vw = w * e;
    s_m[i].cy -= arriba;
    cuadro(i);
    s_m[i].encuadrada = 1;
    g_nv2a_mascota[i].ocultar = 0;
    fprintf(stderr, "[MASCOTA] %d: modelo %d en u %d..%d, v %d..%d, pies %d de su imagen (camara x%.2f):"
            " escala %.3f, de la columna %.1f a %.1f\n", i, s_m[i].n, bb[0], bb[2], bb[1], bb[3], bb[4],
            s_m[i].lejos, e, izq, izq + w * e);
}

/* The measures kept between runs (2026-11-15, the user: "la primera vez que
 * pones una mascota tarda en aparecer, luego aparece seguido"): each new one
 * goes to YGO_AJUSTES\mascotas_encuadre3.txt ("n lejos u0 v0 u1 v1", n =
 * model * 2 + side), read at the first duel. */
static void encuadres_archivo(char *out, size_t cap)
{
    const char *dir = getenv("YGO_AJUSTES");
    /* "3": measured at the one camera angle on both sides (the first file's
     * were at each model's own angle, the second's opponents mirrored) */
    snprintf(out, cap, "%s%smascotas_encuadre4.txt", dir && *dir ? dir : "", dir && *dir ? "\\" : "");
}

static void encuadres_leer(void)
{
    static int leido;
    char path[MAX_PATH], line[128];
    FILE *f;
    if (leido)
        return;
    leido = 1;
    encuadres_archivo(path, sizeof path);
    if (!(f = fopen(path, "r")))
        return;
    while (fgets(line, sizeof line, f) && s_nencuadre < NENCUADRE) {
        int n, b[5] = { 0, 0, 0, 0, -1 }, e;
        double l;
        if (sscanf(line, "%d %lf %d %d %d %d %d", &n, &l, &b[0], &b[1], &b[2], &b[3], &b[4]) < 6 || l < 0.5)
            continue;
        for (e = 0; e < s_nencuadre; e++)
            if (s_encuadre[e].n == n)
                break;
        s_encuadre[e].n = n;
        s_encuadre[e].lejos = l;
        memcpy(s_encuadre[e].bb, b, sizeof b);
        if (e == s_nencuadre)
            s_nencuadre++;
    }
    fclose(f);
    fprintf(stderr, "[MASCOTA] %d medidas (con las guardadas en %s)\n", s_nencuadre, path);
}

static void encuadres_guardar(int n, double lejos, const int bb[5])
{
    char path[MAX_PATH];
    FILE *f;
    encuadres_archivo(path, sizeof path);
    if ((f = fopen(path, "a"))) {
        fprintf(f, "%d %.3f %d %d %d %d %d\n", n, lejos, bb[0], bb[1], bb[2], bb[3], bb[4]);
        fclose(f);
    }
}

/* a measure came back */
static void medida(int i)
{
    struct nv2a_mascota_kit *m = &g_nv2a_mascota[i];
    int bb[5], k, borde;
    for (k = 0; k < 4; k++)
        bb[k] = m->bb[k];
    bb[4] = m->pies;
    /* cut by the picture's edge: farther and again. A small one is only
     * scaled up (2026-11-15: moving nearer took two more measures, ~8 s for
     * the Kuriboh; the picture is 1920 host px for 480, a 100 px model
     * shown 124 rows tall is still finer than the screen) */
    borde = bb[2] >= bb[0] && (bb[0] <= 2 || bb[1] <= 2 || bb[2] >= 478 || bb[3] >= 478);
    fprintf(stderr, "[MASCOTA] %d: medida u %d..%d, v %d..%d (camara x%.2f)%s\n", i, bb[0], bb[2],
            bb[1], bb[3], s_m[i].lejos, borde ? ", toca el borde" : "");
    if (s_m[i].intentos < 4 && borde) {
        s_m[i].intentos++;
        s_m[i].lejos *= 1.35;
        camara(i);
        m->pedido++;
        return;
    }
    ubicar(i, bb);
    if (bb[2] >= bb[0] && s_nencuadre < NENCUADRE) {
        s_encuadre[s_nencuadre].n = CLAVE(s_m[i].n, LADO(i));
        s_encuadre[s_nencuadre].lejos = s_m[i].lejos;
        memcpy(s_encuadre[s_nencuadre].bb, bb, sizeof bb);
        encuadres_guardar(CLAVE(s_m[i].n, LADO(i)), s_m[i].lejos, bb);
        s_nencuadre++;
    }
}

static void lista(int i)
{
    uint32_t v = s_m[i].v, root, node, sp, a[9];
    int n = s_m[i].n;
    a[0] = s_m[i].h;
    root = gcall(sub_00019AF0, 0, 1, a);
    sp = g_esp;
    g_esp -= 0x10u;
    MEM32(g_esp) = 0;
    a[0] = root;
    gcall(sub_00013610, g_esp, 1, a);
    node = MEM32(g_esp);
    g_esp = sp;
    if (!node) {
        fprintf(stderr, "[MASCOTA] %d: el modelo no dio nodo (raiz %08X)\n", i, root);
        s_m[i].estado = M_ERROR;
        return;
    }
    MEM32(node + 8) += 1;               /* the copy the call takes (and lets go) */
    a[0] = node; a[1] = (uint32_t)n;
    gcall(sub_0005A670, v, 2, a);
    /* and ours (sub_00013610's) let go too, as the battle does with its
     * temporary (0x76413): sub_0005A670 takes a reference of its own (inc at
     * 0x5A6F0) and drops its argument's (0x5A72B). Kept, it held every
     * mascot model in memory for good: 2026-11-15, the heap full after
     * changing the mascot ~20 times ("out of memory ... used 49319936/
     * 50528256"), even with the handles let go. The viewer still holds one:
     * this never reaches 0 here. */
    if ((MEM32(node + 8) & 0x7FFFFFu) > 1u)
        MEM32(node + 8) -= 1;
    if (s_m[i].viejo && s_m[i].viejo != s_m[i].h) {
        soltar(s_m[i].viejo);
        s_m[i].viejo = 0;
    }
    a[0] = 1;
    gcall(sub_00057020, v, 1, a);
    {
        int e;
        s_m[i].lejos = s_m[i].n == EXODIA_ENTERO ? LEJOS_EXODIA : LEJOS0;
        for (e = 0; e < s_nencuadre; e++)
            if (s_encuadre[e].n == CLAVE(s_m[i].n, LADO(i)))   /* per side: each side's camera frames it apart */
                break;
        if (e < s_nencuadre && s_encuadre[e].bb[4] < 0 && por_pies(s_m[i].n))
            e = s_nencuadre;            /* placed by its feet: measured once more for them */
        if (e < s_nencuadre) {
            /* measured in an earlier duel */
            s_m[i].lejos = s_encuadre[e].lejos;
            camara(i);
            ubicar(i, s_encuadre[e].bb);
        } else {
            /* hidden until measured; meanwhile where the Blue-Eyes went */
            camara(i);
            s_m[i].esc = SEGUNDA(i) ? 0.25f : 0.5f;   /* the second's box smaller: the kit tells them apart by it */
            s_m[i].cx = LADO(i) ? 531.f : 126.f;
            s_m[i].cy = LADO(i) ? 219.f : 183.f;
            g_nv2a_mascota[i].ocultar = 1;
            cuadro(i);
            g_nv2a_mascota[i].pedido++;
        }
    }
    s_m[i].estado = M_LISTA;
    g_nv2a_alpha_uno_480 = 1;
    fprintf(stderr, "[MASCOTA] %d: lista en %llu ms (nodo %08X)\n", i,
            (unsigned long long)(GetTickCount64() - s_m[i].desde), node);
    fflush(stderr);
}

static int en_llegada(void);
static int en_partida(void);

/* ---- the entrance (2026-11-15, the user's video: the mascots popped in
 * ~1.3 s after the field, with nothing; "como Master Duel ... pero egipcio"):
 * the first time a mascot shows in a duel (or a new one is picked) it rises
 * into its place, growing from 15 % to its size in ENTRADA_MS (ease out with
 * a small overshoot), the player's from below, the opponent's from above.
 * Its strips are rebuilt each frame meanwhile (cuadro, quiet), then once
 * more at rest. The duel's states are logged with their time (s_crear_t) to
 * see where the doors open on the field. */
#define ENTRADA_MS 700u
#define DESTELLO_MS 650.f
static void entrada(int i, ULONGLONG now)
{
    float f, e, cx = s_m[i].cx, cy = s_m[i].cy, esc = s_m[i].esc, g;
    if (!s_m[i].entrada || s_m[i].entrada_hecha)
        return;
    f = (float)(now - s_m[i].entrada) / (float)ENTRADA_MS;
    /* the flash (2026-11-15, the user: "el destello de las mascotas"): a golden
     * glow on the model's middle, up in 80 ms and gone by DESTELLO_MS, and a
     * ring growing out from it */
    {
        float t = (float)(now - s_m[i].entrada), gl, rg;
        gl = t < 80.f ? t / 80.f : 1.f - (t - 80.f) / (DESTELLO_MS - 80.f);
        if (gl < 0.f)
            gl = 0.f;
        rg = 1.f - t / DESTELLO_MS;
        if (rg < 0.f)
            rg = 0.f;
        if (i < 2) {
        g_nv2a_destello[i][0] = s_m[i].vx;
        g_nv2a_destello[i][1] = s_m[i].vy;
        g_nv2a_destello[i][2] = 0.55f * s_m[i].vh + 10.f;
        g_nv2a_destello[i][3] = gl * gl * 0.9f;
        g_nv2a_destello[i][4] = (0.15f + 0.9f * (t / DESTELLO_MS)) * s_m[i].vh;
        g_nv2a_destello[i][5] = rg * 0.8f;
        }
    }
    if (f >= 1.f) {
        s_m[i].entrada_hecha = 1;
        if (i < 2)
            g_nv2a_destello[i][3] = g_nv2a_destello[i][5] = 0.f;
        s_callado = 1;
        cuadro(i);                      /* at rest, exactly */
        s_callado = 0;
        fprintf(stderr, "[MASCOTA] %d: entrada hecha (%llu ms desde la escena)\n", i, now - s_crear_t);
        return;
    }
    /* ease out back: past 1 a little, then settles */
    g = f - 1.f;
    e = 1.f + 2.2f * g * g * g + 1.2f * g * g;
    s_m[i].esc = esc * (0.15f + 0.85f * e);
    s_m[i].cy = cy + (LADO(i) ? -40.f : 40.f) * (1.f - e);
    s_callado = 1;
    cuadro(i);
    s_callado = 0;
    s_m[i].esc = esc;
    s_m[i].cx = cx;
    s_m[i].cy = cy;
}

#define RENDER_MS 16u                    /* one mascot picture each, by turns: ~30 a second each */
/* its place in the turns among the viewers there are */
static int orden(int i)
{
    int j, n = 0;
    for (j = 0; j < i; j++)
        n += s_m[j].v != 0;
    return n;
}
/* another mascot ready to take a turn */
static int otra_lista(int i)
{
    int j;
    for (j = 0; j < NMASC; j++)
        if (j != i && s_m[j].v && s_m[j].estado == M_LISTA)
            return 1;
    return 0;
}
static int en_partida(void);
static void reaccion(ULONGLONG now);   /* your mascot, touched */
static void fin_duelo(ULONGLONG now);  /* both, when the duel is decided */
void ygo_estadisticas_fin(int exodia);   /* estadisticas.c */
void ygo_estadisticas_mirar(void);
unsigned xbox_VblankHz(void);       /* the kit: the frames a second asked for */
static void tick(uint32_t r)
{
    /* each mascot is drawn every other frame, by turns (2026-11-14: two
     * more 480x480 renders a frame slowed the game; its texture keeps the
     * last picture, the idle animation still runs at half the frame rate) */
    /* 2026-11-15 (the user: "se siente mas lento ... 60, 90, 120 fps"): by
     * the clock, not by frames: one mascot's picture every RENDER_MS, by
     * turns (each ~30 a second, as at 60 fps before), so 90 / 120 fps no
     * longer render them 1.5 / 2 x as often. Their cost is logged
     * ([MASCOTA] costo) every 10 s of the duel. */
    static unsigned turno;
    static uint32_t estado_visto = 0xFFFFFFFFu;
    static LARGE_INTEGER qf, t_ult, t_log;
    static double ms_render, ms_cuadro_max;
    static unsigned n_render, n_cuadros, n_8, n_17;   /* frames over 8.5 / 17 ms (2026-11-21: how even it runs) */
    static LONGLONG proximo;            /* QPC (GetTickCount64 steps ~15.6 ms) */
    static double t_ant_d;              /* the last frame, ms */
    static unsigned proximo_espera;     /* frames a due picture waited for room */
    LARGE_INTEGER t0;
    int i, toca = 0, nvis = (s_m[0].v != 0) + (s_m[1].v != 0) + (s_m[2].v != 0) + (s_m[3].v != 0);
    /* 2026-11-21: with more than two, one picture every RENDER_MS * 2 / n:
     * each still ~30 a second */
    if (nvis < 2)
        nvis = 2;
    if (!qf.QuadPart)
        QueryPerformanceFrequency(&qf);
    QueryPerformanceCounter(&t0);
    if (t_ult.QuadPart) {
        double d = (double)(t0.QuadPart - t_ult.QuadPart) * 1000.0 / (double)qf.QuadPart;
        if (d > ms_cuadro_max)
            ms_cuadro_max = d;
        n_8 += d > 8.5;
        n_17 += d > 17.0;
        n_cuadros++;
        t_ant_d = d;
    }
    t_ult = t0;
    if (!t_log.QuadPart)
        t_log = t0;
    if ((double)(t0.QuadPart - t_log.QuadPart) / (double)qf.QuadPart >= 10.0) {
        double seg = (double)(t0.QuadPart - t_log.QuadPart) / (double)qf.QuadPart;
        if (en_partida())
            fprintf(stderr, "[MASCOTA] costo: %.0f cuadros/s (el mas largo %.1f ms; %u de mas de 8.5 ms, %u de mas de 17),"
                    " %u dibujos de mascota (%u cuadros esperaron lugar), %.2f ms cada uno (%.1f%% del tiempo)\n",
                    n_cuadros / seg, ms_cuadro_max, n_8, n_17, n_render, proximo_espera,
                    n_render ? ms_render / n_render : 0., ms_render / (seg * 10.));
        t_log = t0;
        ms_render = ms_cuadro_max = 0.;
        n_render = n_cuadros = n_8 = n_17 = proximo_espera = 0;
    }
    /* 2026-11-21 (the 04:02 log: the duel's frame is ~6 ms of a 6.06 ms
     * refresh, and the frames over 8.5 ms are the ones that add something,
     * as a mascot's 1.55 ms picture): when the last frame came late (over
     * 1.15 refreshes), a picture due now waits one frame, at most RENDER_MS
     * past its time */
    /* 04:1x (the 04:13 log, every frame over 7.5 ms: 900 of 1751 had a
     * mascot picture, 2-3 ms of ours; the duel's own frame is 4-5 ms of the
     * 6.06): a picture only where the frame has room for it. Its time into
     * the frame is the time since the last vblank (the kit's
     * g_xbox_vblank_qpc); with the picture (~1.8 ms) it must end within 3/4
     * of the refresh, and the last frame must not have come late. A picture
     * that waited 2 x RENDER_MS past its time is drawn anyway (the animation
     * never stops). */
    /* 04:2x: TAKEN OUT (the user: "las mascotas van ultra lentas"). The
     * 04:22 log: 226-267 pictures each 10 s instead of ~550, with 843-967
     * frames waiting for room: the tick runs late in the frame, so there
     * was almost never room, and a picture waited its 2 x RENDER_MS. The
     * pictures go by their clock again, as before 04:03. */
    (void)t_ant_d;
    if (t0.QuadPart >= proximo) {
        toca = 1;
        proximo = t0.QuadPart + qf.QuadPart * RENDER_MS * 2 / ((LONGLONG)nvis * 1000) - qf.QuadPart / 1000;   /* 1 ms slack: not a frame late at 60 fps */
        turno++;
    }
    if (MEM32(0x00657148u) == 6u)
        ygo_estadisticas_mirar();        /* the result, while the duel still holds it */
    if (MEM32(0x00657148u) != estado_visto) {
        uint32_t antes = estado_visto;
        estado_visto = MEM32(0x00657148u);
        /* 2026-11-16: the duel ended (the win check's result in 0x65F8DA):
         * the Perfil's statistics (estadisticas.c) */
        /* (2026-11-16 log: a won duel went 6 -> 9 between two of our
         * frames, so any way out of 6) */
        if (antes == 6u && estado_visto > 6u)
            ygo_estadisticas_fin(s_exodia_gana != 0);
        fprintf(stderr, "[MASCOTA] estado del duelo %u (%llu ms desde la escena)\n", estado_visto,
                GetTickCount64() - s_crear_t);
    }
    if (s_m[0].v && s_on && (s_m[0].estado == M_LISTA || s_m[0].estado == M_ERROR)
            && modelo_de(0) != s_m[0].n) {
        /* "Tu mascota" changed in F2 / Configuración during the duel (the
         * user: "tuve que reiniciar para cambiar de mascota"): the new model
         * loads into the same viewer (sub_0005A670 takes it as the battle's
         * viewers take each battle's monster), hidden until it is measured */
        int n = modelo_de(0);
        fprintf(stderr, "[MASCOTA] 0: la mascota cambia a %d\n", n);
        g_nv2a_mascota[0].ocultar = 1;
        s_m[0].ataque = 0;
        s_m[0].entrada = 0;
        s_m[0].entrada_hecha = 0;
        s_m[0].encuadrada = 0;
        s_m[0].intentos = 0;
        s_m[0].alejada = 0;
        cargar(0, n);
    }
    for (i = 0; i < NMASC; i++) {
        if (!s_m[i].v)
            continue;
        if (s_exodia_gana && s_m[i].n == EXODIA_ENTERO && !s_m[i].ataque
                && s_m[i].estado == M_LISTA && s_m[i].encuadrada) {
            fprintf(stderr, "[MASCOTA] %d: Exodia golpea\n", i);
            s_m[i].ataque = 1;
            cargar(i, EXODIA_ENTERO);
        }
        if (s_m[i].estado == M_CARGA) {
            uint32_t a[1];
            gcall(sub_00018FE0, 0, 0, NULL);
            a[0] = s_m[i].h;
            if (gcall(sub_000199C0, 0, 1, a) & 0xFFu)
                lista(i);
            else if (GetTickCount64() - s_m[i].desde > 20000u) {
                fprintf(stderr, "[MASCOTA] %d: el modelo no cargo en 20 s\n", i);
                s_m[i].estado = M_ERROR;
            }
        }
        if (s_m[i].estado == M_LISTA && !s_m[i].encuadrada
                && g_nv2a_mascota[i].medido == g_nv2a_mascota[i].pedido)
            medida(i);
        if (s_m[i].estado == M_LISTA && s_on && en_partida() && toca
                && (turno % (unsigned)nvis == (unsigned)orden(i) || !otra_lista(i))) {
            uint32_t a[1];
            LARGE_INTEGER r0, r1;
            QueryPerformanceCounter(&r0);
            /* its light (2026-11-14 capture: only the Blue-Eyes' white and
             * blue highlights showed, the rest dark): the battle turns the
             * viewer's light [v+0x38] every frame through its vtable +4
             * (sub_0005A400 / sub_0005A460 -> sub_00058420: angle +0x3C,
             * direction sin / cos, height 10); the mascot's was never set. */
            /* 2026-11-15 (the user: "cuando el turno del rival esta en x2 las
             * mascotas se mueven x2 tambien"): its animation reads the guest's
             * TSC (Alchemy's timer, sub_00117xxx), which the fast turn speeds
             * up; while it draws, the kit hands it a clock at speed 1 */
            xbox_TscX1(1);
            gcall(sub_00058420, s_m[i].v, 0, NULL);
            a[0] = r;
            gcall(sub_00056FC0_pad, s_m[i].v, 1, a);
            xbox_TscX1(0);
            s_m[i].dibujada = 1;
            QueryPerformanceCounter(&r1);
            ms_render += (double)(r1.QuadPart - r0.QuadPart) * 1000.0 / (double)qf.QuadPart;
            n_render++;
            toca = 0;                   /* one a turn */
        }
        /* its entrance: once it is shown in the duel, placed */
        if (s_m[i].estado == M_LISTA && s_m[i].dibujada && s_m[i].encuadrada && en_llegada()
                && !s_m[i].entrada && !g_nv2a_mascota[i].ocultar) {
            s_m[i].entrada = GetTickCount64();
            if (s_entrada_modo == 2)
                s_m[i].entrada_hecha = 1;   /* Original: no rise */
            fprintf(stderr, "[MASCOTA] %d: entrada (%llu ms desde la escena)\n", i, s_m[i].entrada - s_crear_t);
        }
        entrada(i, GetTickCount64());
    }
    reaccion(GetTickCount64());         /* your mascot, touched */
    fin_duelo(GetTickCount64());        /* both, when the duel is decided */
}

/* ---- touching your mascot (2026-11-16, the user: "Tocar tu mascota", then
 * "podemos hacer 2 o 3 mas? y que se vayan turnando?"; the mock-up of four
 * approved): a click on your mascot in the duel makes it react, each click
 * the next of four, over its own idle animation (the game's models carry no
 * other: an attack is the battle's camera and effects):
 *   0 Salto: squashes, jumps a fifth of its height, a gold glow and ring;
 *   1 Giro: turns round once in place, blue sparks rising;
 *   2 Rugido: crouches, swells and shakes, a red glow and a ring out;
 *   3 Destello: floats up, lights up and throws out gold sparks.
 * As the entrance, its strips are rebuilt each frame (cuadro) with the
 * size across / up (s_kx / s_ky) about its feet and middle, and the kit's
 * flash (g_nv2a_destello, with a tint and sparks: g_nv2a_destello_extra).
 * Each plays one of the game's own sounds (sub_0011A400). */
#define REAC_MS 700.f
extern volatile float g_nv2a_destello_extra[2][2];   /* tint 0 gold 1 blue 2 red 3 gold, sparks 0..1 */
static ULONGLONG s_reac_t;
static int s_reac_n = -1, s_reac_sig;
static const uint32_t k_reac_sonido[4] = { 0x1000008Du, 0x10000089u, 0x10000017u, 0x10000093u };
void sub_0011A400(void);

static void reaccion(ULONGLONG now)
{
    float t, p, kx = 1.f, ky = 1.f, dx = 0.f, dy = 0.f, gl = 0.f, gr = 0.f, rr = 0.f, ra = 0.f, tinte = 0.f, ch = 0.f;
    float cx = s_m[0].cx, cy = s_m[0].cy, vh = s_m[0].vh, pie = s_m[0].vy + s_m[0].vh * 0.5f, pi = 3.14159265f;
    if (s_reac_n < 0)
        return;
    t = (float)(now - s_reac_t);
    if (t >= REAC_MS || s_m[0].estado != M_LISTA || g_nv2a_mascota[0].ocultar) {
        s_reac_n = -1;
        s_kx = s_ky = 1.f;
        g_nv2a_destello[0][3] = g_nv2a_destello[0][5] = 0.f;
        g_nv2a_destello_extra[0][0] = g_nv2a_destello_extra[0][1] = 0.f;
        s_callado = 1;
        cuadro(0);                      /* at rest, exactly */
        s_callado = 0;
        return;
    }
    p = t / REAC_MS;
    switch (s_reac_n) {
    case 0: {                           /* Salto */
        float q = t / 500.f;
        if (q < 1.f)
            dy = -0.2f * vh * sinf(pi * q);
        if (t < 40.f) { kx = 1.12f; ky = 0.88f; }
        else if (t < 125.f) { kx = 0.94f; ky = 1.08f; }
        gl = q < 1.f ? (1.f - q) * 0.8f : 0.f;
        gr = 0.55f * vh + 10.f;
        rr = (0.15f + 0.9f * (q < 1.f ? q : 1.f)) * vh;
        ra = q < 0.8f ? (1.f - q / 0.8f) * 0.8f : 0.f;
        break;
    }
    case 1: {                           /* Giro */
        float a = 2.f * pi * (p / 0.85f < 1.f ? p / 0.85f : 1.f);
        kx = cosf(a);
        if (kx > -0.06f && kx < 0.06f)
            kx = kx < 0.f ? -0.06f : 0.06f;
        dy = -0.06f * vh * sinf(pi * p);
        tinte = 1.f;
        gl = 0.35f * sinf(pi * p);
        gr = 0.45f * vh + 8.f;
        ch = p;
        break;
    }
    case 2: {                           /* Rugido */
        tinte = 2.f;
        gr = 0.6f * vh + 10.f;
        if (p < 0.15f) {
            kx = 1.06f;
            ky = 0.9f;
        } else {
            float q = (p - 0.15f) / 0.85f, k = 1.f + 0.14f * sinf(pi * (q * 1.6f < 1.f ? q * 1.6f : 1.f));
            kx = ky = k;
            dx = 0.04f * vh * sinf(q * 40.f) * (1.f - q);
            gl = 0.9f * sinf(pi * q);
            rr = (0.2f + 1.0f * q) * vh;
            ra = (1.f - q) * 0.9f;
        }
        break;
    }
    default: {                          /* Destello */
        float fl = 1.f - fabsf(p - 0.3f) / 0.3f;
        dy = -0.13f * vh * sinf(pi * p);
        kx = ky = 1.f + 0.05f * sinf(pi * p);
        tinte = 3.f;
        gl = fl > 0.f ? 0.9f * fl : 0.f;
        gr = 0.6f * vh + 10.f;
        ch = p;
        break;
    }
    }
    g_nv2a_destello[0][0] = s_m[0].vx;
    g_nv2a_destello[0][1] = s_m[0].vy + dy;
    g_nv2a_destello[0][2] = gr;
    g_nv2a_destello[0][3] = gl;
    g_nv2a_destello[0][4] = rr;
    g_nv2a_destello[0][5] = ra;
    g_nv2a_destello_extra[0][0] = tinte;
    g_nv2a_destello_extra[0][1] = ch;
    /* across about its middle, up about its feet */
    s_kx = kx;
    s_ky = ky;
    s_m[0].cx = s_m[0].vx - (s_m[0].vx - cx) * kx + dx;
    s_m[0].cy = cy + (pie - cy) * (1.f - ky) + dy;
    s_callado = 1;
    cuadro(0);
    s_callado = 0;
    s_m[0].cx = cx;
    s_m[0].cy = cy;
    s_kx = s_ky = 1.f;
}

/* ---- the duel's end (2026-11-16, the user: "podemos hacer que reaccione a
 * perder o ganar?"; after Blender mock-ups of the real model, the calm pair
 * approved: "hace uno de ganar pero como el perder a ... que no cambie mucho
 * el tono", then "agregale un aura negativa", softer and centred). As soon
 * as the game's win check has decided (0x65F894 bit 0x10, 0x65F8DA bit 0x40
 * player 0 won / 0x80 player 1; estadisticas.c), over 1 s (ease out):
 *   the winner's mascot stands a little taller (x1.03 across, x1.08 up,
 *   about its feet), 15 % brighter in its own colours, a soft gold halo
 *   behind it with motes rising;
 *   the loser's slumps (x1.04, x0.90), dims (-35 %) and goes grey (85 %),
 *   a soft violet aura behind it with smoke rising.
 * The kit tones the picture (g_nv2a_mascota_tono) and draws the halo /
 * aura behind its silhouette (g_nv2a_aura). Only while the duel scene is in
 * 6 (the mascots are not drawn after). A draw: nothing. */
extern volatile float g_nv2a_mascota_tono[2][2];
extern volatile float g_nv2a_aura[2][12];
static void fin_limpiar(void)
{
    int i;
    for (i = 0; i < 2; i++) {
        g_nv2a_mascota_tono[i][0] = g_nv2a_mascota_tono[i][1] = 1.f;
        g_nv2a_aura[i][4] = g_nv2a_aura[i][5] = 0.f;
    }
}

static void fin_duelo(ULONGLONG now)
{
    uint32_t st = MEM32(0x00657148u);
    float t, q, e;
    int i;
    if (st == 6u && !s_fin_t) {
        unsigned fl = MEM16(0x0065F8DAu) & 0xC0u;
        if ((MEM8(0x0065F894u) & 0x10u) && (fl == 0x40u || fl == 0x80u)) {
            s_fin_t = now;
            s_fin_gana = fl == 0x40u ? 0 : 1;
            s_fin_hecho[0] = s_fin_hecho[1] = 0;
            s_reac_n = -1;              /* a touch reaction gives way */
            s_kx = s_ky = 1.f;
            g_nv2a_destello[0][3] = g_nv2a_destello[0][5] = 0.f;
            g_nv2a_destello_extra[0][0] = g_nv2a_destello_extra[0][1] = 0.f;
            fprintf(stderr, "[MASCOTA] fin del duelo: gana la mascota %d (%llu ms desde la escena)\n",
                    s_fin_gana, now - s_crear_t);
        }
    }
    if (!s_fin_t)
        return;
    if (st != 6u) {                     /* the result screens: the mascots are not drawn */
        fin_limpiar();
        return;
    }
    t = (float)(now - s_fin_t) / 1000.f;
    q = t < 1.f ? t : 1.f;
    e = 1.f - (1.f - q) * (1.f - q) * (1.f - q);
    for (i = 0; i < NMASC; i++) {
        int gana = LADO(i) == s_fin_gana;
        float kx = gana ? 1.f + 0.03f * e : 1.f + 0.04f * e, ky = gana ? 1.f + 0.08f * e : 1.f - 0.10f * e;
        float cx = s_m[i].cx, cy = s_m[i].cy, pie = s_m[i].vy + s_m[i].vh * 0.5f, ncx, ncy;
        if (s_m[i].estado != M_LISTA || g_nv2a_mascota[i].ocultar || s_m[i].vh < 4.f) {
            if (i < 2) {
                g_nv2a_mascota_tono[i][0] = g_nv2a_mascota_tono[i][1] = 1.f;
                g_nv2a_aura[i][4] = 0.f;
            }
            continue;
        }
        ncx = s_m[i].vx - (s_m[i].vx - cx) * kx;
        ncy = cy + (pie - cy) * (1.f - ky);
        if (i < 2) {                    /* the second: its pose only */
        g_nv2a_mascota_tono[i][0] = gana ? 1.f + 0.15f * e : 1.f - 0.35f * e;
        g_nv2a_mascota_tono[i][1] = gana ? 1.f : 1.f - 0.85f * e;
        g_nv2a_aura[i][0] = s_m[i].vx;
        g_nv2a_aura[i][1] = pie - s_m[i].vh * ky * 0.5f;
        g_nv2a_aura[i][2] = s_m[i].vw * kx * 0.5f + 12.f;
        g_nv2a_aura[i][3] = s_m[i].vh * ky * 0.5f + 12.f;
        g_nv2a_aura[i][4] = gana ? 1.f : 2.f;
        g_nv2a_aura[i][5] = e;
        g_nv2a_aura[i][6] = t;
        g_nv2a_aura[i][7] = 0.f;
        g_nv2a_aura[i][8] = ncx;
        g_nv2a_aura[i][9] = ncy;
        g_nv2a_aura[i][10] = 480.f * s_m[i].esc * kx;
        g_nv2a_aura[i][11] = 480.f * s_m[i].esc * ky;
        }
        if (s_fin_hecho[i] && q >= 1.f)
            continue;                   /* settled: its strips stay */
        s_fin_hecho[i] = q >= 1.f;
        s_kx = kx;
        s_ky = ky;
        s_m[i].cx = ncx;
        s_m[i].cy = ncy;
        s_callado = 1;
        cuadro(i);
        s_callado = 0;
        s_m[i].cx = cx;
        s_m[i].cy = cy;
        s_kx = s_ky = 1.f;
    }
}

/* raton.c, the game's thread, a left click in the game (frame px): 1 if it
 * was on your mascot (taken: the game does not get it) */
int ygo_mascota_clic(float gx, float gy)
{
    ULONGLONG now = GetTickCount64();
    float hw, hh;
    if (!s_escena || MEM32(0x00657148u) != 6u || s_m[0].estado != M_LISTA || !s_m[0].entrada_hecha
            || g_nv2a_mascota[0].ocultar || !g_nv2a_mascota[0].activa || s_m[0].vh < 4.f)
        return 0;
    hw = s_m[0].vw * 0.5f > 25.f ? s_m[0].vw * 0.5f : 25.f;
    hh = s_m[0].vh * 0.5f;
    if (fabsf(gx - s_m[0].vx) > hw || fabsf(gy - s_m[0].vy) > hh)
        return 0;
    if (s_fin_t)
        return 0;                       /* the duel is decided: the mascots show that */
    if (s_reac_n >= 0)
        return 1;                       /* still reacting: the click is just taken */
    s_reac_n = s_reac_sig;
    s_reac_sig = (s_reac_sig + 1) % 4;
    s_reac_t = now;
    {
        uint32_t sp = g_esp, sa = g_eax, sc = g_ecx, sd = g_edx;
        PUSH32(g_esp, k_reac_sonido[s_reac_n]);
        PUSH32(g_esp, 0u);
        sub_0011A400();
        g_esp = sp;
        g_eax = sa; g_ecx = sc; g_edx = sd;
    }
    fprintf(stderr, "[MASCOTA] tocada en %.0f,%.0f: reaccion %d (sonido %08X)\n", gx, gy, s_reac_n,
            k_reac_sonido[s_reac_n]);
    return 1;
}

/* ---- the temple's camera (2026-11-15, toward a Master Duel-like entrance:
 * the camera travelling through the temple to the field) -------------------
 * sub_0005C3F0 (thiscall, the temple and its light; the duel frame calls it,
 * 0x4605C / 0x46152) copies the 4x4 floats at [this+0xC] to its camera
 * object ([this+8]+0x118, sub_001E5EE0) before drawing. Logged at each duel
 * state and a few times in the opening, to know what it holds. */
void sub_0005C3F0_pad(void);
void ygo_danio_tick(void);
void ygo_cadena_tick(void);
void ygo_fundas_tick(void);
void ygo_danio_limpiar(void);
void ygo_danio_batalla(void);
void sub_0005C3F0(void)
{
    uint32_t e = MEM32(0x00657148u);
    ULONGLONG now = GetTickCount64();
    /* the entrance: the whole picture comes in from VIAJE_ZOOM x to its size
     * (the temple's lens, scaled 05:23, changed nothing on screen: what you
     * see behind the board is not drawn with it) */
    /* 2026-11-16 (the user: "no se ve nada saca el zoom y deja el efecto de
     * las mascotas"): the whole-picture zoom of the entrance is gone (it
     * scaled the doors and the bars too: it could not look like a camera
     * travel); the mascots' rise and glow stay. "Completa" and "Solo las
     * mascotas" now do the same */
    (void)e; (void)now;
    g_nv2a_zoom = 1.f;
    sub_0005C3F0_pad();
}

/* the duel scene: the mascots are built with it */
void sub_000444B0(void)
{
    uint32_t esc = g_ecx;
    sub_000444B0_pad();
    {
        uint32_t ra = g_eax, rc = g_ecx, rd = g_edx;
        crear(esc);
        g_eax = ra; g_ecx = rc; g_edx = rd;
    }
}

/* On screen (2026-11-14 20:04, video: still nothing). The 0xD0 object built
 * for each viewer (sub_00057040) is a screen renderer: sub_0004C060 with an
 * ortho projection sub_001E6540(0, 640, 480, 0, 1, 100) (vtable 0x429940).
 * The viewer hangs its layer (the 8 strips with the texture) on it
 * (sub_0005B4A0: [viewer+0x40] = it, sub_0004AB50(it; layer)). Its vf0
 * (sub_0004AA90) draws at once through r.
 * Where (2026-11-14 22:20, the user: the card panel over them, and under the
 * battle's monsters; the [T480C] trace of one frame): the duel frame
 * (sub_00045FB0 / sub_00046064) draws scene2 1, scene layer 0, scene2 2 and
 * 3, sub_0005C3F0(0) (the temple and its light), then the scene's 2D layers
 * 1..5 (sub_00044360(r, k) -> sub_0001B280: plates, life points, the card
 * panel), the field (sub_00043830), the pictures, the battle's two screen
 * renderers (sub_00043800 k 1, k 0) and layers 7..9. Drawn after k 0 they
 * covered the plates, the panel and the battle; now they go right before
 * layer 1: over the temple, under everything else. Their pictures are drawn
 * later in the frame (tick): each shows the last frame's, as the every
 * other frame turns already did.
 * (2026-11-21: en_duelo(), unused, removed from under this note.) */

/* the opening and the duel: the pictures are rendered and measured while
 * the plates slide in (the strips hidden by the kit until then) */
/* 2026-11-15 05:10 run (the state log): 3 at 0.77 s from the scene (the doors
 * open on the field), 4 at 1.39 s (the purple "Comenzar duelo"), 5 at 2.06 s
 * (the plates, life points and the opponent's portrait come in), 6 at 3.03 s
 * (the duel). The mascots were placed by 0.77 s; they come in with the
 * plates (5), not 1 s after them ("que aparezca justo cuando ya esta el
 * duelo"). */
static int en_llegada(void)
{
    uint32_t e = MEM32(0x00657148u);
    if (s_entrada_modo == 2)
        return e == 6u;                 /* Original: in the duel only */
    return e == 5u || e == 6u;
}

static int en_partida(void)
{
    uint32_t e = MEM32(0x00657148u);
    return e >= 3u && e <= 6u;
}

static void dibujar(uint32_t r)
{
    uint32_t ra = g_eax, rc = g_ecx, rd = g_edx;
    int i;
    for (i = 0; i < NMASC; i++) {           /* the second last: in front of the first */
        uint32_t d0 = s_m[i].d0, fn;
        recomp_func_t f;
        if (s_m[i].estado != M_LISTA || !s_m[i].dibujada || !rd_ok(d0, 4) || !rd_ok(MEM32(d0), 4))
            continue;
        if (s_m[i].encuadrada ? !en_llegada() : !en_partida())
            continue;
        if (s_m[i].encuadrada && !s_m[i].entrada)
            continue;                   /* not before its entrance starts (tick, later this frame) */
        fn = MEM32(MEM32(d0));
        f = recomp_lookup(fn);
        if (f) {
            uint32_t a[1];
            a[0] = r;
            gcall(f, d0, 1, a);
        }
    }
    g_eax = ra; g_ecx = rc; g_edx = rd;
}

void sub_00044360(void)
{
    uint32_t r = MEM32(g_esp + 4), k = MEM32(g_esp + 8), esc = MEM32(0x00656F88u);
    if (k == 1 && s_on && s_escena && esc == s_escena && en_partida())
        dibujar(r);
    sub_00044360_pad();
}

void sub_00043800(void)
{
    sub_00043800_pad();
}

volatile ULONGLONG g_ygo_batalla_t;     /* last frame a battle viewer was on screen */

/* the battle viewers drawn each frame; the mascots after the second */
void sub_00056FC0(void)
{
    uint32_t v = g_ecx, r = MEM32(g_esp + 4), esc = MEM32(0x00656F88u);
    /* a battle viewer showing (its picture [v+0x2C], on [v+0xCC], as
     * sub_00056FC0 checks): the damage number flies from the fight (danio.c);
     * the mascots' own viewers never come through here (gcall of the _pad) */
    if (rd_ok(v, 0xD0) && MEM32(v + 0x2Cu) && MEM8(v + 0xCCu) && v != s_m[0].v && v != s_m[1].v && v != s_m[2].v && v != s_m[3].v) {
        ygo_danio_batalla();
        g_ygo_batalla_t = GetTickCount64();     /* auto.c: AUTO hidden meanwhile */
    }
    sub_00056FC0_pad();
    if (s_escena && esc == s_escena && rd_ok(esc, 0x80) && v == MEM32(esc + 0x78)) {
        uint32_t ra = g_eax, rc = g_ecx, rd = g_edx;
        LARGE_INTEGER qf, q0, q1, q2, q3, q4;
        QueryPerformanceFrequency(&qf);
        QueryPerformanceCounter(&q0);
        {
            /* 2026-11-21: the screen for the kit's [CUADROS] lines */
            extern const char *volatile g_nv2a_escena;
            extern volatile unsigned long long g_nv2a_escena_t;
            ULONGLONG ya = GetTickCount64();
            g_nv2a_escena = ya - g_ygo_batalla_t < 300u ? "batalla 3D" : "duelo";
            g_nv2a_escena_t = ya;
        }
        ygo_danio_tick();               /* danio.c: the damage number */
        QueryPerformanceCounter(&q1);
        ygo_cadena_tick();              /* cadena.c: activation glow, chain numbers */
        QueryPerformanceCounter(&q2);
        ygo_fundas_tick();              /* fundas.c: the opponent's sleeve on its side */
        QueryPerformanceCounter(&q3);
        tick(r);
        QueryPerformanceCounter(&q4);
        {
            /* 2026-11-21: ours in this frame, for the kit's [LENTO] lines
             * (the damage number, the chain, the sleeves, the mascots and
             * their pictures) */
            extern volatile float g_nv2a_extra_ms;
            g_nv2a_extra_ms += (float)((double)(q4.QuadPart - q0.QuadPart) * 1000.0 / (double)qf.QuadPart);
        }
        /* 2026-11-16 (the first battle's stall): ours, if slow */
        if ((double)(q4.QuadPart - q0.QuadPart) * 1000.0 / (double)qf.QuadPart > 15.0)
            fprintf(stderr, "[MASCOTA] lento: danio %.0f, cadena %.0f, fundas %.0f, mascotas %.0f ms\n",
                    (double)(q1.QuadPart - q0.QuadPart) * 1000.0 / (double)qf.QuadPart,
                    (double)(q2.QuadPart - q1.QuadPart) * 1000.0 / (double)qf.QuadPart,
                    (double)(q3.QuadPart - q2.QuadPart) * 1000.0 / (double)qf.QuadPart,
                    (double)(q4.QuadPart - q3.QuadPart) * 1000.0 / (double)qf.QuadPart);
        g_eax = ra; g_ecx = rc; g_edx = rd;
    } else if (esc != s_escena) {
        /* another scene: the old mascots went with it */
        if (s_escena) {
            soltar_todo();
            memset((void *)g_nv2a_mascota, 0, sizeof g_nv2a_mascota);
        }
        s_escena = 0;
        g_nv2a_alpha_uno_480 = 0;
        memset((void *)g_nv2a_destello, 0, sizeof g_nv2a_destello);
        fin_limpiar();
        ygo_danio_limpiar();
        g_nv2a_zoom = 1.f;
    }
}

/* ---- diagnostic of the battle's own viewers (first battles) ------------- */
#define DUMP_MAX 4

static void dump(const char *what, uint32_t a, int n)
{
    int i;
    if (!rd_ok(a, 4u * (uint32_t)n)) {
        fprintf(stderr, "[MASCOTA]   %s %08X: (no se puede leer)\n", what, a);
        return;
    }
    fprintf(stderr, "[MASCOTA]   %s %08X:", what, a);
    for (i = 0; i < n; i++) {
        uint32_t v = MEM32(a + 4u * i);
        if (v && ((v >> 23) & 0xFF) > 0x60 && ((v >> 23) & 0xFF) < 0x98)
            fprintf(stderr, " +%02X=%.2f", 4 * i, fb(v));
        else
            fprintf(stderr, " +%02X=%08X", 4 * i, v);
    }
    fprintf(stderr, "\n");
}

void sub_00057D60(void)
{
    static int n;
    uint32_t v = g_ecx, ret = MEM32(g_esp);
    sub_00057D60_pad();
    if (s_modo || es_mascota(v) || n >= DUMP_MAX || !rd_ok(v, 0x138))
        return;
    n++;
    fprintf(stderr, "[MASCOTA] 57D60 visor %08X (vtable %08X, desde %08X): lado %u, listo %u,"
            " avance %d/%d, activo %u, modelo %u\n",
            v, MEM32(v), ret, MEM8(v + 0x44), MEM8(v + 0x45), (int)MEM32(v + 0xC0),
            (int)MEM32(v + 0xC4), MEM8(v + 0xCC), MEM32(v + 0xD8));
    dump("visor", v, 0x4E);
    dump("capa [visor+8]", MEM32(v + 8), 11);
    fflush(stderr);
}

void sub_00057020(void)
{
    static int n;
    uint32_t v = g_ecx, on = MEM32(g_esp + 4) & 0xFF, ret = MEM32(g_esp);
    if (n < 40) {
        n++;
        fprintf(stderr, "[MASCOTA] visor %08X %s (desde %08X), modelo %u, lado %u%s\n",
                v, on ? "prendido" : "apagado", ret, MEM32(v + 0xD8), MEM8(v + 0x44),
                es_mascota(v) ? " (mascota)" : "");
        fflush(stderr);
    }
    sub_00057020_pad();
}

void sub_000583A0(void)
{
    static int n;
    uint32_t v = g_ecx, sp = g_esp, ret = MEM32(g_esp);
    if (n < 24) {
        n++;
        fprintf(stderr, "[MASCOTA] camara visor %08X (desde %08X, modelo %u): %.2f %.2f %.2f /"
                " %.2f %.2f %.2f / %.2f %.2f %.2f%s\n", v, ret, MEM32(v + 0xD8),
                fb(MEM32(sp + 4)), fb(MEM32(sp + 8)), fb(MEM32(sp + 12)),
                fb(MEM32(sp + 16)), fb(MEM32(sp + 20)), fb(MEM32(sp + 24)),
                fb(MEM32(sp + 28)), fb(MEM32(sp + 32)), fb(MEM32(sp + 36)),
                es_mascota(v) ? " (mascota)" : "");
        fflush(stderr);
    }
    sub_000583A0_pad();
}

/* ---- 2026-11-18: the textures of every duel's mascots stayed for good ----
 * The user: after ~30 min "el disco esta sucio" / the game hung. Log 20:02:
 * the contiguous arena (textures) full ("arena exhausted ... 64300 of 65408
 * KB in use"), growing ~2.8 MB at each duel's mascots and never coming down
 * when they were let go. The viewer keeps the model's node ([v+0x34], in its
 * group [v+0x30]): letting the handle go (sub_00019700) is not enough, the
 * node's reference holds the model and its textures. sub_0005A670 drops the
 * old one this way when a battle swaps monsters (0x5A6AE..0x5A6E9): out of
 * the group (sub_002345A0(group; node)), one reference less, destroyed at 0
 * (sub_0019F5E0). Done here when the duel scene ends (sub_00045750, the
 * scene still alive), then the handles. */
void sub_002345A0(void);
void sub_0019F5E0(void);
void sub_00045750_pad(void);

/* 2026-11-19 (registro del usuario: al terminar un duelo, crash en la
 * destruccion de la escena, sub_00045750 -> sub_00044E60 -> sub_0004F140 ->
 * sub_0004CD10 -> sub_00013E70, leyendo una referencia ya liberada): bajar la
 * referencia del nodo ANTES de que el juego desarme la escena dejaba objetos
 * de la escena apuntando a lo que el nodo liberaba. Ahora: antes, solo se
 * saca el nodo del grupo (como sub_0005A670) y se anota; la referencia del
 * visor se baja DESPUES de que el juego destruyo la escena. */
/* 2026-11-19 07:1x (registro 07:02, primer duelo de la sesion: el mismo
 * crash en el desarme, sub_0004F140 recorriendo un arreglo de la escena con
 * basura): lo unico de la mascota que todavia corria ANTES del desarme era
 * sacar el nodo del grupo del visor (sub_002345A0, escribe en memoria de la
 * escena). Antes del arreglo de memoria no se tocaba nada ahi y nunca hubo
 * crash. Ahora antes del desarme SOLO se lee (nodo, su vtable y cuantas
 * referencias tiene) y no se escribe nada en la escena; despues, si el nodo
 * sigue igual (misma vtable, mismas referencias), se le bajan las dos
 * referencias de la mascota (la del grupo y la del visor; el visor y su
 * grupo quedan abandonados con la escena) y se destruye en 0, como
 * sub_0005A670. Si cambio algo, no se toca (registro "no se toca"). */
static uint32_t s_nodos[NMASC], s_nodo_vt[NMASC], s_nodo_refs[NMASC];

static void anotar_nodos(void)
{
    int i;
    for (i = 0; i < NMASC; i++) {
        uint32_t v = s_m[i].v, node;
        s_nodos[i] = 0;
        if (!v || !rd_ok(v, 0x138))
            continue;
        node = MEM32(v + 0x34);
        if (!node || !rd_ok(node, 0x10) || !MEM32(v + 0x30))
            continue;
        s_nodos[i] = node;
        s_nodo_vt[i] = MEM32(node);
        s_nodo_refs[i] = MEM32(node + 8) & 0x7FFFFFu;
    }
}

static void soltar_nodos(void)
{
    int i, k;
    for (i = 0; i < NMASC; i++) {
        uint32_t node = s_nodos[i], c;
        s_nodos[i] = 0;
        if (!node)
            continue;
        if (!rd_ok(node, 0x10) || MEM32(node) != s_nodo_vt[i]
            || (MEM32(node + 8) & 0x7FFFFFu) != s_nodo_refs[i] || s_nodo_refs[i] < 2) {
            fprintf(stderr, "[MASCOTA] %d: nodo %08X cambio tras la escena (refs %u antes): no se toca\n",
                    i, node, s_nodo_refs[i]);
            continue;
        }
        for (k = 0; k < 2; k++) {
            c = MEM32(node + 8) - 1u;
            MEM32(node + 8) = c;
            if (!(c & 0x7FFFFFu)) {
                gcall(sub_0019F5E0, node, 0, NULL);
                break;
            }
        }
        fprintf(stderr, "[MASCOTA] %d: el visor suelta su modelo tras la escena (nodo %08X, %u -> %u referencias)\n",
                i, node, s_nodo_refs[i], s_nodo_refs[i] - (uint32_t)(k < 2 ? k + 1 : 2));
    }
}

/* 2026-11-20 (registro 16:25: colgado a los 25 min, memoria contigua llena
 * otra vez; "[CONTIG] vivo": 2 bloques de 900 KB (0xE1000, imagen 480x480)
 * y 2 de 452 KB (0x70800) mas por duelo, pedidos al crear los visores de
 * las mascotas): los visores propios nunca se destruian, solo se soltaba su
 * modelo. El juego destruye sus dos visores de batalla dentro del desarme
 * de la escena (sub_00044E60, 0x45025..0x4506E): por cada lado
 * sub_000570C0(d0) + sub_00192320(d0), despues sub_0005B9C0(visor) +
 * sub_00192320(visor). sub_0005B9C0 suelta [v+0xE0]/[v+0xE4] (sus imagenes)
 * y la base sub_0005B680 suelta sus 8 capas, [v+0x30] (grupo) y [v+0x34]
 * (nodo del modelo). Aqui, justo despues de que el juego destruye su visor
 * de +0x78 (el ultimo), se destruyen los de las mascotas igual: mismo
 * momento del desarme, mismo estado. Entonces soltar_nodos() no hace nada
 * (el destructor ya bajo esas referencias). Si el juego no tenia visor en
 * +0x78, queda el camino de antes (soltar_nodos tras la escena). */
/* 2026-11-20 22:3x: fe_conservar (fe007..fe012 kept with one more use in the
 * resource table, reused next duel) is gone. Registro 22:31: the game closed
 * at the end of duel 2 inside this destructor (sub_0005C170+0x108 ->
 * sub_0004B460 -> sub_0019F5E0 -> sub_00187090, objects with wild vtables
 * 0xF6900018.. = freed memory). Changing the game's use counts here was the only foreign code
 * on that path, and it never was the leak (the leak was mascot m716). The
 * wrapper stays as a plain pass-through (rename_pads keeps the _pad). */
void sub_0005C170_pad(void);

void sub_0005C170(void)
{
    sub_0005C170_pad();
}

void sub_0005B9C0_pad(void);
void sub_000570C0(void);
void sub_00192320(void);
static int s_visores_destruidos;

void sub_0005B9C0(void)
{
    uint32_t self = g_ecx, esc = s_escena;
    sub_0005B9C0_pad();
    if (esc && esc == MEM32(0x00656F88u) && self && self == MEM32(esc + 0x78)) {
        uint32_t sa = g_eax, sb = g_ebx, sc = g_ecx, sd = g_edx, si = g_esi, di = g_edi, bp = g_ebp;
        uint32_t a[1];
        uint32_t uso0 = 0, uso1 = 0, pico = 0;
        extern void xbox_ContiguousUsageKB(uint32_t *in_use, uint32_t *peak);
        int i;
        xbox_ContiguousUsageKB(&uso0, &pico);
        for (i = 0; i < NMASC; i++) {
            uint32_t v = s_m[i].v, d0 = s_m[i].d0;
            if (d0 && rd_ok(d0, 0xD0)) {
                gcall(sub_000570C0, d0, 0, NULL);
                a[0] = d0;
                gcall(sub_00192320, 0, 1, a);
            }
            if (v && rd_ok(v, 0x138)) {
                gcall(sub_0005B9C0_pad, v, 0, NULL);
                a[0] = v;
                gcall(sub_00192320, 0, 1, a);
            }
            if (v || d0)
                fprintf(stderr, "[MASCOTA] %d: visor %08X destruido (modelo %d)\n", i, v, s_m[i].n);
            s_m[i].v = s_m[i].d0 = 0;
            s_nodos[i] = 0;             /* el destructor ya bajo las referencias del modelo */
        }
        s_visores_destruidos = 1;
        xbox_ContiguousUsageKB(&uso1, &pico);
        fprintf(stderr, "[MASCOTA] visores de las mascotas destruidos con los del juego: texturas %u -> %u KB\n",
                uso0, uso1);
        g_eax = sa; g_ebx = sb; g_ecx = sc; g_edx = sd; g_esi = si; g_edi = di; g_ebp = bp;
    }
}

void sub_00045750(void)
{
    int mia = s_escena && s_escena == MEM32(0x00656F88u);
    s_visores_destruidos = 0;
    if (mia) {
        anotar_nodos();                 /* solo lee: nada se escribe en la escena */
        memset((void *)g_nv2a_mascota, 0, sizeof g_nv2a_mascota);   /* del kit, no del juego */
    }
    sub_00045750_pad();
    if (mia) {
        uint32_t sa = g_eax, sb = g_ebx, sc = g_ecx, sd = g_edx, si = g_esi, di = g_edi, bp = g_ebp;
        if (!s_visores_destruidos && (s_m[0].v || s_m[1].v || s_m[2].v || s_m[3].v))
            fprintf(stderr, "[MASCOTA] el juego no destruyo su visor de +0x78: los de las mascotas quedan (camino viejo)\n");
        {
            /* 2026-11-20: cuanto libera soltar los modelos de las mascotas
             * (los bloques chicos que suben 1,2 MB por duelo: de ellas o no) */
            extern void xbox_ContiguousUsageKB(uint32_t *in_use, uint32_t *peak);
            uint32_t u0 = 0, u1 = 0, pk = 0;
            xbox_ContiguousUsageKB(&u0, &pk);
            soltar_nodos();
            soltar_todo();
            xbox_ContiguousUsageKB(&u1, &pk);
            fprintf(stderr, "[MASCOTA] soltar los modelos: texturas %u -> %u KB\n", u0, u1);
        }
        {
            extern void nombres_resumen(void);
            nombres_resumen();          /* nombres.c: the shared-name table guard */
        }
        {
            /* 2026-11-19: the texture arena at each duel's end (it filled up
             * after ~30 min before the mascots let their models go) */
            extern void xbox_ContiguousUsageKB(uint32_t *in_use, uint32_t *peak);
            static int n;
            uint32_t uso = 0, pico = 0;
            xbox_ContiguousUsageKB(&uso, &pico);
            fprintf(stderr, "[MEMORIA] fin del duelo %d: texturas en uso %u KB (pico %u KB, el maximo es 65408)\n",
                    ++n, uso, pico);
            {
                /* 2026-11-20: la memoria comun (heap) del kit */
                extern void xbox_HeapUsageKB(uint32_t *live_kb, uint32_t *free_kb, uint32_t *top_kb, uint32_t *blocks);
                uint32_t lv = 0, fr = 0, tp = 0, nb = 0;
                xbox_HeapUsageKB(&lv, &fr, &tp, &nb);
                fprintf(stderr, "[MEMORIA] fin del duelo %d: heap vivo %u KB en %u bloques, devuelto %u KB, alcance %u KB\n",
                        n, lv, nb, fr, tp);
            }
        }
        g_eax = sa; g_ebx = sb; g_ecx = sc; g_edx = sd; g_esi = si; g_edi = di; g_ebp = bp;
        fflush(stderr);
    }
    {
        extern void ygo_guardado_tras_escena(void);
        ygo_guardado_tras_escena();     /* guardado.c: a finished duel saved now */
    }
    if (mia) {
        /* 2026-11-20 ultima proteccion (el usuario: "si el juego se llega a
         * colgar otra vez fue un fracaso"): la memoria contigua es la de la
         * Xbox (64 MB); dentro de un duelo sube hasta ~9 MB sobre lo que queda
         * al final. Si al terminar un duelo ya pasa 50 MB (por algo que no
         * sean las mascotas: esas se apagan desde 40), un aviso una vez por
         * sesion para guardar y reiniciar, en vez de colgarse en el proximo. */
        extern void xbox_ContiguousUsageKB(uint32_t *in_use, uint32_t *peak);
        static int avisado;
        uint32_t uso = 0, pico = 0;
        xbox_ContiguousUsageKB(&uso, &pico);
        if (!avisado && uso > 50u * 1024u) {
            wchar_t box[700];
            avisado = 1;
            fprintf(stderr, "[MEMORIA] aviso al usuario: %u KB en uso al terminar el duelo\n", uso);
            fflush(stderr);
            _snwprintf(box, 699,
                       L"La memoria de texturas del juego está casi llena (%u de 64 MB).\n\n"
                       L"Guardá la partida, cerrá el juego y volvé a abrirlo antes del próximo duelo "
                       L"para que no se trabe.\n\n"
                       L"Mandá registro.txt para que se revise.\n\n"
                       L"The game's texture memory is almost full. Save, close and reopen the game "
                       L"before the next duel.",
                       uso / 1024u);
            box[699] = 0;
            MessageBoxW(NULL, box, L"Yu-Gi-Oh! The Dawn of Destiny",
                        MB_OK | MB_ICONWARNING | MB_TOPMOST | MB_SETFOREGROUND);
        }
    }
}
