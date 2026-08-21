Next version

Controls:
- The configurable open hotkey is gone from the MCM, along with the setting that
  went with it for whether to block the button's normal use. Grip + trigger is
  the one way in now, and it opens the menu for the nearest scene in range as
  well as for one you are in yourself
- "Max. range to OStim Scene" now governs how far grip + trigger reaches

The menu:
- Opening the menu with grip + trigger now puts it at the hand that pressed
  rather than at a fixed spot in front of you
- The previous and next stage buttons in the category view sit half again as far
  apart, so reaching for one of them cannot land between the two

The MCM and the Quick Start spell:
- The plugin file still carried the mod's old name. Its quest asked the game for
  scripts called MatchmakerVR_*, none of which have shipped since the rename, so
  the MCM never registered and the Quick Start spell - which the MCM is what
  hands you - was never reachable. Everything is named VRSexMenu_* now and the
  MCM appears
- The SEQ file was named after the plugin's old name and held a record count the
  format does not have. On a save made before installing the mod, the MCM quest
  never started. Both fixed

Scenes that would not start:
- OStim hands the plugin a C++ interface at load, and every scene start, scene
  change and scene end the plugin knows about arrives through it. Where OStim
  did not answer - some OStim VR builds do not - the plugin heard nothing all
  session, and quietly lost the ability to do most of its job: it could not tell
  that an actor was already busy, so it never stopped the running scene before
  starting the next one and OStim refused the new scene outright; it never
  learned a scene had ended, so nobody was redressed
- The handshake is now retried later in the load, and where it still fails the
  plugin listens to OStim's Papyrus events for the same three facts instead.
  Whichever route works, only one of them feeds the tracker
- Restarting a scene no longer hides the menu before it knows the restart
  worked. A failed restart used to leave you looking at nothing with no way back
- Stopping a running scene before starting the next one gives up after ten
  seconds rather than waiting forever for an end that is not coming

Creatures:
- Which creature an actor is, and what it has, now comes from OStim's own actor
  properties rather than being guessed from the race. The guess only ever
  produced a generic "creature" and no body parts at all, so a creature scene
  asking for so much as "penis" matched nothing and the menu came up with the
  bottom bar and an empty grid

Browse by category:
- A new button in the tool row switches the menu between the scene graph it has
  always shown and a flat browse over every animation you have installed
- A row of category buttons under the tool row picks the filter: vaginal, anal,
  creature, group, standing, lying down and the rest. Hovering one names it and
  counts what these actors can perform, e.g. "Vaginal (255)"
- Only categories with something these actors can actually do get a button, and
  only animations they fulfil are listed - a canine pairing no longer sees the
  human catalogue
- One entry per animation rather than one per stage, so picking a scene drops you
  at its beginning
- Back and forward buttons in a row under the category row step through the
  stages of the animation that is playing, and appear only when there is a stage
  to step to
- The view and the selected category are saved with your game and come back when
  you reload
- The browse grid is four rows tall and scrolls, the same height the menu has in
  the scene graph view, rather than stacking every row that fits
- The category row is no wider than the grid above it and scrolls sideways,
  instead of running off both sides of the menu
- Animations show the icon their pack advertises them with. Where a pack ships no
  icon, the category's own icon stands in, picked to match who is doing what to
  whom - female-on-male animations get the female-on-male art
- The ones with art of their own are listed first, so the run of identical
  category icons is what you scroll to rather than what you scroll past
- Four new categories: Fisting, Vaginal Fingering, Anal Fingering and Anilingus

What the browser lists:
- A category now describes the whole animation rather than only the pose it opens
  on. A thread that starts with fingering and ends in fisting shows up under
  Fisting, which is where picking it leads
- Animations that are part of OStim's own free-form navigation web - its base
  scenes, and packs that wire into them rather than shipping their own hub - are
  left out of the browser. Nothing in there is a beginning ("kneel down", "sit
  up", "turn around"), so listing it flat gave pages of near-identical entries.
  The scene graph view still reaches all of it, which is where it makes sense
- Threads whose packs label their stages with pose icons instead of "Next"
  (Fencing In The Dark, Night-blooming Violets, parts of Lovemaking Compendium)
  are now recognised as threads, so they collapse to one entry each instead of
  one per stage - and the new back/forward buttons walk them

Tool row:
- Two buttons for OStim VR's own settings, shown while you are in the scene
  yourself: switch between first and third person, and lock the height of your
  view to the body or leave it at your own. The same switches the OStim Wheel
  has, without leaving the menu
- The row is split evenly around the orb whichever buttons it is holding, so the
  orb stays in the middle of the menu instead of drifting off to one side
- Restart, shown when a scene ends, now sits to the left of the orb
- The minimize button is not shown while browsing by category, where minimizing
  would hide the very grid you came for
- Minimizing now actually hides the tool buttons. They used to stay on screen
  because the row put them straight back

Controller hotkeys:
- Hold grip and press a second button on the same hand to work the menu without
  reaching for it: A for the next stage, B for the previous one, the left stick
  to switch camera, the right stick to lock your height to the body, and the
  trigger to show or hide the menu
- Grip on its own still does whatever it always did. Only once the second button
  lands is the pair taken, and only where the button it stands for is on screen
  anyway - so a combo can never do something you could not have reached for
- Grip and trigger bring the menu up with it closed too, but only for a scene you
  are in yourself. Anywhere else the pair goes straight to the game
- Every button that has one now names its combo in its hover text, e.g.
  "Next Stage (Grip + A)"
- A hand resting on the menu keeps its trigger and grip: those are 3DUI's click
  and grab, and a press that lands on a button is not read as a combo
- "Menu hotkey combos" in the MCM turns the lot off, along with the hover text
  they add

Tool row icons:
- First/third person is now your own body - the female or male symbol, whichever
  the player is - lit up while you are looking out of it, instead of OStim's
  switch arrows
- Lock height to body uses the menu's own move icon, lit up while it is on
- The two stage steps are drawn a third larger than the rest, being the buttons
  you reach for most while a scene plays

Requires 3DUI 0.10.5.
