# OmniSearch

A modern lightweight metasearch engine with a clean design written in C.

## Configuration
Create a config.ini, there is an example included in the root. Or if you installed omnisearch, edit the config file at `/etc/omnisearch/config.ini`.

## Dependencies
- libxml2
- libcurl (may be replaced in the future with curl-impersonate)
- beaker [(source)](https://git.bwaaa.monster/beaker/)

## First Setup
Depending on your system, you may first need to install libcurl and libxml2.

### Arch Linux
```
# pacman -S libxml2 libcurl
```

### Debian/Ubuntu
```
# apt install libxml2-dev libcurl4-openssl-dev
```

### Fedora
```
# dnf install libxml2-devel libcurl-devel
```

### openSUSE
```
# zypper install libxml2-devel libcurl-devel
```

### Alpine
```
# apk add libxml2-dev curl-dev
```

### Void
```
# xbps-install -S libxml2-devel libcurl-devel
```

Install libbeaker:
```
$ git clone https://git.bwaaa.monster/beaker
$ cd beaker
$ make
# make install
```
And then install omnisearch:
```
$ git clone https://git.bwaaa.monster/omnisearch
$ cd omnisearch
$ make
# make install-<init>
```
Replace `<init>` with your init system (openrc,systemd,runit,s6)

## Hosting
Run it normally behind a reverse proxy (like nginx)

## Customisation
To make your own changes while still being able to receive upstream updates:

```
$ git checkout -b my-changes
```

Make your changes in the cloned folder, then periodically merge upstream:

```
$ git fetch origin
$ git merge origin/master
```

If there are conflicts in the files you modified, resolve them manually. You should also make the changes to the cloned repository, and then run the install command again if you installed omnisearch. Changes made directly to the configuration/assets folder will be overwritten on reinstall.

## Contribution
Generate a patch with ```git format-patch HEAD~1``` and email to [gabriel@bwaaa.monster](mailto:gabriel@bwaaa.monster), beginning the subject line with [PATCH omnisearch] 

*If you are send
ing a revised version of a previous patch, please use [PATCH omnisearch v2, v3, etc].*
