# OmniSearch

A modern lightweight metasearch engine with a clean design written in C. [You can try using it here!](https://search.bwaaa.monster/)

## Configuration
Create a config.ini, there is an example included in the root.

## Dependencies
- libxml2
- libcurl (may be replaced in the future with curl-impersonate)
- beaker [(source)](https://git.bwaaa.monster/beaker/)

## Running
```bash
git clone https://git.bwaaa.monster/omnisearch/
cd omnisearch
make run
```

## Hosting
Run it normally behind a reverse proxy (like nginx)

## Contribution
Generate a patch with ```git format-patch HEAD~1``` and email to [gabriel@bwaaa.monster](mailto:gabriel@bwaaa.monster), beginning the subject line with [PATCH omnisearch] 

*If you are sending a revised version of a previous patch, please use [PATCH omnisearch v2, v3, etc].*
