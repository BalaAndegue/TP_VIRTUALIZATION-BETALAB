#include "../../vm_manager/manager.h"
#include "../../load_manager/loader.h"
#include "../../utils/util.h"
#include "../../memory_manager/memory.h"

#define VM_MEMORY_SIZE  0xF000
#define STACK_ADDRESS   0xE000
#define STACK_SIZE      0x3000

/* TODO */
int main(int argc, char *argv)
{
    int fd;
    struct VMState state;
    uint8_t *guest_mem;

    printf("RESTORE OPERATION\n");
    /* Crate a blank VM */
    create_vm();
    // Au lieu de add_memory, on utilise souvent create_guest_physical_memory 
    // ou la fonction équivalente de votre manager pour allouer la RAM.
    add_memory(VM_MEMORY_SIZE, 0x0); 
    
    // On crée le vCPU (nécessaire pour pouvoir appeler KVM_SET_REGS ensuite)
    create_bootstrap();        
    
    guest_mem = get_memory();
    /* Read the VM image file */
    fd = open("vm_image.bin", O_RDONLY);
    if (fd < 0) {
        perror("ERREUR : Impossible de trouver vm_image.bin");
        return 1;
    }

    // On lit les structures dans le même ordre que le dump()
    read(fd, &state.regs, sizeof(state.regs));
    read(fd, &state.sregs, sizeof(state.sregs));
    read(fd, &state.fpu, sizeof(state.fpu));
    read(fd, &state.memory_size, sizeof(size_t));

    // Restauration du contenu brut de la mémoire (incluant les tables de pages)
    read(fd, guest_mem, state.memory_size);
    close(fd);
    /* Update the VM state */
    // On restaure d'abord les SREGS car ils contiennent CR3 et les segments 64-bit
    if (ioctl(vcpufd, KVM_SET_SREGS, &state.sregs) < 0) err(1, "KVM_SET_SREGS");
    
    // On restaure les registres généraux (RIP, RSP, RAX...)
    if (ioctl(vcpufd, KVM_SET_REGS, &state.regs) < 0) err(1, "KVM_SET_REGS");
    
    // On restaure l'unité de calcul flottant
    if (ioctl(vcpufd, KVM_SET_FPU, &state.fpu) < 0) err(1, "KVM_SET_FPU");

    /* Launch the VM */
    printf("REPRISE DE L'EXECUTION...\n");
    // launch_vm doit être appelé sans réinitialiser RIP manuellement 
    // car state.regs.rip contient déjà l'adresse après le SAVE()
    launch_vm(state.regs.rax, state.regs.rbx, state.regs.rip);
    
    return 0;
}
