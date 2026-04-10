# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

IvascuSearch (internally "Omnisearch") is a lightweight C metasearch engine that aggregates results from DuckDuckGo, Startpage, Yahoo, and Mojeek, with image search via Bing. It scrapes search engines using libcurl + libxml2 XPath, deduplicates results by URL, and renders server-side HTML. Similar to SearXNG but implemented as a native C application with no runtime dependencies on Python or Node.js.

## Build & Run Commands

```bash
make              # Build bin/omnisearch
make run          # Build and run the server
make clean        # Remove obj/ and bin/
make rebuild      # Clean + build
make info         # Print compiler flags, sources, and object files
```

**Runtime requirements:** The `beaker` C web framework library must be installed (`-lbeaker`), along with `libxml2`, `libcurl`, and `openssl`. The server reads `config.ini` from the working directory on startup (gitignored; copy from `example-config.ini`).

**Nix build:** `nix build` — flake.nix builds beaker from source then omnisearch.

No test suite exists.

## Architecture

### Request Flow

```
HTTP Request → Beaker Server → Route Handler → Parallel Scraping + Infobox Lookup → Template Render → HTML Response
```

### Routes (src/Routes/)

| Route | Handler | Description |
|---|---|---|
| `/` | `home_handler` | Home page with search box |
| `/opensearch.xml` | `handle_opensearch` | OpenSearch description for browser integration |
| `/search` | `results_handler` | Web search + infobox sidebar |
| `/images` | `images_handler` | Bing image search grid |
| `/proxy` | `image_proxy_handler` | Proxies images from whitelisted domains |

### Key Subsystems

- **Scraping (src/Scraping/):** Uses curl multi interface for parallel requests to all enabled engines. Each engine has an XPath parser in `ScrapingParsers.c` (registered in `ENGINE_REGISTRY`). Responses are classified (OK/blocked/parse error/fetch error) and cached. Retries with proxy rotation on failure.
- **Infobox (src/Infobox/):** Spawned in parallel threads on page 1 results. Handlers tried in order: Dictionary → Calculator → Unit Conversion → Currency → Wikipedia. First successful result is displayed.
- **Cache (src/Cache/):** Disk-based, MD5-keyed, two-level directory structure. Separate TTLs for search (1h default) and infobox (24h default).
- **Proxy (src/Proxy/):** Supports single proxy URL or list file. HTTP/SOCKS4/SOCKS5. Tracks failures and rotates. Can randomize credentials per request.
- **Config (src/Config.h/.c):** INI parser for `config.ini`. `global_config` is a global variable set at startup.

### Frontend

- **Templates (templates/):** Beaker template engine with `{{variable}}`, `{{for}}`, `{{if}}` syntax.
- **CSS (static/main.css):** Gruvbox dark theme, responsive.
- **WebGL background (static/dither.js):** Animated dithered wave canvas on home page.

### Engine Configuration

Engines are configured in `[engines]` section of config.ini. Supports `*` (all), comma-separated IDs (`ddg,startpage`), and exclusion with `-` prefix (`*,-startpage`). Available engines: `ddg`, `startpage`, `yahoo`, `mojeek`. All enabled by default.

## Code Conventions

- The project binary and internal identifiers still use "omnisearch" (e.g., `bin/omnisearch`, service user, install paths). The public-facing name is "IvascuSearch".
- C headers are co-located with their `.c` files in subdirectories under `src/`.
- Include paths use the subdirectory: `#include "Cache/Cache.h"`, `#include "Scraping/Scraping.h"`.
- Global state is used sparingly: `global_config` (Config), `proxy_url`/`proxy_count`/`max_proxy_retries` (Proxy), cache TTL globals.
- The Beaker framework provides: HTTP server, template rendering (`render_template`, `TemplateContext`, `context_set`, `context_set_array_of_arrays`), routing (`set_handler`), and response helpers (`serve_data`, `send_response`, `send_redirect`, `UrlParams`).

## Install Targets

`make install-systemd`, `install-openrc`, `install-runit`, `install-s6`, `install-dinit`, `install-launchd` (macOS only). Each copies templates/static/binary, creates an `omnisearch` service user, and installs the appropriate init definition. Uninstall with `make uninstall`.