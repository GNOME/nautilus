# Install Nautilus

## On Ubuntu 18.04 LTS

### Install GNOME
```
$ sudo apt-get install gnome-session
```

### Install Meson
```
$ sudo apt-get install python3 python3-pip ninja-build
$ pip3 install --user meson
$ echo PATH=$HOME/.local/bin:$PATH >> .bashrc
$ source .bashrc
```

### Install development files
```
$ sudo apt-get install libgtk-3-dev libgail-3-dev libgexiv2-dev libgnome-autoar-0-dev libgnome-desktop-3-dev libtracker-sparql-2.0-dev libxml++2.6-dev libgirepository1.0-dev
```

### Install Git
```
$ sudo apt-get install git
```

### Compile and Install
```
$ cd ~/Downloads
$ git clone https://gitlab.gnome.org/GNOME/nautilus.git
$ cd nautilus/
$ meson build
$ cd build
$ ninja
$ sudo ninja install
```
