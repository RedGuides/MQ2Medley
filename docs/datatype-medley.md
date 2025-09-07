---
tags:
  - datatype
---
# `Medley`

<!--dt-desc-start-->
Holds members that return current medley, information about the current queue, and other status about the songs being played
<!--dt-desc-end-->

## Members
<!--dt-members-start-->
### {{ renderMember(type='string', name='Medley') }}

:   Current medley name. Empty string if no current medley.

### {{ renderMember(type='double', name='TTQE') }}

:   (Time to queue empty) double time in seconds until queue is empty, this is estimate only. If performating normal medley, this will be 0.0

### {{ renderMember(type='int', name='Tune') }}

:   Deprecated when "A Tune Stuck in My Head" was changed to a passive AA, it's now always 0. It used to show 1 when buffed with A Tune Stuck in My Head.

### {{ renderMember(type='bool', name='Active') }}

:   true - medley is active

<!--dt-members-end-->

<!--dt-linkrefs-start-->
[bool]: ../macroquest/reference/data-types/datatype-bool.md
[double]: ../macroquest/reference/data-types/datatype-double.md
[int]: ../macroquest/reference/data-types/datatype-int.md
[string]: ../macroquest/reference/data-types/datatype-string.md
<!--dt-linkrefs-end-->
