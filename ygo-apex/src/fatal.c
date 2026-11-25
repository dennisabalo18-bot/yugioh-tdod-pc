/*
 * ygo-port: the game's fatal exits.
 *
 * Yu-Gi-Oh TDOD has no Quitf with a message (RDR's did): when something it
 * cannot live without fails (e.g. sub_0001177E: the 3D device, then
 * exit(1)), it calls the CRT's exit. That runs doexit (sub_0035614F), which
 * ends in KeBugCheck; our kernel returns from it, so the guest would spin
 * there with a frozen picture. Here the exit is logged as "[FATAL] ..." with
 * the caller, and the game closes with a notice.
 *
 *   sub_00356213  exit(code)           cdecl, one argument
 *   sub_0035621F  the CRT's own exit   (abort, _amsg_exit: runtime errors)
 *
 * recomp_0119.c's definitions are renamed _pad (tools/rename_pads.py).
 */
#include <stdio.h>
#include <windows.h>
#include "recomp/gen/recomp_types.h"

void ygo_caja_volcar(const char *por);   /* caja_negra.c */

static void fatal_exit(const char *what, uint32_t code)
{
    uint32_t from = MEM32(g_esp) - 5u;          /* the call instruction */
    wchar_t box[600];

    fprintf(stderr, "[FATAL] %s(%u) from 0x%08X: the game gave up\n", what, code, from);
    fflush(stderr);
    ygo_caja_volcar("error del juego");
    {
        extern void ygo_marcar_error(const char *motivo);   /* perfil_dir.c: Configuración's report */
        char m[96];
        snprintf(m, sizeof m, "%s(%u) desde 0x%08X", what, code, from);
        ygo_marcar_error(m);
    }
    _snwprintf(box, 599,
               L"El juego encontró un error y se va a cerrar.\n"
               L"(%hs(%u) desde 0x%08X)\n\n"
               L"Abrí Configuración y tocá \"Informe de error\" para armar el archivo para mandar.\n\n"
               L"The game hit an error and will close. Open Configuración and press the error report button.",
               what, code, from);
    box[599] = 0;
    MessageBoxW(NULL, box, L"Yu-Gi-Oh! The Dawn of Destiny",
                MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND);
    ExitProcess(code ? code : 1);
}

void sub_00356213(void)
{
    fatal_exit("exit", MEM32(g_esp + 4));
}

void sub_0035621F(void)
{
    fatal_exit("CRT exit", 255);
}
