// *************** EX7 ***************** //
// gcc -Wall ex7.c -o ex7 -lvirt

#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <nom_vm> <dest_uri>\n", argv[0]);
        return 1;
    }

    const char *targetVM = argv[1];   // Nom de la VM à migrer 
    const char *destURI  = argv[2];   // URI de l'hôte destination

    virConnectPtr srcConn = NULL;
    virConnectPtr destConn = NULL;
    virDomainPtr dom = NULL;

    /* Connexion à l'hôte source */
    srcConn = virConnectOpen("qemu:///system");
    if (!srcConn) {
        fprintf(stderr, "Erreur : impossible de se connecter à la source (qemu:///system)\n");
        return 1;
    }
    printf("Connexion locale (source) réussie.\n");

    /* Connexion à l'hôte de destination */
    destConn = virConnectOpen(destURI);
    if (!destConn) {
        fprintf(stderr, "Erreur : impossible de se connecter à la destination (%s)\n", destURI);
        virConnectClose(srcConn);
        return 1;
    }
    printf("Connexion à la destination réussie.\n");

    /* Recherche de la VM à migrer */
    dom = virDomainLookupByName(srcConn, targetVM);
    if (!dom) {
        fprintf(stderr, "Erreur : impossible de trouver la VM %s\n", targetVM);
        virConnectClose(destConn);
        virConnectClose(srcConn);
        return 1;
    }

    printf("Migration de la VM '%s' vers %s (mode UNSAFE)...\n", targetVM, destURI);

    int flags = VIR_MIGRATE_LIVE | VIR_MIGRATE_UNSAFE |
                VIR_MIGRATE_UNDEFINE_SOURCE | VIR_MIGRATE_PERSIST_DEST;

    virDomainPtr newDom = virDomainMigrate(dom, destConn, flags, NULL, NULL, 0);

    if (newDom) {
        printf("Migration réussie vers %s.\n", destURI);
        virDomainFree(newDom);
    } else {
        fprintf(stderr, "Échec de la migration.\n");
    }

    virDomainFree(dom);
    virConnectClose(destConn);
    virConnectClose(srcConn);
    return 0;
}

