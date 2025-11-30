#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>
#include <string.h>
#include <unistd.h>
const char *HOST_URI = "qemu+ssh://user@172.19.3.1/system";
/* ==========================================================
   FONCTIONS COMMUNES
   ========================================================== */

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

/* ==========================================================
   EXERCICE 1 — Connexion distante
   ========================================================== */
void exercice1() {
    virConnectPtr conn = openConnection(HOST_URI);
    closeConnection(conn);
}

/* ==========================================================
   EXERCICE 2 — Informations sur l’hôte
   ========================================================== */
void printHostInfo(virConnectPtr conn) {
    char *host = virConnectGetHostname(conn);
    int vcpus = virConnectGetMaxVcpus(conn, NULL);
    unsigned long long free_mem = virNodeGetFreeMemory(conn);

    printf("Hostname: %s\n", host);
    printf("Max virtual CPUs: %d\n", vcpus);
    printf("Free memory: %llu KB\n", free_mem / 1024);

    free(host);
}

void exercice2() {
    virConnectPtr conn = openConnection(HOST_URI);
    printHostInfo(conn);
    closeConnection(conn);
}

/* ==========================================================
   EXERCICE 3 — Liste des domaines
   ========================================================== */
void printDomainsInfo(virConnectPtr conn) {
    /* --- Domaines actifs --- */
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

    /* --- Domaines inactifs --- */
    printf("Inactive domain names:\n");
    int numInactive = virConnectNumOfDefinedDomains(conn);
    if (numInactive > 0) {
        char **inactiveNames = malloc(sizeof(char *) * numInactive);
        virConnectListDefinedDomains(conn, inactiveNames, numInactive);

        for (int i = 0; i < numInactive; i++) {
            printf("%s\n", inactiveNames[i]);
            free(inactiveNames[i]);
        }
        free(inactiveNames);
    }
}

/* Fonction principale Exercice 3 */
void exercice3() {
    virConnectPtr conn = openConnection(HOST_URI);
    printHostInfo(conn);
    int encrypted = virConnectIsEncrypted(conn);
    printf("Connection is encrypted: %d\n", encrypted);
    printDomainsInfo(conn);
    closeConnection(conn);
}

/* ==========================================================
   EXERCICE 5 — Arrêt et redémarrage d'une VM
   ========================================================== */
/* ==========================================================
   EXERCICE 5 — Arrêt et redémarrage d'une VM
   ========================================================== */
void stopVM(virConnectPtr conn, const char *vmName) {
    virDomainPtr dom = virDomainLookupByName(conn, vmName);
    if (dom == NULL) {
        printf("VM %s introuvable\n", vmName);
        return;
    }
    printf(">>> Arrêt de %s (force)\n", vmName);

    int ret = virDomainDestroy(dom);  // arrêt brutal immédiat
    if (ret == 0)
        printf("VM %s arrêtée avec succès.\n", vmName);
    else
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
    if (ret == 0)
        printf("VM %s démarrée avec succès.\n", vmName);
    else
        printf("Échec du démarrage de %s.\n", vmName);

    virDomainFree(dom);
}

void exercice5() {
    virConnectPtr conn = openConnection(HOST_URI);
    stopVM(conn, "vm-debian13");
    sleep(3); // attendre un peu avant de redémarrer
    startVM(conn, "vm-debian13");
    closeConnection(conn);
}


/* ==========================================================
   EXERCICE 6 — Suspendre et restaurer une VM
   ========================================================== */
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

void exercice6() {
    virConnectPtr conn = openConnection(HOST_URI);
    suspendVM(conn, "vm-debian13");
    sleep(3);
    resumeVM(conn, "vm-debian13");
    closeConnection(conn);
}
/* ==========================================================
   EXERCICE 7 — Migration d'une VM vers un autre hôte
   ========================================================== */

/* Fonction réutilisable : migration d'une VM vers un autre hôte */
void migrateVM(virConnectPtr srcConn, const char *vmName, const char *destURI) {
    virDomainPtr dom = virDomainLookupByName(srcConn, vmName);
    if (dom == NULL) {
        printf("VM %s introuvable sur l’hôte source\n", vmName);
        return;
    }

    printf(">>> Migration de %s vers %s\n", vmName, destURI);

    /* Flags pour la migration : LIVE + PERSISTENT */
    unsigned long flags = VIR_MIGRATE_LIVE | VIR_MIGRATE_PERSIST_DEST;

    int ret = virDomainMigrateToURI(dom, destURI, flags, NULL, 0);

    if (ret == 0)
        printf("✅ Migration de %s terminée avec succès.\n", vmName);
    else
        printf("❌ Échec de la migration de %s.\n", vmName);

    virDomainFree(dom);
}

/* Fonction principale pour tester la migration */
void exercice7() {
    const char *DEST_URI = "qemu+ssh://user@172.19.3.20/system"; // <-- change l’IP du voisin ici

    virConnectPtr srcConn = openConnection(HOST_URI); // hôte source
    migrateVM(srcConn, "vm-debian13", DEST_URI);
    closeConnection(srcConn);
}

/* ==========================================================
   MAIN — Pour tester les exercices
   ========================================================== */
int main() {
    // Décommente au fur et à mesure selon l'exercice à tester :
    // printf("--------------exercice1----------------\n");
    // exercice1();
    // printf("--------------exercice2----------------\n");
    // exercice2();
    // printf("--------------exercice3----------------\n");
    // exercice3();
    // printf("--------------exercice5----------------\n");
    // exercice5();
    // printf("--------------exercice6----------------\n");
    // exercice6();
    printf("--------------exercice7----------------\n");
    exercice7();

    return 0;
}
