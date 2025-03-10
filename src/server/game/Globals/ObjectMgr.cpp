/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ObjectMgr.h"
#include "ArenaTeamMgr.h"
#include "AreaTriggerDataStore.h"
#include "AreaTriggerTemplate.h"
#include "AzeriteEmpoweredItem.h"
#include "AzeriteItem.h"
#include "Chat.h"
#include "Containers.h"
#include "CreatureAIFactory.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "DisableMgr.h"
#include "GameObject.h"
#include "GameObjectAIFactory.h"
#include "GameTables.h"
#include "GameTime.h"
#include "GridDefines.h"
#include "GossipDef.h"
#include "GroupMgr.h"
#include "GuildMgr.h"
#include "InstanceScript.h"
#include "Item.h"
#include "LFGMgr.h"
#include "Log.h"
#include "LootMgr.h"
#include "Mail.h"
#include "MapManager.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "ObjectDefines.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "PoolMgr.h"
#include "QueryPackets.h"
#include "QuestDef.h"
#include "Random.h"
#include "ReputationMgr.h"
#include "ScriptMgr.h"
#include "ScriptReloadMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "Timer.h"
#include "TransportMgr.h"
#include "Vehicle.h"
#include "VMapFactory.h"
#include "World.h"
#include <G3D/g3dmath.h>
#include <numeric>
#include <limits>

ScriptMapMap sSpellScripts;
ScriptMapMap sEventScripts;
ScriptMapMap sWaypointScripts;

std::string GetScriptsTableNameByType(ScriptsType type)
{
    std::string res = "";
    switch (type)
    {
        case SCRIPTS_SPELL:         res = "spell_scripts";      break;
        case SCRIPTS_EVENT:         res = "event_scripts";      break;
        case SCRIPTS_WAYPOINT:      res = "waypoint_scripts";   break;
        default: break;
    }
    return res;
}

ScriptMapMap* GetScriptsMapByType(ScriptsType type)
{
    ScriptMapMap* res = nullptr;
    switch (type)
    {
        case SCRIPTS_SPELL:         res = &sSpellScripts;       break;
        case SCRIPTS_EVENT:         res = &sEventScripts;       break;
        case SCRIPTS_WAYPOINT:      res = &sWaypointScripts;    break;
        default: break;
    }
    return res;
}

std::string GetScriptCommandName(ScriptCommands command)
{
    std::string res = "";
    switch (command)
    {
        case SCRIPT_COMMAND_TALK: res = "SCRIPT_COMMAND_TALK"; break;
        case SCRIPT_COMMAND_EMOTE: res = "SCRIPT_COMMAND_EMOTE"; break;
        case SCRIPT_COMMAND_FIELD_SET_DEPRECATED: res = "SCRIPT_COMMAND_FIELD_SET_DEPRECATED"; break;
        case SCRIPT_COMMAND_MOVE_TO: res = "SCRIPT_COMMAND_MOVE_TO"; break;
        case SCRIPT_COMMAND_FLAG_SET_DEPRECATED: res = "SCRIPT_COMMAND_FLAG_SET_DEPRECATED"; break;
        case SCRIPT_COMMAND_FLAG_REMOVE_DEPRECATED: res = "SCRIPT_COMMAND_FLAG_REMOVE_DEPRECATED"; break;
        case SCRIPT_COMMAND_TELEPORT_TO: res = "SCRIPT_COMMAND_TELEPORT_TO"; break;
        case SCRIPT_COMMAND_QUEST_EXPLORED: res = "SCRIPT_COMMAND_QUEST_EXPLORED"; break;
        case SCRIPT_COMMAND_KILL_CREDIT: res = "SCRIPT_COMMAND_KILL_CREDIT"; break;
        case SCRIPT_COMMAND_RESPAWN_GAMEOBJECT: res = "SCRIPT_COMMAND_RESPAWN_GAMEOBJECT"; break;
        case SCRIPT_COMMAND_TEMP_SUMMON_CREATURE: res = "SCRIPT_COMMAND_TEMP_SUMMON_CREATURE"; break;
        case SCRIPT_COMMAND_OPEN_DOOR: res = "SCRIPT_COMMAND_OPEN_DOOR"; break;
        case SCRIPT_COMMAND_CLOSE_DOOR: res = "SCRIPT_COMMAND_CLOSE_DOOR"; break;
        case SCRIPT_COMMAND_ACTIVATE_OBJECT: res = "SCRIPT_COMMAND_ACTIVATE_OBJECT"; break;
        case SCRIPT_COMMAND_REMOVE_AURA: res = "SCRIPT_COMMAND_REMOVE_AURA"; break;
        case SCRIPT_COMMAND_CAST_SPELL: res = "SCRIPT_COMMAND_CAST_SPELL"; break;
        case SCRIPT_COMMAND_PLAY_SOUND: res = "SCRIPT_COMMAND_PLAY_SOUND"; break;
        case SCRIPT_COMMAND_CREATE_ITEM: res = "SCRIPT_COMMAND_CREATE_ITEM"; break;
        case SCRIPT_COMMAND_DESPAWN_SELF: res = "SCRIPT_COMMAND_DESPAWN_SELF"; break;
        case SCRIPT_COMMAND_LOAD_PATH: res = "SCRIPT_COMMAND_LOAD_PATH"; break;
        case SCRIPT_COMMAND_CALLSCRIPT_TO_UNIT: res = "SCRIPT_COMMAND_CALLSCRIPT_TO_UNIT"; break;
        case SCRIPT_COMMAND_KILL: res = "SCRIPT_COMMAND_KILL"; break;
        // TrinityCore only
        case SCRIPT_COMMAND_ORIENTATION: res = "SCRIPT_COMMAND_ORIENTATION"; break;
        case SCRIPT_COMMAND_EQUIP: res = "SCRIPT_COMMAND_EQUIP"; break;
        case SCRIPT_COMMAND_MODEL: res = "SCRIPT_COMMAND_MODEL"; break;
        case SCRIPT_COMMAND_CLOSE_GOSSIP: res = "SCRIPT_COMMAND_CLOSE_GOSSIP"; break;
        case SCRIPT_COMMAND_PLAYMOVIE: res = "SCRIPT_COMMAND_PLAYMOVIE"; break;
        case SCRIPT_COMMAND_MOVEMENT: res = "SCRIPT_COMMAND_MOVEMENT"; break;
        case SCRIPT_COMMAND_PLAY_ANIMKIT: res = "SCRIPT_COMMAND_PLAY_ANIMKIT"; break;
        default:
        {
            char sz[32];
            sprintf(sz, "Unknown command: %d", command);
            res = sz;
            break;
        }
    }
    return res;
}

std::string ScriptInfo::GetDebugInfo() const
{
    char sz[256];
    sprintf(sz, "%s ('%s' script id: %u)", GetScriptCommandName(command).c_str(), GetScriptsTableNameByType(type).c_str(), id);
    return std::string(sz);
}

bool normalizePlayerName(std::string& name)
{
    if (name.empty())
        return false;

    wchar_t wstr_buf[MAX_INTERNAL_PLAYER_NAME+1];
    size_t wstr_len = MAX_INTERNAL_PLAYER_NAME;

    if (!Utf8toWStr(name, &wstr_buf[0], wstr_len))
        return false;

    wstr_buf[0] = wcharToUpper(wstr_buf[0]);
    for (size_t i = 1; i < wstr_len; ++i)
        wstr_buf[i] = wcharToLower(wstr_buf[i]);

    if (!WStrToUtf8(wstr_buf, wstr_len, name))
        return false;

    return true;
}

// Extracts player and realm names delimited by -
ExtendedPlayerName ExtractExtendedPlayerName(std::string const& name)
{
    size_t pos = name.find('-');
    if (pos != std::string::npos)
        return ExtendedPlayerName(name.substr(0, pos), name.substr(pos + 1));
    else
        return ExtendedPlayerName(name, "");
}

bool SpellClickInfo::IsFitToRequirements(Unit const* clicker, Unit const* clickee) const
{
    Player const* playerClicker = clicker->ToPlayer();
    if (!playerClicker)
        return true;

    Unit const* summoner = nullptr;
    // Check summoners for party
    if (clickee->IsSummon())
        summoner = clickee->ToTempSummon()->GetSummoner();
    if (!summoner)
        summoner = clickee;

    // This only applies to players
    switch (userType)
    {
        case SPELL_CLICK_USER_FRIEND:
            if (!playerClicker->IsFriendlyTo(summoner))
                return false;
            break;
        case SPELL_CLICK_USER_RAID:
            if (!playerClicker->IsInRaidWith(summoner))
                return false;
            break;
        case SPELL_CLICK_USER_PARTY:
            if (!playerClicker->IsInPartyWith(summoner))
                return false;
            break;
        default:
            break;
    }

    return true;
}

ObjectMgr::ObjectMgr():
    _auctionId(1),
    _equipmentSetGuid(1),
    _mailId(1),
    _hiPetNumber(1),
    _creatureSpawnId(1),
    _gameObjectSpawnId(1),
    _voidItemId(1),
    DBCLocaleIndex(LOCALE_enUS)
{
}

ObjectMgr* ObjectMgr::instance()
{
    static ObjectMgr instance;
    return &instance;
}

ObjectMgr::~ObjectMgr()
{
}

void ObjectMgr::AddLocaleString(std::string&& value, LocaleConstant localeConstant, std::vector<std::string>& data)
{
    if (!value.empty())
    {
        if (data.size() <= size_t(localeConstant))
            data.resize(localeConstant + 1);

        data[localeConstant] = std::move(value);
    }
}

void ObjectMgr::LoadCreatureLocales()
{
    uint32 oldMSTime = getMSTime();

    _creatureLocaleStore.clear(); // need for reload case

    //                                               0      1       2     3        4      5
    QueryResult result = WorldDatabase.Query("SELECT entry, locale, Name, NameAlt, Title, TitleAlt FROM creature_template_locale");
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();

        uint32 id               = fields[0].GetUInt32();
        std::string localeName  = fields[1].GetString();

        LocaleConstant locale = GetLocaleByName(localeName);
        if (!IsValidLocale(locale) || locale == LOCALE_enUS)
            continue;

        CreatureLocale& data = _creatureLocaleStore[id];
        AddLocaleString(fields[2].GetString(), locale, data.Name);
        AddLocaleString(fields[3].GetString(), locale, data.NameAlt);
        AddLocaleString(fields[4].GetString(), locale, data.Title);
        AddLocaleString(fields[5].GetString(), locale, data.TitleAlt);

    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u creature locale strings in %u ms", uint32(_creatureLocaleStore.size()), GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadGossipMenuItemsLocales()
{
    uint32 oldMSTime = getMSTime();

    _gossipMenuItemsLocaleStore.clear();                              // need for reload case

    //                                               0       1            2       3           4
    QueryResult result = WorldDatabase.Query("SELECT MenuId, OptionIndex, Locale, OptionText, BoxText FROM gossip_menu_option_locale");

    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();

        uint32 menuId           = fields[0].GetUInt32();
        uint32 optionIndex      = fields[1].GetUInt32();
        std::string localeName  = fields[2].GetString();

        LocaleConstant locale   = GetLocaleByName(localeName);
        if (!IsValidLocale(locale) || locale == LOCALE_enUS)
            continue;

        GossipMenuItemsLocale& data = _gossipMenuItemsLocaleStore[std::make_pair(menuId, optionIndex)];
        AddLocaleString(fields[3].GetString(), locale, data.OptionText);
        AddLocaleString(fields[4].GetString(), locale, data.BoxText);
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded " SZFMTD " gossip_menu_option locale strings in %u ms", _gossipMenuItemsLocaleStore.size(), GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadPointOfInterestLocales()
{
    uint32 oldMSTime = getMSTime();

    _pointOfInterestLocaleStore.clear(); // need for reload case

    //                                               0      1      2
    QueryResult result = WorldDatabase.Query("SELECT ID, locale, Name FROM points_of_interest_locale");
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();

        uint32 id               = fields[0].GetUInt32();
        std::string localeName  = fields[1].GetString();

        LocaleConstant locale = GetLocaleByName(localeName);
        if (!IsValidLocale(locale) || locale == LOCALE_enUS)
            continue;

        PointOfInterestLocale& data = _pointOfInterestLocaleStore[id];
        AddLocaleString(fields[2].GetString(), locale, data.Name);
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u points_of_interest locale strings in %u ms", uint32(_pointOfInterestLocaleStore.size()), GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadCreatureTemplates()
{
    uint32 oldMSTime = getMSTime();

    //                                               0      1                   2                   3                   4            5            6     7           8
    //                                       "SELECT entry, difficulty_entry_1, difficulty_entry_2, difficulty_entry_3, KillCredit1, KillCredit2, name, femaleName, subname, "
    //                                        9         10        11              12        13        14                      15                 16
    //                                       "TitleAlt, IconName, gossip_menu_id, minlevel, maxlevel, HealthScalingExpansion, RequiredExpansion, VignetteID, "
    //                                        17       18       19          20         21     22    23         24              25               26            27
    //                                       "faction, npcflag, speed_walk, speed_run, scale, `rank`, dmgschool, BaseAttackTime, RangeAttackTime, BaseVariance, RangeVariance, "
    //                                        28          29          30           31           32            33      34             35
    //                                       "unit_class, unit_flags, unit_flags2, unit_flags3, dynamicflags, family, trainer_class, type, "
    //                                        36          37           38      39              40        41           42           43           44           45           46
    //                                       "type_flags, type_flags2, lootid, pickpocketloot, skinloot, resistance1, resistance2, resistance3, resistance4, resistance5, resistance6, "
    //                                        47      48      49      50      51      52      53      54      55         56       57       58      59
    //                                       "spell1, spell2, spell3, spell4, spell5, spell6, spell7, spell8, VehicleId, mingold, maxgold, AIName, MovementType, "
    //                                        60          61        62          63          64           65              66                   67            68                 69             70              71
    //                                       "ctm.Ground, ctm.Swim, ctm.Flight, ctm.Rooted, HoverHeight, HealthModifier, HealthModifierExtra, ManaModifier, ManaModifierExtra, ArmorModifier, DamageModifier, ExperienceModifier, "
    //                                        72            73          74                    75           76                        77
    //                                       "RacialLeader, movementId, CreatureDifficultyID, WidgetSetID, WidgetSetUnitConditionID, RegenHealth, "
    //                                        78                    79                        80
    //                                       "mechanic_immune_mask, spell_school_immune_mask, flags_extra, "
    //                                        81
    //                                       "ScriptName FROM creature_template WHERE entry = ? OR 1 = ?");

    WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_CREATURE_TEMPLATE);
    stmt->setUInt32(0, 0);
    stmt->setUInt32(1, 1);

    PreparedQueryResult result = WorldDatabase.Query(stmt);
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 creature template definitions. DB table `creature_template` is empty.");
        return;
    }

    _creatureTemplateStore.reserve(result->GetRowCount());
    do
    {
        Field* fields = result->Fetch();
        LoadCreatureTemplate(fields);
    } while (result->NextRow());

    // We load the creature models after loading but before checking
    LoadCreatureTemplateModels();

    // Checking needs to be done after loading because of the difficulty self referencing
    for (auto const& ctPair : _creatureTemplateStore)
        CheckCreatureTemplate(&ctPair.second);

    TC_LOG_INFO("server.loading", ">> Loaded " SZFMTD " creature definitions in %u ms", _creatureTemplateStore.size(), GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadCreatureTemplate(Field* fields)
{
    uint32 entry = fields[0].GetUInt32();
    CreatureTemplate& creatureTemplate = _creatureTemplateStore[entry];

    creatureTemplate.Entry = entry;

    for (uint32 i = 0; i < MAX_CREATURE_DIFFICULTIES; ++i)
        creatureTemplate.DifficultyEntry[i] = fields[1 + i].GetUInt32();

    for (uint8 i = 0; i < MAX_KILL_CREDIT; ++i)
        creatureTemplate.KillCredit[i]      = fields[4 + i].GetUInt32();

    creatureTemplate.Name                   = fields[6].GetString();
    creatureTemplate.FemaleName             = fields[7].GetString();
    creatureTemplate.SubName                = fields[8].GetString();
    creatureTemplate.TitleAlt               = fields[9].GetString();
    creatureTemplate.IconName               = fields[10].GetString();
    creatureTemplate.GossipMenuId           = fields[11].GetUInt32();
    creatureTemplate.minlevel               = fields[12].GetInt16();
    creatureTemplate.maxlevel               = fields[13].GetInt16();
    creatureTemplate.HealthScalingExpansion = fields[14].GetInt32();
    creatureTemplate.RequiredExpansion      = fields[15].GetUInt32();
    creatureTemplate.VignetteID             = fields[16].GetUInt32();
    creatureTemplate.faction                = fields[17].GetUInt16();
    creatureTemplate.npcflag                = fields[18].GetUInt64();
    creatureTemplate.speed_walk             = fields[19].GetFloat();
    creatureTemplate.speed_run              = fields[20].GetFloat();
    creatureTemplate.scale                  = fields[21].GetFloat();
    creatureTemplate.rank                   = uint32(fields[22].GetUInt8());
    creatureTemplate.dmgschool              = uint32(fields[23].GetInt8());
    creatureTemplate.BaseAttackTime         = fields[24].GetUInt32();
    creatureTemplate.RangeAttackTime        = fields[25].GetUInt32();
    creatureTemplate.BaseVariance           = fields[26].GetFloat();
    creatureTemplate.RangeVariance          = fields[27].GetFloat();
    creatureTemplate.unit_class             = uint32(fields[28].GetUInt8());
    creatureTemplate.unit_flags             = fields[29].GetUInt32();
    creatureTemplate.unit_flags2            = fields[30].GetUInt32();
    creatureTemplate.unit_flags3            = fields[31].GetUInt32();
    creatureTemplate.dynamicflags           = fields[32].GetUInt32();
    creatureTemplate.family                 = CreatureFamily(fields[33].GetInt32());
    creatureTemplate.trainer_class          = uint32(fields[34].GetUInt8());
    creatureTemplate.type                   = uint32(fields[35].GetUInt8());
    creatureTemplate.type_flags             = fields[36].GetUInt32();
    creatureTemplate.type_flags2            = fields[37].GetUInt32();
    creatureTemplate.lootid                 = fields[38].GetUInt32();
    creatureTemplate.pickpocketLootId       = fields[39].GetUInt32();
    creatureTemplate.SkinLootId             = fields[40].GetUInt32();

    for (uint8 i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
        creatureTemplate.resistance[i]      = fields[41 + i - 1].GetInt16();

    for (uint8 i = 0; i < MAX_CREATURE_SPELLS; ++i)
        creatureTemplate.spells[i]          = fields[47 + i].GetUInt32();

    creatureTemplate.VehicleId              = fields[55].GetUInt32();
    creatureTemplate.mingold                = fields[56].GetUInt32();
    creatureTemplate.maxgold                = fields[57].GetUInt32();
    creatureTemplate.AIName                 = fields[58].GetString();
    creatureTemplate.MovementType           = uint32(fields[59].GetUInt8());
    if (!fields[60].IsNull())
        creatureTemplate.Movement.Ground = static_cast<CreatureGroundMovementType>(fields[60].GetUInt8());

    if (!fields[61].IsNull())
        creatureTemplate.Movement.Swim = fields[61].GetBool();

    if (!fields[62].IsNull())
        creatureTemplate.Movement.Flight = static_cast<CreatureFlightMovementType>(fields[62].GetUInt8());

    if (!fields[63].IsNull())
        creatureTemplate.Movement.Rooted = fields[63].GetBool();

    creatureTemplate.HoverHeight            = fields[64].GetFloat();
    creatureTemplate.ModHealth              = fields[65].GetFloat();
    creatureTemplate.ModHealthExtra         = fields[66].GetFloat();
    creatureTemplate.ModMana                = fields[67].GetFloat();
    creatureTemplate.ModManaExtra           = fields[68].GetFloat();
    creatureTemplate.ModArmor               = fields[69].GetFloat();
    creatureTemplate.ModDamage              = fields[70].GetFloat();
    creatureTemplate.ModExperience          = fields[71].GetFloat();
    creatureTemplate.RacialLeader           = fields[72].GetBool();
    creatureTemplate.movementId             = fields[73].GetUInt32();
    creatureTemplate.CreatureDifficultyID   = fields[74].GetInt32();
    creatureTemplate.WidgetSetID            = fields[75].GetInt32();
    creatureTemplate.WidgetSetUnitConditionID = fields[76].GetInt32();
    creatureTemplate.RegenHealth            = fields[77].GetBool();
    creatureTemplate.MechanicImmuneMask     = fields[78].GetUInt32();
    creatureTemplate.SpellSchoolImmuneMask  = fields[79].GetUInt32();
    creatureTemplate.flags_extra            = fields[80].GetUInt32();
    creatureTemplate.ScriptID               = GetScriptId(fields[81].GetString());
}

void ObjectMgr::LoadCreatureTemplateModels()
{
    uint32 oldMSTime = getMSTime();

    //                                               0           1                  2             3
    QueryResult result = WorldDatabase.Query("SELECT CreatureID, CreatureDisplayID, DisplayScale, Probability FROM creature_template_model ORDER BY Idx ASC");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 creature template model definitions. DB table `creature_template_model` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 creatureId = fields[0].GetUInt32();
        uint32 creatureDisplayId = fields[1].GetUInt32();
        float displayScale = fields[2].GetFloat();
        float probability = fields[3].GetFloat();

        CreatureTemplate const* cInfo = GetCreatureTemplate(creatureId);
        if (!cInfo)
        {
            TC_LOG_ERROR("sql.sql", "Creature template (Entry: %u) does not exist but has a record in `creature_template_model`", creatureId);
            continue;
        }

        CreatureDisplayInfoEntry const* displayEntry = sCreatureDisplayInfoStore.LookupEntry(creatureDisplayId);
        if (!displayEntry)
        {
            TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) lists non-existing CreatureDisplayID id (%u), this can crash the client.", creatureId, creatureDisplayId);
            continue;
        }

        CreatureModelInfo const* modelInfo = GetCreatureModelInfo(creatureDisplayId);
        if (!modelInfo)
            TC_LOG_ERROR("sql.sql", "No model data exist for `CreatureDisplayID` = %u listed by creature (Entry: %u).", creatureDisplayId, creatureId);

        if (displayScale <= 0.0f)
            displayScale = 1.0f;

        const_cast<CreatureTemplate*>(cInfo)->Models.emplace_back(creatureDisplayId, displayScale, probability);

        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u creature template models in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadCreatureTemplateAddons()
{
    uint32 oldMSTime = getMSTime();

    //                                                 0       1       2      3       4       5        6             7              8                  9              10
    QueryResult result = WorldDatabase.Query("SELECT entry, path_id, mount, bytes1, bytes2, emote, aiAnimKit, movementAnimKit, meleeAnimKit, visibilityDistanceType, auras FROM creature_template_addon");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 creature template addon definitions. DB table `creature_template_addon` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 entry = fields[0].GetUInt32();

        if (!sObjectMgr->GetCreatureTemplate(entry))
        {
            TC_LOG_ERROR("sql.sql", "Creature template (Entry: %u) does not exist but has a record in `creature_template_addon`", entry);
            continue;
        }

        CreatureAddon& creatureAddon = _creatureTemplateAddonStore[entry];

        creatureAddon.path_id                   = fields[1].GetUInt32();
        creatureAddon.mount                     = fields[2].GetUInt32();
        creatureAddon.bytes1                    = fields[3].GetUInt32();
        creatureAddon.bytes2                    = fields[4].GetUInt32();
        creatureAddon.emote                     = fields[5].GetUInt32();
        creatureAddon.aiAnimKit                 = fields[6].GetUInt16();
        creatureAddon.movementAnimKit           = fields[7].GetUInt16();
        creatureAddon.meleeAnimKit              = fields[8].GetUInt16();
        creatureAddon.visibilityDistanceType    = VisibilityDistanceType(fields[9].GetUInt8());

        Tokenizer tokens(fields[10].GetString(), ' ');
        uint8 i = 0;
        creatureAddon.auras.resize(tokens.size());
        for (Tokenizer::const_iterator itr = tokens.begin(); itr != tokens.end(); ++itr)
        {
            uint32 spellId = uint32(atoul(*itr));
            SpellInfo const* AdditionalSpellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
            if (!AdditionalSpellInfo)
            {
                TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) has wrong spell %u defined in `auras` field in `creature_template_addon`.", entry, spellId);
                continue;
            }

            if (AdditionalSpellInfo->HasAura(SPELL_AURA_CONTROL_VEHICLE))
                TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) has SPELL_AURA_CONTROL_VEHICLE aura %u defined in `auras` field in `creature_template_addon`.", entry, spellId);

            if (std::find(creatureAddon.auras.begin(), creatureAddon.auras.end(), spellId) != creatureAddon.auras.end())
            {
                TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) has duplicate aura (spell %u) in `auras` field in `creature_template_addon`.", entry, spellId);
                continue;
            }

            creatureAddon.auras[i++] = spellId;
        }

        if (creatureAddon.mount)
        {
            if (!sCreatureDisplayInfoStore.LookupEntry(creatureAddon.mount))
            {
                TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) has invalid displayInfoId (%u) for mount defined in `creature_template_addon`", entry, creatureAddon.mount);
                creatureAddon.mount = 0;
            }
        }

        if (!sEmotesStore.LookupEntry(creatureAddon.emote))
        {
            TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) has invalid emote (%u) defined in `creature_template_addon`.", entry, creatureAddon.emote);
            creatureAddon.emote = 0;
        }

        if (creatureAddon.aiAnimKit && !sAnimKitStore.LookupEntry(creatureAddon.aiAnimKit))
        {
            TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) has invalid aiAnimKit (%u) defined in `creature_template_addon`.", entry, creatureAddon.aiAnimKit);
            creatureAddon.aiAnimKit = 0;
        }

        if (creatureAddon.movementAnimKit && !sAnimKitStore.LookupEntry(creatureAddon.movementAnimKit))
        {
            TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) has invalid movementAnimKit (%u) defined in `creature_template_addon`.", entry, creatureAddon.movementAnimKit);
            creatureAddon.movementAnimKit = 0;
        }

        if (creatureAddon.meleeAnimKit && !sAnimKitStore.LookupEntry(creatureAddon.meleeAnimKit))
        {
            TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) has invalid meleeAnimKit (%u) defined in `creature_template_addon`.", entry, creatureAddon.meleeAnimKit);
            creatureAddon.meleeAnimKit = 0;
        }

        if (creatureAddon.visibilityDistanceType >= VisibilityDistanceType::Max)
        {
            TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) has invalid visibilityDistanceType (%u) defined in `creature_template_addon`.",
                entry, AsUnderlyingType(creatureAddon.visibilityDistanceType));
            creatureAddon.visibilityDistanceType = VisibilityDistanceType::Normal;
        }

        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u creature template addons in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadCreatureScalingData()
{
    uint32 oldMSTime = getMSTime();

    //                                                 0          1                 2                     3                  4
    QueryResult result = WorldDatabase.Query("SELECT Entry, DifficultyID, LevelScalingDeltaMin, LevelScalingDeltaMax, ContentTuningID FROM creature_template_scaling ORDER BY Entry");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 creature template scaling definitions. DB table `creature_template_scaling` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 entry = fields[0].GetUInt32();
        Difficulty difficulty = Difficulty(fields[1].GetUInt8());

        auto itr = _creatureTemplateStore.find(entry);
        if (itr == _creatureTemplateStore.end())
        {
            TC_LOG_ERROR("sql.sql", "Creature template (Entry: %u) does not exist but has a record in `creature_template_scaling`", entry);
            continue;
        }

        CreatureLevelScaling creatureLevelScaling;
        creatureLevelScaling.DeltaLevelMin         = fields[2].GetInt16();
        creatureLevelScaling.DeltaLevelMax         = fields[3].GetInt16();
        creatureLevelScaling.ContentTuningID       = fields[4].GetInt32();

        itr->second.scalingStore[difficulty] = creatureLevelScaling;

        ++count;
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u creature template scaling data in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::CheckCreatureTemplate(CreatureTemplate const* cInfo)
{
    if (!cInfo)
        return;

    bool ok = true;                                     // bool to allow continue outside this loop
    for (uint32 diff = 0; diff < MAX_CREATURE_DIFFICULTIES && ok; ++diff)
    {
        if (!cInfo->DifficultyEntry[diff])
            continue;
        ok = false;                                     // will be set to true at the end of this loop again

        CreatureTemplate const* difficultyInfo = GetCreatureTemplate(cInfo->DifficultyEntry[diff]);
        if (!difficultyInfo)
        {
            TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) has `difficulty_entry_%u`=%u but creature entry %u does not exist.",
                cInfo->Entry, diff + 1, cInfo->DifficultyEntry[diff], cInfo->DifficultyEntry[diff]);
            continue;
        }

        bool ok2 = true;
        for (uint32 diff2 = 0; diff2 < MAX_CREATURE_DIFFICULTIES && ok2; ++diff2)
        {
            ok2 = false;
            if (_difficultyEntries[diff2].find(cInfo->Entry) != _difficultyEntries[diff2].end())
            {
                TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) is listed as `difficulty_entry_%u` of another creature, but itself lists %u in `difficulty_entry_%u`.",
                    cInfo->Entry, diff2 + 1, cInfo->DifficultyEntry[diff], diff + 1);
                continue;
            }

            if (_difficultyEntries[diff2].find(cInfo->DifficultyEntry[diff]) != _difficultyEntries[diff2].end())
            {
                TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) already listed as `difficulty_entry_%u` for another entry.", cInfo->DifficultyEntry[diff], diff2 + 1);
                continue;
            }

            if (_hasDifficultyEntries[diff2].find(cInfo->DifficultyEntry[diff]) != _hasDifficultyEntries[diff2].end())
            {
                TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) has `difficulty_entry_%u`=%u but creature entry %u has itself a value in `difficulty_entry_%u`.",
                    cInfo->Entry, diff + 1, cInfo->DifficultyEntry[diff], cInfo->DifficultyEntry[diff], diff2 + 1);
                continue;
            }
            ok2 = true;
        }

        if (!ok2)
            continue;

        if (cInfo->HealthScalingExpansion > difficultyInfo->HealthScalingExpansion)
        {
            TC_LOG_ERROR("sql.sql", "Creature (ID: %u, Expansion: %u) has different `HealthScalingExpansion` in difficulty %u mode (ID: %u, Expansion: %u).",
                cInfo->Entry, cInfo->HealthScalingExpansion, diff + 1, cInfo->DifficultyEntry[diff], difficultyInfo->HealthScalingExpansion);
        }

        if (cInfo->minlevel > difficultyInfo->minlevel)
        {
            TC_LOG_ERROR("sql.sql", "Creature (Entry: %u, minlevel: %u) has lower `minlevel` in difficulty %u mode (Entry: %u, minlevel: %u).",
                cInfo->Entry, cInfo->minlevel, diff + 1, cInfo->DifficultyEntry[diff], difficultyInfo->minlevel);
        }

        if (cInfo->maxlevel > difficultyInfo->maxlevel)
        {
            TC_LOG_ERROR("sql.sql", "Creature (Entry: %u, maxlevel: %u) has lower `maxlevel` in difficulty %u mode (Entry: %u, maxlevel: %u).",
                cInfo->Entry, cInfo->maxlevel, diff + 1, cInfo->DifficultyEntry[diff], difficultyInfo->maxlevel);
        }

        if (cInfo->faction != difficultyInfo->faction)
        {
            TC_LOG_ERROR("sql.sql", "Creature (Entry: %u, faction: %u) has different `faction` in difficulty %u mode (Entry: %u, faction: %u).",
                cInfo->Entry, cInfo->faction, diff + 1, cInfo->DifficultyEntry[diff], difficultyInfo->faction);
            TC_LOG_ERROR("sql.sql", "Possible FIX: UPDATE `creature_template` SET `faction`=%u WHERE `entry`=%u;",
                cInfo->faction, cInfo->DifficultyEntry[diff]);
        }

        if (cInfo->unit_class != difficultyInfo->unit_class)
        {
            TC_LOG_ERROR("sql.sql", "Creature (Entry: %u, class: %u) has different `unit_class` in difficulty %u mode (Entry: %u, class: %u).",
                cInfo->Entry, cInfo->unit_class, diff + 1, cInfo->DifficultyEntry[diff], difficultyInfo->unit_class);
            TC_LOG_ERROR("sql.sql", "Possible FIX: UPDATE `creature_template` SET `unit_class`=%u WHERE `entry`=%u;",
                cInfo->unit_class, cInfo->DifficultyEntry[diff]);
            continue;
        }

        uint32 differenceMask = cInfo->npcflag ^ difficultyInfo->npcflag;
        if (cInfo->npcflag != difficultyInfo->npcflag)
        {
            TC_LOG_ERROR("sql.sql", "Creature (Entry: %u, `npcflag`: " UI64FMTD ") has different `npcflag` in difficulty %u mode (Entry: %u, `npcflag`: " UI64FMTD ").",
                cInfo->Entry, cInfo->npcflag, diff + 1, cInfo->DifficultyEntry[diff], difficultyInfo->npcflag);
            TC_LOG_ERROR("sql.sql", "Possible FIX: UPDATE `creature_template` SET `npcflag`=`npcflag`^%u WHERE `entry`=%u;",
                differenceMask, cInfo->DifficultyEntry[diff]);
            continue;
        }

        if (cInfo->dmgschool != difficultyInfo->dmgschool)
        {
            TC_LOG_ERROR("sql.sql", "Creature (Entry: %u, `dmgschool`: %u) has different `dmgschool` in difficulty %u mode (Entry: %u, `dmgschool`: %u).",
                cInfo->Entry, cInfo->dmgschool, diff + 1, cInfo->DifficultyEntry[diff], difficultyInfo->dmgschool);
            TC_LOG_ERROR("sql.sql", "Possible FIX: UPDATE `creature_template` SET `dmgschool`=%u WHERE `entry`=%u;",
                cInfo->dmgschool, cInfo->DifficultyEntry[diff]);
        }

        differenceMask = cInfo->unit_flags2 ^ difficultyInfo->unit_flags2;
        if (cInfo->unit_flags2 != difficultyInfo->unit_flags2)
        {
            TC_LOG_ERROR("sql.sql", "Creature (Entry: %u, `unit_flags2`: %u) has different `unit_flags2` in difficulty %u mode (Entry: %u, `unit_flags2`: %u).",
                cInfo->Entry, cInfo->unit_flags2, diff + 1, cInfo->DifficultyEntry[diff], difficultyInfo->unit_flags2);
            TC_LOG_ERROR("sql.sql", "Possible FIX: UPDATE `creature_template` SET `unit_flags2`=`unit_flags2`^%u WHERE `entry`=%u;",
                differenceMask, cInfo->DifficultyEntry[diff]);
        }

        if (cInfo->family != difficultyInfo->family)
        {
            TC_LOG_ERROR("sql.sql", "Creature (Entry: %u, family: %u) has different `family` in difficulty %u mode (Entry: %u, family: %u).",
                cInfo->Entry, cInfo->family, diff + 1, cInfo->DifficultyEntry[diff], difficultyInfo->family);
            TC_LOG_ERROR("sql.sql", "Possible FIX: UPDATE `creature_template` SET `family`=%u WHERE `entry`=%u;",
                cInfo->family, cInfo->DifficultyEntry[diff]);
        }

        if (cInfo->trainer_class != difficultyInfo->trainer_class)
        {
            TC_LOG_ERROR("sql.sql", "Creature (Entry: %u, trainer_class: %u) has different `trainer_class` in difficulty %u mode (Entry: %u, trainer_class: %u).",
                cInfo->Entry, cInfo->trainer_class, diff + 1, cInfo->DifficultyEntry[diff], difficultyInfo->trainer_class);
            TC_LOG_ERROR("sql.sql", "Possible FIX: UPDATE `creature_template` SET `trainer_class`=%u WHERE `entry`=%u;",
                cInfo->trainer_class, cInfo->DifficultyEntry[diff]);
            continue;
        }

        if (cInfo->type != difficultyInfo->type)
        {
            TC_LOG_ERROR("sql.sql", "Creature (Entry: %u, type: %u) has different `type` in difficulty %u mode (Entry: %u, type: %u).",
                cInfo->Entry, cInfo->type, diff + 1, cInfo->DifficultyEntry[diff], difficultyInfo->type);
            TC_LOG_ERROR("sql.sql", "Possible FIX: UPDATE `creature_template` SET `type`=%u WHERE `entry`=%u;",
                cInfo->type, cInfo->DifficultyEntry[diff]);
        }

        if (!cInfo->VehicleId && difficultyInfo->VehicleId)
        {
            TC_LOG_ERROR("sql.sql", "Non-vehicle Creature (Entry: %u, VehicleId: %u) has `VehicleId` set in difficulty %u mode (Entry: %u, VehicleId: %u).",
                cInfo->Entry, cInfo->VehicleId, diff + 1, cInfo->DifficultyEntry[diff], difficultyInfo->VehicleId);
        }

        if (cInfo->RegenHealth != difficultyInfo->RegenHealth)
        {
            TC_LOG_ERROR("sql.sql", "Creature (Entry: %u, RegenHealth: %u) has different `RegenHealth` in difficulty %u mode (Entry: %u, RegenHealth: %u).",
                cInfo->Entry, cInfo->RegenHealth, diff + 1, cInfo->DifficultyEntry[diff], difficultyInfo->RegenHealth);
            TC_LOG_ERROR("sql.sql", "Possible FIX: UPDATE `creature_template` SET `RegenHealth`=%u WHERE `entry`=%u;",
                cInfo->RegenHealth, cInfo->DifficultyEntry[diff]);
        }

        differenceMask = cInfo->MechanicImmuneMask & (~difficultyInfo->MechanicImmuneMask);
        if (differenceMask)
        {
            TC_LOG_ERROR("sql.sql", "Creature (Entry: %u, mechanic_immune_mask: %u) has weaker immunities in difficulty %u mode (Entry: %u, mechanic_immune_mask: %u).",
                cInfo->Entry, cInfo->MechanicImmuneMask, diff + 1, cInfo->DifficultyEntry[diff], difficultyInfo->MechanicImmuneMask);
            TC_LOG_ERROR("sql.sql", "Possible FIX: UPDATE `creature_template` SET `mechanic_immune_mask`=`mechanic_immune_mask`|%u WHERE `entry`=%u;",
                differenceMask, cInfo->DifficultyEntry[diff]);
        }

        differenceMask = (cInfo->flags_extra ^ difficultyInfo->flags_extra) & (~CREATURE_FLAG_EXTRA_INSTANCE_BIND);
        if (differenceMask)
        {
            TC_LOG_ERROR("sql.sql", "Creature (Entry: %u, flags_extra: %u) has different `flags_extra` in difficulty %u mode (Entry: %u, flags_extra: %u).",
                cInfo->Entry, cInfo->flags_extra, diff + 1, cInfo->DifficultyEntry[diff], difficultyInfo->flags_extra);
            TC_LOG_ERROR("sql.sql", "Possible FIX: UPDATE `creature_template` SET `flags_extra`=`flags_extra`^%u WHERE `entry`=%u;",
                differenceMask, cInfo->DifficultyEntry[diff]);
        }

        if (!difficultyInfo->AIName.empty())
        {
            TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) lists difficulty %u mode entry %u with `AIName` filled in. `AIName` of difficulty 0 mode creature is always used instead.",
                cInfo->Entry, diff + 1, cInfo->DifficultyEntry[diff]);
            continue;
        }

        if (difficultyInfo->ScriptID)
        {
            TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) lists difficulty %u mode entry %u with `ScriptName` filled in. `ScriptName` of difficulty 0 mode creature is always used instead.",
                cInfo->Entry, diff + 1, cInfo->DifficultyEntry[diff]);
            continue;
        }

        _hasDifficultyEntries[diff].insert(cInfo->Entry);
        _difficultyEntries[diff].insert(cInfo->DifficultyEntry[diff]);
        ok = true;
    }

    if (cInfo->mingold > cInfo->maxgold)
    {
        TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) has `mingold` %u which is greater than `maxgold` %u, setting `maxgold` to %u.",
            cInfo->Entry, cInfo->mingold, cInfo->maxgold, cInfo->mingold);
        const_cast<CreatureTemplate*>(cInfo)->maxgold = cInfo->mingold;
    }

    if (!cInfo->AIName.empty())
    {
        auto registryItem = sCreatureAIRegistry->GetRegistryItem(cInfo->AIName);
        if (!registryItem)
        {
            TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) has non-registered `AIName` '%s' set, removing", cInfo->Entry, cInfo->AIName.c_str());
            const_cast<CreatureTemplate*>(cInfo)->AIName.clear();
        }
        else
        {
            DBPermit const* permit = dynamic_cast<DBPermit const*>(registryItem);
            if (!ASSERT_NOTNULL(permit)->IsScriptNameAllowedInDB())
            {
                TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) has not-allowed `AIName` '%s' set, removing", cInfo->Entry, cInfo->AIName.c_str());
                const_cast<CreatureTemplate*>(cInfo)->AIName.clear();
            }
        }
    }

    FactionTemplateEntry const* factionTemplate = sFactionTemplateStore.LookupEntry(cInfo->faction);
    if (!factionTemplate)
    {
        TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) has non-existing faction template (%u). This can lead to crashes, set to faction 35.", cInfo->Entry, cInfo->faction);
        const_cast<CreatureTemplate*>(cInfo)->faction = sFactionTemplateStore.AssertEntry(35)->ID; // this might seem stupid but all shit will would break if faction 35 did not exist
    }

    for (uint8 k = 0; k < MAX_KILL_CREDIT; ++k)
    {
        if (cInfo->KillCredit[k])
        {
            if (!GetCreatureTemplate(cInfo->KillCredit[k]))
            {
                TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) lists non-existing creature entry %u in `KillCredit%d`.", cInfo->Entry, cInfo->KillCredit[k], k + 1);
                const_cast<CreatureTemplate*>(cInfo)->KillCredit[k] = 0;
            }
        }
    }

    if (cInfo->Models.empty())
        TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) does not have any existing display id in creature_template_model.", cInfo->Entry);

    if (!cInfo->unit_class || ((1 << (cInfo->unit_class-1)) & CLASSMASK_ALL_CREATURES) == 0)
    {
        TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) has invalid unit_class (%u) in creature_template. Set to 1 (UNIT_CLASS_WARRIOR).", cInfo->Entry, cInfo->unit_class);
        const_cast<CreatureTemplate*>(cInfo)->unit_class = UNIT_CLASS_WARRIOR;
    }

    if (cInfo->dmgschool >= MAX_SPELL_SCHOOL)
    {
        TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) has invalid spell school value (%u) in `dmgschool`.", cInfo->Entry, cInfo->dmgschool);
        const_cast<CreatureTemplate*>(cInfo)->dmgschool = SPELL_SCHOOL_NORMAL;
    }

    if (cInfo->BaseAttackTime == 0)
        const_cast<CreatureTemplate*>(cInfo)->BaseAttackTime  = BASE_ATTACK_TIME;

    if (cInfo->RangeAttackTime == 0)
        const_cast<CreatureTemplate*>(cInfo)->RangeAttackTime = BASE_ATTACK_TIME;

    if (cInfo->speed_walk == 0.0f)
    {
        TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) has wrong value (%f) in speed_walk, set to 1.", cInfo->Entry, cInfo->speed_walk);
        const_cast<CreatureTemplate*>(cInfo)->speed_walk = 1.0f;
    }

    if (cInfo->speed_run == 0.0f)
    {
        TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) has wrong value (%f) in speed_run, set to 1.14286.", cInfo->Entry, cInfo->speed_run);
        const_cast<CreatureTemplate*>(cInfo)->speed_run = 1.14286f;
    }

    if (cInfo->type && !sCreatureTypeStore.LookupEntry(cInfo->type))
    {
        TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) has invalid creature type (%u) in `type`.", cInfo->Entry, cInfo->type);
        const_cast<CreatureTemplate*>(cInfo)->type = CREATURE_TYPE_HUMANOID;
    }

    if (cInfo->family && !sCreatureFamilyStore.LookupEntry(cInfo->family))
    {
        TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) has invalid creature family (%u) in `family`.", cInfo->Entry, cInfo->family);
        const_cast<CreatureTemplate*>(cInfo)->family = CREATURE_FAMILY_NONE;
    }

    CheckCreatureMovement("creature_template_movement", cInfo->Entry, const_cast<CreatureTemplate*>(cInfo)->Movement);

    if (cInfo->HoverHeight < 0.0f)
    {
        TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) has wrong value (%f) in `HoverHeight`", cInfo->Entry, cInfo->HoverHeight);
        const_cast<CreatureTemplate*>(cInfo)->HoverHeight = 1.0f;
    }

    if (cInfo->VehicleId)
    {
        VehicleEntry const* vehId = sVehicleStore.LookupEntry(cInfo->VehicleId);
        if (!vehId)
        {
             TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) has a non-existing VehicleId (%u). This *WILL* cause the client to freeze!", cInfo->Entry, cInfo->VehicleId);
             const_cast<CreatureTemplate*>(cInfo)->VehicleId = 0;
        }
    }

    for (uint8 j = 0; j < MAX_CREATURE_SPELLS; ++j)
    {
        if (cInfo->spells[j] && !sSpellMgr->GetSpellInfo(cInfo->spells[j], DIFFICULTY_NONE))
        {
            TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) has non-existing Spell%d (%u), set to 0.", cInfo->Entry, j+1, cInfo->spells[j]);
            const_cast<CreatureTemplate*>(cInfo)->spells[j] = 0;
        }
    }

    if (cInfo->MovementType >= MAX_DB_MOTION_TYPE)
    {
        TC_LOG_ERROR("sql.sql", "Creature (Entry: %u) has wrong movement generator type (%u), ignored and set to IDLE.", cInfo->Entry, cInfo->MovementType);
        const_cast<CreatureTemplate*>(cInfo)->MovementType = IDLE_MOTION_TYPE;
    }

    if (cInfo->HealthScalingExpansion < EXPANSION_LEVEL_CURRENT || cInfo->HealthScalingExpansion >= MAX_EXPANSIONS)
    {
        TC_LOG_ERROR("sql.sql", "Table `creature_template` lists creature (ID: %u) with invalid `HealthScalingExpansion` %i. Ignored and set to 0.", cInfo->Entry, cInfo->HealthScalingExpansion);
        const_cast<CreatureTemplate*>(cInfo)->HealthScalingExpansion = 0;
    }

    if (cInfo->RequiredExpansion >= MAX_EXPANSIONS)
    {
        TC_LOG_ERROR("sql.sql", "Table `creature_template` lists creature (Entry: %u) with `RequiredExpansion` %u. Ignored and set to 0.", cInfo->Entry, cInfo->RequiredExpansion);
        const_cast<CreatureTemplate*>(cInfo)->RequiredExpansion = 0;
    }

    if (uint32 badFlags = (cInfo->flags_extra & ~CREATURE_FLAG_EXTRA_DB_ALLOWED))
    {
        TC_LOG_ERROR("sql.sql", "Table `creature_template` lists creature (Entry: %u) with disallowed `flags_extra` %u, removing incorrect flag.", cInfo->Entry, badFlags);
        const_cast<CreatureTemplate*>(cInfo)->flags_extra &= CREATURE_FLAG_EXTRA_DB_ALLOWED;
    }

    std::pair<int16, int16> levels = cInfo->GetMinMaxLevel();
    if (levels.first < 1 || levels.first > STRONG_MAX_LEVEL)
    {
        TC_LOG_ERROR("sql.sql", "Creature (ID: %u): Calculated minLevel %i is not within [1, 255], value has been set to %u.", cInfo->Entry, cInfo->minlevel,
            cInfo->HealthScalingExpansion == EXPANSION_LEVEL_CURRENT ? MAX_LEVEL : 1);
        const_cast<CreatureTemplate*>(cInfo)->minlevel = cInfo->HealthScalingExpansion == EXPANSION_LEVEL_CURRENT ? 0 : 1;
    }

    if (levels.second < 1 || levels.second > STRONG_MAX_LEVEL)
    {
        TC_LOG_ERROR("sql.sql", "Creature (ID: %u): Calculated maxLevel %i is not within [1, 255], value has been set to %u.", cInfo->Entry, cInfo->maxlevel,
            cInfo->HealthScalingExpansion == EXPANSION_LEVEL_CURRENT ? MAX_LEVEL : 1);
        const_cast<CreatureTemplate*>(cInfo)->maxlevel = cInfo->HealthScalingExpansion == EXPANSION_LEVEL_CURRENT ? 0 : 1;
    }

    const_cast<CreatureTemplate*>(cInfo)->ModDamage *= Creature::_GetDamageMod(cInfo->rank);
}

void ObjectMgr::CheckCreatureMovement(char const* table, uint64 id, CreatureMovementData& creatureMovement)
{
    if (creatureMovement.Ground >= CreatureGroundMovementType::Max)
    {
        TC_LOG_ERROR("sql.sql", "`%s`.`Ground` wrong value (%u) for Id " UI64FMTD ", setting to Run.",
            table, uint32(creatureMovement.Ground), id);
        creatureMovement.Ground = CreatureGroundMovementType::Run;
    }

    if (creatureMovement.Flight >= CreatureFlightMovementType::Max)
    {
        TC_LOG_ERROR("sql.sql", "`%s`.`Flight` wrong value (%u) for Id " UI64FMTD ", setting to None.",
            table, uint32(creatureMovement.Flight), id);
        creatureMovement.Flight = CreatureFlightMovementType::None;
    }
}

void ObjectMgr::LoadCreatureAddons()
{
    uint32 oldMSTime = getMSTime();

    //                                                0       1       2      3       4       5        6             7              8                  9              10
    QueryResult result = WorldDatabase.Query("SELECT guid, path_id, mount, bytes1, bytes2, emote, aiAnimKit, movementAnimKit, meleeAnimKit, visibilityDistanceType, auras FROM creature_addon");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 creature addon definitions. DB table `creature_addon` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        ObjectGuid::LowType guid = fields[0].GetUInt64();

        CreatureData const* creData = GetCreatureData(guid);
        if (!creData)
        {
            TC_LOG_ERROR("sql.sql", "Creature (GUID: " UI64FMTD ") does not exist but has a record in `creature_addon`", guid);
            continue;
        }

        CreatureAddon& creatureAddon = _creatureAddonStore[guid];

        creatureAddon.path_id = fields[1].GetUInt32();
        if (creData->movementType == WAYPOINT_MOTION_TYPE && !creatureAddon.path_id)
        {
            const_cast<CreatureData*>(creData)->movementType = IDLE_MOTION_TYPE;
            TC_LOG_ERROR("sql.sql", "Creature (GUID " UI64FMTD ") has movement type set to WAYPOINT_MOTION_TYPE but no path assigned", guid);
        }

        creatureAddon.mount                     = fields[2].GetUInt32();
        creatureAddon.bytes1                    = fields[3].GetUInt32();
        creatureAddon.bytes2                    = fields[4].GetUInt32();
        creatureAddon.emote                     = fields[5].GetUInt32();
        creatureAddon.aiAnimKit                 = fields[6].GetUInt16();
        creatureAddon.movementAnimKit           = fields[7].GetUInt16();
        creatureAddon.meleeAnimKit              = fields[8].GetUInt16();
        creatureAddon.visibilityDistanceType    = VisibilityDistanceType(fields[9].GetUInt8());

        Tokenizer tokens(fields[10].GetString(), ' ');
        uint8 i = 0;
        creatureAddon.auras.resize(tokens.size());
        for (Tokenizer::const_iterator itr = tokens.begin(); itr != tokens.end(); ++itr)
        {
            uint32 spellId = uint32(atoul(*itr));
            SpellInfo const* AdditionalSpellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
            if (!AdditionalSpellInfo)
            {
                TC_LOG_ERROR("sql.sql", "Creature (GUID: " UI64FMTD ") has wrong spell %u defined in `auras` field in `creature_addon`.", guid, spellId);
                continue;
            }

            if (AdditionalSpellInfo->HasAura(SPELL_AURA_CONTROL_VEHICLE))
                TC_LOG_ERROR("sql.sql", "Creature (GUID: " UI64FMTD ") has SPELL_AURA_CONTROL_VEHICLE aura %u defined in `auras` field in `creature_addon`.", guid, spellId);

            if (std::find(creatureAddon.auras.begin(), creatureAddon.auras.end(), spellId) != creatureAddon.auras.end())
            {
                TC_LOG_ERROR("sql.sql", "Creature (GUID: " UI64FMTD ") has duplicate aura (spell %u) in `auras` field in `creature_addon`.", guid, spellId);
                continue;
            }

            creatureAddon.auras[i++] = spellId;
        }

        if (creatureAddon.mount)
        {
            if (!sCreatureDisplayInfoStore.LookupEntry(creatureAddon.mount))
            {
                TC_LOG_ERROR("sql.sql", "Creature (GUID: " UI64FMTD ") has invalid displayInfoId (%u) for mount defined in `creature_addon`", guid, creatureAddon.mount);
                creatureAddon.mount = 0;
            }
        }

        if (!sEmotesStore.LookupEntry(creatureAddon.emote))
        {
            TC_LOG_ERROR("sql.sql", "Creature (GUID: " UI64FMTD ") has invalid emote (%u) defined in `creature_addon`.", guid, creatureAddon.emote);
            creatureAddon.emote = 0;
        }

        if (creatureAddon.aiAnimKit && !sAnimKitStore.LookupEntry(creatureAddon.aiAnimKit))
        {
            TC_LOG_ERROR("sql.sql", "Creature (GUID: " UI64FMTD ") has invalid aiAnimKit (%u) defined in `creature_addon`.", guid, creatureAddon.aiAnimKit);
            creatureAddon.aiAnimKit = 0;
        }

        if (creatureAddon.movementAnimKit && !sAnimKitStore.LookupEntry(creatureAddon.movementAnimKit))
        {
            TC_LOG_ERROR("sql.sql", "Creature (GUID: " UI64FMTD ") has invalid movementAnimKit (%u) defined in `creature_addon`.", guid, creatureAddon.movementAnimKit);
            creatureAddon.movementAnimKit = 0;
        }

        if (creatureAddon.meleeAnimKit && !sAnimKitStore.LookupEntry(creatureAddon.meleeAnimKit))
        {
            TC_LOG_ERROR("sql.sql", "Creature (GUID: " UI64FMTD ") has invalid meleeAnimKit (%u) defined in `creature_addon`.", guid, creatureAddon.meleeAnimKit);
            creatureAddon.meleeAnimKit = 0;
        }

        if (creatureAddon.visibilityDistanceType >= VisibilityDistanceType::Max)
        {
            TC_LOG_ERROR("sql.sql", "Creature (GUID: " UI64FMTD ") has invalid visibilityDistanceType (%u) defined in `creature_addon`.",
                guid, AsUnderlyingType(creatureAddon.visibilityDistanceType));
            creatureAddon.visibilityDistanceType = VisibilityDistanceType::Normal;
        }

        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u creature addons in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadGameObjectAddons()
{
    uint32 oldMSTime = getMSTime();

    //                                               0     1                 2                 3                 4                 5                 6                  7              8
    QueryResult result = WorldDatabase.Query("SELECT guid, parent_rotation0, parent_rotation1, parent_rotation2, parent_rotation3, invisibilityType, invisibilityValue, WorldEffectID, AIAnimKitID FROM gameobject_addon");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 gameobject addon definitions. DB table `gameobject_addon` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        ObjectGuid::LowType guid = fields[0].GetUInt64();

        GameObjectData const* goData = GetGameObjectData(guid);
        if (!goData)
        {
            TC_LOG_ERROR("sql.sql", "GameObject (GUID: " UI64FMTD ") does not exist but has a record in `gameobject_addon`", guid);
            continue;
        }

        GameObjectAddon& gameObjectAddon = _gameObjectAddonStore[guid];
        gameObjectAddon.ParentRotation = QuaternionData(fields[1].GetFloat(), fields[2].GetFloat(), fields[3].GetFloat(), fields[4].GetFloat());
        gameObjectAddon.invisibilityType = InvisibilityType(fields[5].GetUInt8());
        gameObjectAddon.InvisibilityValue = fields[6].GetUInt32();
        gameObjectAddon.WorldEffectID = fields[7].GetUInt32();
        gameObjectAddon.AIAnimKitID   = fields[8].GetUInt32();

        if (gameObjectAddon.invisibilityType >= TOTAL_INVISIBILITY_TYPES)
        {
            TC_LOG_ERROR("sql.sql", "GameObject (GUID: " UI64FMTD ") has invalid InvisibilityType in `gameobject_addon`, disabled invisibility", guid);
            gameObjectAddon.invisibilityType = INVISIBILITY_GENERAL;
            gameObjectAddon.InvisibilityValue = 0;
        }

        if (gameObjectAddon.invisibilityType && !gameObjectAddon.InvisibilityValue)
        {
            TC_LOG_ERROR("sql.sql", "GameObject (GUID: " UI64FMTD ") has InvisibilityType set but has no InvisibilityValue in `gameobject_addon`, set to 1", guid);
            gameObjectAddon.InvisibilityValue = 1;
        }

        if (!gameObjectAddon.ParentRotation.isUnit())
        {
            TC_LOG_ERROR("sql.sql", "GameObject (GUID: " UI64FMTD ") has invalid parent rotation in `gameobject_addon`, set to default", guid);
            gameObjectAddon.ParentRotation = QuaternionData();
        }

        if (gameObjectAddon.WorldEffectID && !sWorldEffectStore.LookupEntry(gameObjectAddon.WorldEffectID))
        {
            TC_LOG_ERROR("sql.sql", "GameObject (GUID: " UI64FMTD ") has invalid WorldEffectID (%u) in `gameobject_addon`, set to 0.", guid, gameObjectAddon.WorldEffectID);
            gameObjectAddon.WorldEffectID = 0;
        }

        if (gameObjectAddon.AIAnimKitID && !sAnimKitStore.LookupEntry(gameObjectAddon.AIAnimKitID))
        {
            TC_LOG_ERROR("sql.sql", "GameObject (GUID: " UI64FMTD ") has invalid AIAnimKitID (%u) in `gameobject_addon`, set to 0.", guid, gameObjectAddon.AIAnimKitID);
            gameObjectAddon.AIAnimKitID = 0;
        }

        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u gameobject addons in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

GameObjectAddon const* ObjectMgr::GetGameObjectAddon(ObjectGuid::LowType lowguid) const
{
    GameObjectAddonContainer::const_iterator itr = _gameObjectAddonStore.find(lowguid);
    if (itr != _gameObjectAddonStore.end())
        return &(itr->second);

    return nullptr;
}

CreatureAddon const* ObjectMgr::GetCreatureAddon(ObjectGuid::LowType lowguid) const
{
    CreatureAddonContainer::const_iterator itr = _creatureAddonStore.find(lowguid);
    if (itr != _creatureAddonStore.end())
        return &(itr->second);

    return nullptr;
}

CreatureAddon const* ObjectMgr::GetCreatureTemplateAddon(uint32 entry) const
{
    CreatureTemplateAddonContainer::const_iterator itr = _creatureTemplateAddonStore.find(entry);
    if (itr != _creatureTemplateAddonStore.end())
        return &(itr->second);

    return nullptr;
}

CreatureMovementData const* ObjectMgr::GetCreatureMovementOverride(ObjectGuid::LowType spawnId) const
{
    return Trinity::Containers::MapGetValuePtr(_creatureMovementOverrides, spawnId);
}

EquipmentInfo const* ObjectMgr::GetEquipmentInfo(uint32 entry, int8& id) const
{
    EquipmentInfoContainer::const_iterator itr = _equipmentInfoStore.find(entry);
    if (itr == _equipmentInfoStore.end())
        return nullptr;

    if (itr->second.empty())
        return nullptr;

    if (id == -1) // select a random element
    {
        EquipmentInfoContainerInternal::const_iterator ritr = itr->second.begin();
        std::advance(ritr, urand(0u, itr->second.size() - 1));
        id = std::distance(itr->second.begin(), ritr) + 1;
        return &ritr->second;
    }
    else
    {
        EquipmentInfoContainerInternal::const_iterator itr2 = itr->second.find(id);
        if (itr2 != itr->second.end())
            return &itr2->second;
    }

    return nullptr;
}

void ObjectMgr::LoadEquipmentTemplates()
{
    uint32 oldMSTime = getMSTime();

    //                                                        0   1        2                 3            4
    QueryResult result = WorldDatabase.Query("SELECT CreatureID, ID, ItemID1, AppearanceModID1, ItemVisual1, "
    //                                                                     5                 6            7
                                                                    "ItemID2, AppearanceModID2, ItemVisual2, "
    //                                                                     8                 9           10
                                                                    "ItemID3, AppearanceModID3, ItemVisual3 "
                                                                    "FROM creature_equip_template");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 creature equipment templates. DB table `creature_equip_template` is empty!");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 entry = fields[0].GetUInt32();

        if (!sObjectMgr->GetCreatureTemplate(entry))
        {
            TC_LOG_ERROR("sql.sql", "Creature template (CreatureID: %u) does not exist but has a record in `creature_equip_template`", entry);
            continue;
        }

        uint8 id = fields[1].GetUInt8();
        if (!id)
        {
            TC_LOG_ERROR("sql.sql", "Creature equipment template with id 0 found for creature %u, skipped.", entry);
            continue;
        }

        EquipmentInfo& equipmentInfo = _equipmentInfoStore[entry][id];
        for (uint8 i = 0; i < MAX_EQUIPMENT_ITEMS; ++i)
        {
            equipmentInfo.Items[i].ItemId = fields[2 + i * 3].GetUInt32();
            equipmentInfo.Items[i].AppearanceModId = fields[3 + i * 3].GetUInt16();
            equipmentInfo.Items[i].ItemVisual = fields[4 + i * 3].GetUInt16();

            if (!equipmentInfo.Items[i].ItemId)
                continue;

            ItemEntry const* dbcItem = sItemStore.LookupEntry(equipmentInfo.Items[i].ItemId);
            if (!dbcItem)
            {
                TC_LOG_ERROR("sql.sql", "Unknown item (ID=%u) in creature_equip_template.ItemID%u for CreatureID = %u and ID=%u, forced to 0.",
                    equipmentInfo.Items[i].ItemId, i + 1, entry, id);
                equipmentInfo.Items[i].ItemId = 0;
                continue;
            }

            if (!sDB2Manager.GetItemModifiedAppearance(equipmentInfo.Items[i].ItemId, equipmentInfo.Items[i].AppearanceModId))
            {
                TC_LOG_ERROR("sql.sql", "Unknown item appearance for (ID=%u, AppearanceModID=%u) pair in creature_equip_template.ItemID%u creature_equip_template.AppearanceModID%u "
                    "for CreatureID = %u and ID=%u, forced to default.",
                    equipmentInfo.Items[i].ItemId, equipmentInfo.Items[i].AppearanceModId, i + 1, i + 1, entry, id);
                if (ItemModifiedAppearanceEntry const* defaultAppearance = sDB2Manager.GetDefaultItemModifiedAppearance(equipmentInfo.Items[i].ItemId))
                    equipmentInfo.Items[i].AppearanceModId = defaultAppearance->ItemAppearanceModifierID;
                else
                    equipmentInfo.Items[i].AppearanceModId = 0;
                continue;
            }

            if (dbcItem->InventoryType != INVTYPE_WEAPON &&
                dbcItem->InventoryType != INVTYPE_SHIELD &&
                dbcItem->InventoryType != INVTYPE_RANGED &&
                dbcItem->InventoryType != INVTYPE_2HWEAPON &&
                dbcItem->InventoryType != INVTYPE_WEAPONMAINHAND &&
                dbcItem->InventoryType != INVTYPE_WEAPONOFFHAND &&
                dbcItem->InventoryType != INVTYPE_HOLDABLE &&
                dbcItem->InventoryType != INVTYPE_THROWN &&
                dbcItem->InventoryType != INVTYPE_RANGEDRIGHT)
            {
                TC_LOG_ERROR("sql.sql", "Item (ID=%u) in creature_equip_template.ItemID%u for CreatureID = %u and ID = %u is not equipable in a hand, forced to 0.",
                    equipmentInfo.Items[i].ItemId, i + 1, entry, id);
                equipmentInfo.Items[i].ItemId = 0;
            }
        }

        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u equipment templates in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadCreatureMovementOverrides()
{
    uint32 oldMSTime = getMSTime();

    _creatureMovementOverrides.clear();

    QueryResult result = WorldDatabase.Query("SELECT SpawnId, Ground, Swim, Flight, Rooted from creature_movement_override");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 creature movement overrides. DB table `creature_movement_override` is empty!");
        return;
    }

    do
    {
        Field* fields = result->Fetch();
        ObjectGuid::LowType spawnId = fields[0].GetUInt64();
        if (!GetCreatureData(spawnId))
        {
            TC_LOG_ERROR("sql.sql", "Creature (GUID: " UI64FMTD ") does not exist but has a record in `creature_movement_override`", spawnId);
            continue;
        }

        CreatureMovementData& movement = _creatureMovementOverrides[spawnId];
        movement.Ground = static_cast<CreatureGroundMovementType>(fields[1].GetUInt8());
        movement.Swim = fields[2].GetBool();
        movement.Flight = static_cast<CreatureFlightMovementType>(fields[3].GetUInt8());
        movement.Rooted = fields[4].GetBool();

        CheckCreatureMovement("creature_movement_override", spawnId, movement);
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded " SZFMTD " movement overrides in %u ms", _creatureMovementOverrides.size(), GetMSTimeDiffToNow(oldMSTime));
}

CreatureModelInfo const* ObjectMgr::GetCreatureModelInfo(uint32 modelId) const
{
    CreatureModelContainer::const_iterator itr = _creatureModelStore.find(modelId);
    if (itr != _creatureModelStore.end())
        return &(itr->second);

    return nullptr;
}

CreatureModel const* ObjectMgr::ChooseDisplayId(CreatureTemplate const* cinfo, CreatureData const* data /*= nullptr*/)
{
    // Load creature model (display id)
    if (data && data->displayid)
        if (CreatureModel const* model = cinfo->GetModelWithDisplayId(data->displayid))
            return model;

    if (!(cinfo->flags_extra & CREATURE_FLAG_EXTRA_TRIGGER))
        if (CreatureModel const* model = cinfo->GetRandomValidModel())
            return model;

    // Triggers by default receive the invisible model
    return cinfo->GetFirstInvisibleModel();
}

void ObjectMgr::ChooseCreatureFlags(CreatureTemplate const* cInfo, uint64& npcFlags, uint32& unitFlags, uint32& unitFlags2, uint32& unitFlags3, uint32& dynamicFlags, CreatureData const* data /*= nullptr*/)
{
    npcFlags = cInfo->npcflag;
    unitFlags = cInfo->unit_flags;
    unitFlags2 = cInfo->unit_flags2;
    unitFlags3 = cInfo->unit_flags3;
    dynamicFlags = cInfo->dynamicflags;

    if (data)
    {
        if (data->npcflag)
            npcFlags = data->npcflag;

        if (data->unit_flags)
            unitFlags = data->unit_flags;

        if (data->unit_flags2)
            unitFlags2 = data->unit_flags2;

        if (data->unit_flags3)
            unitFlags3 = data->unit_flags3;

        if (data->dynamicflags)
            dynamicFlags = data->dynamicflags;
    }
}

CreatureModelInfo const* ObjectMgr::GetCreatureModelRandomGender(CreatureModel* model, CreatureTemplate const* creatureTemplate) const
{
    CreatureModelInfo const* modelInfo = GetCreatureModelInfo(model->CreatureDisplayID);
    if (!modelInfo)
        return nullptr;

    // If a model for another gender exists, 50% chance to use it
    if (modelInfo->displayId_other_gender != 0 && urand(0, 1) == 0)
    {
        CreatureModelInfo const* minfo_tmp = GetCreatureModelInfo(modelInfo->displayId_other_gender);
        if (!minfo_tmp)
            TC_LOG_ERROR("sql.sql", "Model (Entry: %u) has modelid_other_gender %u not found in table `creature_model_info`. ", model->CreatureDisplayID, modelInfo->displayId_other_gender);
        else
        {
            // DisplayID changed
            model->CreatureDisplayID = modelInfo->displayId_other_gender;
            if (creatureTemplate)
            {
                auto itr = std::find_if(creatureTemplate->Models.begin(), creatureTemplate->Models.end(), [&](CreatureModel const& templateModel)
                {
                    return templateModel.CreatureDisplayID == modelInfo->displayId_other_gender;
                });
                if (itr != creatureTemplate->Models.end())
                    *model = *itr;
            }
            return minfo_tmp;
        }
    }

    return modelInfo;
}

void ObjectMgr::LoadCreatureModelInfo()
{
    uint32 oldMSTime = getMSTime();

    QueryResult result = WorldDatabase.Query("SELECT DisplayID, BoundingRadius, CombatReach, DisplayID_Other_Gender FROM creature_model_info");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 creature model definitions. DB table `creature_model_info` is empty.");
        return;
    }

    _creatureModelStore.reserve(result->GetRowCount());
    uint32 count = 0;

    // List of model FileDataIDs that the client treats as invisible stalker
    uint32 trigggerCreatureModelFileID[5] = { 124640, 124641, 124642, 343863, 439302 };

    do
    {
        Field* fields = result->Fetch();

        uint32 displayId = fields[0].GetUInt32();

        CreatureDisplayInfoEntry const* creatureDisplay = sCreatureDisplayInfoStore.LookupEntry(displayId);
        if (!creatureDisplay)
        {
            TC_LOG_ERROR("sql.sql", "Table `creature_model_info` has a non-existent DisplayID (ID: %u). Skipped.", displayId);
            continue;
        }

        CreatureModelInfo& modelInfo = _creatureModelStore[displayId];

        modelInfo.bounding_radius        = fields[1].GetFloat();
        modelInfo.combat_reach           = fields[2].GetFloat();
        modelInfo.displayId_other_gender = fields[3].GetUInt32();
        modelInfo.gender                 = creatureDisplay->Gender;
        modelInfo.is_trigger             = false;

        // Checks

        // to remove when the purpose of GENDER_UNKNOWN is known
        if (modelInfo.gender == GENDER_UNKNOWN)
        {
            // We don't need more errors
            //TC_LOG_ERROR("sql.sql", "Table `creature_model_info` has an unimplemented Gender (ID: %i) being used by DisplayID (ID: %u). Gender set to GENDER_MALE.", modelInfo.gender, modelId);
            modelInfo.gender = GENDER_MALE;
        }

        if (modelInfo.displayId_other_gender && !sCreatureDisplayInfoStore.LookupEntry(modelInfo.displayId_other_gender))
        {
            TC_LOG_ERROR("sql.sql", "Table `creature_model_info` has a non-existent DisplayID_Other_Gender (ID: %u) being used by DisplayID (ID: %u).", modelInfo.displayId_other_gender, displayId);
            modelInfo.displayId_other_gender = 0;
        }

        if (modelInfo.combat_reach < 0.1f)
            modelInfo.combat_reach = DEFAULT_PLAYER_COMBAT_REACH;

        if (CreatureModelDataEntry const* modelData = sCreatureModelDataStore.LookupEntry(creatureDisplay->ModelID))
        {
            for (uint32 i = 0; i < 5; ++i)
            {
                if (modelData->FileDataID == trigggerCreatureModelFileID[i])
                {
                    modelInfo.is_trigger = true;
                    break;
                }
            }
        }

        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u creature model based info in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadLinkedRespawn()
{
    uint32 oldMSTime = getMSTime();

    _linkedRespawnStore.clear();
    //                                                 0        1          2
    QueryResult result = WorldDatabase.Query("SELECT guid, linkedGuid, linkType FROM linked_respawn ORDER BY guid ASC");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 linked respawns. DB table `linked_respawn` is empty.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        ObjectGuid::LowType guidLow = fields[0].GetUInt64();
        ObjectGuid::LowType linkedGuidLow = fields[1].GetUInt64();
        uint8  linkType = fields[2].GetUInt8();

        ObjectGuid guid, linkedGuid;
        bool error = false;
        switch (linkType)
        {
            case LINKED_RESPAWN_CREATURE_TO_CREATURE:
            {
                CreatureData const* slave = GetCreatureData(guidLow);
                if (!slave)
                {
                    TC_LOG_ERROR("sql.sql", "LinkedRespawn: Creature (guid) '" UI64FMTD "' not found in creature table", guidLow);
                    error = true;
                    break;
                }

                CreatureData const* master = GetCreatureData(linkedGuidLow);
                if (!master)
                {
                    TC_LOG_ERROR("sql.sql", "LinkedRespawn: Creature (linkedGuid) '" UI64FMTD "' not found in creature table", linkedGuidLow);
                    error = true;
                    break;
                }

                MapEntry const* const map = sMapStore.LookupEntry(master->spawnPoint.GetMapId());
                if (!map || !map->Instanceable() || (master->spawnPoint.GetMapId() != slave->spawnPoint.GetMapId()))
                {
                    TC_LOG_ERROR("sql.sql", "LinkedRespawn: Creature '" UI64FMTD "' linking to Creature '" UI64FMTD "' on an unpermitted map.", guidLow, linkedGuidLow);
                    error = true;
                    break;
                }

                // they must have a possibility to meet (normal/heroic difficulty)
                if (!Trinity::Containers::Intersects(master->spawnDifficulties.begin(), master->spawnDifficulties.end(), slave->spawnDifficulties.begin(), slave->spawnDifficulties.end()))
                {
                    TC_LOG_ERROR("sql.sql", "LinkedRespawn: Creature '" UI64FMTD "' linking to Creature '" UI64FMTD "' with not corresponding spawnMask", guidLow, linkedGuidLow);
                    error = true;
                    break;
                }

                guid = ObjectGuid::Create<HighGuid::Creature>(slave->spawnPoint.GetMapId(), slave->id, guidLow);
                linkedGuid = ObjectGuid::Create<HighGuid::Creature>(master->spawnPoint.GetMapId(), master->id, linkedGuidLow);
                break;
            }
            case LINKED_RESPAWN_CREATURE_TO_GO:
            {
                CreatureData const* slave = GetCreatureData(guidLow);
                if (!slave)
                {
                    TC_LOG_ERROR("sql.sql", "LinkedRespawn: Creature (guid) '" UI64FMTD "' not found in creature table", guidLow);
                    error = true;
                    break;
                }

                GameObjectData const* master = GetGameObjectData(linkedGuidLow);
                if (!master)
                {
                    TC_LOG_ERROR("sql.sql", "LinkedRespawn: Gameobject (linkedGuid) '" UI64FMTD "' not found in gameobject table", linkedGuidLow);
                    error = true;
                    break;
                }

                MapEntry const* const map = sMapStore.LookupEntry(master->spawnPoint.GetMapId());
                if (!map || !map->Instanceable() || (master->spawnPoint.GetMapId() != slave->spawnPoint.GetMapId()))
                {
                    TC_LOG_ERROR("sql.sql", "LinkedRespawn: Creature '" UI64FMTD "' linking to Gameobject '" UI64FMTD "' on an unpermitted map.", guidLow, linkedGuidLow);
                    error = true;
                    break;
                }

                // they must have a possibility to meet (normal/heroic difficulty)
                if (!Trinity::Containers::Intersects(master->spawnDifficulties.begin(), master->spawnDifficulties.end(), slave->spawnDifficulties.begin(), slave->spawnDifficulties.end()))
                {
                    TC_LOG_ERROR("sql.sql", "LinkedRespawn: Creature '" UI64FMTD "' linking to Gameobject '" UI64FMTD "' with not corresponding spawnMask", guidLow, linkedGuidLow);
                    error = true;
                    break;
                }

                guid = ObjectGuid::Create<HighGuid::Creature>(slave->spawnPoint.GetMapId(), slave->id, guidLow);
                linkedGuid = ObjectGuid::Create<HighGuid::GameObject>(master->spawnPoint.GetMapId(), master->id, linkedGuidLow);
                break;
            }
            case LINKED_RESPAWN_GO_TO_GO:
            {
                GameObjectData const* slave = GetGameObjectData(guidLow);
                if (!slave)
                {
                    TC_LOG_ERROR("sql.sql", "LinkedRespawn: Gameobject (guid) '" UI64FMTD "' not found in gameobject table", guidLow);
                    error = true;
                    break;
                }

                GameObjectData const* master = GetGameObjectData(linkedGuidLow);
                if (!master)
                {
                    TC_LOG_ERROR("sql.sql", "LinkedRespawn: Gameobject (linkedGuid) '" UI64FMTD "' not found in gameobject table", linkedGuidLow);
                    error = true;
                    break;
                }

                MapEntry const* const map = sMapStore.LookupEntry(master->spawnPoint.GetMapId());
                if (!map || !map->Instanceable() || (master->spawnPoint.GetMapId() != slave->spawnPoint.GetMapId()))
                {
                    TC_LOG_ERROR("sql.sql", "LinkedRespawn: Gameobject '" UI64FMTD "' linking to Gameobject '" UI64FMTD "' on an unpermitted map.", guidLow, linkedGuidLow);
                    error = true;
                    break;
                }

                // they must have a possibility to meet (normal/heroic difficulty)
                if (!Trinity::Containers::Intersects(master->spawnDifficulties.begin(), master->spawnDifficulties.end(), slave->spawnDifficulties.begin(), slave->spawnDifficulties.end()))
                {
                    TC_LOG_ERROR("sql.sql", "LinkedRespawn: Gameobject '" UI64FMTD "' linking to Gameobject '" UI64FMTD "' with not corresponding spawnMask", guidLow, linkedGuidLow);
                    error = true;
                    break;
                }

                guid = ObjectGuid::Create<HighGuid::GameObject>(slave->spawnPoint.GetMapId(), slave->id, guidLow);
                linkedGuid = ObjectGuid::Create<HighGuid::GameObject>(master->spawnPoint.GetMapId(), master->id, linkedGuidLow);
                break;
            }
            case LINKED_RESPAWN_GO_TO_CREATURE:
            {
                GameObjectData const* slave = GetGameObjectData(guidLow);
                if (!slave)
                {
                    TC_LOG_ERROR("sql.sql", "LinkedRespawn: Gameobject (guid) '" UI64FMTD "' not found in gameobject table", guidLow);
                    error = true;
                    break;
                }

                CreatureData const* master = GetCreatureData(linkedGuidLow);
                if (!master)
                {
                    TC_LOG_ERROR("sql.sql", "LinkedRespawn: Creature (linkedGuid) '" UI64FMTD "' not found in creature table", linkedGuidLow);
                    error = true;
                    break;
                }

                MapEntry const* const map = sMapStore.LookupEntry(master->spawnPoint.GetMapId());
                if (!map || !map->Instanceable() || (master->spawnPoint.GetMapId() != slave->spawnPoint.GetMapId()))
                {
                    TC_LOG_ERROR("sql.sql", "LinkedRespawn: Gameobject '" UI64FMTD "' linking to Creature '" UI64FMTD "' on an unpermitted map.", guidLow, linkedGuidLow);
                    error = true;
                    break;
                }

                // they must have a possibility to meet (normal/heroic difficulty)
                if (!Trinity::Containers::Intersects(master->spawnDifficulties.begin(), master->spawnDifficulties.end(), slave->spawnDifficulties.begin(), slave->spawnDifficulties.end()))
                {
                    TC_LOG_ERROR("sql.sql", "LinkedRespawn: Gameobject '" UI64FMTD "' linking to Creature '" UI64FMTD "' with not corresponding spawnMask", guidLow, linkedGuidLow);
                    error = true;
                    break;
                }

                guid = ObjectGuid::Create<HighGuid::GameObject>(slave->spawnPoint.GetMapId(), slave->id, guidLow);
                linkedGuid = ObjectGuid::Create<HighGuid::Creature>(master->spawnPoint.GetMapId(), master->id, linkedGuidLow);
                break;
            }
        }

        if (!error)
            _linkedRespawnStore[guid] = linkedGuid;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded " UI64FMTD " linked respawns in %u ms", uint64(_linkedRespawnStore.size()), GetMSTimeDiffToNow(oldMSTime));
}

bool ObjectMgr::SetCreatureLinkedRespawn(ObjectGuid::LowType guidLow, ObjectGuid::LowType linkedGuidLow)
{
    if (!guidLow)
        return false;

    CreatureData const* master = GetCreatureData(guidLow);
    ASSERT(master);
    ObjectGuid guid = ObjectGuid::Create<HighGuid::Creature>(master->spawnPoint.GetMapId(), master->id, guidLow);

    if (!linkedGuidLow) // we're removing the linking
    {
        _linkedRespawnStore.erase(guid);
        WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_DEL_LINKED_RESPAWN);
        stmt->setUInt64(0, guidLow);
        stmt->setUInt32(1, LINKED_RESPAWN_CREATURE_TO_CREATURE);
        WorldDatabase.Execute(stmt);
        return true;
    }

    CreatureData const* slave = GetCreatureData(linkedGuidLow);
    if (!slave)
    {
        TC_LOG_ERROR("sql.sql", "Creature '" UI64FMTD "' linking to non-existent creature '" UI64FMTD "'.", guidLow, linkedGuidLow);
        return false;
    }

    MapEntry const* map = sMapStore.LookupEntry(master->spawnPoint.GetMapId());
    if (!map || !map->Instanceable() || (master->spawnPoint.GetMapId() != slave->spawnPoint.GetMapId()))
    {
        TC_LOG_ERROR("sql.sql", "Creature '" UI64FMTD "' linking to '" UI64FMTD "' on an unpermitted map.", guidLow, linkedGuidLow);
        return false;
    }

    // they must have a possibility to meet (normal/heroic difficulty)
    if (!Trinity::Containers::Intersects(master->spawnDifficulties.begin(), master->spawnDifficulties.end(), slave->spawnDifficulties.begin(), slave->spawnDifficulties.end()))
    {
        TC_LOG_ERROR("sql.sql", "LinkedRespawn: Creature '" UI64FMTD "' linking to '" UI64FMTD "' with not corresponding spawnMask", guidLow, linkedGuidLow);
        return false;
    }

    ObjectGuid linkedGuid = ObjectGuid::Create<HighGuid::Creature>(slave->spawnPoint.GetMapId(), slave->id, linkedGuidLow);

    _linkedRespawnStore[guid] = linkedGuid;
    WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_REP_LINKED_RESPAWN);
    stmt->setUInt64(0, guidLow);
    stmt->setUInt64(1, linkedGuidLow);
    stmt->setUInt32(2, LINKED_RESPAWN_CREATURE_TO_CREATURE);
    WorldDatabase.Execute(stmt);
    return true;
}

void ObjectMgr::LoadTempSummons()
{
    uint32 oldMSTime = getMSTime();

    _tempSummonDataStore.clear();   // needed for reload case

    //                                               0           1             2        3      4           5           6           7            8           9
    QueryResult result = WorldDatabase.Query("SELECT summonerId, summonerType, groupId, entry, position_x, position_y, position_z, orientation, summonType, summonTime FROM creature_summon_groups");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 temp summons. DB table `creature_summon_groups` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 summonerId               = fields[0].GetUInt32();
        SummonerType summonerType       = SummonerType(fields[1].GetUInt8());
        uint8 group                     = fields[2].GetUInt8();

        switch (summonerType)
        {
            case SUMMONER_TYPE_CREATURE:
                if (!GetCreatureTemplate(summonerId))
                {
                    TC_LOG_ERROR("sql.sql", "Table `creature_summon_groups` has summoner with non existing entry %u for creature summoner type, skipped.", summonerId);
                    continue;
                }
                break;
            case SUMMONER_TYPE_GAMEOBJECT:
                if (!GetGameObjectTemplate(summonerId))
                {
                    TC_LOG_ERROR("sql.sql", "Table `creature_summon_groups` has summoner with non existing entry %u for gameobject summoner type, skipped.", summonerId);
                    continue;
                }
                break;
            case SUMMONER_TYPE_MAP:
                if (!sMapStore.LookupEntry(summonerId))
                {
                    TC_LOG_ERROR("sql.sql", "Table `creature_summon_groups` has summoner with non existing entry %u for map summoner type, skipped.", summonerId);
                    continue;
                }
                break;
            default:
                TC_LOG_ERROR("sql.sql", "Table `creature_summon_groups` has unhandled summoner type %u for summoner %u, skipped.", summonerType, summonerId);
                continue;
        }

        TempSummonData data;
        data.entry                      = fields[3].GetUInt32();

        if (!GetCreatureTemplate(data.entry))
        {
            TC_LOG_ERROR("sql.sql", "Table `creature_summon_groups` has creature in group [Summoner ID: %u, Summoner Type: %u, Group ID: %u] with non existing creature entry %u, skipped.", summonerId, summonerType, group, data.entry);
            continue;
        }

        float posX                      = fields[4].GetFloat();
        float posY                      = fields[5].GetFloat();
        float posZ                      = fields[6].GetFloat();
        float orientation               = fields[7].GetFloat();

        data.pos.Relocate(posX, posY, posZ, orientation);

        data.type                       = TempSummonType(fields[8].GetUInt8());

        if (data.type > TEMPSUMMON_MANUAL_DESPAWN)
        {
            TC_LOG_ERROR("sql.sql", "Table `creature_summon_groups` has unhandled temp summon type %u in group [Summoner ID: %u, Summoner Type: %u, Group ID: %u] for creature entry %u, skipped.", data.type, summonerId, summonerType, group, data.entry);
            continue;
        }

        data.time                       = fields[9].GetUInt32();

        TempSummonGroupKey key(summonerId, summonerType, group);
        _tempSummonDataStore[key].push_back(data);

        ++count;

    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u temp summons in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

inline std::vector<Difficulty> ParseSpawnDifficulties(std::string const& difficultyString, std::string const& table, ObjectGuid::LowType spawnId, uint32 mapId,
    std::set<Difficulty> const& mapDifficulties)
{
    Tokenizer tokens(difficultyString, ',', 0, false);
    std::vector<Difficulty> difficulties;
    bool isTransportMap = sObjectMgr->IsTransportMap(mapId);
    for (char const* token : tokens)
    {
        Difficulty difficultyId = Difficulty(strtoul(token, nullptr, 10));
        if (difficultyId && !sDifficultyStore.LookupEntry(difficultyId))
        {
            TC_LOG_ERROR("sql.sql", "Table `%s` has %s (GUID: " UI64FMTD ") with non invalid difficulty id %u, skipped.",
                table.c_str(), table.c_str(), spawnId, uint32(difficultyId));
            continue;
        }

        if (!isTransportMap && mapDifficulties.find(difficultyId) == mapDifficulties.end())
        {
            TC_LOG_ERROR("sql.sql", "Table `%s` has %s (GUID: " UI64FMTD ") has unsupported difficulty %u for map (Id: %u).",
                table.c_str(), table.c_str(), spawnId, uint32(difficultyId), mapId);
            continue;
        }

        difficulties.push_back(difficultyId);
    }

    std::sort(difficulties.begin(), difficulties.end());
    return difficulties;
}

void ObjectMgr::LoadCreatures()
{
    uint32 oldMSTime = getMSTime();

    //                                               0              1   2    3           4           5           6            7        8             9              10
    QueryResult result = WorldDatabase.Query("SELECT creature.guid, id, map, position_x, position_y, position_z, orientation, modelid, equipment_id, spawntimesecs, spawndist, "
    //   11               12         13       14            15                 16          17          18                19                   20                    21
        "currentwaypoint, curhealth, curmana, MovementType, spawnDifficulties, eventEntry, pool_entry, creature.npcflag, creature.unit_flags, creature.unit_flags2, creature.unit_flags3, "
    //   22                     23                      24                25                   26                       27
        "creature.dynamicflags, creature.phaseUseFlags, creature.phaseid, creature.phasegroup, creature.terrainSwapMap, creature.ScriptName "
        "FROM creature "
        "LEFT OUTER JOIN game_event_creature ON creature.guid = game_event_creature.guid "
        "LEFT OUTER JOIN pool_creature ON creature.guid = pool_creature.guid");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 creatures. DB table `creature` is empty.");
        return;
    }

    // Build single time for check spawnmask
    std::unordered_map<uint32, std::set<Difficulty>> spawnMasks;
    for (auto& mapDifficultyPair : sDB2Manager.GetMapDifficulties())
        for (auto& difficultyPair : mapDifficultyPair.second)
            spawnMasks[mapDifficultyPair.first].insert(Difficulty(difficultyPair.first));

    PhaseShift phaseShift;

    _creatureDataStore.reserve(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();

        ObjectGuid::LowType guid = fields[0].GetUInt64();
        uint32 entry        = fields[1].GetUInt32();

        CreatureTemplate const* cInfo = GetCreatureTemplate(entry);
        if (!cInfo)
        {
            TC_LOG_ERROR("sql.sql", "Table `creature` has creature (GUID: " UI64FMTD ") with non existing creature entry %u, skipped.", guid, entry);
            continue;
        }

        CreatureData& data = _creatureDataStore[guid];
        data.spawnId        = guid;
        data.id             = entry;
        data.spawnPoint.WorldRelocate(fields[2].GetUInt16(), fields[3].GetFloat(), fields[4].GetFloat(), fields[5].GetFloat(), fields[6].GetFloat());
        data.displayid      = fields[7].GetUInt32();
        data.equipmentId    = fields[8].GetInt8();
        data.spawntimesecs  = fields[9].GetUInt32();
        data.spawndist      = fields[10].GetFloat();
        data.currentwaypoint= fields[11].GetUInt32();
        data.curhealth      = fields[12].GetUInt32();
        data.curmana        = fields[13].GetUInt32();
        data.movementType   = fields[14].GetUInt8();
        data.spawnDifficulties      = ParseSpawnDifficulties(fields[15].GetString(), "creature", guid, data.spawnPoint.GetMapId(), spawnMasks[data.spawnPoint.GetMapId()]);
        int16 gameEvent     = fields[16].GetInt8();
        uint32 PoolId       = fields[17].GetUInt32();
        data.npcflag        = fields[18].GetUInt64();
        data.unit_flags     = fields[19].GetUInt32();
        data.unit_flags2    = fields[20].GetUInt32();
        data.unit_flags3    = fields[21].GetUInt32();
        data.dynamicflags   = fields[22].GetUInt32();
        data.phaseUseFlags  = fields[23].GetUInt8();
        data.phaseId        = fields[24].GetUInt32();
        data.phaseGroup     = fields[25].GetUInt32();
        data.terrainSwapMap = fields[26].GetInt32();
        data.scriptId       = GetScriptId(fields[27].GetString());
        data.spawnGroupData = &_spawnGroupDataStore[IsTransportMap(data.spawnPoint.GetMapId()) ? 1 : 0]; // transport spawns default to compatibility group

        MapEntry const* mapEntry = sMapStore.LookupEntry(data.spawnPoint.GetMapId());
        if (!mapEntry)
        {
            TC_LOG_ERROR("sql.sql", "Table `creature` has creature (GUID: " UI64FMTD ") that spawned at nonexistent map (Id: %u), skipped.", guid, data.spawnPoint.GetMapId());
            continue;
        }

        if (sWorld->getBoolConfig(CONFIG_CREATURE_CHECK_INVALID_POSITION))
        {
            if (VMAP::IVMapManager* vmgr = VMAP::VMapFactory::createOrGetVMapManager())
            {
                if (vmgr->isMapLoadingEnabled() && !IsTransportMap(data.spawnPoint.GetMapId()))
                {
                    GridCoord gridCoord = Trinity::ComputeGridCoord(data.spawnPoint.GetPositionX(), data.spawnPoint.GetPositionY());
                    int gx = (MAX_NUMBER_OF_GRIDS - 1) - gridCoord.x_coord;
                    int gy = (MAX_NUMBER_OF_GRIDS - 1) - gridCoord.y_coord;

                    VMAP::LoadResult result = vmgr->existsMap((sWorld->GetDataPath() + "vmaps").c_str(), data.spawnPoint.GetMapId(), gx, gy);
                    if (result != VMAP::LoadResult::Success)
                        TC_LOG_ERROR("sql.sql", "Table `creature` has creature (GUID: " UI64FMTD " Entry: %u MapID: %u) spawned on a possible invalid position (%s)",
                            guid, data.id, data.spawnPoint.GetMapId(), data.spawnPoint.ToString().c_str());
                }
            }
        }

        if (data.spawnDifficulties.empty())
        {
            TC_LOG_ERROR("sql.sql", "Table `creature` has creature (GUID: " UI64FMTD ") that is not spawned in any difficulty, skipped.", guid);
            continue;
        }

        bool ok = true;
        for (uint32 diff = 0; diff < MAX_CREATURE_DIFFICULTIES && ok; ++diff)
        {
            if (_difficultyEntries[diff].find(data.id) != _difficultyEntries[diff].end())
            {
                TC_LOG_ERROR("sql.sql", "Table `creature` has creature (GUID: " UI64FMTD ") that is listed as difficulty %u template (entry: %u) in `creature_template`, skipped.",
                    guid, diff + 1, data.id);
                ok = false;
            }
        }
        if (!ok)
            continue;

        // -1 random, 0 no equipment
        if (data.equipmentId != 0)
        {
            if (!GetEquipmentInfo(data.id, data.equipmentId))
            {
                TC_LOG_ERROR("sql.sql", "Table `creature` has creature (Entry: %u) with equipment_id %u not found in table `creature_equip_template`, set to no equipment.", data.id, data.equipmentId);
                data.equipmentId = 0;
            }
        }

        if (cInfo->flags_extra & CREATURE_FLAG_EXTRA_INSTANCE_BIND)
        {
            if (!mapEntry->IsDungeon())
                TC_LOG_ERROR("sql.sql", "Table `creature` has creature (GUID: " UI64FMTD " Entry: %u) with `creature_template`.`flags_extra` including CREATURE_FLAG_EXTRA_INSTANCE_BIND but creature is not in instance.", guid, data.id);
        }

        if (data.movementType >= MAX_DB_MOTION_TYPE)
        {
            TC_LOG_ERROR("sql.sql", "Table `creature` has creature (GUID: " UI64FMTD " Entry: %u) with wrong movement generator type (%u), ignored and set to IDLE.", guid, data.id, data.movementType);
            data.movementType = IDLE_MOTION_TYPE;
        }

        if (data.spawndist < 0.0f)
        {
            TC_LOG_ERROR("sql.sql", "Table `creature` has creature (GUID: " UI64FMTD " Entry: %u) with `spawndist`< 0, set to 0.", guid, data.id);
            data.spawndist = 0.0f;
        }
        else if (data.movementType == RANDOM_MOTION_TYPE)
        {
            if (G3D::fuzzyEq(data.spawndist, 0.0f))
            {
                TC_LOG_ERROR("sql.sql", "Table `creature` has creature (GUID: " UI64FMTD " Entry: %u) with `MovementType`=1 (random movement) but with `spawndist`=0, replace by idle movement type (0).", guid, data.id);
                data.movementType = IDLE_MOTION_TYPE;
            }
        }
        else if (data.movementType == IDLE_MOTION_TYPE)
        {
            if (data.spawndist != 0.0f)
            {
                TC_LOG_ERROR("sql.sql", "Table `creature` has creature (GUID: " UI64FMTD " Entry: %u) with `MovementType`=0 (idle) have `spawndist`<>0, set to 0.", guid, data.id);
                data.spawndist = 0.0f;
            }
        }

        if (data.phaseUseFlags & ~PHASE_USE_FLAGS_ALL)
        {
            TC_LOG_ERROR("sql.sql", "Table `creature` have creature (GUID: " UI64FMTD " Entry: %u) has unknown `phaseUseFlags` set, removed unknown value.", guid, data.id);
            data.phaseUseFlags &= PHASE_USE_FLAGS_ALL;
        }

        if (data.phaseUseFlags & PHASE_USE_FLAGS_ALWAYS_VISIBLE && data.phaseUseFlags & PHASE_USE_FLAGS_INVERSE)
        {
            TC_LOG_ERROR("sql.sql", "Table `creature` have creature (GUID: " UI64FMTD " Entry: %u) has both `phaseUseFlags` PHASE_USE_FLAGS_ALWAYS_VISIBLE and PHASE_USE_FLAGS_INVERSE,"
                " removing PHASE_USE_FLAGS_INVERSE.", guid, data.id);
            data.phaseUseFlags &= ~PHASE_USE_FLAGS_INVERSE;
        }

        if (data.phaseGroup && data.phaseId)
        {
            TC_LOG_ERROR("sql.sql", "Table `creature` have creature (GUID: " UI64FMTD " Entry: %u) with both `phaseid` and `phasegroup` set, `phasegroup` set to 0", guid, data.id);
            data.phaseGroup = 0;
        }

        if (data.phaseId)
        {
            if (!sPhaseStore.LookupEntry(data.phaseId))
            {
                TC_LOG_ERROR("sql.sql", "Table `creature` have creature (GUID: " UI64FMTD " Entry: %u) with `phaseid` %u does not exist, set to 0", guid, data.id, data.phaseId);
                data.phaseId = 0;
            }
        }

        if (data.phaseGroup)
        {
            if (!sDB2Manager.GetPhasesForGroup(data.phaseGroup))
            {
                TC_LOG_ERROR("sql.sql", "Table `creature` have creature (GUID: " UI64FMTD " Entry: %u) with `phasegroup` %u does not exist, set to 0", guid, data.id, data.phaseGroup);
                data.phaseGroup = 0;
            }
        }

        if (data.terrainSwapMap != -1)
        {
            MapEntry const* terrainSwapEntry = sMapStore.LookupEntry(data.terrainSwapMap);
            if (!terrainSwapEntry)
            {
                TC_LOG_ERROR("sql.sql", "Table `creature` have creature (GUID: " UI64FMTD " Entry: %u) with `terrainSwapMap` %u does not exist, set to -1", guid, data.id, data.terrainSwapMap);
                data.terrainSwapMap = -1;
            }
            else if (terrainSwapEntry->ParentMapID != int16(data.spawnPoint.GetMapId()))
            {
                TC_LOG_ERROR("sql.sql", "Table `creature` have creature (GUID: " UI64FMTD " Entry: %u) with `terrainSwapMap` %u which cannot be used on spawn map, set to -1", guid, data.id, data.terrainSwapMap);
                data.terrainSwapMap = -1;
            }
        }

        if (sWorld->getBoolConfig(CONFIG_CALCULATE_CREATURE_ZONE_AREA_DATA))
        {
            uint32 zoneId = 0;
            uint32 areaId = 0;
            PhasingHandler::InitDbVisibleMapId(phaseShift, data.terrainSwapMap);
            sMapMgr->GetZoneAndAreaId(phaseShift, zoneId, areaId, data.spawnPoint);

            WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_UPD_CREATURE_ZONE_AREA_DATA);

            stmt->setUInt32(0, zoneId);
            stmt->setUInt32(1, areaId);
            stmt->setUInt64(2, guid);

            WorldDatabase.Execute(stmt);
        }

        // Add to grid if not managed by the game event or pool system
        if (gameEvent == 0 && PoolId == 0)
            AddCreatureToGrid(guid, &data);
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded " SZFMTD " creatures in %u ms", _creatureDataStore.size(), GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::AddCreatureToGrid(ObjectGuid::LowType guid, CreatureData const* data)
{
    for (Difficulty difficulty : data->spawnDifficulties)
    {
        CellCoord cellCoord = Trinity::ComputeCellCoord(data->spawnPoint.GetPositionX(), data->spawnPoint.GetPositionY());
        CellObjectGuids& cell_guids = _mapObjectGuidsStore[MAKE_PAIR32(data->spawnPoint.GetMapId(), difficulty)][cellCoord.GetId()];
        cell_guids.creatures.insert(guid);
    }
}

void ObjectMgr::RemoveCreatureFromGrid(ObjectGuid::LowType guid, CreatureData const* data)
{
    for (Difficulty difficulty : data->spawnDifficulties)
    {
        CellCoord cellCoord = Trinity::ComputeCellCoord(data->spawnPoint.GetPositionX(), data->spawnPoint.GetPositionY());
        CellObjectGuids& cell_guids = _mapObjectGuidsStore[MAKE_PAIR32(data->spawnPoint.GetMapId(), difficulty)][cellCoord.GetId()];
        cell_guids.creatures.erase(guid);
    }
}

ObjectGuid::LowType ObjectMgr::AddGameObjectData(uint32 entry, uint32 mapId, Position const& pos, QuaternionData const& rot, uint32 spawntimedelay /*= 0*/)
{
    GameObjectTemplate const* goinfo = GetGameObjectTemplate(entry);
    if (!goinfo)
        return UI64LIT(0);

    Map* map = sMapMgr->CreateBaseMap(mapId);
    if (!map)
        return UI64LIT(0);

    ObjectGuid::LowType spawnId = GenerateGameObjectSpawnId();
    GameObjectData& data = NewOrExistGameObjectData(spawnId);
    data.spawnId        = spawnId;
    data.id             = entry;
    data.spawnPoint.WorldRelocate(mapId,pos);
    data.rotation       = rot;
    data.spawntimesecs  = spawntimedelay;
    data.animprogress   = 100;
    data.spawnDifficulties.push_back(DIFFICULTY_NONE);
    data.goState        = GO_STATE_READY;
    data.artKit         = goinfo->type == GAMEOBJECT_TYPE_CONTROL_ZONE ? 21 : 0;
    data.dbData         = false;
    data.spawnGroupData = GetLegacySpawnGroup();

    AddGameobjectToGrid(spawnId, &data);

    // Spawn if necessary (loaded grids only)
    // We use spawn coords to spawn
    if (!map->Instanceable() && map->IsGridLoaded(data.spawnPoint))
    {
        GameObject* go = GameObject::CreateGameObjectFromDB(spawnId, map);
        if (!go)
        {
            TC_LOG_ERROR("misc", "AddGameObjectData: cannot add gameobject entry %u to map", entry);
            return UI64LIT(0);
        }
    }

    TC_LOG_DEBUG("maps", "AddGameObjectData: dbguid " UI64FMTD " entry %u map %u pos %s", spawnId, entry, mapId, data.spawnPoint.ToString().c_str());

    return spawnId;
}

ObjectGuid::LowType ObjectMgr::AddCreatureData(uint32 entry, uint32 mapId, Position const& pos, uint32 spawntimedelay /*= 0*/)
{
    CreatureTemplate const* cInfo = GetCreatureTemplate(entry);
    if (!cInfo)
        return UI64LIT(0);

    std::pair<int16, int16> levels = cInfo->GetMinMaxLevel();
    uint32 level = levels.first == levels.second ? levels.first : urand(levels.first, levels.second); // Only used for extracting creature base stats
    CreatureBaseStats const* stats = GetCreatureBaseStats(level, cInfo->unit_class);
    Map* map = sMapMgr->CreateBaseMap(mapId);
    if (!map)
        return UI64LIT(0);

    CreatureLevelScaling const* scaling = cInfo->GetLevelScaling(map->GetDifficultyID());

    ObjectGuid::LowType spawnId = GenerateCreatureSpawnId();
    CreatureData& data = NewOrExistCreatureData(spawnId);
    data.spawnId = spawnId;
    data.id = entry;
    data.spawnPoint.WorldRelocate(mapId, pos);
    data.displayid = 0;
    data.equipmentId = 0;
    data.spawntimesecs = spawntimedelay;
    data.spawndist = 0;
    data.currentwaypoint = 0;
    data.curhealth = sDB2Manager.EvaluateExpectedStat(ExpectedStatType::CreatureHealth, level, cInfo->GetHealthScalingExpansion(), scaling->ContentTuningID, Classes(cInfo->unit_class)) * cInfo->ModHealth * cInfo->ModHealthExtra;
    data.curmana = stats->GenerateMana(cInfo);
    data.movementType = cInfo->MovementType;
    data.spawnDifficulties.push_back(DIFFICULTY_NONE);
    data.dbData = false;
    data.npcflag = cInfo->npcflag;
    data.unit_flags = cInfo->unit_flags;
    data.dynamicflags = cInfo->dynamicflags;
    data.spawnGroupData = GetLegacySpawnGroup();

    AddCreatureToGrid(spawnId, &data);

    // We use spawn coords to spawn
    if (!map->Instanceable() && !map->IsRemovalGrid(data.spawnPoint))
    {
        Creature* creature = Creature::CreateCreatureFromDB(spawnId, map, true, true);
        if (!creature)
        {
            TC_LOG_ERROR("misc", "AddCreature: Cannot add creature entry %u to map", entry);
            return UI64LIT(0);
        }
    }

    return spawnId;
}

void ObjectMgr::LoadGameObjects()
{
    uint32 oldMSTime = getMSTime();

    //                                                0                1   2    3           4           5           6
    QueryResult result = WorldDatabase.Query("SELECT gameobject.guid, id, map, position_x, position_y, position_z, orientation, "
    //   7          8          9          10         11             12            13     14                 15          16
        "rotation0, rotation1, rotation2, rotation3, spawntimesecs, animprogress, state, spawnDifficulties, eventEntry, pool_entry, "
    //   17             18       19          20              21
        "phaseUseFlags, phaseid, phasegroup, terrainSwapMap, ScriptName "
        "FROM gameobject LEFT OUTER JOIN game_event_gameobject ON gameobject.guid = game_event_gameobject.guid "
        "LEFT OUTER JOIN pool_gameobject ON gameobject.guid = pool_gameobject.guid");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 gameobjects. DB table `gameobject` is empty.");
        return;
    }

    // build single time for check spawnmask
    std::unordered_map<uint32, std::set<Difficulty>> spawnMasks;
    for (auto& mapDifficultyPair : sDB2Manager.GetMapDifficulties())
        for (auto& difficultyPair : mapDifficultyPair.second)
            spawnMasks[mapDifficultyPair.first].insert(Difficulty(difficultyPair.first));

    PhaseShift phaseShift;

    _gameObjectDataStore.reserve(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();

        ObjectGuid::LowType guid = fields[0].GetUInt64();
        uint32 entry        = fields[1].GetUInt32();

        GameObjectTemplate const* gInfo = GetGameObjectTemplate(entry);
        if (!gInfo)
        {
            TC_LOG_ERROR("sql.sql", "Table `gameobject` has gameobject (GUID: " UI64FMTD ") with non existing gameobject entry %u, skipped.", guid, entry);
            continue;
        }

        if (!gInfo->displayId)
        {
            switch (gInfo->type)
            {
                case GAMEOBJECT_TYPE_TRAP:
                case GAMEOBJECT_TYPE_SPELL_FOCUS:
                    break;
                default:
                    TC_LOG_ERROR("sql.sql", "Gameobject (GUID: " UI64FMTD " Entry %u GoType: %u) doesn't have a displayId (%u), not loaded.", guid, entry, gInfo->type, gInfo->displayId);
                    break;
            }
        }

        if (gInfo->displayId && !sGameObjectDisplayInfoStore.LookupEntry(gInfo->displayId))
        {
            TC_LOG_ERROR("sql.sql", "Gameobject (GUID: " UI64FMTD " Entry %u GoType: %u) has an invalid displayId (%u), not loaded.", guid, entry, gInfo->type, gInfo->displayId);
            continue;
        }

        GameObjectData& data = _gameObjectDataStore[guid];

        data.spawnId        = guid;
        data.id             = entry;
        data.spawnPoint.WorldRelocate(fields[2].GetUInt16(), fields[3].GetFloat(), fields[4].GetFloat(), fields[5].GetFloat(), fields[6].GetFloat());
        data.rotation.x     = fields[7].GetFloat();
        data.rotation.y     = fields[8].GetFloat();
        data.rotation.z     = fields[9].GetFloat();
        data.rotation.w     = fields[10].GetFloat();
        data.spawntimesecs  = fields[11].GetInt32();
        data.spawnGroupData = &_spawnGroupDataStore[IsTransportMap(data.spawnPoint.GetMapId()) ? 1 : 0]; // transport spawns default to compatibility group

        MapEntry const* mapEntry = sMapStore.LookupEntry(data.spawnPoint.GetMapId());
        if (!mapEntry)
        {
            TC_LOG_ERROR("sql.sql", "Table `gameobject` has gameobject (GUID: " UI64FMTD " Entry: %u) spawned on a non-existed map (Id: %u), skip", guid, data.id, data.spawnPoint.GetMapId());
            continue;
        }

        if (sWorld->getBoolConfig(CONFIG_GAME_OBJECT_CHECK_INVALID_POSITION))
        {
            if (VMAP::IVMapManager* vmgr = VMAP::VMapFactory::createOrGetVMapManager())
            {
                if (vmgr->isMapLoadingEnabled() && !IsTransportMap(data.spawnPoint.GetMapId()))
                {
                    GridCoord gridCoord = Trinity::ComputeGridCoord(data.spawnPoint.GetPositionX(), data.spawnPoint.GetPositionY());
                    int gx = (MAX_NUMBER_OF_GRIDS - 1) - gridCoord.x_coord;
                    int gy = (MAX_NUMBER_OF_GRIDS - 1) - gridCoord.y_coord;

                    VMAP::LoadResult result = vmgr->existsMap((sWorld->GetDataPath() + "vmaps").c_str(), data.spawnPoint.GetMapId(), gx, gy);
                    if (result != VMAP::LoadResult::Success)
                        TC_LOG_ERROR("sql.sql", "Table `gameobject` has gameobject (GUID: " UI64FMTD " Entry: %u MapID: %u) spawned on a possible invalid position (%s)",
                            guid, data.id, data.spawnPoint.GetMapId(), data.spawnPoint.ToString().c_str());
                }
            }
        }

        if (data.spawntimesecs == 0 && gInfo->IsDespawnAtAction())
        {
            TC_LOG_ERROR("sql.sql", "Table `gameobject` has gameobject (GUID: " UI64FMTD " Entry: %u) with `spawntimesecs` (0) value, but the gameobejct is marked as despawnable at action.", guid, data.id);
        }

        data.animprogress   = fields[12].GetUInt8();
        data.artKit         = 0;

        uint32 go_state     = fields[13].GetUInt8();
        if (go_state >= MAX_GO_STATE)
        {
            if (gInfo->type != GAMEOBJECT_TYPE_TRANSPORT || go_state > GO_STATE_TRANSPORT_ACTIVE + MAX_GO_STATE_TRANSPORT_STOP_FRAMES)
            {
                TC_LOG_ERROR("sql.sql", "Table `gameobject` has gameobject (GUID: " UI64FMTD " Entry: %u) with invalid `state` (%u) value, skip", guid, data.id, go_state);
                continue;
            }
        }
        data.goState       = GOState(go_state);

        data.spawnDifficulties      = ParseSpawnDifficulties(fields[14].GetString(), "gameobject", guid, data.spawnPoint.GetMapId(), spawnMasks[data.spawnPoint.GetMapId()]);
        if (data.spawnDifficulties.empty())
        {
            TC_LOG_ERROR("sql.sql", "Table `creature` has creature (GUID: " UI64FMTD ") that is not spawned in any difficulty, skipped.", guid);
            continue;
        }

        int16 gameEvent     = fields[15].GetInt8();
        uint32 PoolId       = fields[16].GetUInt32();
        data.phaseUseFlags  = fields[17].GetUInt8();
        data.phaseId        = fields[18].GetUInt32();
        data.phaseGroup     = fields[19].GetUInt32();

        if (data.phaseUseFlags & ~PHASE_USE_FLAGS_ALL)
        {
            TC_LOG_ERROR("sql.sql", "Table `gameobject` have gameobject (GUID: " UI64FMTD " Entry: %u) has unknown `phaseUseFlags` set, removed unknown value.", guid, data.id);
            data.phaseUseFlags &= PHASE_USE_FLAGS_ALL;
        }

        if (data.phaseUseFlags & PHASE_USE_FLAGS_ALWAYS_VISIBLE && data.phaseUseFlags & PHASE_USE_FLAGS_INVERSE)
        {
            TC_LOG_ERROR("sql.sql", "Table `gameobject` have gameobject (GUID: " UI64FMTD " Entry: %u) has both `phaseUseFlags` PHASE_USE_FLAGS_ALWAYS_VISIBLE and PHASE_USE_FLAGS_INVERSE,"
                " removing PHASE_USE_FLAGS_INVERSE.", guid, data.id);
            data.phaseUseFlags &= ~PHASE_USE_FLAGS_INVERSE;
        }

        if (data.phaseGroup && data.phaseId)
        {
            TC_LOG_ERROR("sql.sql", "Table `gameobject` have gameobject (GUID: " UI64FMTD " Entry: %u) with both `phaseid` and `phasegroup` set, `phasegroup` set to 0", guid, data.id);
            data.phaseGroup = 0;
        }

        if (data.phaseId)
        {
            if (!sPhaseStore.LookupEntry(data.phaseId))
            {
                TC_LOG_ERROR("sql.sql", "Table `gameobject` have gameobject (GUID: " UI64FMTD " Entry: %u) with `phaseid` %u does not exist, set to 0", guid, data.id, data.phaseId);
                data.phaseId = 0;
            }
        }

        if (data.phaseGroup)
        {
            if (!sDB2Manager.GetPhasesForGroup(data.phaseGroup))
            {
                TC_LOG_ERROR("sql.sql", "Table `gameobject` have gameobject (GUID: " UI64FMTD " Entry: %u) with `phaseGroup` %u does not exist, set to 0", guid, data.id, data.phaseGroup);
                data.phaseGroup = 0;
            }
        }

        data.terrainSwapMap = fields[20].GetInt32();
        if (data.terrainSwapMap != -1)
        {
            MapEntry const* terrainSwapEntry = sMapStore.LookupEntry(data.terrainSwapMap);
            if (!terrainSwapEntry)
            {
                TC_LOG_ERROR("sql.sql", "Table `gameobject` have gameobject (GUID: " UI64FMTD " Entry: %u) with `terrainSwapMap` %u does not exist, set to -1", guid, data.id, data.terrainSwapMap);
                data.terrainSwapMap = -1;
            }
            else if (terrainSwapEntry->ParentMapID != int16(data.spawnPoint.GetMapId()))
            {
                TC_LOG_ERROR("sql.sql", "Table `gameobject` have gameobject (GUID: " UI64FMTD " Entry: %u) with `terrainSwapMap` %u which cannot be used on spawn map, set to -1", guid, data.id, data.terrainSwapMap);
                data.terrainSwapMap = -1;
            }
        }

        data.scriptId = GetScriptId(fields[21].GetString());

        if (data.rotation.x < -1.0f || data.rotation.x > 1.0f)
        {
            TC_LOG_ERROR("sql.sql", "Table `gameobject` has gameobject (GUID: " UI64FMTD " Entry: %u) with invalid rotationX (%f) value, skip", guid, data.id, data.rotation.x);
            continue;
        }

        if (data.rotation.y < -1.0f || data.rotation.y > 1.0f)
        {
            TC_LOG_ERROR("sql.sql", "Table `gameobject` has gameobject (GUID: " UI64FMTD " Entry: %u) with invalid rotationY (%f) value, skip", guid, data.id, data.rotation.y);
            continue;
        }

        if (data.rotation.z < -1.0f || data.rotation.z > 1.0f)
        {
            TC_LOG_ERROR("sql.sql", "Table `gameobject` has gameobject (GUID: " UI64FMTD " Entry: %u) with invalid rotationZ (%f) value, skip", guid, data.id, data.rotation.z);
            continue;
        }

        if (data.rotation.w < -1.0f || data.rotation.w > 1.0f)
        {
            TC_LOG_ERROR("sql.sql", "Table `gameobject` has gameobject (GUID: " UI64FMTD " Entry: %u) with invalid rotationW (%f) value, skip", guid, data.id, data.rotation.w);
            continue;
        }

        if (!MapManager::IsValidMapCoord(data.spawnPoint))
        {
            TC_LOG_ERROR("sql.sql", "Table `gameobject` has gameobject (GUID: " UI64FMTD " Entry: %u) with invalid coordinates, skip", guid, data.id);
            continue;
        }

        if (sWorld->getBoolConfig(CONFIG_CALCULATE_GAMEOBJECT_ZONE_AREA_DATA))
        {
            uint32 zoneId = 0;
            uint32 areaId = 0;
            PhasingHandler::InitDbVisibleMapId(phaseShift, data.terrainSwapMap);
            sMapMgr->GetZoneAndAreaId(phaseShift, zoneId, areaId, data.spawnPoint);

            WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_UPD_GAMEOBJECT_ZONE_AREA_DATA);

            stmt->setUInt32(0, zoneId);
            stmt->setUInt32(1, areaId);
            stmt->setUInt64(2, guid);

            WorldDatabase.Execute(stmt);
        }

        if (gameEvent == 0 && PoolId == 0)                      // if not this is to be managed by GameEvent System or Pool system
            AddGameobjectToGrid(guid, &data);
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded " SZFMTD " gameobjects in %u ms", _gameObjectDataStore.size(), GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadSpawnGroupTemplates()
{
    uint32 oldMSTime = getMSTime();

    //                                               0        1          2
    QueryResult result = WorldDatabase.Query("SELECT groupId, groupName, groupFlags FROM spawn_group_template");

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 groupId = fields[0].GetUInt32();
            SpawnGroupTemplateData& group = _spawnGroupDataStore[groupId];
            group.groupId = groupId;
            group.name = fields[1].GetString();
            group.mapId = SPAWNGROUP_MAP_UNSET;
            uint32 flags = fields[2].GetUInt32();
            if (flags & ~SPAWNGROUP_FLAGS_ALL)
            {
                flags &= SPAWNGROUP_FLAGS_ALL;
                TC_LOG_ERROR("sql.sql", "Invalid spawn group flag %u on group ID %u (%s), reduced to valid flag %u.", flags, groupId, group.name.c_str(), uint32(group.flags));
            }
            if (flags & SPAWNGROUP_FLAG_SYSTEM && flags & SPAWNGROUP_FLAG_MANUAL_SPAWN)
            {
                flags &= ~SPAWNGROUP_FLAG_MANUAL_SPAWN;
                TC_LOG_ERROR("sql.sql", "System spawn group %u (%s) has invalid manual spawn flag. Ignored.", groupId, group.name.c_str());
            }
            group.flags = SpawnGroupFlags(flags);
        } while (result->NextRow());
    }

    if (_spawnGroupDataStore.find(0) == _spawnGroupDataStore.end())
    {
        TC_LOG_ERROR("sql.sql", "Default spawn group (index 0) is missing from DB! Manually inserted.");
        SpawnGroupTemplateData& data = _spawnGroupDataStore[0];
        data.groupId = 0;
        data.name = "Default Group";
        data.mapId = 0;
        data.flags = SPAWNGROUP_FLAG_SYSTEM;
    }
    if (_spawnGroupDataStore.find(1) == _spawnGroupDataStore.end())
    {
        TC_LOG_ERROR("sql.sql", "Default legacy spawn group (index 1) is missing from DB! Manually inserted.");
        SpawnGroupTemplateData&data = _spawnGroupDataStore[1];
        data.groupId = 1;
        data.name = "Legacy Group";
        data.mapId = 0;
        data.flags = SpawnGroupFlags(SPAWNGROUP_FLAG_SYSTEM | SPAWNGROUP_FLAG_COMPATIBILITY_MODE);
    }

    if (result)
        TC_LOG_INFO("server.loading", ">> Loaded " SZFMTD " spawn group templates in %u ms", _spawnGroupDataStore.size(), GetMSTimeDiffToNow(oldMSTime));
    else
        TC_LOG_INFO("server.loading", ">> Loaded 0 spawn group templates. DB table `spawn_group_template` is empty.");

    return;
}

void ObjectMgr::LoadSpawnGroups()
{
    uint32 oldMSTime = getMSTime();

    //                                               0        1          2
    QueryResult result = WorldDatabase.Query("SELECT groupId, spawnType, spawnId FROM spawn_group");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 spawn group members. DB table `spawn_group` is empty.");
        return;
    }

    uint32 numMembers = 0;
    do
    {
        Field* fields = result->Fetch();
        uint32 groupId = fields[0].GetUInt32();
        SpawnObjectType spawnType;
        {
            uint32 type = fields[1].GetUInt8();
            if (type >= SPAWN_TYPE_MAX)
            {
                TC_LOG_ERROR("sql.sql", "Spawn data with invalid type %u listed for spawn group %u. Skipped.", type, groupId);
                continue;
            }
            spawnType = SpawnObjectType(type);
        }
        ObjectGuid::LowType spawnId = fields[2].GetUInt64();

        SpawnData const* data = GetSpawnData(spawnType, spawnId);
        if (!data)
        {
            TC_LOG_ERROR("sql.sql", "Spawn data with ID (%u," UI64FMTD ") not found, but is listed as a member of spawn group %u!", uint32(spawnType), spawnId, groupId);
            continue;
        }
        else if (data->spawnGroupData->groupId)
        {
            TC_LOG_ERROR("sql.sql", "Spawn with ID (%u," UI64FMTD ") is listed as a member of spawn group %u, but is already a member of spawn group %u. Skipping.", uint32(spawnType), spawnId, groupId, data->spawnGroupData->groupId);
            continue;
        }
        auto it = _spawnGroupDataStore.find(groupId);
        if (it == _spawnGroupDataStore.end())
        {
            TC_LOG_ERROR("sql.sql", "Spawn group %u assigned to spawn ID (%u," UI64FMTD "), but group is found!", groupId, uint32(spawnType), spawnId);
            continue;
        }
        else
        {
            SpawnGroupTemplateData& groupTemplate = it->second;
            if (groupTemplate.mapId == SPAWNGROUP_MAP_UNSET)
                groupTemplate.mapId = data->spawnPoint.GetMapId();
            else if (groupTemplate.mapId != data->spawnPoint.GetMapId() && !(groupTemplate.flags & SPAWNGROUP_FLAG_SYSTEM))
            {
                TC_LOG_ERROR("sql.sql", "Spawn group %u has map ID %u, but spawn (%u," UI64FMTD ") has map id %u - spawn NOT added to group!", groupId, groupTemplate.mapId, uint32(spawnType), spawnId, data->spawnPoint.GetMapId());
                continue;
            }
            const_cast<SpawnData*>(data)->spawnGroupData = &groupTemplate;
            if (!(groupTemplate.flags & SPAWNGROUP_FLAG_SYSTEM))
                _spawnGroupMapStore.emplace(groupId, data);
            ++numMembers;
        }
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u spawn group members in %u ms", numMembers, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadInstanceSpawnGroups()
{
    uint32 oldMSTime = getMSTime();

    //                                               0              1            2           3             4
    QueryResult result = WorldDatabase.Query("SELECT instanceMapId, bossStateId, bossStates, spawnGroupId, flags FROM instance_spawn_groups");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 instance spawn groups. DB table `instance_spawn_groups` is empty.");
        return;
    }

    uint32 n = 0;
    do
    {
        Field* fields = result->Fetch();
        uint32 const spawnGroupId = fields[3].GetUInt32();
        auto it = _spawnGroupDataStore.find(spawnGroupId);
        if (it == _spawnGroupDataStore.end() || (it->second.flags & SPAWNGROUP_FLAG_SYSTEM))
        {
            TC_LOG_ERROR("sql.sql", "Invalid spawn group %u specified for instance %u. Skipped.", spawnGroupId, fields[0].GetUInt16());
            continue;
        }

        uint16 const instanceMapId = fields[0].GetUInt16();
        std::vector<InstanceSpawnGroupInfo>& vector = _instanceSpawnGroupStore[instanceMapId];
        vector.emplace_back();
        InstanceSpawnGroupInfo& info = vector.back();
        info.SpawnGroupId = spawnGroupId;
        info.BossStateId = fields[1].GetUInt8();

        uint8 const ALL_STATES = (1 << TO_BE_DECIDED) - 1;
        uint8 const states = fields[2].GetUInt8();
        if (states & ~ALL_STATES)
        {
            info.BossStates = states & ALL_STATES;
            TC_LOG_ERROR("sql.sql", "Instance spawn group (%u,%u) had invalid boss state mask %u - truncated to %u.", instanceMapId, spawnGroupId, states, info.BossStates);
        }
        else
            info.BossStates = states;

        uint8 const flags = fields[4].GetUInt8();
        if (flags & ~InstanceSpawnGroupInfo::FLAG_ALL)
        {
            info.Flags = flags & InstanceSpawnGroupInfo::FLAG_ALL;
            TC_LOG_ERROR("sql.sql", "Instance spawn group (%u,%u) had invalid flags %u - truncated to %u.", instanceMapId, spawnGroupId, flags, info.Flags);
        }
        else
            info.Flags = flags;

        ++n;
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u instance spawn groups in %u ms", n, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::OnDeleteSpawnData(SpawnData const* data)
{
    auto templateIt = _spawnGroupDataStore.find(data->spawnGroupData->groupId);
    ASSERT(templateIt != _spawnGroupDataStore.end(), "Creature data for (%u," UI64FMTD ") is being deleted and has invalid spawn group index %u!", uint32(data->type), data->spawnId, data->spawnGroupData->groupId);
    if (templateIt->second.flags & SPAWNGROUP_FLAG_SYSTEM) // system groups don't store their members in the map
        return;

    auto pair = _spawnGroupMapStore.equal_range(data->spawnGroupData->groupId);
    for (auto it = pair.first; it != pair.second; ++it)
    {
        if (it->second != data)
            continue;
        _spawnGroupMapStore.erase(it);
        return;
    }
    ASSERT(false, "Spawn data (%u," UI64FMTD ") being removed is member of spawn group %u, but not actually listed in the lookup table for that group!", uint32(data->type), data->spawnId, data->spawnGroupData->groupId);
}

void ObjectMgr::AddGameobjectToGrid(ObjectGuid::LowType guid, GameObjectData const* data)
{
    for (Difficulty difficulty : data->spawnDifficulties)
    {
        CellCoord cellCoord = Trinity::ComputeCellCoord(data->spawnPoint.GetPositionX(), data->spawnPoint.GetPositionY());
        CellObjectGuids& cell_guids = _mapObjectGuidsStore[MAKE_PAIR32(data->spawnPoint.GetMapId(), difficulty)][cellCoord.GetId()];
        cell_guids.gameobjects.insert(guid);
    }
}

void ObjectMgr::RemoveGameobjectFromGrid(ObjectGuid::LowType guid, GameObjectData const* data)
{
    for (Difficulty difficulty : data->spawnDifficulties)
    {
        CellCoord cellCoord = Trinity::ComputeCellCoord(data->spawnPoint.GetPositionX(), data->spawnPoint.GetPositionY());
        CellObjectGuids& cell_guids = _mapObjectGuidsStore[MAKE_PAIR32(data->spawnPoint.GetMapId(), difficulty)][cellCoord.GetId()];
        cell_guids.gameobjects.erase(guid);
    }
}

uint32 FillMaxDurability(uint32 itemClass, uint32 itemSubClass, uint32 inventoryType, uint32 quality, uint32 itemLevel)
{
    if (itemClass != ITEM_CLASS_ARMOR && itemClass != ITEM_CLASS_WEAPON)
        return 0;

    static float const qualityMultipliers[MAX_ITEM_QUALITY] =
    {
        0.92f, 0.92f, 0.92f, 1.11f, 1.32f, 1.61f, 0.0f, 0.0f
    };

    static float const armorMultipliers[MAX_INVTYPE] =
    {
        0.00f, // INVTYPE_NON_EQUIP
        0.60f, // INVTYPE_HEAD
        0.00f, // INVTYPE_NECK
        0.60f, // INVTYPE_SHOULDERS
        0.00f, // INVTYPE_BODY
        1.00f, // INVTYPE_CHEST
        0.33f, // INVTYPE_WAIST
        0.72f, // INVTYPE_LEGS
        0.48f, // INVTYPE_FEET
        0.33f, // INVTYPE_WRISTS
        0.33f, // INVTYPE_HANDS
        0.00f, // INVTYPE_FINGER
        0.00f, // INVTYPE_TRINKET
        0.00f, // INVTYPE_WEAPON
        0.72f, // INVTYPE_SHIELD
        0.00f, // INVTYPE_RANGED
        0.00f, // INVTYPE_CLOAK
        0.00f, // INVTYPE_2HWEAPON
        0.00f, // INVTYPE_BAG
        0.00f, // INVTYPE_TABARD
        1.00f, // INVTYPE_ROBE
        0.00f, // INVTYPE_WEAPONMAINHAND
        0.00f, // INVTYPE_WEAPONOFFHAND
        0.00f, // INVTYPE_HOLDABLE
        0.00f, // INVTYPE_AMMO
        0.00f, // INVTYPE_THROWN
        0.00f, // INVTYPE_RANGEDRIGHT
        0.00f, // INVTYPE_QUIVER
        0.00f, // INVTYPE_RELIC
    };

    static float const weaponMultipliers[MAX_ITEM_SUBCLASS_WEAPON] =
    {
        0.91f, // ITEM_SUBCLASS_WEAPON_AXE
        1.00f, // ITEM_SUBCLASS_WEAPON_AXE2
        1.00f, // ITEM_SUBCLASS_WEAPON_BOW
        1.00f, // ITEM_SUBCLASS_WEAPON_GUN
        0.91f, // ITEM_SUBCLASS_WEAPON_MACE
        1.00f, // ITEM_SUBCLASS_WEAPON_MACE2
        1.00f, // ITEM_SUBCLASS_WEAPON_POLEARM
        0.91f, // ITEM_SUBCLASS_WEAPON_SWORD
        1.00f, // ITEM_SUBCLASS_WEAPON_SWORD2
        1.00f, // ITEM_SUBCLASS_WEAPON_WARGLAIVES
        1.00f, // ITEM_SUBCLASS_WEAPON_STAFF
        0.00f, // ITEM_SUBCLASS_WEAPON_EXOTIC
        0.00f, // ITEM_SUBCLASS_WEAPON_EXOTIC2
        0.66f, // ITEM_SUBCLASS_WEAPON_FIST_WEAPON
        0.00f, // ITEM_SUBCLASS_WEAPON_MISCELLANEOUS
        0.66f, // ITEM_SUBCLASS_WEAPON_DAGGER
        0.00f, // ITEM_SUBCLASS_WEAPON_THROWN
        0.00f, // ITEM_SUBCLASS_WEAPON_SPEAR
        1.00f, // ITEM_SUBCLASS_WEAPON_CROSSBOW
        0.66f, // ITEM_SUBCLASS_WEAPON_WAND
        0.66f, // ITEM_SUBCLASS_WEAPON_FISHING_POLE
    };

    float levelPenalty = 1.0f;
    if (itemLevel <= 28)
        levelPenalty = 0.966f - float(28u - itemLevel) / 54.0f;

    if (itemClass == ITEM_CLASS_ARMOR)
    {
        if (inventoryType > INVTYPE_ROBE)
            return 0;

        return 5 * uint32(round(25.0f * qualityMultipliers[quality] * armorMultipliers[inventoryType] * levelPenalty));
    }

    return 5 * uint32(round(18.0f * qualityMultipliers[quality] * weaponMultipliers[itemSubClass] * levelPenalty));
};

struct ItemSpecStats
{
    uint32 ItemType;
    uint32 ItemSpecStatTypes[MAX_ITEM_PROTO_STATS];
    uint32 ItemSpecStatCount;

    ItemSpecStats(ItemEntry const* item, ItemSparseEntry const* sparse) : ItemType(0), ItemSpecStatCount(0)
    {
        memset(ItemSpecStatTypes, -1, sizeof(ItemSpecStatTypes));

        if (item->ClassID == ITEM_CLASS_WEAPON)
        {
            ItemType = 5;
            switch (item->SubclassID)
            {
                case ITEM_SUBCLASS_WEAPON_AXE:
                    AddStat(ITEM_SPEC_STAT_ONE_HANDED_AXE);
                    break;
                case ITEM_SUBCLASS_WEAPON_AXE2:
                    AddStat(ITEM_SPEC_STAT_TWO_HANDED_AXE);
                    break;
                case ITEM_SUBCLASS_WEAPON_BOW:
                    AddStat(ITEM_SPEC_STAT_BOW);
                    break;
                case ITEM_SUBCLASS_WEAPON_GUN:
                    AddStat(ITEM_SPEC_STAT_GUN);
                    break;
                case ITEM_SUBCLASS_WEAPON_MACE:
                    AddStat(ITEM_SPEC_STAT_ONE_HANDED_MACE);
                    break;
                case ITEM_SUBCLASS_WEAPON_MACE2:
                    AddStat(ITEM_SPEC_STAT_TWO_HANDED_MACE);
                    break;
                case ITEM_SUBCLASS_WEAPON_POLEARM:
                    AddStat(ITEM_SPEC_STAT_POLEARM);
                    break;
                case ITEM_SUBCLASS_WEAPON_SWORD:
                    AddStat(ITEM_SPEC_STAT_ONE_HANDED_SWORD);
                    break;
                case ITEM_SUBCLASS_WEAPON_SWORD2:
                    AddStat(ITEM_SPEC_STAT_TWO_HANDED_SWORD);
                    break;
                case ITEM_SUBCLASS_WEAPON_WARGLAIVES:
                    AddStat(ITEM_SPEC_STAT_WARGLAIVES);
                    break;
                case ITEM_SUBCLASS_WEAPON_STAFF:
                    AddStat(ITEM_SPEC_STAT_STAFF);
                    break;
                case ITEM_SUBCLASS_WEAPON_FIST_WEAPON:
                    AddStat(ITEM_SPEC_STAT_FIST_WEAPON);
                    break;
                case ITEM_SUBCLASS_WEAPON_DAGGER:
                    AddStat(ITEM_SPEC_STAT_DAGGER);
                    break;
                case ITEM_SUBCLASS_WEAPON_THROWN:
                    AddStat(ITEM_SPEC_STAT_THROWN);
                    break;
                case ITEM_SUBCLASS_WEAPON_CROSSBOW:
                    AddStat(ITEM_SPEC_STAT_CROSSBOW);
                    break;
                case ITEM_SUBCLASS_WEAPON_WAND:
                    AddStat(ITEM_SPEC_STAT_WAND);
                    break;
                default:
                    break;
            }
        }
        else if (item->ClassID == ITEM_CLASS_ARMOR)
        {
            switch (item->SubclassID)
            {
                case ITEM_SUBCLASS_ARMOR_CLOTH:
                    if (sparse->InventoryType != INVTYPE_CLOAK)
                    {
                        ItemType = 1;
                        break;
                    }

                    ItemType = 0;
                    AddStat(ITEM_SPEC_STAT_CLOAK);
                    break;
                case ITEM_SUBCLASS_ARMOR_LEATHER:
                    ItemType = 2;
                    break;
                case ITEM_SUBCLASS_ARMOR_MAIL:
                    ItemType = 3;
                    break;
                case ITEM_SUBCLASS_ARMOR_PLATE:
                    ItemType = 4;
                    break;
                default:
                    if (item->SubclassID == ITEM_SUBCLASS_ARMOR_SHIELD)
                    {
                        ItemType = 6;
                        AddStat(ITEM_SPEC_STAT_SHIELD);
                    }
                    else if (item->SubclassID > ITEM_SUBCLASS_ARMOR_SHIELD && item->SubclassID <= ITEM_SUBCLASS_ARMOR_RELIC)
                    {
                        ItemType = 6;
                        AddStat(ITEM_SPEC_STAT_RELIC);
                    }
                    else
                        ItemType = 0;
                    break;
            }
        }
        else if (item->ClassID == ITEM_CLASS_GEM)
        {
            ItemType = 7;
            if (GemPropertiesEntry const* gem = sGemPropertiesStore.LookupEntry(sparse->GemProperties))
            {
                if (gem->Type & SOCKET_COLOR_RELIC_IRON)
                    AddStat(ITEM_SPEC_STAT_RELIC_IRON);
                if (gem->Type & SOCKET_COLOR_RELIC_BLOOD)
                    AddStat(ITEM_SPEC_STAT_RELIC_BLOOD);
                if (gem->Type & SOCKET_COLOR_RELIC_SHADOW)
                    AddStat(ITEM_SPEC_STAT_RELIC_SHADOW);
                if (gem->Type & SOCKET_COLOR_RELIC_FEL)
                    AddStat(ITEM_SPEC_STAT_RELIC_FEL);
                if (gem->Type & SOCKET_COLOR_RELIC_ARCANE)
                    AddStat(ITEM_SPEC_STAT_RELIC_ARCANE);
                if (gem->Type & SOCKET_COLOR_RELIC_FROST)
                    AddStat(ITEM_SPEC_STAT_RELIC_FROST);
                if (gem->Type & SOCKET_COLOR_RELIC_FIRE)
                    AddStat(ITEM_SPEC_STAT_RELIC_FIRE);
                if (gem->Type & SOCKET_COLOR_RELIC_WATER)
                    AddStat(ITEM_SPEC_STAT_RELIC_WATER);
                if (gem->Type & SOCKET_COLOR_RELIC_LIFE)
                    AddStat(ITEM_SPEC_STAT_RELIC_LIFE);
                if (gem->Type & SOCKET_COLOR_RELIC_WIND)
                    AddStat(ITEM_SPEC_STAT_RELIC_WIND);
                if (gem->Type & SOCKET_COLOR_RELIC_HOLY)
                    AddStat(ITEM_SPEC_STAT_RELIC_HOLY);
            }
        }
        else
            ItemType = 0;

        for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
            if (sparse->StatModifierBonusStat[i] != -1)
                AddModStat(sparse->StatModifierBonusStat[i]);
    }

    void AddStat(ItemSpecStat statType)
    {
        if (ItemSpecStatCount >= MAX_ITEM_PROTO_STATS)
            return;

        for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
            if (ItemSpecStatTypes[i] == uint32(statType))
                return;

        ItemSpecStatTypes[ItemSpecStatCount++] = statType;
    }

    void AddModStat(int32 itemStatType)
    {
        switch (itemStatType)
        {
            case ITEM_MOD_AGILITY:
                AddStat(ITEM_SPEC_STAT_AGILITY);
                break;
            case ITEM_MOD_STRENGTH:
                AddStat(ITEM_SPEC_STAT_STRENGTH);
                break;
            case ITEM_MOD_INTELLECT:
                AddStat(ITEM_SPEC_STAT_INTELLECT);
                break;
            case ITEM_MOD_DODGE_RATING:
                AddStat(ITEM_SPEC_STAT_DODGE);
                break;
            case ITEM_MOD_PARRY_RATING:
                AddStat(ITEM_SPEC_STAT_PARRY);
                break;
            case ITEM_MOD_CRIT_MELEE_RATING:
            case ITEM_MOD_CRIT_RANGED_RATING:
            case ITEM_MOD_CRIT_SPELL_RATING:
            case ITEM_MOD_CRIT_RATING:
                AddStat(ITEM_SPEC_STAT_CRIT);
                break;
            case ITEM_MOD_HASTE_RATING:
                AddStat(ITEM_SPEC_STAT_HASTE);
                break;
            case ITEM_MOD_HIT_RATING:
                AddStat(ITEM_SPEC_STAT_HIT);
                break;
            case ITEM_MOD_EXTRA_ARMOR:
                AddStat(ITEM_SPEC_STAT_BONUS_ARMOR);
                break;
            case ITEM_MOD_AGI_STR_INT:
                AddStat(ITEM_SPEC_STAT_AGILITY);
                AddStat(ITEM_SPEC_STAT_STRENGTH);
                AddStat(ITEM_SPEC_STAT_INTELLECT);
                break;
            case ITEM_MOD_AGI_STR:
                AddStat(ITEM_SPEC_STAT_AGILITY);
                AddStat(ITEM_SPEC_STAT_STRENGTH);
                break;
            case ITEM_MOD_AGI_INT:
                AddStat(ITEM_SPEC_STAT_AGILITY);
                AddStat(ITEM_SPEC_STAT_INTELLECT);
                break;
            case ITEM_MOD_STR_INT:
                AddStat(ITEM_SPEC_STAT_STRENGTH);
                AddStat(ITEM_SPEC_STAT_INTELLECT);
                break;
        }
    }
};

void ObjectMgr::LoadItemTemplates()
{
    uint32 oldMSTime = getMSTime();
    uint32 sparseCount = 0;

    for (ItemSparseEntry const* sparse : sItemSparseStore)
    {
        ItemEntry const* db2Data = sItemStore.LookupEntry(sparse->ID);
        if (!db2Data)
            continue;

        ItemTemplate& itemTemplate = _itemTemplateStore[sparse->ID];

        itemTemplate.BasicData = db2Data;
        itemTemplate.ExtendedData = sparse;

        itemTemplate.MaxDurability = FillMaxDurability(db2Data->ClassID, db2Data->SubclassID, sparse->InventoryType, sparse->OverallQualityID, sparse->ItemLevel);
        itemTemplate.ScriptId = 0;
        itemTemplate.FoodType = 0;
        itemTemplate.MinMoneyLoot = 0;
        itemTemplate.MaxMoneyLoot = 0;
        itemTemplate.FlagsCu = 0;
        itemTemplate.SpellPPMRate = 0.0f;
        itemTemplate.RandomBonusListTemplateId = 0;
        itemTemplate.ItemSpecClassMask = 0;

        if (std::vector<ItemSpecOverrideEntry const*> const* itemSpecOverrides = sDB2Manager.GetItemSpecOverrides(sparse->ID))
        {
            for (ItemSpecOverrideEntry const* itemSpecOverride : *itemSpecOverrides)
            {
                if (ChrSpecializationEntry const* specialization = sChrSpecializationStore.LookupEntry(itemSpecOverride->SpecID))
                {
                    itemTemplate.ItemSpecClassMask |= 1 << (specialization->ClassID - 1);
                    itemTemplate.Specializations[0].set(ItemTemplate::CalculateItemSpecBit(specialization));
                    itemTemplate.Specializations[1] |= itemTemplate.Specializations[0];
                    itemTemplate.Specializations[2] |= itemTemplate.Specializations[0];
                }
            }
        }
        else
        {
            ItemSpecStats itemSpecStats(db2Data, sparse);

            for (ItemSpecEntry const* itemSpec : sItemSpecStore)
            {
                if (itemSpecStats.ItemType != itemSpec->ItemType)
                    continue;

                bool hasPrimary = itemSpec->PrimaryStat == ITEM_SPEC_STAT_NONE;
                bool hasSecondary = itemSpec->SecondaryStat == ITEM_SPEC_STAT_NONE;
                for (uint32 i = 0; i < itemSpecStats.ItemSpecStatCount; ++i)
                {
                    if (itemSpecStats.ItemSpecStatTypes[i] == itemSpec->PrimaryStat)
                        hasPrimary = true;
                    if (itemSpecStats.ItemSpecStatTypes[i] == itemSpec->SecondaryStat)
                        hasSecondary = true;
                }

                if (!hasPrimary || !hasSecondary)
                    continue;

                if (ChrSpecializationEntry const* specialization = sChrSpecializationStore.LookupEntry(itemSpec->SpecializationID))
                {
                    if ((1 << (specialization->ClassID - 1)) & sparse->AllowableClass)
                    {
                        itemTemplate.ItemSpecClassMask |= 1 << (specialization->ClassID - 1);
                        std::size_t specBit = ItemTemplate::CalculateItemSpecBit(specialization);
                        itemTemplate.Specializations[0].set(specBit);
                        if (itemSpec->MaxLevel > 40)
                            itemTemplate.Specializations[1].set(specBit);
                        if (itemSpec->MaxLevel >= 110)
                            itemTemplate.Specializations[2].set(specBit);
                    }
                }
            }
        }

        // Items that have no specializations set can be used by everyone
        for (auto& specs : itemTemplate.Specializations)
            if (specs.count() == 0)
                specs.set();

        ++sparseCount;
    }

    // Load item effects (spells)
    for (ItemXItemEffectEntry const* effectEntry : sItemXItemEffectStore)
        if (ItemTemplate* item = Trinity::Containers::MapGetValuePtr(_itemTemplateStore, effectEntry->ItemID))
            if (ItemEffectEntry const* effect = sItemEffectStore.LookupEntry(effectEntry->ItemEffectID))
                item->Effects.push_back(effect);

    TC_LOG_INFO("server.loading", ">> Loaded " SZFMTD " item templates in %u ms", _itemTemplateStore.size(), GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadItemTemplateAddon()
{
    uint32 oldMSTime = getMSTime();
    uint32 count = 0;

    QueryResult result = WorldDatabase.Query("SELECT Id, FlagsCu, FoodType, MinMoneyLoot, MaxMoneyLoot, SpellPPMChance, RandomBonusListTemplateId FROM item_template_addon");
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 itemId = fields[0].GetUInt32();
            ItemTemplate* itemTemplate = const_cast<ItemTemplate*>(GetItemTemplate(itemId));
            if (!itemTemplate)
            {
                TC_LOG_ERROR("sql.sql", "Item %u specified in `item_template_addon` does not exist, skipped.", itemId);
                continue;
            }

            uint32 minMoneyLoot = fields[3].GetUInt32();
            uint32 maxMoneyLoot = fields[4].GetUInt32();
            if (minMoneyLoot > maxMoneyLoot)
            {
                TC_LOG_ERROR("sql.sql", "Minimum money loot specified in `item_template_addon` for item %u was greater than maximum amount, swapping.", itemId);
                std::swap(minMoneyLoot, maxMoneyLoot);
            }
            itemTemplate->FlagsCu = fields[1].GetUInt32();
            itemTemplate->FoodType = fields[2].GetUInt8();
            itemTemplate->MinMoneyLoot = minMoneyLoot;
            itemTemplate->MaxMoneyLoot = maxMoneyLoot;
            itemTemplate->SpellPPMRate = fields[5].GetFloat();
            itemTemplate->RandomBonusListTemplateId = fields[6].GetUInt32();
            ++count;
        } while (result->NextRow());
    }
    TC_LOG_INFO("server.loading", ">> Loaded %u item addon templates in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadItemScriptNames()
{
    uint32 oldMSTime = getMSTime();
    uint32 count = 0;

    QueryResult result = WorldDatabase.Query("SELECT Id, ScriptName FROM item_script_names");
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 itemId = fields[0].GetUInt32();
            ItemTemplate* itemTemplate = const_cast<ItemTemplate*>(GetItemTemplate(itemId));
            if (!itemTemplate)
            {
                TC_LOG_ERROR("sql.sql", "Item %u specified in `item_script_names` does not exist, skipped.", itemId);
                continue;
            }

            itemTemplate->ScriptId = GetScriptId(fields[1].GetString());
            ++count;
        } while (result->NextRow());
    }

    TC_LOG_INFO("server.loading", ">> Loaded %u item script names in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

ItemTemplate const* ObjectMgr::GetItemTemplate(uint32 entry) const
{
    return Trinity::Containers::MapGetValuePtr(_itemTemplateStore, entry);
}

void ObjectMgr::LoadVehicleTemplateAccessories()
{
    uint32 oldMSTime = getMSTime();

    _vehicleTemplateAccessoryStore.clear();                           // needed for reload case

    uint32 count = 0;

    //                                                  0             1              2          3           4             5
    QueryResult result = WorldDatabase.Query("SELECT `entry`, `accessory_entry`, `seat_id`, `minion`, `summontype`, `summontimer` FROM `vehicle_template_accessory`");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 vehicle template accessories. DB table `vehicle_template_accessory` is empty.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        uint32 entry        = fields[0].GetUInt32();
        uint32 accessory    = fields[1].GetUInt32();
        int8   seatId       = fields[2].GetInt8();
        bool   isMinion     = fields[3].GetBool();
        uint8  summonType   = fields[4].GetUInt8();
        uint32 summonTimer  = fields[5].GetUInt32();

        if (!sObjectMgr->GetCreatureTemplate(entry))
        {
            TC_LOG_ERROR("sql.sql", "Table `vehicle_template_accessory`: creature template entry %u does not exist.", entry);
            continue;
        }

        if (!sObjectMgr->GetCreatureTemplate(accessory))
        {
            TC_LOG_ERROR("sql.sql", "Table `vehicle_template_accessory`: Accessory %u does not exist.", accessory);
            continue;
        }

        if (_spellClickInfoStore.find(entry) == _spellClickInfoStore.end())
        {
            TC_LOG_ERROR("sql.sql", "Table `vehicle_template_accessory`: creature template entry %u has no data in npc_spellclick_spells", entry);
            continue;
        }

        _vehicleTemplateAccessoryStore[entry].push_back(VehicleAccessory(accessory, seatId, isMinion, summonType, summonTimer));

        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u Vehicle Template Accessories in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadVehicleTemplate()
{
    uint32 oldMSTime = getMSTime();

    _vehicleTemplateStore.clear();

    //                                               0           1
    QueryResult result = WorldDatabase.Query("SELECT creatureId, despawnDelayMs FROM vehicle_template");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 vehicle template. DB table `vehicle_template` is empty.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        uint32 creatureId = fields[0].GetUInt32();

        if (!sObjectMgr->GetCreatureTemplate(creatureId))
        {
            TC_LOG_ERROR("sql.sql", "Table `vehicle_template`: Vehicle %u does not exist.", creatureId);
            continue;
        }

        VehicleTemplate& vehicleTemplate = _vehicleTemplateStore[creatureId];
        vehicleTemplate.DespawnDelay = Milliseconds(fields[1].GetInt32());

    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded " SZFMTD " Vehicle Template entries in %u ms", _vehicleTemplateStore.size(), GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadVehicleAccessories()
{
    uint32 oldMSTime = getMSTime();

    _vehicleAccessoryStore.clear();                           // needed for reload case

    uint32 count = 0;

    //                                                  0             1             2          3           4             5
    QueryResult result = WorldDatabase.Query("SELECT `guid`, `accessory_entry`, `seat_id`, `minion`, `summontype`, `summontimer` FROM `vehicle_accessory`");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 Vehicle Accessories in %u ms", GetMSTimeDiffToNow(oldMSTime));
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        ObjectGuid::LowType uiGUID = fields[0].GetUInt64();
        uint32 uiAccessory  = fields[1].GetUInt32();
        int8   uiSeat       = int8(fields[2].GetInt16());
        bool   bMinion      = fields[3].GetBool();
        uint8  uiSummonType = fields[4].GetUInt8();
        uint32 uiSummonTimer= fields[5].GetUInt32();

        if (!sObjectMgr->GetCreatureTemplate(uiAccessory))
        {
            TC_LOG_ERROR("sql.sql", "Table `vehicle_accessory`: Accessory %u does not exist.", uiAccessory);
            continue;
        }

        _vehicleAccessoryStore[uiGUID].push_back(VehicleAccessory(uiAccessory, uiSeat, bMinion, uiSummonType, uiSummonTimer));

        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u Vehicle Accessories in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadPetLevelInfo()
{
    uint32 oldMSTime = getMSTime();

    //                                                 0               1      2   3     4    5    6    7     8    9
    QueryResult result = WorldDatabase.Query("SELECT creature_entry, level, hp, mana, str, agi, sta, inte, spi, armor FROM pet_levelstats");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 level pet stats definitions. DB table `pet_levelstats` is empty.");
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();

        uint32 creature_id = fields[0].GetUInt32();
        if (!sObjectMgr->GetCreatureTemplate(creature_id))
        {
            TC_LOG_ERROR("sql.sql", "Wrong creature id %u in `pet_levelstats` table, ignoring.", creature_id);
            continue;
        }

        uint32 current_level = fields[1].GetUInt8();
        if (current_level > sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL))
        {
            if (current_level > STRONG_MAX_LEVEL)        // hardcoded level maximum
                TC_LOG_ERROR("sql.sql", "Wrong (> %u) level %u in `pet_levelstats` table, ignoring.", STRONG_MAX_LEVEL, current_level);
            else
            {
                TC_LOG_INFO("misc", "Unused (> MaxPlayerLevel in worldserver.conf) level %u in `pet_levelstats` table, ignoring.", current_level);
                ++count;                                // make result loading percent "expected" correct in case disabled detail mode for example.
            }
            continue;
        }
        else if (current_level < 1)
        {
            TC_LOG_ERROR("sql.sql", "Wrong (<1) level %u in `pet_levelstats` table, ignoring.", current_level);
            continue;
        }

        auto& pInfoMapEntry = _petInfoStore[creature_id];
        if (!pInfoMapEntry)
            pInfoMapEntry = std::make_unique<PetLevelInfo[]>(sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL));

        // data for level 1 stored in [0] array element, ...
        PetLevelInfo* pLevelInfo = &pInfoMapEntry[current_level - 1];

        pLevelInfo->health = fields[2].GetUInt16();
        pLevelInfo->mana   = fields[3].GetUInt16();
        pLevelInfo->armor  = fields[9].GetUInt32();

        for (uint8 i = 0; i < MAX_STATS; i++)
            pLevelInfo->stats[i] = fields[i + 4].GetUInt16();

        ++count;
    }
    while (result->NextRow());

    // Fill gaps and check integrity
    for (PetLevelInfoContainer::iterator itr = _petInfoStore.begin(); itr != _petInfoStore.end(); ++itr)
    {
        auto& pInfo = itr->second;

        // fatal error if no level 1 data
        if (!pInfo || pInfo[0].health == 0)
        {
            TC_LOG_ERROR("sql.sql", "Creature %u does not have pet stats data for Level 1!", itr->first);
            ABORT();
        }

        // fill level gaps
        for (uint8 level = 1; level < sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL); ++level)
        {
            if (pInfo[level].health == 0)
            {
                TC_LOG_ERROR("sql.sql", "Creature %u has no data for Level %i pet stats data, using data of Level %i.", itr->first, level + 1, level);
                pInfo[level] = pInfo[level - 1];
            }
        }
    }

    TC_LOG_INFO("server.loading", ">> Loaded %u level pet stats definitions in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

PetLevelInfo const* ObjectMgr::GetPetLevelInfo(uint32 creature_id, uint8 level) const
{
    if (level > sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL))
        level = sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL);

    auto itr = _petInfoStore.find(creature_id);
    if (itr == _petInfoStore.end())
        return nullptr;

    return &itr->second[level - 1];                         // data for level 1 stored in [0] array element, ...
}

void ObjectMgr::PlayerCreateInfoAddItemHelper(uint32 race_, uint32 class_, uint32 itemId, int32 count)
{
    if (!_playerInfo[race_][class_])
        return;

    if (count > 0)
        _playerInfo[race_][class_]->item.emplace_back(itemId, count);
    else
    {
        if (count < -1)
            TC_LOG_ERROR("sql.sql", "Invalid count %i specified on item %u be removed from original player create info (use -1)!", count, itemId);

        PlayerCreateInfoItems& items = _playerInfo[race_][class_]->item;

        auto erased = std::remove_if(items.begin(), items.end(), [itemId](PlayerCreateInfoItem const& item) { return item.item_id == itemId; });
        if (erased == items.end())
        {
            TC_LOG_ERROR("sql.sql", "Item %u specified to be removed from original create info not found in db2!", itemId);
            return;
        }

        items.erase(erased, items.end());
    }
}

void ObjectMgr::LoadPlayerInfo()
{
    // Load playercreate
    {
        uint32 oldMSTime = getMSTime();
        //                                                0     1      2       3           4           5           6           7           8               9               10              11                 12                13              14                15
        QueryResult result = WorldDatabase.Query("SELECT race, class, map, position_x, position_y, position_z, orientation, npe_map, npe_position_x, npe_position_y, npe_position_z, npe_orientation, npe_transport_guid, intro_movie_id, intro_scene_id, npe_intro_scene_id FROM playercreateinfo");

        if (!result)
        {
            TC_LOG_ERROR("server.loading", ">> Loaded 0 player create definitions. DB table `playercreateinfo` is empty.");
            ABORT();
        }
        else
        {
            uint32 count = 0;

            do
            {
                Field* fields = result->Fetch();

                uint32 current_race  = fields[0].GetUInt8();
                uint32 current_class = fields[1].GetUInt8();
                uint32 mapId         = fields[2].GetUInt16();
                float  positionX     = fields[3].GetFloat();
                float  positionY     = fields[4].GetFloat();
                float  positionZ     = fields[5].GetFloat();
                float  orientation   = fields[6].GetFloat();

                if (!sChrRacesStore.LookupEntry(current_race))
                {
                    TC_LOG_ERROR("sql.sql", "Wrong race %u in `playercreateinfo` table, ignoring.", current_race);
                    continue;
                }

                if (!sChrClassesStore.LookupEntry(current_class))
                {
                    TC_LOG_ERROR("sql.sql", "Wrong class %u in `playercreateinfo` table, ignoring.", current_class);
                    continue;
                }

                // accept DB data only for valid position (and non instanceable)
                if (!MapManager::IsValidMapCoord(mapId, positionX, positionY, positionZ, orientation))
                {
                    TC_LOG_ERROR("sql.sql", "Wrong home position for class %u race %u pair in `playercreateinfo` table, ignoring.", current_class, current_race);
                    continue;
                }

                if (sMapStore.LookupEntry(mapId)->Instanceable())
                {
                    TC_LOG_ERROR("sql.sql", "Home position in instanceable map for class %u race %u pair in `playercreateinfo` table, ignoring.", current_class, current_race);
                    continue;
                }

                if (!sDB2Manager.GetChrModel(current_race, GENDER_MALE))
                {
                    TC_LOG_ERROR("sql.sql", "Missing male model for race %u, ignoring.", current_race);
                    continue;
                }

                if (!sDB2Manager.GetChrModel(current_race, GENDER_FEMALE))
                {
                    TC_LOG_ERROR("sql.sql", "Missing female model for race %u, ignoring.", current_race);
                    continue;
                }

                std::unique_ptr<PlayerInfo> info = std::make_unique<PlayerInfo>();
                info->createPosition.Loc.WorldRelocate(mapId, positionX, positionY, positionZ, orientation);

                if (std::none_of(fields + 7, fields + 12, [](Field const& field) { return field.IsNull(); }))
                {
                    info->createPositionNPE.emplace();

                    info->createPositionNPE->Loc.WorldRelocate(fields[7].GetUInt32(), fields[8].GetFloat(), fields[9].GetFloat(), fields[10].GetFloat(), fields[11].GetFloat());
                    if (!fields[12].IsNull())
                        info->createPositionNPE->TransportGuid = fields[12].GetUInt64();

                    if (!sMapStore.LookupEntry(info->createPositionNPE->Loc.GetMapId()))
                    {
                        TC_LOG_ERROR("sql.sql", "Invalid NPE map id %u for class %u race %u pair in `playercreateinfo` table, ignoring.",
                            info->createPositionNPE->Loc.GetMapId(), current_class, current_race);
                        info->createPositionNPE.reset();
                    }

                    if (info->createPositionNPE && info->createPositionNPE->TransportGuid && !sTransportMgr->GetTransportSpawn(*info->createPositionNPE->TransportGuid))
                    {
                        TC_LOG_ERROR("sql.sql", "Invalid NPE transport spawn id " UI64FMTD " for class %u race %u pair in `playercreateinfo` table, ignoring.",
                            *info->createPositionNPE->TransportGuid, current_class, current_race);
                        info->createPositionNPE.reset(); // remove entire NPE data - assume user put transport offsets into npe_position fields
                    }
                }

                if (!fields[13].IsNull())
                {
                    uint32 introMovieId = fields[13].GetUInt32();
                    if (sMovieStore.LookupEntry(introMovieId))
                        info->introMovieId = introMovieId;
                    else
                        TC_LOG_ERROR("sql.sql", "Invalid intro movie id %u for class %u race %u pair in `playercreateinfo` table, ignoring.",
                            introMovieId, current_class, current_race);
                }

                if (!fields[14].IsNull())
                {
                    uint32 introSceneId = fields[14].GetUInt32();
                    if (GetSceneTemplate(introSceneId))
                        info->introSceneId = introSceneId;
                    else
                        TC_LOG_ERROR("sql.sql", "Invalid intro scene id %u for class %u race %u pair in `playercreateinfo` table, ignoring.",
                            introSceneId, current_class, current_race);
                }

                if (!fields[15].IsNull())
                {
                    uint32 introSceneId = fields[15].GetUInt32();
                    if (GetSceneTemplate(introSceneId))
                        info->introSceneIdNPE = introSceneId;
                    else
                        TC_LOG_ERROR("sql.sql", "Invalid NPE intro scene id %u for class %u race %u pair in `playercreateinfo` table, ignoring.",
                            introSceneId, current_class, current_race);
                }

                _playerInfo[current_race][current_class] = std::move(info);

                ++count;
            }
            while (result->NextRow());

            TC_LOG_INFO("server.loading", ">> Loaded %u player create definitions in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
        }
    }

    // Load playercreate items
    TC_LOG_INFO("server.loading", "Loading Player Create Items Data...");
    {
        std::unordered_map<uint32, std::vector<ItemTemplate const*>> itemsByCharacterLoadout;
        for (CharacterLoadoutItemEntry const* characterLoadoutItem : sCharacterLoadoutItemStore)
            if (ItemTemplate const* itemTemplate = GetItemTemplate(characterLoadoutItem->ItemID))
                itemsByCharacterLoadout[characterLoadoutItem->CharacterLoadoutID].push_back(itemTemplate);

        for (CharacterLoadoutEntry const* characterLoadout : sCharacterLoadoutStore)
        {
            if (!characterLoadout->IsForNewCharacter())
                continue;

            std::vector<ItemTemplate const*> const* items = Trinity::Containers::MapGetValuePtr(itemsByCharacterLoadout, characterLoadout->ID);
            if (!items)
                continue;

            for (uint32 raceIndex = RACE_HUMAN; raceIndex < MAX_RACES; ++raceIndex)
            {
                if (!characterLoadout->RaceMask.HasRace(raceIndex))
                    continue;

                if (auto& playerInfo = _playerInfo[raceIndex][characterLoadout->ChrClassID])
                {
                    for (ItemTemplate const* itemTemplate : *items)
                    {
                        // BuyCount by default
                        uint32 count = itemTemplate->GetBuyCount();

                        // special amount for food/drink
                        if (itemTemplate->GetClass() == ITEM_CLASS_CONSUMABLE && itemTemplate->GetSubClass() == ITEM_SUBCLASS_FOOD_DRINK)
                        {
                            if (!itemTemplate->Effects.empty())
                            {
                                switch (itemTemplate->Effects[0]->SpellCategoryID)
                                {
                                    case SPELL_CATEGORY_FOOD:                                // food
                                        count = characterLoadout->ChrClassID == CLASS_DEATH_KNIGHT ? 10 : 4;
                                        break;
                                    case SPELL_CATEGORY_DRINK:                                // drink
                                        count = 2;
                                        break;
                                }
                            }
                            if (itemTemplate->GetMaxStackSize() < count)
                                count = itemTemplate->GetMaxStackSize();
                        }

                        playerInfo->item.emplace_back(itemTemplate->GetId(), count);
                    }
                }
            }
        }
    }

    TC_LOG_INFO("server.loading", "Loading Player Create Items Override Data...");
    {
        uint32 oldMSTime = getMSTime();
        //                                                0     1      2       3
        QueryResult result = WorldDatabase.Query("SELECT race, class, itemid, amount FROM playercreateinfo_item");

        if (!result)
        {
            TC_LOG_INFO("server.loading", ">> Loaded 0 custom player create items. DB table `playercreateinfo_item` is empty.");
        }
        else
        {
            uint32 count = 0;

            do
            {
                Field* fields = result->Fetch();

                uint32 current_race = fields[0].GetUInt8();
                if (current_race >= MAX_RACES)
                {
                    TC_LOG_ERROR("sql.sql", "Wrong race %u in `playercreateinfo_item` table, ignoring.", current_race);
                    continue;
                }

                uint32 current_class = fields[1].GetUInt8();
                if (current_class >= MAX_CLASSES)
                {
                    TC_LOG_ERROR("sql.sql", "Wrong class %u in `playercreateinfo_item` table, ignoring.", current_class);
                    continue;
                }

                uint32 item_id = fields[2].GetUInt32();

                if (!GetItemTemplate(item_id))
                {
                    TC_LOG_ERROR("sql.sql", "Item id %u (race %u class %u) in `playercreateinfo_item` table but it does not exist, ignoring.", item_id, current_race, current_class);
                    continue;
                }

                int32 amount   = fields[3].GetInt8();

                if (!amount)
                {
                    TC_LOG_ERROR("sql.sql", "Item id %u (class %u race %u) have amount == 0 in `playercreateinfo_item` table, ignoring.", item_id, current_race, current_class);
                    continue;
                }

                if (!current_race || !current_class)
                {
                    uint32 min_race = current_race ? current_race : 1;
                    uint32 max_race = current_race ? current_race + 1 : MAX_RACES;
                    uint32 min_class = current_class ? current_class : 1;
                    uint32 max_class = current_class ? current_class + 1 : MAX_CLASSES;
                    for (uint32 r = min_race; r < max_race; ++r)
                        for (uint32 c = min_class; c < max_class; ++c)
                            PlayerCreateInfoAddItemHelper(r, c, item_id, amount);
                }
                else
                    PlayerCreateInfoAddItemHelper(current_race, current_class, item_id, amount);

                ++count;
            }
            while (result->NextRow());

            TC_LOG_INFO("server.loading", ">> Loaded %u custom player create items in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
        }
    }

    // Load playercreate skills
    TC_LOG_INFO("server.loading", "Loading Player Create Skill Data...");
    {
        uint32 oldMSTime = getMSTime();

        for (SkillRaceClassInfoEntry const* rcInfo : sSkillRaceClassInfoStore)
            if (rcInfo->Availability == 1)
                for (uint32 raceIndex = RACE_HUMAN; raceIndex < MAX_RACES; ++raceIndex)
                    if (rcInfo->RaceMask.HasRace(raceIndex))
                        for (uint32 classIndex = CLASS_WARRIOR; classIndex < MAX_CLASSES; ++classIndex)
                            if (rcInfo->ClassMask == -1 || ((1 << (classIndex - 1)) & rcInfo->ClassMask))
                                if (auto& info = _playerInfo[raceIndex][classIndex])
                                    info->skills.push_back(rcInfo);

        TC_LOG_INFO("server.loading", ">> Loaded player create skills in %u ms", GetMSTimeDiffToNow(oldMSTime));
    }

    // Load playercreate custom spells
    TC_LOG_INFO("server.loading", "Loading Player Create Custom Spell Data...");
    {
        uint32 oldMSTime = getMSTime();

        QueryResult result = WorldDatabase.PQuery("SELECT racemask, classmask, Spell FROM playercreateinfo_spell_custom");

        if (!result)
        {
            TC_LOG_INFO("server.loading", ">> Loaded 0 player create custom spells. DB table `playercreateinfo_spell_custom` is empty.");
        }
        else
        {
            uint32 count = 0;

            do
            {
                Field* fields = result->Fetch();
                Trinity::RaceMask<uint64> raceMask = { fields[0].GetUInt64() };
                uint32 classMask = fields[1].GetUInt32();
                uint32 spellId = fields[2].GetUInt32();

                if (raceMask && !(raceMask.RawValue & RACEMASK_ALL_PLAYABLE))
                {
                    TC_LOG_ERROR("sql.sql", "Wrong race mask " UI64FMTD " in `playercreateinfo_spell_custom` table, ignoring.", raceMask.RawValue);
                    continue;
                }

                if (classMask != 0 && !(classMask & CLASSMASK_ALL_PLAYABLE))
                {
                    TC_LOG_ERROR("sql.sql", "Wrong class mask %u in `playercreateinfo_spell_custom` table, ignoring.", classMask);
                    continue;
                }

                for (uint32 raceIndex = RACE_HUMAN; raceIndex < MAX_RACES; ++raceIndex)
                {
                    if (!raceMask || raceMask.HasRace(raceIndex))
                    {
                        for (uint32 classIndex = CLASS_WARRIOR; classIndex < MAX_CLASSES; ++classIndex)
                        {
                            if (classMask == 0 || ((1 << (classIndex - 1)) & classMask))
                            {
                                if (auto& info = _playerInfo[raceIndex][classIndex])
                                {
                                    info->customSpells.push_back(spellId);
                                    ++count;
                                }
                                // We need something better here, the check is not accounting for spells used by multiple races/classes but not all of them.
                                // Either split the masks per class, or per race, which kind of kills the point yet.
                                // else if (raceMask != 0 && classMask != 0)
                                //     TC_LOG_ERROR("sql.sql", "Racemask/classmask (%u/%u) combination was found containing an invalid race/class combination (%u/%u) in `%s` (Spell %u), ignoring.", raceMask, classMask, raceIndex, classIndex, tableName.c_str(), spellId);
                            }
                        }
                    }
                }
            }
            while (result->NextRow());

            TC_LOG_INFO("server.loading", ">> Loaded %u custom player create spells in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
        }
    }

    // Load playercreate cast spell
    TC_LOG_INFO("server.loading", "Loading Player Create Cast Spell Data...");
    {
        uint32 oldMSTime = getMSTime();

        QueryResult result = WorldDatabase.PQuery("SELECT raceMask, classMask, spell FROM playercreateinfo_cast_spell");

        if (!result)
            TC_LOG_INFO("server.loading", ">> Loaded 0 player create cast spells. DB table `playercreateinfo_cast_spell` is empty.");
        else
        {
            uint32 count = 0;

            do
            {
                Field* fields       = result->Fetch();
                Trinity::RaceMask<uint64> raceMask = { fields[0].GetUInt64() };
                uint32 classMask    = fields[1].GetUInt32();
                uint32 spellId      = fields[2].GetUInt32();

                if (raceMask && !(raceMask.RawValue & RACEMASK_ALL_PLAYABLE))
                {
                    TC_LOG_ERROR("sql.sql", "Wrong race mask " UI64FMTD " in `playercreateinfo_cast_spell` table, ignoring.", raceMask.RawValue);
                    continue;
                }

                if (classMask != 0 && !(classMask & CLASSMASK_ALL_PLAYABLE))
                {
                    TC_LOG_ERROR("sql.sql", "Wrong class mask %u in `playercreateinfo_cast_spell` table, ignoring.", classMask);
                    continue;
                }

                for (uint32 raceIndex = RACE_HUMAN; raceIndex < MAX_RACES; ++raceIndex)
                {
                    if (!raceMask || raceMask.HasRace(raceIndex))
                    {
                        for (uint32 classIndex = CLASS_WARRIOR; classIndex < MAX_CLASSES; ++classIndex)
                        {
                            if (classMask == 0 || ((1 << (classIndex - 1)) & classMask))
                            {
                                if (auto& info = _playerInfo[raceIndex][classIndex])
                                {
                                    info->castSpells.push_back(spellId);
                                    ++count;
                                }
                            }
                        }
                    }
                }
            } while (result->NextRow());

            TC_LOG_INFO("server.loading", ">> Loaded %u player create cast spells in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
        }
    }

    // Load playercreate actions
    TC_LOG_INFO("server.loading", "Loading Player Create Action Data...");
    {
        uint32 oldMSTime = getMSTime();

        //                                                0     1      2       3       4
        QueryResult result = WorldDatabase.Query("SELECT race, class, button, action, type FROM playercreateinfo_action");

        if (!result)
        {
            TC_LOG_INFO("server.loading", ">> Loaded 0 player create actions. DB table `playercreateinfo_action` is empty.");
        }
        else
        {
            uint32 count = 0;

            do
            {
                Field* fields = result->Fetch();

                uint32 current_race = fields[0].GetUInt8();
                if (current_race >= MAX_RACES)
                {
                    TC_LOG_ERROR("sql.sql", "Wrong race %u in `playercreateinfo_action` table, ignoring.", current_race);
                    continue;
                }

                uint32 current_class = fields[1].GetUInt8();
                if (current_class >= MAX_CLASSES)
                {
                    TC_LOG_ERROR("sql.sql", "Wrong class %u in `playercreateinfo_action` table, ignoring.", current_class);
                    continue;
                }

                if (auto& info = _playerInfo[current_race][current_class])
                    info->action.push_back(PlayerCreateInfoAction(fields[2].GetUInt16(), fields[3].GetUInt32(), fields[4].GetUInt16()));

                ++count;
            }
            while (result->NextRow());

            TC_LOG_INFO("server.loading", ">> Loaded %u player create actions in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
        }
    }

    // Loading levels data (class/race dependent)
    TC_LOG_INFO("server.loading", "Loading Player Create Level Stats Data...");
    {
        struct RaceStats
        {
            std::array<int16, MAX_STATS> StatModifier = { };
        };

        std::array<RaceStats, MAX_RACES> raceStatModifiers = { };

        uint32 oldMSTime = getMSTime();

        QueryResult raceStatsResult = WorldDatabase.Query("SELECT race, str, agi, sta, inte FROM player_racestats");

        if (!raceStatsResult)
        {
            TC_LOG_ERROR("server.loading", ">> Loaded 0 race stats definitions. DB table `player_racestats` is empty.");
            ABORT();
        }

        do
        {
            Field* fields = raceStatsResult->Fetch();

            uint32 current_race = fields[0].GetUInt8();
            if (current_race >= MAX_RACES)
            {
                TC_LOG_ERROR("sql.sql", "Wrong race %u in `player_racestats` table, ignoring.", current_race);
                continue;
            }

            for (uint32 i = 0; i < MAX_STATS; ++i)
                raceStatModifiers[current_race].StatModifier[i] = fields[i + 1].GetInt16();

        } while (raceStatsResult->NextRow());

        //                                                  0      1     2    3    4    5
        QueryResult result  = WorldDatabase.Query("SELECT class, level, str, agi, sta, inte FROM player_classlevelstats");

        if (!result)
        {
            TC_LOG_ERROR("server.loading", ">> Loaded 0 level stats definitions. DB table `player_classlevelstats` is empty.");
            ABORT();
        }

        uint32 count = 0;

        do
        {
            Field* fields = result->Fetch();

            uint32 current_class = fields[0].GetUInt8();
            if (current_class >= MAX_CLASSES)
            {
                TC_LOG_ERROR("sql.sql", "Wrong class %u in `player_classlevelstats` table, ignoring.", current_class);
                continue;
            }

            uint32 current_level = fields[1].GetUInt8();
            if (current_level > sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL))
            {
                if (current_level > STRONG_MAX_LEVEL)        // hardcoded level maximum
                    TC_LOG_ERROR("sql.sql", "Wrong (> %u) level %u in `player_classlevelstats` table, ignoring.", STRONG_MAX_LEVEL, current_level);
                else
                    TC_LOG_INFO("misc", "Unused (> MaxPlayerLevel in worldserver.conf) level %u in `player_classlevelstats` table, ignoring.", current_level);

                continue;
            }

            for (std::size_t race = 0; race < raceStatModifiers.size(); ++race)
            {
                if (auto& info = _playerInfo[race][current_class])
                {
                    if (!info->levelInfo)
                        info->levelInfo = std::make_unique<PlayerLevelInfo[]>(sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL));

                    PlayerLevelInfo& levelInfo = info->levelInfo[current_level - 1];
                    for (uint8 i = 0; i < MAX_STATS; ++i)
                        levelInfo.stats[i] = fields[i + 2].GetUInt16() + raceStatModifiers[race].StatModifier[i];
                }
            }

            ++count;
        }
        while (result->NextRow());

        // Fill gaps and check integrity
        for (uint8 race = 0; race < MAX_RACES; ++race)
        {
            // skip non existed races
            if (!sChrRacesStore.LookupEntry(race))
                continue;

            for (uint8 class_ = 0; class_ < MAX_CLASSES; ++class_)
            {
                // skip non existed classes
                if (!sChrClassesStore.LookupEntry(class_))
                    continue;

                auto& info = _playerInfo[race][class_];
                if (!info)
                    continue;

                // skip expansion races if not playing with expansion
                if (sWorld->getIntConfig(CONFIG_EXPANSION) < EXPANSION_THE_BURNING_CRUSADE && (race == RACE_BLOODELF || race == RACE_DRAENEI))
                    continue;

                // skip expansion classes if not playing with expansion
                if (sWorld->getIntConfig(CONFIG_EXPANSION) < EXPANSION_WRATH_OF_THE_LICH_KING && class_ == CLASS_DEATH_KNIGHT)
                    continue;

                // skip expansion races if not playing with expansion
                if (sWorld->getIntConfig(CONFIG_EXPANSION) < EXPANSION_CATACLYSM && (race == RACE_GOBLIN || race == RACE_WORGEN))
                    continue;

                if (sWorld->getIntConfig(CONFIG_EXPANSION) < EXPANSION_MISTS_OF_PANDARIA && (race == RACE_PANDAREN_NEUTRAL || race == RACE_PANDAREN_HORDE || race == RACE_PANDAREN_ALLIANCE))
                    continue;

                if (sWorld->getIntConfig(CONFIG_EXPANSION) < EXPANSION_LEGION && class_ == CLASS_DEMON_HUNTER)
                    continue;

                // fatal error if no level 1 data
                if (!info->levelInfo || info->levelInfo[0].stats[0] == 0)
                {
                    TC_LOG_ERROR("sql.sql", "Race %i Class %i Level 1 does not have stats data!", race, class_);
                    ABORT();
                }

                // fill level gaps
                for (uint8 level = 1; level < sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL); ++level)
                {
                    if (info->levelInfo[level].stats[0] == 0)
                    {
                        TC_LOG_ERROR("sql.sql", "Race %i Class %i Level %i does not have stats data. Using stats data of level %i.", race, class_, level + 1, level);
                        info->levelInfo[level] = info->levelInfo[level - 1];
                    }
                }
            }
        }

        TC_LOG_INFO("server.loading", ">> Loaded %u level stats definitions in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
    }

    // Loading xp per level data
    TC_LOG_INFO("server.loading", "Loading Player Create XP Data...");
    {
        uint32 oldMSTime = getMSTime();

        _playerXPperLevel.resize(sXpGameTable.GetTableRowCount(), 0);

        //                                               0      1
        QueryResult result = WorldDatabase.Query("SELECT Level, Experience FROM player_xp_for_level");

        // load the DBC's levels at first...
        for (uint32 level = 1; level < sXpGameTable.GetTableRowCount(); ++level)
            _playerXPperLevel[level] = sXpGameTable.GetRow(level)->Total;

        uint32 count = 0;

        // ...overwrite if needed (custom values)
        if (result)
        {
            do
            {
                Field* fields = result->Fetch();

                uint32 current_level = fields[0].GetUInt8();
                uint32 current_xp = fields[1].GetUInt32();

                if (current_level >= sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL))
                {
                    if (current_level > STRONG_MAX_LEVEL)        // hardcoded level maximum
                        TC_LOG_ERROR("sql.sql", "Wrong (> %u) level %u in `player_xp_for_level` table, ignoring.", STRONG_MAX_LEVEL, current_level);
                    else
                    {
                        TC_LOG_INFO("misc", "Unused (> MaxPlayerLevel in worldserver.conf) level %u in `player_xp_for_level` table, ignoring.", current_level);
                        ++count;                                // make result loading percent "expected" correct in case disabled detail mode for example.
                    }
                    continue;
                }
                //PlayerXPperLevel
                _playerXPperLevel[current_level] = current_xp;
                ++count;
            } while (result->NextRow());
        }

        // fill level gaps - only accounting levels > MAX_LEVEL
        for (uint8 level = 1; level < sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL); ++level)
        {
            if (_playerXPperLevel[level] == 0)
            {
                TC_LOG_ERROR("sql.sql", "Level %i does not have XP for level data. Using data of level [%i] + 12000.", level + 1, level);
                _playerXPperLevel[level] = _playerXPperLevel[level - 1] + 12000;
            }
        }

        TC_LOG_INFO("server.loading", ">> Loaded %u xp for level definition(s) from database in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
    }
}

void ObjectMgr::GetPlayerClassLevelInfo(uint32 class_, uint8 level, uint32& baseMana) const
{
    if (level < 1 || class_ >= MAX_CLASSES)
        return;

    if (level > sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL))
        level = sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL);

    GtBaseMPEntry const* mp = sBaseMPGameTable.GetRow(level);
    if (!mp)
    {
        TC_LOG_ERROR("misc", "Tried to get non-existant Class-Level combination data for base hp/mp. Class %u Level %u", class_, level);
        return;
    }

    baseMana = uint32(GetGameTableColumnForClass(mp, class_));
}

void ObjectMgr::GetPlayerLevelInfo(uint32 race, uint32 class_, uint8 level, PlayerLevelInfo* info) const
{
    if (level < 1 || race >= MAX_RACES || class_ >= MAX_CLASSES)
        return;

    auto const& pInfo = _playerInfo[race][class_];
    if (!pInfo)
        return;

    if (level <= sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL))
        *info = pInfo->levelInfo[level - 1];
    else
        BuildPlayerLevelInfo(race, class_, level, info);
}

void ObjectMgr::BuildPlayerLevelInfo(uint8 race, uint8 _class, uint8 level, PlayerLevelInfo* info) const
{
    // base data (last known level)
    *info = _playerInfo[race][_class]->levelInfo[sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL) - 1];

    // if conversion from uint32 to uint8 causes unexpected behaviour, change lvl to uint32
    for (uint8 lvl = sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL) - 1; lvl < level; ++lvl)
    {
        switch (_class)
        {
            case CLASS_WARRIOR:
                info->stats[STAT_STRENGTH]  += (lvl > 23 ? 2: (lvl > 1  ? 1: 0));
                info->stats[STAT_STAMINA]   += (lvl > 23 ? 2: (lvl > 1  ? 1: 0));
                info->stats[STAT_AGILITY]   += (lvl > 36 ? 1: (lvl > 6 && (lvl%2) ? 1: 0));
                info->stats[STAT_INTELLECT] += (lvl > 9 && !(lvl%2) ? 1: 0);
                break;
            case CLASS_PALADIN:
                info->stats[STAT_STRENGTH]  += (lvl > 3  ? 1: 0);
                info->stats[STAT_STAMINA]   += (lvl > 33 ? 2: (lvl > 1 ? 1: 0));
                info->stats[STAT_AGILITY]   += (lvl > 38 ? 1: (lvl > 7 && !(lvl%2) ? 1: 0));
                info->stats[STAT_INTELLECT] += (lvl > 6 && (lvl%2) ? 1: 0);
                break;
            case CLASS_HUNTER:
                info->stats[STAT_STRENGTH]  += (lvl > 4  ? 1: 0);
                info->stats[STAT_STAMINA]   += (lvl > 4  ? 1: 0);
                info->stats[STAT_AGILITY]   += (lvl > 33 ? 2: (lvl > 1 ? 1: 0));
                info->stats[STAT_INTELLECT] += (lvl > 8 && (lvl%2) ? 1: 0);
                break;
            case CLASS_ROGUE:
                info->stats[STAT_STRENGTH]  += (lvl > 5  ? 1: 0);
                info->stats[STAT_STAMINA]   += (lvl > 4  ? 1: 0);
                info->stats[STAT_AGILITY]   += (lvl > 16 ? 2: (lvl > 1 ? 1: 0));
                info->stats[STAT_INTELLECT] += (lvl > 8 && !(lvl%2) ? 1: 0);
                break;
            case CLASS_PRIEST:
                info->stats[STAT_STRENGTH]  += (lvl > 9 && !(lvl%2) ? 1: 0);
                info->stats[STAT_STAMINA]   += (lvl > 5  ? 1: 0);
                info->stats[STAT_AGILITY]   += (lvl > 38 ? 1: (lvl > 8 && (lvl%2) ? 1: 0));
                info->stats[STAT_INTELLECT] += (lvl > 22 ? 2: (lvl > 1 ? 1: 0));
                break;
            case CLASS_SHAMAN:
                info->stats[STAT_STRENGTH]  += (lvl > 34 ? 1: (lvl > 6 && (lvl%2) ? 1: 0));
                info->stats[STAT_STAMINA]   += (lvl > 4 ? 1: 0);
                info->stats[STAT_AGILITY]   += (lvl > 7 && !(lvl%2) ? 1: 0);
                info->stats[STAT_INTELLECT] += (lvl > 5 ? 1: 0);
                break;
            case CLASS_MAGE:
                info->stats[STAT_STRENGTH]  += (lvl > 9 && !(lvl%2) ? 1: 0);
                info->stats[STAT_STAMINA]   += (lvl > 5  ? 1: 0);
                info->stats[STAT_AGILITY]   += (lvl > 9 && !(lvl%2) ? 1: 0);
                info->stats[STAT_INTELLECT] += (lvl > 24 ? 2: (lvl > 1 ? 1: 0));
                break;
            case CLASS_WARLOCK:
                info->stats[STAT_STRENGTH]  += (lvl > 9 && !(lvl%2) ? 1: 0);
                info->stats[STAT_STAMINA]   += (lvl > 38 ? 2: (lvl > 3 ? 1: 0));
                info->stats[STAT_AGILITY]   += (lvl > 9 && !(lvl%2) ? 1: 0);
                info->stats[STAT_INTELLECT] += (lvl > 33 ? 2: (lvl > 2 ? 1: 0));
                break;
            case CLASS_DRUID:
                info->stats[STAT_STRENGTH]  += (lvl > 38 ? 2: (lvl > 6 && (lvl%2) ? 1: 0));
                info->stats[STAT_STAMINA]   += (lvl > 32 ? 2: (lvl > 4 ? 1: 0));
                info->stats[STAT_AGILITY]   += (lvl > 38 ? 2: (lvl > 8 && (lvl%2) ? 1: 0));
                info->stats[STAT_INTELLECT] += (lvl > 38 ? 3: (lvl > 4 ? 1: 0));
                break;
        }
    }
}

void ObjectMgr::LoadQuests()
{
    uint32 oldMSTime = getMSTime();

    _questTemplates.clear();
    _questTemplatesAutoPush.clear();
    _questObjectives.clear();

    _exclusiveQuestGroups.clear();

    QueryResult result = WorldDatabase.Query("SELECT "
        //0  1          2               3                4            5            6                  7                8                   9
        "ID, QuestType, QuestPackageID, ContentTuningID, QuestSortID, QuestInfoID, SuggestedGroupNum, RewardNextQuest, RewardXPDifficulty, RewardXPMultiplier, "
        //10          11                     12                     13                14           15           16               17
        "RewardMoney, RewardMoneyDifficulty, RewardMoneyMultiplier, RewardBonusMoney, RewardSpell, RewardHonor, RewardKillHonor, StartItem, "
        //18                         19                          20                        21     22       23
        "RewardArtifactXPDifficulty, RewardArtifactXPMultiplier, RewardArtifactCategoryID, Flags, FlagsEx, FlagsEx2, "
        //24          25             26         27                 28           29             30         31
        "RewardItem1, RewardAmount1, ItemDrop1, ItemDropQuantity1, RewardItem2, RewardAmount2, ItemDrop2, ItemDropQuantity2, "
        //32          33             34         35                 36           37             38         39
        "RewardItem3, RewardAmount3, ItemDrop3, ItemDropQuantity3, RewardItem4, RewardAmount4, ItemDrop4, ItemDropQuantity4, "
        //40                  41                         42                          43                   44                         45
        "RewardChoiceItemID1, RewardChoiceItemQuantity1, RewardChoiceItemDisplayID1, RewardChoiceItemID2, RewardChoiceItemQuantity2, RewardChoiceItemDisplayID2, "
        //46                  47                         48                          49                   50                         51
        "RewardChoiceItemID3, RewardChoiceItemQuantity3, RewardChoiceItemDisplayID3, RewardChoiceItemID4, RewardChoiceItemQuantity4, RewardChoiceItemDisplayID4, "
        //52                  53                         54                          55                   56                         57
        "RewardChoiceItemID5, RewardChoiceItemQuantity5, RewardChoiceItemDisplayID5, RewardChoiceItemID6, RewardChoiceItemQuantity6, RewardChoiceItemDisplayID6, "
        //58           59    60    61           62           63                 64                 65
        "POIContinent, POIx, POIy, POIPriority, RewardTitle, RewardArenaPoints, RewardSkillLineID, RewardNumSkillUps, "
        //66            67                  68                         69
        "PortraitGiver, PortraitGiverMount, PortraitGiverModelSceneID, PortraitTurnIn, "
        //70               71                   72                      73                   74                75                   76                      77
        "RewardFactionID1, RewardFactionValue1, RewardFactionOverride1, RewardFactionCapIn1, RewardFactionID2, RewardFactionValue2, RewardFactionOverride2, RewardFactionCapIn2, "
        //78               79                   80                      81                   82                83                   84                      85
        "RewardFactionID3, RewardFactionValue3, RewardFactionOverride3, RewardFactionCapIn3, RewardFactionID4, RewardFactionValue4, RewardFactionOverride4, RewardFactionCapIn4, "
        //86               87                   88                      89                   90
        "RewardFactionID5, RewardFactionValue5, RewardFactionOverride5, RewardFactionCapIn5, RewardFactionFlags, "
        //91                92                  93                 94                  95                 96                  97                 98
        "RewardCurrencyID1, RewardCurrencyQty1, RewardCurrencyID2, RewardCurrencyQty2, RewardCurrencyID3, RewardCurrencyQty3, RewardCurrencyID4, RewardCurrencyQty4, "
        //99                 100                 101          102          103             104               105        106                  107
        "AcceptedSoundKitID, CompleteSoundKitID, AreaGroupID, TimeAllowed, AllowableRaces, TreasurePickerID, Expansion, ManagedWorldStateID, QuestSessionBonus, "
        //108      109             110               111              112                113                114                 115                 116
        "LogTitle, LogDescription, QuestDescription, AreaDescription, PortraitGiverText, PortraitGiverName, PortraitTurnInText, PortraitTurnInName, QuestCompletionLog"
        " FROM quest_template");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 quests definitions. DB table `quest_template` is empty.");
        return;
    }

    _questTemplates.reserve(result->GetRowCount());

    // create multimap previous quest for each existed quest
    // some quests can have many previous maps set by NextQuestId in previous quest
    // for example set of race quests can lead to single not race specific quest
    do
    {
        Field* fields = result->Fetch();

        uint32 questId = fields[0].GetUInt32();
        auto itr = _questTemplates.emplace(std::piecewise_construct, std::forward_as_tuple(questId), std::forward_as_tuple(fields)).first;
        if (itr->second.IsAutoPush())
            _questTemplatesAutoPush.push_back(&itr->second);
    } while (result->NextRow());

    struct QuestLoaderHelper
    {
        typedef void(Quest::* QuestLoaderFunction)(Field* fields);

        char const* QueryFields;
        char const* TableName;
        char const* QueryExtra;
        char const* TableDesc;
        QuestLoaderFunction LoaderFunction;
    };

    // QuestID needs to be fields[0]
    QuestLoaderHelper const QuestLoaderHelpers[] =
    {
        // 0        1      2      3      4      5      6
        { "QuestID, Type1, Type2, Type3, Type4, Type5, Type6",                                                                                                            "quest_reward_choice_items", "",                                  "reward choice items", &Quest::LoadRewardChoiceItems },

        // 0        1        2
        { "QuestID, SpellID, PlayerConditionID",                                                                                                                          "quest_reward_display_spell", "ORDER BY QuestID ASC, Idx ASC",    "reward display spells", &Quest::LoadRewardDisplaySpell },

        // 0   1       2       3       4       5            6            7            8
        { "ID, Emote1, Emote2, Emote3, Emote4, EmoteDelay1, EmoteDelay2, EmoteDelay3, EmoteDelay4",                                                                       "quest_details",        "",                                       "details",             &Quest::LoadQuestDetails       },

        // 0   1                2                  3                     4                       5
        { "ID, EmoteOnComplete, EmoteOnIncomplete, EmoteOnCompleteDelay, EmoteOnIncompleteDelay, CompletionText",                                                         "quest_request_items",  "",                                       "request items",       &Quest::LoadQuestRequestItems  },

        // 0   1       2       3       4       5            6            7            8            9
        { "ID, Emote1, Emote2, Emote3, Emote4, EmoteDelay1, EmoteDelay2, EmoteDelay3, EmoteDelay4, RewardText",                                                           "quest_offer_reward",   "",                                       "reward emotes",       &Quest::LoadQuestOfferReward   },

        // 0   1         2                 3              4            5            6               7                     8
        { "ID, MaxLevel, AllowableClasses, SourceSpellID, PrevQuestID, NextQuestID, ExclusiveGroup, RewardMailTemplateID, RewardMailDelay,"
        // 9               10                   11                     12                     13                   14                   15                 16
        " RequiredSkillID, RequiredSkillPoints, RequiredMinRepFaction, RequiredMaxRepFaction, RequiredMinRepValue, RequiredMaxRepValue, ProvidedItemCount, SpecialFlags,"
        // 17
        " ScriptName",                                                                                                                                                    "quest_template_addon", "",                                       "template addons",     &Quest::LoadQuestTemplateAddon },

        // 0        1
        { "QuestId, RewardMailSenderEntry",                                                                                                                               "quest_mail_sender",    "",                                       "mail sender entries", &Quest::LoadQuestMailSender    },

        // 0        1   2     3             4         5       6      7       8                  9
        { "QuestID, ID, Type, StorageIndex, ObjectID, Amount, Flags, Flags2, ProgressBarWeight, Description",                                                             "quest_objectives",     "ORDER BY `Order` ASC, StorageIndex ASC", "quest objectives",    &Quest::LoadQuestObjective     }
    };

    for (QuestLoaderHelper const& loader : QuestLoaderHelpers)
    {
        result = WorldDatabase.PQuery("SELECT %s FROM %s %s", loader.QueryFields, loader.TableName, loader.QueryExtra);

        if (!result)
            TC_LOG_INFO("server.loading", ">> Loaded 0 quest %s. DB table `%s` is empty.", loader.TableDesc, loader.TableName);
        else
        {
            do
            {
                Field* fields = result->Fetch();
                uint32 questId = fields[0].GetUInt32();

                auto itr = _questTemplates.find(questId);
                if (itr != _questTemplates.end())
                    (itr->second.*loader.LoaderFunction)(fields);
                else
                    TC_LOG_ERROR("server.loading", "Table `%s` has data for quest %u but such quest does not exist", loader.TableName, questId);
            } while (result->NextRow());
        }
    }

    // Load `quest_visual_effect` join table with quest_objectives because visual effects are based on objective ID (core stores objectives by their index in quest)
    //                                   0     1     2          3        4
    result = WorldDatabase.Query("SELECT v.ID, o.ID, o.QuestID, v.Index, v.VisualEffect FROM quest_visual_effect AS v LEFT JOIN quest_objectives AS o ON v.ID = o.ID ORDER BY v.Index DESC");

    if (!result)
    {
        TC_LOG_ERROR("server.loading", ">> Loaded 0 quest visual effects. DB table `quest_visual_effect` is empty.");
    }
    else
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 vID = fields[0].GetUInt32();
            uint32 oID = fields[1].GetUInt32();

            if (!vID)
            {
                TC_LOG_ERROR("server.loading", "Table `quest_visual_effect` has visual effect for null objective id");
                continue;
            }

            // objID will be null if match for table join is not found
            if (vID != oID)
            {
                TC_LOG_ERROR("server.loading", "Table `quest_visual_effect` has visual effect for objective %u but such objective does not exist.", vID);
                continue;
            }

            uint32 questId = fields[2].GetUInt32();

            // Do not throw error here because error for non existing quest is thrown while loading quest objectives. we do not need duplication
            auto itr = _questTemplates.find(questId);
            if (itr != _questTemplates.end())
                itr->second.LoadQuestObjectiveVisualEffect(fields);
        } while (result->NextRow());
    }

    std::map<uint32, uint32> usedMailTemplates;

    // Post processing
    for (auto& questPair : _questTemplates)
    {
        // skip post-loading checks for disabled quests
        if (DisableMgr::IsDisabledFor(DISABLE_TYPE_QUEST, questPair.first, nullptr))
            continue;

        Quest* qinfo = &questPair.second;

        // additional quest integrity checks (GO, creature_template and items must be loaded already)

        if (qinfo->GetQuestType() >= MAX_QUEST_TYPES)
            TC_LOG_ERROR("sql.sql", "Quest %u has `Method` = %u, expected values are 0, 1 or 2.", qinfo->GetQuestId(), qinfo->GetQuestType());

        if (qinfo->_specialFlags & ~QUEST_SPECIAL_FLAGS_DB_ALLOWED)
        {
            TC_LOG_ERROR("sql.sql", "Quest %u has `SpecialFlags` = %u > max allowed value. Correct `SpecialFlags` to value <= %u",
                qinfo->GetQuestId(), qinfo->_specialFlags, QUEST_SPECIAL_FLAGS_DB_ALLOWED);
            qinfo->_specialFlags &= QUEST_SPECIAL_FLAGS_DB_ALLOWED;
        }

        if (qinfo->_flags & QUEST_FLAGS_DAILY && qinfo->_flags & QUEST_FLAGS_WEEKLY)
        {
            TC_LOG_ERROR("sql.sql", "Weekly Quest %u is marked as daily quest in `Flags`, removed daily flag.", qinfo->GetQuestId());
            qinfo->_flags &= ~QUEST_FLAGS_DAILY;
        }

        if (qinfo->_flags & QUEST_FLAGS_DAILY)
        {
            if (!(qinfo->_specialFlags & QUEST_SPECIAL_FLAGS_REPEATABLE))
            {
                TC_LOG_DEBUG("sql.sql", "Daily Quest %u not marked as repeatable in `SpecialFlags`, added.", qinfo->GetQuestId());
                qinfo->_specialFlags |= QUEST_SPECIAL_FLAGS_REPEATABLE;
            }
        }

        if (qinfo->_flags & QUEST_FLAGS_WEEKLY)
        {
            if (!(qinfo->_specialFlags & QUEST_SPECIAL_FLAGS_REPEATABLE))
            {
                TC_LOG_DEBUG("sql.sql", "Weekly Quest %u not marked as repeatable in `SpecialFlags`, added.", qinfo->GetQuestId());
                qinfo->_specialFlags |= QUEST_SPECIAL_FLAGS_REPEATABLE;
            }
        }

        if (qinfo->_specialFlags & QUEST_SPECIAL_FLAGS_MONTHLY)
        {
            if (!(qinfo->_specialFlags & QUEST_SPECIAL_FLAGS_REPEATABLE))
            {
                TC_LOG_DEBUG("sql.sql", "Monthly quest %u not marked as repeatable in `SpecialFlags`, added.", qinfo->GetQuestId());
                qinfo->_specialFlags |= QUEST_SPECIAL_FLAGS_REPEATABLE;
            }
        }

        if (qinfo->_flags & QUEST_FLAGS_TRACKING)
        {
            // at auto-reward can be rewarded only RewardChoiceItemId[0]
            for (uint32 j = 1; j < QUEST_REWARD_CHOICES_COUNT; ++j )
            {
                if (uint32 id = qinfo->RewardChoiceItemId[j])
                {
                    TC_LOG_ERROR("sql.sql", "Quest %u has `RewardChoiceItemId%d` = %u but item from `RewardChoiceItemId%d` can't be rewarded with quest flag QUEST_FLAGS_TRACKING.",
                        qinfo->GetQuestId(), j + 1, id, j + 1);
                    // no changes, quest ignore this data
                }
            }
        }

        if (qinfo->_contentTuningID && !sContentTuningStore.LookupEntry(qinfo->_contentTuningID))
        {
            TC_LOG_ERROR("sql.sql", "Quest %u has `ContentTuningID` = %u but content tuning with this id does not exist.",
                qinfo->GetQuestId(), qinfo->_contentTuningID);
        }

        // client quest log visual (area case)
        if (qinfo->_questSortID > 0)
        {
            if (!sAreaTableStore.LookupEntry(qinfo->_questSortID))
            {
                TC_LOG_ERROR("sql.sql", "Quest %u has `QuestSortID` = %u (zone case) but zone with this id does not exist.",
                    qinfo->GetQuestId(), qinfo->_questSortID);
                // no changes, quest not dependent from this value but can have problems at client
            }
        }
        // client quest log visual (sort case)
        if (qinfo->_questSortID < 0)
        {
            QuestSortEntry const* qSort = sQuestSortStore.LookupEntry(-int32(qinfo->_questSortID));
            if (!qSort)
            {
                TC_LOG_ERROR("sql.sql", "Quest %u has `QuestSortID` = %i (sort case) but quest sort with this id does not exist.",
                    qinfo->GetQuestId(), qinfo->_questSortID);
                // no changes, quest not dependent from this value but can have problems at client (note some may be 0, we must allow this so no check)
            }
            //check for proper RequiredSkillId value (skill case)
            if (uint32 skill_id = SkillByQuestSort(-int32(qinfo->_questSortID)))
            {
                if (qinfo->_requiredSkillId != skill_id)
                {
                    TC_LOG_ERROR("sql.sql", "Quest %u has `QuestSortID` = %i but `RequiredSkillId` does not have a corresponding value (%d).",
                        qinfo->GetQuestId(), qinfo->_questSortID, skill_id);
                    //override, and force proper value here?
                }
            }
        }

        // AllowableClasses, can be 0/CLASSMASK_ALL_PLAYABLE to allow any class
        if (qinfo->_allowableClasses)
        {
            if (!(qinfo->_allowableClasses & CLASSMASK_ALL_PLAYABLE))
            {
                TC_LOG_ERROR("sql.sql", "Quest %u does not contain any playable classes in `AllowableClasses` (%u), value set to 0 (all classes).", qinfo->GetQuestId(), qinfo->_allowableClasses);
                qinfo->_allowableClasses = 0;
            }
        }
        // AllowableRaces, can be -1/RACEMASK_ALL_PLAYABLE to allow any race
        if (qinfo->_allowableRaces.RawValue != uint64(-1))
        {
            if (qinfo->_allowableRaces && !(qinfo->_allowableRaces.RawValue & RACEMASK_ALL_PLAYABLE))
            {
                TC_LOG_ERROR("sql.sql", "Quest %u does not contain any playable races in `AllowableRaces` (" UI64FMTD "), value set to -1 (all races).", qinfo->GetQuestId(), qinfo->_allowableRaces.RawValue);
                qinfo->_allowableRaces.RawValue = uint64(-1);
            }
        }
        // RequiredSkillId, can be 0
        if (qinfo->_requiredSkillId)
        {
            if (!sSkillLineStore.LookupEntry(qinfo->_requiredSkillId))
            {
                TC_LOG_ERROR("sql.sql", "Quest %u has `RequiredSkillId` = %u but this skill does not exist",
                    qinfo->GetQuestId(), qinfo->_requiredSkillId);
            }
        }

        if (qinfo->_requiredSkillPoints)
        {
            if (qinfo->_requiredSkillPoints > sWorld->GetConfigMaxSkillValue())
            {
                TC_LOG_ERROR("sql.sql", "Quest %u has `RequiredSkillPoints` = %u but max possible skill is %u, quest can't be done.",
                    qinfo->GetQuestId(), qinfo->_requiredSkillPoints, sWorld->GetConfigMaxSkillValue());
                // no changes, quest can't be done for this requirement
            }
        }
        // else Skill quests can have 0 skill level, this is ok

        if (qinfo->_requiredMinRepFaction && !sFactionStore.LookupEntry(qinfo->_requiredMinRepFaction))
        {
            TC_LOG_ERROR("sql.sql", "Quest %u has `RequiredMinRepFaction` = %u but faction template %u does not exist, quest can't be done.",
                qinfo->GetQuestId(), qinfo->_requiredMinRepFaction, qinfo->_requiredMinRepFaction);
            // no changes, quest can't be done for this requirement
        }

        if (qinfo->_requiredMaxRepFaction && !sFactionStore.LookupEntry(qinfo->_requiredMaxRepFaction))
        {
            TC_LOG_ERROR("sql.sql", "Quest %u has `RequiredMaxRepFaction` = %u but faction template %u does not exist, quest can't be done.",
                qinfo->GetQuestId(), qinfo->_requiredMaxRepFaction, qinfo->_requiredMaxRepFaction);
            // no changes, quest can't be done for this requirement
        }

        if (qinfo->_requiredMinRepValue && qinfo->_requiredMinRepValue > ReputationMgr::Reputation_Cap)
        {
            TC_LOG_ERROR("sql.sql", "Quest %u has `RequiredMinRepValue` = %d but max reputation is %u, quest can't be done.",
                qinfo->GetQuestId(), qinfo->_requiredMinRepValue, ReputationMgr::Reputation_Cap);
            // no changes, quest can't be done for this requirement
        }

        if (qinfo->_requiredMinRepValue && qinfo->_requiredMaxRepValue && qinfo->_requiredMaxRepValue <= qinfo->_requiredMinRepValue)
        {
            TC_LOG_ERROR("sql.sql", "Quest %u has `RequiredMaxRepValue` = %d and `RequiredMinRepValue` = %d, quest can't be done.",
                qinfo->GetQuestId(), qinfo->_requiredMaxRepValue, qinfo->_requiredMinRepValue);
            // no changes, quest can't be done for this requirement
        }

        if (!qinfo->_requiredMinRepFaction && qinfo->_requiredMinRepValue != 0)
        {
            TC_LOG_ERROR("sql.sql", "Quest %u has `RequiredMinRepValue` = %d but `RequiredMinRepFaction` is 0, value has no effect",
                qinfo->GetQuestId(), qinfo->_requiredMinRepValue);
            // warning
        }

        if (!qinfo->_requiredMaxRepFaction && qinfo->_requiredMaxRepValue != 0)
        {
            TC_LOG_ERROR("sql.sql", "Quest %u has `RequiredMaxRepValue` = %d but `RequiredMaxRepFaction` is 0, value has no effect",
                qinfo->GetQuestId(), qinfo->_requiredMaxRepValue);
            // warning
        }

        if (qinfo->_rewardTitleId && !sCharTitlesStore.LookupEntry(qinfo->_rewardTitleId))
        {
            TC_LOG_ERROR("sql.sql", "Quest %u has `RewardTitleId` = %u but CharTitle Id %u does not exist, quest can't be rewarded with title.",
                qinfo->GetQuestId(), qinfo->_rewardTitleId, qinfo->_rewardTitleId);
            qinfo->_rewardTitleId = 0;
            // quest can't reward this title
        }

        if (qinfo->_sourceItemId)
        {
            if (!sObjectMgr->GetItemTemplate(qinfo->_sourceItemId))
            {
                TC_LOG_ERROR("sql.sql", "Quest %u has `SourceItemId` = %u but item with entry %u does not exist, quest can't be done.",
                    qinfo->GetQuestId(), qinfo->_sourceItemId, qinfo->_sourceItemId);
                qinfo->_sourceItemId = 0;                       // quest can't be done for this requirement
            }
            else if (qinfo->_sourceItemIdCount == 0)
            {
                TC_LOG_ERROR("sql.sql", "Quest %u has `StartItem` = %u but `ProvidedItemCount` = 0, set to 1 but need fix in DB.",
                    qinfo->GetQuestId(), qinfo->_sourceItemId);
                qinfo->_sourceItemIdCount = 1;                    // update to 1 for allow quest work for backward compatibility with DB
            }
        }
        else if (qinfo->_sourceItemIdCount > 0)
        {
            TC_LOG_ERROR("sql.sql", "Quest %u has `SourceItemId` = 0 but `SourceItemIdCount` = %u, useless value.",
                qinfo->GetQuestId(), qinfo->_sourceItemIdCount);
            qinfo->_sourceItemIdCount = 0;                          // no quest work changes in fact
        }

        if (qinfo->_sourceSpellID)
        {
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(qinfo->_sourceSpellID, DIFFICULTY_NONE);
            if (!spellInfo)
            {
                TC_LOG_ERROR("sql.sql", "Quest %u has `SourceSpellid` = %u but spell %u doesn't exist, quest can't be done.",
                    qinfo->GetQuestId(), qinfo->_sourceSpellID, qinfo->_sourceSpellID);
                qinfo->_sourceSpellID = 0;                        // quest can't be done for this requirement
            }
            else if (!SpellMgr::IsSpellValid(spellInfo))
            {
                TC_LOG_ERROR("sql.sql", "Quest %u has `SourceSpellid` = %u but spell %u is broken, quest can't be done.",
                    qinfo->GetQuestId(), qinfo->_sourceSpellID, qinfo->_sourceSpellID);
                qinfo->_sourceSpellID = 0;                        // quest can't be done for this requirement
            }
        }

        for (QuestObjective const& obj : qinfo->GetObjectives())
        {
            // Store objective for lookup by id
            _questObjectives[obj.ID] = &obj;

            // Check storage index for objectives which store data
            if (obj.StorageIndex < 0)
            {
                switch (obj.Type)
                {
                    case QUEST_OBJECTIVE_MONSTER:
                    case QUEST_OBJECTIVE_ITEM:
                    case QUEST_OBJECTIVE_GAMEOBJECT:
                    case QUEST_OBJECTIVE_TALKTO:
                    case QUEST_OBJECTIVE_PLAYERKILLS:
                    case QUEST_OBJECTIVE_AREATRIGGER:
                    case QUEST_OBJECTIVE_WINPETBATTLEAGAINSTNPC:
                    case QUEST_OBJECTIVE_OBTAIN_CURRENCY:
                        TC_LOG_ERROR("sql.sql", "Quest %u objective %u has invalid StorageIndex = %d for objective type %u", qinfo->GetQuestId(), obj.ID, obj.StorageIndex, obj.Type);
                        break;
                    default:
                        break;
                }
            }

            switch (obj.Type)
            {
                case QUEST_OBJECTIVE_ITEM:
                    if (!sObjectMgr->GetItemTemplate(obj.ObjectID))
                        TC_LOG_ERROR("sql.sql", "Quest %u objective %u has non existing item entry %u, quest can't be done.",
                            qinfo->GetQuestId(), obj.ID, obj.ObjectID);
                    break;
                case QUEST_OBJECTIVE_MONSTER:
                    if (!sObjectMgr->GetCreatureTemplate(obj.ObjectID))
                        TC_LOG_ERROR("sql.sql", "Quest %u objective %u has non existing creature entry %u, quest can't be done.",
                            qinfo->GetQuestId(), obj.ID, uint32(obj.ObjectID));
                    break;
                case QUEST_OBJECTIVE_GAMEOBJECT:
                    if (!sObjectMgr->GetGameObjectTemplate(obj.ObjectID))
                        TC_LOG_ERROR("sql.sql", "Quest %u objective %u has non existing gameobject entry %u, quest can't be done.",
                            qinfo->GetQuestId(), obj.ID, uint32(obj.ObjectID));
                    break;
                case QUEST_OBJECTIVE_TALKTO:
                    if (!sObjectMgr->GetCreatureTemplate(obj.ObjectID))
                        TC_LOG_ERROR("sql.sql", "Quest %u objective %u has non existing creature entry %u, quest can't be done.",
                            qinfo->GetQuestId(), obj.ID, uint32(obj.ObjectID));
                    break;
                case QUEST_OBJECTIVE_MIN_REPUTATION:
                case QUEST_OBJECTIVE_MAX_REPUTATION:
                case QUEST_OBJECTIVE_INCREASE_REPUTATION:
                    if (!sFactionStore.LookupEntry(obj.ObjectID))
                        TC_LOG_ERROR("sql.sql", "Quest %u objective %u has non existing faction id %d", qinfo->GetQuestId(), obj.ID, obj.ObjectID);
                    break;
                case QUEST_OBJECTIVE_PLAYERKILLS:
                    if (obj.Amount <= 0)
                        TC_LOG_ERROR("sql.sql", "Quest %u objective %u has invalid player kills count %d", qinfo->GetQuestId(), obj.ID, obj.Amount);
                    break;
                case QUEST_OBJECTIVE_CURRENCY:
                case QUEST_OBJECTIVE_HAVE_CURRENCY:
                case QUEST_OBJECTIVE_OBTAIN_CURRENCY:
                    if (!sCurrencyTypesStore.LookupEntry(obj.ObjectID))
                        TC_LOG_ERROR("sql.sql", "Quest %u objective %u has non existing currency %d", qinfo->GetQuestId(), obj.ID, obj.ObjectID);
                    if (obj.Amount <= 0)
                        TC_LOG_ERROR("sql.sql", "Quest %u objective %u has invalid currency amount %d", qinfo->GetQuestId(), obj.ID, obj.Amount);
                    break;
                case QUEST_OBJECTIVE_LEARNSPELL:
                    if (!sSpellMgr->GetSpellInfo(obj.ObjectID, DIFFICULTY_NONE))
                        TC_LOG_ERROR("sql.sql", "Quest %u objective %u has non existing spell id %d", qinfo->GetQuestId(), obj.ID, obj.ObjectID);
                    break;
                case QUEST_OBJECTIVE_WINPETBATTLEAGAINSTNPC:
                    if (obj.ObjectID && !sObjectMgr->GetCreatureTemplate(obj.ObjectID))
                        TC_LOG_ERROR("sql.sql", "Quest %u objective %u has non existing creature entry %u, quest can't be done.",
                            qinfo->GetQuestId(), obj.ID, uint32(obj.ObjectID));
                    break;
                case QUEST_OBJECTIVE_DEFEATBATTLEPET:
                    if (!sBattlePetSpeciesStore.LookupEntry(obj.ObjectID))
                        TC_LOG_ERROR("sql.sql", "Quest %u objective %u has non existing battlepet species id %d", qinfo->GetQuestId(), obj.ID, obj.ObjectID);
                    break;
                case QUEST_OBJECTIVE_CRITERIA_TREE:
                    if (!sCriteriaTreeStore.LookupEntry(obj.ObjectID))
                        TC_LOG_ERROR("sql.sql", "Quest %u objective %u has non existing criteria tree id %d", qinfo->GetQuestId(), obj.ID, obj.ObjectID);
                    break;
                case QUEST_OBJECTIVE_AREATRIGGER:
                    if (!sAreaTriggerStore.LookupEntry(uint32(obj.ObjectID)) && obj.ObjectID != -1)
                        TC_LOG_ERROR("sql.sql", "Quest %u objective %u has non existing AreaTrigger.db2 id %d", qinfo->GetQuestId(), obj.ID, obj.ObjectID);
                    break;
                case QUEST_OBJECTIVE_AREA_TRIGGER_ENTER:
                case QUEST_OBJECTIVE_AREA_TRIGGER_EXIT:
                    if (!sAreaTriggerDataStore->GetAreaTriggerTemplate({ uint32(obj.ObjectID), false }) && !sAreaTriggerDataStore->GetAreaTriggerTemplate({ uint32(obj.ObjectID), true }))
                        TC_LOG_ERROR("sql.sql", "Quest %u objective %u has non existing areatrigger id %d", qinfo->GetQuestId(), obj.ID, obj.ObjectID);
                    break;
                case QUEST_OBJECTIVE_MONEY:
                case QUEST_OBJECTIVE_WINPVPPETBATTLES:
                    break;
                default:
                    TC_LOG_ERROR("sql.sql", "Quest %u objective %u has unhandled type %u", qinfo->GetQuestId(), obj.ID, obj.Type);
                    break;
            }

            if (obj.Flags & QUEST_OBJECTIVE_FLAG_SEQUENCED)
                qinfo->SetSpecialFlag(QUEST_SPECIAL_FLAGS_SEQUENCED_OBJECTIVES);
        }

        for (uint8 j = 0; j < QUEST_ITEM_DROP_COUNT; ++j)
        {
            uint32 id = qinfo->ItemDrop[j];
            if (id)
            {
                if (!sObjectMgr->GetItemTemplate(id))
                {
                    TC_LOG_ERROR("sql.sql", "Quest %u has `ItemDrop%d` = %u but item with entry %u does not exist, quest can't be done.",
                        qinfo->GetQuestId(), j+1, id, id);
                    // no changes, quest can't be done for this requirement
                }
            }
            else
            {
                if (qinfo->ItemDropQuantity[j]>0)
                {
                    TC_LOG_ERROR("sql.sql", "Quest %u has `ItemDrop%d` = 0 but `ItemDropQuantity%d` = %u.",
                        qinfo->GetQuestId(), j+1, j+1, qinfo->ItemDropQuantity[j]);
                    // no changes, quest ignore this data
                }
            }
        }

        for (uint8 j = 0; j < QUEST_REWARD_CHOICES_COUNT; ++j)
        {
            if (uint32 id = qinfo->RewardChoiceItemId[j])
            {
                switch (qinfo->RewardChoiceItemType[j])
                {
                    case LootItemType::Item:
                        if (!sObjectMgr->GetItemTemplate(id))
                        {
                            TC_LOG_ERROR("sql.sql", "Quest %u has `RewardChoiceItemId%d` = %u but item with entry %u does not exist, quest will not reward this item.",
                                qinfo->GetQuestId(), j + 1, id, id);
                            qinfo->RewardChoiceItemId[j] = 0;          // no changes, quest will not reward this
                        }
                        break;
                    case LootItemType::Currency:
                        if (!sCurrencyTypesStore.HasRecord(id))
                        {
                            TC_LOG_ERROR("sql.sql", "Quest %u has `RewardChoiceItemId%d` = %u but currency with id %u does not exist, quest will not reward this currency.",
                                qinfo->GetQuestId(), j + 1, id, id);
                            qinfo->RewardChoiceItemId[j] = 0;          // no changes, quest will not reward this
                        }
                        break;
                    default:
                        TC_LOG_ERROR("sql.sql", "Quest %u has `RewardChoiceItemType%d` = %u but it is not a valid item type, reward removed.",
                            qinfo->GetQuestId(), j + 1, uint32(qinfo->RewardChoiceItemType[j]));
                        qinfo->RewardChoiceItemId[j] = 0;
                        break;
                }

                if (!qinfo->RewardChoiceItemCount[j])
                {
                    TC_LOG_ERROR("sql.sql", "Quest %u has `RewardChoiceItemId%d` = %u but `RewardChoiceItemCount%d` = 0.",
                        qinfo->GetQuestId(), j + 1, id, j + 1);
                }
            }
            else if (qinfo->RewardChoiceItemCount[j] > 0)
            {
                TC_LOG_ERROR("sql.sql", "Quest %u has `RewardChoiceItemId%d` = 0 but `RewardChoiceItemCount%d` = %u.",
                    qinfo->GetQuestId(), j + 1, j + 1, qinfo->RewardChoiceItemCount[j]);
                // no changes, quest ignore this data
            }
        }

        for (uint8 j = 0; j < QUEST_REWARD_ITEM_COUNT; ++j)
        {
            uint32 id = qinfo->RewardItemId[j];
            if (id)
            {
                if (!sObjectMgr->GetItemTemplate(id))
                {
                    TC_LOG_ERROR("sql.sql", "Quest %u has `RewardItemId%d` = %u but item with entry %u does not exist, quest will not reward this item.",
                        qinfo->GetQuestId(), j+1, id, id);
                    qinfo->RewardItemId[j] = 0;                // no changes, quest will not reward this item
                }

                if (!qinfo->RewardItemCount[j])
                {
                    TC_LOG_ERROR("sql.sql", "Quest %u has `RewardItemId%d` = %u but `RewardItemCount%d` = 0, quest will not reward this item.",
                        qinfo->GetQuestId(), j+1, id, j+1);
                    // no changes
                }
            }
            else if (qinfo->RewardItemCount[j]>0)
            {
                TC_LOG_ERROR("sql.sql", "Quest %u has `RewardItemId%d` = 0 but `RewardItemCount%d` = %u.",
                    qinfo->GetQuestId(), j+1, j+1, qinfo->RewardItemCount[j]);
                // no changes, quest ignore this data
            }
        }

        for (uint8 j = 0; j < QUEST_REWARD_REPUTATIONS_COUNT; ++j)
        {
            if (qinfo->RewardFactionId[j])
            {
                if (std::abs(qinfo->RewardFactionValue[j]) > 9)
                {
               TC_LOG_ERROR("sql.sql", "Quest %u has RewardFactionValueId%d = %i. That is outside the range of valid values (-9 to 9).", qinfo->GetQuestId(), j+1, qinfo->RewardFactionValue[j]);
                }
                if (!sFactionStore.LookupEntry(qinfo->RewardFactionId[j]))
                {
                    TC_LOG_ERROR("sql.sql", "Quest %u has `RewardFactionId%d` = %u but raw faction (faction.dbc) %u does not exist, quest will not reward reputation for this faction.", qinfo->GetQuestId(), j+1, qinfo->RewardFactionId[j], qinfo->RewardFactionId[j]);
                    qinfo->RewardFactionId[j] = 0;            // quest will not reward this
                }
            }

            else if (qinfo->RewardFactionOverride[j] != 0)
            {
                TC_LOG_ERROR("sql.sql", "Quest %u has `RewardFactionId%d` = 0 but `RewardFactionValueIdOverride%d` = %i.",
                    qinfo->GetQuestId(), j+1, j+1, qinfo->RewardFactionOverride[j]);
                // no changes, quest ignore this data
            }
        }

        if (qinfo->_rewardSpell > 0)
        {
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(qinfo->_rewardSpell, DIFFICULTY_NONE);

            if (!spellInfo)
            {
                TC_LOG_ERROR("sql.sql", "Quest %u has `RewardSpellCast` = %u but spell %u does not exist, quest will not have a spell reward.",
                    qinfo->GetQuestId(), qinfo->_rewardSpell, qinfo->_rewardSpell);
                qinfo->_rewardSpell = 0;                    // no spell will be cast on player
            }

            else if (!SpellMgr::IsSpellValid(spellInfo))
            {
                TC_LOG_ERROR("sql.sql", "Quest %u has `RewardSpellCast` = %u but spell %u is broken, quest will not have a spell reward.",
                    qinfo->GetQuestId(), qinfo->_rewardSpell, qinfo->_rewardSpell);
                qinfo->_rewardSpell = 0;                    // no spell will be cast on player
            }
        }

        if (qinfo->_rewardMailTemplateId)
        {
            if (!sMailTemplateStore.LookupEntry(qinfo->_rewardMailTemplateId))
            {
                TC_LOG_ERROR("sql.sql", "Quest %u has `RewardMailTemplateId` = %u but mail template  %u does not exist, quest will not have a mail reward.",
                    qinfo->GetQuestId(), qinfo->_rewardMailTemplateId, qinfo->_rewardMailTemplateId);
                qinfo->_rewardMailTemplateId = 0;               // no mail will send to player
                qinfo->_rewardMailDelay = 0;                // no mail will send to player
                qinfo->_rewardMailSenderEntry = 0;
            }
            else if (usedMailTemplates.find(qinfo->_rewardMailTemplateId) != usedMailTemplates.end())
            {
                auto used_mt_itr = usedMailTemplates.find(qinfo->_rewardMailTemplateId);
                TC_LOG_ERROR("sql.sql", "Quest %u has `RewardMailTemplateId` = %u but mail template  %u already used for quest %u, quest will not have a mail reward.",
                    qinfo->GetQuestId(), qinfo->_rewardMailTemplateId, qinfo->_rewardMailTemplateId, used_mt_itr->second);
                qinfo->_rewardMailTemplateId = 0;               // no mail will send to player
                qinfo->_rewardMailDelay = 0;                // no mail will send to player
                qinfo->_rewardMailSenderEntry = 0;
            }
            else
                usedMailTemplates.emplace(qinfo->_rewardMailTemplateId, qinfo->GetQuestId());
        }

        if (uint32 nextQuestInChain = qinfo->_nextQuestInChain)
        {
            if (!_questTemplates.count(nextQuestInChain))
            {
                TC_LOG_ERROR("sql.sql", "Quest %u has `NextQuestInChain` = %u but quest %u does not exist, quest chain will not work.",
                    qinfo->GetQuestId(), qinfo->_nextQuestInChain, qinfo->_nextQuestInChain);
                qinfo->_nextQuestInChain = 0;
            }
        }

        for (uint8 j = 0; j < QUEST_REWARD_CURRENCY_COUNT; ++j)
        {
            if (qinfo->RewardCurrencyId[j])
            {
                if (qinfo->RewardCurrencyCount[j] == 0)
                {
                    TC_LOG_ERROR("sql.sql", "Quest %u has `RewardCurrencyId%d` = %u but `RewardCurrencyCount%d` = 0, quest can't be done.",
                        qinfo->GetQuestId(), j+1, qinfo->RewardCurrencyId[j], j+1);
                    // no changes, quest can't be done for this requirement
                }

                if (!sCurrencyTypesStore.LookupEntry(qinfo->RewardCurrencyId[j]))
                {
                    TC_LOG_ERROR("sql.sql", "Quest %u has `RewardCurrencyId%d` = %u but currency with entry %u does not exist, quest can't be done.",
                        qinfo->GetQuestId(), j+1, qinfo->RewardCurrencyId[j], qinfo->RewardCurrencyId[j]);
                    qinfo->RewardCurrencyCount[j] = 0;             // prevent incorrect work of quest
                }
            }
            else if (qinfo->RewardCurrencyCount[j] > 0)
            {
                TC_LOG_ERROR("sql.sql", "Quest %u has `RewardCurrencyId%d` = 0 but `RewardCurrencyCount%d` = %u, quest can't be done.",
                    qinfo->GetQuestId(), j+1, j+1, qinfo->RewardCurrencyCount[j]);
                qinfo->RewardCurrencyCount[j] = 0;                 // prevent incorrect work of quest
            }
        }

        if (qinfo->_soundAccept)
        {
            if (!sSoundKitStore.LookupEntry(qinfo->_soundAccept))
            {
                TC_LOG_ERROR("sql.sql", "Quest %u has `SoundAccept` = %u but sound %u does not exist, set to 0.",
                    qinfo->GetQuestId(), qinfo->_soundAccept, qinfo->_soundAccept);
                qinfo->_soundAccept = 0;                        // no sound will be played
            }
        }

        if (qinfo->_soundTurnIn)
        {
            if (!sSoundKitStore.LookupEntry(qinfo->_soundTurnIn))
            {
                TC_LOG_ERROR("sql.sql", "Quest %u has `SoundTurnIn` = %u but sound %u does not exist, set to 0.",
                    qinfo->GetQuestId(), qinfo->_soundTurnIn, qinfo->_soundTurnIn);
                qinfo->_soundTurnIn = 0;                        // no sound will be played
            }
        }

        if (qinfo->_rewardSkillId)
        {
            if (!sSkillLineStore.LookupEntry(qinfo->_rewardSkillId))
            {
                TC_LOG_ERROR("sql.sql", "Quest %u has `RewardSkillId` = %u but this skill does not exist",
                    qinfo->GetQuestId(), qinfo->_rewardSkillId);
            }
            if (!qinfo->_rewardSkillPoints)
            {
                TC_LOG_ERROR("sql.sql", "Quest %u has `RewardSkillId` = %u but `RewardSkillPoints` is 0",
                    qinfo->GetQuestId(), qinfo->_rewardSkillId);
            }
        }

        if (qinfo->_rewardSkillPoints)
        {
            if (qinfo->_rewardSkillPoints > sWorld->GetConfigMaxSkillValue())
            {
                TC_LOG_ERROR("sql.sql", "Quest %u has `RewardSkillPoints` = %u but max possible skill is %u, quest can't be done.",
                    qinfo->GetQuestId(), qinfo->_rewardSkillPoints, sWorld->GetConfigMaxSkillValue());
                // no changes, quest can't be done for this requirement
            }
            if (!qinfo->_rewardSkillId)
            {
                TC_LOG_ERROR("sql.sql", "Quest %u has `RewardSkillPoints` = %u but `RewardSkillId` is 0",
                    qinfo->GetQuestId(), qinfo->_rewardSkillPoints);
            }
        }

        // fill additional data stores
        if (uint32 prevQuestId = std::abs(qinfo->_prevQuestID))
        {
            if (!_questTemplates.count(prevQuestId))
                TC_LOG_ERROR("sql.sql", "Quest %d has PrevQuestId %i, but no such quest", qinfo->GetQuestId(), qinfo->GetPrevQuestId());
        }

        if (uint32 nextQuestId = qinfo->_nextQuestID)
        {
            auto nextQuestItr = _questTemplates.find(nextQuestId);
            if (nextQuestItr == _questTemplates.end())
                TC_LOG_ERROR("sql.sql", "Quest %d has NextQuestId %u, but no such quest", qinfo->GetQuestId(), qinfo->_nextQuestID);
            else
                nextQuestItr->second.DependentPreviousQuests.push_back(qinfo->GetQuestId());
        }

        if (qinfo->_exclusiveGroup)
            _exclusiveQuestGroups.insert(std::pair<int32, uint32>(qinfo->_exclusiveGroup, qinfo->GetQuestId()));
    }

    // check QUEST_SPECIAL_FLAGS_EXPLORATION_OR_EVENT for spell with SPELL_EFFECT_QUEST_COMPLETE
    for (SpellNameEntry const* spellNameEntry : sSpellNameStore)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellNameEntry->ID, DIFFICULTY_NONE);
        if (!spellInfo)
            continue;

        for (SpellEffectInfo const& spellEffectInfo : spellInfo->GetEffects())
        {
            if (spellEffectInfo.Effect != SPELL_EFFECT_QUEST_COMPLETE)
                continue;

            uint32 quest_id = spellEffectInfo.MiscValue;

            Quest const* quest = GetQuestTemplate(quest_id);

            // some quest referenced in spells not exist (outdated spells)
            if (!quest)
                continue;

            if (!quest->HasSpecialFlag(QUEST_SPECIAL_FLAGS_EXPLORATION_OR_EVENT))
            {
                TC_LOG_ERROR("sql.sql", "Spell (id: %u) have SPELL_EFFECT_QUEST_COMPLETE for quest %u, but quest not have flag QUEST_SPECIAL_FLAGS_EXPLORATION_OR_EVENT. Quest flags must be fixed, quest modified to enable objective.", spellInfo->Id, quest_id);

                // this will prevent quest completing without objective
                const_cast<Quest*>(quest)->SetSpecialFlag(QUEST_SPECIAL_FLAGS_EXPLORATION_OR_EVENT);
            }
        }
    }

    // Make all paragon reward quests repeatable
    for (ParagonReputationEntry const* paragonReputation : sParagonReputationStore)
        if (Quest const* quest = GetQuestTemplate(paragonReputation->QuestID))
            const_cast<Quest*>(quest)->SetSpecialFlag(QUEST_SPECIAL_FLAGS_REPEATABLE);

    TC_LOG_INFO("server.loading", ">> Loaded " SZFMTD " quests definitions in %u ms", _questTemplates.size(), GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadQuestStartersAndEnders()
{
    TC_LOG_INFO("server.loading", "Loading GO Start Quest Data...");
    LoadGameobjectQuestStarters();
    TC_LOG_INFO("server.loading", "Loading GO End Quest Data...");
    LoadGameobjectQuestEnders();
    TC_LOG_INFO("server.loading", "Loading Creature Start Quest Data...");
    LoadCreatureQuestStarters();
    TC_LOG_INFO("server.loading", "Loading Creature End Quest Data...");
    LoadCreatureQuestEnders();
}

void ObjectMgr::LoadQuestTemplateLocale()
{
    uint32 oldMSTime = getMSTime();

    _questTemplateLocaleStore.clear(); // need for reload case
    //                                               0     1
    QueryResult result = WorldDatabase.Query("SELECT Id, locale, "
    //      2           3                 4                5                 6                  7                   8                   9                  10
        "LogTitle, LogDescription, QuestDescription, AreaDescription, PortraitGiverText, PortraitGiverName, PortraitTurnInText, PortraitTurnInName, QuestCompletionLog"
        " FROM quest_template_locale");
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();

        uint32 id                       = fields[0].GetUInt32();
        std::string localeName          = fields[1].GetString();

        LocaleConstant locale = GetLocaleByName(localeName);
        if (!IsValidLocale(locale) || locale == LOCALE_enUS)
            continue;

        QuestTemplateLocale& data = _questTemplateLocaleStore[id];
        AddLocaleString(fields[2].GetString(), locale, data.LogTitle);
        AddLocaleString(fields[3].GetString(), locale, data.LogDescription);
        AddLocaleString(fields[4].GetString(), locale, data.QuestDescription);
        AddLocaleString(fields[5].GetString(), locale, data.AreaDescription);
        AddLocaleString(fields[6].GetString(), locale, data.PortraitGiverText);
        AddLocaleString(fields[7].GetString(), locale, data.PortraitGiverName);
        AddLocaleString(fields[8].GetString(), locale, data.PortraitTurnInText);
        AddLocaleString(fields[9].GetString(), locale, data.PortraitTurnInName);
        AddLocaleString(fields[10].GetString(), locale, data.QuestCompletionLog);
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded " SZFMTD " Quest Template locale strings in %u ms", _questTemplateLocaleStore.size(), GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadQuestObjectivesLocale()
{
    uint32 oldMSTime = getMSTime();

    _questObjectivesLocaleStore.clear(); // need for reload case
    //                                               0     1          2
    QueryResult result = WorldDatabase.Query("SELECT Id, locale, Description FROM quest_objectives_locale");
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();

        uint32 id                           = fields[0].GetUInt32();
        std::string localeName              = fields[1].GetString();

        LocaleConstant locale = GetLocaleByName(localeName);
        if (!IsValidLocale(locale) || locale == LOCALE_enUS)
            continue;

        QuestObjectivesLocale& data = _questObjectivesLocaleStore[id];
        AddLocaleString(fields[2].GetString(), locale, data.Description);
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded " SZFMTD " Quest Objectives locale strings in %u ms", _questObjectivesLocaleStore.size(), GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadQuestGreetingLocales()
{
    uint32 oldMSTime = getMSTime();

    for (std::size_t i = 0; i < _questGreetingLocaleStore.size(); ++i)
        _questGreetingLocaleStore[i].clear();

    //                                               0   1     2       3
    QueryResult result = WorldDatabase.Query("SELECT Id, type, locale, Greeting FROM quest_greeting_locale");
    if (!result)
        return;

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 id = fields[0].GetUInt32();
        uint8 type = fields[1].GetUInt8();
        switch (type)
        {
            case 0: // Creature
                if (!sObjectMgr->GetCreatureTemplate(id))
                {
                    TC_LOG_ERROR("sql.sql", "Table `quest_greeting_locale`: creature template entry %u does not exist.", id);
                    continue;
                }
                break;
            case 1: // GameObject
                if (!sObjectMgr->GetGameObjectTemplate(id))
                {
                    TC_LOG_ERROR("sql.sql", "Table `quest_greeting_locale`: gameobject template entry %u does not exist.", id);
                    continue;
                }
                break;
            default:
                continue;
        }

        std::string localeName = fields[2].GetString();

        LocaleConstant locale = GetLocaleByName(localeName);
        if (!IsValidLocale(locale) || locale == LOCALE_enUS)
            continue;

        QuestGreetingLocale& data = _questGreetingLocaleStore[type][id];
        AddLocaleString(fields[3].GetString(), locale, data.Greeting);
        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u Quest Greeting locale strings in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadQuestOfferRewardLocale()
{
    uint32 oldMSTime = getMSTime();

    _questOfferRewardLocaleStore.clear(); // need for reload case
    //                                               0     1          2
    QueryResult result = WorldDatabase.Query("SELECT Id, locale, RewardText FROM quest_offer_reward_locale");
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();

        uint32 id = fields[0].GetUInt32();
        std::string localeName = fields[1].GetString();

        LocaleConstant locale = GetLocaleByName(localeName);
        if (!IsValidLocale(locale) || locale == LOCALE_enUS)
            continue;

        QuestOfferRewardLocale& data = _questOfferRewardLocaleStore[id];
        AddLocaleString(fields[2].GetString(), locale, data.RewardText);
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded " SZFMTD " Quest Offer Reward locale strings in %u ms", _questOfferRewardLocaleStore.size(), GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadQuestRequestItemsLocale()
{
    uint32 oldMSTime = getMSTime();

    _questRequestItemsLocaleStore.clear(); // need for reload case
    //                                               0     1          2
    QueryResult result = WorldDatabase.Query("SELECT Id, locale, CompletionText FROM quest_request_items_locale");
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();

        uint32 id = fields[0].GetUInt32();
        std::string localeName = fields[1].GetString();

        LocaleConstant locale = GetLocaleByName(localeName);
        if (!IsValidLocale(locale) || locale == LOCALE_enUS)
            continue;

        QuestRequestItemsLocale& data = _questRequestItemsLocaleStore[id];
        AddLocaleString(fields[2].GetString(), locale, data.CompletionText);
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded " SZFMTD " Quest Request Items locale strings in %u ms", _questRequestItemsLocaleStore.size(), GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadScripts(ScriptsType type)
{
    uint32 oldMSTime = getMSTime();

    ScriptMapMap* scripts = GetScriptsMapByType(type);
    if (!scripts)
        return;

    std::string tableName = GetScriptsTableNameByType(type);
    if (tableName.empty())
        return;

    if (sMapMgr->IsScriptScheduled())                    // function cannot be called when scripts are in use.
        return;

    TC_LOG_INFO("server.loading", "Loading %s...", tableName.c_str());

    scripts->clear();                                       // need for reload support

    bool isSpellScriptTable = (type == SCRIPTS_SPELL);
    //                                                 0    1       2         3         4          5    6  7  8  9
    QueryResult result = WorldDatabase.PQuery("SELECT id, delay, command, datalong, datalong2, dataint, x, y, z, o%s FROM %s", isSpellScriptTable ? ", effIndex" : "", tableName.c_str());

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 script definitions. DB table `%s` is empty!", tableName.c_str());
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();
        ScriptInfo tmp;
        tmp.type      = type;
        tmp.id           = fields[0].GetUInt32();
        if (isSpellScriptTable)
            tmp.id      |= fields[10].GetUInt8() << 24;
        tmp.delay        = fields[1].GetUInt32();
        tmp.command      = ScriptCommands(fields[2].GetUInt32());
        tmp.Raw.nData[0] = fields[3].GetUInt32();
        tmp.Raw.nData[1] = fields[4].GetUInt32();
        tmp.Raw.nData[2] = fields[5].GetInt32();
        tmp.Raw.fData[0] = fields[6].GetFloat();
        tmp.Raw.fData[1] = fields[7].GetFloat();
        tmp.Raw.fData[2] = fields[8].GetFloat();
        tmp.Raw.fData[3] = fields[9].GetFloat();

        // generic command args check
        switch (tmp.command)
        {
            case SCRIPT_COMMAND_TALK:
            {
                if (tmp.Talk.ChatType > CHAT_TYPE_WHISPER && tmp.Talk.ChatType != CHAT_MSG_RAID_BOSS_WHISPER)
                {
                    TC_LOG_ERROR("sql.sql", "Table `%s` has invalid talk type (datalong = %u) in SCRIPT_COMMAND_TALK for script id %u",
                        tableName.c_str(), tmp.Talk.ChatType, tmp.id);
                    continue;
                }
                if (!sBroadcastTextStore.LookupEntry(uint32(tmp.Talk.TextID)))
                {
                    TC_LOG_ERROR("sql.sql", "Table `%s` has invalid talk text id (dataint = %i) in SCRIPT_COMMAND_TALK for script id %u",
                        tableName.c_str(), tmp.Talk.TextID, tmp.id);
                    continue;
                }

                break;
            }

            case SCRIPT_COMMAND_EMOTE:
            {
                if (!sEmotesStore.LookupEntry(tmp.Emote.EmoteID))
                {
                    TC_LOG_ERROR("sql.sql", "Table `%s` has invalid emote id (datalong = %u) in SCRIPT_COMMAND_EMOTE for script id %u",
                        tableName.c_str(), tmp.Emote.EmoteID, tmp.id);
                    continue;
                }
                break;
            }

            case SCRIPT_COMMAND_TELEPORT_TO:
            {
                if (!sMapStore.LookupEntry(tmp.TeleportTo.MapID))
                {
                    TC_LOG_ERROR("sql.sql", "Table `%s` has invalid map (Id: %u) in SCRIPT_COMMAND_TELEPORT_TO for script id %u",
                        tableName.c_str(), tmp.TeleportTo.MapID, tmp.id);
                    continue;
                }

                if (!Trinity::IsValidMapCoord(tmp.TeleportTo.DestX, tmp.TeleportTo.DestY, tmp.TeleportTo.DestZ, tmp.TeleportTo.Orientation))
                {
                    TC_LOG_ERROR("sql.sql", "Table `%s` has invalid coordinates (X: %f Y: %f Z: %f O: %f) in SCRIPT_COMMAND_TELEPORT_TO for script id %u",
                        tableName.c_str(), tmp.TeleportTo.DestX, tmp.TeleportTo.DestY, tmp.TeleportTo.DestZ, tmp.TeleportTo.Orientation, tmp.id);
                    continue;
                }
                break;
            }

            case SCRIPT_COMMAND_QUEST_EXPLORED:
            {
                Quest const* quest = GetQuestTemplate(tmp.QuestExplored.QuestID);
                if (!quest)
                {
                    TC_LOG_ERROR("sql.sql", "Table `%s` has invalid quest (ID: %u) in SCRIPT_COMMAND_QUEST_EXPLORED in `datalong` for script id %u",
                        tableName.c_str(), tmp.QuestExplored.QuestID, tmp.id);
                    continue;
                }

                if (!quest->HasSpecialFlag(QUEST_SPECIAL_FLAGS_EXPLORATION_OR_EVENT))
                {
                    TC_LOG_ERROR("sql.sql", "Table `%s` has quest (ID: %u) in SCRIPT_COMMAND_QUEST_EXPLORED in `datalong` for script id %u, but quest not have flag QUEST_SPECIAL_FLAGS_EXPLORATION_OR_EVENT in quest flags. Script command or quest flags wrong. Quest modified to require objective.",
                        tableName.c_str(), tmp.QuestExplored.QuestID, tmp.id);

                    // this will prevent quest completing without objective
                    const_cast<Quest*>(quest)->SetSpecialFlag(QUEST_SPECIAL_FLAGS_EXPLORATION_OR_EVENT);

                    // continue; - quest objective requirement set and command can be allowed
                }

                if (float(tmp.QuestExplored.Distance) > DEFAULT_VISIBILITY_DISTANCE)
                {
                    TC_LOG_ERROR("sql.sql", "Table `%s` has too large distance (%u) for exploring objective complete in `datalong2` in SCRIPT_COMMAND_QUEST_EXPLORED in `datalong` for script id %u",
                        tableName.c_str(), tmp.QuestExplored.Distance, tmp.id);
                    continue;
                }

                if (tmp.QuestExplored.Distance && float(tmp.QuestExplored.Distance) > DEFAULT_VISIBILITY_DISTANCE)
                {
                    TC_LOG_ERROR("sql.sql", "Table `%s` has too large distance (%u) for exploring objective complete in `datalong2` in SCRIPT_COMMAND_QUEST_EXPLORED in `datalong` for script id %u, max distance is %f or 0 for disable distance check",
                        tableName.c_str(), tmp.QuestExplored.Distance, tmp.id, DEFAULT_VISIBILITY_DISTANCE);
                    continue;
                }

                if (tmp.QuestExplored.Distance && float(tmp.QuestExplored.Distance) < INTERACTION_DISTANCE)
                {
                    TC_LOG_ERROR("sql.sql", "Table `%s` has too small distance (%u) for exploring objective complete in `datalong2` in SCRIPT_COMMAND_QUEST_EXPLORED in `datalong` for script id %u, min distance is %f or 0 for disable distance check",
                        tableName.c_str(), tmp.QuestExplored.Distance, tmp.id, INTERACTION_DISTANCE);
                    continue;
                }

                break;
            }

            case SCRIPT_COMMAND_KILL_CREDIT:
            {
                if (!GetCreatureTemplate(tmp.KillCredit.CreatureEntry))
                {
                    TC_LOG_ERROR("sql.sql", "Table `%s` has invalid creature (Entry: %u) in SCRIPT_COMMAND_KILL_CREDIT for script id %u",
                        tableName.c_str(), tmp.KillCredit.CreatureEntry, tmp.id);
                    continue;
                }
                break;
            }

            case SCRIPT_COMMAND_RESPAWN_GAMEOBJECT:
            {
                GameObjectData const* data = GetGameObjectData(tmp.RespawnGameobject.GOGuid);
                if (!data)
                {
                    TC_LOG_ERROR("sql.sql", "Table `%s` has invalid gameobject (GUID: %u) in SCRIPT_COMMAND_RESPAWN_GAMEOBJECT for script id %u",
                        tableName.c_str(), tmp.RespawnGameobject.GOGuid, tmp.id);
                    continue;
                }

                GameObjectTemplate const* info = GetGameObjectTemplate(data->id);
                if (!info)
                {
                    TC_LOG_ERROR("sql.sql", "Table `%s` has gameobject with invalid entry (GUID: %u Entry: %u) in SCRIPT_COMMAND_RESPAWN_GAMEOBJECT for script id %u",
                        tableName.c_str(), tmp.RespawnGameobject.GOGuid, data->id, tmp.id);
                    continue;
                }

                if (info->type == GAMEOBJECT_TYPE_FISHINGNODE ||
                    info->type == GAMEOBJECT_TYPE_FISHINGHOLE ||
                    info->type == GAMEOBJECT_TYPE_DOOR        ||
                    info->type == GAMEOBJECT_TYPE_BUTTON      ||
                    info->type == GAMEOBJECT_TYPE_TRAP)
                {
                    TC_LOG_ERROR("sql.sql", "Table `%s` has gameobject type (%u) unsupported by command SCRIPT_COMMAND_RESPAWN_GAMEOBJECT for script id %u",
                        tableName.c_str(), info->entry, tmp.id);
                    continue;
                }
                break;
            }

            case SCRIPT_COMMAND_TEMP_SUMMON_CREATURE:
            {
                if (!Trinity::IsValidMapCoord(tmp.TempSummonCreature.PosX, tmp.TempSummonCreature.PosY, tmp.TempSummonCreature.PosZ, tmp.TempSummonCreature.Orientation))
                {
                    TC_LOG_ERROR("sql.sql", "Table `%s` has invalid coordinates (X: %f Y: %f Z: %f O: %f) in SCRIPT_COMMAND_TEMP_SUMMON_CREATURE for script id %u",
                        tableName.c_str(), tmp.TempSummonCreature.PosX, tmp.TempSummonCreature.PosY, tmp.TempSummonCreature.PosZ, tmp.TempSummonCreature.Orientation, tmp.id);
                    continue;
                }

                if (!GetCreatureTemplate(tmp.TempSummonCreature.CreatureEntry))
                {
                    TC_LOG_ERROR("sql.sql", "Table `%s` has invalid creature (Entry: %u) in SCRIPT_COMMAND_TEMP_SUMMON_CREATURE for script id %u",
                        tableName.c_str(), tmp.TempSummonCreature.CreatureEntry, tmp.id);
                    continue;
                }
                break;
            }

            case SCRIPT_COMMAND_OPEN_DOOR:
            case SCRIPT_COMMAND_CLOSE_DOOR:
            {
                GameObjectData const* data = GetGameObjectData(tmp.ToggleDoor.GOGuid);
                if (!data)
                {
                    TC_LOG_ERROR("sql.sql", "Table `%s` has invalid gameobject (GUID: %u) in %s for script id %u",
                        tableName.c_str(), tmp.ToggleDoor.GOGuid, GetScriptCommandName(tmp.command).c_str(), tmp.id);
                    continue;
                }

                GameObjectTemplate const* info = GetGameObjectTemplate(data->id);
                if (!info)
                {
                    TC_LOG_ERROR("sql.sql", "Table `%s` has gameobject with invalid entry (GUID: %u Entry: %u) in %s for script id %u",
                        tableName.c_str(), tmp.ToggleDoor.GOGuid, data->id, GetScriptCommandName(tmp.command).c_str(), tmp.id);
                    continue;
                }

                if (info->type != GAMEOBJECT_TYPE_DOOR)
                {
                    TC_LOG_ERROR("sql.sql", "Table `%s` has gameobject type (%u) unsupported by command %s for script id %u",
                        tableName.c_str(), info->entry, GetScriptCommandName(tmp.command).c_str(), tmp.id);
                    continue;
                }

                break;
            }

            case SCRIPT_COMMAND_REMOVE_AURA:
            {
                if (!sSpellMgr->GetSpellInfo(tmp.RemoveAura.SpellID, DIFFICULTY_NONE))
                {
                    TC_LOG_ERROR("sql.sql", "Table `%s` using non-existent spell (id: %u) in SCRIPT_COMMAND_REMOVE_AURA for script id %u",
                        tableName.c_str(), tmp.RemoveAura.SpellID, tmp.id);
                    continue;
                }
                if (tmp.RemoveAura.Flags & ~0x1)                    // 1 bits (0, 1)
                {
                    TC_LOG_ERROR("sql.sql", "Table `%s` using unknown flags in datalong2 (%u) in SCRIPT_COMMAND_REMOVE_AURA for script id %u",
                        tableName.c_str(), tmp.RemoveAura.Flags, tmp.id);
                    continue;
                }
                break;
            }

            case SCRIPT_COMMAND_CAST_SPELL:
            {
                if (!sSpellMgr->GetSpellInfo(tmp.CastSpell.SpellID, DIFFICULTY_NONE))
                {
                    TC_LOG_ERROR("sql.sql", "Table `%s` using non-existent spell (id: %u) in SCRIPT_COMMAND_CAST_SPELL for script id %u",
                        tableName.c_str(), tmp.CastSpell.SpellID, tmp.id);
                    continue;
                }
                if (tmp.CastSpell.Flags > 4)                      // targeting type
                {
                    TC_LOG_ERROR("sql.sql", "Table `%s` using unknown target in datalong2 (%u) in SCRIPT_COMMAND_CAST_SPELL for script id %u",
                        tableName.c_str(), tmp.CastSpell.Flags, tmp.id);
                    continue;
                }
                if (tmp.CastSpell.Flags != 4 && tmp.CastSpell.CreatureEntry & ~0x1)                      // 1 bit (0, 1)
                {
                    TC_LOG_ERROR("sql.sql", "Table `%s` using unknown flags in dataint (%u) in SCRIPT_COMMAND_CAST_SPELL for script id %u",
                        tableName.c_str(), tmp.CastSpell.CreatureEntry, tmp.id);
                    continue;
                }
                else if (tmp.CastSpell.Flags == 4 && !GetCreatureTemplate(tmp.CastSpell.CreatureEntry))
                {
                    TC_LOG_ERROR("sql.sql", "Table `%s` using invalid creature entry in dataint (%u) in SCRIPT_COMMAND_CAST_SPELL for script id %u",
                        tableName.c_str(), tmp.CastSpell.CreatureEntry, tmp.id);
                    continue;
                }
                break;
            }

            case SCRIPT_COMMAND_CREATE_ITEM:
            {
                if (!GetItemTemplate(tmp.CreateItem.ItemEntry))
                {
                    TC_LOG_ERROR("sql.sql", "Table `%s` has nonexistent item (entry: %u) in SCRIPT_COMMAND_CREATE_ITEM for script id %u",
                        tableName.c_str(), tmp.CreateItem.ItemEntry, tmp.id);
                    continue;
                }
                if (!tmp.CreateItem.Amount)
                {
                    TC_LOG_ERROR("sql.sql", "Table `%s` SCRIPT_COMMAND_CREATE_ITEM but amount is %u for script id %u",
                        tableName.c_str(), tmp.CreateItem.Amount, tmp.id);
                    continue;
                }
                break;
            }
            case SCRIPT_COMMAND_PLAY_ANIMKIT:
            {
                if (!sAnimKitStore.LookupEntry(tmp.PlayAnimKit.AnimKitID))
                {
                    TC_LOG_ERROR("sql.sql", "Table `%s` has invalid AnimKid id (datalong = %u) in SCRIPT_COMMAND_PLAY_ANIMKIT for script id %u",
                        tableName.c_str(), tmp.PlayAnimKit.AnimKitID, tmp.id);
                    continue;
                }
                break;
            }
            case SCRIPT_COMMAND_FIELD_SET_DEPRECATED:
            case SCRIPT_COMMAND_FLAG_SET_DEPRECATED:
            case SCRIPT_COMMAND_FLAG_REMOVE_DEPRECATED:
            {
                TC_LOG_ERROR("sql.sql", "Table `%s` uses deprecated direct updatefield modify command %s for script id %u", tableName.c_str(), GetScriptCommandName(tmp.command).c_str(), tmp.id);
                continue;
            }
            default:
                break;
        }

        if (scripts->find(tmp.id) == scripts->end())
        {
            ScriptMap emptyMap;
            (*scripts)[tmp.id] = emptyMap;
        }
        (*scripts)[tmp.id].insert(std::pair<uint32, ScriptInfo>(tmp.delay, tmp));

        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u script definitions in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadSpellScripts()
{
    LoadScripts(SCRIPTS_SPELL);

    // check ids
    for (ScriptMapMap::const_iterator itr = sSpellScripts.begin(); itr != sSpellScripts.end(); ++itr)
    {
        uint32 spellId = uint32(itr->first) & 0x00FFFFFF;
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);

        if (!spellInfo)
        {
            TC_LOG_ERROR("sql.sql", "Table `spell_scripts` has not existing spell (Id: %u) as script id", spellId);
            continue;
        }

        SpellEffIndex i = SpellEffIndex((uint32(itr->first) >> 24) & 0x000000FF);
        if (uint32(i) >= spellInfo->GetEffects().size())
        {
            TC_LOG_ERROR("sql.sql", "Table `spell_scripts` has too high effect index %u for spell (Id: %u) as script id", uint32(i), spellId);
            continue;
        }

        //check for correct spellEffect
        if (!spellInfo->GetEffect(i).Effect || (spellInfo->GetEffect(i).Effect != SPELL_EFFECT_SCRIPT_EFFECT && spellInfo->GetEffect(i).Effect != SPELL_EFFECT_DUMMY))
            TC_LOG_ERROR("sql.sql", "Table `spell_scripts` - spell %u effect %u is not SPELL_EFFECT_SCRIPT_EFFECT or SPELL_EFFECT_DUMMY", spellId, uint32(i));
    }
}

void ObjectMgr::LoadEventScripts()
{
    LoadScripts(SCRIPTS_EVENT);

    std::set<uint32> evt_scripts;
    // Load all possible script entries from gameobjects
    for (auto const& gameObjectTemplatePair : _gameObjectTemplateStore)
        if (uint32 eventId = gameObjectTemplatePair.second.GetEventScriptId())
            evt_scripts.insert(eventId);

    // Load all possible script entries from spells
    for (SpellNameEntry const* spellNameEntry : sSpellNameStore)
        if (SpellInfo const* spell = sSpellMgr->GetSpellInfo(spellNameEntry->ID, DIFFICULTY_NONE))
            for (SpellEffectInfo const& spellEffectInfo : spell->GetEffects())
                if (spellEffectInfo.IsEffect(SPELL_EFFECT_SEND_EVENT))
                    if (spellEffectInfo.MiscValue)
                        evt_scripts.insert(spellEffectInfo.MiscValue);

    for (size_t path_idx = 0; path_idx < sTaxiPathNodesByPath.size(); ++path_idx)
    {
        for (size_t node_idx = 0; node_idx < sTaxiPathNodesByPath[path_idx].size(); ++node_idx)
        {
            TaxiPathNodeEntry const* node = sTaxiPathNodesByPath[path_idx][node_idx];

            if (node->ArrivalEventID)
                evt_scripts.insert(node->ArrivalEventID);

            if (node->DepartureEventID)
                evt_scripts.insert(node->DepartureEventID);
        }
    }

    // Then check if all scripts are in above list of possible script entries
    for (ScriptMapMap::const_iterator itr = sEventScripts.begin(); itr != sEventScripts.end(); ++itr)
    {
        std::set<uint32>::const_iterator itr2 = evt_scripts.find(itr->first);
        if (itr2 == evt_scripts.end())
            TC_LOG_ERROR("sql.sql", "Table `event_scripts` has script (Id: %u) not referring to any gameobject_template type 10 data2 field, type 3 data6 field, type 13 data 2 field or any spell effect %u",
                itr->first, SPELL_EFFECT_SEND_EVENT);
    }
}

//Load WP Scripts
void ObjectMgr::LoadWaypointScripts()
{
    LoadScripts(SCRIPTS_WAYPOINT);

    std::set<uint32> actionSet;

    for (ScriptMapMap::const_iterator itr = sWaypointScripts.begin(); itr != sWaypointScripts.end(); ++itr)
        actionSet.insert(itr->first);

    WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_WAYPOINT_DATA_ACTION);
    PreparedQueryResult result = WorldDatabase.Query(stmt);

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 action = fields[0].GetUInt32();

            actionSet.erase(action);
        }
        while (result->NextRow());
    }

    for (std::set<uint32>::iterator itr = actionSet.begin(); itr != actionSet.end(); ++itr)
        TC_LOG_ERROR("sql.sql", "There is no waypoint which links to the waypoint script %u", *itr);
}

void ObjectMgr::LoadSpellScriptNames()
{
    uint32 oldMSTime = getMSTime();

    _spellScriptsStore.clear();                            // need for reload case

    QueryResult result = WorldDatabase.Query("SELECT spell_id, ScriptName FROM spell_script_names");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 spell script names. DB table `spell_script_names` is empty!");
        return;
    }

    uint32 count = 0;

    do
    {

        Field* fields = result->Fetch();

        int32 spellId                = fields[0].GetInt32();
        std::string const scriptName = fields[1].GetString();

        bool allRanks = false;
        if (spellId < 0)
        {
            allRanks = true;
            spellId = -spellId;
        }

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE);
        if (!spellInfo)
        {
            TC_LOG_ERROR("sql.sql", "Scriptname: `%s` spell (Id: %d) does not exist.", scriptName.c_str(), fields[0].GetInt32());
            continue;
        }

        if (allRanks)
        {
            if (!spellInfo->IsRanked())
                TC_LOG_ERROR("sql.sql", "Scriptname: `%s` spell (Id: %d) has no ranks of spell.", scriptName.c_str(), fields[0].GetInt32());

            if (spellInfo->GetFirstRankSpell()->Id != uint32(spellId))
            {
                TC_LOG_ERROR("sql.sql", "Scriptname: `%s` spell (Id: %d) is not first rank of spell.", scriptName.c_str(), fields[0].GetInt32());
                continue;
            }
            while (spellInfo)
            {
                _spellScriptsStore.insert(SpellScriptsContainer::value_type(spellInfo->Id, std::make_pair(GetScriptId(scriptName), true)));
                spellInfo = spellInfo->GetNextRankSpell();
            }
        }
        else
        {
            if (spellInfo->IsRanked())
                TC_LOG_ERROR("sql.sql", "Scriptname: `%s` spell (Id: %d) is ranked spell. Perhaps not all ranks are assigned to this script.", scriptName.c_str(), spellId);

            _spellScriptsStore.insert(SpellScriptsContainer::value_type(spellInfo->Id, std::make_pair(GetScriptId(scriptName), true)));
        }

        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u spell script names in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::ValidateSpellScripts()
{
    uint32 oldMSTime = getMSTime();

    if (_spellScriptsStore.empty())
    {
        TC_LOG_INFO("server.loading", ">> Validated 0 scripts.");
        return;
    }

    uint32 count = 0;

    for (auto& spell : _spellScriptsStore)
    {
        SpellInfo const* spellEntry = sSpellMgr->GetSpellInfo(spell.first, DIFFICULTY_NONE);

        if (SpellScriptLoader* spellScriptLoader = sScriptMgr->GetSpellScriptLoader(spell.second.first))
        {
            ++count;

            std::unique_ptr<SpellScript> spellScript(spellScriptLoader->GetSpellScript());
            std::unique_ptr<AuraScript> auraScript(spellScriptLoader->GetAuraScript());

            if (!spellScript && !auraScript)
            {
                TC_LOG_ERROR("scripts", "Functions GetSpellScript() and GetAuraScript() of script `%s` do not return objects - script skipped", GetScriptName(spell.second.first).c_str());

                spell.second.second = false;
                continue;
            }

            if (spellScript)
            {
                spellScript->_Init(&spellScriptLoader->GetName(), spellEntry->Id);
                spellScript->_Register();

                if (!spellScript->_Validate(spellEntry))
                {
                    spell.second.second = false;
                    continue;
                }
            }

            if (auraScript)
            {
                auraScript->_Init(&spellScriptLoader->GetName(), spellEntry->Id);
                auraScript->_Register();

                if (!auraScript->_Validate(spellEntry))
                {
                    spell.second.second = false;
                    continue;
                }
            }

            // Enable the script when all checks passed
            spell.second.second = true;
        }
        else
            spell.second.second = false;
    }

    TC_LOG_INFO("server.loading", ">> Validated %u scripts in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadPageTexts()
{
    uint32 oldMSTime = getMSTime();

    //                                               0   1     2           3                  4
    QueryResult result = WorldDatabase.Query("SELECT ID, Text, NextPageID, PlayerConditionID, Flags FROM page_text");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 page texts. DB table `page_text` is empty!");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 id           = fields[0].GetUInt32();

        PageText& pageText = _pageTextStore[id];
        pageText.Text       = fields[1].GetString();
        pageText.NextPageID = fields[2].GetUInt32();
        pageText.PlayerConditionID = fields[3].GetInt32();
        pageText.Flags = fields[4].GetUInt8();

        ++count;
    }
    while (result->NextRow());

    for (PageTextContainer::const_iterator itr = _pageTextStore.begin(); itr != _pageTextStore.end(); ++itr)
        if (itr->second.NextPageID)
            if (_pageTextStore.find(itr->second.NextPageID) == _pageTextStore.end())
                TC_LOG_ERROR("sql.sql", "Page text (ID: %u) has non-existing `NextPageID` (%u)", itr->first, itr->second.NextPageID);

    TC_LOG_INFO("server.loading", ">> Loaded %u page texts in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

PageText const* ObjectMgr::GetPageText(uint32 pageEntry)
{
    PageTextContainer::const_iterator itr = _pageTextStore.find(pageEntry);
    if (itr != _pageTextStore.end())
        return &(itr->second);

    return nullptr;
}

void ObjectMgr::LoadPageTextLocales()
{
    uint32 oldMSTime = getMSTime();

    _pageTextLocaleStore.clear(); // needed for reload case

    //                                               0      1     2
    QueryResult result = WorldDatabase.Query("SELECT ID, locale, Text FROM page_text_locale");
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();

        uint32 id                   = fields[0].GetUInt32();
        std::string localeName      = fields[1].GetString();

        LocaleConstant locale = GetLocaleByName(localeName);
        if (!IsValidLocale(locale) || locale == LOCALE_enUS)
            continue;

        PageTextLocale& data = _pageTextLocaleStore[id];
        AddLocaleString(fields[2].GetString(), locale, data.Text);
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u PageText locale strings in %u ms", uint32(_pageTextLocaleStore.size()), GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadInstanceTemplate()
{
    uint32 oldMSTime = getMSTime();

    //                                                0     1       2
    QueryResult result = WorldDatabase.Query("SELECT map, parent, script FROM instance_template");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 instance templates. DB table `page_text` is empty!");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint16 mapID = fields[0].GetUInt16();

        if (!MapManager::IsValidMAP(mapID, true))
        {
            TC_LOG_ERROR("sql.sql", "ObjectMgr::LoadInstanceTemplate: bad mapid %d for template!", mapID);
            continue;
        }

        InstanceTemplate instanceTemplate;

        instanceTemplate.Parent     = uint32(fields[1].GetUInt16());
        instanceTemplate.ScriptId   = sObjectMgr->GetScriptId(fields[2].GetString());

        _instanceTemplateStore[mapID] = instanceTemplate;

        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u instance templates in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

InstanceTemplate const* ObjectMgr::GetInstanceTemplate(uint32 mapID) const
{
    InstanceTemplateContainer::const_iterator itr = _instanceTemplateStore.find(uint16(mapID));
    if (itr != _instanceTemplateStore.end())
        return &(itr->second);

    return nullptr;
}

void ObjectMgr::LoadInstanceEncounters()
{
    uint32 oldMSTime = getMSTime();

    //                                                 0         1            2                3
    QueryResult result = WorldDatabase.Query("SELECT entry, creditType, creditEntry, lastEncounterDungeon FROM instance_encounters");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 instance encounters, table is empty!");
        return;
    }

    uint32 count = 0;
    std::map<uint32, std::pair<uint32, DungeonEncounterEntry const*>> dungeonLastBosses;
    do
    {
        Field* fields = result->Fetch();
        uint32 entry = fields[0].GetUInt32();
        uint8 creditType = fields[1].GetUInt8();
        uint32 creditEntry = fields[2].GetUInt32();
        uint32 lastEncounterDungeon = fields[3].GetUInt16();
        DungeonEncounterEntry const* dungeonEncounter = sDungeonEncounterStore.LookupEntry(entry);
        if (!dungeonEncounter)
        {
            TC_LOG_ERROR("sql.sql", "Table `instance_encounters` has an invalid encounter id %u, skipped!", entry);
            continue;
        }

        if (lastEncounterDungeon && !sLFGMgr->GetLFGDungeonEntry(lastEncounterDungeon))
        {
            TC_LOG_ERROR("sql.sql", "Table `instance_encounters` has an encounter %u (%s) marked as final for invalid dungeon id %u, skipped!",
                entry, dungeonEncounter->Name[sWorld->GetDefaultDbcLocale()], lastEncounterDungeon);
            continue;
        }

        auto itr = dungeonLastBosses.find(lastEncounterDungeon);
        if (lastEncounterDungeon)
        {
            if (itr != dungeonLastBosses.end())
            {
                TC_LOG_ERROR("sql.sql", "Table `instance_encounters` specified encounter %u (%s) as last encounter but %u (%s) is already marked as one, skipped!",
                    entry, dungeonEncounter->Name[sWorld->GetDefaultDbcLocale()], itr->second.first, itr->second.second->Name[sWorld->GetDefaultDbcLocale()]);
                continue;
            }

            dungeonLastBosses[lastEncounterDungeon] = std::make_pair(entry, dungeonEncounter);
        }

        switch (creditType)
        {
            case ENCOUNTER_CREDIT_KILL_CREATURE:
            {
                CreatureTemplate const* creatureInfo = GetCreatureTemplate(creditEntry);
                if (!creatureInfo)
                {
                    TC_LOG_ERROR("sql.sql", "Table `instance_encounters` has an invalid creature (entry %u) linked to the encounter %u (%s), skipped!",
                        creditEntry, entry, dungeonEncounter->Name[sWorld->GetDefaultDbcLocale()]);
                    continue;
                }
                const_cast<CreatureTemplate*>(creatureInfo)->flags_extra |= CREATURE_FLAG_EXTRA_DUNGEON_BOSS;
                for (uint8 diff = 0; diff < MAX_CREATURE_DIFFICULTIES; ++diff)
                {
                    if (uint32 diffEntry = creatureInfo->DifficultyEntry[diff])
                    {
                        if (CreatureTemplate const* diffInfo = GetCreatureTemplate(diffEntry))
                            const_cast<CreatureTemplate*>(diffInfo)->flags_extra |= CREATURE_FLAG_EXTRA_DUNGEON_BOSS;
                    }
                }
                break;
            }
            case ENCOUNTER_CREDIT_CAST_SPELL:
                if (!sSpellMgr->GetSpellInfo(creditEntry, DIFFICULTY_NONE))
                {
                    TC_LOG_ERROR("sql.sql", "Table `instance_encounters` has an invalid spell (entry %u) linked to the encounter %u (%s), skipped!",
                        creditEntry, entry, dungeonEncounter->Name[sWorld->GetDefaultDbcLocale()]);
                    continue;
                }
                break;
            default:
                TC_LOG_ERROR("sql.sql", "Table `instance_encounters` has an invalid credit type (%u) for encounter %u (%s), skipped!",
                    creditType, entry, dungeonEncounter->Name[sWorld->GetDefaultDbcLocale()]);
                continue;
        }

        if (!dungeonEncounter->DifficultyID)
        {
            for (DifficultyEntry const* difficulty : sDifficultyStore)
            {
                if (sDB2Manager.GetMapDifficultyData(dungeonEncounter->MapID, Difficulty(difficulty->ID)))
                {
                    DungeonEncounterList& encounters = _dungeonEncounterStore[MAKE_PAIR64(dungeonEncounter->MapID, difficulty->ID)];
                    encounters.emplace_back(dungeonEncounter, EncounterCreditType(creditType), creditEntry, lastEncounterDungeon);
                }
            }
        }
        else
        {
            DungeonEncounterList& encounters = _dungeonEncounterStore[MAKE_PAIR64(dungeonEncounter->MapID, dungeonEncounter->DifficultyID)];
            encounters.emplace_back(dungeonEncounter, EncounterCreditType(creditType), creditEntry, lastEncounterDungeon);
        }

        ++count;
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u instance encounters in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

NpcText const* ObjectMgr::GetNpcText(uint32 Text_ID) const
{
    NpcTextContainer::const_iterator itr = _npcTextStore.find(Text_ID);
    if (itr != _npcTextStore.end())
        return &itr->second;
    return nullptr;
}

void ObjectMgr::LoadNPCText()
{
    uint32 oldMSTime = getMSTime();

    QueryResult result = WorldDatabase.Query("SELECT ID, "
        "Probability0, Probability1, Probability2, Probability3, Probability4, Probability5, Probability6, Probability7, "
        "BroadcastTextID0, BroadcastTextID1, BroadcastTextID2, BroadcastTextID3, BroadcastTextID4, BroadcastTextID5, BroadcastTextID6, BroadcastTextID7"
        " FROM npc_text");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 npc texts, table is empty!");
        return;
    }

    _npcTextStore.reserve(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();

        uint32 textID = fields[0].GetUInt32();
        if (!textID)
        {
            TC_LOG_ERROR("sql.sql", "Table `npc_text` has record with reserved id 0, ignore.");
            continue;
        }

        NpcText& npcText = _npcTextStore[textID];

        for (uint8 i = 0; i < MAX_NPC_TEXT_OPTIONS; ++i)
        {
            npcText.Data[i].Probability      = fields[1 + i].GetFloat();
            npcText.Data[i].BroadcastTextID  = fields[9 + i].GetUInt32();
        }

        for (uint8 i = 0; i < MAX_NPC_TEXT_OPTIONS; i++)
        {
            if (npcText.Data[i].BroadcastTextID)
            {
                if (!sBroadcastTextStore.LookupEntry(npcText.Data[i].BroadcastTextID))
                {
                    TC_LOG_ERROR("sql.sql", "NPCText (ID: %u) has a non-existing or incompatible BroadcastText (ID: %u, Index: %u)", textID, npcText.Data[i].BroadcastTextID, i);
                    npcText.Data[i].BroadcastTextID = 0;
                }
            }
        }

        for (uint8 i = 0; i < MAX_NPC_TEXT_OPTIONS; i++)
        {
            if (npcText.Data[i].Probability > 0 && npcText.Data[i].BroadcastTextID == 0)
            {
                TC_LOG_ERROR("sql.sql", "NPCText (ID: %u) has a probability (Index: %u) set, but no BroadcastTextID to go with it", textID, i);
                npcText.Data[i].Probability = 0;
            }
        }
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u npc texts in %u ms", uint32(_npcTextStore.size()), GetMSTimeDiffToNow(oldMSTime));
}

//not very fast function but it is called only once a day, or on starting-up
void ObjectMgr::ReturnOrDeleteOldMails(bool serverUp)
{
    uint32 oldMSTime = getMSTime();

    time_t curTime = GameTime::GetGameTime();
    tm lt;
    localtime_r(&curTime, &lt);
    TC_LOG_INFO("misc", "Returning mails current time: hour: %d, minute: %d, second: %d ", lt.tm_hour, lt.tm_min, lt.tm_sec);

    // Delete all old mails without item and without body immediately, if starting server
    if (!serverUp)
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_EMPTY_EXPIRED_MAIL);
        stmt->setInt64(0, curTime);
        CharacterDatabase.Execute(stmt);
    }
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_EXPIRED_MAIL);
    stmt->setInt64(0, curTime);
    PreparedQueryResult result = CharacterDatabase.Query(stmt);
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> No expired mails found.");
        return;                                             // any mails need to be returned or deleted
    }

    std::map<uint32 /*messageId*/, MailItemInfoVec> itemsCache;
    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_EXPIRED_MAIL_ITEMS);
    stmt->setUInt32(0, curTime);
    if (PreparedQueryResult items = CharacterDatabase.Query(stmt))
    {
        MailItemInfo item;
        do
        {
            Field* fields = items->Fetch();
            item.item_guid = fields[0].GetUInt64();
            item.item_template = fields[1].GetUInt32();
            uint32 mailId = fields[2].GetUInt32();
            itemsCache[mailId].push_back(item);
        } while (items->NextRow());
    }

    uint32 deletedCount = 0;
    uint32 returnedCount = 0;
    do
    {
        Field* fields = result->Fetch();
        Mail* m = new Mail;
        m->messageID      = fields[0].GetUInt32();
        m->messageType    = fields[1].GetUInt8();
        m->sender         = fields[2].GetUInt64();
        m->receiver       = fields[3].GetUInt64();
        bool has_items    = fields[4].GetBool();
        m->expire_time    = fields[5].GetInt64();
        m->deliver_time   = 0;
        m->COD            = fields[6].GetUInt64();
        m->checked        = fields[7].GetUInt8();
        m->mailTemplateId = fields[8].GetInt16();

        Player* player = nullptr;
        if (serverUp)
            player = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(m->receiver));

        if (player && player->m_mailsLoaded)
        {                                                   // this code will run very improbably (the time is between 4 and 5 am, in game is online a player, who has old mail
            // his in mailbox and he has already listed his mails)
            delete m;
            continue;
        }

        // Delete or return mail
        if (has_items)
        {
            // read items from cache
            m->items.swap(itemsCache[m->messageID]);

            // if it is mail from non-player, or if it's already return mail, it shouldn't be returned, but deleted
            if (m->messageType != MAIL_NORMAL || (m->checked & (MAIL_CHECK_MASK_COD_PAYMENT | MAIL_CHECK_MASK_RETURNED)))
            {
                CharacterDatabaseTransaction nonTransactional(nullptr);
                // mail open and then not returned
                for (MailItemInfoVec::iterator itr2 = m->items.begin(); itr2 != m->items.end(); ++itr2)
                {
                    Item::DeleteFromDB(nonTransactional, itr2->item_guid);
                    AzeriteItem::DeleteFromDB(nonTransactional, itr2->item_guid);
                    AzeriteEmpoweredItem::DeleteFromDB(nonTransactional, itr2->item_guid);
                }

                stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_MAIL_ITEM_BY_ID);
                stmt->setUInt32(0, m->messageID);
                CharacterDatabase.Execute(stmt);
            }
            else
            {
                // Mail will be returned
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_MAIL_RETURNED);
                stmt->setUInt64(0, m->receiver);
                stmt->setUInt64(1, m->sender);
                stmt->setInt64 (2, curTime + 30 * DAY);
                stmt->setInt64 (3, curTime);
                stmt->setUInt8 (4, uint8(MAIL_CHECK_MASK_RETURNED));
                stmt->setUInt32(5, m->messageID);
                CharacterDatabase.Execute(stmt);
                for (MailItemInfoVec::iterator itr2 = m->items.begin(); itr2 != m->items.end(); ++itr2)
                {
                    // Update receiver in mail items for its proper delivery, and in instance_item for avoid lost item at sender delete
                    stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_MAIL_ITEM_RECEIVER);
                    stmt->setUInt64(0, m->sender);
                    stmt->setUInt64(1, itr2->item_guid);
                    CharacterDatabase.Execute(stmt);

                    stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_ITEM_OWNER);
                    stmt->setUInt64(0, m->sender);
                    stmt->setUInt64(1, itr2->item_guid);
                    CharacterDatabase.Execute(stmt);
                }
                delete m;
                ++returnedCount;
                continue;
            }
        }

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_MAIL_BY_ID);
        stmt->setUInt32(0, m->messageID);
        CharacterDatabase.Execute(stmt);
        delete m;
        ++deletedCount;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Processed %u expired mails: %u deleted and %u returned in %u ms", deletedCount + returnedCount, deletedCount, returnedCount, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadQuestAreaTriggers()
{
    uint32 oldMSTime = getMSTime();

    _questAreaTriggerStore.clear();                           // need for reload case

    QueryResult result = WorldDatabase.Query("SELECT id, quest FROM areatrigger_involvedrelation");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 quest trigger points. DB table `areatrigger_involvedrelation` is empty.");
        return;
    }

    uint32 count = 0;

    do
    {
        ++count;

        Field* fields = result->Fetch();

        uint32 trigger_ID = fields[0].GetUInt32();
        uint32 quest_ID   = fields[1].GetUInt32();

        AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(trigger_ID);
        if (!atEntry)
        {
            TC_LOG_ERROR("sql.sql", "Area trigger (ID:%u) does not exist in `AreaTrigger.dbc`.", trigger_ID);
            continue;
        }

        Quest const* quest = GetQuestTemplate(quest_ID);

        if (!quest)
        {
            TC_LOG_ERROR("sql.sql", "Table `areatrigger_involvedrelation` has record (id: %u) for not existing quest %u", trigger_ID, quest_ID);
            continue;
        }

        if (!quest->HasSpecialFlag(QUEST_SPECIAL_FLAGS_EXPLORATION_OR_EVENT))
        {
            TC_LOG_ERROR("sql.sql", "Table `areatrigger_involvedrelation` has record (id: %u) for not quest %u, but quest not have flag QUEST_SPECIAL_FLAGS_EXPLORATION_OR_EVENT. Trigger or quest flags must be fixed, quest modified to require objective.", trigger_ID, quest_ID);

            // this will prevent quest completing without objective
            const_cast<Quest*>(quest)->SetSpecialFlag(QUEST_SPECIAL_FLAGS_EXPLORATION_OR_EVENT);

            // continue; - quest modified to required objective and trigger can be allowed.
        }

        _questAreaTriggerStore[trigger_ID].insert(quest_ID);

    } while (result->NextRow());

    for (auto const& pair : _questObjectives)
    {
        QuestObjective const* objective = pair.second;
        if (objective->Type == QUEST_OBJECTIVE_AREATRIGGER)
            _questAreaTriggerStore[objective->ObjectID].insert(objective->QuestID);
    }

    TC_LOG_INFO("server.loading", ">> Loaded %u quest trigger points in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

QuestGreeting const* ObjectMgr::GetQuestGreeting(TypeID type, uint32 id) const
{
    uint32 typeIndex;
    if (type == TYPEID_UNIT)
        typeIndex = 0;
    else if (type == TYPEID_GAMEOBJECT)
        typeIndex = 1;
    else
        return nullptr;

    return Trinity::Containers::MapGetValuePtr(_questGreetingStore[typeIndex], id);
}

QuestGreetingLocale const* ObjectMgr::GetQuestGreetingLocale(TypeID type, uint32 id) const
{
    uint32 typeIndex;
    if (type == TYPEID_UNIT)
        typeIndex = 0;
    else if (type == TYPEID_GAMEOBJECT)
        typeIndex = 1;
    else
        return nullptr;

    return Trinity::Containers::MapGetValuePtr(_questGreetingLocaleStore[typeIndex], id);
}

void ObjectMgr::LoadQuestGreetings()
{
    uint32 oldMSTime = getMSTime();

    for (std::size_t i = 0; i < _questGreetingStore.size(); ++i)
        _questGreetingStore[i].clear();

    //                                                0   1          2                3
    QueryResult result = WorldDatabase.Query("SELECT ID, type, GreetEmoteType, GreetEmoteDelay, Greeting FROM quest_greeting");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 npc texts, table is empty!");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 id                   = fields[0].GetUInt32();
        uint8 type                  = fields[1].GetUInt8();
        switch (type)
        {
            case 0: // Creature
                if (!sObjectMgr->GetCreatureTemplate(id))
                {
                    TC_LOG_ERROR("sql.sql", "Table `quest_greeting`: creature template entry %u does not exist.", id);
                    continue;
                }
                break;
            case 1: // GameObject
                if (!sObjectMgr->GetGameObjectTemplate(id))
                {
                    TC_LOG_ERROR("sql.sql", "Table `quest_greeting`: gameobject template entry %u does not exist.", id);
                    continue;
                }
                break;
            default:
                continue;
        }

        uint16 greetEmoteType       = fields[2].GetUInt16();
        uint32 greetEmoteDelay      = fields[3].GetUInt32();
        std::string greeting        = fields[4].GetString();

        _questGreetingStore[type].emplace(std::piecewise_construct, std::forward_as_tuple(id), std::forward_as_tuple(greetEmoteType, greetEmoteDelay, std::move(greeting)));
        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u quest_greeting in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadTavernAreaTriggers()
{
    uint32 oldMSTime = getMSTime();

    _tavernAreaTriggerStore.clear();                          // need for reload case

    QueryResult result = WorldDatabase.Query("SELECT id FROM areatrigger_tavern");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 tavern triggers. DB table `areatrigger_tavern` is empty.");
        return;
    }

    uint32 count = 0;

    do
    {
        ++count;

        Field* fields = result->Fetch();

        uint32 Trigger_ID      = fields[0].GetUInt32();

        AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(Trigger_ID);
        if (!atEntry)
        {
            TC_LOG_ERROR("sql.sql", "Area trigger (ID:%u) does not exist in `AreaTrigger.dbc`.", Trigger_ID);
            continue;
        }

        _tavernAreaTriggerStore.insert(Trigger_ID);
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u tavern triggers in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadAreaTriggerScripts()
{
    uint32 oldMSTime = getMSTime();

    _areaTriggerScriptStore.clear();                            // need for reload case

    QueryResult result = WorldDatabase.Query("SELECT entry, ScriptName FROM areatrigger_scripts");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 areatrigger scripts. DB table `areatrigger_scripts` is empty.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        uint32 triggerId             = fields[0].GetUInt32();
        std::string const scriptName = fields[1].GetString();

        AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(triggerId);
        if (!atEntry)
        {
            TC_LOG_ERROR("sql.sql", "AreaTrigger (ID: %u) does not exist in `AreaTrigger.dbc`.", triggerId);
            continue;
        }
        _areaTriggerScriptStore[triggerId] = GetScriptId(scriptName);
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded " SZFMTD " areatrigger scripts in %u ms", _areaTriggerScriptStore.size(), GetMSTimeDiffToNow(oldMSTime));
}

uint32 ObjectMgr::GetNearestTaxiNode(float x, float y, float z, uint32 mapid, uint32 team)
{
    bool found = false;
    float dist = 10000;
    uint32 id = 0;

    uint32 requireFlag = (team == ALLIANCE) ? TAXI_NODE_FLAG_ALLIANCE : TAXI_NODE_FLAG_HORDE;
    for (TaxiNodesEntry const* node : sTaxiNodesStore)
    {
        if (!node || node->ContinentID != mapid || !(node->Flags & requireFlag))
            continue;

        uint32 field   = uint32((node->ID - 1) / 8);
        uint32 submask = 1 << ((node->ID - 1) % 8);

        // skip not taxi network nodes
        if ((sTaxiNodesMask[field] & submask) == 0)
            continue;

        float dist2 = (node->Pos.X - x)*(node->Pos.X - x) + (node->Pos.Y - y)*(node->Pos.Y - y) + (node->Pos.Z - z)*(node->Pos.Z - z);
        if (found)
        {
            if (dist2 < dist)
            {
                dist = dist2;
                id = node->ID;
            }
        }
        else
        {
            found = true;
            dist = dist2;
            id = node->ID;
        }
    }

    return id;
}

void ObjectMgr::GetTaxiPath(uint32 source, uint32 destination, uint32 &path, uint32 &cost)
{
    TaxiPathSetBySource::iterator src_i = sTaxiPathSetBySource.find(source);
    if (src_i == sTaxiPathSetBySource.end())
    {
        path = 0;
        cost = 0;
        return;
    }

    TaxiPathSetForSource& pathSet = src_i->second;

    TaxiPathSetForSource::iterator dest_i = pathSet.find(destination);
    if (dest_i == pathSet.end())
    {
        path = 0;
        cost = 0;
        return;
    }

    cost = dest_i->second.price;
    path = dest_i->second.ID;
}

uint32 ObjectMgr::GetTaxiMountDisplayId(uint32 id, uint32 team, bool allowed_alt_team /* = false */)
{
    CreatureModel mountModel;
    CreatureTemplate const* mount_info = nullptr;

    // select mount creature id
    TaxiNodesEntry const* node = sTaxiNodesStore.LookupEntry(id);
    if (node)
    {
        uint32 mount_entry = 0;
        if (team == ALLIANCE)
            mount_entry = node->MountCreatureID[1];
        else
            mount_entry = node->MountCreatureID[0];

        // Fix for Alliance not being able to use Acherus taxi
        // only one mount type for both sides
        if (mount_entry == 0 && allowed_alt_team)
        {
            // Simply reverse the selection. At least one team in theory should have a valid mount ID to choose.
            mount_entry = team == ALLIANCE ? node->MountCreatureID[0] : node->MountCreatureID[1];
        }

        mount_info = GetCreatureTemplate(mount_entry);
        if (mount_info)
        {
            CreatureModel const* model = mount_info->GetRandomValidModel();
            if (!model)
            {
                TC_LOG_ERROR("sql.sql", "No displayid found for the taxi mount with the entry %u! Can't load it!", mount_entry);
                return 0;
            }
            mountModel = *model;
        }
    }

    // minfo is not actually used but the mount_id was updated
    GetCreatureModelRandomGender(&mountModel, mount_info);

    return mountModel.CreatureDisplayID;
}

Quest const* ObjectMgr::GetQuestTemplate(uint32 quest_id) const
{
    return Trinity::Containers::MapGetValuePtr(_questTemplates, quest_id);
}

void ObjectMgr::LoadGraveyardZones()
{
    uint32 oldMSTime = getMSTime();

    GraveyardStore.clear(); // needed for reload case

    //                                               0   1          2
    QueryResult result = WorldDatabase.Query("SELECT ID, GhostZone, Faction FROM graveyard_zone");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 graveyard-zone links. DB table `graveyard_zone` is empty.");
        return;
    }

    uint32 count = 0;

    do
    {
        ++count;

        Field* fields = result->Fetch();

        uint32 safeLocId = fields[0].GetUInt32();
        uint32 zoneId = fields[1].GetUInt32();
        uint32 team   = fields[2].GetUInt16();

        WorldSafeLocsEntry const* entry = GetWorldSafeLoc(safeLocId);
        if (!entry)
        {
            TC_LOG_ERROR("sql.sql", "Table `graveyard_zone` has a record for non-existing graveyard (WorldSafeLocsID: %u), skipped.", safeLocId);
            continue;
        }

        AreaTableEntry const* areaEntry = sAreaTableStore.LookupEntry(zoneId);
        if (!areaEntry)
        {
            TC_LOG_ERROR("sql.sql", "Table `graveyard_zone` has a record for non-existing Zone (ID: %u), skipped.", zoneId);
            continue;
        }

        if (team != 0 && team != HORDE && team != ALLIANCE)
        {
            TC_LOG_ERROR("sql.sql", "Table `graveyard_zone` has a record for non player faction (%u), skipped.", team);
            continue;
        }

        if (!AddGraveyardLink(safeLocId, zoneId, team, false))
            TC_LOG_ERROR("sql.sql", "Table `graveyard_zone` has a duplicate record for Graveyard (ID: %u) and Zone (ID: %u), skipped.", safeLocId, zoneId);
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u graveyard-zone links in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

WorldSafeLocsEntry const* ObjectMgr::GetDefaultGraveyard(uint32 team) const
{
    enum DefaultGraveyard
    {
        HORDE_GRAVEYARD    = 10, // Crossroads
        ALLIANCE_GRAVEYARD = 4   // Westfall
    };

    if (team == HORDE)
        return GetWorldSafeLoc(HORDE_GRAVEYARD);
    else if (team == ALLIANCE)
        return GetWorldSafeLoc(ALLIANCE_GRAVEYARD);
    else return nullptr;
}

WorldSafeLocsEntry const* ObjectMgr::GetClosestGraveyard(WorldLocation const& location, uint32 team, WorldObject* conditionObject) const
{
    float x, y, z;
    location.GetPosition(x, y, z);
    uint32 MapId = location.GetMapId();

    // search for zone associated closest graveyard
    uint32 zoneId = sMapMgr->GetZoneId(conditionObject ? conditionObject->GetPhaseShift() : PhasingHandler::GetEmptyPhaseShift(), MapId, x, y, z);

    if (!zoneId)
    {
        if (z > -500)
        {
            TC_LOG_ERROR("misc", "ZoneId not found for map %u coords (%f, %f, %f)", MapId, x, y, z);
            return GetDefaultGraveyard(team);
        }
    }

    // Simulate std. algorithm:
    //   found some graveyard associated to (ghost_zone, ghost_map)
    //
    //   if mapId == graveyard.mapId (ghost in plain zone or city or battleground) and search graveyard at same map
    //     then check faction
    //   if mapId != graveyard.mapId (ghost in instance) and search any graveyard associated
    //     then check faction
    GraveyardMapBounds range = GraveyardStore.equal_range(zoneId);
    MapEntry const* map = sMapStore.LookupEntry(MapId);

    // not need to check validity of map object; MapId _MUST_ be valid here
    if (range.first == range.second && !map->IsBattlegroundOrArena())
    {
        if (zoneId != 0) // zone == 0 can't be fixed, used by bliz for bugged zones
            TC_LOG_ERROR("sql.sql", "Table `game_graveyard_zone` incomplete: Zone %u Team %u does not have a linked graveyard.", zoneId, team);
        return GetDefaultGraveyard(team);
    }

    // at corpse map
    bool foundNear = false;
    float distNear = 10000;
    WorldSafeLocsEntry const* entryNear = nullptr;

    // at entrance map for corpse map
    bool foundEntr = false;
    float distEntr = 10000;
    WorldSafeLocsEntry const* entryEntr = nullptr;

    // some where other
    WorldSafeLocsEntry const* entryFar = nullptr;

    MapEntry const* mapEntry = sMapStore.LookupEntry(MapId);

    ConditionSourceInfo conditionSource(conditionObject);

    for (; range.first != range.second; ++range.first)
    {
        GraveyardData const& data = range.first->second;

        WorldSafeLocsEntry const* entry = ASSERT_NOTNULL(GetWorldSafeLoc(data.safeLocId));

        // skip enemy faction graveyard
        // team == 0 case can be at call from .neargrave
        if (data.team != 0 && team != 0 && data.team != team)
            continue;

        if (conditionObject)
        {
            if (!sConditionMgr->IsObjectMeetingNotGroupedConditions(CONDITION_SOURCE_TYPE_GRAVEYARD, data.safeLocId, conditionSource))
                continue;

            if (int16(entry->Loc.GetMapId()) == mapEntry->ParentMapID && !conditionObject->GetPhaseShift().HasVisibleMapId(entry->Loc.GetMapId()))
                continue;
        }

        // find now nearest graveyard at other map
        if (MapId != entry->Loc.GetMapId() && int16(entry->Loc.GetMapId()) != mapEntry->ParentMapID)
        {
            // if find graveyard at different map from where entrance placed (or no entrance data), use any first
            if (!mapEntry
                || mapEntry->CorpseMapID < 0
                || uint32(mapEntry->CorpseMapID) != entry->Loc.GetMapId()
                || (mapEntry->Corpse.X == 0 && mapEntry->Corpse.Y == 0)) // Check X and Y
            {
                // not have any corrdinates for check distance anyway
                entryFar = entry;
                continue;
            }

            // at entrance map calculate distance (2D);
            float dist2 = (entry->Loc.GetPositionX() - mapEntry->Corpse.X) * (entry->Loc.GetPositionX() - mapEntry->Corpse.X)
                + (entry->Loc.GetPositionY() - mapEntry->Corpse.Y) * (entry->Loc.GetPositionY() - mapEntry->Corpse.Y);
            if (foundEntr)
            {
                if (dist2 < distEntr)
                {
                    distEntr = dist2;
                    entryEntr = entry;
                }
            }
            else
            {
                foundEntr = true;
                distEntr = dist2;
                entryEntr = entry;
            }
        }
        // find now nearest graveyard at same map
        else
        {
            float dist2 = (entry->Loc.GetPositionX() - x) * (entry->Loc.GetPositionX() - x)
                + (entry->Loc.GetPositionY() - y) * (entry->Loc.GetPositionY() - y)
                + (entry->Loc.GetPositionZ() - z) * (entry->Loc.GetPositionZ() - z);
            if (foundNear)
            {
                if (dist2 < distNear)
                {
                    distNear = dist2;
                    entryNear = entry;
                }
            }
            else
            {
                foundNear = true;
                distNear = dist2;
                entryNear = entry;
            }
        }
    }

    if (entryNear)
        return entryNear;

    if (entryEntr)
        return entryEntr;

    return entryFar;
}

GraveyardData const* ObjectMgr::FindGraveyardData(uint32 id, uint32 zoneId) const
{
    GraveyardMapBounds range = GraveyardStore.equal_range(zoneId);
    for (; range.first != range.second; ++range.first)
    {
        GraveyardData const& data = range.first->second;
        if (data.safeLocId == id)
            return &data;
    }
    return nullptr;
}

void ObjectMgr::LoadWorldSafeLocs()
{
    uint32 oldMSTime = getMSTime();

    //                                                   0   1      2     3     4     5
    if (QueryResult result = WorldDatabase.Query("SELECT ID, MapID, LocX, LocY, LocZ, Facing FROM world_safe_locs"))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 id = fields[0].GetUInt32();
            WorldLocation loc(fields[1].GetUInt32(), fields[2].GetFloat(), fields[3].GetFloat(), fields[4].GetFloat(), fields[5].GetFloat());
            if (!MapManager::IsValidMapCoord(loc))
            {
                TC_LOG_ERROR("sql.sql", "World location (ID: %u) has a invalid position MapID: %u %s, skipped", id, loc.GetMapId(), loc.ToString().c_str());
                continue;
            }

            WorldSafeLocsEntry& worldSafeLocs = _worldSafeLocs[id];
            worldSafeLocs.ID = id;
            worldSafeLocs.Loc.WorldRelocate(loc);

        } while (result->NextRow());

        TC_LOG_INFO("server.loading", ">> Loaded " SZFMTD " world locations %u ms", _worldSafeLocs.size(), GetMSTimeDiffToNow(oldMSTime));
    }
    else
        TC_LOG_INFO("server.loading", ">> Loaded 0 world locations. DB table `world_safe_locs` is empty.");
}

WorldSafeLocsEntry const* ObjectMgr::GetWorldSafeLoc(uint32 id) const
{
    return Trinity::Containers::MapGetValuePtr(_worldSafeLocs, id);
}

Trinity::IteratorPair<std::unordered_map<uint32, WorldSafeLocsEntry>::const_iterator> ObjectMgr::GetWorldSafeLocs() const
{
    return std::make_pair(_worldSafeLocs.begin(), _worldSafeLocs.end());
}

AreaTriggerStruct const* ObjectMgr::GetAreaTrigger(uint32 trigger) const
{
    AreaTriggerContainer::const_iterator itr = _areaTriggerStore.find(trigger);
    if (itr != _areaTriggerStore.end())
        return &itr->second;
    return nullptr;
}

AccessRequirement const* ObjectMgr::GetAccessRequirement(uint32 mapid, Difficulty difficulty) const
{
    return Trinity::Containers::MapGetValuePtr(_accessRequirementStore, MAKE_PAIR64(mapid, difficulty));
}

bool ObjectMgr::AddGraveyardLink(uint32 id, uint32 zoneId, uint32 team, bool persist /*= true*/)
{
    if (FindGraveyardData(id, zoneId))
        return false;

    // add link to loaded data
    GraveyardData data;
    data.safeLocId = id;
    data.team = team;

    GraveyardStore.insert(GraveyardContainer::value_type(zoneId, data));

    // add link to DB
    if (persist)
    {
        WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_INS_GRAVEYARD_ZONE);

        stmt->setUInt32(0, id);
        stmt->setUInt32(1, zoneId);
        stmt->setUInt16(2, uint16(team));

        WorldDatabase.Execute(stmt);
    }

    return true;
}

void ObjectMgr::RemoveGraveyardLink(uint32 id, uint32 zoneId, uint32 team, bool persist /*= false*/)
{
    GraveyardMapBoundsNonConst range = GraveyardStore.equal_range(zoneId);
    if (range.first == range.second)
    {
        //TC_LOG_ERROR("sql.sql", "Table `graveyard_zone` incomplete: Zone %u Team %u does not have a linked graveyard.", zoneId, team);
        return;
    }

    bool found = false;


    for (; range.first != range.second; ++range.first)
    {
        GraveyardData & data = range.first->second;

        // skip not matching safezone id
        if (data.safeLocId != id)
            continue;

        // skip enemy faction graveyard at same map (normal area, city, or battleground)
        // team == 0 case can be at call from .neargrave
        if (data.team != 0 && team != 0 && data.team != team)
            continue;

        found = true;
        break;
    }

    // no match, return
    if (!found)
        return;

    // remove from links
    GraveyardStore.erase(range.first);

    // remove link from DB
    if (persist)
    {
        WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_DEL_GRAVEYARD_ZONE);

        stmt->setUInt32(0, id);
        stmt->setUInt32(1, zoneId);
        stmt->setUInt16(2, uint16(team));

        WorldDatabase.Execute(stmt);
    }
}

void ObjectMgr::LoadAreaTriggerTeleports()
{
    uint32 oldMSTime = getMSTime();

    _areaTriggerStore.clear(); // needed for reload case

    //                                               0   1
    QueryResult result = WorldDatabase.Query("SELECT ID, PortLocID FROM areatrigger_teleport");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 area trigger teleport definitions. DB table `areatrigger_teleport` is empty.");
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();

        ++count;

        uint32 Trigger_ID = fields[0].GetUInt32();
        uint32 PortLocID  = fields[1].GetUInt32();

        WorldSafeLocsEntry const* portLoc = GetWorldSafeLoc(PortLocID);
        if (!portLoc)
        {
            TC_LOG_ERROR("sql.sql", "Area Trigger (ID: %u) has a non-existing Port Loc (ID: %u) in WorldSafeLocs.dbc, skipped", Trigger_ID, PortLocID);
            continue;
        }

        AreaTriggerStruct at;

        at.target_mapId       = portLoc->Loc.GetMapId();
        at.target_X           = portLoc->Loc.GetPositionX();
        at.target_Y           = portLoc->Loc.GetPositionY();
        at.target_Z           = portLoc->Loc.GetPositionZ();
        at.target_Orientation = portLoc->Loc.GetOrientation();

        AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(Trigger_ID);
        if (!atEntry)
        {
            TC_LOG_ERROR("sql.sql", "Area Trigger (ID: %u) does not exist in AreaTrigger.dbc.", Trigger_ID);
            continue;
        }

        _areaTriggerStore[Trigger_ID] = at;

    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u area trigger teleport definitions in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadAccessRequirements()
{
    uint32 oldMSTime = getMSTime();

    _accessRequirementStore.clear();                                  // need for reload case

    //                                                 0       1           2          3        4      5       6               7                8                   9
    QueryResult result = WorldDatabase.Query("SELECT mapid, difficulty, level_min, level_max, item, item2, quest_done_A, quest_done_H, completed_achievement, quest_failed_text FROM access_requirement");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 access requirement definitions. DB table `access_requirement` is empty.");
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();

        uint32 mapid = fields[0].GetUInt32();
        if (!sMapStore.LookupEntry(mapid))
        {
            TC_LOG_ERROR("sql.sql", "Map %u referenced in `access_requirement` does not exist, skipped.", mapid);
            continue;
        }

        uint32 difficulty = fields[1].GetUInt8();
        if (!sDB2Manager.GetMapDifficultyData(mapid, Difficulty(difficulty)))
        {
            TC_LOG_ERROR("sql.sql", "Map %u referenced in `access_requirement` does not have difficulty %u, skipped", mapid, difficulty);
            continue;
        }

        uint64 requirement_ID = MAKE_PAIR64(mapid, difficulty);

        AccessRequirement* ar = &_accessRequirementStore[requirement_ID];
        ar->levelMin            = fields[2].GetUInt8();
        ar->levelMax            = fields[3].GetUInt8();
        ar->item                = fields[4].GetUInt32();
        ar->item2               = fields[5].GetUInt32();
        ar->quest_A             = fields[6].GetUInt32();
        ar->quest_H             = fields[7].GetUInt32();
        ar->achievement         = fields[8].GetUInt32();
        ar->questFailedText     = fields[9].GetString();

        if (ar->item)
        {
            ItemTemplate const* pProto = GetItemTemplate(ar->item);
            if (!pProto)
            {
                TC_LOG_ERROR("sql.sql", "Key item %u does not exist for map %u difficulty %u, removing key requirement.", ar->item, mapid, difficulty);
                ar->item = 0;
            }
        }

        if (ar->item2)
        {
            ItemTemplate const* pProto = GetItemTemplate(ar->item2);
            if (!pProto)
            {
                TC_LOG_ERROR("sql.sql", "Second item %u does not exist for map %u difficulty %u, removing key requirement.", ar->item2, mapid, difficulty);
                ar->item2 = 0;
            }
        }

        if (ar->quest_A)
        {
            if (!GetQuestTemplate(ar->quest_A))
            {
                TC_LOG_ERROR("sql.sql", "Required Alliance Quest %u not exist for map %u difficulty %u, remove quest done requirement.", ar->quest_A, mapid, difficulty);
                ar->quest_A = 0;
            }
        }

        if (ar->quest_H)
        {
            if (!GetQuestTemplate(ar->quest_H))
            {
                TC_LOG_ERROR("sql.sql", "Required Horde Quest %u not exist for map %u difficulty %u, remove quest done requirement.", ar->quest_H, mapid, difficulty);
                ar->quest_H = 0;
            }
        }

        if (ar->achievement)
        {
            if (!sAchievementStore.LookupEntry(ar->achievement))
            {
                TC_LOG_ERROR("sql.sql", "Required Achievement %u not exist for map %u difficulty %u, remove quest done requirement.", ar->achievement, mapid, difficulty);
                ar->achievement = 0;
            }
        }
        ++count;

    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u access requirement definitions in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

/*
 * Searches for the areatrigger which teleports players out of the given map with instance_template.parent field support
 */
AreaTriggerStruct const* ObjectMgr::GetGoBackTrigger(uint32 Map) const
{
    bool useParentDbValue = false;
    uint32 parentId = 0;
    MapEntry const* mapEntry = sMapStore.LookupEntry(Map);
    if (!mapEntry || mapEntry->CorpseMapID < 0)
        return nullptr;

    if (mapEntry->IsDungeon())
    {
        InstanceTemplate const* iTemplate = sObjectMgr->GetInstanceTemplate(Map);

        if (!iTemplate)
            return nullptr;

        parentId = iTemplate->Parent;
        useParentDbValue = true;
    }

    uint32 entrance_map = uint32(mapEntry->CorpseMapID);
    for (AreaTriggerContainer::const_iterator itr = _areaTriggerStore.begin(); itr != _areaTriggerStore.end(); ++itr)
        if ((!useParentDbValue && itr->second.target_mapId == entrance_map) || (useParentDbValue && itr->second.target_mapId == parentId))
        {
            AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(itr->first);
            if (atEntry && atEntry->ContinentID == int32(Map))
                return &itr->second;
        }
    return nullptr;
}

/**
 * Searches for the areatrigger which teleports players to the given map
 */
AreaTriggerStruct const* ObjectMgr::GetMapEntranceTrigger(uint32 Map) const
{
    for (AreaTriggerContainer::const_iterator itr = _areaTriggerStore.begin(); itr != _areaTriggerStore.end(); ++itr)
    {
        if (itr->second.target_mapId == Map)
        {
            AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(itr->first);
            if (atEntry)
                return &itr->second;
        }
    }
    return nullptr;
}

void ObjectMgr::SetHighestGuids()
{
    QueryResult result = CharacterDatabase.Query("SELECT MAX(guid) FROM characters");
    if (result)
        GetGuidSequenceGenerator<HighGuid::Player>().Set((*result)[0].GetUInt64() + 1);

    result = CharacterDatabase.Query("SELECT MAX(guid) FROM item_instance");
    if (result)
        GetGuidSequenceGenerator<HighGuid::Item>().Set((*result)[0].GetUInt64() + 1);

    // Cleanup other tables from nonexistent guids ( >= _hiItemGuid)
    CharacterDatabase.PExecute("DELETE FROM character_inventory WHERE item >= '%u'", GetGuidSequenceGenerator<HighGuid::Item>().GetNextAfterMaxUsed());    // One-time query
    CharacterDatabase.PExecute("DELETE FROM mail_items WHERE item_guid >= '%u'", GetGuidSequenceGenerator<HighGuid::Item>().GetNextAfterMaxUsed());        // One-time query
    CharacterDatabase.PExecute("DELETE a, ab, ai FROM auctionhouse a LEFT JOIN auction_bidders ab ON ab.auctionId = a.id LEFT JOIN auction_items ai ON ai.auctionId = a.id WHERE ai.itemGuid >= '%u'",
        GetGuidSequenceGenerator<HighGuid::Item>().GetNextAfterMaxUsed());       // One-time query
    CharacterDatabase.PExecute("DELETE FROM guild_bank_item WHERE item_guid >= '%u'", GetGuidSequenceGenerator<HighGuid::Item>().GetNextAfterMaxUsed());   // One-time query

    result = WorldDatabase.Query("SELECT MAX(guid) FROM transports");
    if (result)
        GetGuidSequenceGenerator<HighGuid::Transport>().Set((*result)[0].GetUInt64() + 1);

    result = CharacterDatabase.Query("SELECT MAX(id) FROM auctionhouse");
    if (result)
        _auctionId = (*result)[0].GetUInt32()+1;

    result = CharacterDatabase.Query("SELECT MAX(id) FROM mail");
    if (result)
        _mailId = (*result)[0].GetUInt32()+1;

    result = CharacterDatabase.Query("SELECT MAX(arenateamid) FROM arena_team");
    if (result)
        sArenaTeamMgr->SetNextArenaTeamId((*result)[0].GetUInt32()+1);

    result = CharacterDatabase.Query("SELECT MAX(maxguid) FROM ((SELECT MAX(setguid) AS maxguid FROM character_equipmentsets) UNION (SELECT MAX(setguid) AS maxguid FROM character_transmog_outfits)) allsets");
    if (result)
        _equipmentSetGuid = (*result)[0].GetUInt64()+1;

    result = CharacterDatabase.Query("SELECT MAX(guildId) FROM guild");
    if (result)
        sGuildMgr->SetNextGuildId((*result)[0].GetUInt64()+1);

    result = CharacterDatabase.Query("SELECT MAX(guid) FROM `groups`");
    if (result)
        sGroupMgr->SetGroupDbStoreSize((*result)[0].GetUInt32()+1);

    result = CharacterDatabase.Query("SELECT MAX(itemId) from character_void_storage");
    if (result)
        _voidItemId = (*result)[0].GetUInt64()+1;

    result = WorldDatabase.Query("SELECT MAX(guid) FROM creature");
    if (result)
        _creatureSpawnId = (*result)[0].GetUInt64() + 1;

    result = WorldDatabase.Query("SELECT MAX(guid) FROM gameobject");
    if (result)
        _gameObjectSpawnId = (*result)[0].GetUInt64() + 1;
}

uint32 ObjectMgr::GenerateAuctionID()
{
    if (_auctionId >= 0xFFFFFFFE)
    {
        TC_LOG_ERROR("misc", "Auctions ids overflow!! Can't continue, shutting down server. Search on forum for TCE00007 for more info. ");
        World::StopNow(ERROR_EXIT_CODE);
    }
    return _auctionId++;
}

uint64 ObjectMgr::GenerateEquipmentSetGuid()
{
    if (_equipmentSetGuid >= uint64(0xFFFFFFFFFFFFFFFELL))
    {
        TC_LOG_ERROR("misc", "EquipmentSet guid overflow!! Can't continue, shutting down server. Search on forum for TCE00007 for more info. ");
        World::StopNow(ERROR_EXIT_CODE);
    }
    return _equipmentSetGuid++;
}

uint32 ObjectMgr::GenerateMailID()
{
    if (_mailId >= 0xFFFFFFFE)
    {
        TC_LOG_ERROR("misc", "Mail ids overflow!! Can't continue, shutting down server. Search on forum for TCE00007 for more info. ");
        World::StopNow(ERROR_EXIT_CODE);
    }
    return _mailId++;
}

uint32 ObjectMgr::GeneratePetNumber()
{
    if (_hiPetNumber >= 0xFFFFFFFE)
    {
        TC_LOG_ERROR("misc", "_hiPetNumber Id overflow!! Can't continue, shutting down server. Search on forum for TCE00007 for more info.");
        World::StopNow(ERROR_EXIT_CODE);
    }
    return _hiPetNumber++;
}

uint64 ObjectMgr::GenerateVoidStorageItemId()
{
    if (_voidItemId >= uint64(0xFFFFFFFFFFFFFFFELL))
    {
        TC_LOG_ERROR("misc", "_voidItemId overflow!! Can't continue, shutting down server. ");
        World::StopNow(ERROR_EXIT_CODE);
    }
    return _voidItemId++;
}

uint64 ObjectMgr::GenerateCreatureSpawnId()
{
    if (_creatureSpawnId >= uint64(0xFFFFFFFFFFFFFFFELL))
    {
        TC_LOG_ERROR("misc", "Creature spawn id overflow!! Can't continue, shutting down server. Search on forum for TCE00007 for more info.");
        World::StopNow(ERROR_EXIT_CODE);
    }
    return _creatureSpawnId++;
}

uint64 ObjectMgr::GenerateGameObjectSpawnId()
{
    if (_gameObjectSpawnId >= uint64(0xFFFFFFFFFFFFFFFELL))
    {
        TC_LOG_ERROR("misc", "GameObject spawn id overflow!! Can't continue, shutting down server. Search on forum for TCE00007 for more info. ");
        World::StopNow(ERROR_EXIT_CODE);
    }
    return _gameObjectSpawnId++;
}

void ObjectMgr::LoadGameObjectLocales()
{
    uint32 oldMSTime = getMSTime();

    _gameObjectLocaleStore.clear(); // need for reload case

    //                                               0      1       2     3               4
    QueryResult result = WorldDatabase.Query("SELECT entry, locale, name, castBarCaption, unk1 FROM gameobject_template_locale");
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();

        uint32 id                   = fields[0].GetUInt32();
        std::string localeName      = fields[1].GetString();

        LocaleConstant locale = GetLocaleByName(localeName);
        if (!IsValidLocale(locale) || locale == LOCALE_enUS)
            continue;

        GameObjectLocale& data = _gameObjectLocaleStore[id];
        AddLocaleString(fields[2].GetString(), locale, data.Name);
        AddLocaleString(fields[3].GetString(), locale, data.CastBarCaption);
        AddLocaleString(fields[4].GetString(), locale, data.Unk1);

    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u gameobject_template_locale strings in %u ms", uint32(_gameObjectLocaleStore.size()), GetMSTimeDiffToNow(oldMSTime));
}

inline void CheckGOLockId(GameObjectTemplate const* goInfo, uint32 dataN, uint32 N)
{
    if (sLockStore.LookupEntry(dataN))
        return;

    TC_LOG_ERROR("sql.sql", "Gameobject (Entry: %u GoType: %u) have data%d=%u but lock (Id: %u) not found.",
        goInfo->entry, goInfo->type, N, goInfo->door.open, goInfo->door.open);
}

inline void CheckGOLinkedTrapId(GameObjectTemplate const* goInfo, uint32 dataN, uint32 N)
{
    if (GameObjectTemplate const* trapInfo = sObjectMgr->GetGameObjectTemplate(dataN))
    {
        if (trapInfo->type != GAMEOBJECT_TYPE_TRAP)
            TC_LOG_ERROR("sql.sql", "Gameobject (Entry: %u GoType: %u) have data%d=%u but GO (Entry %u) have not GAMEOBJECT_TYPE_TRAP (%u) type.",
            goInfo->entry, goInfo->type, N, dataN, dataN, GAMEOBJECT_TYPE_TRAP);
    }
}

inline void CheckGOSpellId(GameObjectTemplate const* goInfo, uint32 dataN, uint32 N)
{
    if (sSpellMgr->GetSpellInfo(dataN, DIFFICULTY_NONE))
        return;

    TC_LOG_ERROR("sql.sql", "Gameobject (Entry: %u GoType: %u) have data%d=%u but Spell (Entry %u) not exist.",
        goInfo->entry, goInfo->type, N, dataN, dataN);
}

inline void CheckAndFixGOChairHeightId(GameObjectTemplate const* goInfo, uint32& dataN, uint32 N)
{
    if (dataN <= (UNIT_STAND_STATE_SIT_HIGH_CHAIR-UNIT_STAND_STATE_SIT_LOW_CHAIR))
        return;

    TC_LOG_ERROR("sql.sql", "Gameobject (Entry: %u GoType: %u) have data%d=%u but correct chair height in range 0..%i.",
        goInfo->entry, goInfo->type, N, dataN, UNIT_STAND_STATE_SIT_HIGH_CHAIR-UNIT_STAND_STATE_SIT_LOW_CHAIR);

    // prevent client and server unexpected work
    dataN = 0;
}

inline void CheckGONoDamageImmuneId(GameObjectTemplate* goTemplate, uint32 dataN, uint32 N)
{
    // 0/1 correct values
    if (dataN <= 1)
        return;

    TC_LOG_ERROR("sql.sql", "Gameobject (Entry: %u GoType: %u) have data%d=%u but expected boolean (0/1) noDamageImmune field value.", goTemplate->entry, goTemplate->type, N, dataN);
}

inline void CheckGOConsumable(GameObjectTemplate const* goInfo, uint32 dataN, uint32 N)
{
    // 0/1 correct values
    if (dataN <= 1)
        return;

    TC_LOG_ERROR("sql.sql", "Gameobject (Entry: %u GoType: %u) have data%d=%u but expected boolean (0/1) consumable field value.",
        goInfo->entry, goInfo->type, N, dataN);
}

void ObjectMgr::LoadGameObjectTemplate()
{
    uint32 oldMSTime = getMSTime();

    for (GameObjectsEntry const* db2go : sGameObjectsStore)
    {
        GameObjectTemplate& go = _gameObjectTemplateStore[db2go->ID];
        go.entry = db2go->ID;
        go.type = db2go->TypeID;
        go.displayId = db2go->DisplayID;
        go.name = db2go->Name[sWorld->GetDefaultDbcLocale()];
        go.size = db2go->Scale;
        memset(go.raw.data, 0, sizeof(go.raw.data));
        memcpy(go.raw.data, db2go->PropValue, std::min(sizeof(db2go->PropValue), sizeof(go.raw.data)));
        go.ContentTuningId = 0;
        go.ScriptId = 0;
    }

    //                                               0      1     2          3     4         5               6     7
    QueryResult result = WorldDatabase.Query("SELECT entry, type, displayId, name, IconName, castBarCaption, unk1, size, "
    //                                        8      9      10     11     12     13     14     15     16     17     18      19      20
                                             "Data0, Data1, Data2, Data3, Data4, Data5, Data6, Data7, Data8, Data9, Data10, Data11, Data12, "
    //                                        21      22      23      24      25      26      27      28      29      30      31      32      33      34      35      36
                                             "Data13, Data14, Data15, Data16, Data17, Data18, Data19, Data20, Data21, Data22, Data23, Data24, Data25, Data26, Data27, Data28, "
    //                                        37      38       39     40      41      42               43      44
                                             "Data29, Data30, Data31, Data32, Data33, ContentTuningId, AIName, ScriptName "
                                             "FROM gameobject_template");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 gameobject definitions. DB table `gameobject_template` is empty.");
        return;
    }

    _gameObjectTemplateStore.reserve(result->GetRowCount());
    do
    {
        Field* fields = result->Fetch();

        uint32 entry = fields[0].GetUInt32();

        GameObjectTemplate& got = _gameObjectTemplateStore[entry];
        got.entry          = entry;
        got.type           = uint32(fields[1].GetUInt8());
        got.displayId      = fields[2].GetUInt32();
        got.name           = fields[3].GetString();
        got.IconName       = fields[4].GetString();
        got.castBarCaption = fields[5].GetString();
        got.unk1           = fields[6].GetString();
        got.size           = fields[7].GetFloat();

        for (uint8 i = 0; i < MAX_GAMEOBJECT_DATA; ++i)
            got.raw.data[i] = fields[8 + i].GetUInt32();

        got.ContentTuningId = fields[42].GetInt32();
        got.AIName = fields[43].GetString();
        got.ScriptId = GetScriptId(fields[44].GetString());

        // Checks
        if (!got.AIName.empty() && !sGameObjectAIRegistry->HasItem(got.AIName))
        {
            TC_LOG_ERROR("sql.sql", "GameObject (Entry: %u) has non-registered `AIName` '%s' set, removing", got.entry, got.AIName.c_str());
            got.AIName.clear();
        }

        switch (got.type)
        {
            case GAMEOBJECT_TYPE_DOOR:                      //0
            {
                if (got.door.open)
                    CheckGOLockId(&got, got.door.open, 1);
                CheckGONoDamageImmuneId(&got, got.door.noDamageImmune, 3);
                break;
            }
            case GAMEOBJECT_TYPE_BUTTON:                    //1
            {
                if (got.button.open)
                    CheckGOLockId(&got, got.button.open, 1);
                CheckGONoDamageImmuneId(&got, got.button.noDamageImmune, 4);
                break;
            }
            case GAMEOBJECT_TYPE_QUESTGIVER:                //2
            {
                if (got.questgiver.open)
                    CheckGOLockId(&got, got.questgiver.open, 0);
                CheckGONoDamageImmuneId(&got, got.questgiver.noDamageImmune, 5);
                break;
            }
            case GAMEOBJECT_TYPE_CHEST:                     //3
            {
                if (got.chest.open)
                    CheckGOLockId(&got, got.chest.open, 0);

                CheckGOConsumable(&got, got.chest.consumable, 3);

                if (got.chest.linkedTrap)               // linked trap
                    CheckGOLinkedTrapId(&got, got.chest.linkedTrap, 7);
                break;
            }
            case GAMEOBJECT_TYPE_TRAP:                      //6
            {
                if (got.trap.open)
                    CheckGOLockId(&got, got.trap.open, 0);
                break;
            }
            case GAMEOBJECT_TYPE_CHAIR:                     //7
                CheckAndFixGOChairHeightId(&got, got.chair.chairheight, 1);
                break;
            case GAMEOBJECT_TYPE_SPELL_FOCUS:               //8
            {
                if (got.spellFocus.spellFocusType)
                {
                    if (!sSpellFocusObjectStore.LookupEntry(got.spellFocus.spellFocusType))
                        TC_LOG_ERROR("sql.sql", "GameObject (Entry: %u GoType: %u) have data0=%u but SpellFocus (Id: %u) not exist.",
                        entry, got.type, got.spellFocus.spellFocusType, got.spellFocus.spellFocusType);
                }

                if (got.spellFocus.linkedTrap)        // linked trap
                    CheckGOLinkedTrapId(&got, got.spellFocus.linkedTrap, 2);
                break;
            }
            case GAMEOBJECT_TYPE_GOOBER:                    //10
            {
                if (got.goober.open)
                    CheckGOLockId(&got, got.goober.open, 0);

                CheckGOConsumable(&got, got.goober.consumable, 3);

                if (got.goober.pageID)                  // pageId
                {
                    if (!GetPageText(got.goober.pageID))
                        TC_LOG_ERROR("sql.sql", "GameObject (Entry: %u GoType: %u) have data7=%u but PageText (Entry %u) not exist.",
                        entry, got.type, got.goober.pageID, got.goober.pageID);
                }
                CheckGONoDamageImmuneId(&got, got.goober.noDamageImmune, 11);
                if (got.goober.linkedTrap)            // linked trap
                    CheckGOLinkedTrapId(&got, got.goober.linkedTrap, 12);
                break;
            }
            case GAMEOBJECT_TYPE_AREADAMAGE:                //12
            {
                if (got.areaDamage.open)
                    CheckGOLockId(&got, got.areaDamage.open, 0);
                break;
            }
            case GAMEOBJECT_TYPE_CAMERA:                    //13
            {
                if (got.camera.open)
                    CheckGOLockId(&got, got.camera.open, 0);
                break;
            }
            case GAMEOBJECT_TYPE_MAP_OBJ_TRANSPORT:              //15
            {
                if (got.moTransport.taxiPathID)
                {
                    if (got.moTransport.taxiPathID >= sTaxiPathNodesByPath.size() || sTaxiPathNodesByPath[got.moTransport.taxiPathID].empty())
                        TC_LOG_ERROR("sql.sql", "GameObject (Entry: %u GoType: %u) have data0=%u but TaxiPath (Id: %u) not exist.",
                            entry, got.type, got.moTransport.taxiPathID, got.moTransport.taxiPathID);
                }
                if (uint32 transportMap = got.moTransport.SpawnMap)
                    _transportMaps.insert(transportMap);
                break;
            }
            case GAMEOBJECT_TYPE_RITUAL:          //18
                break;
            case GAMEOBJECT_TYPE_SPELLCASTER:               //22
            {
                // always must have spell
                CheckGOSpellId(&got, got.spellCaster.spell, 0);
                break;
            }
            case GAMEOBJECT_TYPE_FLAGSTAND:                 //24
            {
                if (got.flagStand.open)
                    CheckGOLockId(&got, got.flagStand.open, 0);
                CheckGONoDamageImmuneId(&got, got.flagStand.noDamageImmune, 5);
                break;
            }
            case GAMEOBJECT_TYPE_FISHINGHOLE:               //25
            {
                if (got.fishingHole.open)
                    CheckGOLockId(&got, got.fishingHole.open, 4);
                break;
            }
            case GAMEOBJECT_TYPE_FLAGDROP:                  //26
            {
                if (got.flagDrop.open)
                    CheckGOLockId(&got, got.flagDrop.open, 0);
                CheckGONoDamageImmuneId(&got, got.flagDrop.noDamageImmune, 3);
                break;
            }
            case GAMEOBJECT_TYPE_BARBER_CHAIR:              //32
                CheckAndFixGOChairHeightId(&got, got.barberChair.chairheight, 0);

                if (got.barberChair.SitAnimKit && !sAnimKitStore.LookupEntry(got.barberChair.SitAnimKit))
                {
                    TC_LOG_ERROR("sql.sql", "GameObject (Entry: %u GoType: %u) have data2 = %u but AnimKit.dbc (Id: %u) not exist, set to 0.",
                       entry, got.type, got.barberChair.SitAnimKit, got.barberChair.SitAnimKit);
                    got.barberChair.SitAnimKit = 0;
                }
                break;
            case GAMEOBJECT_TYPE_GARRISON_BUILDING:
                if (uint32 transportMap = got.garrisonBuilding.SpawnMap)
                    _transportMaps.insert(transportMap);
                break;
            case GAMEOBJECT_TYPE_GATHERING_NODE:
                if (got.gatheringNode.open)
                    CheckGOLockId(&got, got.gatheringNode.open, 0);
                if (got.gatheringNode.linkedTrap)
                    CheckGOLinkedTrapId(&got, got.gatheringNode.linkedTrap, 20);
                break;
        }
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded " SZFMTD " game object templates in %u ms", _gameObjectTemplateStore.size(), GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadGameObjectTemplateAddons()
{
    uint32 oldMSTime = getMSTime();

    //                                               0      1        2      3        4        5              6
    QueryResult result = WorldDatabase.Query("SELECT entry, faction, flags, mingold, maxgold, WorldEffectID, AIAnimKitID FROM gameobject_template_addon");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 gameobject template addon definitions. DB table `gameobject_template_addon` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 entry = fields[0].GetUInt32();

        GameObjectTemplate const* got = sObjectMgr->GetGameObjectTemplate(entry);
        if (!got)
        {
            TC_LOG_ERROR("sql.sql", "GameObject template (Entry: %u) does not exist but has a record in `gameobject_template_addon`", entry);
            continue;
        }

        GameObjectTemplateAddon& gameObjectAddon = _gameObjectTemplateAddonStore[entry];
        gameObjectAddon.Faction       = uint32(fields[1].GetUInt16());
        gameObjectAddon.Flags         = fields[2].GetUInt32();
        gameObjectAddon.Mingold       = fields[3].GetUInt32();
        gameObjectAddon.Maxgold       = fields[4].GetUInt32();
        gameObjectAddon.WorldEffectID = fields[5].GetUInt32();
        gameObjectAddon.AIAnimKitID   = fields[6].GetUInt32();

        // checks
        if (gameObjectAddon.Faction && !sFactionTemplateStore.LookupEntry(gameObjectAddon.Faction))
            TC_LOG_ERROR("sql.sql", "GameObject (Entry: %u) has invalid faction (%u) defined in `gameobject_template_addon`.", entry, gameObjectAddon.Faction);

        if (gameObjectAddon.Maxgold > 0)
        {
            switch (got->type)
            {
                case GAMEOBJECT_TYPE_CHEST:
                case GAMEOBJECT_TYPE_FISHINGHOLE:
                    break;
                default:
                    TC_LOG_ERROR("sql.sql", "GameObject (Entry %u GoType: %u) cannot be looted but has maxgold set in `gameobject_template_addon`.", entry, got->type);
                    break;
            }
        }

        if (gameObjectAddon.WorldEffectID && !sWorldEffectStore.LookupEntry(gameObjectAddon.WorldEffectID))
        {
            TC_LOG_ERROR("sql.sql", "GameObject (Entry: %u) has invalid WorldEffectID (%u) defined in `gameobject_template_addon`, set to 0.", entry, gameObjectAddon.WorldEffectID);
            gameObjectAddon.WorldEffectID = 0;
        }

        if (gameObjectAddon.AIAnimKitID && !sAnimKitStore.LookupEntry(gameObjectAddon.AIAnimKitID))
        {
            TC_LOG_ERROR("sql.sql", "GameObject (Entry: %u) has invalid AIAnimKitID (%u) defined in `gameobject_template_addon`, set to 0.", entry, gameObjectAddon.AIAnimKitID);
            gameObjectAddon.AIAnimKitID = 0;
        }

        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u game object template addons in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadGameObjectOverrides()
{
    uint32 oldMSTime = getMSTime();

    //                                                     0        1      2
    QueryResult result = WorldDatabase.Query("SELECT spawnId, faction, flags FROM gameobject_overrides");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 gameobject faction and flags overrides. DB table `gameobject_overrides` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        ObjectGuid::LowType spawnId = fields[0].GetUInt64();
        GameObjectData const* goData = GetGameObjectData(spawnId);
        if (!goData)
        {
            TC_LOG_ERROR("sql.sql", "GameObject (SpawnId: " UI64FMTD ") does not exist but has a record in `gameobject_overrides`", spawnId);
            continue;
        }

        GameObjectOverride& gameObjectOverride = _gameObjectOverrideStore[spawnId];
        gameObjectOverride.Faction = fields[1].GetUInt16();
        gameObjectOverride.Flags = fields[2].GetUInt32();

        if (gameObjectOverride.Faction && !sFactionTemplateStore.LookupEntry(gameObjectOverride.Faction))
            TC_LOG_ERROR("sql.sql", "GameObject (SpawnId: " UI64FMTD ") has invalid faction (%u) defined in `gameobject_overrides`.", spawnId, gameObjectOverride.Faction);

        ++count;
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u gameobject faction and flags overrides in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadExplorationBaseXP()
{
    uint32 oldMSTime = getMSTime();

    QueryResult result = WorldDatabase.Query("SELECT level, basexp FROM exploration_basexp");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 BaseXP definitions. DB table `exploration_basexp` is empty.");
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();
        uint8 level  = fields[0].GetUInt8();
        uint32 basexp = fields[1].GetInt32();
        _baseXPTable[level] = basexp;
        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u BaseXP definitions in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

uint32 ObjectMgr::GetBaseXP(uint8 level)
{
    return _baseXPTable[level] ? _baseXPTable[level] : 0;
}

uint32 ObjectMgr::GetXPForLevel(uint8 level) const
{
    if (level < _playerXPperLevel.size())
        return _playerXPperLevel[level];
    return 0;
}

void ObjectMgr::LoadPetNames()
{
    uint32 oldMSTime = getMSTime();
    //                                                0     1      2
    QueryResult result = WorldDatabase.Query("SELECT word, entry, half FROM pet_name_generation");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 pet name parts. DB table `pet_name_generation` is empty!");
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();
        std::string word = fields[0].GetString();
        uint32 entry     = fields[1].GetUInt32();
        bool   half      = fields[2].GetBool();
        if (half)
            _petHalfName1[entry].push_back(word);
        else
            _petHalfName0[entry].push_back(word);
        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u pet name parts in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadPetNumber()
{
    uint32 oldMSTime = getMSTime();

    QueryResult result = CharacterDatabase.Query("SELECT MAX(id) FROM character_pet");
    if (result)
    {
        Field* fields = result->Fetch();
        _hiPetNumber = fields[0].GetUInt32()+1;
    }

    TC_LOG_INFO("server.loading", ">> Loaded the max pet number: %d in %u ms", _hiPetNumber-1, GetMSTimeDiffToNow(oldMSTime));
}

std::string ObjectMgr::GeneratePetName(uint32 entry)
{
    std::vector<std::string>& list0 = _petHalfName0[entry];
    std::vector<std::string>& list1 = _petHalfName1[entry];

    if (list0.empty() || list1.empty())
    {
        CreatureTemplate const* cinfo = GetCreatureTemplate(entry);
        if (!cinfo)
            return std::string();

        char const* petname = DB2Manager::GetCreatureFamilyPetName(cinfo->family, sWorld->GetDefaultDbcLocale());
        if (petname)
            return std::string(petname);
        else
            return cinfo->Name;
    }

    return *(list0.begin()+urand(0, list0.size()-1)) + *(list1.begin()+urand(0, list1.size()-1));
}

void ObjectMgr::LoadReputationRewardRate()
{
    uint32 oldMSTime = getMSTime();

    _repRewardRateStore.clear();                             // for reload case

    uint32 count = 0; //                                0          1             2                  3                  4                 5                      6             7
    QueryResult result = WorldDatabase.Query("SELECT faction, quest_rate, quest_daily_rate, quest_weekly_rate, quest_monthly_rate, quest_repeatable_rate, creature_rate, spell_rate FROM reputation_reward_rate");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded `reputation_reward_rate`, table is empty!");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        uint32 factionId            = fields[0].GetUInt32();

        RepRewardRate repRate;

        repRate.questRate           = fields[1].GetFloat();
        repRate.questDailyRate      = fields[2].GetFloat();
        repRate.questWeeklyRate     = fields[3].GetFloat();
        repRate.questMonthlyRate    = fields[4].GetFloat();
        repRate.questRepeatableRate = fields[5].GetFloat();
        repRate.creatureRate        = fields[6].GetFloat();
        repRate.spellRate           = fields[7].GetFloat();

        FactionEntry const* factionEntry = sFactionStore.LookupEntry(factionId);
        if (!factionEntry)
        {
            TC_LOG_ERROR("sql.sql", "Faction (faction.dbc) %u does not exist but is used in `reputation_reward_rate`", factionId);
            continue;
        }

        if (repRate.questRate < 0.0f)
        {
            TC_LOG_ERROR("sql.sql", "Table reputation_reward_rate has quest_rate with invalid rate %f, skipping data for faction %u", repRate.questRate, factionId);
            continue;
        }

        if (repRate.questDailyRate < 0.0f)
        {
            TC_LOG_ERROR("sql.sql", "Table reputation_reward_rate has quest_daily_rate with invalid rate %f, skipping data for faction %u", repRate.questDailyRate, factionId);
            continue;
        }

        if (repRate.questWeeklyRate < 0.0f)
        {
            TC_LOG_ERROR("sql.sql", "Table reputation_reward_rate has quest_weekly_rate with invalid rate %f, skipping data for faction %u", repRate.questWeeklyRate, factionId);
            continue;
        }

        if (repRate.questMonthlyRate < 0.0f)
        {
            TC_LOG_ERROR("sql.sql", "Table reputation_reward_rate has quest_monthly_rate with invalid rate %f, skipping data for faction %u", repRate.questMonthlyRate, factionId);
            continue;
        }

        if (repRate.questRepeatableRate < 0.0f)
        {
            TC_LOG_ERROR("sql.sql", "Table reputation_reward_rate has quest_repeatable_rate with invalid rate %f, skipping data for faction %u", repRate.questRepeatableRate, factionId);
            continue;
        }

        if (repRate.creatureRate < 0.0f)
        {
            TC_LOG_ERROR("sql.sql", "Table reputation_reward_rate has creature_rate with invalid rate %f, skipping data for faction %u", repRate.creatureRate, factionId);
            continue;
        }

        if (repRate.spellRate < 0.0f)
        {
            TC_LOG_ERROR("sql.sql", "Table reputation_reward_rate has spell_rate with invalid rate %f, skipping data for faction %u", repRate.spellRate, factionId);
            continue;
        }

        _repRewardRateStore[factionId] = repRate;

        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u reputation_reward_rate in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadReputationOnKill()
{
    uint32 oldMSTime = getMSTime();

    // For reload case
    _repOnKillStore.clear();

    uint32 count = 0;

    //                                                0            1                     2
    QueryResult result = WorldDatabase.Query("SELECT creature_id, RewOnKillRepFaction1, RewOnKillRepFaction2, "
    //   3             4             5                   6             7             8                   9
        "IsTeamAward1, MaxStanding1, RewOnKillRepValue1, IsTeamAward2, MaxStanding2, RewOnKillRepValue2, TeamDependent "
        "FROM creature_onkill_reputation");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 creature award reputation definitions. DB table `creature_onkill_reputation` is empty.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        uint32 creature_id = fields[0].GetUInt32();

        ReputationOnKillEntry repOnKill;
        repOnKill.RepFaction1          = fields[1].GetInt16();
        repOnKill.RepFaction2          = fields[2].GetInt16();
        repOnKill.IsTeamAward1        = fields[3].GetBool();
        repOnKill.ReputationMaxCap1  = fields[4].GetUInt8();
        repOnKill.RepValue1            = fields[5].GetInt32();
        repOnKill.IsTeamAward2        = fields[6].GetBool();
        repOnKill.ReputationMaxCap2  = fields[7].GetUInt8();
        repOnKill.RepValue2            = fields[8].GetInt32();
        repOnKill.TeamDependent       = fields[9].GetBool();

        if (!GetCreatureTemplate(creature_id))
        {
            TC_LOG_ERROR("sql.sql", "Table `creature_onkill_reputation` has data for nonexistent creature entry (%u), skipped", creature_id);
            continue;
        }

        if (repOnKill.RepFaction1)
        {
            FactionEntry const* factionEntry1 = sFactionStore.LookupEntry(repOnKill.RepFaction1);
            if (!factionEntry1)
            {
                TC_LOG_ERROR("sql.sql", "Faction (faction.dbc) %u does not exist but is used in `creature_onkill_reputation`", repOnKill.RepFaction1);
                continue;
            }
        }

        if (repOnKill.RepFaction2)
        {
            FactionEntry const* factionEntry2 = sFactionStore.LookupEntry(repOnKill.RepFaction2);
            if (!factionEntry2)
            {
                TC_LOG_ERROR("sql.sql", "Faction (faction.dbc) %u does not exist but is used in `creature_onkill_reputation`", repOnKill.RepFaction2);
                continue;
            }
        }

        _repOnKillStore[creature_id] = repOnKill;

        ++count;
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u creature award reputation definitions in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadReputationSpilloverTemplate()
{
    uint32 oldMSTime = getMSTime();

    _repSpilloverTemplateStore.clear();                      // for reload case

    uint32 count = 0; //                                0         1        2       3        4       5       6         7        8      9        10       11     12        13       14     15
    QueryResult result = WorldDatabase.Query("SELECT faction, faction1, rate_1, rank_1, faction2, rate_2, rank_2, faction3, rate_3, rank_3, faction4, rate_4, rank_4, faction5, rate_5, rank_5 FROM reputation_spillover_template");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded `reputation_spillover_template`, table is empty.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        uint32 factionId                = fields[0].GetUInt16();

        RepSpilloverTemplate repTemplate;

        repTemplate.faction[0]          = fields[1].GetUInt16();
        repTemplate.faction_rate[0]     = fields[2].GetFloat();
        repTemplate.faction_rank[0]     = fields[3].GetUInt8();
        repTemplate.faction[1]          = fields[4].GetUInt16();
        repTemplate.faction_rate[1]     = fields[5].GetFloat();
        repTemplate.faction_rank[1]     = fields[6].GetUInt8();
        repTemplate.faction[2]          = fields[7].GetUInt16();
        repTemplate.faction_rate[2]     = fields[8].GetFloat();
        repTemplate.faction_rank[2]     = fields[9].GetUInt8();
        repTemplate.faction[3]          = fields[10].GetUInt16();
        repTemplate.faction_rate[3]     = fields[11].GetFloat();
        repTemplate.faction_rank[3]     = fields[12].GetUInt8();
        repTemplate.faction[4]          = fields[13].GetUInt16();
        repTemplate.faction_rate[4]     = fields[14].GetFloat();
        repTemplate.faction_rank[4]     = fields[15].GetUInt8();

        FactionEntry const* factionEntry = sFactionStore.LookupEntry(factionId);

        if (!factionEntry)
        {
            TC_LOG_ERROR("sql.sql", "Faction (faction.dbc) %u does not exist but is used in `reputation_spillover_template`", factionId);
            continue;
        }

        if (factionEntry->ParentFactionID == 0)
        {
            TC_LOG_ERROR("sql.sql", "Faction (faction.dbc) %u in `reputation_spillover_template` does not belong to any team, skipping", factionId);
            continue;
        }

        bool invalidSpilloverFaction = false;
        for (uint32 i = 0; i < MAX_SPILLOVER_FACTIONS; ++i)
        {
            if (repTemplate.faction[i])
            {
                FactionEntry const* factionSpillover = sFactionStore.LookupEntry(repTemplate.faction[i]);

                if (!factionSpillover)
                {
                    TC_LOG_ERROR("sql.sql", "Spillover faction (faction.dbc) %u does not exist but is used in `reputation_spillover_template` for faction %u, skipping", repTemplate.faction[i], factionId);
                    invalidSpilloverFaction = true;
                    break;
                }

                if (!factionSpillover->CanHaveReputation())
                {
                    TC_LOG_ERROR("sql.sql", "Spillover faction (faction.dbc) %u for faction %u in `reputation_spillover_template` can not be listed for client, and then useless, skipping", repTemplate.faction[i], factionId);
                    invalidSpilloverFaction = true;
                    break;
                }

                if (repTemplate.faction_rank[i] >= MAX_REPUTATION_RANK)
                {
                    TC_LOG_ERROR("sql.sql", "Rank %u used in `reputation_spillover_template` for spillover faction %u is not valid, skipping", repTemplate.faction_rank[i], repTemplate.faction[i]);
                    invalidSpilloverFaction = true;
                    break;
                }
            }
        }

        if (invalidSpilloverFaction)
            continue;

        _repSpilloverTemplateStore[factionId] = repTemplate;

        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u reputation_spillover_template in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadPointsOfInterest()
{
    uint32 oldMSTime = getMSTime();

    _pointsOfInterestStore.clear(); // need for reload case

    uint32 count = 0;

    //                                               0   1          2          3          4     5      6           7     8
    QueryResult result = WorldDatabase.Query("SELECT ID, PositionX, PositionY, PositionZ, Icon, Flags, Importance, Name, Unknown905 FROM points_of_interest");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 Points of Interest definitions. DB table `points_of_interest` is empty.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        uint32 id = fields[0].GetUInt32();

        PointOfInterest pointOfInterest;
        pointOfInterest.ID              = id;
        pointOfInterest.Pos.Relocate(fields[1].GetFloat(), fields[2].GetFloat(), fields[3].GetFloat());
        pointOfInterest.Icon            = fields[4].GetUInt32();
        pointOfInterest.Flags           = fields[5].GetUInt32();
        pointOfInterest.Importance      = fields[6].GetUInt32();
        pointOfInterest.Name            = fields[7].GetString();
        pointOfInterest.Unknown905      = fields[8].GetInt32();

        if (!Trinity::IsValidMapCoord(pointOfInterest.Pos.GetPositionX(), pointOfInterest.Pos.GetPositionY(), pointOfInterest.Pos.GetPositionZ()))
        {
            TC_LOG_ERROR("sql.sql", "Table `points_of_interest` (ID: %u) have invalid coordinates (PositionX: %f PositionY: %f, PositionZ: %f), ignored.",
                id, pointOfInterest.Pos.GetPositionX(), pointOfInterest.Pos.GetPositionY(), pointOfInterest.Pos.GetPositionZ());
            continue;
        }

        _pointsOfInterestStore[id] = pointOfInterest;

        ++count;
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u Points of Interest definitions in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadQuestPOI()
{
    uint32 oldMSTime = getMSTime();

    _questPOIStore.clear();                              // need for reload case

    //                                               0        1          2     3               4                 5              6      7        8         9      10             11                 12                           13               14
    QueryResult result = WorldDatabase.Query("SELECT QuestID, BlobIndex, Idx1, ObjectiveIndex, QuestObjectiveID, QuestObjectID, MapID, UiMapID, Priority, Flags, WorldEffectID, PlayerConditionID, NavigationPlayerConditionID, SpawnTrackingID, AlwaysAllowMergingBlobs FROM quest_poi order by QuestID, Idx1");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 quest POI definitions. DB table `quest_poi` is empty.");
        return;
    }

    //                                                0        1    2  3  4
    QueryResult pointsResult = WorldDatabase.Query("SELECT QuestID, Idx1, X, Y, Z FROM quest_poi_points ORDER BY QuestID DESC, Idx1, Idx2");

    std::unordered_map<int32, std::map<int32, std::vector<QuestPOIBlobPoint>>> allPoints;

    if (pointsResult)
    {
        do
        {
            Field* fields = pointsResult->Fetch();

            int32 QuestID             = fields[0].GetInt32();
            int32 Idx1                = fields[1].GetInt32();
            int32 x                   = fields[2].GetInt32();
            int32 y                   = fields[3].GetInt32();
            int32 z                   = fields[4].GetInt32();

            allPoints[QuestID][Idx1].emplace_back(x, y, z);
        } while (pointsResult->NextRow());
    }

    do
    {
        Field* fields = result->Fetch();

        int32 questID               = fields[0].GetInt32();
        int32 blobIndex             = fields[1].GetInt32();
        int32 idx1                  = fields[2].GetInt32();
        int32 objectiveIndex        = fields[3].GetInt32();
        int32 questObjectiveID      = fields[4].GetInt32();
        int32 questObjectID         = fields[5].GetInt32();
        int32 mapID                 = fields[6].GetInt32();
        int32 uiMapID               = fields[7].GetInt32();
        int32 priority              = fields[8].GetInt32();
        int32 flags                 = fields[9].GetInt32();
        int32 worldEffectID         = fields[10].GetInt32();
        int32 playerConditionID     = fields[11].GetInt32();
        int32 navigationPlayerConditionID = fields[12].GetInt32();
        int32 spawnTrackingID       = fields[13].GetInt32();
        bool alwaysAllowMergingBlobs = fields[14].GetBool();

        if (!sObjectMgr->GetQuestTemplate(questID))
            TC_LOG_ERROR("sql.sql", "`quest_poi` quest id (%u) Idx1 (%u) does not exist in `quest_template`", questID, idx1);

        if (std::map<int32, std::vector<QuestPOIBlobPoint>>* blobs = Trinity::Containers::MapGetValuePtr(allPoints, questID))
        {
            if (std::vector<QuestPOIBlobPoint>* points = Trinity::Containers::MapGetValuePtr(*blobs, idx1))
            {
                QuestPOIData& poiData = _questPOIStore[questID];
                poiData.QuestID = questID;
                poiData.Blobs.emplace_back(blobIndex, objectiveIndex, questObjectiveID, questObjectID, mapID, uiMapID, priority, flags,
                    worldEffectID, playerConditionID, navigationPlayerConditionID, spawnTrackingID, std::move(*points), alwaysAllowMergingBlobs);
                continue;
            }
        }

        TC_LOG_ERROR("sql.sql", "Table quest_poi references unknown quest points for quest %i POI id %i", questID, blobIndex);

    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded " SZFMTD " quest POI definitions in %u ms", _questPOIStore.size(), GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadNPCSpellClickSpells()
{
    uint32 oldMSTime = getMSTime();

    _spellClickInfoStore.clear();
    //                                                0          1         2            3
    QueryResult result = WorldDatabase.Query("SELECT npc_entry, spell_id, cast_flags, user_type FROM npc_spellclick_spells");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 spellclick spells. DB table `npc_spellclick_spells` is empty.");
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();

        uint32 npc_entry = fields[0].GetUInt32();
        CreatureTemplate const* cInfo = GetCreatureTemplate(npc_entry);
        if (!cInfo)
        {
            TC_LOG_ERROR("sql.sql", "Table npc_spellclick_spells references unknown creature_template %u. Skipping entry.", npc_entry);
            continue;
        }

        uint32 spellid = fields[1].GetUInt32();
        SpellInfo const* spellinfo = sSpellMgr->GetSpellInfo(spellid, DIFFICULTY_NONE);
        if (!spellinfo)
        {
            TC_LOG_ERROR("sql.sql", "Table npc_spellclick_spells creature: %u references unknown spellid %u. Skipping entry.", npc_entry, spellid);
            continue;
        }

        uint8 userType = fields[3].GetUInt16();
        if (userType >= SPELL_CLICK_USER_MAX)
            TC_LOG_ERROR("sql.sql", "Table npc_spellclick_spells creature: %u  references unknown user type %u. Skipping entry.", npc_entry, uint32(userType));

        uint8 castFlags = fields[2].GetUInt8();
        SpellClickInfo info;
        info.spellId = spellid;
        info.castFlags = castFlags;
        info.userType = SpellClickUserTypes(userType);
        _spellClickInfoStore.insert(SpellClickInfoContainer::value_type(npc_entry, info));

        ++count;
    }
    while (result->NextRow());

    // all spellclick data loaded, now we check if there are creatures with NPC_FLAG_SPELLCLICK but with no data
    // NOTE: It *CAN* be the other way around: no spellclick flag but with spellclick data, in case of creature-only vehicle accessories
    for (auto& creatureTemplatePair : _creatureTemplateStore)
    {
        if ((creatureTemplatePair.second.npcflag & UNIT_NPC_FLAG_SPELLCLICK) && !_spellClickInfoStore.count(creatureTemplatePair.first))
        {
            TC_LOG_ERROR("sql.sql", "npc_spellclick_spells: Creature template %u has UNIT_NPC_FLAG_SPELLCLICK but no data in spellclick table! Removing flag", creatureTemplatePair.first);
            creatureTemplatePair.second.npcflag &= ~UNIT_NPC_FLAG_SPELLCLICK;
        }
    }

    TC_LOG_INFO("server.loading", ">> Loaded %u spellclick definitions in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::DeleteCreatureData(ObjectGuid::LowType guid)
{
    // remove mapid*cellid -> guid_set map
    CreatureData const* data = GetCreatureData(guid);
    if (data)
    {
        RemoveCreatureFromGrid(guid, data);
        OnDeleteSpawnData(data);
    }

    _creatureDataStore.erase(guid);
}

void ObjectMgr::DeleteGameObjectData(ObjectGuid::LowType guid)
{
    // remove mapid*cellid -> guid_set map
    GameObjectData const* data = GetGameObjectData(guid);
    if (data)
    {
        RemoveGameobjectFromGrid(guid, data);
        OnDeleteSpawnData(data);
    }

    _gameObjectDataStore.erase(guid);
}

void ObjectMgr::LoadQuestRelationsHelper(QuestRelations& map, QuestRelationsReverse* reverseMap, std::string const& table, bool starter, bool go)
{
    uint32 oldMSTime = getMSTime();

    map.clear();                                            // need for reload case

    uint32 count = 0;

    QueryResult result = WorldDatabase.PQuery("SELECT id, quest, pool_entry FROM %s qr LEFT JOIN pool_quest pq ON qr.quest = pq.entry", table.c_str());

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 quest relations from `%s`, table is empty.", table.c_str());
        return;
    }

    PooledQuestRelation* poolRelationMap = go ? &sPoolMgr->mQuestGORelation : &sPoolMgr->mQuestCreatureRelation;
    if (starter)
        poolRelationMap->clear();

    do
    {
        uint32 id     = result->Fetch()[0].GetUInt32();
        uint32 quest  = result->Fetch()[1].GetUInt32();
        uint32 poolId = result->Fetch()[2].GetUInt32();

        if (!_questTemplates.count(quest))
        {
            TC_LOG_ERROR("sql.sql", "Table `%s`: Quest %u listed for entry %u does not exist.", table.c_str(), quest, id);
            continue;
        }

        if (!poolId || !starter)
        {
            map.insert(QuestRelations::value_type(id, quest));
            if (reverseMap)
                reverseMap->insert(QuestRelationsReverse::value_type(quest, id));
        }
        else
            poolRelationMap->insert(PooledQuestRelation::value_type(quest, id));

        ++count;
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u quest relations from %s in %u ms", count, table.c_str(), GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadGameobjectQuestStarters()
{
    LoadQuestRelationsHelper(_goQuestRelations, nullptr, "gameobject_queststarter", true, true);

    for (QuestRelations::iterator itr = _goQuestRelations.begin(); itr != _goQuestRelations.end(); ++itr)
    {
        GameObjectTemplate const* goInfo = GetGameObjectTemplate(itr->first);
        if (!goInfo)
            TC_LOG_ERROR("sql.sql", "Table `gameobject_queststarter` has data for nonexistent gameobject entry (%u) and existed quest %u", itr->first, itr->second);
        else if (goInfo->type != GAMEOBJECT_TYPE_QUESTGIVER)
            TC_LOG_ERROR("sql.sql", "Table `gameobject_queststarter` has data gameobject entry (%u) for quest %u, but GO is not GAMEOBJECT_TYPE_QUESTGIVER", itr->first, itr->second);
    }
}

void ObjectMgr::LoadGameobjectQuestEnders()
{
    LoadQuestRelationsHelper(_goQuestInvolvedRelations, &_goQuestInvolvedRelationsReverse, "gameobject_questender", false, true);

    for (QuestRelations::iterator itr = _goQuestInvolvedRelations.begin(); itr != _goQuestInvolvedRelations.end(); ++itr)
    {
        GameObjectTemplate const* goInfo = GetGameObjectTemplate(itr->first);
        if (!goInfo)
            TC_LOG_ERROR("sql.sql", "Table `gameobject_questender` has data for nonexistent gameobject entry (%u) and existed quest %u", itr->first, itr->second);
        else if (goInfo->type != GAMEOBJECT_TYPE_QUESTGIVER)
            TC_LOG_ERROR("sql.sql", "Table `gameobject_questender` has data gameobject entry (%u) for quest %u, but GO is not GAMEOBJECT_TYPE_QUESTGIVER", itr->first, itr->second);
    }
}

void ObjectMgr::LoadCreatureQuestStarters()
{
    LoadQuestRelationsHelper(_creatureQuestRelations, nullptr, "creature_queststarter", true, false);

    for (QuestRelations::iterator itr = _creatureQuestRelations.begin(); itr != _creatureQuestRelations.end(); ++itr)
    {
        CreatureTemplate const* cInfo = GetCreatureTemplate(itr->first);
        if (!cInfo)
            TC_LOG_ERROR("sql.sql", "Table `creature_queststarter` has data for nonexistent creature entry (%u) and existed quest %u", itr->first, itr->second);
        else if (!(cInfo->npcflag & UNIT_NPC_FLAG_QUESTGIVER))
            TC_LOG_ERROR("sql.sql", "Table `creature_queststarter` has creature entry (%u) for quest %u, but npcflag does not include UNIT_NPC_FLAG_QUESTGIVER", itr->first, itr->second);
    }
}

void ObjectMgr::LoadCreatureQuestEnders()
{
    LoadQuestRelationsHelper(_creatureQuestInvolvedRelations, &_creatureQuestInvolvedRelationsReverse, "creature_questender", false, false);

    for (QuestRelations::iterator itr = _creatureQuestInvolvedRelations.begin(); itr != _creatureQuestInvolvedRelations.end(); ++itr)
    {
        CreatureTemplate const* cInfo = GetCreatureTemplate(itr->first);
        if (!cInfo)
            TC_LOG_ERROR("sql.sql", "Table `creature_questender` has data for nonexistent creature entry (%u) and existed quest %u", itr->first, itr->second);
        else if (!(cInfo->npcflag & UNIT_NPC_FLAG_QUESTGIVER))
            TC_LOG_ERROR("sql.sql", "Table `creature_questender` has creature entry (%u) for quest %u, but npcflag does not include UNIT_NPC_FLAG_QUESTGIVER", itr->first, itr->second);
    }
}

void ObjectMgr::LoadReservedPlayersNames()
{
    uint32 oldMSTime = getMSTime();

    _reservedNamesStore.clear();                                // need for reload case

    QueryResult result = CharacterDatabase.Query("SELECT name FROM reserved_name");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 reserved player names. DB table `reserved_name` is empty!");
        return;
    }

    uint32 count = 0;

    Field* fields;
    do
    {
        fields = result->Fetch();
        std::string name= fields[0].GetString();

        std::wstring wstr;
        if (!Utf8toWStr (name, wstr))
        {
            TC_LOG_ERROR("misc", "Table `reserved_name` has invalid name: %s", name.c_str());
            continue;
        }

        wstrToLower(wstr);

        _reservedNamesStore.insert(wstr);
        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u reserved player names in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

bool ObjectMgr::IsReservedName(const std::string& name) const
{
    std::wstring wstr;
    if (!Utf8toWStr (name, wstr))
        return false;

    wstrToLower(wstr);

    return _reservedNamesStore.find(wstr) != _reservedNamesStore.end();
}

enum LanguageType
{
    LT_BASIC_LATIN    = 0x0000,
    LT_EXTENDEN_LATIN = 0x0001,
    LT_CYRILLIC       = 0x0002,
    LT_EAST_ASIA      = 0x0004,
    LT_ANY            = 0xFFFF
};

static LanguageType GetRealmLanguageType(bool create)
{
    switch (sWorld->getIntConfig(CONFIG_REALM_ZONE))
    {
        case REALM_ZONE_UNKNOWN:                            // any language
        case REALM_ZONE_DEVELOPMENT:
        case REALM_ZONE_TEST_SERVER:
        case REALM_ZONE_QA_SERVER:
            return LT_ANY;
        case REALM_ZONE_UNITED_STATES:                      // extended-Latin
        case REALM_ZONE_OCEANIC:
        case REALM_ZONE_LATIN_AMERICA:
        case REALM_ZONE_ENGLISH:
        case REALM_ZONE_GERMAN:
        case REALM_ZONE_FRENCH:
        case REALM_ZONE_SPANISH:
            return LT_EXTENDEN_LATIN;
        case REALM_ZONE_KOREA:                              // East-Asian
        case REALM_ZONE_TAIWAN:
        case REALM_ZONE_CHINA:
            return LT_EAST_ASIA;
        case REALM_ZONE_RUSSIAN:                            // Cyrillic
            return LT_CYRILLIC;
        default:
            return create ? LT_BASIC_LATIN : LT_ANY;        // basic-Latin at create, any at login
    }
}

bool isValidString(const std::wstring& wstr, uint32 strictMask, bool numericOrSpace, bool create = false)
{
    if (strictMask == 0)                                       // any language, ignore realm
    {
        if (isExtendedLatinString(wstr, numericOrSpace))
            return true;
        if (isCyrillicString(wstr, numericOrSpace))
            return true;
        if (isEastAsianString(wstr, numericOrSpace))
            return true;
        return false;
    }

    if (strictMask & 0x2)                                    // realm zone specific
    {
        LanguageType lt = GetRealmLanguageType(create);
        if (lt & LT_EXTENDEN_LATIN)
            if (isExtendedLatinString(wstr, numericOrSpace))
                return true;
        if (lt & LT_CYRILLIC)
            if (isCyrillicString(wstr, numericOrSpace))
                return true;
        if (lt & LT_EAST_ASIA)
            if (isEastAsianString(wstr, numericOrSpace))
                return true;
    }

    if (strictMask & 0x1)                                    // basic Latin
    {
        if (isBasicLatinString(wstr, numericOrSpace))
            return true;
    }

    return false;
}

ResponseCodes ObjectMgr::CheckPlayerName(std::string const& name, LocaleConstant locale, bool create /*= false*/)
{
    std::wstring wname;
    if (!Utf8toWStr(name, wname))
        return CHAR_NAME_INVALID_CHARACTER;

    if (wname.size() > MAX_PLAYER_NAME)
        return CHAR_NAME_TOO_LONG;

    uint32 minName = sWorld->getIntConfig(CONFIG_MIN_PLAYER_NAME);
    if (wname.size() < minName)
        return CHAR_NAME_TOO_SHORT;

    uint32 strictMask = sWorld->getIntConfig(CONFIG_STRICT_PLAYER_NAMES);
    if (!isValidString(wname, strictMask, false, create))
        return CHAR_NAME_MIXED_LANGUAGES;

    wstrToLower(wname);
    for (size_t i = 2; i < wname.size(); ++i)
        if (wname[i] == wname[i-1] && wname[i] == wname[i-2])
            return CHAR_NAME_THREE_CONSECUTIVE;

    return sDB2Manager.ValidateName(wname, locale);
}

bool ObjectMgr::IsValidCharterName(const std::string& name)
{
    std::wstring wname;
    if (!Utf8toWStr(name, wname))
        return false;

    if (wname.size() > MAX_CHARTER_NAME)
        return false;

    uint32 minName = sWorld->getIntConfig(CONFIG_MIN_CHARTER_NAME);
    if (wname.size() < minName)
        return false;

    uint32 strictMask = sWorld->getIntConfig(CONFIG_STRICT_CHARTER_NAMES);

    return isValidString(wname, strictMask, true);
}

PetNameInvalidReason ObjectMgr::CheckPetName(const std::string& name)
{
    std::wstring wname;
    if (!Utf8toWStr(name, wname))
        return PET_NAME_INVALID;

    if (wname.size() > MAX_PET_NAME)
        return PET_NAME_TOO_LONG;

    uint32 minName = sWorld->getIntConfig(CONFIG_MIN_PET_NAME);
    if (wname.size() < minName)
        return PET_NAME_TOO_SHORT;

    uint32 strictMask = sWorld->getIntConfig(CONFIG_STRICT_PET_NAMES);
    if (!isValidString(wname, strictMask, false))
        return PET_NAME_MIXED_LANGUAGES;

    return PET_NAME_SUCCESS;
}

void ObjectMgr::LoadGameObjectForQuests()
{
    uint32 oldMSTime = getMSTime();

    _gameObjectForQuestStore.clear();                         // need for reload case

    if (_gameObjectTemplateStore.empty())
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 GameObjects for quests");
        return;
    }

    uint32 count = 0;

    // collect GO entries for GO that must activated
    for (auto const& gameObjectTemplatePair : _gameObjectTemplateStore)
    {
        switch (gameObjectTemplatePair.second.type)
        {
            case GAMEOBJECT_TYPE_QUESTGIVER:
                break;
            case GAMEOBJECT_TYPE_CHEST:
            {
                // scan GO chest with loot including quest items
                uint32 lootId = gameObjectTemplatePair.second.GetLootId();
                // find quest loot for GO
                if (gameObjectTemplatePair.second.chest.questID || LootTemplates_Gameobject.HaveQuestLootFor(lootId))
                    break;
                continue;
            }
            case GAMEOBJECT_TYPE_GENERIC:
            {
                if (gameObjectTemplatePair.second.generic.questID > 0)             //quests objects
                    break;
                continue;
            }
            case GAMEOBJECT_TYPE_GOOBER:
            {
                if (gameObjectTemplatePair.second.goober.questID > 0)              //quests objects
                    break;
                continue;
            }
            default:
                continue;
        }

        _gameObjectForQuestStore.insert(gameObjectTemplatePair.first);
        ++count;
    }

    TC_LOG_INFO("server.loading", ">> Loaded %u GameObjects for quests in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

bool ObjectMgr::LoadTrinityStrings()
{
    uint32 oldMSTime = getMSTime();

    _trinityStringStore.clear(); // for reload case

    QueryResult result = WorldDatabase.Query("SELECT entry, content_default, content_loc1, content_loc2, content_loc3, content_loc4, content_loc5, content_loc6, content_loc7, content_loc8 FROM trinity_string");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 trinity strings. DB table `trinity_string` is empty. You have imported an incorrect database for more info search for TCE00003 on forum.");
        return false;
    }

    do
    {
        Field* fields = result->Fetch();

        uint32 entry = fields[0].GetUInt32();

        TrinityString& data = _trinityStringStore[entry];

        data.Content.resize(DEFAULT_LOCALE + 1);

        for (int8 i = OLD_TOTAL_LOCALES - 1; i >= 0; --i)
            AddLocaleString(fields[i + 1].GetString(), LocaleConstant(i), data.Content);
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded " SZFMTD " trinity strings in %u ms", _trinityStringStore.size(), GetMSTimeDiffToNow(oldMSTime));
    return true;
}

char const* ObjectMgr::GetTrinityString(uint32 entry, LocaleConstant locale) const
{
    if (TrinityString const* ts = GetTrinityString(entry))
    {
        if (ts->Content.size() > size_t(locale) && !ts->Content[locale].empty())
            return ts->Content[locale].c_str();
        return ts->Content[DEFAULT_LOCALE].c_str();
    }

    TC_LOG_ERROR("sql.sql", "Trinity string entry %u not found in DB.", entry);
    return "<error>";
}

void ObjectMgr::LoadFishingBaseSkillLevel()
{
    uint32 oldMSTime = getMSTime();

    _fishingBaseForAreaStore.clear();                            // for reload case

    QueryResult result = WorldDatabase.Query("SELECT entry, skill FROM skill_fishing_base_level");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 areas for fishing base skill level. DB table `skill_fishing_base_level` is empty.");
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();
        uint32 entry  = fields[0].GetUInt32();
        int32 skill   = fields[1].GetInt16();

        AreaTableEntry const* fArea = sAreaTableStore.LookupEntry(entry);
        if (!fArea)
        {
            TC_LOG_ERROR("sql.sql", "AreaId %u defined in `skill_fishing_base_level` does not exist", entry);
            continue;
        }

        _fishingBaseForAreaStore[entry] = skill;
        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u areas for fishing base skill level in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadSkillTiers()
{
    uint32 oldMSTime = getMSTime();

    QueryResult result = WorldDatabase.Query("SELECT ID, Value1, Value2, Value3, Value4, Value5, Value6, Value7, Value8, Value9, Value10, "
        " Value11, Value12, Value13, Value14, Value15, Value16 FROM skill_tiers");

    if (!result)
    {
        TC_LOG_ERROR("server.loading", ">> Loaded 0 skill max values. DB table `skill_tiers` is empty.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();
        uint32 id = fields[0].GetUInt32();
        SkillTiersEntry& tier = _skillTiers[id];
        for (uint32 i = 0; i < MAX_SKILL_STEP; ++i)
            tier.Value[i] = fields[1 + i].GetUInt32();

    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u skill max values in %u ms", uint32(_skillTiers.size()), GetMSTimeDiffToNow(oldMSTime));
}

bool ObjectMgr::CheckDeclinedNames(const std::wstring& w_ownname, DeclinedName const& names)
{
    // get main part of the name
    std::wstring mainpart = GetMainPartOfName(w_ownname, 0);
    // prepare flags
    bool x = true;
    bool y = true;

    // check declined names
    for (uint8 i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
    {
        std::wstring wname;
        if (!Utf8toWStr(names.name[i], wname))
            return false;

        if (mainpart != GetMainPartOfName(wname, i+1))
            x = false;

        if (w_ownname != wname)
            y = false;
    }
    return (x || y);
}

uint32 ObjectMgr::GetAreaTriggerScriptId(uint32 trigger_id) const
{
    AreaTriggerScriptContainer::const_iterator i = _areaTriggerScriptStore.find(trigger_id);
    if (i!= _areaTriggerScriptStore.end())
        return i->second;
    return 0;
}

SpellScriptsBounds ObjectMgr::GetSpellScriptsBounds(uint32 spellId)
{
    return SpellScriptsBounds(_spellScriptsStore.equal_range(spellId));
}

// this allows calculating base reputations to offline players, just by race and class
int32 ObjectMgr::GetBaseReputationOf(FactionEntry const* factionEntry, uint8 race, uint8 playerClass) const
{
    if (!factionEntry)
        return 0;

    uint32 classMask = 1 << (playerClass - 1);

    for (uint8 i = 0; i < 4; ++i)
    {
        if ((!factionEntry->ReputationClassMask[i] ||
            factionEntry->ReputationClassMask[i] & classMask) &&
            (!factionEntry->ReputationRaceMask[i] ||
            factionEntry->ReputationRaceMask[i].HasRace(race)))
            return factionEntry->ReputationBase[i];
    }

    return 0;
}

SkillRangeType GetSkillRangeType(SkillRaceClassInfoEntry const* rcEntry)
{
    SkillLineEntry const* skill = sSkillLineStore.LookupEntry(rcEntry->SkillID);
    if (!skill)
        return SKILL_RANGE_NONE;

    if (sObjectMgr->GetSkillTier(rcEntry->SkillTierID))
        return SKILL_RANGE_RANK;

    if (rcEntry->SkillID == SKILL_RUNEFORGING)
        return SKILL_RANGE_MONO;

    switch (skill->CategoryID)
    {
        case SKILL_CATEGORY_ARMOR:
            return SKILL_RANGE_MONO;
        case SKILL_CATEGORY_LANGUAGES:
            return SKILL_RANGE_LANGUAGE;
    }

    return SKILL_RANGE_LEVEL;
}

void ObjectMgr::LoadGameTele()
{
    uint32 oldMSTime = getMSTime();

    _gameTeleStore.clear();                                  // for reload case

    //                                                0       1           2           3           4        5     6
    QueryResult result = WorldDatabase.Query("SELECT id, position_x, position_y, position_z, orientation, map, name FROM game_tele");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 GameTeleports. DB table `game_tele` is empty!");
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();

        uint32 id         = fields[0].GetUInt32();

        GameTele gt;

        gt.position_x     = fields[1].GetFloat();
        gt.position_y     = fields[2].GetFloat();
        gt.position_z     = fields[3].GetFloat();
        gt.orientation    = fields[4].GetFloat();
        gt.mapId          = fields[5].GetUInt16();
        gt.name           = fields[6].GetString();

        if (!MapManager::IsValidMapCoord(gt.mapId, gt.position_x, gt.position_y, gt.position_z, gt.orientation))
        {
            TC_LOG_ERROR("sql.sql", "Wrong position for id %u (name: %s) in `game_tele` table, ignoring.", id, gt.name.c_str());
            continue;
        }

        if (!Utf8toWStr(gt.name, gt.wnameLow))
        {
            TC_LOG_ERROR("sql.sql", "Wrong UTF8 name for id %u in `game_tele` table, ignoring.", id);
            continue;
        }

        wstrToLower(gt.wnameLow);

        _gameTeleStore[id] = gt;

        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u GameTeleports in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

GameTele const* ObjectMgr::GetGameTele(const std::string& name) const
{
    // explicit name case
    std::wstring wname;
    if (!Utf8toWStr(name, wname))
        return nullptr;

    // converting string that we try to find to lower case
    wstrToLower(wname);

    // Alternative first GameTele what contains wnameLow as substring in case no GameTele location found
    GameTele const* alt = nullptr;
    for (GameTeleContainer::const_iterator itr = _gameTeleStore.begin(); itr != _gameTeleStore.end(); ++itr)
    {
        if (itr->second.wnameLow == wname)
            return &itr->second;
        else if (!alt && itr->second.wnameLow.find(wname) != std::wstring::npos)
            alt = &itr->second;
    }

    return alt;
}

GameTele const* ObjectMgr::GetGameTeleExactName(const std::string& name) const
{
    // explicit name case
    std::wstring wname;
    if (!Utf8toWStr(name, wname))
        return nullptr;

    // converting string that we try to find to lower case
    wstrToLower(wname);

    for (GameTeleContainer::const_iterator itr = _gameTeleStore.begin(); itr != _gameTeleStore.end(); ++itr)
    {
        if (itr->second.wnameLow == wname)
            return &itr->second;
    }

    return nullptr;
}

bool ObjectMgr::AddGameTele(GameTele& tele)
{
    // find max id
    uint32 new_id = 0;
    for (GameTeleContainer::const_iterator itr = _gameTeleStore.begin(); itr != _gameTeleStore.end(); ++itr)
        if (itr->first > new_id)
            new_id = itr->first;

    // use next
    ++new_id;

    if (!Utf8toWStr(tele.name, tele.wnameLow))
        return false;

    wstrToLower(tele.wnameLow);

    _gameTeleStore[new_id] = tele;

    WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_INS_GAME_TELE);

    stmt->setUInt32(0, new_id);
    stmt->setFloat(1, tele.position_x);
    stmt->setFloat(2, tele.position_y);
    stmt->setFloat(3, tele.position_z);
    stmt->setFloat(4, tele.orientation);
    stmt->setUInt16(5, uint16(tele.mapId));
    stmt->setString(6, tele.name);

    WorldDatabase.Execute(stmt);

    return true;
}

bool ObjectMgr::DeleteGameTele(const std::string& name)
{
    // explicit name case
    std::wstring wname;
    if (!Utf8toWStr(name, wname))
        return false;

    // converting string that we try to find to lower case
    wstrToLower(wname);

    for (GameTeleContainer::iterator itr = _gameTeleStore.begin(); itr != _gameTeleStore.end(); ++itr)
    {
        if (itr->second.wnameLow == wname)
        {
            WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_DEL_GAME_TELE);

            stmt->setString(0, itr->second.name);

            WorldDatabase.Execute(stmt);

            _gameTeleStore.erase(itr);
            return true;
        }
    }

    return false;
}

void ObjectMgr::LoadMailLevelRewards()
{
    uint32 oldMSTime = getMSTime();

    _mailLevelRewardStore.clear();                           // for reload case

    //                                                 0        1             2            3
    QueryResult result = WorldDatabase.Query("SELECT level, raceMask, mailTemplateId, senderEntry FROM mail_level_reward");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 level dependent mail rewards. DB table `mail_level_reward` is empty.");
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();

        uint8 level           = fields[0].GetUInt8();
        uint64 raceMask       = fields[1].GetUInt64();
        uint32 mailTemplateId = fields[2].GetUInt32();
        uint32 senderEntry    = fields[3].GetUInt32();

        if (level > MAX_LEVEL)
        {
            TC_LOG_ERROR("sql.sql", "Table `mail_level_reward` has data for level %u that more supported by client (%u), ignoring.", level, MAX_LEVEL);
            continue;
        }

        if (!(raceMask & RACEMASK_ALL_PLAYABLE))
        {
            TC_LOG_ERROR("sql.sql", "Table `mail_level_reward` has raceMask (" UI64FMTD ") for level %u that not include any player races, ignoring.", raceMask, level);
            continue;
        }

        if (!sMailTemplateStore.LookupEntry(mailTemplateId))
        {
            TC_LOG_ERROR("sql.sql", "Table `mail_level_reward` has invalid mailTemplateId (%u) for level %u that invalid not include any player races, ignoring.", mailTemplateId, level);
            continue;
        }

        if (!GetCreatureTemplate(senderEntry))
        {
            TC_LOG_ERROR("sql.sql", "Table `mail_level_reward` has nonexistent sender creature entry (%u) for level %u that invalid not include any player races, ignoring.", senderEntry, level);
            continue;
        }

        _mailLevelRewardStore[level].emplace_back(raceMask, mailTemplateId, senderEntry);

        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u level dependent mail rewards in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadTrainers()
{
    uint32 oldMSTime = getMSTime();

    // For reload case
    _trainers.clear();

    std::unordered_map<int32, std::vector<Trainer::Spell>> spellsByTrainer;
    if (QueryResult trainerSpellsResult = WorldDatabase.Query("SELECT TrainerId, SpellId, MoneyCost, ReqSkillLine, ReqSkillRank, ReqAbility1, ReqAbility2, ReqAbility3, ReqLevel FROM trainer_spell"))
    {
        do
        {
            Field* fields = trainerSpellsResult->Fetch();

            Trainer::Spell spell;
            uint32 trainerId = fields[0].GetUInt32();
            spell.SpellId = fields[1].GetUInt32();
            spell.MoneyCost = fields[2].GetUInt32();
            spell.ReqSkillLine = fields[3].GetUInt32();
            spell.ReqSkillRank = fields[4].GetUInt32();
            spell.ReqAbility[0] = fields[5].GetUInt32();
            spell.ReqAbility[1] = fields[6].GetUInt32();
            spell.ReqAbility[2] = fields[7].GetUInt32();
            spell.ReqLevel = fields[8].GetUInt8();

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spell.SpellId, DIFFICULTY_NONE);
            if (!spellInfo)
            {
                TC_LOG_ERROR("sql.sql", "Table `trainer_spell` references non-existing spell (SpellId: %u) for TrainerId %u, ignoring", spell.SpellId, trainerId);
                continue;
            }

            if (spell.ReqSkillLine && !sSkillLineStore.LookupEntry(spell.ReqSkillLine))
            {
                TC_LOG_ERROR("sql.sql", "Table `trainer_spell` references non-existing skill (ReqSkillLine: %u) for TrainerId %u and SpellId %u, ignoring",
                    spell.ReqSkillLine, trainerId, spell.SpellId);
                continue;
            }

            bool allReqValid = true;
            for (std::size_t i = 0; i < spell.ReqAbility.size(); ++i)
            {
                uint32 requiredSpell = spell.ReqAbility[i];
                if (requiredSpell && !sSpellMgr->GetSpellInfo(requiredSpell, DIFFICULTY_NONE))
                {
                    TC_LOG_ERROR("sql.sql", "Table `trainer_spell` references non-existing spell (ReqAbility" SZFMTD ": %u) for TrainerId %u and SpellId %u, ignoring",
                        i + 1, requiredSpell, trainerId, spell.SpellId);
                    allReqValid = false;
                }
            }

            if (!allReqValid)
                continue;

            spellsByTrainer[trainerId].push_back(spell);

        } while (trainerSpellsResult->NextRow());
    }

    if (QueryResult trainersResult = WorldDatabase.Query("SELECT Id, Type, Greeting FROM trainer"))
    {
        do
        {
            Field* fields = trainersResult->Fetch();
            uint32 trainerId = fields[0].GetUInt32();
            Trainer::Type trainerType = Trainer::Type(fields[1].GetUInt8());
            std::string greeting = fields[2].GetString();
            std::vector<Trainer::Spell> spells;
            auto spellsItr = spellsByTrainer.find(trainerId);
            if (spellsItr != spellsByTrainer.end())
            {
                spells = std::move(spellsItr->second);
                spellsByTrainer.erase(spellsItr);
            }

            _trainers.emplace(std::piecewise_construct, std::forward_as_tuple(trainerId), std::forward_as_tuple(trainerId, trainerType, std::move(greeting), std::move(spells)));

        } while (trainersResult->NextRow());
    }

    for (auto const& unusedSpells : spellsByTrainer)
    {
        for (Trainer::Spell const& unusedSpell : unusedSpells.second)
        {
            TC_LOG_ERROR("sql.sql", "Table `trainer_spell` references non-existing trainer (TrainerId: %u) for SpellId %u, ignoring", unusedSpells.first, unusedSpell.SpellId);
        }
    }

    if (QueryResult trainerLocalesResult = WorldDatabase.Query("SELECT Id, locale, Greeting_lang FROM trainer_locale"))
    {
        do
        {
            Field* fields = trainerLocalesResult->Fetch();
            uint32 trainerId = fields[0].GetUInt32();
            std::string localeName = fields[1].GetString();

            LocaleConstant locale = GetLocaleByName(localeName);
            if (!IsValidLocale(locale) || locale == LOCALE_enUS)
                continue;

            if (Trainer::Trainer* trainer = Trinity::Containers::MapGetValuePtr(_trainers, trainerId))
                trainer->AddGreetingLocale(locale, fields[2].GetString());
            else
                TC_LOG_ERROR("sql.sql", "Table `trainer_locale` references non-existing trainer (TrainerId: %u) for locale %s, ignoring",
                    trainerId, localeName.c_str());

        } while (trainerLocalesResult->NextRow());
    }

    TC_LOG_INFO("server.loading", ">> Loaded " SZFMTD " Trainers in %u ms", _trainers.size(), GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadCreatureTrainers()
{
    uint32 oldMSTime = getMSTime();

    _creatureDefaultTrainers.clear();

    if (QueryResult result = WorldDatabase.Query("SELECT CreatureId, TrainerId, MenuId, OptionIndex FROM creature_trainer"))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 creatureId = fields[0].GetUInt32();
            uint32 trainerId = fields[1].GetUInt32();
            uint32 gossipMenuId = fields[2].GetUInt32();
            uint32 gossipOptionIndex = fields[3].GetUInt32();

            if (!GetCreatureTemplate(creatureId))
            {
                TC_LOG_ERROR("sql.sql", "Table `creature_trainer` references non-existing creature template (CreatureId: %u), ignoring", creatureId);
                continue;
            }

            if (!GetTrainer(trainerId))
            {
                TC_LOG_ERROR("sql.sql", "Table `creature_trainer` references non-existing trainer (TrainerId: %u) for CreatureId %u MenuId %u OptionIndex %u, ignoring",
                    trainerId, creatureId, gossipMenuId, gossipOptionIndex);
                continue;
            }

            if (gossipMenuId || gossipOptionIndex)
            {
                Trinity::IteratorPair<GossipMenuItemsContainer::const_iterator> gossipMenuItems = GetGossipMenuItemsMapBounds(gossipMenuId);
                auto gossipOptionItr = std::find_if(gossipMenuItems.begin(), gossipMenuItems.end(), [gossipOptionIndex](std::pair<uint32 const, GossipMenuItems> const& entry)
                {
                    return entry.second.OptionIndex == gossipOptionIndex;
                });
                if (gossipOptionItr == gossipMenuItems.end())
                {
                    TC_LOG_ERROR("sql.sql", "Table `creature_trainer` references non-existing gossip menu option (MenuId %u OptionIndex %u) for CreatureId %u and TrainerId %u, ignoring",
                        gossipMenuId, gossipOptionIndex, creatureId, trainerId);
                    continue;
                }
            }

            _creatureDefaultTrainers[std::make_tuple(creatureId, gossipMenuId, gossipOptionIndex)] = trainerId;
        } while (result->NextRow());
    }

    TC_LOG_INFO("server.loading", ">> Loaded " SZFMTD " default trainers in %u ms", _creatureDefaultTrainers.size(), GetMSTimeDiffToNow(oldMSTime));
}

uint32 ObjectMgr::LoadReferenceVendor(int32 vendor, int32 item, std::set<uint32>* skip_vendors)
{
    // find all items from the reference vendor
    WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_NPC_VENDOR_REF);
    stmt->setUInt32(0, uint32(item));
    PreparedQueryResult result = WorldDatabase.Query(stmt);

    if (!result)
        return 0;

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        int32 item_id = fields[0].GetInt32();

        // if item is a negative, its a reference
        if (item_id < 0)
            count += LoadReferenceVendor(vendor, -item_id, skip_vendors);
        else
        {
            VendorItem vItem;
            vItem.item              = item_id;
            vItem.maxcount          = fields[1].GetUInt32();
            vItem.incrtime          = fields[2].GetUInt32();
            vItem.ExtendedCost      = fields[3].GetUInt32();
            vItem.Type              = fields[4].GetUInt8();
            vItem.PlayerConditionId = fields[6].GetUInt32();
            vItem.IgnoreFiltering   = fields[7].GetBool();

            Tokenizer bonusListIDsTok(fields[5].GetString(), ' ');
            for (char const* token : bonusListIDsTok)
                vItem.BonusListIDs.push_back(int32(atol(token)));

            if (!IsVendorItemValid(vendor, vItem, nullptr, skip_vendors))
                continue;

            VendorItemData& vList = _cacheVendorItemStore[vendor];
            vList.AddItem(std::move(vItem));
            ++count;
        }
    } while (result->NextRow());

    return count;
}

void ObjectMgr::LoadVendors()
{
    uint32 oldMSTime = getMSTime();

    // For reload case
    for (CacheVendorItemContainer::iterator itr = _cacheVendorItemStore.begin(); itr != _cacheVendorItemStore.end(); ++itr)
        itr->second.Clear();
    _cacheVendorItemStore.clear();

    std::set<uint32> skip_vendors;

    QueryResult result = WorldDatabase.Query("SELECT entry, item, maxcount, incrtime, ExtendedCost, type, BonusListIDs, PlayerConditionID, IgnoreFiltering FROM npc_vendor ORDER BY entry, slot ASC");
    if (!result)
    {
        TC_LOG_ERROR("server.loading", ">>  Loaded 0 Vendors. DB table `npc_vendor` is empty!");
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();

        uint32 entry        = fields[0].GetUInt32();
        int32 item_id      = fields[1].GetInt32();

        // if item is a negative, its a reference
        if (item_id < 0)
            count += LoadReferenceVendor(entry, -item_id, &skip_vendors);
        else
        {
            VendorItem vItem;
            vItem.item              = item_id;
            vItem.maxcount          = fields[2].GetUInt32();
            vItem.incrtime          = fields[3].GetUInt32();
            vItem.ExtendedCost      = fields[4].GetUInt32();
            vItem.Type              = fields[5].GetUInt8();
            vItem.PlayerConditionId = fields[7].GetUInt32();
            vItem.IgnoreFiltering   = fields[8].GetBool();

            Tokenizer bonusListIDsTok(fields[6].GetString(), ' ');
            for (char const* token : bonusListIDsTok)
                vItem.BonusListIDs.push_back(int32(atol(token)));

            if (!IsVendorItemValid(entry, vItem, nullptr, &skip_vendors))
                continue;

            VendorItemData& vList = _cacheVendorItemStore[entry];
            vList.AddItem(std::move(vItem));
            ++count;
        }
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %d Vendors in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadGossipMenu()
{
    uint32 oldMSTime = getMSTime();

    _gossipMenusStore.clear();

    //                                               0       1
    QueryResult result = WorldDatabase.Query("SELECT MenuId, TextId FROM gossip_menu");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 gossip_menu IDs. DB table `gossip_menu` is empty!");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        GossipMenus gMenu;

        gMenu.MenuId = fields[0].GetUInt32();
        gMenu.TextId = fields[1].GetUInt32();

        if (!GetNpcText(gMenu.TextId))
        {
            TC_LOG_ERROR("sql.sql", "Table gossip_menu: ID %u is using non-existing TextId %u", gMenu.MenuId, gMenu.TextId);
            continue;
        }

        _gossipMenusStore.insert(GossipMenusContainer::value_type(gMenu.MenuId, gMenu));
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u gossip_menu IDs in %u ms", uint32(_gossipMenusStore.size()), GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadGossipMenuItems()
{
    uint32 oldMSTime = getMSTime();

    _gossipMenuItemsStore.clear();

    QueryResult result = WorldDatabase.Query(
    //          0         1              2             3             4                        5             6
        "SELECT o.MenuId, o.OptionIndex, o.OptionIcon, o.OptionText, o.OptionBroadcastTextId, o.OptionType, o.OptionNpcFlag, "
    //   7                8
        "oa.ActionMenuId, oa.ActionPoiId, "
    //   9            10           11          12
        "ob.BoxCoded, ob.BoxMoney, ob.BoxText, ob.BoxBroadcastTextId "
        "FROM gossip_menu_option o "
        "LEFT JOIN gossip_menu_option_action oa ON o.MenuId = oa.MenuId AND o.OptionIndex = oa.OptionIndex "
        "LEFT JOIN gossip_menu_option_box ob ON o.MenuId = ob.MenuId AND o.OptionIndex = ob.OptionIndex "
        "ORDER BY o.MenuId, o.OptionIndex");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 gossip_menu_option IDs. DB table `gossip_menu_option` is empty!");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        GossipMenuItems gMenuItem;

        gMenuItem.MenuId                = fields[0].GetUInt32();
        gMenuItem.OptionIndex           = fields[1].GetUInt32();
        gMenuItem.OptionIcon            = GossipOptionIcon(fields[2].GetUInt8());
        gMenuItem.OptionText            = fields[3].GetString();
        gMenuItem.OptionBroadcastTextId = fields[4].GetUInt32();
        gMenuItem.OptionType            = fields[5].GetUInt32();
        gMenuItem.OptionNpcFlag         = fields[6].GetUInt64();
        gMenuItem.ActionMenuId          = fields[7].GetUInt32();
        gMenuItem.ActionPoiId           = fields[8].GetUInt32();
        gMenuItem.BoxCoded              = fields[9].GetBool();
        gMenuItem.BoxMoney              = fields[10].GetUInt32();
        gMenuItem.BoxText               = fields[11].GetString();
        gMenuItem.BoxBroadcastTextId    = fields[12].GetUInt32();

        if (gMenuItem.OptionIcon >= GossipOptionIcon::Count)
        {
            TC_LOG_ERROR("sql.sql", "Table `gossip_menu_option` for MenuId %u, OptionIndex %u has unknown icon id %u. Replacing with GossipOptionIcon::None", gMenuItem.MenuId, gMenuItem.OptionIndex, uint8(gMenuItem.OptionIcon));
            gMenuItem.OptionIcon = GossipOptionIcon::None;
        }

        if (gMenuItem.OptionBroadcastTextId)
        {
            if (!sBroadcastTextStore.LookupEntry(gMenuItem.OptionBroadcastTextId))
            {
                TC_LOG_ERROR("sql.sql", "Table `gossip_menu_option` for MenuId %u, OptionIndex %u has non-existing or incompatible OptionBroadcastTextID %u, ignoring.", gMenuItem.MenuId, gMenuItem.OptionIndex, gMenuItem.OptionBroadcastTextId);
                gMenuItem.OptionBroadcastTextId = 0;
            }
        }

        if (gMenuItem.OptionType >= GOSSIP_OPTION_MAX)
            TC_LOG_ERROR("sql.sql", "Table `gossip_menu_option` for MenuId %u, OptionIndex %u has unknown option id %u. Option will not be used", gMenuItem.MenuId, gMenuItem.OptionIndex, gMenuItem.OptionType);

        if (gMenuItem.ActionPoiId && !GetPointOfInterest(gMenuItem.ActionPoiId))
        {
            TC_LOG_ERROR("sql.sql", "Table `gossip_menu_option` for MenuId %u, OptionIndex %u use non-existing action_poi_id %u, ignoring", gMenuItem.MenuId, gMenuItem.OptionIndex, gMenuItem.ActionPoiId);
            gMenuItem.ActionPoiId = 0;
        }

        if (gMenuItem.BoxBroadcastTextId)
        {
            if (!sBroadcastTextStore.LookupEntry(gMenuItem.BoxBroadcastTextId))
            {
                TC_LOG_ERROR("sql.sql", "Table `gossip_menu_option` for MenuId %u, OptionIndex %u has non-existing or incompatible BoxBroadcastTextId %u, ignoring.", gMenuItem.MenuId, gMenuItem.OptionIndex, gMenuItem.BoxBroadcastTextId);
                gMenuItem.BoxBroadcastTextId = 0;
            }
        }

        _gossipMenuItemsStore.insert(GossipMenuItemsContainer::value_type(gMenuItem.MenuId, gMenuItem));
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded " SZFMTD " gossip_menu_option entries in %u ms", _gossipMenuItemsStore.size(), GetMSTimeDiffToNow(oldMSTime));
}

Trainer::Trainer const* ObjectMgr::GetTrainer(uint32 trainerId) const
{
    return Trinity::Containers::MapGetValuePtr(_trainers, trainerId);
}

uint32 ObjectMgr::GetCreatureTrainerForGossipOption(uint32 creatureId, uint32 gossipMenuId, uint32 gossipOptionIndex) const
{
    auto itr = _creatureDefaultTrainers.find(std::make_tuple(creatureId, gossipMenuId, gossipOptionIndex));
    if (itr != _creatureDefaultTrainers.end())
        return itr->second;

    return 0;
}

void ObjectMgr::AddVendorItem(uint32 entry, VendorItem const& vItem, bool persist /*= true*/)
{
    VendorItemData& vList = _cacheVendorItemStore[entry];
    vList.AddItem(vItem);

    if (persist)
    {
        WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_INS_NPC_VENDOR);

        stmt->setUInt32(0, entry);
        stmt->setUInt32(1, vItem.item);
        stmt->setUInt8(2, vItem.maxcount);
        stmt->setUInt32(3, vItem.incrtime);
        stmt->setUInt32(4, vItem.ExtendedCost);
        stmt->setUInt8(5, vItem.Type);

        WorldDatabase.Execute(stmt);
    }
}

bool ObjectMgr::RemoveVendorItem(uint32 entry, uint32 item, uint8 type, bool persist /*= true*/)
{
    CacheVendorItemContainer::iterator  iter = _cacheVendorItemStore.find(entry);
    if (iter == _cacheVendorItemStore.end())
        return false;

    if (!iter->second.RemoveItem(item, type))
        return false;

    if (persist)
    {
        WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_DEL_NPC_VENDOR);

        stmt->setUInt32(0, entry);
        stmt->setUInt32(1, item);
        stmt->setUInt8(2, type);

        WorldDatabase.Execute(stmt);
    }

    return true;
}

bool ObjectMgr::IsVendorItemValid(uint32 vendor_entry, VendorItem const& vItem, Player* player, std::set<uint32>* skip_vendors, uint32 ORnpcflag) const
{
    CreatureTemplate const* cInfo = sObjectMgr->GetCreatureTemplate(vendor_entry);
    if (!cInfo)
    {
        if (player)
            ChatHandler(player->GetSession()).SendSysMessage(LANG_COMMAND_VENDORSELECTION);
        else
            TC_LOG_ERROR("sql.sql", "Table `(game_event_)npc_vendor` has data for nonexistent creature template (Entry: %u), ignore", vendor_entry);
        return false;
    }

    if (!((cInfo->npcflag | ORnpcflag) & UNIT_NPC_FLAG_VENDOR))
    {
        if (!skip_vendors || skip_vendors->count(vendor_entry) == 0)
        {
            if (player)
                ChatHandler(player->GetSession()).SendSysMessage(LANG_COMMAND_VENDORSELECTION);
            else
                TC_LOG_ERROR("sql.sql", "Table `(game_event_)npc_vendor` has data for creature template (Entry: %u) without vendor flag, ignore", vendor_entry);

            if (skip_vendors)
                skip_vendors->insert(vendor_entry);
        }
        return false;
    }

    if ((vItem.Type == ITEM_VENDOR_TYPE_ITEM && !sObjectMgr->GetItemTemplate(vItem.item)) ||
        (vItem.Type == ITEM_VENDOR_TYPE_CURRENCY && !sCurrencyTypesStore.LookupEntry(vItem.item)))
    {
        if (player)
            ChatHandler(player->GetSession()).PSendSysMessage(LANG_ITEM_NOT_FOUND, vItem.item, vItem.Type);
        else
            TC_LOG_ERROR("sql.sql", "Table `(game_event_)npc_vendor` for Vendor (Entry: %u) have in item list non-existed item (%u, type %u), ignore", vendor_entry, vItem.item, vItem.Type);
        return false;
    }

    if (vItem.PlayerConditionId && !sPlayerConditionStore.LookupEntry(vItem.PlayerConditionId))
    {
        TC_LOG_ERROR("sql.sql", "Table `(game_event_)npc_vendor` has Item (Entry: %u) with invalid PlayerConditionId (%u) for vendor (%u), ignore", vItem.item, vItem.PlayerConditionId, vendor_entry);
        return false;
    }

    if (vItem.ExtendedCost && !sItemExtendedCostStore.LookupEntry(vItem.ExtendedCost))
    {
        if (player)
            ChatHandler(player->GetSession()).PSendSysMessage(LANG_EXTENDED_COST_NOT_EXIST, vItem.ExtendedCost);
        else
            TC_LOG_ERROR("sql.sql", "Table `(game_event_)npc_vendor` has Item (Entry: %u) with wrong ExtendedCost (%u) for vendor (%u), ignore", vItem.item, vItem.ExtendedCost, vendor_entry);
        return false;
    }

    if (vItem.Type == ITEM_VENDOR_TYPE_ITEM) // not applicable to currencies
    {
        if (vItem.maxcount > 0 && vItem.incrtime == 0)
        {
            if (player)
                ChatHandler(player->GetSession()).PSendSysMessage("MaxCount != 0 (%u) but IncrTime == 0", vItem.maxcount);
            else
                TC_LOG_ERROR("sql.sql", "Table `(game_event_)npc_vendor` has `maxcount` (%u) for item %u of vendor (Entry: %u) but `incrtime`=0, ignore", vItem.maxcount, vItem.item, vendor_entry);
            return false;
        }
        else if (vItem.maxcount == 0 && vItem.incrtime > 0)
        {
            if (player)
                ChatHandler(player->GetSession()).PSendSysMessage("MaxCount == 0 but IncrTime<>= 0");
            else
                TC_LOG_ERROR("sql.sql", "Table `(game_event_)npc_vendor` has `maxcount`=0 for item %u of vendor (Entry: %u) but `incrtime`<>0, ignore", vItem.item, vendor_entry);
            return false;
        }

        for (int32 bonusList : vItem.BonusListIDs)
        {
            if (!sDB2Manager.GetItemBonusList(bonusList))
            {
                TC_LOG_ERROR("sql.sql", "Table `(game_event_)npc_vendor` have Item (Entry: %u) with invalid bonus %u for vendor (%u), ignore", vItem.item, bonusList, vendor_entry);
                return false;
            }
        }
    }

    VendorItemData const* vItems = GetNpcVendorItemList(vendor_entry);
    if (!vItems)
        return true;                                        // later checks for non-empty lists

    if (vItems->FindItemCostPair(vItem.item, vItem.ExtendedCost, vItem.Type))
    {
        if (player)
            ChatHandler(player->GetSession()).PSendSysMessage(LANG_ITEM_ALREADY_IN_LIST, vItem.item, vItem.ExtendedCost, vItem.Type);
        else
            TC_LOG_ERROR("sql.sql", "Table `npc_vendor` has duplicate items %u (with extended cost %u, type %u) for vendor (Entry: %u), ignoring", vItem.item, vItem.ExtendedCost, vItem.Type, vendor_entry);
        return false;
    }

    if (vItem.Type == ITEM_VENDOR_TYPE_CURRENCY && vItem.maxcount == 0)
    {
        TC_LOG_ERROR("sql.sql", "Table `(game_event_)npc_vendor` have Item (Entry: %u, type: %u) with missing maxcount for vendor (%u), ignore", vItem.item, vItem.Type, vendor_entry);
        return false;
    }

    return true;
}

ObjectMgr::ScriptNameContainer::ScriptNameContainer()
{
    // We insert an empty placeholder here so we can use the
    // script id 0 as dummy for "no script found".
    uint32 const id = insert("", false);

    ASSERT(id == 0);
    (void)id;
}

void ObjectMgr::ScriptNameContainer::reserve(size_t capacity)
{
    IndexToName.reserve(capacity);
}

uint32 ObjectMgr::ScriptNameContainer::insert(std::string const& scriptName, bool isScriptNameBound)
{
    // c++17 try_emplace
    auto const itr = NameToIndex.find(scriptName);
    if (itr != NameToIndex.end())
    {
        return itr->second.Id;
    }
    else
    {
        ASSERT(NameToIndex.size() < std::numeric_limits<uint32>::max());
        uint32 const id = static_cast<uint32>(NameToIndex.size());

        auto result = NameToIndex.insert({ scriptName, Entry{ id, isScriptNameBound } });
        IndexToName.emplace_back(result.first);

        return id;
    }
}

size_t ObjectMgr::ScriptNameContainer::size() const
{
    return IndexToName.size();
}

ObjectMgr::ScriptNameContainer::NameMap::const_iterator ObjectMgr::ScriptNameContainer::find(size_t index) const
{
    return index < IndexToName.size() ? IndexToName[index] : end();
}

ObjectMgr::ScriptNameContainer::NameMap::const_iterator ObjectMgr::ScriptNameContainer::find(std::string const& name) const
{
    // assume "" is the first element
    if (name.empty())
        return end();

    return NameToIndex.find(name);
}

ObjectMgr::ScriptNameContainer::NameMap::const_iterator ObjectMgr::ScriptNameContainer::end() const
{
    return NameToIndex.end();
}

std::unordered_set<std::string> ObjectMgr::ScriptNameContainer::GetAllDBScriptNames() const
{
    std::unordered_set<std::string> scriptNames;

    for (std::pair<std::string const, Entry> const& entry : NameToIndex)
    {
        if (entry.second.IsScriptDatabaseBound)
        {
            scriptNames.insert(entry.first);
        }
    }

    return scriptNames;
}

std::unordered_set<std::string> ObjectMgr::GetAllDBScriptNames() const
{
    return _scriptNamesStore.GetAllDBScriptNames();
}

std::string const& ObjectMgr::GetScriptName(uint32 id) const
{
    auto const itr = _scriptNamesStore.find(id);
    if (itr != _scriptNamesStore.end())
    {
        return itr->first;
    }
    else
    {
        static std::string const empty;
        return empty;
    }
}

bool ObjectMgr::IsScriptDatabaseBound(uint32 id) const
{
    auto const itr = _scriptNamesStore.find(id);
    if (itr != _scriptNamesStore.end())
    {
        return itr->second.IsScriptDatabaseBound;
    }
    else
    {
        return false;
    }
}

uint32 ObjectMgr::GetScriptId(std::string const& name, bool isDatabaseBound)
{
    return _scriptNamesStore.insert(name, isDatabaseBound);
}

CreatureBaseStats const* ObjectMgr::GetCreatureBaseStats(uint8 level, uint8 unitClass)
{
    CreatureBaseStatsContainer::const_iterator it = _creatureBaseStatsStore.find(MAKE_PAIR16(level, unitClass));

    if (it != _creatureBaseStatsStore.end())
        return &(it->second);

    struct DefaultCreatureBaseStats : public CreatureBaseStats
    {
        DefaultCreatureBaseStats()
        {
            BaseMana = 0;
            AttackPower = 0;
            RangedAttackPower = 0;
        }
    };
    static const DefaultCreatureBaseStats defStats;
    return &defStats;
}

void ObjectMgr::LoadCreatureClassLevelStats()
{
    uint32 oldMSTime = getMSTime();
    //                                               0      1      2         3            4
    QueryResult result = WorldDatabase.Query("SELECT level, class, basemana, attackpower, rangedattackpower FROM creature_classlevelstats");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 creature base stats. DB table `creature_classlevelstats` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint8 Level = fields[0].GetUInt8();
        uint8 Class = fields[1].GetUInt8();

        if (!Class || ((1 << (Class - 1)) & CLASSMASK_ALL_CREATURES) == 0)
            TC_LOG_ERROR("sql.sql", "Creature base stats for level %u has invalid class %u", Level, Class);

        CreatureBaseStats stats;

        stats.BaseMana = fields[2].GetUInt32();

        stats.AttackPower = fields[3].GetUInt16();
        stats.RangedAttackPower = fields[4].GetUInt16();

        _creatureBaseStatsStore[MAKE_PAIR16(Level, Class)] = stats;

        ++count;
    }
    while (result->NextRow());

    for (auto const& creatureTemplatePair : _creatureTemplateStore)
    {
        std::pair<int16, int16> levels = creatureTemplatePair.second.GetMinMaxLevel();
        for (int16 lvl = levels.first; lvl <= levels.second; ++lvl)
        {
            if (!_creatureBaseStatsStore.count(MAKE_PAIR16(lvl, creatureTemplatePair.second.unit_class)))
                TC_LOG_ERROR("sql.sql", "Missing base stats for creature class %u level %u", creatureTemplatePair.second.unit_class, lvl);
        }
    }

    TC_LOG_INFO("server.loading", ">> Loaded %u creature base stats in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadFactionChangeAchievements()
{
    uint32 oldMSTime = getMSTime();

    QueryResult result = WorldDatabase.Query("SELECT alliance_id, horde_id FROM player_factionchange_achievement");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 faction change achievement pairs. DB table `player_factionchange_achievement` is empty.");
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();

        uint32 alliance = fields[0].GetUInt32();
        uint32 horde = fields[1].GetUInt32();

        if (!sAchievementStore.LookupEntry(alliance))
            TC_LOG_ERROR("sql.sql", "Achievement %u (alliance_id) referenced in `player_factionchange_achievement` does not exist, pair skipped!", alliance);
        else if (!sAchievementStore.LookupEntry(horde))
            TC_LOG_ERROR("sql.sql", "Achievement %u (horde_id) referenced in `player_factionchange_achievement` does not exist, pair skipped!", horde);
        else
            FactionChangeAchievements[alliance] = horde;

        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u faction change achievement pairs in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadFactionChangeItems()
{
    uint32 oldMSTime = getMSTime();

    QueryResult result = WorldDatabase.Query("SELECT alliance_id, horde_id FROM player_factionchange_items");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 faction change item pairs. DB table `player_factionchange_items` is empty.");
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();

        uint32 alliance = fields[0].GetUInt32();
        uint32 horde = fields[1].GetUInt32();

        if (!GetItemTemplate(alliance))
            TC_LOG_ERROR("sql.sql", "Item %u (alliance_id) referenced in `player_factionchange_items` does not exist, pair skipped!", alliance);
        else if (!GetItemTemplate(horde))
            TC_LOG_ERROR("sql.sql", "Item %u (horde_id) referenced in `player_factionchange_items` does not exist, pair skipped!", horde);
        else
            FactionChangeItems[alliance] = horde;

        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u faction change item pairs in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadFactionChangeQuests()
{
    uint32 oldMSTime = getMSTime();

    QueryResult result = WorldDatabase.Query("SELECT alliance_id, horde_id FROM player_factionchange_quests");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 faction change quest pairs. DB table `player_factionchange_quests` is empty.");
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();

        uint32 alliance = fields[0].GetUInt32();
        uint32 horde = fields[1].GetUInt32();

        if (!sObjectMgr->GetQuestTemplate(alliance))
            TC_LOG_ERROR("sql.sql", "Quest %u (alliance_id) referenced in `player_factionchange_quests` does not exist, pair skipped!", alliance);
        else if (!sObjectMgr->GetQuestTemplate(horde))
            TC_LOG_ERROR("sql.sql", "Quest %u (horde_id) referenced in `player_factionchange_quests` does not exist, pair skipped!", horde);
        else
            FactionChangeQuests[alliance] = horde;

        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u faction change quest pairs in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadFactionChangeReputations()
{
    uint32 oldMSTime = getMSTime();

    QueryResult result = WorldDatabase.Query("SELECT alliance_id, horde_id FROM player_factionchange_reputations");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 faction change reputation pairs. DB table `player_factionchange_reputations` is empty.");
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();

        uint32 alliance = fields[0].GetUInt32();
        uint32 horde = fields[1].GetUInt32();

        if (!sFactionStore.LookupEntry(alliance))
            TC_LOG_ERROR("sql.sql", "Reputation %u (alliance_id) referenced in `player_factionchange_reputations` does not exist, pair skipped!", alliance);
        else if (!sFactionStore.LookupEntry(horde))
            TC_LOG_ERROR("sql.sql", "Reputation %u (horde_id) referenced in `player_factionchange_reputations` does not exist, pair skipped!", horde);
        else
            FactionChangeReputation[alliance] = horde;

        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u faction change reputation pairs in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadFactionChangeSpells()
{
    uint32 oldMSTime = getMSTime();

    QueryResult result = WorldDatabase.Query("SELECT alliance_id, horde_id FROM player_factionchange_spells");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 faction change spell pairs. DB table `player_factionchange_spells` is empty.");
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();

        uint32 alliance = fields[0].GetUInt32();
        uint32 horde = fields[1].GetUInt32();

        if (!sSpellMgr->GetSpellInfo(alliance, DIFFICULTY_NONE))
            TC_LOG_ERROR("sql.sql", "Spell %u (alliance_id) referenced in `player_factionchange_spells` does not exist, pair skipped!", alliance);
        else if (!sSpellMgr->GetSpellInfo(horde, DIFFICULTY_NONE))
            TC_LOG_ERROR("sql.sql", "Spell %u (horde_id) referenced in `player_factionchange_spells` does not exist, pair skipped!", horde);
        else
            FactionChangeSpells[alliance] = horde;

        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u faction change spell pairs in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadFactionChangeTitles()
{
    uint32 oldMSTime = getMSTime();

    QueryResult result = WorldDatabase.Query("SELECT alliance_id, horde_id FROM player_factionchange_titles");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 faction change title pairs. DB table `player_factionchange_title` is empty.");
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();

        uint32 alliance = fields[0].GetUInt32();
        uint32 horde = fields[1].GetUInt32();

        if (!sCharTitlesStore.LookupEntry(alliance))
            TC_LOG_ERROR("sql.sql", "Title %u (alliance_id) referenced in `player_factionchange_title` does not exist, pair skipped!", alliance);
        else if (!sCharTitlesStore.LookupEntry(horde))
            TC_LOG_ERROR("sql.sql", "Title %u (horde_id) referenced in `player_factionchange_title` does not exist, pair skipped!", horde);
        else
            FactionChangeTitles[alliance] = horde;

        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u faction change title pairs in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadPhases()
{
    for (PhaseEntry const* phase : sPhaseStore)
        _phaseInfoById.emplace(std::make_pair(phase->ID, PhaseInfoStruct{ phase->ID, std::unordered_set<uint32>{} }));

    for (MapEntry const* map : sMapStore)
        if (map->ParentMapID != -1)
            _terrainSwapInfoById.emplace(std::make_pair(map->ID, TerrainSwapInfo{ map->ID, std::vector<uint32>{} }));

    TC_LOG_INFO("server.loading", "Loading Terrain World Map definitions...");
    LoadTerrainWorldMaps();

    TC_LOG_INFO("server.loading", "Loading Terrain Swap Default definitions...");
    LoadTerrainSwapDefaults();

    TC_LOG_INFO("server.loading", "Loading Phase Area definitions...");
    LoadAreaPhases();
}

void ObjectMgr::UnloadPhaseConditions()
{
    for (auto itr = _phaseInfoByArea.begin(); itr != _phaseInfoByArea.end(); ++itr)
        for (PhaseAreaInfo& phase : itr->second)
            phase.Conditions.clear();
}

void ObjectMgr::LoadTerrainWorldMaps()
{
    uint32 oldMSTime = getMSTime();

    //                                               0               1
    QueryResult result = WorldDatabase.Query("SELECT TerrainSwapMap, UiMapPhaseId FROM `terrain_worldmap`");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 terrain world maps. DB table `terrain_worldmap` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 mapId = fields[0].GetUInt32();
        uint32 uiMapPhaseId = fields[1].GetUInt32();

        if (!sMapStore.LookupEntry(mapId))
        {
            TC_LOG_ERROR("sql.sql", "TerrainSwapMap %u defined in `terrain_worldmap` does not exist, skipped.", mapId);
            continue;
        }

        if (!sDB2Manager.IsUiMapPhase(uiMapPhaseId))
        {
            TC_LOG_ERROR("sql.sql", "Phase %u defined in `terrain_worldmap` is not a valid terrain swap phase, skipped.", uiMapPhaseId);
            continue;
        }

        TerrainSwapInfo* terrainSwapInfo = &_terrainSwapInfoById[mapId];
        terrainSwapInfo->Id = mapId;
        terrainSwapInfo->UiMapPhaseIDs.push_back(uiMapPhaseId);

        ++count;
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u terrain world maps in %u ms.", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadTerrainSwapDefaults()
{
    uint32 oldMSTime = getMSTime();

    //                                               0       1
    QueryResult result = WorldDatabase.Query("SELECT MapId, TerrainSwapMap FROM `terrain_swap_defaults`");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 terrain swap defaults. DB table `terrain_swap_defaults` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 mapId = fields[0].GetUInt32();

        if (!sMapStore.LookupEntry(mapId))
        {
            TC_LOG_ERROR("sql.sql", "Map %u defined in `terrain_swap_defaults` does not exist, skipped.", mapId);
            continue;
        }

        uint32 terrainSwap = fields[1].GetUInt32();

        if (!sMapStore.LookupEntry(terrainSwap))
        {
            TC_LOG_ERROR("sql.sql", "TerrainSwapMap %u defined in `terrain_swap_defaults` does not exist, skipped.", terrainSwap);
            continue;
        }

        TerrainSwapInfo* terrainSwapInfo = &_terrainSwapInfoById[terrainSwap];
        terrainSwapInfo->Id = terrainSwap;
        _terrainSwapInfoByMap[mapId].push_back(terrainSwapInfo);

        ++count;
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u terrain swap defaults in %u ms.", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadAreaPhases()
{
    uint32 oldMSTime = getMSTime();

    //                                               0       1
    QueryResult result = WorldDatabase.Query("SELECT AreaId, PhaseId FROM `phase_area`");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 phase areas. DB table `phase_area` is empty.");
        return;
    }

    auto getOrCreatePhaseIfMissing = [this](uint32 phaseId)
    {
        PhaseInfoStruct* phaseInfo = &_phaseInfoById[phaseId];
        phaseInfo->Id = phaseId;
        return phaseInfo;
    };

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        uint32 area = fields[0].GetUInt32();
        uint32 phaseId = fields[1].GetUInt32();
        if (!sAreaTableStore.LookupEntry(area))
        {
            TC_LOG_ERROR("sql.sql", "Area %u defined in `phase_area` does not exist, skipped.", area);
            continue;
        }

        if (!sPhaseStore.LookupEntry(phaseId))
        {
            TC_LOG_ERROR("sql.sql", "Phase %u defined in `phase_area` does not exist, skipped.", phaseId);
            continue;
        }

        PhaseInfoStruct* phase = getOrCreatePhaseIfMissing(phaseId);
        phase->Areas.insert(area);
        _phaseInfoByArea[area].emplace_back(phase);

        ++count;
    } while (result->NextRow());

    for (auto itr = _phaseInfoByArea.begin(); itr != _phaseInfoByArea.end(); ++itr)
    {
        for (PhaseAreaInfo& phase : itr->second)
        {
            uint32 parentAreaId = itr->first;
            do
            {
                AreaTableEntry const* area = sAreaTableStore.LookupEntry(parentAreaId);
                if (!area)
                    break;

                parentAreaId = area->ParentAreaID;
                if (!parentAreaId)
                    break;

                if (std::vector<PhaseAreaInfo>* parentAreaPhases = Trinity::Containers::MapGetValuePtr(_phaseInfoByArea, parentAreaId))
                    for (PhaseAreaInfo& parentAreaPhase : *parentAreaPhases)
                        if (parentAreaPhase.PhaseInfo->Id == phase.PhaseInfo->Id)
                            parentAreaPhase.SubAreaExclusions.insert(itr->first);

            } while (true);
        }
    }

    TC_LOG_INFO("server.loading", ">> Loaded %u phase areas in %u ms.", count, GetMSTimeDiffToNow(oldMSTime));
}

bool PhaseInfoStruct::IsAllowedInArea(uint32 areaId) const
{
    return std::any_of(Areas.begin(), Areas.end(), [areaId](uint32 areaToCheck)
    {
        return DB2Manager::IsInArea(areaId, areaToCheck);
    });
}

PhaseInfoStruct const* ObjectMgr::GetPhaseInfo(uint32 phaseId) const
{
    return Trinity::Containers::MapGetValuePtr(_phaseInfoById, phaseId);
}

std::vector<PhaseAreaInfo> const* ObjectMgr::GetPhasesForArea(uint32 areaId) const
{
    return Trinity::Containers::MapGetValuePtr(_phaseInfoByArea, areaId);
}

TerrainSwapInfo const* ObjectMgr::GetTerrainSwapInfo(uint32 terrainSwapId) const
{
    return Trinity::Containers::MapGetValuePtr(_terrainSwapInfoById, terrainSwapId);
}

GameObjectTemplate const* ObjectMgr::GetGameObjectTemplate(uint32 entry) const
{
    return Trinity::Containers::MapGetValuePtr(_gameObjectTemplateStore, entry);
}

GameObjectTemplateAddon const* ObjectMgr::GetGameObjectTemplateAddon(uint32 entry) const
{
    auto itr = _gameObjectTemplateAddonStore.find(entry);
    if (itr != _gameObjectTemplateAddonStore.end())
        return &itr->second;

    return nullptr;
}

GameObjectOverride const* ObjectMgr::GetGameObjectOverride(ObjectGuid::LowType spawnId) const
{
    return Trinity::Containers::MapGetValuePtr(_gameObjectOverrideStore, spawnId);
}

CreatureTemplate const* ObjectMgr::GetCreatureTemplate(uint32 entry) const
{
    return Trinity::Containers::MapGetValuePtr(_creatureTemplateStore, entry);
}

QuestPOIData const* ObjectMgr::GetQuestPOIData(int32 questId)
{
    return Trinity::Containers::MapGetValuePtr(_questPOIStore, questId);
}

VehicleTemplate const* ObjectMgr::GetVehicleTemplate(Vehicle* veh) const
{
    return Trinity::Containers::MapGetValuePtr(_vehicleTemplateStore, veh->GetCreatureEntry());
}

VehicleAccessoryList const* ObjectMgr::GetVehicleAccessoryList(Vehicle* veh) const
{
    if (Creature* cre = veh->GetBase()->ToCreature())
    {
        // Give preference to GUID-based accessories
        VehicleAccessoryContainer::const_iterator itr = _vehicleAccessoryStore.find(cre->GetSpawnId());
        if (itr != _vehicleAccessoryStore.end())
            return &itr->second;
    }

    // Otherwise return entry-based
    VehicleAccessoryTemplateContainer::const_iterator itr = _vehicleTemplateAccessoryStore.find(veh->GetCreatureEntry());
    if (itr != _vehicleTemplateAccessoryStore.end())
        return &itr->second;
    return nullptr;
}

DungeonEncounterList const* ObjectMgr::GetDungeonEncounterList(uint32 mapId, Difficulty difficulty) const
{
    auto itr = _dungeonEncounterStore.find(MAKE_PAIR64(mapId, difficulty));
    if (itr != _dungeonEncounterStore.end())
        return &itr->second;
    return nullptr;
}

PlayerInfo const* ObjectMgr::GetPlayerInfo(uint32 race, uint32 class_) const
{
    if (race >= MAX_RACES)
        return nullptr;
    if (class_ >= MAX_CLASSES)
        return nullptr;
    auto const& info = _playerInfo[race][class_];
    if (!info)
        return nullptr;
    return info.get();
}

void ObjectMgr::LoadRaceAndClassExpansionRequirements()
{
    uint32 oldMSTime = getMSTime();
    _raceUnlockRequirementStore.clear();

    //                                               0       1          2
    QueryResult result = WorldDatabase.Query("SELECT raceID, expansion, achievementId FROM `race_unlock_requirement`");

    if (result)
    {
        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();

            uint8 raceID = fields[0].GetUInt8();
            uint8 expansion = fields[1].GetUInt8();
            uint32 achievementId = fields[2].GetUInt32();

            ChrRacesEntry const* raceEntry = sChrRacesStore.LookupEntry(raceID);
            if (!raceEntry)
            {
                TC_LOG_ERROR("sql.sql", "Race %u defined in `race_unlock_requirement` does not exists, skipped.", raceID);
                continue;
            }

            if (expansion >= MAX_ACCOUNT_EXPANSIONS)
            {
                TC_LOG_ERROR("sql.sql", "Race %u defined in `race_unlock_requirement` has incorrect expansion %u, skipped.", raceID, expansion);
                continue;
            }

            if (achievementId && !sAchievementStore.LookupEntry(achievementId))
            {
                TC_LOG_ERROR("sql.sql", "Race %u defined in `race_unlock_requirement` has incorrect achievement %u, skipped.", raceID, achievementId);
                continue;
            }

            RaceUnlockRequirement& raceUnlockRequirement = _raceUnlockRequirementStore[raceID];
            raceUnlockRequirement.Expansion = expansion;
            raceUnlockRequirement.AchievementId = achievementId;

            ++count;
        }
        while (result->NextRow());
        TC_LOG_INFO("server.loading", ">> Loaded " SZFMTD " race expansion requirements in %u ms.", _raceUnlockRequirementStore.size(), GetMSTimeDiffToNow(oldMSTime));
    }
    else
        TC_LOG_INFO("server.loading", ">> Loaded 0 race expansion requirements. DB table `race_expansion_requirement` is empty.");

    oldMSTime = getMSTime();
    _classExpansionRequirementStore.clear();

    //                                         0       1                     2                      3
    result = WorldDatabase.Query("SELECT ClassID, RaceID, ActiveExpansionLevel, AccountExpansionLevel FROM `class_expansion_requirement`");

    if (result)
    {
        std::map<uint8, std::map<uint8, std::pair<uint8, uint8>>> temp;
        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();

            uint8 classID = fields[0].GetUInt8();
            uint8 raceID = fields[1].GetUInt8();
            uint8 activeExpansionLevel = fields[2].GetUInt8();
            uint8 accountExpansionLevel = fields[3].GetUInt8();

            ChrClassesEntry const* classEntry = sChrClassesStore.LookupEntry(classID);
            if (!classEntry)
            {
                TC_LOG_ERROR("sql.sql", "Class %u (race %u) defined in `class_expansion_requirement` does not exists, skipped.",
                    uint32(classID), uint32(raceID));
                continue;
            }

            ChrRacesEntry const* raceEntry = sChrRacesStore.LookupEntry(raceID);
            if (!raceEntry)
            {
                TC_LOG_ERROR("sql.sql", "Race %u (class %u) defined in `class_expansion_requirement` does not exists, skipped.",
                    uint32(raceID), uint32(classID));
                continue;
            }

            if (activeExpansionLevel >= MAX_EXPANSIONS)
            {
                TC_LOG_ERROR("sql.sql", "Class %u Race %u defined in `class_expansion_requirement` has incorrect ActiveExpansionLevel %u, skipped.",
                    uint32(classID), uint32(raceID), activeExpansionLevel);
                continue;
            }

            if (accountExpansionLevel >= MAX_ACCOUNT_EXPANSIONS)
            {
                TC_LOG_ERROR("sql.sql", "Class %u Race %u defined in `class_expansion_requirement` has incorrect AccountExpansionLevel %u, skipped.",
                    uint32(classID), uint32(raceID), accountExpansionLevel);
                continue;
            }

            temp[raceID][classID] = { activeExpansionLevel, accountExpansionLevel };

            ++count;
        }
        while (result->NextRow());

        for (auto&& race : temp)
        {
            _classExpansionRequirementStore.emplace_back();

            RaceClassAvailability& raceClassAvailability = _classExpansionRequirementStore.back();
            raceClassAvailability.RaceID = race.first;

            for (auto&& class_ : race.second)
            {
                raceClassAvailability.Classes.emplace_back();

                ClassAvailability& classAvailability = raceClassAvailability.Classes.back();
                classAvailability.ClassID = class_.first;
                classAvailability.ActiveExpansionLevel = class_.second.first;
                classAvailability.AccountExpansionLevel = class_.second.second;
            }
        }

        TC_LOG_INFO("server.loading", ">> Loaded %u class expansion requirements in %u ms.", count, GetMSTimeDiffToNow(oldMSTime));
    }
    else
        TC_LOG_INFO("server.loading", ">> Loaded 0 class expansion requirements. DB table `class_expansion_requirement` is empty.");
}

void ObjectMgr::LoadRealmNames()
{
    uint32 oldMSTime = getMSTime();
    _realmNameStore.clear();

    //                                               0   1
    QueryResult result = LoginDatabase.Query("SELECT id, name FROM `realmlist`");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 realm names. DB table `realmlist` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 realmId = fields[0].GetUInt32();
        std::string realmName = fields[1].GetString();

        _realmNameStore[realmId] = realmName;

        ++count;
    }
    while (result->NextRow());
    TC_LOG_INFO("server.loading", ">> Loaded %u realm names in %u ms.", count, GetMSTimeDiffToNow(oldMSTime));
}

std::string ObjectMgr::GetRealmName(uint32 realmId) const
{
    RealmNameContainer::const_iterator iter = _realmNameStore.find(realmId);
    return iter != _realmNameStore.end() ? iter->second : "";
}

std::string ObjectMgr::GetNormalizedRealmName(uint32 realmId) const
{
    std::string name = GetRealmName(realmId);
    name.erase(std::remove_if(name.begin(), name.end(), ::isspace), name.end());
    return name;
}

bool ObjectMgr::GetRealmName(uint32 realmId, std::string& name, std::string& normalizedName) const
{
    RealmNameContainer::const_iterator itr = _realmNameStore.find(realmId);
    if (itr != _realmNameStore.end())
    {
        name = itr->second;
        normalizedName = itr->second;
        normalizedName.erase(std::remove_if(normalizedName.begin(), normalizedName.end(), ::isspace), normalizedName.end());
        return true;
    }
    return false;
}

ClassAvailability const* ObjectMgr::GetClassExpansionRequirement(uint8 raceId, uint8 classId) const
{
    auto raceItr = std::find_if(_classExpansionRequirementStore.begin(), _classExpansionRequirementStore.end(), [raceId](RaceClassAvailability const& raceClass)
    {
        return raceClass.RaceID == raceId;
    });
    if (raceItr == _classExpansionRequirementStore.end())
        return nullptr;

    auto classItr = std::find_if(raceItr->Classes.begin(), raceItr->Classes.end(), [classId](ClassAvailability const& classAvailability)
    {
        return classAvailability.ClassID == classId;
    });
    if (classItr == raceItr->Classes.end())
        return nullptr;

    return &(*classItr);
}

PlayerChoice const* ObjectMgr::GetPlayerChoice(int32 choiceId) const
{
    return Trinity::Containers::MapGetValuePtr(_playerChoices, choiceId);
}

void ObjectMgr::LoadGameObjectQuestItems()
{
    uint32 oldMSTime = getMSTime();

    //                                               0                1       2
    QueryResult result = WorldDatabase.Query("SELECT GameObjectEntry, ItemId, Idx FROM gameobject_questitem ORDER BY Idx ASC");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 gameobject quest items. DB table `gameobject_questitem` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 entry = fields[0].GetUInt32();
        uint32 item  = fields[1].GetUInt32();
        uint32 idx   = fields[2].GetUInt32();

        GameObjectTemplate const* goInfo = GetGameObjectTemplate(entry);
        if (!goInfo)
        {
            TC_LOG_ERROR("sql.sql", "Table `gameobject_questitem` has data for nonexistent gameobject (entry: %u, idx: %u), skipped", entry, idx);
            continue;
        };

        ItemEntry const* db2Data = sItemStore.LookupEntry(item);
        if (!db2Data)
        {
            TC_LOG_ERROR("sql.sql", "Table `gameobject_questitem` has nonexistent item (ID: %u) in gameobject (entry: %u, idx: %u), skipped", item, entry, idx);
            continue;
        };

        _gameObjectQuestItemStore[entry].push_back(item);

        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u gameobject quest items in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadCreatureQuestItems()
{
    uint32 oldMSTime = getMSTime();

    //                                               0              1       2
    QueryResult result = WorldDatabase.Query("SELECT CreatureEntry, ItemId, Idx FROM creature_questitem ORDER BY Idx ASC");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 creature quest items. DB table `creature_questitem` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 entry = fields[0].GetUInt32();
        uint32 item  = fields[1].GetUInt32();
        uint32 idx   = fields[2].GetUInt32();

        CreatureTemplate const* creatureInfo = GetCreatureTemplate(entry);
        if (!creatureInfo)
        {
            TC_LOG_ERROR("sql.sql", "Table `creature_questitem` has data for nonexistent creature (entry: %u, idx: %u), skipped", entry, idx);
            continue;
        };

        ItemEntry const* db2Data = sItemStore.LookupEntry(item);
        if (!db2Data)
        {
            TC_LOG_ERROR("sql.sql", "Table `creature_questitem` has nonexistent item (ID: %u) in creature (entry: %u, idx: %u), skipped", item, entry, idx);
            continue;
        };

        _creatureQuestItemStore[entry].push_back(item);

        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u creature quest items in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::InitializeQueriesData(QueryDataGroup mask)
{
    // cache disabled
    if (!sWorld->getBoolConfig(CONFIG_CACHE_DATA_QUERIES))
        return;

    // Initialize Query data for creatures
    if (mask & QUERY_DATA_CREATURES)
        for (auto& creatureTemplatePair : _creatureTemplateStore)
            creatureTemplatePair.second.InitializeQueryData();

    // Initialize Query Data for gameobjects
    if (mask & QUERY_DATA_GAMEOBJECTS)
        for (auto& gameObjectTemplatePair : _gameObjectTemplateStore)
            gameObjectTemplatePair.second.InitializeQueryData();

    // Initialize Query Data for quests
    if (mask & QUERY_DATA_QUESTS)
        for (auto& questTemplatePair : _questTemplates)
            questTemplatePair.second.InitializeQueryData();

    // Initialize Quest POI data
    if (mask & QUERY_DATA_POIS)
        for (auto& poiPair : _questPOIStore)
            poiPair.second.InitializeQueryData();
}

void QuestPOIData::InitializeQueryData()
{
    QueryDataBuffer << *this;
    QueryDataBuffer.shrink_to_fit();
}

void ObjectMgr::LoadSceneTemplates()
{
    uint32 oldMSTime = getMSTime();
    _sceneTemplateStore.clear();

    QueryResult templates = WorldDatabase.Query("SELECT SceneId, Flags, ScriptPackageID, Encrypted, ScriptName FROM scene_template");

    if (!templates)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 scene templates. DB table `scene_template` is empty.");
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = templates->Fetch();

        uint32 sceneId = fields[0].GetUInt32();
        SceneTemplate& sceneTemplate    = _sceneTemplateStore[sceneId];
        sceneTemplate.SceneId           = sceneId;
        sceneTemplate.PlaybackFlags     = static_cast<SceneFlag>(fields[1].GetUInt32());
        sceneTemplate.ScenePackageId    = fields[2].GetUInt32();
        sceneTemplate.Encrypted         = fields[3].GetUInt8() != 0;
        sceneTemplate.ScriptId          = sObjectMgr->GetScriptId(fields[4].GetCString());

    } while (templates->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u scene templates in %u ms.", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadPlayerChoices()
{
    uint32 oldMSTime = getMSTime();
    _playerChoices.clear();

    QueryResult choices = WorldDatabase.Query("SELECT ChoiceId, UiTextureKitId, SoundKitId, Question, HideWarboardHeader, KeepOpenAfterChoice FROM playerchoice");

    if (!choices)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 player choices. DB table `playerchoice` is empty.");
        return;
    }

    uint32 responseCount = 0;
    uint32 rewardCount = 0;
    uint32 itemRewardCount = 0;
    uint32 currencyRewardCount = 0;
    uint32 factionRewardCount = 0;
    uint32 itemChoiceRewardCount = 0;
    uint32 mawPowersCount = 0;

    do
    {
        Field* fields = choices->Fetch();

        int32 choiceId = fields[0].GetInt32();

        PlayerChoice& choice = _playerChoices[choiceId];
        choice.ChoiceId = choiceId;
        choice.UiTextureKitId = fields[1].GetInt32();
        choice.SoundKitId = fields[2].GetUInt32();
        choice.Question = fields[3].GetString();
        choice.HideWarboardHeader = fields[4].GetBool();
        choice.KeepOpenAfterChoice = fields[5].GetBool();

    } while (choices->NextRow());

    //                                                             0           1                   2                3      4            5
    if (QueryResult responses = WorldDatabase.Query("SELECT ChoiceId, ResponseId, ResponseIdentifier, ChoiceArtFileId, Flags, WidgetSetID, "
    //                         6           7        8               9      10      11         12              13           14            15             16
        "UiTextureAtlasElementID, SoundKitID, GroupID, UiTextureKitID, Answer, Header, SubHeader, ButtonTooltip, Description, Confirmation, RewardQuestID "
        "FROM playerchoice_response ORDER BY `Index` ASC"))
    {
        do
        {
            Field* fields = responses->Fetch();

            int32 choiceId      = fields[0].GetInt32();
            int32 responseId    = fields[1].GetInt32();

            PlayerChoice* choice = Trinity::Containers::MapGetValuePtr(_playerChoices, choiceId);
            if (!choice)
            {
                TC_LOG_ERROR("sql.sql", "Table `playerchoice_response` references non-existing ChoiceId: %d (ResponseId: %d), skipped", choiceId, responseId);
                continue;
            }

            choice->Responses.emplace_back();

            PlayerChoiceResponse& response = choice->Responses.back();
            response.ResponseId         = responseId;
            response.ResponseIdentifier = fields[2].GetUInt16();
            response.ChoiceArtFileId    = fields[3].GetInt32();
            response.Flags              = fields[4].GetInt32();
            response.WidgetSetID        = fields[5].GetUInt32();
            response.UiTextureAtlasElementID = fields[6].GetUInt32();
            response.SoundKitID         = fields[7].GetUInt32();
            response.GroupID            = fields[8].GetUInt8();
            response.UiTextureKitID     = fields[9].GetInt32();
            response.Answer             = fields[10].GetString();
            response.Header             = fields[11].GetString();
            response.SubHeader          = fields[12].GetString();
            response.ButtonTooltip      = fields[13].GetString();
            response.Description        = fields[14].GetString();
            response.Confirmation       = fields[15].GetString();
            if (!fields[16].IsNull())
                response.RewardQuestID  = fields[16].GetUInt32();

            ++responseCount;

        } while (responses->NextRow());
    }

    if (QueryResult rewards = WorldDatabase.Query("SELECT ChoiceId, ResponseId, TitleId, PackageId, SkillLineId, SkillPointCount, ArenaPointCount, HonorPointCount, Money, Xp FROM playerchoice_response_reward"))
    {
        do
        {
            Field* fields = rewards->Fetch();

            int32 choiceId      = fields[0].GetInt32();
            int32 responseId    = fields[1].GetInt32();

            PlayerChoice* choice = Trinity::Containers::MapGetValuePtr(_playerChoices, choiceId);
            if (!choice)
            {
                TC_LOG_ERROR("sql.sql", "Table `playerchoice_response_reward` references non-existing ChoiceId: %d (ResponseId: %d), skipped", choiceId, responseId);
                continue;
            }

            auto responseItr = std::find_if(choice->Responses.begin(), choice->Responses.end(),
                [responseId](PlayerChoiceResponse const& playerChoiceResponse) { return playerChoiceResponse.ResponseId == responseId; });
            if (responseItr == choice->Responses.end())
            {
                TC_LOG_ERROR("sql.sql", "Table `playerchoice_response_reward` references non-existing ResponseId: %d for ChoiceId %d, skipped", responseId, choiceId);
                continue;
            }

            responseItr->Reward = boost::in_place();

            PlayerChoiceResponseReward* reward = responseItr->Reward.get_ptr();
            reward->TitleId          = fields[2].GetInt32();
            reward->PackageId        = fields[3].GetInt32();
            reward->SkillLineId      = fields[4].GetInt32();
            reward->SkillPointCount  = fields[5].GetUInt32();
            reward->ArenaPointCount  = fields[6].GetUInt32();
            reward->HonorPointCount  = fields[7].GetUInt32();
            reward->Money            = fields[8].GetUInt64();
            reward->Xp               = fields[9].GetUInt32();
            ++rewardCount;

            if (reward->TitleId && !sCharTitlesStore.LookupEntry(reward->TitleId))
            {
                TC_LOG_ERROR("sql.sql", "Table `playerchoice_response_reward` references non-existing Title %d for ChoiceId %d, ResponseId: %d, set to 0",
                    reward->TitleId, choiceId, responseId);
                reward->TitleId = 0;
            }

            if (reward->PackageId && !sDB2Manager.GetQuestPackageItems(reward->PackageId))
            {
                TC_LOG_ERROR("sql.sql", "Table `playerchoice_response_reward` references non-existing QuestPackage %d for ChoiceId %d, ResponseId: %d, set to 0",
                    reward->TitleId, choiceId, responseId);
                reward->PackageId = 0;
            }

            if (reward->SkillLineId && !sSkillLineStore.LookupEntry(reward->SkillLineId))
            {
                TC_LOG_ERROR("sql.sql", "Table `playerchoice_response_reward` references non-existing SkillLine %d for ChoiceId %d, ResponseId: %d, set to 0",
                    reward->TitleId, choiceId, responseId);
                reward->SkillLineId = 0;
                reward->SkillPointCount = 0;
            }

        } while (rewards->NextRow());
    }

    if (QueryResult rewards = WorldDatabase.Query("SELECT ChoiceId, ResponseId, ItemId, BonusListIDs, Quantity FROM playerchoice_response_reward_item ORDER BY `Index` ASC"))
    {
        do
        {
            Field* fields = rewards->Fetch();

            int32 choiceId = fields[0].GetInt32();
            int32 responseId = fields[1].GetInt32();
            uint32 itemId = fields[2].GetUInt32();
            Tokenizer bonusListIDsTok(fields[3].GetString(), ' ');
            std::vector<int32> bonusListIds;
            for (char const* token : bonusListIDsTok)
                bonusListIds.push_back(int32(atol(token)));
            int32 quantity = fields[4].GetInt32();

            PlayerChoice* choice = Trinity::Containers::MapGetValuePtr(_playerChoices, choiceId);
            if (!choice)
            {
                TC_LOG_ERROR("sql.sql", "Table `playerchoice_response_reward_item` references non-existing ChoiceId: %d (ResponseId: %d), skipped", choiceId, responseId);
                continue;
            }

            auto responseItr = std::find_if(choice->Responses.begin(), choice->Responses.end(),
                [responseId](PlayerChoiceResponse const& playerChoiceResponse) { return playerChoiceResponse.ResponseId == responseId; });
            if (responseItr == choice->Responses.end())
            {
                TC_LOG_ERROR("sql.sql", "Table `playerchoice_response_reward_item` references non-existing ResponseId: %d for ChoiceId %d, skipped", responseId, choiceId);
                continue;
            }

            if (!responseItr->Reward)
            {
                TC_LOG_ERROR("sql.sql", "Table `playerchoice_response_reward_item` references non-existing player choice reward for ChoiceId %d, ResponseId: %d, skipped",
                    choiceId, responseId);
                continue;
            }

            if (!GetItemTemplate(itemId))
            {
                TC_LOG_ERROR("sql.sql", "Table `playerchoice_response_reward_item` references non-existing item %u for ChoiceId %d, ResponseId: %d, skipped",
                    itemId, choiceId, responseId);
                continue;
            }

            responseItr->Reward->Items.emplace_back(itemId, std::move(bonusListIds), quantity);
            ++itemRewardCount;

        } while (rewards->NextRow());
    }

    if (QueryResult rewards = WorldDatabase.Query("SELECT ChoiceId, ResponseId, CurrencyId, Quantity FROM playerchoice_response_reward_currency ORDER BY `Index` ASC"))
    {
        do
        {
            Field* fields = rewards->Fetch();

            int32 choiceId = fields[0].GetInt32();
            int32 responseId = fields[1].GetInt32();
            uint32 currencyId = fields[2].GetUInt32();
            int32 quantity = fields[3].GetInt32();

            PlayerChoice* choice = Trinity::Containers::MapGetValuePtr(_playerChoices, choiceId);
            if (!choice)
            {
                TC_LOG_ERROR("sql.sql", "Table `playerchoice_response_reward_currency` references non-existing ChoiceId: %d (ResponseId: %d), skipped", choiceId, responseId);
                continue;
            }

            auto responseItr = std::find_if(choice->Responses.begin(), choice->Responses.end(),
                [responseId](PlayerChoiceResponse const& playerChoiceResponse) { return playerChoiceResponse.ResponseId == responseId; });
            if (responseItr == choice->Responses.end())
            {
                TC_LOG_ERROR("sql.sql", "Table `playerchoice_response_reward_currency` references non-existing ResponseId: %d for ChoiceId %d, skipped", responseId, choiceId);
                continue;
            }

            if (!responseItr->Reward)
            {
                TC_LOG_ERROR("sql.sql", "Table `playerchoice_response_reward_currency` references non-existing player choice reward for ChoiceId %d, ResponseId: %d, skipped",
                    choiceId, responseId);
                continue;
            }

            if (!sCurrencyTypesStore.LookupEntry(currencyId))
            {
                TC_LOG_ERROR("sql.sql", "Table `playerchoice_response_reward_currency` references non-existing currency %u for ChoiceId %d, ResponseId: %d, skipped",
                    currencyId, choiceId, responseId);
                continue;
            }

            responseItr->Reward->Currency.emplace_back(currencyId, quantity);
            ++currencyRewardCount;

        } while (rewards->NextRow());
    }

    if (QueryResult rewards = WorldDatabase.Query("SELECT ChoiceId, ResponseId, FactionId, Quantity FROM playerchoice_response_reward_faction ORDER BY `Index` ASC"))
    {
        do
        {
            Field* fields = rewards->Fetch();

            int32 choiceId = fields[0].GetInt32();
            int32 responseId = fields[1].GetInt32();
            uint32 factionId = fields[2].GetUInt32();
            int32 quantity = fields[3].GetInt32();

            PlayerChoice* choice = Trinity::Containers::MapGetValuePtr(_playerChoices, choiceId);
            if (!choice)
            {
                TC_LOG_ERROR("sql.sql", "Table `playerchoice_response_reward_faction` references non-existing ChoiceId: %d (ResponseId: %d), skipped", choiceId, responseId);
                continue;
            }

            auto responseItr = std::find_if(choice->Responses.begin(), choice->Responses.end(),
                [responseId](PlayerChoiceResponse const& playerChoiceResponse) { return playerChoiceResponse.ResponseId == responseId; });
            if (responseItr == choice->Responses.end())
            {
                TC_LOG_ERROR("sql.sql", "Table `playerchoice_response_reward_faction` references non-existing ResponseId: %d for ChoiceId %d, skipped", responseId, choiceId);
                continue;
            }

            if (!responseItr->Reward)
            {
                TC_LOG_ERROR("sql.sql", "Table `playerchoice_response_reward_faction` references non-existing player choice reward for ChoiceId %d, ResponseId: %d, skipped",
                    choiceId, responseId);
                continue;
            }

            if (!sFactionStore.LookupEntry(factionId))
            {
                TC_LOG_ERROR("sql.sql", "Table `playerchoice_response_reward_faction` references non-existing faction %u for ChoiceId %d, ResponseId: %d, skipped",
                    factionId, choiceId, responseId);
                continue;
            }

            responseItr->Reward->Faction.emplace_back(factionId, quantity);
            ++factionRewardCount;

        } while (rewards->NextRow());
    }

    if (QueryResult rewards = WorldDatabase.Query("SELECT ChoiceId, ResponseId, ItemId, BonusListIDs, Quantity FROM playerchoice_response_reward_item_choice ORDER BY `Index` ASC"))
    {
        do
        {
            Field* fields = rewards->Fetch();

            int32 choiceId = fields[0].GetInt32();
            int32 responseId = fields[1].GetInt32();
            uint32 itemId = fields[2].GetUInt32();
            Tokenizer bonusListIDsTok(fields[3].GetString(), ' ');
            std::vector<int32> bonusListIds;
            for (char const* token : bonusListIDsTok)
                bonusListIds.push_back(int32(atol(token)));
            int32 quantity = fields[4].GetInt32();

            PlayerChoice* choice = Trinity::Containers::MapGetValuePtr(_playerChoices, choiceId);
            if (!choice)
            {
                TC_LOG_ERROR("sql.sql", "Table `playerchoice_response_reward_item_choice` references non-existing ChoiceId: %d (ResponseId: %d), skipped", choiceId, responseId);
                continue;
            }

            auto responseItr = std::find_if(choice->Responses.begin(), choice->Responses.end(),
                [responseId](PlayerChoiceResponse const& playerChoiceResponse) { return playerChoiceResponse.ResponseId == responseId; });
            if (responseItr == choice->Responses.end())
            {
                TC_LOG_ERROR("sql.sql", "Table `playerchoice_response_reward_item_choice` references non-existing ResponseId: %d for ChoiceId %d, skipped", responseId, choiceId);
                continue;
            }

            if (!responseItr->Reward)
            {
                TC_LOG_ERROR("sql.sql", "Table `playerchoice_response_reward_item_choice` references non-existing player choice reward for ChoiceId %d, ResponseId: %d, skipped",
                    choiceId, responseId);
                continue;
            }

            if (!GetItemTemplate(itemId))
            {
                TC_LOG_ERROR("sql.sql", "Table `playerchoice_response_reward_item_choice` references non-existing item %u for ChoiceId %d, ResponseId: %d, skipped",
                    itemId, choiceId, responseId);
                continue;
            }

            responseItr->Reward->ItemChoices.emplace_back(itemId, std::move(bonusListIds), quantity);
            ++itemChoiceRewardCount;

        } while (rewards->NextRow());
    }

    if (QueryResult mawPowersResult = WorldDatabase.Query("SELECT ChoiceId, ResponseId, TypeArtFileID, Rarity, RarityColor, SpellID, MaxStacks FROM playerchoice_response_maw_power"))
    {
        do
        {
            Field* fields = mawPowersResult->Fetch();
            int32 choiceId = fields[0].GetInt32();
            int32 responseId = fields[1].GetInt32();

            PlayerChoice* choice = Trinity::Containers::MapGetValuePtr(_playerChoices, choiceId);
            if (!choice)
            {
                TC_LOG_ERROR("sql.sql", "Table `playerchoice_response_maw_power` references non-existing ChoiceId: %d (ResponseId: %d), skipped", choiceId, responseId);
                continue;
            }

            auto responseItr = std::find_if(choice->Responses.begin(), choice->Responses.end(),
                [responseId](PlayerChoiceResponse const& playerChoiceResponse)
            {
                return playerChoiceResponse.ResponseId == responseId;
            });
            if (responseItr == choice->Responses.end())
            {
                TC_LOG_ERROR("sql.sql", "Table `playerchoice_response_maw_power` references non-existing ResponseId: %d for ChoiceId %d, skipped", responseId, choiceId);
                continue;
            }

            responseItr->MawPower.emplace();
            PlayerChoiceResponseMawPower& mawPower = responseItr->MawPower.get();
            mawPower.TypeArtFileID = fields[2].GetInt32();
            mawPower.Rarity = fields[3].GetInt32();
            mawPower.RarityColor = fields[4].GetUInt32();
            mawPower.SpellID = fields[5].GetInt32();
            mawPower.MaxStacks = fields[6].GetInt32();

            ++mawPowersCount;

        } while (mawPowersResult->NextRow());
    }

    TC_LOG_INFO("server.loading", ">> Loaded " SZFMTD " player choices, %u responses, %u rewards, %u item rewards, %u currency rewards, %u faction rewards, %u item choice rewards and %u maw powers in %u ms.",
        _playerChoices.size(), responseCount, rewardCount, itemRewardCount, currencyRewardCount, factionRewardCount, itemChoiceRewardCount, mawPowersCount, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadPlayerChoicesLocale()
{
    uint32 oldMSTime = getMSTime();

    // need for reload case
    _playerChoiceLocales.clear();

    //                                                   0         1       2
    if (QueryResult result = WorldDatabase.Query("SELECT ChoiceId, locale, Question FROM playerchoice_locale"))
    {
        do
        {
            Field* fields = result->Fetch();

            uint32 choiceId         = fields[0].GetUInt32();
            std::string localeName  = fields[1].GetString();

            if (!GetPlayerChoice(choiceId))
            {
                TC_LOG_ERROR("sql.sql", "Table `playerchoice_locale` references non-existing ChoiceId: %d for locale %s, skipped", choiceId, localeName.c_str());
                continue;
            }

            LocaleConstant locale = GetLocaleByName(localeName);
            if (!IsValidLocale(locale) || locale == LOCALE_enUS)
                continue;

            PlayerChoiceLocale& data = _playerChoiceLocales[choiceId];
            AddLocaleString(fields[2].GetString(), locale, data.Question);
        } while (result->NextRow());

        TC_LOG_INFO("server.loading", ">> Loaded " SZFMTD " Player Choice locale strings in %u ms", _playerChoiceLocales.size(), GetMSTimeDiffToNow(oldMSTime));
    }

    oldMSTime = getMSTime();

    //                                                   0         1           2       3       4       5          6               7            8
    if (QueryResult result = WorldDatabase.Query("SELECT ChoiceID, ResponseID, locale, Answer, Header, SubHeader, ButtonTooltip, Description, Confirmation FROM playerchoice_response_locale"))
    {
        std::size_t count = 0;
        do
        {
            Field* fields = result->Fetch();

            int32 choiceId         = fields[0].GetInt32();
            int32 responseId       = fields[1].GetInt32();
            std::string localeName = fields[2].GetString();

            auto itr = _playerChoiceLocales.find(choiceId);
            if (itr == _playerChoiceLocales.end())
            {
                TC_LOG_ERROR("sql.sql", "Table `playerchoice_locale` references non-existing ChoiceId: %d for ResponseId %d locale %s, skipped",
                    choiceId, responseId, localeName.c_str());
                continue;
            }

            PlayerChoice const* playerChoice = ASSERT_NOTNULL(GetPlayerChoice(choiceId));
            if (!playerChoice->GetResponse(responseId))
            {
                TC_LOG_ERROR("sql.sql", "Table `playerchoice_locale` references non-existing ResponseId: %d for ChoiceId %d locale %s, skipped",
                    responseId, choiceId, localeName.c_str());
                continue;
            }

            LocaleConstant locale = GetLocaleByName(localeName);
            if (!IsValidLocale(locale) || locale == LOCALE_enUS)
                continue;

            PlayerChoiceResponseLocale& data = itr->second.Responses[responseId];
            AddLocaleString(fields[3].GetString(), locale, data.Answer);
            AddLocaleString(fields[4].GetString(), locale, data.Header);
            AddLocaleString(fields[5].GetString(), locale, data.SubHeader);
            AddLocaleString(fields[6].GetString(), locale, data.ButtonTooltip);
            AddLocaleString(fields[7].GetString(), locale, data.Description);
            AddLocaleString(fields[8].GetString(), locale, data.Confirmation);
            ++count;
        } while (result->NextRow());

        TC_LOG_INFO("server.loading", ">> Loaded " SZFMTD " Player Choice Response locale strings in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
    }
}
