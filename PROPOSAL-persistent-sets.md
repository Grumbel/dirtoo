# Proposal: Persistent ad-hoc file sets (“groups”)

Status: brainstorm / design notes only. No implementation yet.  
Related: existing **View → Group By** (Day / Directory / Session) is *transient
sectioning* of the current listing — different feature. Tags are *global
categories*. This is something in between.

---

## Intent (user words)

> “These few files belong together.”

Not a taxonomy, not a permanent label for the whole library, not “sort the
folder by date.” Just a sticky, ad-hoc bag of files the user can form, expand,
shrink, jump back to, and filter on — often while working across a few folders
or within one media-heavy directory.

Typical moments:

- “These five takes are the ones I’m considering for the final cut.”
- “Keep these reference images together while I browse the rest.”
- “This set of screenshots and the matching log files go with the bug report.”
- “Pin this handful so I don’t lose them in a 10k-file folder.”

---

## Naming

“Group” collides hard with **Group By**. Better names (pick later):

| Candidate | Feel | Notes |
|-----------|------|--------|
| **Set** | Neutral, accurate | “File sets”, `set:…` filter. Short. |
| **Collection** | Friendly | Slightly heavy; overlaps “file collection” in code. |
| **Bundle** | Ad-hoc | Good for “these belong together”; “bundle://” is fine. |
| **Pile** | Very informal | Memorable; maybe too cute. |
| **Clump** / **Cluster** | Visual | Cluster already used in Session grouping. |
| **Selection** (saved) | Direct | “Saved selection” is clear but long; Ctrl+G fits. |
| **Mark** / **Marked set** | Lightweight | Risk of confusion with “new” marks / badges. |
| **Album** | Media-centric | Strong for photos/video; weak for mixed files. |
| **Basket** / **Tray** | Temporary | Suggests ephemeral; we want persistence. |

Working title in this doc: **set** (user-facing “Set”, filter `set:`).  
UI strings can still say “Add to set”, “Show set”, etc. Final name is open.

---

## What it is *not*

- Not **Group By** (view arrangement).
- Not **tags** (reusable global categories like `holiday`, `raw`, `keep`).
- Not bookmarks / favorites of *folders*.
- Not a full playlist or project file format.
- Not requiring a name up front (anonymous by default).

---

## Relation to tags

Tags and sets share a lot: membership, filter language, virtual location,
badges, “show members”. Tempting to implement sets as **anonymous tags**
(or a tag namespace `set:` / special flag).

**Why it might work**

- One store, one badge pipeline, one completer, one `tag://`-style location.
- “Anonymous tag” + optional label ≈ set.
- Color could be a tag property.

**Why it might hurt**

- Tags are meant to be *named, reusable categories*. Sets are *ad-hoc bags*.
  Mixing them in the Tag Manager pollutes the category list with “Set 3”,
  “Set 7”, unlabeled entries.
- Mental model: “tag this as holiday” vs “these five belong together right now”.
- Overlap rules differ: many tags on one file is normal; many sets on one file
  is also normal, but the *primary* UI for sets is selection ↔ set, not
  vocabulary management.
- Filter vocabulary: `tag:foo` vs `set:…` — users should not have to care that
  they share a table.

**Suggestion:** same *storage technology* and identity rules (checksum /
path) if convenient, but **separate product concept and UI**. If the
implementation shares a table with a `kind` column, fine — the user should
still see Sets and Tags as different tools.

---

## Scope: one folder vs cross-directory

Two modes people actually want:

1. **Local sets** — “in this folder, these files stick together.”  
   Simple mental model; no surprise when browsing elsewhere.
2. **Cross-folder sets** — “these files from three different directories are
   one unit.”  
   Powerful for media work; needs a virtual place to *see* the whole set.

**Lean:** allow cross-directory membership from day one, but make the *default
creation* feel local (Ctrl+G on a selection in one view). Discoverability of
cross-folder sets comes from:

- a Sets list / manager,
- virtual location (`set://…`),
- filter `set:…` / `in-set:…` from anywhere.

If cross-folder feels too big for v1, ship local-only membership first and
open the door later; the UX for “show set” should still be designed as if the
set can outgrow the current folder.

---

## User journeys (brainstorm)

### A. Make a set from the current selection

1. Select files (click, shift, rubber-band, …).
2. **Ctrl+G** (or context menu **Add to set → New set**).
3. Selection becomes a new anonymous set. Subtle feedback: tint/badge appears,
   status line “Set created (5 files)”, optional toast.
4. Optional immediate prompt for a label (or rename later). Default: no dialog
   — friction kills ad-hoc use.

### B. Grow / shrink a set

- Select more files → **Add to set →** (submenu of existing sets) or
  **Ctrl+Shift+G** “add to last set”.
- Context menu **Remove from set** (submenu if file is in several).
- **Ungroup / Dissolve set** when the set is selected or from the manager.

### C. Set → selection (round trip)

- “Select members of this set” (context menu on a member, or from Sets list).
- Inverse of Ctrl+G: turn the set back into a multi-selection so the user can
  open, copy, tag, or delete the whole bag with existing commands.
- Keyboard candidate: **Ctrl+Shift+A** “select set under cursor” or a
  dedicated “Select set” when focus is on a member.

### D. See only the set

- **Show set** → main view lists exactly those members (virtual location
  `set://id` or `set://label`). Location chrome shows the set name / id.
- From there: normal open, copy, drag out, remove from set, etc.
- Breadcrumb / back returns to previous folder.

### E. Build a set across folders (DnD)

- Open set as virtual location (empty or partial).
- Drag files from other windows / split views / search results **into** the
  set view (or onto a set item in a sidebar list).
- Drop = add to membership. No file is moved on disk unless the user also
  uses normal move/copy.

### F. Filter

- Filter bar: `set:vacation-picks` or `set:g3` or `in-set:yes`.
- QuickFilter chip for “In a set” / specific sets (optional later).
- Combine with other predicates: `set:refs type:image`.

### G. Visual presence while browsing normally

- Members show a **color cue** (left edge, soft background, or small badge).
- Overlap: one primary color + tooltip listing all sets, or thin multi-stripe.
- Preference: “Show set colors” on/off so busy folders stay calm.

### H. Manage sets

- Lightweight **Sets** dialog or sidebar section: list (label or “Set #n”),
  count, color, last used.
- Actions: rename, recolor, show, delete/dissolve, select members.
- Deleted set only drops membership; files stay on disk.

---

## Keyboard & mouse (sketch)

| Action | Idea |
|--------|------|
| New set from selection | **Ctrl+G** |
| Add selection to last set | **Ctrl+Shift+G** |
| Select all members of set under cursor | TBD (e.g. Ctrl+Alt+A) |
| Context menu | Add to set →, Remove from set →, Show set, Select set |
| DnD onto set (sidebar or `set://` view) | Add membership |
| DnD from `set://` view to a real folder | Normal copy/move (existing transfer paths) |

Avoid stealing shortcuts already used for navigation / filter.

---

## Labels, anonymity, color

- **Anonymous by default.** “Set 1”, “Set 2” or a short id in UI until named.
- Optional label anytime (inline rename in manager or F2-style).
- Labels need not be unique; stable id is the real key.
- **Color** per set from a small palette (or free color). Auto-assign on
  create so anonymous sets are still distinguishable at a glance.

---

## Persistence expectations (user-level)

- Survives app restart.
- Survives rename of a file *if* we can still recognize it (user expectation
  often matches “tags still work after rename”).
- Does **not** move or copy files on disk by itself.
- Dissolving a set never deletes files.

Exact identity rules (hash vs path) are an implementation choice; the user
promise is “I put these together and they stay together unless I change the
set.”

---

## Overlap with other features (user view)

| Feature | Difference |
|---------|------------|
| Tags | Named categories; vocabulary; Tag Manager. Sets = bags. |
| Group By | How the *current list* is sectioned. Sets persist and can be shown alone. |
| Bookmarks | Places (folders), not file bags. |
| Search / `tag://` | Pattern for “show members” virtual views. |
| Selection | Ephemeral. Sets are the thing you turn a selection into and back. |

---

## Open questions (product)

1. **Name:** Set / Bundle / Collection / Saved selection / …?
2. **Default scope:** allow cross-directory from day one, or local-only first?
3. **Ctrl+G:** always new set, or “new if none selected / add to last if held”?
4. **Must a set have a color always visible**, or only when “Show set colors” is on?
5. **Anonymous sets in the filter bar:** only via id (`set:g3`) until labeled?
6. **Sidebar:** dedicated Sets section, or only manager dialog for v1?
7. **Multi-window:** sets are global to the app (like tags), right?
8. **Empty sets:** keep them until user deletes, or auto-prune?

---

## Suggested UX priority (when we implement)

1. Ctrl+G → new set from selection; color cue on members.
2. Context menu add/remove; set → selection.
3. “Show set” virtual view (`set://…`) + DnD into it.
4. Filter `set:` / `in-set:`.
5. Manager dialog (rename, recolor, delete, show).
6. Polish: last-set accelerator, QuickFilter chips, multi-color overlap.

---

## One-sentence pitch

**Sets are persistent, optionally named bags of files you form from a
selection (Ctrl+G), paint with a color, jump back to as a virtual folder, and
filter on — ad-hoc “these belong together,” not another tagging taxonomy.**
