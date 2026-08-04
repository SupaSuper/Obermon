# Obermon design system

Obermon uses the supplied Vivaldi build as an interaction reference, not as redistributable source.

## Layout principles

- Dense desktop-first browser chrome.
- Tabs and navigation controls remain visually subordinate to content.
- A left or right utility panel may host bookmarks, history, downloads, notes, and web panels.
- Tool surfaces use compact cards and clear active states instead of oversized mobile controls.

## Color tokens

- Void: `#070B12`
- Elevated void: `#0B101A`
- Surface: `#111824`
- Border: `rgba(255,255,255,.09)`
- Primary text: `#F5F7FB`
- Secondary text: `#99A4B5`
- Scramjet orange: `#FF8930`
- Orange highlight: `#FF9141`
- Success: `#48D597`
- Error: `#FF767F`

The built-in `obermon_theme` component applies the base frame, toolbar, tab, new-tab-page, and icon palette. Native Views/WebUI surfaces should consume these same semantic tokens rather than introducing unrelated colors.
