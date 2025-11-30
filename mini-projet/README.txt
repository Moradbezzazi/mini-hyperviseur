Installation et Mise en Place de l’Environnement du mini-projet


- Mise à jour du système:
sudo apt update

- Installation de KVM, Libvirt et des outils de virtualisation:

sudo apt install -y \
  qemu-kvm libvirt-daemon-system libvirt-clients \
  virtinst virt-manager \
  gcc make python3 python3-pip



- Activer et vérifier le service Libvirt
sudo systemctl enable --now libvirtd
sudo systemctl status libvirtd


- Donner les permissions à l’utilisateur:

sudo usermod -aG libvirt $USER
sudo usermod -aG kvm $USER
newgrp libvirt


- Récupérer le projet sur github


- Installer les dépendances Libvirt pour compiler le module C
sudo apt install libvirt-dev
sudo apt install build-essential pkg-config libvirt-dev



- Installer Flask (version système adaptée Debian)
sudo apt install python3-flask



- Compiler la bibliothèque C en .so
gcc -shared -fPIC libvirt_api.c -o libvirt_api.so -lvirt

- Lancer l’application Flask
python3 app.py


L’interface web est alors accessible à l’adresse :

http://127.0.0.1:8080
