from flask import Flask, jsonify, render_template, request
import ctypes
import os
import libvirt
import json
UPLOAD_FOLDER = "/var/lib/libvirt/images/"
os.makedirs(UPLOAD_FOLDER, exist_ok=True)

app = Flask(__name__)

lib = ctypes.CDLL("./libvirt_api.so")

# SIGNATURES C
lib.list_vms.argtypes = [ctypes.c_char_p]
lib.list_vms.restype = ctypes.c_char_p

lib.create_vm.argtypes = [
    ctypes.c_char_p,  # uri
    
    ctypes.c_char_p,  # name
    ctypes.c_char_p,  # ram
    ctypes.c_char_p,  # cpu
    ctypes.c_char_p,  # disk
    ctypes.c_char_p   # iso
]
lib.create_vm.restype = ctypes.c_char_p

lib.start_vm.argtypes   = [ctypes.c_char_p, ctypes.c_char_p]
lib.start_vm.restype    = ctypes.c_char_p
lib.stop_vm.argtypes    = [ctypes.c_char_p, ctypes.c_char_p]
lib.stop_vm.restype     = ctypes.c_char_p
lib.pause_vm.argtypes   = [ctypes.c_char_p, ctypes.c_char_p]
lib.pause_vm.restype    = ctypes.c_char_p
lib.resume_vm.argtypes  = [ctypes.c_char_p, ctypes.c_char_p]
lib.resume_vm.restype   = ctypes.c_char_p
lib.destroy_vm.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
lib.destroy_vm.restype  = ctypes.c_char_p

lib.console_vm.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
lib.console_vm.restype = ctypes.c_char_p

lib.clone_vm.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
lib.clone_vm.restype  = ctypes.c_char_p

lib.migrate_vm.argtypes = [
    ctypes.c_char_p,  # srcURI
    ctypes.c_char_p,  # vmName
    ctypes.c_char_p   # destURI
]
lib.migrate_vm.restype = ctypes.c_char_p

lib.restart_vm.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
lib.restart_vm.restype  = ctypes.c_char_p

lib.snapshot_vm.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
lib.snapshot_vm.restype  = ctypes.c_char_p

lib.list_snapshots.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
lib.list_snapshots.restype  = ctypes.c_char_p

lib.revert_snapshot.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
lib.revert_snapshot.restype  = ctypes.c_char_p

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/api/list_snapshots", methods=["POST"])
def api_list_snapshots():
    data = request.get_json()
    result = lib.list_snapshots(
        data["uri"].encode("utf-8"),
        data["name"].encode("utf-8")
    )
    return jsonify(json.loads(result.decode("utf-8")))

@app.route("/api/revert_snapshot", methods=["POST"])
def api_revert_snapshot():
    data = request.get_json()
    msg = lib.revert_snapshot(
        data["uri"].encode("utf-8"),
        data["name"].encode("utf-8"),
        data["snapshot"].encode("utf-8")
    )
    return jsonify({"message": msg.decode("utf-8")})

@app.route("/api/snapshot", methods=["POST"])
def api_snapshot():
    data = request.get_json()
    msg = lib.snapshot_vm(
        data["uri"].encode("utf-8"),
        data["name"].encode("utf-8"),
        data["snapshot"].encode("utf-8")
    )
    return jsonify({"message": msg.decode("utf-8")})

@app.route("/api/restart", methods=["POST"])
def api_restart():
    data = request.get_json()
    msg = lib.restart_vm(
        data["uri"].encode("utf-8"),
        data["name"].encode("utf-8")
    )
    return jsonify({"message": msg.decode("utf-8")})

@app.route("/api/vms")
def api_vms():
    uri = request.args.get("uri", "qemu:///system").encode("utf-8")
    result = lib.list_vms(uri)
    return jsonify(eval(result.decode("utf-8")))


@app.route("/api/create", methods=["POST"])
def api_create():
    data = request.get_json()

    uri  = data["uri"].encode("utf-8")
    name = data["name"].encode("utf-8")
    ram  = str(data["ram"]).encode("utf-8")
    cpu  = str(data["cpu"]).encode("utf-8")
    disk = str(data["disk"]).encode("utf-8")
    iso  = data["iso"].encode("utf-8")
    osinfo = data.get("osinfo", "linux2022").encode("utf-8")
    
    msg = lib.create_vm(uri, name, ram, cpu, disk, iso, osinfo)
    return jsonify({"message": msg.decode("utf-8")})


@app.route("/api/start", methods=["POST"])
def api_start():
    data = request.get_json()
    msg = lib.start_vm(
        data["uri"].encode("utf-8"),
        data["name"].encode("utf-8")
    )
    return jsonify({"message": msg.decode("utf-8")})


@app.route("/api/stop", methods=["POST"])
def api_stop():
    data = request.get_json()
    msg = lib.stop_vm(
        data["uri"].encode("utf-8"),
        data["name"].encode("utf-8")
    )
    return jsonify({"message": msg.decode("utf-8")})


@app.route("/api/destroy", methods=["POST"])
def api_destroy():
    data = request.get_json()
    msg = lib.destroy_vm(
        data["uri"].encode("utf-8"),
        data["name"].encode("utf-8")
    )
    return jsonify({"message": msg.decode("utf-8")})


@app.route("/api/pause", methods=["POST"])
def api_pause():
    data = request.get_json()
    msg = lib.pause_vm(
        data["uri"].encode("utf-8"),
        data["name"].encode("utf-8")
    )
    return jsonify({"message": msg.decode("utf-8")})


@app.route("/api/resume", methods=["POST"])
def api_resume():
    data = request.get_json()
    msg = lib.resume_vm(
        data["uri"].encode("utf-8"),
        data["name"].encode("utf-8")
    )
    return jsonify({"message": msg.decode("utf-8")})


@app.route("/api/iso")
def api_iso():
    files = [f for f in os.listdir(UPLOAD_FOLDER) if f.endswith(".iso")]
    return jsonify(files)

@app.route("/api/console", methods=["POST"])
def api_console():
    data = request.get_json()

    uri  = data["uri"].encode("utf-8")
    name = data["name"].encode("utf-8")

    res = lib.console_vm(uri, name).decode("utf-8")

    # res est un JSON dans une chaîne de caractères
    return jsonify(eval(res))


@app.route("/api/clone", methods=["POST"])
def api_clone():
    data = request.get_json()
    uri  = data["uri"].encode()
    src  = data["src"].encode()
    dst  = data["dst"].encode()

    res = lib.clone_vm(uri, src, dst).decode()
    return jsonify({"message": res})

@app.post("/api/migrate")
def api_migrate():
    data = request.get_json()

    src  = data.get("uri")
    name = data.get("name")
    dest = data.get("dest")

    # Vérification des valeurs
    if not src or not name or not dest:
        return jsonify({"error": "Champs requis : uri, name, dest"}), 400

    # Encodage UTF-8 pour l’appel C
    src_b  = src.encode("utf-8")
    name_b = name.encode("utf-8")
    dest_b = dest.encode("utf-8")

    res = lib.migrate_vm(src_b, name_b, dest_b).decode("utf-8")
    return jsonify({"message": res})


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8080, debug=True)
