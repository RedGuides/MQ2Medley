---
tags:
  - command
---

# /medley

## Syntax

<!--cmd-syntax-start-->
```eqcommand
/medley [option] [setting] | [queue <song name> <id> [-interrupt]]
```
<!--cmd-syntax-end-->

## Description

<!--cmd-desc-start-->
Controls a queue for bard songs
<!--cmd-desc-end-->

## Options

`(no option)`
:   Resume the medley after using `/medley stop`.

`<name>`
:   Sing the given medley.

`queue <"song/item/aa"> [-targetid|<spawnid>] [-interrupt]`
:   Add songs to queue to cast once.  
    The `|` in this syntax is used as part of the command.  
    Example: `/medley queue "Slumber of Silisia" -targetid|${Me.XTarget[2].ID}`

`stop` / `end` / `off`
:   Stop singing.

`delay <#>`
:   10ths of a second, minimum of 0, default 3. How long after casting a spell to wait before casting the next spell.

`reload`
:   Reload the INI file.

`quiet`
:   Toggles songs listing for medley and queued songs.

`debug`
:   Toggles debug mode.

`clear`
:   Clears the Medley.

## Examples

Here are some common usage examples:

**Play a medley defined in [MQ2Medley-melee] section in the INI file:**
```bash
/medley melee
```

**Interrupt current song to cast an AA:**
```bash
/medley queue "Dirge of the Sleepwalker" -interrupt
```

**Cast a spell on a specific target after current song ends (then switch back to current target):**
```bash
/medley queue "Slumber of Silisia" -targetid|${Me.XTarget[2].ID}
```

**Queue an item to cast:**
```bash
/medley queue "Blade of Vesagran"
```

**Queue an AA to cast:**
```bash
/medley queue "Lesson of the Devoted"
```