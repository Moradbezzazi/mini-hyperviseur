#include <libvirt/libvirt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libvirt/virterror.h>

#include <sys/types.h>  // pid_t
#include <sys/wait.h>   // waitpid
/*
 * Compile:
 * gcc -shared -fPIC libvirt_api.c -o libvirt_api.so -lvirt
 */
const char* list_snapshots(const char *uri, const char *name) {
    static char buffer[65536];
    buffer[0] = '\0';

    virConnectPtr conn = virConnectOpen(uri);
    if (!conn) {
        snprintf(buffer, sizeof(buffer), "{\"error\":\"Impossible de se connecter à %s\"}", uri);
        return buffer;
    }

    virDomainPtr dom = virDomainLookupByName(conn, name);
    if (!dom) {
        snprintf(buffer, sizeof(buffer), "{\"error\":\"VM %s introuvable\"}", name);
        virConnectClose(conn);
        return buffer;
    }

    unsigned int num = virDomainSnapshotNum(dom, 0);
    if (num == 0) {
        strcat(buffer, "{\"snapshots\":[]}");
        virDomainFree(dom);
        virConnectClose(conn);
        return buffer;
    }

    char **names = malloc(sizeof(char*) * num);
    if (virDomainSnapshotListNames(dom, names, num, 0) < 0) {
        snprintf(buffer, sizeof(buffer), "{\"error\":\"Impossible de lister les snapshots\"}");
        free(names);
        virDomainFree(dom);
        virConnectClose(conn);
        return buffer;
    }

    strcat(buffer, "{\"snapshots\":[");
    for (unsigned int i = 0; i < num; i++) {
        if (i > 0) strcat(buffer, ",");
        char tmp[256];
        snprintf(tmp, sizeof(tmp), "\"%s\"", names[i]);
        strcat(buffer, tmp);
    }
    strcat(buffer, "]}");

    free(names);
    virDomainFree(dom);
    virConnectClose(conn);
    return buffer;
}

const char* revert_snapshot(const char *uri, const char *name, const char *snapname) {
    static char msg[512];

    virConnectPtr conn = virConnectOpen(uri);
    if (!conn) {
        snprintf(msg, sizeof(msg), "Erreur : impossible de se connecter à %s", uri);
        return msg;
    }

    virDomainPtr dom = virDomainLookupByName(conn, name);
    if (!dom) {
        snprintf(msg, sizeof(msg), "Erreur : VM %s introuvable", name);
        virConnectClose(conn);
        return msg;
    }

    virDomainSnapshotPtr snap = virDomainSnapshotLookupByName(dom, snapname, 0);
    if (!snap) {
        snprintf(msg, sizeof(msg), "Erreur : snapshot %s introuvable", snapname);
        virDomainFree(dom);
        virConnectClose(conn);
        return msg;
    }

    if (virDomainRevertToSnapshot(snap, 0) < 0) {
        snprintf(msg, sizeof(msg), "Erreur : impossible de restaurer le snapshot %s", snapname);
        virDomainSnapshotFree(snap);
        virDomainFree(dom);
        virConnectClose(conn);
        return msg;
    }

    snprintf(msg, sizeof(msg), "Snapshot %s restauré pour la VM %s.", snapname, name);

    virDomainSnapshotFree(snap);
    virDomainFree(dom);
    virConnectClose(conn);
    return msg;
}

const char* snapshot_vm(const char *uri, const char *name, const char *snapname) {
    static char msg[512];

    virConnectPtr conn = virConnectOpen(uri);
    if (!conn) {
        snprintf(msg, sizeof(msg), "Erreur : impossible de se connecter à %s", uri);
        return msg;
    }

    virDomainPtr dom = virDomainLookupByName(conn, name);
    if (!dom) {
        snprintf(msg, sizeof(msg), "Erreur : VM %s introuvable", name);
        virConnectClose(conn);
        return msg;
    }

    char xml[512];
    snprintf(xml, sizeof(xml),
        "<domainsnapshot>"
        "  <name>%s</name>"
        "</domainsnapshot>",
        snapname
    );

    if (virDomainSnapshotCreateXML(dom, xml, 0) < 0) {
        const virError *e = virGetLastError();
        snprintf(msg, sizeof(msg), "Erreur : snapshot impossible (%s)", e ? e->message : "inconnu");
        virDomainFree(dom);
        virConnectClose(conn);
        return msg;
    }

    snprintf(msg, sizeof(msg), "Snapshot %s créé pour la VM %s.", snapname, name);

    virDomainFree(dom);
    virConnectClose(conn);
    return msg;
}

const char* restart_vm(const char *uri, const char *name) {
    static char msg[512];

    virConnectPtr conn = virConnectOpen(uri);
    if (!conn) {
        snprintf(msg, sizeof(msg), "Erreur : impossible de se connecter à %s", uri);
        return msg;
    }

    virDomainPtr dom = virDomainLookupByName(conn, name);
    if (!dom) {
        snprintf(msg, sizeof(msg), "Erreur : VM %s introuvable", name);
        virConnectClose(conn);
        return msg;
    }

    if (virDomainReboot(dom, 0) < 0) {
        const virError *e = virGetLastError();
        snprintf(msg, sizeof(msg), "Erreur : échec reboot %s (%s)", name, e ? e->message : "unknown");
        virDomainFree(dom);
        virConnectClose(conn);
        return msg;
    }

    snprintf(msg, sizeof(msg), "VM %s redémarrée.", name);

    virDomainFree(dom);
    virConnectClose(conn);
    return msg;
}

const char* list_vms(const char *uri) {
    static char buffer[65536];
    buffer[0] = '\0';

    virConnectPtr conn = virConnectOpen(uri);
    if (!conn) {
        snprintf(buffer, sizeof(buffer), "{\"error\":\"Failed to connect to %s\"}", uri);
        return buffer;
    }

    strcat(buffer, "{\"vms\":[");

    int first = 1;

    int numDomains = virConnectNumOfDomains(conn);
    if (numDomains > 0) {
        int *ids = malloc(sizeof(int) * numDomains);
        virConnectListDomains(conn, ids, numDomains);

        for (int i = 0; i < numDomains; i++) {
            virDomainPtr dom = virDomainLookupByID(conn, ids[i]);
            if (!dom) continue;

            virDomainInfo info;
            virDomainGetInfo(dom, &info);

            const char *state = "unknown";
            switch (info.state) {
                case VIR_DOMAIN_RUNNING: state = "running"; break;
                case VIR_DOMAIN_PAUSED:  state = "paused";  break;
                case VIR_DOMAIN_SHUTOFF: state = "shutoff"; break;
            }

            if (!first) strcat(buffer, ",");
            first = 0;

            char tmp[256];
            snprintf(tmp, sizeof(tmp),
                     "{\"name\":\"%s\",\"state\":\"%s\"}",
                     virDomainGetName(dom), state);

            strcat(buffer, tmp);
            virDomainFree(dom);
        }
        free(ids);
    }

    int numDefined = virConnectNumOfDefinedDomains(conn);
    if (numDefined > 0) {
        char **names = malloc(sizeof(char*) * numDefined);
        virConnectListDefinedDomains(conn, names, numDefined);

        for (int i = 0; i < numDefined; i++) {
            if (!first) strcat(buffer, ",");
            first = 0;

            char tmp[256];
            snprintf(tmp, sizeof(tmp),
                     "{\"name\":\"%s\",\"state\":\"shutoff\"}", names[i]);

            strcat(buffer, tmp);
            free(names[i]);
        }
        free(names);
    }

    strcat(buffer, "]}");
    virConnectClose(conn);
    return buffer;
}

const char* create_vm(const char *uri, const char *name,
                      const char *ram, const char *cpu,
                      const char *disk, const char *iso, const char *osinfo)
{
    static char msg[1024];

    int ram_mb   = atoi(ram);
    int vcpu     = atoi(cpu);
    int size_gb  = atoi(disk);

    /* Connexion libvirt */
    virConnectPtr conn = virConnectOpen(uri);
    if (!conn) {
        snprintf(msg, sizeof(msg),
                 "Erreur : impossible de se connecter à %s", uri);
        return msg;
    }

    /* ---------- 1) Créer le disque QCOW2 via libvirt ---------- */

    virStoragePoolPtr pool = virStoragePoolLookupByName(conn, "default");
    if (!pool) {
        snprintf(msg, sizeof(msg), "Erreur : pool 'default' introuvable");
        virConnectClose(conn);
        return msg;
    }

    char vol_xml[512];
    snprintf(vol_xml, sizeof(vol_xml),
        "<volume>"
        "  <name>%s.qcow2</name>"
        "  <capacity unit='G'>%d</capacity>"
        "  <allocation unit='G'>0</allocation>"
        "  <target>"
        "    <format type='qcow2'/>"
        "  </target>"
        "</volume>",
        name, size_gb);

    virStorageVolPtr vol = virStorageVolCreateXML(pool, vol_xml, 0);
    if (!vol) {
        snprintf(msg, sizeof(msg), "Erreur : création du volume QCOW2");
        virStoragePoolFree(pool);
        virConnectClose(conn);
        return msg;
    }

    char *disk_path = virStorageVolGetPath(vol);

    /* ---------- 2) Générer le XML de la VM ---------- */

    char xml[4096];
    snprintf(xml, sizeof(xml),
        "<domain type='kvm'>"
        "  <name>%s</name>"
        "  <memory unit='MiB'>%d</memory>"
        "  <currentMemory unit='MiB'>%d</currentMemory>"
        "  <vcpu>%d</vcpu>"
        "  <os>"
        "    <type arch='x86_64'>hvm</type>"
        "    <boot dev='cdrom'/>"
        "  </os>"
        "  <devices>"
        "    <disk type='file' device='disk'>"
        "      <driver name='qemu' type='qcow2'/>"
        "      <source file='%s'/>"
        "      <target dev='vda' bus='virtio'/>"
        "    </disk>"
        "    <disk type='file' device='cdrom'>"
        "      <source file='%s'/>"
        "      <target dev='hda' bus='ide'/>"
        "    </disk>"
        "    <graphics type='vnc' port='-1' listen='0.0.0.0'/>"
        "    <interface type='bridge'>"
        "      <source bridge='br0'/>"
        "      <model type='virtio'/>"
        "    </interface>"
        "  </devices>"
        "</domain>",
        name, ram_mb, ram_mb, vcpu, disk_path, iso
    );

    /* ---------- 3) Définir la VM ---------- */

    virDomainPtr dom = virDomainDefineXML(conn, xml);
    if (!dom) {
        snprintf(msg, sizeof(msg), "Erreur : defineXML a échoué");
        virStorageVolFree(vol);
        virStoragePoolFree(pool);
        virConnectClose(conn);
        return msg;
    }

    /* ---------- 4) Démarrer la VM ---------- */

    if (virDomainCreate(dom) < 0) {
        snprintf(msg, sizeof(msg), "Erreur : impossible de démarrer la VM");
        virDomainUndefine(dom);
        virStorageVolDelete(vol, 0);
        virStorageVolFree(vol);
        virStoragePoolFree(pool);
        virConnectClose(conn);
        return msg;
    }

    snprintf(msg, sizeof(msg),
             "VM %s créée avec succès (RAM=%dMB, CPU=%d, DISK=%dG)",
             name, ram_mb, vcpu, size_gb);

    /* Libération */
    virDomainFree(dom);
    virStorageVolFree(vol);
    virStoragePoolFree(pool);
    virConnectClose(conn);

    return msg;
}

/* ---------- START / STOP / PAUSE / RESUME / DELETE ---------- */

const char* start_vm(const char *uri, const char *name) {
    static char msg[512];
    virConnectPtr conn = virConnectOpen(uri);
    virDomainPtr dom = virDomainLookupByName(conn, name);

    virDomainCreate(dom);
    snprintf(msg, sizeof(msg), "VM %s démarrée.", name);
    virDomainFree(dom);
    virConnectClose(conn);
    return msg;
}

const char* stop_vm(const char *uri, const char *name) {
    static char msg[512];
    virConnectPtr conn = virConnectOpen(uri);
    virDomainPtr dom = virDomainLookupByName(conn, name);

    virDomainDestroy(dom);
    snprintf(msg, sizeof(msg), "VM %s arrêtée.", name);
    virDomainFree(dom);
    virConnectClose(conn);
    return msg;
}

const char* pause_vm(const char *uri, const char *name) {
    static char msg[512];
    virConnectPtr conn = virConnectOpen(uri);
    virDomainPtr dom = virDomainLookupByName(conn, name);

    virDomainSuspend(dom);
    snprintf(msg, sizeof(msg), "VM %s mise en pause.", name);
    virDomainFree(dom);
    virConnectClose(conn);
    return msg;
}

const char* resume_vm(const char *uri, const char *name) {
    static char msg[512];
    virConnectPtr conn = virConnectOpen(uri);
    virDomainPtr dom = virDomainLookupByName(conn, name);

    virDomainResume(dom);
    snprintf(msg, sizeof(msg), "VM %s reprise.", name);
    virDomainFree(dom);
    virConnectClose(conn);
    return msg;
}

// const char* destroy_vm(const char *uri, const char *name) {
//     static char msg[512];
//     virConnectPtr conn = virConnectOpen(uri);
//     virDomainPtr dom = virDomainLookupByName(conn, name);

//     virDomainDestroy(dom);
//     virDomainUndefine(dom);
//     snprintf(msg, sizeof(msg), "VM %s supprimée.", name);
//     virDomainFree(dom);
//     virConnectClose(conn);
//     return msg;
// }
const char* destroy_vm(const char *uri, const char *name) {
    static char msg[512];

    virConnectPtr conn = virConnectOpen(uri);
    if (!conn) {
        snprintf(msg, sizeof(msg), "Erreur: connexion libvirt");
        return msg;
    }

    virDomainPtr dom = virDomainLookupByName(conn, name);
    if (!dom) {
        snprintf(msg, sizeof(msg), "Erreur: VM introuvable");
        virConnectClose(conn);
        return msg;
    }

    // 1) Lire le XML pour identifier le disque
    char *xml = virDomainGetXMLDesc(dom, 0);
    char disk_path[512] = {0};

    char *p = strstr(xml, "<source file='");
    if (p)
        sscanf(p, "<source file='%511[^']'", disk_path);

    // 2) Stopper la VM
    virDomainDestroy(dom);

    // 3) Undefine
    virDomainUndefine(dom);

    // 4) Supprimer le disque s’il existe
    if (strlen(disk_path) > 0) {
        virStoragePoolPtr pool = virStoragePoolLookupByName(conn, "default");
        if (pool) {
            virStorageVolPtr vol = virStorageVolLookupByPath(conn, disk_path);
            if (vol) {
                virStorageVolDelete(vol, 0);
                virStorageVolFree(vol);
            }
            virStoragePoolFree(pool);
        }
    }

    free(xml);
    virDomainFree(dom);
    virConnectClose(conn);

    snprintf(msg, sizeof(msg), "VM %s supprimée.", name);
    return msg;
}

/* ------------------------------------------------------------ */
/*              OUVRIR LA CONSOLE VNC / SPICE                   */
/* ------------------------------------------------------------ */
/* ------------------------------------------------------------ */
/*        CONSOLE VNC COMPATIBLE ANCIENNE VERSION LIBVIRT       */
/* ------------------------------------------------------------ */


const char* console_vm(const char *uri, const char *name) {
    static char msg[512];

    virConnectPtr conn = virConnectOpen(uri);
    if (!conn) {
        snprintf(msg, sizeof(msg),
                 "{\"error\":\"Impossible de se connecter à %s\"}", uri);
        return msg;
    }

    virDomainPtr dom = virDomainLookupByName(conn, name);
    if (!dom) {
        snprintf(msg, sizeof(msg),
                 "{\"error\":\"VM %s introuvable\"}", name);
        virConnectClose(conn);
        return msg;
    }

    /* Vérifier l'état */
    int state, reason;
    if (virDomainGetState(dom, &state, &reason, 0) < 0) {
        snprintf(msg, sizeof(msg),
                 "{\"error\":\"Impossible de lire l'état\"}");
        virDomainFree(dom);
        virConnectClose(conn);
        return msg;
    }

    /* Démarrer la VM si nécessaire */
    if (state != VIR_DOMAIN_RUNNING) {
        if (virDomainCreate(dom) < 0) {
            snprintf(msg, sizeof(msg),
                     "{\"error\":\"Impossible de démarrer %s\"}", name);
            virDomainFree(dom);
            virConnectClose(conn);
            return msg;
        }
    }

    /* ⚡ Lance virt-viewer dans un process fils */
    pid_t pid = fork();
    if (pid == 0) {
        execlp("virt-viewer", "virt-viewer", "--connect", uri, name, NULL);
        exit(1);  // si execlp échoue
    }

    snprintf(msg, sizeof(msg),
         "{\"message\":\"Console ouverte pour %s\", \"url\": \"\"}", name);

    virDomainFree(dom);
    virConnectClose(conn);
    return msg;
}


const char* clone_vm(const char *uri, const char *srcName, const char *dstName) {
    static char msg[512];

    virConnectPtr conn = virConnectOpen(uri);
    if (!conn) {
        snprintf(msg, sizeof(msg), "Erreur: connexion libvirt");
        return msg;
    }

    virDomainPtr srcDom = virDomainLookupByName(conn, srcName);
    if (!srcDom) {
        snprintf(msg, sizeof(msg), "Erreur: VM source introuvable");
        virConnectClose(conn);
        return msg;
    }

    /* Lire le XML complet de la VM source */
    char *xml = virDomainGetXMLDesc(srcDom, VIR_DOMAIN_XML_INACTIVE);
    if (!xml) {
        snprintf(msg, sizeof(msg), "Erreur: impossible de lire XML source");
        virDomainFree(srcDom);
        virConnectClose(conn);
        return msg;
    }

    /* Extraire chemin ancien disque */
    char oldDisk[512];
    char *p = strstr(xml, "<source file='");
    if (!p) {
        snprintf(msg, sizeof(msg), "Erreur: disque source introuvable dans XML");
        free(xml);
        virDomainFree(srcDom);
        virConnectClose(conn);
        return msg;
    }
    sscanf(p, "<source file='%511[^']'", oldDisk);

    /* Nouveau disque */
    char newDisk[512];
    snprintf(newDisk, sizeof(newDisk),
             "/var/lib/libvirt/images/%s.qcow2", dstName);

    /* Copier le disque proprement */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "qemu-img convert -O qcow2 '%s' '%s'",
             oldDisk, newDisk);
    system(cmd);

    /* Modifier le XML :
       - changer le nom
       - supprimer uuid
       - remplacer ancien disque par nouveau
    */

    char newXML[8192];
    strcpy(newXML, xml);

    /* changer <name> */
    char *nameStart = strstr(newXML, "<name>");
    if (nameStart) {
        char *nameEnd = strstr(nameStart, "</name>");
        if (nameEnd) {
            sprintf(nameStart, "<name>%s</name>%s",
                    dstName, nameEnd + 7);
        }
    }

    /* supprimer UUID */
    char *uuidStart = strstr(newXML, "<uuid>");
    if (uuidStart) {
        char *uuidEnd = strstr(uuidStart, "</uuid>");
        if (uuidEnd) {
            sprintf(uuidStart, "%s", uuidEnd + 7);
        }
    }

    /* remplacer disque */
    while ((p = strstr(newXML, oldDisk))) {
        memmove(p, newDisk, strlen(newDisk));
        memmove(p + strlen(newDisk),
                p + strlen(oldDisk),
                strlen(p + strlen(oldDisk)) + 1);
    }

    /* Définir la nouvelle VM */
    virDomainPtr newDom = virDomainDefineXML(conn, newXML);
    if (!newDom) {
        snprintf(msg, sizeof(msg), "Erreur: impossible de créer VM clone");
        free(xml);
        virDomainFree(srcDom);
        virConnectClose(conn);
        return msg;
    }

    /* Lancer la VM clonée */
    virDomainCreate(newDom);

    snprintf(msg, sizeof(msg),
             "Clone créé avec succès : %s → %s",
             srcName, dstName);

    free(xml);
    virDomainFree(newDom);
    virDomainFree(srcDom);
    virConnectClose(conn);

    return msg;
}

/* ===================================================================== */
/*                       MIGRATION D’UNE VM (UNSAFE)                     */
/* ===================================================================== */
char* migrate_vm(const char* src_uri, const char* name, const char* dest_uri)
{
    virConnectPtr src = NULL;
    virConnectPtr dest = NULL;
    virDomainPtr dom = NULL;
    virDomainPtr newDom = NULL;

    /* Connexion source */
    src = virConnectOpen(src_uri);
    if (!src) {
        const virError *e = virGetLastError();
        return strdup(e ? e->message : "Erreur : impossible de se connecter à la source");
    }

    /* Connexion destination */
    dest = virConnectOpen(dest_uri);
    if (!dest) {
        const virError *e = virGetLastError();
        virConnectClose(src);
        return strdup(e ? e->message : "Erreur : impossible de se connecter à la destination");
    }

    /* Recherche de la VM */
    dom = virDomainLookupByName(src, name);
    if (!dom) {
        const virError *e = virGetLastError();
        virConnectClose(dest);
        virConnectClose(src);
        return strdup(e ? e->message : "Erreur : VM introuvable");
    }

    /* Flags identiques à ton exercice */
    int flags = VIR_MIGRATE_LIVE |
                VIR_MIGRATE_UNSAFE |
                VIR_MIGRATE_UNDEFINE_SOURCE |
                VIR_MIGRATE_PERSIST_DEST;

    /* Migration CONNECT-TO-CONNECT */
    newDom = virDomainMigrate(dom, dest, flags, NULL, NULL, 0);

    if (!newDom) {
        const virError *e = virGetLastError();
        virDomainFree(dom);
        virConnectClose(dest);
        virConnectClose(src);
        return strdup(e ? e->message : "Erreur : migration impossible");
    }

    /* Succès */
    virDomainFree(newDom);
    virDomainFree(dom);
    virConnectClose(dest);
    virConnectClose(src);

    return strdup("Migration effectuée avec succès");
}

