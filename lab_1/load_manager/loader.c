#include "loader.h"
/***
 * TODO
 * This Function Loads The Binary Code In The Guest Physical Memory.
 */
int load_vm_code(const uint8_t *code)
{
    // On récupère le pointeur vers le début de la RAM de la VM
    uint8_t *guest_mem = get_memory();
    
    // On copie les instructions binaires (le tableau code[]) au tout début (adresse 0)
    // Nous copions environ 16 octets qui correspondent à la taille du binaire fourni dans main.c
    memcpy(guest_mem, code, 16); // [cite: 6]
    return 0;
}