async function chargerVMs() {
  const uri = encodeURIComponent(document.getElementById("uri").value);
  const res = await fetch(`/api/vms?uri=${uri}`);
  const data = await res.json();

  const activeDiv = document.getElementById("active");
  const inactiveDiv = document.getElementById("inactive");
  activeDiv.innerHTML = "";
  inactiveDiv.innerHTML = "";

  if (data.error) {
    activeDiv.innerHTML = `<p style='color:red'>${data.error}</p>`;
    return;
  }

  (data.vms || []).forEach(vm => {
    const card = document.createElement("div");
    card.className = "vm-card";

    let badgeClass = "inactive";
    let actions = "";

    if (vm.state === "running") {
      badgeClass = "active";
      actions = `
        <button style="background:#1abc9c" onclick="migrerVM('${vm.name}')">Migrer</button>
        <button style="background:#8e44ad" onclick="ouvrirConsole('${vm.name}')">Console</button>
        <button style="background:#f1c40f" onclick="pauseVM('${vm.name}')">Pause</button>
        <button style="background:#e74c3c" onclick="arreterVM('${vm.name}')">Stop</button>
        <button style="background:#f39c12" onclick="restartVM('${vm.name}')">Restart</button>
        <button style="background:#2980b9" onclick="listSnapshots('${vm.name}')">Snapshots</button>
      `;
    } else if (vm.state === "paused") {
      badgeClass = "paused";
      actions = `
        <button style="background:#3498db" onclick="reprendreVM('${vm.name}')">Reprendre</button>
        <button style="background:#e74c3c" onclick="arreterVM('${vm.name}')">Stop</button>
      `;
    } else {
      actions = `
        <button style="background:#8e44ad" onclick="clonerVM('${vm.name}')">Clone</button>
        <button style="background:#2ecc71" onclick="demarrerVM('${vm.name}')">Démarrer</button>
        <button style="background:#e74c3c" onclick="detruireVM('${vm.name}')">Supprimer</button>
        <button style="background:#3498db" onclick="snapshotVM('${vm.name}')">Snapshot</button>
      `;
    }

    card.innerHTML = `
      <div class="badge ${badgeClass}">${vm.state}</div>
      <div class="vm-name">${vm.name}</div>
      <div class="actions">${actions}</div>
    `;

    if (vm.state === "running") activeDiv.appendChild(card);
    else inactiveDiv.appendChild(card);
  });
}
async function detruireVM(nameParam) {
  const uri = document.getElementById("uri").value;
  const name = nameParam || document.getElementById("vmDelete").value;
  const res = await fetch("/api/destroy", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ uri, name })
  });
  const data = await res.json();
  alert(data.message);
  chargerVMs();
}

async function demarrerVM(name) {
  const uri = document.getElementById("uri").value;
  const res = await fetch("/api/start", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ uri, name })
  });
  const data = await res.json();
  alert(data.message);
  chargerVMs();
}

async function arreterVM(name) {
  const uri = document.getElementById("uri").value;
  const res = await fetch("/api/stop", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ uri, name })
  });
  const data = await res.json();
  alert(data.message);
  chargerVMs();
}

async function pauseVM(name) {
  const uri = document.getElementById("uri").value;
  const res = await fetch("/api/pause", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ uri, name })
  });
  const data = await res.json();
  alert(data.message);
  chargerVMs();
}

async function reprendreVM(name) {
  const uri = document.getElementById("uri").value;
  const res = await fetch("/api/resume", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ uri, name })
  });
  const data = await res.json();
  alert(data.message);
  chargerVMs();
}

// Chargement des ISO
async function chargerISOs() {
  const res = await fetch("/api/iso");
  const files = await res.json();
  const select = document.getElementById("vmISO");
  select.innerHTML = "";

  files.forEach(fname => {
    const opt = document.createElement("option");
    opt.value = "/var/lib/libvirt/images/" + fname;
    opt.textContent = fname;
    select.appendChild(opt);
  });
}

async function submitCreateVM() {
  const uri  = document.getElementById("uri").value;

  const name = document.getElementById("vmNamePopup").value;
  const ram  = parseInt(document.getElementById("vmRAM").value);
  const cpu  = parseInt(document.getElementById("vmCPU").value);
  const disk = parseInt(document.getElementById("vmDisk").value);
  const iso  = document.getElementById("vmISO").value;
  const osinfo = document.getElementById("vmOS").value;

  if (!name) return alert("Veuillez entrer un nom !");

  const res = await fetch("/api/create", {
    method: "POST",
    headers: {"Content-Type": "application/json"},
    body: JSON.stringify({ uri, name, ram, cpu, disk, iso, osinfo  })
  });

  const data = await res.json();
  alert(data.message);

  closeCreateVM();
  chargerVMs();
}

function openCreateVM() {
  document.getElementById("createVMModal").style.display = "block";
}

function closeCreateVM() {
  document.getElementById("createVMModal").style.display = "none";
}

async function ouvrirConsole(name) {
    const uri = document.getElementById("uri").value;

    const res = await fetch("/api/console", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ uri, name })
    });

    const data = await res.json();

    if (data.error) {
    alert(data.error);
    } else {
        alert(data.message); // console lancée côté serveur
    }

}

async function clonerVM(srcName) {
    const dstName = prompt("Nom du clone :");
    if (!dstName) return;

    const uri = document.getElementById("uri").value;

    const res = await fetch("/api/clone", {
        method: "POST",
        headers: {"Content-Type":"application/json"},
        body: JSON.stringify({ uri, src: srcName, dst: dstName })
    });

    const data = await res.json();
    alert(data.message);
    chargerVMs();
}

async function migrerVM(name) {
    const dest = prompt("URI de destination ?");
    const uri  = document.getElementById("uri").value;

    const res = await fetch("/api/migrate", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
            uri: uri,
            name: name,
            dest: dest
        })
    });

    const data = await res.json();
    alert(data.message);
    chargerVMs();
}

async function restartVM(name) {
  const uri = document.getElementById("uri").value;

  const res = await fetch("/api/restart", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ uri, name })
  });

  const data = await res.json();
  alert(data.message || data.error);

  chargerVMs();
}

async function snapshotVM(name) {
    const uri = document.getElementById("uri").value;

    const snap = prompt("Nom du snapshot :");
    if (!snap) return;

    const res = await fetch("/api/snapshot", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ uri, name, snapshot: snap })
    });

    const data = await res.json();
    alert(data.message || data.error);

    chargerVMs();
}

async function listSnapshots(name) {
    const uri = document.getElementById("uri").value;
    const res = await fetch("/api/list_snapshots", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ uri, name })
    });
    const data = await res.json();
    if (data.error) return alert(data.error);

    const snap = prompt("Snapshots disponibles:\n" + data.snapshots.join("\n") + "\nNom du snapshot à restaurer :");
    if (snap) revertSnapshot(name, snap);
}

async function revertSnapshot(name, snapname) {
    const uri = document.getElementById("uri").value;
    const res = await fetch("/api/revert_snapshot", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ uri, name, snapshot: snapname })
    });
    const data = await res.json();
    alert(data.message);
    chargerVMs();
}

window.onload = function() {
  chargerVMs();
  chargerISOs();
};
