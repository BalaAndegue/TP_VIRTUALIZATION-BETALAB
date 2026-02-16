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
    /*int ret = -1;
    return ret;
    */
    //1. ouvrir le module KVM pour obtenur un descripteur systeme
    kvmfd = open(DEV_KVM, O_RDWR | O_CLOEXEC);
    if (kvmfd == -1)
        err(1, "Cannot open /dev/kvm");

    // 2. creer l'instance de la VM . Cela retourne un descripteur de fichier (vmfd)
    //qui represente la VM elle-meme
    vmfd = ioctl(kvmfd, KVM_CREATE_VM, (unsigned long)0);
    if (vmfd == -1)
        err(1, "Cannot create VM");

    return vmfd; //[cite: 12]
}

/***
 * TODO
 * This Function Creates Guest Physical Memory Based On Host Virtual Memory
 * And Submits The Memory Area To KVM.
 */
int create_guest_physical_memory(size_t size)
{
    /*int ret = -1;
    return ret;
    */
    struct kvm_userspace_memory_region region ;

    //1. allouer un bloc de memoire dans notre programme Hote
    // on utilise mmap pour avoir une zone de memoire alignee et propre
    memory = mmap(NULL , size , PROT_READ | PROT_WRITE , MAP_PRIVATE | MAP_ANONYMOUS , -1 , 0);
    if (memory == MAP_FAILED)
        err(1, "mmap memory");

    // 2. definir la region memoire pour KVM
    region.slot = slot_id++;                //identifiant de la zone (0 pour la RAM principale)
    region.flags = 0;                       //pas de flags particluiers (ex : Read Only)
    region.memory_size = size;              //Taille (VM_MEMORY_SIZE = 0xF000) [cite : 23]
    region.userspace_addr = (uint64_t)memory;   //Addresse reelle dans notre code C    //Adresse de depart vue par la VM
    region.create_guest_physical_memory = 0x0;     //Adresse de depart vue par la VM

    //3. envoyer cette configuration a KVM
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

    //1. Lire l'etat des registres (qui sont vides a la creation)
    ioctl(vcpufd, KVM_GET_REGS, &regs);

    /* Update Of The vCPU Registers - RAX, RBX and RIP */
    regs.rflags = 2;     //requis pour l'architecture x89 (bit reserve)

    // pour que e binaire fasse "2 + 4 = 6"
    regs.rax = 2;     //valeur initiale dans EAX
    regs.rbx = 4;     //valeur initiale dans EBX


    // le pointeur d'instruction (RIP) doit pointer sur l'adresse 0.
    //car c'est la que load_vm_code a copie le binaire .
    regs.rip = 0;   // [cite : 81-83]

    //2.sauvegarde ces valeurs dans le vCPU
    ret = ioctl(vcpufd, KVM_SET_REGS, &regs);
    if (ret == -1)
        err(1, "KVM_SET_REGS");

    //3.boucle d'execution 
    while (1)
    {
        //on demande a KVM de faire tourner le processeur
        if (ioctl(vcpufd, KVM_RUN, NULL) == -1)
            err(1, "KVM_RUN");
        else
        {
            //des qu'il y a un VM EXIT , le hnadler analyse la raison
            if (vmexit_handler(run->exit_reason) == 0)
                break;  //Arret si HLT est recu
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
    
    //verifie si la VM essaie d'ecrire sur le port serie (0c3f8)
    case KVM_EXIT_IO:
        if (run->io.direction == KVM_EXIT_IO_OUT && run->io.size == 1 && run->io.port == 0x3f8 && run->io.count == 1)
            putchar(*(((char *)run) + run->io.data_offset)); //affiche '6' sur notre terminal
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