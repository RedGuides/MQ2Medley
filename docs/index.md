---
tags:
  - plugin
resource_link: "https://www.redguides.com/community/resources/mq2medley.94/"
support_link: "https://www.redguides.com/community/threads/mq2medley.33896/"
repository: "https://github.com/RedGuides/MQ2Medley"
config: "servername_charname.ini"
authors: "winnower, Dewey, plure"
tagline: "Bard song scheduler"
quick_start: "https://www.redguides.com/wiki/MQ2Medley#Quickstart_Example"
---

# MQ2Medley

<!--desc-start-->
This plugin grew out of winnower's frustration with [MQ2Twist](../mq2twist/index.md) and it being hard to change from twist to twist while still being efficient and not recasting the songs that already had signification duration left.
<!--desc-end-->

You can have both plugins loaded at the same time, but don't try to `/twist` and `/medley` at the same time.

### Features
  - Set conditions for each song in the medley. Only want to cast on named? Only want to do insult if mana > 10%? Only want to cast dots if attack is on? Only want to cast mana regen when not in combat?
  - Advanced queuing support. Can specific target of queued spells for mez or cure and plugin will switch back to existing target with plugin reflexes. Optional interrupt when queing song.
  - Adapt your song song set without missing a beat when under the effect of "A tune stuck in my head"
  - Priority scheduling. Did you just mez 3 mobs? Switch back to your most important spells automatically
  - Automatically switch to maintaining 7 songs when Tune is up
  - Switch from medley to medley while still remembering the duration of current songs. Named up? just do switch to your burn medley to introduce new songs to the mix, while knowing what songs are already up.


## Commands

<a href="cmd-medley/">
{% 
  include-markdown "projects/mq2medley/cmd-medley.md" 
  start="<!--cmd-syntax-start-->" 
  end="<!--cmd-syntax-end-->" 
%}
</a>
:    {% include-markdown "projects/mq2medley/cmd-medley.md" 
        start="<!--cmd-desc-start-->" 
        end="<!--cmd-desc-end-->" 
        trailing-newlines=false 
     %} {{ readMore('projects/mq2medley/cmd-medley.md') }}

## Settings

!!! info "INI Format"
    - **Multiple Medleys**: Define medleys in sections named `MQ2Medley-medleyname`
    - **Song Definitions**: Up to 20 songs can be defined (`song1`-`song20`)
    - **Song Format**: Each song has 3 parts separated by `^`:
        1. **Name**: Song, Item or AA name
        2. **Duration**: Expression for `${Math.Calc[part2]}` (expected buff duration)  
           *Example*: `${Medley.Tune}` increases duration when "A Tune Stuck in my Head" is active
        3. **Condition**: Expression for `${Math.Calc}` to determine when to cast

!!! info "Scheduling"
    - **Order**: Songs cast in priority order (song1 > song2 > ... > song20)
    - **Skipped Songs**: 
        - Unreadable songs (Crescendo, Items, AA, etc)
        - Songs with active duration remaining
    - **Recast Timing**: Typically begins casting when duration has <6 seconds remaining
    - **All Active Songs**: Casts the song that will expire soonest

## Quickstart Example

Add a section to your server_charactername.ini file like,

```ini
[MQ2Medley-melee]
song1=War March of Jocelyn^18 + (6*${Medley.Tune})^1
song2=Aria of Maetanrus Rk. II^13 + (6*${Medley.Tune})^1
song3=Blade of Vesagran^180^${Melee.Combat}
song4=Fjilnauk's Song of Suffering^18^1
song5=Arcane Melody^18 + (6*${Medley.Tune})^1
song6=Silisia's Lively Crescendo^45^1
song7=Nilsara's Chant of Flame^24^${Melee.Combat} && ${Medley.Tune}
```

Then type:

```bash
/plugin mq2medley
/medley melee
```

You are now singing songs

## See also

- [MQ2Twist](../mq2twist/index.md)

## Top-Level Objects

## [Medley](tlo-medley.md)
{% include-markdown "projects/mq2medley/tlo-medley.md" start="<!--tlo-desc-start-->" end="<!--tlo-desc-end-->" trailing-newlines=false %} {{ readMore('projects/mq2medley/tlo-medley.md') }}

<h2>Forms</h2>
{% include-markdown "projects/mq2medley/tlo-medley.md" start="<!--tlo-forms-start-->" end="<!--tlo-forms-end-->" %}
{% include-markdown "projects/mq2medley/tlo-medley.md" start="<!--tlo-linkrefs-start-->" end="<!--tlo-linkrefs-end-->" %}

## DataTypes

## [Medley](datatype-medley.md)
{% include-markdown "projects/mq2medley/datatype-medley.md" start="<!--dt-desc-start-->" end="<!--dt-desc-end-->" trailing-newlines=false %} {{ readMore('projects/mq2medley/datatype-medley.md') }}

<h2>Members</h2>
{% include-markdown "projects/mq2medley/datatype-medley.md" start="<!--dt-members-start-->" end="<!--dt-members-end-->" %}
{% include-markdown "projects/mq2medley/datatype-medley.md" start="<!--dt-linkrefs-start-->" end="<!--dt-linkrefs-end-->" %}
