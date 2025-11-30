#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import tkinter as tk
from tkinter import ttk, messagebox, simpledialog
from dataclasses import dataclass
from typing import Dict, List, Optional, Tuple

# ============================================================
# Backend abstrait (interface) — pour basculer entre Python-libvirt et ton backend C
# ============================================================

class HypervisorBackend:
    """Interface backend. Implémente ceci via ctypes si tu veux appeler tes .so C.
       Ici, on fournit une implémentation par défaut avec libvirt-python."""
    def connect(self, uri: str): ...
    def close(self, uri: str): ...
    def list_domains(self, uri: str) -> Tuple[List[dict], List[str]]: ...
    def host_info(self, uri: str) -> dict: ...
    def start_domain(self, uri: str, name: str) -> None: ...
    def stop_domain_force(self, uri: str, name: str) -> None: ...
    def suspend_domain(self, uri: str, name: str) -> None: ...
    def resume_domain(self, uri: str, name: str) -> None: ...
    def undefine_domain(self, uri: str, name: str) -> None: ...
    def migrate_domain(self, uri: str, name: str, dest_uri: str, live: bool = True, tunnelled: bool = True, persist: bool = True, undefine_source: bool = False) -> None: ...


# ============================================================
# Implémentation par défaut : libvirt (python-libvirt)
# ============================================================

try:
    import libvirt
except Exception:
    libvirt = None

class LibvirtPythonBackend(HypervisorBackend):
    def __init__(self):
        self._conns: Dict[str, "libvirt.virConnect"] = {}

    def connect(self, uri: str):
        if uri in self._conns:
            return
        if libvirt is None:
            raise RuntimeError("Le module python-libvirt n'est pas disponible.")
        conn = libvirt.open(uri)
        if conn is None:
            raise RuntimeError(f"Impossible d'ouvrir la connexion: {uri}")
        self._conns[uri] = conn

    def close(self, uri: str):
        conn = self._conns.get(uri)
        if conn:
            conn.close()
            del self._conns[uri]

    def _conn(self, uri: str) -> "libvirt.virConnect":
        c = self._conns.get(uri)
        if c is None:
            raise RuntimeError(f"Non connecté à {uri}")
        return c

    def host_info(self, uri: str) -> dict:
        c = self._conn(uri)
        host = c.getHostname()
        vcpus = c.getMaxVcpus(None)
        free_mem = c.getFreeMemory()  # bytes
        encrypted = c.isEncrypted()
        return {
            "hostname": host,
            "encrypted": encrypted,
            "max_vcpus": vcpus,
            "free_mem_kb": int(free_mem // 1024)
        }

    def list_domains(self, uri: str) -> Tuple[List[dict], List[str]]:
        c = self._conn(uri)
        active_ids = c.listDomainsID()
        active = []
        for i in active_ids:
            dom = c.lookupByID(i)
            info = dom.info()
            # info = (state, maxMem, memory, nrVirtCpu, cpuTime)
            active.append({
                "id": i,
                "name": dom.name(),
                "state": info[0],
                "maxMem": info[1],
                "memory": info[2],
                "nrVirtCpu": info[3],
                "cpuTime": info[4],
            })
        inactive = c.listDefinedDomains()  # list of names
        return active, inactive

    def start_domain(self, uri: str, name: str) -> None:
        c = self._conn(uri)
        dom = c.lookupByName(name)
        if dom.create() != 0:
            raise RuntimeError(f"Échec du démarrage de {name}")

    def stop_domain_force(self, uri: str, name: str) -> None:
        c = self._conn(uri)
        dom = c.lookupByName(name)
        # Arrêt brutal (équivaut à virDomainDestroy)
        if dom.destroy() != 0:
            raise RuntimeError(f"Échec de l'arrêt de {name}")

    def suspend_domain(self, uri: str, name: str) -> None:
        c = self._conn(uri)
        dom = c.lookupByName(name)
        if dom.suspend() != 0:
            raise RuntimeError(f"Échec de la suspension de {name}")

    def resume_domain(self, uri: str, name: str) -> None:
        c = self._conn(uri)
        dom = c.lookupByName(name)
        if dom.resume() != 0:
            raise RuntimeError(f"Échec de la reprise de {name}")

    def undefine_domain(self, uri: str, name: str) -> None:
        c = self._conn(uri)
        dom = c.lookupByName(name)
        # Undefine sans supprimer le disque (attention aux volumes)
        if dom.undefine() != 0:
            raise RuntimeError(f"Échec de la suppression (undefine) de {name}")

    def migrate_domain(self, uri: str, name: str, dest_uri: str, live: bool = True, tunnelled: bool = True, persist: bool = True, undefine_source: bool = False) -> None:
        c = self._conn(uri)
        dom = c.lookupByName(name)
        flags = 0
        if live:
            flags |= libvirt.VIR_MIGRATE_LIVE
        if persist:
            flags |= libvirt.VIR_MIGRATE_PERSIST_DEST
        if tunnelled:
            flags |= libvirt.VIR_MIGRATE_TUNNELLED
        if undefine_source:
            flags |= libvirt.VIR_MIGRATE_UNDEFINE_SOURCE
        # migrateToURI3 si dispo, sinon migrateToURI
        try:
            # newer API
            dom.migrateToURI3(dest_uri, None, flags)
        except Exception:
            # fallback
            if dom.migrateToURI(dest_uri, flags, None, 0) != 0:
                raise RuntimeError(f"Échec de la migration de {name} vers {dest_uri}")


# ============================================================
# (Option) Backend ctypes — squelette pour brancher tes .so C
# ============================================================

class CLibraryBackend(HypervisorBackend):
    """Exemple de squelette si tu veux appeler tes fonctions C via ctypes.
       A adapter selon les signatures que tu exposes dans ta lib .so.
       Laisse LibvirtPythonBackend si tu veux avancer maintenant."""
    def __init__(self, path_to_so: str):
        raise NotImplementedError("À implémenter si besoin: charger la lib avec ctypes.CDLL et mapper les signatures.")

# ============================================================
# Modèle de données Hyperviseur
# ============================================================

@dataclass
class Hypervisor:
    uri: str


# ============================================================
# GUI Tkinter
# ============================================================

class OrchestratorApp(tk.Tk):
    def __init__(self, backend: HypervisorBackend):
        super().__init__()
        self.title("Hypervisor Orchestrator — Tkinter")
        self.geometry("1100x650")
        self.backend = backend
        self.hypervisors: Dict[str, Hypervisor] = {}  # uri -> Hypervisor
        self.current_uri: Optional[str] = None

        self._build_ui()

    # ---------- UI Layout ----------
    def _build_ui(self):
        # Toolbar
        toolbar = ttk.Frame(self, padding=6)
        toolbar.pack(side=tk.TOP, fill=tk.X)

        ttk.Button(toolbar, text="Ajouter hyperviseur", command=self._add_hypervisor_dialog).pack(side=tk.LEFT, padx=4)
        ttk.Button(toolbar, text="Supprimer", command=self._remove_current_hypervisor).pack(side=tk.LEFT, padx=4)
        ttk.Button(toolbar, text="Rafraîchir", command=self._refresh_all).pack(side=tk.LEFT, padx=4)

        # Paned
        paned = ttk.Panedwindow(self, orient=tk.HORIZONTAL)
        paned.pack(fill=tk.BOTH, expand=True)

        # Left: hypervisors list
        left = ttk.Frame(paned, padding=6)
        ttk.Label(left, text="Hyperviseurs").pack(anchor=tk.W)
        self.hypervisor_list = tk.Listbox(left, height=10)
        self.hypervisor_list.pack(fill=tk.BOTH, expand=True)
        self.hypervisor_list.bind("<<ListboxSelect>>", self._on_select_hypervisor)
        paned.add(left, weight=1)

        # Center: VMs tree + actions
        center = ttk.Frame(paned, padding=6)
        ttk.Label(center, text="Machines virtuelles").pack(anchor=tk.W)
        columns = ("id", "name", "state", "vcpus", "mem_mb", "cpu_time")
        self.vm_tree = ttk.Treeview(center, columns=columns, show="headings", height=18)
        for col, title, width in [
            ("id", "ID", 50),
            ("name", "Nom", 200),
            ("state", "État", 90),
            ("vcpus", "vCPU", 60),
            ("mem_mb", "Mémoire (MB)", 110),
            ("cpu_time", "CPU time", 160),
        ]:
            self.vm_tree.heading(col, text=title)
            self.vm_tree.column(col, width=width, anchor=tk.W)
        self.vm_tree.pack(fill=tk.BOTH, expand=True, pady=(2, 6))

        actions = ttk.Frame(center)
        actions.pack(fill=tk.X)
        ttk.Button(actions, text="Start", command=self._start_vm).pack(side=tk.LEFT, padx=2)
        ttk.Button(actions, text="Stop (force)", command=self._stop_vm_force).pack(side=tk.LEFT, padx=2)
        ttk.Button(actions, text="Suspend", command=self._suspend_vm).pack(side=tk.LEFT, padx=2)
        ttk.Button(actions, text="Resume", command=self._resume_vm).pack(side=tk.LEFT, padx=2)
        ttk.Separator(actions, orient=tk.VERTICAL).pack(side=tk.LEFT, fill=tk.Y, padx=6)
        ttk.Button(actions, text="Undefine (supprimer)", command=self._undefine_vm).pack(side=tk.LEFT, padx=2)
        ttk.Button(actions, text="Migrer…", command=self._migrate_vm_dialog).pack(side=tk.LEFT, padx=2)
        ttk.Button(actions, text="Rafraîchir", command=self._refresh_vms_only).pack(side=tk.LEFT, padx=2)

        paned.add(center, weight=3)

        # Right: host info
        right = ttk.Frame(paned, padding=6)
        ttk.Label(right, text="Informations hôte").pack(anchor=tk.W)
        self.host_text = tk.Text(right, height=20, wrap="word")
        self.host_text.pack(fill=tk.BOTH, expand=True)
        paned.add(right, weight=2)

        # Status bar
        self.status_var = tk.StringVar(value="Prêt.")
        status = ttk.Label(self, textvariable=self.status_var, anchor=tk.W, relief=tk.SUNKEN)
        status.pack(side=tk.BOTTOM, fill=tk.X)

    # ---------- Hypervisors management ----------
    def _add_hypervisor_dialog(self):
        uri = simpledialog.askstring(
            "Ajouter un hyperviseur",
            "URI libvirt (ex: qemu:///system, qemu+ssh://user@172.19.3.225/system, qemu+ssh://user@host/session ):"
        )
        if not uri:
            return
        try:
            self.backend.connect(uri)
            self.hypervisors[uri] = Hypervisor(uri=uri)
            self.hypervisor_list.insert(tk.END, uri)
            self.status_var.set(f"Connecté à {uri}")
        except Exception as e:
            messagebox.showerror("Connexion échouée", str(e))

    def _remove_current_hypervisor(self):
        sel = self.hypervisor_list.curselection()
        if not sel:
            return
        idx = sel[0]
        uri = self.hypervisor_list.get(idx)
        try:
            self.backend.close(uri)
        except Exception:
            pass
        self.hypervisor_list.delete(idx)
        self.hypervisors.pop(uri, None)
        if self.current_uri == uri:
            self.current_uri = None
            self._clear_host()
            self._clear_vms()
        self.status_var.set(f"Déconnecté de {uri}")

    def _on_select_hypervisor(self, _event=None):
        sel = self.hypervisor_list.curselection()
        if not sel:
            return
        uri = self.hypervisor_list.get(sel[0])
        self.current_uri = uri
        self._refresh_all()

    # ---------- Refresh ----------
    def _refresh_all(self):
        if not self.current_uri:
            return
        try:
            info = self.backend.host_info(self.current_uri)
            self._display_host_info(info)
            self._refresh_vms_only()
        except Exception as e:
            messagebox.showerror("Erreur rafraîchissement", str(e))

    def _refresh_vms_only(self):
        if not self.current_uri:
            return
        try:
            active, inactive = self.backend.list_domains(self.current_uri)
            self._display_vms(active, inactive)
        except Exception as e:
            messagebox.showerror("Erreur liste VMs", str(e))

    # ---------- Display helpers ----------
    def _display_host_info(self, info: dict):
        self.host_text.delete("1.0", tk.END)
        # Style d’affichage proche du TD (Ex3)
        self.host_text.insert(tk.END, f"Hostname:{info.get('hostname')}\n")
        self.host_text.insert(tk.END, f"Connection is encrypted: {info.get('encrypted')}\n")
        self.host_text.insert(tk.END, f"Maximum support virtual CPUs: {info.get('max_vcpus')}\n")
        self.host_text.insert(tk.END, f"Memory size: {info.get('free_mem_kb')}kb\n")

    def _clear_host(self):
        self.host_text.delete("1.0", tk.END)

    def _display_vms(self, active: List[dict], inactive: List[str]):
        self._clear_vms()
        for a in active:
            self.vm_tree.insert("", tk.END, values=(
                a["id"],
                a["name"],
                a["state"],
                a["nrVirtCpu"],
                a["memory"] // 1024,     # KB -> MB
                a["cpuTime"],
            ))
        # On ajoute aussi les inactives (id vide, state "shut off")
        for name in inactive:
            self.vm_tree.insert("", tk.END, values=("", name, "shut off", "", "", ""))

    def _clear_vms(self):
        for i in self.vm_tree.get_children():
            self.vm_tree.delete(i)

    def _selected_vm_name(self) -> Optional[str]:
        sel = self.vm_tree.selection()
        if not sel:
            return None
        values = self.vm_tree.item(sel[0], "values")
        return values[1] if values and len(values) >= 2 else None

    # ---------- VM actions ----------
    def _start_vm(self):
        if not self.current_uri:
            return
        name = self._selected_vm_name()
        if not name:
            messagebox.showinfo("Action", "Sélectionne une VM d'abord.")
            return
        try:
            self.backend.start_domain(self.current_uri, name)
            self.status_var.set(f"VM {name} démarrée.")
            self._refresh_vms_only()
        except Exception as e:
            messagebox.showerror("Start VM", str(e))

    def _stop_vm_force(self):
        if not self.current_uri:
            return
        name = self._selected_vm_name()
        if not name:
            messagebox.showinfo("Action", "Sélectionne une VM d'abord.")
            return
        if not messagebox.askyesno("Confirmation", f"Arrêt forcé de {name} ?"):
            return
        try:
            self.backend.stop_domain_force(self.current_uri, name)
            self.status_var.set(f"VM {name} arrêtée (force).")
            self._refresh_vms_only()
        except Exception as e:
            messagebox.showerror("Stop VM", str(e))

    def _suspend_vm(self):
        if not self.current_uri:
            return
        name = self._selected_vm_name()
        if not name:
            messagebox.showinfo("Action", "Sélectionne une VM d'abord.")
            return
        try:
            self.backend.suspend_domain(self.current_uri, name)
            self.status_var.set(f"VM {name} suspendue.")
            self._refresh_vms_only()
        except Exception as e:
            messagebox.showerror("Suspend VM", str(e))

    def _resume_vm(self):
        if not self.current_uri:
            return
        name = self._selected_vm_name()
        if not name:
            messagebox.showinfo("Action", "Sélectionne une VM d'abord.")
            return
        try:
            self.backend.resume_domain(self.current_uri, name)
            self.status_var.set(f"VM {name} reprise.")
            self._refresh_vms_only()
        except Exception as e:
            messagebox.showerror("Resume VM", str(e))

    def _undefine_vm(self):
        if not self.current_uri:
            return
        name = self._selected_vm_name()
        if not name:
            messagebox.showinfo("Action", "Sélectionne une VM d'abord.")
            return
        if not messagebox.askyesno("Confirmation", f"Supprimer la définition de {name} (undefine) ?"):
            return
        try:
            self.backend.undefine_domain(self.current_uri, name)
            self.status_var.set(f"VM {name} supprimée (undefine).")
            self._refresh_vms_only()
        except Exception as e:
            messagebox.showerror("Undefine VM", str(e))

    def _migrate_vm_dialog(self):
        if not self.current_uri:
            return
        name = self._selected_vm_name()
        if not name:
            messagebox.showinfo("Action", "Sélectionne une VM d'abord.")
            return
        dest_uri = simpledialog.askstring("Migration", "Destination URI (ex: qemu+ssh://user@IP/system):")
        if not dest_uri:
            return
        try:
            # Live + tunnelled par défaut (compatible qemu+ssh)
            self.backend.migrate_domain(self.current_uri, name, dest_uri, live=True, tunnelled=True, persist=True, undefine_source=False)
            self.status_var.set(f"VM {name} migrée vers {dest_uri}.")
            self._refresh_vms_only()
        except Exception as e:
            messagebox.showerror("Migration", str(e))


# ============================================================
# Entrée
# ============================================================

def main():
    backend = LibvirtPythonBackend()  # ou CLibraryBackend("/chemin/vers/ta/lib.so") si tu mappes tes C
    app = OrchestratorApp(backend)
    app.mainloop()

if __name__ == "__main__":
    main()
