#include "manager.h"

int kvmfd, vmfd, vcpufd;
struct kvm_run *run;
uint8_t *memory;
int slot_id = 0;

/***
 * TODO
 * This Function Creates Virtual Machine File Descriptor.
 */
int create_vm(void)
{
    /*
    int ret = -1;
    return ret;
    */
    // 1. Ouvrir le module KVM pour obtenir un descripteur système
    kvmfd = open(DEV_KVM, O_RDWR | O_CLOEXEC);
    if (kvmfd == -1) err(1, "Impossible d'ouvrir /dev/kvm");

    // 2. Créer l'instance de la VM. Cela retourne un descripteur de fichier (vmfd)
    // qui représente l'ordinateur virtuel lui-même.
    vmfd = ioctl(kvmfd, KVM_CREATE_VM, (unsigned long)0);
    if (vmfd == -1) err(1, "KVM_CREATE_VM");

    return vmfd; 
}

/***
 * TODO
 * This Function Creates Guest Physical Memory Based On Host Virtual Memory
 * And Submits The Memory Area To KVM.
 */
int create_guest_physical_memory(size_t size)
{
    struct kvm_userspace_memory_region region;

    // 1. Allouer un bloc de mémoire dans notre programme C (l'Hôte)
    // On utilise mmap pour avoir une zone de mémoire alignée et propre.
    memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) err(1, "mmap memory");

    // 2. Définir la région mémoire pour KVM
    region.slot = slot_id++;          // Identifiant de la zone (0 pour la RAM principale)
    region.flags = 0;                 // Pas de flags particuliers (ex: Read Only)
    region.guest_phys_addr = 0x0;      // L'adresse de départ vue par la VM
    region.memory_size = size;        [cite_start]// Taille (VM_MEMORY_SIZE = 0xF000) [cite: 23]
    region.userspace_addr = (uint64_t)memory; // Adresse réelle dans votre code C

    // 3. Envoyer cette configuration à KVM
    return ioctl(vmfd, KVM_SET_USER_MEMORY_REGION, &region);
}

int create_bootstrap()
{
    struct kvm_sregs sregs;
    size_t mmap_size;
    int ret = -1;
    int nent = 128;
    struct kvm_cpuid2 *cpuid2 = (struct kvm_cpuid2 *)malloc(sizeof(struct kvm_cpuid2) + nent * sizeof(struct kvm_cpuid_entry2));
    cpuid2->nent = nent;
    if (ioctl(kvmfd, KVM_GET_SUPPORTED_CPUID, cpuid2) < 0)
        err(1, "cant get cpuid");

    vcpufd = ioctl(vmfd, KVM_CREATE_VCPU, (unsigned long)0);

    if (vcpufd == -1)
        err(1, "Cannot create vcpu\n");

    if (ioctl(vcpufd, KVM_SET_CPUID2, cpuid2) < 0)
        err(1, "cannot set cpuid things\n");

    ret = ioctl(kvmfd, KVM_GET_VCPU_MMAP_SIZE, NULL);
    if (ret == -1)
        err(1, "KVM_GET_VCPU_MMAP_SIZE");
    mmap_size = ret;
    if (mmap_size < sizeof(*run))
        errx(1, "KVM_GET_VCPU_MMAP_SIZE unexpectedly small");
    run = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, vcpufd, 0);
    if (!run)
        err(1, "mmap vcpu");

    ret = ioctl(vcpufd, KVM_GET_SREGS, &sregs);
    if (ret == -1)
        err(1, "KVM_GET_SREGS");
    sregs.cs.base = 0;
    sregs.cs.selector = 0;
    ret = ioctl(vcpufd, KVM_SET_SREGS, &sregs);
    if (ret == -1)
        err(1, "KVM_SET_SREGS");
    return ret;
}

/***
 * TODO
 * This Function Updates vCPU Registers And Runs It.
 */
int launch_vm()
{
    int ret;
    struct kvm_regs regs;
    
    // 1. Lire l'état actuel des registres (qui sont vides à la création)
    ioctl(vcpufd, KVM_GET_REGS, &regs);

    /* Update Of The vCPU Registers - RAX, RBX and RIP */
    regs.rflags = 2;   // Requis pour l'architecture x86 (bit réservé)
    
    // Pour que le binaire fasse "2 + 4 = 6" :
    regs.rax = 2;      // Valeur initiale dans EAX
    regs.rbx = 4;      // Valeur initiale dans EBX
    
    // Le pointeur d'instruction (RIP) doit pointer sur l'adresse 0,
    // car c'est là que load_vm_code a copié le binaire.
    regs.rip = 0;      [cite_start]// [cite: 81-83]

    // 2. Sauvegarder ces valeurs dans le vCPU
    ret = ioctl(vcpufd, KVM_SET_REGS, &regs);
    if (ret == -1) err(1, "KVM_SET_REGS");

    // 3. Boucle d'exécution
    while (1)
    {
        // On demande à KVM de faire tourner le processeur
        if (ioctl(vcpufd, KVM_RUN, NULL) == -1)
            err(1, "KVM_RUN");
        else
        {
            // Dès qu'il y a un VM Exit, le handler analyse la raison
            if (vmexit_handler(run->exit_reason) == 0)
                break; // Arrêt si HLT est reçu
        }
    }
    return 0;
}

int vmexit_handler(int exit_reason)
{
    struct kvm_regs regs;
    struct kvm_sregs sregs;
    struct kvm_fpu fpu;
    ioctl(vcpufd, KVM_GET_FPU, &fpu);
    ioctl(vcpufd, KVM_GET_REGS, &regs);
    ioctl(vcpufd, KVM_GET_SREGS, &sregs);
    switch (exit_reason)
    {
    case KVM_EXIT_HLT:
        printf("KVM_EXIT_HLT\n");
        return 0;
    case KVM_EXIT_IO:
        if (run->io.direction == KVM_EXIT_IO_OUT && run->io.size == 1 && run->io.port == 0x3f8 && run->io.count == 1)
            putchar(*(((char *)run) + run->io.data_offset));
        else
            errx(1, "unhandled KVM_EXIT_IO");
        break;
    case KVM_EXIT_FAIL_ENTRY:
        errx(1, "KVM_EXIT_FAIL_ENTRY: hardware_entry_failure_reason = 0x%llx",
             (unsigned long long)run->fail_entry.hardware_entry_failure_reason);
    case KVM_EXIT_INTERNAL_ERROR:
        errx(1, "KVM_EXIT_INTERNAL_ERROR: suberror = 0x%x RIP %llx", run->internal.suberror, regs.rip);
    case KVM_EXIT_SHUTDOWN:
        errx(1, "SHUTDOWN RIP %llx - RSP %llx - RAX %llx\n", regs.rip, regs.rsp, regs.rax);
        break;
    default:
        errx(1, "exit_reason = 0x%d rip %llx, rax %llx", run->exit_reason, regs.rip, regs.rax);
    }
    return 1;
}

uint8_t *get_memory()
{
    return memory;
}