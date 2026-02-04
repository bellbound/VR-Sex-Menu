
Sexlab specific stuff goes in 
src/sexlab:
- SexlabPapyrusAPI: Similar to the skse\matchmaker-vr\src\ostim\OstimPapyrusAPI.cpp. Lets us call the relevant API papyrus scripts from cpp.
- SexlabSceneStart manager: Equivalent to the ostim one
- SexlabSceneTracker: Takes in scene event cb's from papyrus & re-dispatches them in our code. This acts as an abstraction over the fact that we can only track specific Sexlab Scenes, and not all, as with ostim. Consumers need not know that they dont have all sexlab anims. Receives events from SexlabBridge
- SexlabPapyrusInterface: Similar 
- papyrus\mods\MatchmakerVR\Scripts\Source\MatchmakerVR_SexlabListener.psc
    - Quest Script, that registers for mod events for sl events and dispatches them to MatchmakerVR_SexlabBridge
    - For now we only track: By actor (player) & all scenes started through SexlabSceneStart
- papyrus\mods\MatchmakerVR\Scripts\Source\MatchmakerVR_SexlabBridge.psc
  - Native script, containing bound functions like NotifyAnimStart(tid, ...) ...
- skse\matchmaker-vr\src\config\ConfigOptions.h
    - New Section [Sexlab]
    - Enable Sexlab Integration: True by default (sexlab detection handled in papyrus later)
- SexlabSceneLoader: read SLAL JSON files to build an animation registry
    - Has an id, that we assign based on filename+index of the animation in the anims array
    - Json files located in SLAnims\json\{slalpackName}_{animpackCategory}.json (eg. Billyy_CreatureHumanoids, AnimationsByLeito_Aggressive)
    - skse\matchmaker-vr\docs\slal-schema.json contains the schema of the json files. 
    - Enumerate the files, like with the ostim equivalent and build a strongly typed registry. Store as a list of objects, 'select many' of slalPack.animations
        - Store animPack (slalPack.name Billyy_CreatureFurniture), animPackDisplayName (slalPack.name but _ replaced with space)
        - Values for actors.type: CreatureFemale, CreatureMale, Male, Female. 
        - animations.creature_race contains the race that at least 1 actor must have for the scene to be viable
- SexlabSceneFilter: Class that uses the loader's cached scenes & filters scenes for what is currently valid. (If any CreatureMale / CreatureFemale + creature_race is set, validate that at least one actor is a creature of that race).  `GetFilteredSexlabScenes(optional Actor[] actors, optional category id array 'enabled categories' (if none, then all are enabled, if any then only those are enabled))`
    - SexlabCreatureRaceMapper: Maps  creature_race <> skyrim race: Build the map at startup, after reading the SexlabScenes: Find all creature_race values, then match to any game data race by its fullname: First: Remove all spaces from the game race fullname, then, if the creature_race ends with an s, remove the 's' to de pluralize. Then match, ignoring case. If no match after full search & we removed an s, try again with the 's' at the end. If race name is "" or whitespace we dont consider it. Dont use regex for any of this.   
- ../CategoryRepository: Defines app-scope categories (string id, Displayname, Icon). Have a const string for each defined category. Have just 2 categories for now (Anal, Vaginal)
- SexlabCategoryFilter:  functions like IsInCategory(string categoryId, SexlabAnim), FilterByCategories(string array category ids). Internally Categoriezes based on Tag or custom logic
- SexlabIconResolver: Takes a Sexlab anim object, and returns a dds path, based on a set of rules regarding anim information (Tags, etc): 
- SexlabThreadMenu: A menu like  ThreadMenu, but for sexlab: has m_animationBrowserGrid, m_animationControlGrid, m_filterRow, m_controlRow
    - m_controlRow is the same as its in ostim.
    - m_filterRow uses a similar configuration as well, but has different buttons: One per Category in CategoryRepository. Can be scrolled. (70 units horizatal vsibable aread). Positioned below control row by the same amount control row is positioned below the grid. Clicking one toggles it on / off. Toggled on, the scale is increased to 1.5 
        - Minimize button gets replaced with a 'Switch to Animation browser' / 'Switch to scene controls' toggle. 
    - m_animationBrowserGrid: Shows all animations, filtered by the currently active filters & eligibility using SexlabSceneFilter
        Note: The browser grid must update, whenever a filter changes its toggle state. When no filters are selected, there is no filtering by category.
    - m_animationControlGrid: Can be shown in place of m_animationBrowserGrid (same grid config). Contains only these buttons: ../Interface/OStim/icons/Ostim/symbols/
        - x.dds, previous.dds, next.dds 
Your task: 
- Plan out this feature,  Lets flesh out the implementation (Overall architecture / function signatures only, no concrete code yet, no full class definitions)
then Orchestrate the implementation using multiple sub agents: Split the work into managable tasks, that you write to implementation-plan.md, then start agents to do the brunt work
    - For things like categories, icon mappings, create 2-3 entries for now, we will flesh them out later