### VIRTUALIZATION LAB
# ScratchVM - Lab 1: Micro-Virtual Machine avec KVM

Ce projet consiste en la réalisation d'un moniteur de machine virtuelle (VMM) minimaliste. L'objectif principal est de piloter l'API KVM du noyau Linux pour charger et exécuter du code binaire binaire "Bare Metal" sur un processeur virtuel unique (vCPU).

## 1. Objectifs du TP
Le Lab 1 vise à établir les fondations d'un hyperviseur en accomplissant les tâches suivantes :
* **Initialisation matérielle** : Communication avec `/dev/kvm` pour instancier une VM.
* [cite_start]**Gestion de la mémoire** : Allocation et mapping de la RAM "Guest" à partir de la mémoire de l'hôte[cite: 23].
* [cite_start]**Configuration CPU** : Initialisation des registres x86 (RAX, RBX, RIP) et des segments de contrôle (CS) [cite: 13, 81-83].
* **Interception d'I/O** : Gestion des sorties via le mécanisme de "VM Exit" pour permettre à la VM de communiquer avec le terminal de l'hôte.

## 2. Architecture du Système

Le projet est divisé en quatre couches hiérarchiques :
1. **L'Espace Utilisateur (VMM)** : Le programme C qui orchestre la VM, gère la mémoire et traite les sorties via des `ioctls`.
2. **L'API KVM** : L'interface noyau permettant de configurer les structures de données de virtualisation (VMCS/VMCB).
3. **Le vCPU (Guest)** : Le processeur virtuel qui exécute les instructions machine.
4. **Le Code Binaire** : Un programme binaire minimaliste chargé à l'adresse `0x0000` qui réalise une addition arithmétique et envoie le résultat sur un port série virtuel.



## 3. Fonctionnement Technique

### Flux d'Exécution
1. **Initialisation** : Le VMM ouvre `/dev/kvm` et crée une instance de VM et un vCPU.
2. [cite_start]**Mapping Mémoire** : Une zone de mémoire est allouée via `mmap()` et liée à l'espace physique de la VM via `KVM_SET_USER_MEMORY_REGION`[cite: 23].
3. **Chargement** : Le code binaire est copié dans la RAM de la VM.
4. **Lancement** : Le VMM définit les registres initiaux et appelle `KVM_RUN`.
5. **VM Exit** : Lorsque la VM exécute `OUT 0x3f8`, le processeur sort du mode invité. Le VMM intercepte la sortie, affiche le caractère, et relance le vCPU jusqu'à l'instruction `HLT`.

### Structure des Fichiers
* [cite_start]`vm_manager/manager.c` : Cœur de la logique KVM (création, exécution, handlers) [cite: 12, 29, 81-83].
* [cite_start]`load_manager/loader.c` : Logique de transfert du binaire vers la RAM virtuelle[cite: 6].
* `vm_src/main.c` : Point d'entrée définissant les paramètres de la VM et le code binaire à exécuter.

## 4. Prérequis et Déploiement
* **Système** : Linux avec le module KVM activé.
* **Permissions** : Accès en lecture/écriture sur `/dev/kvm`.
* **Compilation** :
  ```bash
  cd vm_src
  make all
  make run
