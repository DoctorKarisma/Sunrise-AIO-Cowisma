# Sunrise AIO Cowisma


**Sunrise AIO Cowisma** is a community-maintained all-in-one fork of **Sunrise** for offline Destiny 2 exploration and experimentation.

Cowisma combines the current Sunrise codebase with useful features and improvements from **Sunrise-Cow**, **Sunrise-AIO-Karisma**, and other Sunrise community projects into a single build.

> [!IMPORTANT]
> Cowisma is an **unofficial community fork**. It is not maintained by the original Sunrise developers and is not affiliated with Bungie.

---

# Installation

> [!WARNING]
> Sunrise requires the compatible older Destiny 2 build.
> **Do not install this into the current live version of Destiny 2.**

## Quick Install

1. Set up a compatible Sunrise Destiny 2 installation using the upstream Sunrise installation instructions.

2. Download the latest **Cowisma** release from this repository's **Releases** page.

3. Open your Destiny 2 installation folder.

4. Go to:

   ```text
   bin\x64
   ```

5. Replace:

   ```text
   steam_api64.dll
   ```

   with the Cowisma `steam_api64.dll`.

6. Start the game using:

   ```text
   destiny2.exe
   ```

> [!TIP]
> If you are setting up Sunrise for the first time, follow the official Sunrise installation documentation first. Cowisma replaces the Sunrise DLL after the compatible game build has been installed.

Some features may also use additional files such as mission Lua scripts. When a Cowisma release requires extra files, they will be included with the release or explained in its release notes.

---

# What is Sunrise?

**Sunrise** is an open-source Destiny 2 offline preservation/exploration project created by **stanuwu**.

It allows a compatible older build of Destiny 2 to operate without the normal live Destiny services, making it possible to load destinations, explore game content, recreate missions, modify player state, and experiment with otherwise inaccessible parts of the game.

Sunrise itself is still under active development, so not every original Destiny 2 system or activity is fully implemented.

**Upstream project:**
https://github.com/stanuwu/Sunrise

---

# What is Cowisma?

Cowisma is the continuation of **Sunrise-AIO-Karisma**, now incorporating work from **Sunrise-Cow** and newer versions of upstream Sunrise.

The goal is simple:

> **Bring useful Sunrise community features together into one maintained build without requiring users to manually combine several different forks.**

Where possible, Cowisma follows the newer upstream Sunrise architecture while retaining useful functionality from community forks.

Cowisma is not intended to replace or take credit for those projects. It exists because many of the features people use were developed independently across several Sunrise forks.

---

# Features

Cowisma contains the normal Sunrise functionality plus additional community features and integrations.

## Exploration & Player Tools

* Fly
* Noclip
* Teleport
* Player coordinate/position display
* Player size controls
* Field-of-view controls
* World speed controls
* Infinite ammo
* Sword skate support
* Godmode
* No Turn Back suppression
* Activity/destination exploration tools

## Entity Spawner

Cowisma includes the **ReGlitched Entity Spawner**, adapted to work with the newer Sunrise architecture.

Features include:

* Spawn entities at the player
* Spawn entities at the crosshair
* Main entity browser
* Projectile browser
* Loot browser
* Entity name discovery
* Resident entity detection
* Spawn amount control
* Vertical lift
* Ray distance
* Entity scale
* Position offset
* Rotation override
* Camera-based rotation
* Player/crosshair spawn keybinds
* Multi-entity/grid spawning controls

> [!CAUTION]
> Not every game entity is necessarily safe to instantiate manually. Some entities depend on activity scripts, parent objects, replication state, or other game systems. Spawning an incompatible entity may crash the game.

## Gear & Inventory

Cowisma includes expanded inventory and equipment tooling, including community Gear Editor work.

Depending on the item and currently supported Sunrise systems, this includes tools for inspecting or modifying:

* Weapons
* Armor
* Equipment
* Sockets/perks
* Inventory state
* Subclass-related data
* Other investment data exposed by Sunrise

Not every item or socket configuration is guaranteed to function correctly.

## Tower Events

Cowisma includes additional support for restoring and experimenting with seasonal/event content in the Tower.

Supported event content includes work relating to:

* Festival of the Lost
* The Dawning
* Solstice
* Iron Banner
* Crimson Days
* Trials / Saint-14 related Tower content

Festival of the Lost also includes support for **native candy pickups and rewards**.

Event support can involve several different Destiny systems, including roster selection, vendors, investment data, music, and mission scripting.

Some event behavior may therefore require additional mission Lua scripts supplied separately with a release.

## UI & HUD

Cowisma also contains additional UI customization and HUD functionality, including:

* Player coordinates
* Session/activity information
* Sunrise status information
* UI transparency
* Selectable UI themes
* Animated RGB menu/HUD borders
* Original Sunrise theme option

---

# Where the Features Come From

Cowisma is built from work created across the Sunrise community.

| Project / Contributor                   | Major work used by Cowisma                                                                    |
| --------------------------------------- | --------------------------------------------------------------------------------------------- |
| **stanuwu / Sunrise**                   | Original Sunrise project and the core architecture Cowisma is built on                        |
| **PvtSeaCow / Sunrise-Cow**             | Sunrise-Cow functionality and improvements incorporated into Cowisma                          |
| **DoctorKarisma / Sunrise-AIO-Karisma** | Previous AIO project and Cowisma-specific integrations/UI work                                |
| **ReGlitched / Sunrise**                | Entity Spawner                                                                                |
| **Nyxaraa / Sunrise-Nyxara**            | Community Sunrise features and mission/emote-related work incorporated during AIO development |
| **WalterGerig / SunriseGearEditor**     | Gear Editor work                                                                              |
| **ltsReaver**                           | Godmode, No Turn Back, FOV, coordinate/fly and world-speed related community work             |
| **Sunrise contributors**                | Fixes and features that have since been incorporated directly into upstream Sunrise           |

### Source Projects

If you enjoy a feature in Cowisma that originated in another project, please support and credit the people who actually created it.

**Sunrise**
https://github.com/stanuwu/Sunrise

**Sunrise-Cow**
https://github.com/PvtSeaCow/Sunrise-Cow

**ReGlitched Sunrise**
https://github.com/ReGlitched/Sunrise

**Sunrise-Nyxara**
https://github.com/Nyxaraa/Sunrise-Nyxara

**SunriseGearEditor**
https://github.com/WalterGerig/SunriseGearEditor

**Sunrise-AIO-Karisma (superseded)**
https://github.com/DoctorKarisma/Sunrise-AIO-Karisma

> [!NOTE]
> Features have changed over time as Sunrise itself has evolved. Some older community implementations have been replaced with newer upstream systems while retaining the functionality they originally enabled.

---

# Updating from Sunrise-AIO-Karisma

**Sunrise-AIO-Karisma has been superseded by Sunrise-AIO-Cowisma.**

If you previously used AIO-Karisma, use Cowisma for current development and future releases.

Old repository:

https://github.com/DoctorKarisma/Sunrise-AIO-Karisma

Current repository:

https://github.com/DoctorKarisma/Sunrise-AIO-Cowisma

When updating, read the release notes first in case a release requires updated settings, scripts, or other files.

---

# Troubleshooting

Sunrise and Cowisma are both works in progress.

Destiny 2 was not designed to operate in this environment, so crashes, incomplete activities, missing systems, unusual behavior, and regressions are possible.

If Cowisma behaves strangely after updating, first make sure you are using the files intended for that release.

When reporting a reproducible problem, useful information includes:

* Destination/activity
* What you were doing when the problem occurred
* Whether it happens every time
* Whether it also happens on upstream Sunrise
* Relevant Sunrise logs
* Screenshots/video where useful
* Exact steps needed to reproduce it

For crashes involving the Entity Spawner, also include the **entity name/hash you attempted to spawn** if possible.

---

# Work in Progress

Neither Sunrise nor Cowisma recreates every original Destiny 2 service or gameplay system.

Some destinations and activities work much better than others. Some content requires mission scripting or additional server-side implementation, and some original online functionality may not currently be possible.

Features may also change as upstream Sunrise development continues.

---

# Building

Cowisma is written primarily in C++ and uses the same general build environment as upstream Sunrise.

For Windows development, open:

```text
Sunrise.sln
```

and build:

```text
Release | x64
```

The resulting DLL is placed under:

```text
build\x64\Release\steam_api64.dll
```

See the upstream Sunrise repository for current compiler, Visual Studio, Windows SDK, and other development requirements.

---

# Third-Party Libraries

Sunrise/Cowisma also relies on open-source third-party projects, including:

* **Dear ImGui** — https://github.com/ocornut/imgui
* **Microsoft Detours** — https://github.com/microsoft/Detours
* **Lua** — https://www.lua.org/
* **SQLite** — https://www.sqlite.org/

Please respect the licenses of Sunrise and all included third-party projects.

---

# Disclaimer

This is an unofficial community project intended for **offline/private-server experimentation, preservation, research, and development**.

Cowisma is not affiliated with, maintained by, sponsored by, or endorsed by Bungie.

**Destiny**, **Destiny 2**, **Bungie**, and related names, trademarks, artwork, and game assets belong to their respective owners.
