"""Rename generated function definitions to <name>_pad so the wrappers of the
same name in ygo-apex/src get every call, including calls from the same file
and the indirect ones through the dispatch table.

The file each definition sits in is found by searching gen/ (a regeneration
with new seeds moves functions between recomp_00xx.c files).

Run after regenerating ygo-apex/src/recomp/gen (idempotent):
    py -3 tools/rename_pads.py
"""
import pathlib
import re
import sys

GEN = pathlib.Path(__file__).resolve().parent.parent / "ygo-apex" / "src" / "recomp" / "gen"

# functions whose definition is wrapped in ygo-apex/src
PADS = [
    # src/fatal.c: the CRT's exit(code) and its variant (abort, runtime
    # errors) end in KeBugCheck, which our kernel returns from: logged and
    # closed with a notice instead of a frozen game.
    "sub_00356213", "sub_0035621F",
    # src/mando_directo.c: the XDK's XInputGetState reads the host state.
    "sub_0041C323",
    # src/diag_memoria.c: the engine allocator, its morecore and XAPI's
    # VirtualAlloc, logged (bring-up; YGO_MEM_TRACE=0 silences them).
    "sub_001AF320", "sub_0019B400", "sub_00121449",
    # src/videos.c: the .xmv player (skipped until the MP4 player is in).
    "sub_0002C690",
    # src/raton.c: the sprites' build and move-to (where the game's glove is).
    "sub_000F80D0", "sub_000F85F0",
    # src/raton.c: the duel field cursor's d-pad handler (the pointer sets
    # the cursor there directly).
    "sub_00092FA0",
    # src/raton.c: the name screen (keyboard typing)
    "sub_00110FD0",
    # src/raton.c: the title menu (Nueva partida / Cargar partida)
    "sub_001125C0",
    # src/raton.c: the duel's command buttons (the pointer picks one).
    "sub_00080D40",
    # src/raton.c: the command buttons' row opening (the rightmost lit from
    # the start for a hand card, 2026-11-17)
    "sub_00048010",
    # src/raton.c: the main menu wheel of modes (the pointer picks one).
    "sub_0010D6C0",
    # src/raton.c: rock-paper-scissors and other arrow choices (clicks on
    # the arrows press left/right).
    "sub_00109EB0", "sub_00087A80",
    # src/raton.c: other left/right menus, arrows by click when no glove
    # (opponent select among them).
    "sub_000F3DE0", "sub_00101FD0", "sub_00106CE0", "sub_0010B0C0",
    # src/raton.c: the menu sound (the cursor's tick muted on the pointer
    # walk's middle steps; who plays it, logged).
    "sub_0011A400",
    # src/raton.c: the sound engine's command entry (every play goes
    # through it: the walk's middle steps are muted there).
    "sub_001197F0",
    # src/raton.c: the Biblioteca's key handler (the pointer's card is
    # set as the cursor directly).
    "sub_0010F790",
    # src/raton.c: the Biblioteca scene's state machine (the wheel scrolls
    # there at once, as the scroll bar does).
    "sub_00038220",
    # src/raton.c: the field's target cursor (attack and effect targets:
    # the pointer's square, if the game offers it).
    "sub_00092E20",
    # src/raton.c: the duel's 2-line box (sub_00087A80, the 3-line one, is
    # above): the line under the pointer is marked, a click answers.
    "sub_000878C0",
    # src/raton.c: the deck builder's key handler (rows, tabs, wheel by
    # pointer; clicks as keys in its slot).
    "sub_000D9A10",
    # src/raton.c: the deck builder's other side (the deck, [0x66D1EA] 1..2).
    "sub_000DA730",
    # src/raton.c: the round buttons' light (logged in the deck builder).
    "sub_000ED100",
    # src/raton.c: the card lists over the field (graveyard...): the wheel
    # steps the list with its own keys.
    "sub_000C5710",
    # src/raton.c: the Target List (bring a monster, take one back): cards
    # and its two round buttons by pointer.
    "sub_000920B0",
    # src/raton.c: the main menu's Opciones and Guardar pages (rows,
    # arrows and slots by pointer).
    "sub_0010C2E0", "sub_0010E8E0",
    # src/voces.c: the text lookups (duel lines in Latin American Spanish,
    # and their voices).
    "sub_0002C5C0", "sub_0002C590",
    # src/voces.c: the text objects' SetText (the balloon given a voiced
    # line: a press with its text whole closes it and fades the voice).
    "sub_0002B9D0",
    # src/voces.c: the sprite animator (the portraits' mouth follows the voice).
    "sub_00018790",
    # src/cartas.c: the card mod's effects (Prision dimensional takes the
    # condition and the resolver of Magic Cylinder: the attack declared, and
    # its own resolution); Krebons: the attack declaration (battle state 6 -> 1).
    "sub_000A6810", "sub_000AF600", "sub_0009A290",
    # Sincronizador de Basura: the summon state machines (Normal Summon
    # sub_00093FA0 state 2, the other one sub_00094610 state 4 = the "when
    # summoned" triggers), the effect lookup and Senju's resolver.
    # src/carta_as.c: the summon dispatcher (the ace card: the duel waits)
    "sub_00095A30",
    # and the game's own slide of that card from the hand to its square (order
    # 0x90, sub_000CDE60 steps 0-1; sub_00046B30 starts the slide): skipped
    # once after the ace card animation, which already brought it down
    "sub_000CDE60", "sub_00046B30",
    "sub_00093FA0", "sub_00094610", "sub_000C4DE0", "sub_000B1220", "sub_000C1B80",
    # Ciclon del cosmos / Golpe de viento de ala de fenix: Mystical Space
    # Typhoon's condition, valid square, target choice and resolver, and the
    # discard cost (Magic Jammer).
    "sub_000A6C90", "sub_000AB150", "sub_000A35D0", "sub_000BD420", "sub_000A3FB0",
    # the deck limit of a card ("copias" in the card mod).
    "sub_0007EF00",
    # Cerezas naturia: the card sent to the Graveyard (Nimble Momonga's
    # trigger) and Momonga's resolver.
    "sub_000C7C50", "sub_000B13A0",
    # src/raton.c: the "Selecciona la posición de la carta" box's keys.
    "sub_00093EE0",
    # src/raton.c: the "Selecciona Tipo / Atributo" and "Declara el nivel" boxes' keys.
    "sub_00090030", "sub_000904B0", "sub_00090300",
    # src/cartas.c: cards "as another one" (Des Lacooda, Hombre de Nieve, Golem
    # Centinela): the Flip Summon machine, Des Lacooda's resolver, the valid
    # squares of Man-Eater Bug and Tomb Guardian's Guard, the turn-start pass.
    "sub_00094AF0", "sub_000ADDE0", "sub_000BB9E0", "sub_000BAFE0", "sub_000CFE70",
    # and the commands of a field card (the "face-down" one for that group).
    "sub_00098EF0",
    # Carga del Poderoso: the ATK/DEF calculation of a field card (Banner of Courage is in it).
    "sub_000D5DD0",
    # Drillago: can a monster attack directly (Mystic Lamp, Jinzo #7...).
    "sub_00099980",
    # Goblin Entrenador: the End Phase steps (Garuda's trigger) and Garuda's resolver.
    "sub_0009EC80", "sub_000B6DA0",
    # Soldado del Brillo Negro: the hand Special Summon check, the summon
    # machine, the Spirits' Graveyard check, "cannot be Normal Summoned",
    # Barrel Dragon's condition and resolver.
    "sub_00093A30", "sub_000968D0", "sub_000937E0", "sub_000D28B0", "sub_000A5870", "sub_000B3510", "sub_00099BC0", "sub_00099940",
    # Vidente: the hand selection of mode 6 (its own hand instead of the opponent's).
    "sub_00090AA0",
    # Guerrero de la Fortaleza: the battle result (destroyed flag, battle damage).
    "sub_0007F8D0",
    # Caius: Dragon Seeker's resolver, valid cell and choice (Caius uses its trigger).
    "sub_000AB0F0", "sub_000A2CB0", "sub_000BB830",
    # Isaac: a card from the field to the Graveyard (order 0x5A; the battle end).
    "sub_000C6960", "sub_000C8620", "sub_000C6390",
    # Isaac como Macrocosmos: la pregunta "hay Macrocosmos" y la orden 0x54 (destruccion en batalla).
    "sub_000D34B0", "sub_000CA130",
    # Abare Ushioni: Time Wizard's condition and resolver (coin toss -> 1000 damage).
    "sub_000A5010", "sub_000B2D10",
    # Acantilado Naturia: the non-battle field -> Graveyard triggers (Sangan's) and Giant Rat's resolver.
    "sub_000C5DA0", "sub_000B0FF0",
    # Berenjena Naturia: Magician of Faith's target choice and resolver (Graveyard -> hand).
    "sub_000BABA0", "sub_000A9570",
    # Naturia Vanidosa: who may answer an activation, Maryokutai's condition and resolver.
    "sub_00082140", "sub_000A8500", "sub_000B71A0",
    # Acantilado/Berenjena when tributed: the tribute registers Sangan's trigger by image.
    "sub_000C7980",
    # Duel scene teardown: the mascot viewers let their models (and textures) go first.
    "sub_00045750",
    # The battle viewer destructor: the mascot viewers are destroyed right after the game's own (src/mascota.c).
    "sub_0005B9C0",
    # The field effects object's destructor: who still holds its nodes, logged (src/mascota.c).
    "sub_0005C170",
    # src/nombres.c: a shared name let go once too many / not in its table (crash 2026-11-22 18:17).
    "sub_00030BD0", "sub_001AD030",
    # The main menu banner's text (src/raton.c): "Perfil" while the pointer is on the Perfil button.
    "sub_0010C6B0",
    # Every trigger registration: Berenjena only via Sangan (no flip trigger from Magician of Faith).
    "sub_00081EF0",
    # src/auto.c: the name plates slide in/out (AUTO follows the player's).
    "sub_0004F440",
    # src/auto.c: AUTO on, the chain Yes/No box ("Tu adversario Coloco un monstruo...") never shown.
    "sub_00082280", "sub_000BF9C0",
    # Naturia Vanidosa: may chain on any turn (the game only checks Maryokutai off-turn vs spells).
    "sub_000A4EC0",
    # Chest diagnostics: the game's two functions that add copies to the chest.
    "sub_000D9420", "sub_000D9720",
    # Cosmoremolacha Naturia: the two Set machines (order 0x52).
    "sub_000951D0", "sub_00095450",
    # src/raton.c: the coin toss request (mode 0x13): arrows and coin with the mouse.
    "sub_00090D20",
    # src/mascota.c: the duel mascots: the duel scene, the viewers drawn, on screen, on/off, camera.
    "sub_00057D60", "sub_00057020", "sub_000583A0", "sub_000444B0", "sub_00056FC0", "sub_00043800",
    # src/mascota.c: the duel scene's 2D layer k (the mascots go in before layer 1).
    "sub_00044360",
    "sub_0005C3F0",                     # the temple drawn: its camera logged (mascota.c)
    "sub_000C8BC0",                     # lose n life points: the full damage for the number (danio.c)
    "sub_0009A540",                     # the combat's step 1: held while the attack's ray runs (rayo.c)
    # src/cadena.c: activation glow and chain numbers (effect machine, duel messages, orders)
    "sub_000A1990", "sub_000A1A10", "sub_000A1AA0", "sub_000C07F0", "sub_00085720", "sub_00085780", "sub_00081150",
    "sub_000DEE80", "sub_000DEF20",       # cartas.c: the deck editor honours the mod's copy limits
    "sub_00013D10",                       # fundas.c: who builds card-back sprites (research)
    "sub_000D8790",                       # cadena.c: the chain panel shows once
    "sub_00055530",                       # cadena.c: the chain panel as a staircase (Master Duel)
    "sub_00047CD0", "sub_000D8600",       # cadena.c: list titles translated; the chain panel is up
    "sub_000662A0",                     # the Exodia win scene: the mascot punches (mascota.c)
    "sub_001057D0",                     # triple duel order: mouse (raton.c tri_pointer)
    # src/diag_escala.c: Alchemy's matrix *= scale (the 3D objects' scale and
    # position, logged for the model viewer).
    "sub_001E62D0",
    # src/premios_extra.c: a reward slot of a win (drawn again while it is a
    # card you already have 3 of; the new ones noted for the notice).
    "sub_0008CBB0",
    # src/vibracion.c: the pad's rumble straight to the pad (XPP crashed under XInputSetState).
    "sub_001FE410",
    # src/prestado.c: the player's deck at a duel's start (the loaned opponent deck instead).
    "sub_0007DC40",
    # src/diag_carga.c: Alchemy's PNG loader, libpng's png_read_row and
    # zlib's inflate, timed (the loads between screens; diagnostic).
    "sub_0017AF80", "sub_0017B1A0", "sub_0028F180",
    # src/zlib_nativo.c: zlib 1.1.3's inflate and crc32 natively (miniz).
    "sub_00297940", "sub_00297650",
    # src/d3dx_nativo.c: D3DX's pixel row converters natively (checked against the game's).
    "sub_003F3218", "sub_003F3B9D", "sub_003F43B6", "sub_003F4B5C",
    # src/sem_nativo.c: sem_init/destroy/trywait/wait/post with the count in memory.
    "sub_002F1C70", "sub_002F1CC0", "sub_002F1CF0", "sub_002F1D30", "sub_002F1D70",
    # src/diag_carga.c: D3D KickOff's wait for the walker, timed per frame
    "sub_00375B70",
    # src/png_nativo.c: libpng 1.0.8 png_read_filter_row (native) and
    # png_do_read_transformations (timed)
    "sub_00294C80", "sub_0028C280",
]

# (function, exact text -> other text), each found exactly once in the file
# that defines the function.
SUBS = []


def main():
    bad = 0
    files = sorted(GEN.glob("recomp_*.c"))
    texts = {p: p.read_text(encoding="utf-8") for p in files}
    changed = set()

    def owner(f):
        for p, s in texts.items():
            if re.search(r"^void %s(_pad)?\(void\)" % f, s, re.M):
                return p
        return None

    for f in PADS:
        p = owner(f)
        if p is None:
            print(f"{f}: definition not found", file=sys.stderr)
            bad = 1
            continue
        s = texts[p]
        if re.search(r"^void %s_pad\(void\)" % f, s, re.M):
            print(f"{p.name}: {f} already renamed")
            continue
        texts[p] = re.sub(r"^void %s\(void\)" % f,
                          "void %s_pad(void) /* ygo-port: wrapped in ygo-apex/src */" % f,
                          s, count=1, flags=re.M)
        changed.add(p)
        print(f"{p.name}: {f} -> {f}_pad")
    for f, old, new in SUBS:
        p = owner(f)
        s = texts.get(p, "")
        if new in s:
            print(f"{p.name}: {new.strip()} already there")
            continue
        if s.count(old) != 1:
            print(f"{f}: {old.strip()} not found once", file=sys.stderr)
            bad = 1
            continue
        texts[p] = s.replace(old, new)
        changed.add(p)
        print(f"{p.name}: {old.strip()} -> {new.strip()}")
    for p in changed:
        p.write_text(texts[p], encoding="utf-8")
    return bad


if __name__ == "__main__":
    sys.exit(main())
