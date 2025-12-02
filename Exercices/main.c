#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>
#include <string.h>
#include <unistd.h>
const char *HOST_URI = "qemu+ssh://user@172.19.3.1/system";
const char *VM="vm10";


/* Ouvre une connexion vers un hyperviseur donné */
virConnectPtr openConnection(const char *uri) {
    virConnectPtr conn = virConnectOpen(uri);
    if (conn == NULL) {
        fprintf(stderr, "Failed to open connection to %s\n", uri);
        exit(1);
    }
    printf("Connection successful :)\n");
    return conn;
}

/* Ferme proprement la connexion */
void closeConnection(virConnectPtr conn) {
    virConnectClose(conn);
}

// *************** EX1 ***************** //
void exercice1() {
    virConnectPtr conn = openConnection(HOST_URI);
    closeConnection(conn);
}

// *************** EX2 ***************** //
void printHostInfo(virConnectPtr conn) {
    char *host = virConnectGetHostname(conn);
    int vcpus = virConnectGetMaxVcpus(conn, NULL);
    unsigned long long free_mem = virNodeGetFreeMemory(conn);

    printf("Hostname: %s\n", host);
    printf("Max virtual CPUs: %d\n", vcpus);
    printf("Free memory: %llu KB\n", free_mem / 1024);

    free(host);
}

void exercice2(virConnectPtr conn) {
    printHostInfo(conn);    
}

// *************** EX3 ***************** //
void printDomainsInfo(virConnectPtr conn) {
    // Domaines actifs 
    int numActive = virConnectNumOfDomains(conn);
    printf("Active domain IDs:\n");

    if (numActive > 0) {
        int *activeIDs = malloc(sizeof(int) * numActive);
        virConnectListDomains(conn, activeIDs, numActive);

        for (int i = 0; i < numActive; i++) {
            virDomainPtr dom = virDomainLookupByID(conn, activeIDs[i]);
            if (dom) {
                virDomainInfo info;
                if (virDomainGetInfo(dom, &info) == 0) {
                    const char *name = virDomainGetName(dom);
                    printf(" id : %d, nom : %s\n", activeIDs[i], name);
                    printf("    state : %d\n", info.state);
                    printf("    maxMem : %lu\n", info.maxMem);
                    printf("    memory : %lu\n", info.memory);
                    printf("    nrVirtCpu : %u\n", info.nrVirtCpu);
                    printf("    cpuTime : %llu\n", info.cpuTime);
                }
                virDomainFree(dom);
            }
        }
        free(activeIDs);
    }

    // Domaines inactifs
    printf("Inactive domain names:\n");
    int numInactive = virConnectNumOfDefinedDomains(conn);
    if (numInactive > 0) {
        char **inactiveNames = malloc(sizeof(char *) * numInactive);
        virConnectListDefinedDomains(conn, inactiveNames, numInactive);

        for (int i = 0; i < numInactive; i++) {
            printf("   %s\n", inactiveNames[i]);
            free(inactiveNames[i]);
        }
        free(inactiveNames);
    }
}

void exercice3(virConnectPtr conn) {
    printHostInfo(conn);
    int encrypted = virConnectIsEncrypted(conn);
    printf("Connection is encrypted: %d\n", encrypted);
    printf(">>> Liste des domaines\n");
    printDomainsInfo(conn);
}

// *************** EX5 ***************** //
void stopVM(virConnectPtr conn, const char *vmName) {
    virDomainPtr dom = virDomainLookupByName(conn, vmName);
    if (dom == NULL) {
        printf("VM %s introuvable\n", vmName);
        return;
    }
    printf(">>> Arrêt de %s \n", vmName);

    int ret = virDomainDestroy(dom);  // arrêt brutal immédiat
    if (ret != 0)
        printf("Échec de l'arrêt de %s.\n", vmName);

    virDomainFree(dom);
}

void startVM(virConnectPtr conn, const char *vmName) {
    virDomainPtr dom = virDomainLookupByName(conn, vmName);
    if (dom == NULL) {
        printf("VM %s introuvable\n", vmName);
        return;
    }
    printf(">>> Démarrage de %s\n", vmName);

    int ret = virDomainCreate(dom);
    if (ret != 0)
        printf("Échec du démarrage de %s.\n", vmName);

    virDomainFree(dom);
}

void exercice5(virConnectPtr conn) {
    
    exercice3(conn);
    stopVM(conn, VM);
    printDomainsInfo(conn);
    sleep(3); // attendre un peu avant de redémarrer
    startVM(conn, VM);
    printDomainsInfo(conn);
}

// *************** EX6 ***************** //
void suspendVM(virConnectPtr conn, const char *vmName) {
    virDomainPtr dom = virDomainLookupByName(conn, vmName);
    if (dom) {
        printf(">>> Suspendre %s\n", vmName);
        virDomainSuspend(dom);
        virDomainFree(dom);
    }
}

void resumeVM(virConnectPtr conn, const char *vmName) {
    virDomainPtr dom = virDomainLookupByName(conn, vmName);
    if (dom) {
        printf(">>> Reprendre %s\n", vmName);
        virDomainResume(dom);
        virDomainFree(dom);
    }
}

void exercice6(virConnectPtr conn) {
    exercice5(conn);
    suspendVM(conn, VM);
    printDomainsInfo(conn);
    sleep(3);
    resumeVM(conn, VM);
    printDomainsInfo(conn);
}

// *************** main ***************** //
int main() {
    // Commentez/Décommentez au fur et à mesure selon l'exercice à tester :
    printf("--------------exercice1----------------\n");
    exercice1();

    virConnectPtr conn = openConnection(HOST_URI);
    printf("--------------exercice2----------------\n");
    exercice2(conn);
    printf("--------------exercice3----------------\n");
    exercice3(conn);
    printf("--------------exercice5----------------\n");
    exercice5(conn);
    printf("--------------exercice6----------------\n");
    exercice6(conn);

    closeConnection(conn);
    return 0;
}
