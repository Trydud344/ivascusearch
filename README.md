# OmniSearch

A modern lightweight metasearch engine with a clean design written in C.

## Configuration
Create a config.ini, there is an example included in the root.

## Dependencies
- libxml2
- libcurl (may be replaced in the future with curl-impersonate)
- beaker [(source)](https://git.bwaaa.monster/beaker/)

# First Setup
Firstly, install libbeaker:
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

## Contribution
Generate a patch with ```git format-patch HEAD~1``` and email to [gabriel@bwaaa.monster](mailto:gabriel@bwaaa.monster), beginning the subject line with [PATCH omnisearch] 

*If you are sending a revised version of a previous patch, please use [PATCH omnisearch v2, v3, etc].*
