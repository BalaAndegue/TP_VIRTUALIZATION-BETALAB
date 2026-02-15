#include "loader.h"
/***
 * TODO
 * This Function Loads The Binary Code In The Guest Physical Memory.
 */
int load_vm_code(const uint8_t *code)
{
    // on recupere le pointeur vers le debut de la RAM de la VM 
    unit8_t *guest_mem = get_memory();

    //on copie les instructions binaires (le tableau code[]) au tout debut (addresse 0)
    // nous copions environ 16 octets correspondant a la taille du binaire du binaire fourni  dans le main.c
    memcpy(guest_mem, code, 16);

    return 0;
}