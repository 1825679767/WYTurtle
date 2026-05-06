#include "TurtleLuaEngine.h"

#include "AccountMgr.h"
#include "BattleGround.h"
#include "Chat.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "AI/CreatureAI.h"
#include "Config/Config.h"
#include "Database/DatabaseEnv.h"
#include "Database/SqlOperations.h"
#include "Database/SQLStorages.h"
#include "GossipDef.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "GMTicketMgr.h"
#include "GameEventMgr.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "InstanceData.h"
#include "Log.h"
#include "Mail/Mail.h"
#include "Map.h"
#include "MapManager.h"
#include "MapNodes/MasterPlayer.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Objects/Creature.h"
#include "Objects/Corpse.h"
#include "Objects/DynamicObject.h"
#include "Objects/Bag.h"
#include "Objects/GameObject.h"
#include "Objects/Item.h"
#include "Objects/Player.h"
#include "Objects/Unit.h"
#include "Database/DBCStores.h"
#include "Opcodes.h"
#include "Spell.h"
#include "Spells/SpellAuras.h"
#include "SpellMgr.h"
#include "SystemConfig.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Util.h"
#include "httplib.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <filesystem>
#include <list>
#include <limits>
#include <memory>
#include <new>
#include <shared_mutex>
#include <sstream>
#include <thread>
#include <type_traits>
#include <utility>

extern "C"
{
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

namespace
{
constexpr char const* PLAYER_METATABLE = "Turtle.Player";
constexpr char const* CREATURE_METATABLE = "Turtle.Creature";
constexpr char const* GAMEOBJECT_METATABLE = "Turtle.GameObject";
constexpr char const* CORPSE_METATABLE = "Turtle.Corpse";
constexpr char const* DYNAMICOBJECT_METATABLE = "Turtle.DynamicObject";
constexpr char const* GAMEOBJECTTEMPLATE_METATABLE = "Turtle.GameObjectTemplate";
constexpr char const* ITEM_METATABLE = "Turtle.Item";
constexpr char const* ITEMTEMPLATE_METATABLE = "Turtle.ItemTemplate";
constexpr char const* CREATURETEMPLATE_METATABLE = "Turtle.CreatureTemplate";
constexpr char const* QUEST_METATABLE = "Turtle.Quest";
constexpr char const* GROUP_METATABLE = "Turtle.Group";
constexpr char const* GUILD_METATABLE = "Turtle.Guild";
constexpr char const* MAP_METATABLE = "Turtle.Map";
constexpr char const* INSTANCEDATA_METATABLE = "Turtle.InstanceData";
constexpr char const* BATTLEGROUND_METATABLE = "Turtle.BattleGround";
constexpr char const* TICKET_METATABLE = "Turtle.Ticket";
constexpr char const* AURA_METATABLE = "Turtle.Aura";
constexpr char const* SPELL_METATABLE = "Turtle.Spell";
constexpr char const* SPELLINFO_METATABLE = "Turtle.SpellInfo";
constexpr char const* SPELLTARGETS_METATABLE = "Turtle.SpellCastTargets";
constexpr char const* WORLDPACKET_METATABLE = "Turtle.WorldPacket";
constexpr char const* ELUNAQUERY_METATABLE = "Turtle.ElunaQuery";
constexpr char const* OBJECTGUID_METATABLE = "Turtle.ObjectGuid";
constexpr char const* CHATHANDLER_METATABLE = "Turtle.ChatHandler";
constexpr char const* CHANNEL_METATABLE = "Turtle.Channel";
constexpr char const* ROLL_METATABLE = "Turtle.Roll";

uint32 ToElunaChatType(uint32 type)
{
    switch (ChatMsg(type))
    {
        case CHAT_MSG_SYSTEM: return 0x00;
        case CHAT_MSG_SAY: return 0x01;
        case CHAT_MSG_PARTY: return 0x02;
        case CHAT_MSG_RAID: return 0x03;
        case CHAT_MSG_GUILD: return 0x04;
        case CHAT_MSG_OFFICER: return 0x05;
        case CHAT_MSG_YELL: return 0x06;
        case CHAT_MSG_WHISPER: return 0x07;
        case CHAT_MSG_WHISPER_INFORM: return 0x09;
        case CHAT_MSG_EMOTE: return 0x0A;
        case CHAT_MSG_TEXT_EMOTE: return 0x0B;
        case CHAT_MSG_MONSTER_SAY: return 0x0C;
        case CHAT_MSG_MONSTER_YELL: return 0x0E;
        case CHAT_MSG_MONSTER_WHISPER: return 0x0F;
        case CHAT_MSG_MONSTER_EMOTE: return 0x10;
        case CHAT_MSG_CHANNEL: return 0x11;
        case CHAT_MSG_CHANNEL_JOIN: return 0x12;
        case CHAT_MSG_CHANNEL_LEAVE: return 0x13;
        case CHAT_MSG_CHANNEL_LIST: return 0x14;
        case CHAT_MSG_CHANNEL_NOTICE: return 0x15;
        case CHAT_MSG_CHANNEL_NOTICE_USER: return 0x16;
        case CHAT_MSG_AFK: return 0x17;
        case CHAT_MSG_DND: return 0x18;
        case CHAT_MSG_IGNORED: return 0x19;
        case CHAT_MSG_SKILL: return 0x1A;
        case CHAT_MSG_LOOT: return 0x1B;
        default: return type;
    }
}

struct LuaPlayer
{
    Player* player;
};

struct LuaCreature
{
    Creature* creature;
};

struct LuaGameObject
{
    GameObject* go;
};

struct LuaCorpse
{
    Corpse* corpse;
};

struct LuaDynamicObject
{
    DynamicObject* dynObj;
};

struct LuaGameObjectTemplate
{
    GameObjectInfo const* info;
};

struct LuaItem
{
    Item* item;
};

struct LuaItemTemplate
{
    ItemPrototype const* proto;
};

struct LuaCreatureTemplate
{
    CreatureInfo const* info;
};

struct LuaQuest
{
    Quest const* quest;
};

struct LuaGroup
{
    Group* group;
};

struct LuaGuild
{
    Guild* guild;
};

struct LuaMap
{
    Map* map;
};

struct LuaInstanceData
{
    InstanceData* data;
};

struct LuaBattleGround
{
    BattleGround* bg;
};

struct LuaTicket
{
    GmTicket* ticket;
};

struct LuaAura
{
    Aura* aura;
};

struct LuaSpell
{
    Spell* spell;
};

struct LuaSpellInfo
{
    SpellEntry const* spellInfo;
};

struct LuaSpellTargets
{
    SpellCastTargets const* targets;
};

struct LuaWorldPacket
{
    WorldPacket* packet;
    bool owned;
};

struct LuaQuery
{
    QueryNamedResult* result;
    bool hasRow;
};

struct LuaObjectGuid
{
    ObjectGuid guid;
};

struct LuaChatHandler
{
    ObjectGuid playerGuid;
};

struct LuaChannel
{
    std::string name;
    Team team = ALLIANCE;
    uint32 channelId = 0;
};

struct LuaRoll
{
    ObjectGuid itemGuid;
    ObjectGuid lootedTargetGuid;
    uint32 itemId = 0;
    int32 itemRandomPropId = 0;
    uint32 itemRandomSuffix = 0;
    uint32 itemCount = 0;
    uint32 totalPlayersRolling = 0;
    uint32 totalNeed = 0;
    uint32 totalGreed = 0;
    uint32 totalPass = 0;
    uint32 itemSlot = 0;
    uint32 rollVoteMask = 0;
    std::vector<std::pair<ObjectGuid, uint32>> playerVotes;
};

class LuaChatHandlerAccess : public ChatHandler
{
public:
    explicit LuaChatHandlerAccess(Player* player) : ChatHandler(player) {}

    using ChatHandler::GetSelectedCreature;
    using ChatHandler::GetSelectedPlayer;
    using ChatHandler::GetSelectedUnit;
};

std::vector<std::unique_ptr<TaxiPathEntry>> g_luaTaxiPaths;
std::vector<std::unique_ptr<TaxiPathNodeEntry>> g_luaTaxiPathNodes;

void UnrefFunctionRefs(lua_State* state, std::vector<int>& refs)
{
    if (state)
        for (int functionRef : refs)
            luaL_unref(state, LUA_REGISTRYINDEX, functionRef);

    refs.clear();
}

void ClearEventStore(lua_State* state, std::map<uint32, std::vector<int>>& store, uint32 eventId, bool allEvents)
{
    if (allEvents)
    {
        for (auto& eventPair : store)
            UnrefFunctionRefs(state, eventPair.second);

        store.clear();
        return;
    }

    auto eventItr = store.find(eventId);
    if (eventItr == store.end())
        return;

    UnrefFunctionRefs(state, eventItr->second);
    store.erase(eventItr);
}

void ClearEntryEventStore(lua_State* state, std::map<uint32, std::map<uint32, std::vector<int>>>& store, uint32 entry, uint32 eventId, bool allEvents)
{
    auto entryItr = store.find(entry);
    if (entryItr == store.end())
        return;

    if (allEvents)
    {
        for (auto& eventPair : entryItr->second)
            UnrefFunctionRefs(state, eventPair.second);

        store.erase(entryItr);
        return;
    }

    auto eventItr = entryItr->second.find(eventId);
    if (eventItr == entryItr->second.end())
        return;

    UnrefFunctionRefs(state, eventItr->second);
    entryItr->second.erase(eventItr);

    if (entryItr->second.empty())
        store.erase(entryItr);
}

TurtleLuaEngine* GetEngine(lua_State* state)
{
    lua_getfield(state, LUA_REGISTRYINDEX, "TurtleLuaEngine");
    auto* engine = static_cast<TurtleLuaEngine*>(lua_touserdata(state, -1));
    lua_pop(state, 1);
    return engine;
}

Player* CheckPlayer(lua_State* state, int index)
{
    auto* holder = static_cast<LuaPlayer*>(luaL_checkudata(state, index, PLAYER_METATABLE));
    return holder ? holder->player : nullptr;
}

Creature* CheckCreature(lua_State* state, int index)
{
    auto* holder = static_cast<LuaCreature*>(luaL_checkudata(state, index, CREATURE_METATABLE));
    return holder ? holder->creature : nullptr;
}

GameObject* CheckGameObject(lua_State* state, int index)
{
    auto* holder = static_cast<LuaGameObject*>(luaL_checkudata(state, index, GAMEOBJECT_METATABLE));
    return holder ? holder->go : nullptr;
}

Corpse* CheckCorpse(lua_State* state, int index)
{
    auto* holder = static_cast<LuaCorpse*>(luaL_checkudata(state, index, CORPSE_METATABLE));
    return holder ? holder->corpse : nullptr;
}

DynamicObject* CheckDynamicObject(lua_State* state, int index)
{
    auto* holder = static_cast<LuaDynamicObject*>(luaL_checkudata(state, index, DYNAMICOBJECT_METATABLE));
    return holder ? holder->dynObj : nullptr;
}

GameObjectInfo const* CheckGameObjectTemplate(lua_State* state, int index)
{
    auto* holder = static_cast<LuaGameObjectTemplate*>(luaL_checkudata(state, index, GAMEOBJECTTEMPLATE_METATABLE));
    return holder ? holder->info : nullptr;
}

Item* CheckItem(lua_State* state, int index)
{
    auto* holder = static_cast<LuaItem*>(luaL_checkudata(state, index, ITEM_METATABLE));
    return holder ? holder->item : nullptr;
}

ItemPrototype const* CheckItemTemplate(lua_State* state, int index)
{
    auto* holder = static_cast<LuaItemTemplate*>(luaL_checkudata(state, index, ITEMTEMPLATE_METATABLE));
    return holder ? holder->proto : nullptr;
}

CreatureInfo const* CheckCreatureTemplate(lua_State* state, int index)
{
    auto* holder = static_cast<LuaCreatureTemplate*>(luaL_checkudata(state, index, CREATURETEMPLATE_METATABLE));
    return holder ? holder->info : nullptr;
}

Quest const* CheckQuest(lua_State* state, int index)
{
    auto* holder = static_cast<LuaQuest*>(luaL_checkudata(state, index, QUEST_METATABLE));
    return holder ? holder->quest : nullptr;
}

uint32 CheckQuestIdValue(lua_State* state, int index)
{
    if (luaL_testudata(state, index, QUEST_METATABLE))
    {
        Quest const* quest = CheckQuest(state, index);
        return quest ? quest->GetQuestId() : 0;
    }

    return static_cast<uint32>(luaL_checkinteger(state, index));
}

Quest const* CheckQuestTemplateValue(lua_State* state, int index)
{
    if (luaL_testudata(state, index, QUEST_METATABLE))
        return CheckQuest(state, index);

    return sObjectMgr.GetQuestTemplate(static_cast<uint32>(luaL_checkinteger(state, index)));
}

Group* CheckGroup(lua_State* state, int index)
{
    auto* holder = static_cast<LuaGroup*>(luaL_checkudata(state, index, GROUP_METATABLE));
    return holder ? holder->group : nullptr;
}

Guild* CheckGuild(lua_State* state, int index)
{
    auto* holder = static_cast<LuaGuild*>(luaL_checkudata(state, index, GUILD_METATABLE));
    return holder ? holder->guild : nullptr;
}

Map* CheckMap(lua_State* state, int index)
{
    auto* holder = static_cast<LuaMap*>(luaL_checkudata(state, index, MAP_METATABLE));
    return holder ? holder->map : nullptr;
}

InstanceData* CheckInstanceData(lua_State* state, int index)
{
    auto* holder = static_cast<LuaInstanceData*>(luaL_checkudata(state, index, INSTANCEDATA_METATABLE));
    return holder ? holder->data : nullptr;
}

BattleGround* CheckBattleGround(lua_State* state, int index)
{
    auto* holder = static_cast<LuaBattleGround*>(luaL_checkudata(state, index, BATTLEGROUND_METATABLE));
    return holder ? holder->bg : nullptr;
}

GmTicket* CheckTicket(lua_State* state, int index)
{
    auto* holder = static_cast<LuaTicket*>(luaL_checkudata(state, index, TICKET_METATABLE));
    return holder ? holder->ticket : nullptr;
}

Aura* CheckAura(lua_State* state, int index)
{
    auto* holder = static_cast<LuaAura*>(luaL_checkudata(state, index, AURA_METATABLE));
    return holder ? holder->aura : nullptr;
}

Spell* CheckSpell(lua_State* state, int index)
{
    auto* holder = static_cast<LuaSpell*>(luaL_checkudata(state, index, SPELL_METATABLE));
    return holder ? holder->spell : nullptr;
}

SpellEntry const* CheckSpellInfo(lua_State* state, int index)
{
    auto* holder = static_cast<LuaSpellInfo*>(luaL_checkudata(state, index, SPELLINFO_METATABLE));
    return holder ? holder->spellInfo : nullptr;
}

SpellCastTargets const* CheckSpellTargets(lua_State* state, int index)
{
    auto* holder = static_cast<LuaSpellTargets*>(luaL_checkudata(state, index, SPELLTARGETS_METATABLE));
    return holder ? holder->targets : nullptr;
}

WorldPacket* CheckWorldPacket(lua_State* state, int index)
{
    auto* holder = static_cast<LuaWorldPacket*>(luaL_checkudata(state, index, WORLDPACKET_METATABLE));
    return holder ? holder->packet : nullptr;
}

WorldPacket* RequireWorldPacket(lua_State* state, int index)
{
    WorldPacket* packet = CheckWorldPacket(state, index);
    if (!packet)
        luaL_error(state, "valid WorldPacket expected");
    return packet;
}

LuaQuery* CheckQuery(lua_State* state, int index)
{
    return static_cast<LuaQuery*>(luaL_checkudata(state, index, ELUNAQUERY_METATABLE));
}

ObjectGuid const* CheckObjectGuid(lua_State* state, int index)
{
    auto* holder = static_cast<LuaObjectGuid*>(luaL_checkudata(state, index, OBJECTGUID_METATABLE));
    return holder ? &holder->guid : nullptr;
}

LuaChatHandler* CheckChatHandler(lua_State* state, int index)
{
    return static_cast<LuaChatHandler*>(luaL_checkudata(state, index, CHATHANDLER_METATABLE));
}

LuaChannel* CheckChannel(lua_State* state, int index)
{
    return static_cast<LuaChannel*>(luaL_checkudata(state, index, CHANNEL_METATABLE));
}

Channel* ResolveChannel(LuaChannel const* holder)
{
    if (!holder)
        return nullptr;

    if (ChannelMgr* mgr = channelMgr(holder->team))
        return mgr->GetChannel(holder->name, nullptr, false);

    return nullptr;
}

Player* GetChatHandlerPlayer(LuaChatHandler const* holder)
{
    if (!holder || holder->playerGuid.IsEmpty())
        return nullptr;

    Player* player = ObjectAccessor::FindPlayer(holder->playerGuid);
    return player && player->GetSession() ? player : nullptr;
}

void PushChatHandlerValue(lua_State* state, Player* player)
{
    auto* holder = static_cast<LuaChatHandler*>(lua_newuserdata(state, sizeof(LuaChatHandler)));
    holder->playerGuid = player ? player->GetObjectGuid() : ObjectGuid();

    luaL_getmetatable(state, CHATHANDLER_METATABLE);
    lua_setmetatable(state, -2);
}

void PushChannelValue(lua_State* state, Channel* channel)
{
    if (!channel)
    {
        lua_pushnil(state);
        return;
    }

    auto* holder = static_cast<LuaChannel*>(lua_newuserdata(state, sizeof(LuaChannel)));
    new (holder) LuaChannel();
    holder->name = channel->GetName();
    holder->team = channel->GetTeam();
    holder->channelId = channel->GetChannelId();

    luaL_getmetatable(state, CHANNEL_METATABLE);
    lua_setmetatable(state, -2);
}

LuaRoll* CheckRoll(lua_State* state, int index)
{
    return static_cast<LuaRoll*>(luaL_checkudata(state, index, ROLL_METATABLE));
}

void PushRollValue(lua_State* state, Roll const* roll, Item* item, uint32 count)
{
    if (!roll)
    {
        lua_pushnil(state);
        return;
    }

    auto* holder = static_cast<LuaRoll*>(lua_newuserdata(state, sizeof(LuaRoll)));
    new (holder) LuaRoll();

    holder->itemGuid = item ? item->GetObjectGuid() : ObjectGuid();
    holder->lootedTargetGuid = roll->lootedTargetGUID;
    holder->itemId = roll->itemid;
    holder->itemRandomPropId = roll->itemRandomPropId;
    holder->itemCount = count;
    holder->totalPlayersRolling = roll->totalPlayersRolling;
    holder->totalNeed = roll->totalNeed;
    holder->totalGreed = roll->totalGreed;
    holder->totalPass = roll->totalPass;
    holder->itemSlot = roll->itemSlot;
    holder->rollVoteMask = 0x01 | 0x02 | 0x04;

    holder->playerVotes.reserve(roll->playerVote.size());
    for (auto const& vote : roll->playerVote)
        holder->playerVotes.emplace_back(vote.first, static_cast<uint32>(vote.second));

    luaL_getmetatable(state, ROLL_METATABLE);
    lua_setmetatable(state, -2);
}

void PushInstanceDataValue(lua_State* state, InstanceData* data)
{
    if (!data)
    {
        lua_pushnil(state);
        return;
    }

    auto* holder = static_cast<LuaInstanceData*>(lua_newuserdata(state, sizeof(LuaInstanceData)));
    holder->data = data;

    luaL_getmetatable(state, INSTANCEDATA_METATABLE);
    lua_setmetatable(state, -2);
}

void PushBattleGroundValue(lua_State* state, BattleGround* bg)
{
    if (!bg)
    {
        lua_pushnil(state);
        return;
    }

    auto* holder = static_cast<LuaBattleGround*>(lua_newuserdata(state, sizeof(LuaBattleGround)));
    holder->bg = bg;

    luaL_getmetatable(state, BATTLEGROUND_METATABLE);
    lua_setmetatable(state, -2);
}

void PushTicketValue(lua_State* state, GmTicket* ticket)
{
    if (!ticket)
    {
        lua_pushnil(state);
        return;
    }

    auto* holder = static_cast<LuaTicket*>(lua_newuserdata(state, sizeof(LuaTicket)));
    holder->ticket = ticket;

    luaL_getmetatable(state, TICKET_METATABLE);
    lua_setmetatable(state, -2);
}

void PushWorldObjectValue(lua_State* state, TurtleLuaEngine* engine, WorldObject* object)
{
    if (!engine || !object)
    {
        lua_pushnil(state);
        return;
    }

    if (Unit* unit = dynamic_cast<Unit*>(object))
        engine->PushUnit(unit);
    else if (GameObject* go = dynamic_cast<GameObject*>(object))
        engine->PushGameObject(go);
    else if (DynamicObject* dynObj = dynamic_cast<DynamicObject*>(object))
        engine->PushDynamicObject(dynObj);
    else if (Corpse* corpse = dynamic_cast<Corpse*>(object))
        engine->PushCorpse(corpse);
    else
        lua_pushnil(state);
}

void PushItemTemplateValue(lua_State* state, ItemPrototype const* proto)
{
    if (!proto)
    {
        lua_pushnil(state);
        return;
    }

    auto* holder = static_cast<LuaItemTemplate*>(lua_newuserdata(state, sizeof(LuaItemTemplate)));
    holder->proto = proto;

    luaL_getmetatable(state, ITEMTEMPLATE_METATABLE);
    lua_setmetatable(state, -2);
}

void PushGameObjectTemplateValue(lua_State* state, GameObjectInfo const* info)
{
    if (!info)
    {
        lua_pushnil(state);
        return;
    }

    auto* holder = static_cast<LuaGameObjectTemplate*>(lua_newuserdata(state, sizeof(LuaGameObjectTemplate)));
    holder->info = info;

    luaL_getmetatable(state, GAMEOBJECTTEMPLATE_METATABLE);
    lua_setmetatable(state, -2);
}

void PushCreatureTemplateValue(lua_State* state, CreatureInfo const* info)
{
    if (!info)
    {
        lua_pushnil(state);
        return;
    }

    auto* holder = static_cast<LuaCreatureTemplate*>(lua_newuserdata(state, sizeof(LuaCreatureTemplate)));
    holder->info = info;

    luaL_getmetatable(state, CREATURETEMPLATE_METATABLE);
    lua_setmetatable(state, -2);
}

void PushQuest(lua_State* state, Quest const* quest)
{
    if (!quest)
    {
        lua_pushnil(state);
        return;
    }

    auto* holder = static_cast<LuaQuest*>(lua_newuserdata(state, sizeof(LuaQuest)));
    holder->quest = quest;

    luaL_getmetatable(state, QUEST_METATABLE);
    lua_setmetatable(state, -2);
}

Aura* GetFirstAuraFromHolder(SpellAuraHolder* holder)
{
    if (!holder)
        return nullptr;

    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (Aura* aura = holder->GetAuraByEffectIndex(SpellEffectIndex(i)))
            return aura;

    return nullptr;
}

void PushAuraValue(lua_State* state, Aura* aura)
{
    if (!aura)
    {
        lua_pushnil(state);
        return;
    }

    auto* holder = static_cast<LuaAura*>(lua_newuserdata(state, sizeof(LuaAura)));
    holder->aura = aura;

    luaL_getmetatable(state, AURA_METATABLE);
    lua_setmetatable(state, -2);
}

void PushSpellInfo(lua_State* state, SpellEntry const* spellInfo)
{
    if (!spellInfo)
    {
        lua_pushnil(state);
        return;
    }

    auto* holder = static_cast<LuaSpellInfo*>(lua_newuserdata(state, sizeof(LuaSpellInfo)));
    holder->spellInfo = spellInfo;

    luaL_getmetatable(state, SPELLINFO_METATABLE);
    lua_setmetatable(state, -2);
}

void PushSpellTargets(lua_State* state, SpellCastTargets const* targets)
{
    if (!targets)
    {
        lua_pushnil(state);
        return;
    }

    auto* holder = static_cast<LuaSpellTargets*>(lua_newuserdata(state, sizeof(LuaSpellTargets)));
    holder->targets = targets;

    luaL_getmetatable(state, SPELLTARGETS_METATABLE);
    lua_setmetatable(state, -2);
}

void PushWorldPacketValue(lua_State* state, WorldPacket* packet, bool owned)
{
    if (!packet)
    {
        lua_pushnil(state);
        return;
    }

    auto* holder = static_cast<LuaWorldPacket*>(lua_newuserdata(state, sizeof(LuaWorldPacket)));
    holder->packet = packet;
    holder->owned = owned;

    luaL_getmetatable(state, WORLDPACKET_METATABLE);
    lua_setmetatable(state, -2);
}

void PushObjectGuidValue(lua_State* state, ObjectGuid const& guid)
{
    auto* holder = static_cast<LuaObjectGuid*>(lua_newuserdata(state, sizeof(LuaObjectGuid)));
    new (&holder->guid) ObjectGuid(guid);

    luaL_getmetatable(state, OBJECTGUID_METATABLE);
    lua_setmetatable(state, -2);
}

Unit* CheckUnit(lua_State* state, int index)
{
    if (luaL_testudata(state, index, PLAYER_METATABLE))
        return CheckPlayer(state, index);

    if (luaL_testudata(state, index, CREATURE_METATABLE))
        return CheckCreature(state, index);

    return nullptr;
}

Object* CheckObject(lua_State* state, int index)
{
    if (luaL_testudata(state, index, PLAYER_METATABLE))
        return CheckPlayer(state, index);

    if (luaL_testudata(state, index, CREATURE_METATABLE))
        return CheckCreature(state, index);

    if (luaL_testudata(state, index, GAMEOBJECT_METATABLE))
        return CheckGameObject(state, index);

    if (luaL_testudata(state, index, CORPSE_METATABLE))
        return CheckCorpse(state, index);

    if (luaL_testudata(state, index, DYNAMICOBJECT_METATABLE))
        return CheckDynamicObject(state, index);

    if (luaL_testudata(state, index, ITEM_METATABLE))
        return CheckItem(state, index);

    return nullptr;
}

ObjectGuid CheckObjectGuidValue(lua_State* state, int index)
{
    if (lua_isnoneornil(state, index))
        return ObjectGuid();

    if (luaL_testudata(state, index, OBJECTGUID_METATABLE))
    {
        ObjectGuid const* guid = CheckObjectGuid(state, index);
        return guid ? *guid : ObjectGuid();
    }

    if (Object* object = CheckObject(state, index))
        return object->GetObjectGuid();

    uint32 lowGuid = static_cast<uint32>(luaL_checkinteger(state, index));
    return ObjectGuid(HIGHGUID_PLAYER, lowGuid);
}

WorldObject* CheckWorldObject(lua_State* state, int index)
{
    if (luaL_testudata(state, index, PLAYER_METATABLE))
        return CheckPlayer(state, index);

    if (luaL_testudata(state, index, CREATURE_METATABLE))
        return CheckCreature(state, index);

    if (luaL_testudata(state, index, GAMEOBJECT_METATABLE))
        return CheckGameObject(state, index);

    if (luaL_testudata(state, index, CORPSE_METATABLE))
        return CheckCorpse(state, index);

    if (luaL_testudata(state, index, DYNAMICOBJECT_METATABLE))
        return CheckDynamicObject(state, index);

    return nullptr;
}

bool IsWorldObjectValue(lua_State* state, int index)
{
    return luaL_testudata(state, index, PLAYER_METATABLE)
        || luaL_testudata(state, index, CREATURE_METATABLE)
        || luaL_testudata(state, index, GAMEOBJECT_METATABLE)
        || luaL_testudata(state, index, CORPSE_METATABLE);
}

Player* CheckOptionalPlayer(lua_State* state, int index)
{
    if (lua_isnoneornil(state, index) || !luaL_testudata(state, index, PLAYER_METATABLE))
        return nullptr;

    return CheckPlayer(state, index);
}

bool MatchesHostility(WorldObject* source, WorldObject* target, uint32 hostile)
{
    if (!source || !target || hostile == 0)
        return true;

    if (hostile == 1)
        return source->IsHostileTo(target);

    if (hostile == 2)
        return source->IsFriendlyTo(target);

    return false;
}

bool MatchesDeadState(Unit* unit, uint32 dead)
{
    if (!unit || dead == 0)
        return true;

    if (dead == 1)
        return unit->IsAlive();

    if (dead == 2)
        return !unit->IsAlive();

    return false;
}

void GetPlayersInRange(WorldObject* object, float range, uint32 hostile, uint32 dead, std::list<Player*>& players)
{
    if (!object || !object->IsInWorld())
        return;

    MaNGOS::AnyPlayerInObjectRangeCheck check(object, range, true, false);
    MaNGOS::PlayerListSearcher<MaNGOS::AnyPlayerInObjectRangeCheck> searcher(players, check);
    Cell::VisitWorldObjects(object, searcher, range);

    for (auto itr = players.begin(); itr != players.end();)
    {
        Player* player = *itr;
        if (!MatchesDeadState(player, dead) || !MatchesHostility(object, player, hostile))
            itr = players.erase(itr);
        else
            ++itr;
    }
}

void GetCreaturesInRange(WorldObject* object, float range, uint32 entry, uint32 hostile, uint32 dead, std::list<Creature*>& creatures)
{
    if (!object || !object->IsInWorld())
        return;

    CellPair pair(MaNGOS::ComputeCellPair(object->GetPositionX(), object->GetPositionY()));
    Cell cell(pair);
    cell.SetNoCreate();

    if (entry)
    {
        MaNGOS::AllCreaturesOfEntryInRange check(object, entry, range);
        MaNGOS::CreatureListSearcher<MaNGOS::AllCreaturesOfEntryInRange> searcher(creatures, check);
        TypeContainerVisitor<MaNGOS::CreatureListSearcher<MaNGOS::AllCreaturesOfEntryInRange>, GridTypeMapContainer> visitor(searcher);
        cell.Visit(pair, visitor, *object->GetMap(), *object, range);
    }
    else
    {
        MaNGOS::AllCreaturesInRange check(object, range);
        MaNGOS::CreatureListSearcher<MaNGOS::AllCreaturesInRange> searcher(creatures, check);
        TypeContainerVisitor<MaNGOS::CreatureListSearcher<MaNGOS::AllCreaturesInRange>, GridTypeMapContainer> visitor(searcher);
        cell.Visit(pair, visitor, *object->GetMap(), *object, range);
    }

    for (auto itr = creatures.begin(); itr != creatures.end();)
    {
        Creature* creature = *itr;
        if (!MatchesDeadState(creature, dead) || !MatchesHostility(object, creature, hostile))
            itr = creatures.erase(itr);
        else
            ++itr;
    }
}

void GetGameObjectsInRange(WorldObject* object, float range, uint32 entry, uint32 hostile, std::list<GameObject*>& gameObjects)
{
    if (!object || !object->IsInWorld())
        return;

    CellPair pair(MaNGOS::ComputeCellPair(object->GetPositionX(), object->GetPositionY()));
    Cell cell(pair);
    cell.SetNoCreate();

    if (entry)
    {
        MaNGOS::AllGameObjectsWithEntryInRange check(object, entry, range);
        MaNGOS::GameObjectListSearcher<MaNGOS::AllGameObjectsWithEntryInRange> searcher(gameObjects, check);
        TypeContainerVisitor<MaNGOS::GameObjectListSearcher<MaNGOS::AllGameObjectsWithEntryInRange>, GridTypeMapContainer> visitor(searcher);
        cell.Visit(pair, visitor, *object->GetMap(), *object, range);
    }
    else
    {
        MaNGOS::AllGameObjectsInRange check(object, range);
        MaNGOS::GameObjectListSearcher<MaNGOS::AllGameObjectsInRange> searcher(gameObjects, check);
        TypeContainerVisitor<MaNGOS::GameObjectListSearcher<MaNGOS::AllGameObjectsInRange>, GridTypeMapContainer> visitor(searcher);
        cell.Visit(pair, visitor, *object->GetMap(), *object, range);
    }

    for (auto itr = gameObjects.begin(); itr != gameObjects.end();)
    {
        GameObject* go = *itr;
        if (!MatchesHostility(object, go, hostile))
            itr = gameObjects.erase(itr);
        else
            ++itr;
    }
}

Player* SelectNearestPlayer(WorldObject* object, std::list<Player*> const& players)
{
    Player* nearest = nullptr;
    float nearestDistance = 0.0f;
    for (Player* player : players)
    {
        float distance = object->GetDistance(player);
        if (!nearest || distance < nearestDistance)
        {
            nearest = player;
            nearestDistance = distance;
        }
    }

    return nearest;
}

Creature* SelectNearestCreature(WorldObject* object, std::list<Creature*> const& creatures)
{
    Creature* nearest = nullptr;
    float nearestDistance = 0.0f;
    for (Creature* creature : creatures)
    {
        float distance = object->GetDistance(creature);
        if (!nearest || distance < nearestDistance)
        {
            nearest = creature;
            nearestDistance = distance;
        }
    }

    return nearest;
}

GameObject* SelectNearestGameObject(WorldObject* object, std::list<GameObject*> const& gameObjects)
{
    GameObject* nearest = nullptr;
    float nearestDistance = 0.0f;
    for (GameObject* go : gameObjects)
    {
        float distance = object->GetDistance(go);
        if (!nearest || distance < nearestDistance)
        {
            nearest = go;
            nearestDistance = distance;
        }
    }

    return nearest;
}

uint32 ReadEventShots(lua_State* state, int index)
{
    if (lua_isnoneornil(state, index))
        return 0;

    lua_Integer shots = luaL_checkinteger(state, index);
    if (shots <= 0)
        return 0;

    lua_Integer maxShots = static_cast<lua_Integer>((std::numeric_limits<uint32>::max)());
    return static_cast<uint32>(shots > maxShots ? maxShots : shots);
}

int LuaEventDispatchWrapper(lua_State* state)
{
    int argCount = lua_gettop(state);

    lua_getfield(state, lua_upvalueindex(1), "active");
    bool active = lua_toboolean(state, -1) != 0;
    lua_pop(state, 1);
    if (!active)
        return 0;

    lua_getfield(state, lua_upvalueindex(1), "shots");
    lua_Integer shots = lua_tointeger(state, -1);
    lua_pop(state, 1);

    if (shots > 0)
    {
        if (shots == 1)
        {
            lua_pushboolean(state, false);
            lua_setfield(state, lua_upvalueindex(1), "active");
        }
        else
        {
            lua_pushinteger(state, shots - 1);
            lua_setfield(state, lua_upvalueindex(1), "shots");
        }
    }

    lua_getfield(state, lua_upvalueindex(1), "function");
    if (!lua_isfunction(state, -1))
    {
        lua_pop(state, 1);
        return 0;
    }

    lua_insert(state, 1);
    lua_call(state, argCount, LUA_MULTRET);
    return lua_gettop(state);
}

int LuaEventCancelWrapper(lua_State* state)
{
    lua_pushboolean(state, false);
    lua_setfield(state, lua_upvalueindex(1), "active");
    return 0;
}

int CreateEventFunctionRefAndCancel(lua_State* state, int functionIndex, uint32 shots)
{
    luaL_checktype(state, functionIndex, LUA_TFUNCTION);
    functionIndex = lua_absindex(state, functionIndex);

    lua_newtable(state);
    int handleIndex = lua_gettop(state);

    lua_pushvalue(state, functionIndex);
    lua_setfield(state, handleIndex, "function");

    lua_pushinteger(state, shots);
    lua_setfield(state, handleIndex, "shots");

    lua_pushboolean(state, true);
    lua_setfield(state, handleIndex, "active");

    lua_pushvalue(state, handleIndex);
    lua_pushcclosure(state, &LuaEventDispatchWrapper, 1);
    int functionRef = luaL_ref(state, LUA_REGISTRYINDEX);

    lua_pushvalue(state, handleIndex);
    lua_pushcclosure(state, &LuaEventCancelWrapper, 1);
    lua_remove(state, handleIndex);

    return functionRef;
}

int LuaRegisterPlayerEvent(lua_State* state)
{
    auto* engine = GetEngine(state);
    if (!engine)
        return luaL_error(state, "Lua engine is not available");

    uint32 eventId = static_cast<uint32>(luaL_checkinteger(state, 1));
    int functionRef = CreateEventFunctionRefAndCancel(state, 2, ReadEventShots(state, 3));
    engine->RegisterPlayerEvent(eventId, functionRef);
    return 1;
}

int LuaRegisterServerEvent(lua_State* state)
{
    auto* engine = GetEngine(state);
    if (!engine)
        return luaL_error(state, "Lua engine is not available");

    uint32 eventId = static_cast<uint32>(luaL_checkinteger(state, 1));
    int functionRef = CreateEventFunctionRefAndCancel(state, 2, ReadEventShots(state, 3));
    engine->RegisterServerEvent(eventId, functionRef);
    return 1;
}

int LuaRegisterGroupEvent(lua_State* state)
{
    auto* engine = GetEngine(state);
    if (!engine)
        return luaL_error(state, "Lua engine is not available");

    uint32 eventId = static_cast<uint32>(luaL_checkinteger(state, 1));
    int functionRef = CreateEventFunctionRefAndCancel(state, 2, ReadEventShots(state, 3));
    engine->RegisterGroupEvent(eventId, functionRef);
    return 1;
}

int LuaRegisterGuildEvent(lua_State* state)
{
    auto* engine = GetEngine(state);
    if (!engine)
        return luaL_error(state, "Lua engine is not available");

    uint32 eventId = static_cast<uint32>(luaL_checkinteger(state, 1));
    int functionRef = CreateEventFunctionRefAndCancel(state, 2, ReadEventShots(state, 3));
    engine->RegisterGuildEvent(eventId, functionRef);
    return 1;
}

int LuaRegisterBGEvent(lua_State* state)
{
    auto* engine = GetEngine(state);
    if (!engine)
        return luaL_error(state, "Lua engine is not available");

    uint32 eventId = static_cast<uint32>(luaL_checkinteger(state, 1));
    int functionRef = CreateEventFunctionRefAndCancel(state, 2, ReadEventShots(state, 3));
    engine->RegisterBattleGroundEvent(eventId, functionRef);
    return 1;
}

int LuaRegisterTicketEvent(lua_State* state)
{
    auto* engine = GetEngine(state);
    if (!engine)
        return luaL_error(state, "Lua engine is not available");

    uint32 eventId = static_cast<uint32>(luaL_checkinteger(state, 1));
    int functionRef = CreateEventFunctionRefAndCancel(state, 2, ReadEventShots(state, 3));
    engine->RegisterTicketEvent(eventId, functionRef);
    return 1;
}

int LuaRegisterPacketEvent(lua_State* state)
{
    auto* engine = GetEngine(state);
    if (!engine)
        return luaL_error(state, "Lua engine is not available");

    uint32 opcode = static_cast<uint32>(luaL_checkinteger(state, 1));
    if (opcode >= NUM_MSG_TYPES)
        return luaL_argerror(state, 1, "valid opcode expected");

    uint32 eventId = static_cast<uint32>(luaL_checkinteger(state, 2));
    int functionRef = CreateEventFunctionRefAndCancel(state, 3, ReadEventShots(state, 4));
    engine->RegisterPacketEvent(opcode, eventId, functionRef);
    return 1;
}

int RegisterEntryEvent(lua_State* state, void (TurtleLuaEngine::*registrar)(uint32, uint32, int))
{
    auto* engine = GetEngine(state);
    if (!engine)
        return luaL_error(state, "Lua engine is not available");

    uint32 entry = static_cast<uint32>(luaL_checkinteger(state, 1));
    uint32 eventId = static_cast<uint32>(luaL_checkinteger(state, 2));
    int functionRef = CreateEventFunctionRefAndCancel(state, 3, ReadEventShots(state, 4));
    (engine->*registrar)(entry, eventId, functionRef);
    return 1;
}

int LuaRegisterCreatureEvent(lua_State* state)
{
    return RegisterEntryEvent(state, &TurtleLuaEngine::RegisterCreatureEvent);
}

int LuaRegisterMapEvent(lua_State* state)
{
    return RegisterEntryEvent(state, &TurtleLuaEngine::RegisterMapEvent);
}

int LuaRegisterInstanceEvent(lua_State* state)
{
    return RegisterEntryEvent(state, &TurtleLuaEngine::RegisterInstanceEvent);
}

int LuaRegisterUniqueCreatureEvent(lua_State* state)
{
    auto* engine = GetEngine(state);
    if (!engine)
        return luaL_error(state, "Lua engine is not available");

    ObjectGuid guid = CheckObjectGuidValue(state, 1);
    uint32 instanceId = static_cast<uint32>(luaL_checkinteger(state, 2));
    uint32 eventId = static_cast<uint32>(luaL_checkinteger(state, 3));
    int functionRef = CreateEventFunctionRefAndCancel(state, 4, ReadEventShots(state, 5));
    engine->RegisterUniqueCreatureEvent(guid, instanceId, eventId, functionRef);
    return 1;
}

int LuaRegisterGameObjectEvent(lua_State* state)
{
    return RegisterEntryEvent(state, &TurtleLuaEngine::RegisterGameObjectEvent);
}

int LuaRegisterItemEvent(lua_State* state)
{
    return RegisterEntryEvent(state, &TurtleLuaEngine::RegisterItemEvent);
}

int LuaRegisterSpellEvent(lua_State* state)
{
    return RegisterEntryEvent(state, &TurtleLuaEngine::RegisterSpellEvent);
}

int LuaRegisterCreatureGossipEvent(lua_State* state)
{
    return RegisterEntryEvent(state, &TurtleLuaEngine::RegisterCreatureGossipEvent);
}

int LuaRegisterGameObjectGossipEvent(lua_State* state)
{
    return RegisterEntryEvent(state, &TurtleLuaEngine::RegisterGameObjectGossipEvent);
}

int LuaRegisterItemGossipEvent(lua_State* state)
{
    return RegisterEntryEvent(state, &TurtleLuaEngine::RegisterItemGossipEvent);
}

int LuaRegisterPlayerGossipEvent(lua_State* state)
{
    return RegisterEntryEvent(state, &TurtleLuaEngine::RegisterPlayerGossipEvent);
}

int LuaClearPlayerEvents(lua_State* state)
{
    auto* engine = GetEngine(state);
    if (!engine)
        return luaL_error(state, "Lua engine is not available");

    bool allEvents = lua_isnoneornil(state, 1);
    uint32 eventId = allEvents ? 0 : static_cast<uint32>(luaL_checkinteger(state, 1));
    engine->ClearPlayerEvents(eventId, allEvents);
    return 0;
}

int LuaClearServerEvents(lua_State* state)
{
    auto* engine = GetEngine(state);
    if (!engine)
        return luaL_error(state, "Lua engine is not available");

    bool allEvents = lua_isnoneornil(state, 1);
    uint32 eventId = allEvents ? 0 : static_cast<uint32>(luaL_checkinteger(state, 1));
    engine->ClearServerEvents(eventId, allEvents);
    return 0;
}

int LuaClearGroupEvents(lua_State* state)
{
    auto* engine = GetEngine(state);
    if (!engine)
        return luaL_error(state, "Lua engine is not available");

    bool allEvents = lua_isnoneornil(state, 1);
    uint32 eventId = allEvents ? 0 : static_cast<uint32>(luaL_checkinteger(state, 1));
    engine->ClearGroupEvents(eventId, allEvents);
    return 0;
}

int LuaClearGuildEvents(lua_State* state)
{
    auto* engine = GetEngine(state);
    if (!engine)
        return luaL_error(state, "Lua engine is not available");

    bool allEvents = lua_isnoneornil(state, 1);
    uint32 eventId = allEvents ? 0 : static_cast<uint32>(luaL_checkinteger(state, 1));
    engine->ClearGuildEvents(eventId, allEvents);
    return 0;
}

int LuaClearBattleGroundEvents(lua_State* state)
{
    auto* engine = GetEngine(state);
    if (!engine)
        return luaL_error(state, "Lua engine is not available");

    bool allEvents = lua_isnoneornil(state, 1);
    uint32 eventId = allEvents ? 0 : static_cast<uint32>(luaL_checkinteger(state, 1));
    engine->ClearBattleGroundEvents(eventId, allEvents);
    return 0;
}

int LuaClearTicketEvents(lua_State* state)
{
    auto* engine = GetEngine(state);
    if (!engine)
        return luaL_error(state, "Lua engine is not available");

    bool allEvents = lua_isnoneornil(state, 1);
    uint32 eventId = allEvents ? 0 : static_cast<uint32>(luaL_checkinteger(state, 1));
    engine->ClearTicketEvents(eventId, allEvents);
    return 0;
}

int LuaClearPacketEvents(lua_State* state)
{
    auto* engine = GetEngine(state);
    if (!engine)
        return luaL_error(state, "Lua engine is not available");

    uint32 opcode = static_cast<uint32>(luaL_checkinteger(state, 1));
    if (opcode >= NUM_MSG_TYPES)
        return luaL_argerror(state, 1, "valid opcode expected");

    bool allEvents = lua_isnoneornil(state, 2);
    uint32 eventId = allEvents ? 0 : static_cast<uint32>(luaL_checkinteger(state, 2));
    engine->ClearPacketEvents(opcode, eventId, allEvents);
    return 0;
}

int LuaClearEntryEvent(lua_State* state, void (TurtleLuaEngine::*clearer)(uint32, uint32, bool))
{
    auto* engine = GetEngine(state);
    if (!engine)
        return luaL_error(state, "Lua engine is not available");

    uint32 entry = static_cast<uint32>(luaL_checkinteger(state, 1));
    bool allEvents = lua_isnoneornil(state, 2);
    uint32 eventId = allEvents ? 0 : static_cast<uint32>(luaL_checkinteger(state, 2));
    (engine->*clearer)(entry, eventId, allEvents);
    return 0;
}

int LuaClearCreatureEvents(lua_State* state)
{
    return LuaClearEntryEvent(state, &TurtleLuaEngine::ClearCreatureEvents);
}

int LuaClearMapEvents(lua_State* state)
{
    return LuaClearEntryEvent(state, &TurtleLuaEngine::ClearMapEvents);
}

int LuaClearInstanceEvents(lua_State* state)
{
    return LuaClearEntryEvent(state, &TurtleLuaEngine::ClearInstanceEvents);
}

int LuaClearUniqueCreatureEvents(lua_State* state)
{
    auto* engine = GetEngine(state);
    if (!engine)
        return luaL_error(state, "Lua engine is not available");

    ObjectGuid guid = CheckObjectGuidValue(state, 1);
    uint32 instanceId = static_cast<uint32>(luaL_checkinteger(state, 2));
    bool allEvents = lua_isnoneornil(state, 3);
    uint32 eventId = allEvents ? 0 : static_cast<uint32>(luaL_checkinteger(state, 3));
    engine->ClearUniqueCreatureEvents(guid, instanceId, eventId, allEvents);
    return 0;
}

int LuaClearGameObjectEvents(lua_State* state)
{
    return LuaClearEntryEvent(state, &TurtleLuaEngine::ClearGameObjectEvents);
}

int LuaClearItemEvents(lua_State* state)
{
    return LuaClearEntryEvent(state, &TurtleLuaEngine::ClearItemEvents);
}

int LuaClearSpellEvents(lua_State* state)
{
    return LuaClearEntryEvent(state, &TurtleLuaEngine::ClearSpellEvents);
}

int LuaClearCreatureGossipEvents(lua_State* state)
{
    return LuaClearEntryEvent(state, &TurtleLuaEngine::ClearCreatureGossipEvents);
}

int LuaClearGameObjectGossipEvents(lua_State* state)
{
    return LuaClearEntryEvent(state, &TurtleLuaEngine::ClearGameObjectGossipEvents);
}

int LuaClearItemGossipEvents(lua_State* state)
{
    return LuaClearEntryEvent(state, &TurtleLuaEngine::ClearItemGossipEvents);
}

int LuaClearPlayerGossipEvents(lua_State* state)
{
    return LuaClearEntryEvent(state, &TurtleLuaEngine::ClearPlayerGossipEvents);
}

int LuaReloadEluna(lua_State* state)
{
    if (auto* engine = GetEngine(state))
        engine->Reload();

    return 0;
}

std::string LuaStackToString(lua_State* state)
{
    int argc = lua_gettop(state);
    std::string message;

    for (int i = 1; i <= argc; ++i)
    {
        size_t len = 0;
        char const* text = luaL_tolstring(state, i, &len);
        if (i > 1)
            message += '\t';
        message.append(text, len);
        lua_pop(state, 1);
    }

    return message;
}

void LuaCommandPrint(std::any, char const* text)
{
    if (text && *text)
        sLog.outString("[Lua] %s", text);
}

void LuaCommandFinished(std::any, bool success)
{
    if (!success)
        sLog.outError("[Lua] RunCommand finished with errors");
}

int LuaGetLuaEngine(lua_State* state)
{
    lua_pushstring(state, "ElunaEngine");
    return 1;
}

int LuaGetCoreName(lua_State* state)
{
    lua_pushstring(state, "Turtle WoW");
    return 1;
}

int LuaGetRealmID(lua_State* state)
{
    lua_pushinteger(state, realmID ? realmID : sConfig.GetIntDefault("RealmID", 1));
    return 1;
}

int LuaGetCoreVersion(lua_State* state)
{
    lua_pushstring(state, _FULLVERSION);
    return 1;
}

int LuaGetCoreExpansion(lua_State* state)
{
    lua_pushinteger(state, 0);
    return 1;
}

int LuaGetStateMap(lua_State* state)
{
    lua_pushnil(state);
    return 1;
}

int LuaGetStateMapId(lua_State* state)
{
    lua_pushinteger(state, -1);
    return 1;
}

int LuaGetStateInstanceId(lua_State* state)
{
    lua_pushinteger(state, 0);
    return 1;
}

int LuaIsCompatibilityMode(lua_State* state)
{
    lua_pushboolean(state, true);
    return 1;
}

int LuaBitAnd(lua_State* state)
{
    uint32 a = static_cast<uint32>(luaL_checkinteger(state, 1));
    uint32 b = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, static_cast<lua_Integer>(a & b));
    return 1;
}

int LuaBitOr(lua_State* state)
{
    uint32 a = static_cast<uint32>(luaL_checkinteger(state, 1));
    uint32 b = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, static_cast<lua_Integer>(a | b));
    return 1;
}

int LuaBitLShift(lua_State* state)
{
    uint32 a = static_cast<uint32>(luaL_checkinteger(state, 1));
    uint32 shift = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, static_cast<lua_Integer>(shift >= 32 ? 0 : (a << shift)));
    return 1;
}

int LuaBitRShift(lua_State* state)
{
    uint32 a = static_cast<uint32>(luaL_checkinteger(state, 1));
    uint32 shift = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, static_cast<lua_Integer>(shift >= 32 ? 0 : (a >> shift)));
    return 1;
}

int LuaBitXor(lua_State* state)
{
    uint32 a = static_cast<uint32>(luaL_checkinteger(state, 1));
    uint32 b = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, static_cast<lua_Integer>(a ^ b));
    return 1;
}

int LuaBitNot(lua_State* state)
{
    uint32 a = static_cast<uint32>(luaL_checkinteger(state, 1));
    lua_pushinteger(state, static_cast<lua_Integer>(~a));
    return 1;
}

int LuaCreateLongLong(lua_State* state)
{
    long long value = 0;
    if (lua_isstring(state, 1))
    {
        std::istringstream stream(luaL_checkstring(state, 1));
        stream >> value;
        if (stream.fail())
            return luaL_argerror(state, 1, "long long string could not be converted");
    }
    else if (!lua_isnoneornil(state, 1))
        value = static_cast<long long>(luaL_checkinteger(state, 1));

    lua_pushinteger(state, static_cast<lua_Integer>(value));
    return 1;
}

int LuaCreateULongLong(lua_State* state)
{
    unsigned long long value = 0;
    if (lua_isstring(state, 1))
    {
        std::istringstream stream(luaL_checkstring(state, 1));
        stream >> value;
        if (stream.fail())
            return luaL_argerror(state, 1, "unsigned long long string could not be converted");
    }
    else if (!lua_isnoneornil(state, 1))
        value = static_cast<unsigned long long>(luaL_checkinteger(state, 1));

    lua_pushinteger(state, static_cast<lua_Integer>(value));
    return 1;
}

int LuaGetCurrTime(lua_State* state)
{
    lua_pushinteger(state, WorldTimer::getMSTime());
    return 1;
}

int LuaGetTimeDiff(lua_State* state)
{
    uint32 oldTime = static_cast<uint32>(luaL_checkinteger(state, 1));
    lua_pushinteger(state, WorldTimer::getMSTimeDiffToNow(oldTime));
    return 1;
}

int LuaIsInventoryPos(lua_State* state)
{
    uint8 bag = static_cast<uint8>(luaL_checkinteger(state, 1));
    uint8 slot = static_cast<uint8>(luaL_checkinteger(state, 2));
    lua_pushboolean(state, Player::IsInventoryPos(bag, slot));
    return 1;
}

int LuaIsEquipmentPos(lua_State* state)
{
    uint8 bag = static_cast<uint8>(luaL_checkinteger(state, 1));
    uint8 slot = static_cast<uint8>(luaL_checkinteger(state, 2));
    lua_pushboolean(state, Player::IsEquipmentPos(bag, slot));
    return 1;
}

int LuaIsBankPos(lua_State* state)
{
    uint8 bag = static_cast<uint8>(luaL_checkinteger(state, 1));
    uint8 slot = static_cast<uint8>(luaL_checkinteger(state, 2));
    lua_pushboolean(state, Player::IsBankPos(bag, slot));
    return 1;
}

int LuaIsBagPos(lua_State* state)
{
    uint8 bag = static_cast<uint8>(luaL_checkinteger(state, 1));
    uint8 slot = static_cast<uint8>(luaL_checkinteger(state, 2));
    lua_pushboolean(state, Player::IsBagPos(static_cast<uint16>((bag << 8) | slot)));
    return 1;
}

int LuaPrintInfo(lua_State* state)
{
    std::string message = LuaStackToString(state);
    sLog.outInfo("[Lua] %s", message.c_str());
    return 0;
}

int LuaPrintError(lua_State* state)
{
    std::string message = LuaStackToString(state);
    sLog.outError("[Lua] %s", message.c_str());
    return 0;
}

int LuaPrintDebug(lua_State* state)
{
    std::string message = LuaStackToString(state);
    sLog.outDebug("[Lua] %s", message.c_str());
    return 0;
}

int LuaRunCommand(lua_State* state)
{
    std::string command = luaL_checkstring(state, 1);
    if (!command.empty())
    {
        if (command[0] == '.' || command[0] == '!')
            command.erase(command.begin());

        sWorld.QueueCliCommand(new CliCommandHolder(0, SEC_CONSOLE, nullptr, command.c_str(), &LuaCommandPrint, &LuaCommandFinished));
    }

    return 0;
}

int LuaKick(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    if (player && player->GetSession())
        player->GetSession()->KickPlayer();
    return 0;
}

int LuaBan(lua_State* state)
{
    int banMode = static_cast<int>(luaL_checkinteger(state, 1));
    std::string nameOrIP = luaL_checkstring(state, 2);
    uint32 duration = static_cast<uint32>(luaL_checkinteger(state, 3));
    std::string reason = luaL_optstring(state, 4, "");
    std::string author = luaL_optstring(state, 5, "");

    switch (banMode)
    {
        case BAN_ACCOUNT:
            if (!AccountMgr::normalizeString(nameOrIP))
                return luaL_argerror(state, 2, "invalid account name");
            break;
        case BAN_CHARACTER:
            if (!normalizePlayerName(nameOrIP))
                return luaL_argerror(state, 2, "invalid character name");
            break;
        case BAN_IP:
            if (!IsIPAddress(nameOrIP.c_str()))
                return luaL_argerror(state, 2, "invalid ip");
            break;
        default:
            return luaL_argerror(state, 1, "unknown ban mode");
    }

    BanReturn result = sWorld.BanAccount(static_cast<BanMode>(banMode), nameOrIP, duration, reason, author);
    switch (result)
    {
        case BAN_SUCCESS:
            lua_pushinteger(state, 0);
            break;
        case BAN_SYNTAX_ERROR:
            lua_pushinteger(state, 1);
            break;
        case BAN_NOTFOUND:
            lua_pushinteger(state, 2);
            break;
        case BAN_INPROGRESS:
            lua_pushinteger(state, 3);
            break;
        default:
            lua_pushnil(state);
            break;
    }

    return 1;
}

int LuaSaveAllPlayers(lua_State* /*state*/)
{
    sObjectAccessor.SaveAllPlayers();
    return 0;
}

int LuaSendMail(lua_State* state)
{
    int index = 0;
    std::string subject = luaL_checkstring(state, ++index);
    std::string body = luaL_checkstring(state, ++index);
    uint32 receiverLow = static_cast<uint32>(luaL_checkinteger(state, ++index));
    uint32 senderLow = static_cast<uint32>(luaL_optinteger(state, ++index, 0));
    uint32 stationery = static_cast<uint32>(luaL_optinteger(state, ++index, MAIL_STATIONERY_DEFAULT));
    uint32 delay = static_cast<uint32>(luaL_optinteger(state, ++index, 0));
    uint32 money = static_cast<uint32>(luaL_optinteger(state, ++index, 0));
    uint32 cod = static_cast<uint32>(luaL_optinteger(state, ++index, 0));

    ObjectGuid receiverGuid(HIGHGUID_PLAYER, receiverLow);
    Player* receiver = ObjectAccessor::FindPlayer(receiverGuid);
    if (!receiver)
    {
        std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery("SELECT `guid` FROM `characters` WHERE `guid` = '%u'", receiverLow));
        if (!result)
            return luaL_argerror(state, 3, "valid receiver player guid low expected");
    }

    MailDraft draft(subject, body);
    if (money)
        draft.SetMoney(money);
    if (cod)
        draft.SetCOD(cod);

    uint8 addedItems = 0;
    int argCount = lua_gettop(state);
    while (index + 2 <= argCount)
    {
        if (addedItems >= MAX_MAIL_ITEMS)
            return luaL_argerror(state, index + 1, "too many mail item attachments for this core");

        uint32 entry = static_cast<uint32>(luaL_checkinteger(state, ++index));
        uint32 amount = static_cast<uint32>(luaL_checkinteger(state, ++index));

        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(entry);
        if (!proto)
            return luaL_argerror(state, index - 1, "valid item entry expected");
        if (amount < 1 || amount > proto->GetMaxStackSize() || (proto->MaxCount > 0 && amount > uint32(proto->MaxCount)))
            return luaL_argerror(state, index, "valid item amount expected");

        Item* item = Item::CreateItem(entry, amount, receiver);
        if (!item)
            return luaL_error(state, "failed to create mail item %u", entry);

        item->SaveToDB();
        draft.AddItem(item);
        lua_pushinteger(state, item->GetGUIDLow());
        ++addedItems;
    }

    if (index != argCount)
        return luaL_argerror(state, index + 1, "item entry and amount expected");

    MailSender sender(MAIL_NORMAL, senderLow, static_cast<MailStationery>(stationery));
    draft.SendMailTo(MailReceiver(receiver, receiverGuid), sender, MAIL_CHECK_MASK_NONE, delay);
    return addedItems;
}

int LuaRemoveEvents(lua_State* state)
{
    bool allEvents = lua_isnoneornil(state, 1) ? false : lua_toboolean(state, 1) != 0;
    if (TurtleLuaEngine* engine = GetEngine(state))
        engine->RemoveTimedEvents(allEvents);

    return 0;
}

int LuaPrint(lua_State* state)
{
    std::string message = LuaStackToString(state);
    sLog.outString("[Lua] %s", message.c_str());
    return 0;
}

int LuaCreateLuaEvent(lua_State* state)
{
    auto* engine = GetEngine(state);
    if (!engine)
        return luaL_error(state, "Lua engine is not available");

    luaL_checktype(state, 1, LUA_TFUNCTION);
    uint32 delay = static_cast<uint32>(luaL_checkinteger(state, 2));
    uint32 repeats = static_cast<uint32>(luaL_optinteger(state, 3, 1));

    lua_pushvalue(state, 1);
    int functionRef = luaL_ref(state, LUA_REGISTRYINDEX);
    lua_pushinteger(state, engine->CreateTimedEvent(functionRef, delay, repeats));
    return 1;
}

int LuaRemoveEventById(lua_State* state)
{
    auto* engine = GetEngine(state);
    uint32 eventId = static_cast<uint32>(luaL_checkinteger(state, 1));
    lua_pushboolean(state, engine && engine->RemoveTimedEvent(eventId));
    return 1;
}

void PushField(lua_State* state, Field const& field)
{
    if (field.IsNULL())
    {
        lua_pushnil(state);
        return;
    }

    switch (field.GetType())
    {
        case Field::DB_TYPE_INTEGER:
            lua_pushinteger(state, static_cast<lua_Integer>(field.GetInt32()));
            break;
        case Field::DB_TYPE_FLOAT:
            lua_pushnumber(state, field.GetFloat());
            break;
        case Field::DB_TYPE_BOOL:
            lua_pushboolean(state, field.GetBool());
            break;
        case Field::DB_TYPE_STRING:
        case Field::DB_TYPE_UNKNOWN:
        default:
            lua_pushstring(state, field.GetString());
            break;
    }
}

int PushQueryResult(lua_State* state, QueryNamedResult* rawResult)
{
    std::unique_ptr<QueryNamedResult> result(rawResult);
    if (!result)
    {
        lua_pushnil(state);
        return 1;
    }

    auto* holder = static_cast<LuaQuery*>(lua_newuserdata(state, sizeof(LuaQuery)));
    holder->result = result.release();
    holder->hasRow = holder->result && holder->result->Fetch();

    luaL_getmetatable(state, ELUNAQUERY_METATABLE);
    lua_setmetatable(state, -2);
    return 1;
}

QueryNamedResult* RequireQueryResult(lua_State* state, int index, LuaQuery** outHolder = nullptr)
{
    LuaQuery* holder = CheckQuery(state, index);
    if (outHolder)
        *outHolder = holder;

    if (!holder || !holder->result)
        luaL_error(state, "valid ElunaQuery expected");

    return holder->result;
}

Field const* QueryGetField(lua_State* state, int columnIndex)
{
    LuaQuery* holder = nullptr;
    QueryNamedResult* result = RequireQueryResult(state, 1, &holder);
    if (!holder->hasRow)
        luaL_error(state, "ElunaQuery has no current row");

    lua_Integer rawColumn = luaL_checkinteger(state, columnIndex);
    if (rawColumn < 0)
        luaL_argerror(state, columnIndex, "column index must be >= 0");

    uint32 column = static_cast<uint32>(rawColumn);
    uint32 fieldCount = result->GetFieldCount();
    if (column >= fieldCount)
        luaL_argerror(state, columnIndex, "column index out of range");

    Field* fields = result->Fetch();
    if (!fields)
        luaL_error(state, "ElunaQuery has no current row");

    return &fields[column];
}

char const* FieldStringOrEmpty(Field const& field)
{
    return field.IsNULL() || !field.GetString() ? "" : field.GetString();
}

int QueryIsNull(lua_State* state)
{
    Field const* field = QueryGetField(state, 2);
    lua_pushboolean(state, field->IsNULL());
    return 1;
}

int QueryGetColumnCount(lua_State* state)
{
    QueryNamedResult* result = RequireQueryResult(state, 1);
    lua_pushinteger(state, result->GetFieldCount());
    return 1;
}

int QueryGetRowCount(lua_State* state)
{
    QueryNamedResult* result = RequireQueryResult(state, 1);
    uint64 rows = result->GetRowCount();
    lua_pushinteger(state, rows > 0xFFFFFFFFULL ? -1 : static_cast<lua_Integer>(rows));
    return 1;
}

int QueryGetBool(lua_State* state)
{
    Field const* field = QueryGetField(state, 2);
    lua_pushboolean(state, !field->IsNULL() && field->GetBool());
    return 1;
}

int QueryGetUInt8(lua_State* state)
{
    Field const* field = QueryGetField(state, 2);
    lua_pushinteger(state, field->IsNULL() ? 0 : field->GetUInt8());
    return 1;
}

int QueryGetUInt16(lua_State* state)
{
    Field const* field = QueryGetField(state, 2);
    lua_pushinteger(state, field->IsNULL() ? 0 : field->GetUInt16());
    return 1;
}

int QueryGetUInt32(lua_State* state)
{
    Field const* field = QueryGetField(state, 2);
    lua_pushinteger(state, field->IsNULL() ? 0 : field->GetUInt32());
    return 1;
}

int QueryGetUInt64(lua_State* state)
{
    Field const* field = QueryGetField(state, 2);
    lua_pushinteger(state, field->IsNULL() ? 0 : static_cast<lua_Integer>(field->GetUInt64()));
    return 1;
}

int QueryGetInt8(lua_State* state)
{
    Field const* field = QueryGetField(state, 2);
    lua_pushinteger(state, field->IsNULL() ? 0 : static_cast<int8>(field->GetInt32()));
    return 1;
}

int QueryGetInt16(lua_State* state)
{
    Field const* field = QueryGetField(state, 2);
    lua_pushinteger(state, field->IsNULL() ? 0 : field->GetInt16());
    return 1;
}

int QueryGetInt32(lua_State* state)
{
    Field const* field = QueryGetField(state, 2);
    lua_pushinteger(state, field->IsNULL() ? 0 : field->GetInt32());
    return 1;
}

int QueryGetInt64(lua_State* state)
{
    Field const* field = QueryGetField(state, 2);
    lua_pushinteger(state, field->IsNULL() ? 0 : static_cast<lua_Integer>(std::strtoll(FieldStringOrEmpty(*field), nullptr, 10)));
    return 1;
}

int QueryGetFloat(lua_State* state)
{
    Field const* field = QueryGetField(state, 2);
    lua_pushnumber(state, field->IsNULL() ? 0.0f : field->GetFloat());
    return 1;
}

int QueryGetDouble(lua_State* state)
{
    Field const* field = QueryGetField(state, 2);
    lua_pushnumber(state, field->IsNULL() ? 0.0 : std::strtod(FieldStringOrEmpty(*field), nullptr));
    return 1;
}

int QueryGetString(lua_State* state)
{
    Field const* field = QueryGetField(state, 2);
    if (field->IsNULL())
        lua_pushnil(state);
    else
        lua_pushstring(state, field->GetString());
    return 1;
}

int QueryNextRow(lua_State* state)
{
    LuaQuery* holder = nullptr;
    QueryNamedResult* result = RequireQueryResult(state, 1, &holder);
    holder->hasRow = result->NextRow();
    lua_pushboolean(state, holder->hasRow);
    return 1;
}

int QueryGetRow(lua_State* state)
{
    LuaQuery* holder = nullptr;
    QueryNamedResult* result = RequireQueryResult(state, 1, &holder);
    if (!holder->hasRow)
    {
        lua_pushnil(state);
        return 1;
    }

    Field* fields = result->Fetch();
    if (!fields)
    {
        lua_pushnil(state);
        return 1;
    }

    uint32 fieldCount = result->GetFieldCount();
    QueryFieldNames const& names = result->GetFieldNames();
    lua_newtable(state);
    for (uint32 i = 0; i < fieldCount; ++i)
    {
        PushField(state, fields[i]);
        lua_rawseti(state, -2, i + 1);

        if (i < names.size() && !names[i].empty())
        {
            PushField(state, fields[i]);
            lua_setfield(state, -2, names[i].c_str());
        }
    }
    return 1;
}

int QueryGC(lua_State* state)
{
    LuaQuery* holder = CheckQuery(state, 1);
    if (holder)
    {
        delete holder->result;
        holder->result = nullptr;
        holder->hasRow = false;
    }
    return 0;
}

class LuaAsyncNamedQueryOperation : public SqlOperation
{
public:
    LuaAsyncNamedQueryOperation(std::string sql, uint64 stateId, int functionRef)
        : _sql(std::move(sql)), _stateId(stateId), _functionRef(functionRef)
    {
    }

    bool Execute(SqlConnection* conn) override
    {
        QueryNamedResult* result = nullptr;
        {
            SqlConnection::Lock guard(conn);
            result = guard->QueryNamed(_sql.c_str());
        }

        sTurtleLuaEngine.EnqueueAsyncQueryResult(_stateId, _functionRef, result);
        return true;
    }

private:
    std::string _sql;
    uint64 _stateId;
    int _functionRef;
};

int LuaDBQueryAsync(lua_State* state, Database& database)
{
    auto* engine = GetEngine(state);
    if (!engine)
        return luaL_error(state, "Lua engine is not available");

    char const* sql = luaL_checkstring(state, 1);
    luaL_checktype(state, 2, LUA_TFUNCTION);

    lua_pushvalue(state, 2);
    int functionRef = luaL_ref(state, LUA_REGISTRYINDEX);
    if (functionRef == LUA_REFNIL || functionRef == LUA_NOREF)
        return luaL_argerror(state, 2, "unable to make a ref to function");

    if (!database)
    {
        luaL_unref(state, LUA_REGISTRYINDEX, functionRef);
        return luaL_error(state, "database is not available");
    }

    database.AddToDelayQueue(new LuaAsyncNamedQueryOperation(sql, engine->GetStateId(), functionRef));
    return 0;
}

struct LuaHttpRequestData
{
    std::string method;
    std::string url;
    std::string body;
    std::string contentType;
    httplib::Headers headers;
};

struct ParsedHttpUrl
{
    std::string host;
    std::string path;
};

std::string ToUpperCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch)
    {
        return static_cast<char>(std::toupper(ch));
    });
    return value;
}

bool IsSupportedHttpMethod(std::string const& method)
{
    return method == "GET" || method == "HEAD" || method == "POST" || method == "PUT" ||
        method == "PATCH" || method == "DELETE" || method == "OPTIONS";
}

bool ParseHttpUrl(std::string const& url, ParsedHttpUrl& parsed)
{
    std::size_t schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos)
        return false;

    std::string scheme = ToUpperCopy(url.substr(0, schemeEnd));
    if (scheme != "HTTP" && scheme != "HTTPS")
        return false;

    std::size_t authorityStart = schemeEnd + 3;
    if (authorityStart >= url.size())
        return false;

    std::size_t pathStart = url.find_first_of("/?#", authorityStart);
    std::string authority = pathStart == std::string::npos ? url.substr(authorityStart) : url.substr(authorityStart, pathStart - authorityStart);
    if (authority.empty())
        return false;

    parsed.host = (scheme == "HTTPS" ? "https" : "http") + std::string("://") + authority;
    if (pathStart == std::string::npos)
        parsed.path = "/";
    else if (url[pathStart] == '/')
        parsed.path = url.substr(pathStart);
    else
        parsed.path = "/" + url.substr(pathStart);

    return true;
}

httplib::Result DoLuaHttpRequest(httplib::Client& client, LuaHttpRequestData const& request, std::string const& path)
{
    if (request.method == "GET")
        return client.Get(path, request.headers);
    if (request.method == "HEAD")
        return client.Head(path, request.headers);
    if (request.method == "POST")
        return client.Post(path, request.headers, request.body, request.contentType);
    if (request.method == "PUT")
        return client.Put(path, request.headers, request.body, request.contentType);
    if (request.method == "PATCH")
        return client.Patch(path, request.headers, request.body, request.contentType);
    if (request.method == "DELETE")
    {
        if (!request.body.empty())
            return client.Delete(path, request.headers, request.body, request.contentType.empty() ? "text/plain" : request.contentType);

        return client.Delete(path, request.headers);
    }
    if (request.method == "OPTIONS")
        return client.Options(path, request.headers);

    return httplib::Result();
}

void ExecuteLuaHttpRequest(uint64 stateId, int functionRef, LuaHttpRequestData request)
{
    int statusCode = 0;
    std::string body;
    std::vector<std::pair<std::string, std::string>> responseHeaders;

    try
    {
        ParsedHttpUrl parsed;
        if (!ParseHttpUrl(request.url, parsed))
        {
            body = "Could not parse URL";
            sTurtleLuaEngine.EnqueueHttpResponse(stateId, functionRef, statusCode, std::move(body), std::move(responseHeaders));
            return;
        }

        httplib::Client client(parsed.host);
        client.set_connection_timeout(0, 3000000);
        client.set_read_timeout(5, 0);
        client.set_write_timeout(5, 0);
        client.set_follow_location(true);

        httplib::Result result = DoLuaHttpRequest(client, request, parsed.path);
        if (result)
        {
            statusCode = result->status;
            body = result->body;
            responseHeaders.reserve(result->headers.size());
            for (auto const& header : result->headers)
                responseHeaders.emplace_back(header.first, header.second);
        }
        else
            body = httplib::to_string(result.error());
    }
    catch (std::exception const& ex)
    {
        body = ex.what();
    }

    sTurtleLuaEngine.EnqueueHttpResponse(stateId, functionRef, statusCode, std::move(body), std::move(responseHeaders));
}

int LuaHttpRequest(lua_State* state)
{
    auto* engine = GetEngine(state);
    if (!engine)
        return luaL_error(state, "Lua engine is not available");

    LuaHttpRequestData request;
    request.method = ToUpperCopy(luaL_checkstring(state, 1));
    request.url = luaL_checkstring(state, 2);

    if (!IsSupportedHttpMethod(request.method))
        return luaL_argerror(state, 1, "unsupported HTTP method");

    int headersIndex = 3;
    int callbackIndex = 3;
    if (!lua_istable(state, headersIndex) && lua_isstring(state, headersIndex) && lua_isstring(state, headersIndex + 1))
    {
        request.body = luaL_checkstring(state, 3);
        request.contentType = luaL_checkstring(state, 4);
        headersIndex = 5;
        callbackIndex = 5;
    }

    if (lua_istable(state, headersIndex))
    {
        int tableIndex = lua_absindex(state, headersIndex);
        ++callbackIndex;

        lua_pushnil(state);
        while (lua_next(state, tableIndex) != 0)
        {
            if (lua_isstring(state, -2) && lua_isstring(state, -1))
                request.headers.emplace(lua_tostring(state, -2), lua_tostring(state, -1));

            lua_pop(state, 1);
        }
    }

    luaL_checktype(state, callbackIndex, LUA_TFUNCTION);
    lua_pushvalue(state, callbackIndex);
    int functionRef = luaL_ref(state, LUA_REGISTRYINDEX);
    if (functionRef == LUA_REFNIL || functionRef == LUA_NOREF)
        return luaL_argerror(state, callbackIndex, "unable to make a ref to function");

    uint64 stateId = engine->GetStateId();
    try
    {
        std::thread([stateId, functionRef, request = std::move(request)]() mutable
        {
            ExecuteLuaHttpRequest(stateId, functionRef, std::move(request));
        }).detach();
    }
    catch (std::exception const& ex)
    {
        luaL_unref(state, LUA_REGISTRYINDEX, functionRef);
        return luaL_error(state, "could not start HTTP request thread: %s", ex.what());
    }

    return 0;
}

int LuaWorldDBQuery(lua_State* state)
{
    char const* sql = luaL_checkstring(state, 1);
    return PushQueryResult(state, WorldDatabase.QueryNamed(sql));
}

int LuaCharDBQuery(lua_State* state)
{
    char const* sql = luaL_checkstring(state, 1);
    return PushQueryResult(state, CharacterDatabase.QueryNamed(sql));
}

int LuaLoginDBQuery(lua_State* state)
{
    char const* sql = luaL_checkstring(state, 1);
    return PushQueryResult(state, LoginDatabase.QueryNamed(sql));
}

int LuaWorldDBQueryAsync(lua_State* state)
{
    return LuaDBQueryAsync(state, WorldDatabase);
}

int LuaCharDBQueryAsync(lua_State* state)
{
    return LuaDBQueryAsync(state, CharacterDatabase);
}

int LuaLoginDBQueryAsync(lua_State* state)
{
    return LuaDBQueryAsync(state, LoginDatabase);
}

int LuaWorldDBExecute(lua_State* state)
{
    char const* sql = luaL_checkstring(state, 1);
    lua_pushboolean(state, WorldDatabase.DirectExecute(sql));
    return 1;
}

int LuaCharDBExecute(lua_State* state)
{
    char const* sql = luaL_checkstring(state, 1);
    lua_pushboolean(state, CharacterDatabase.DirectExecute(sql));
    return 1;
}

int LuaLoginDBExecute(lua_State* state)
{
    char const* sql = luaL_checkstring(state, 1);
    lua_pushboolean(state, LoginDatabase.DirectExecute(sql));
    return 1;
}

int LuaGetPlayerByName(lua_State* state)
{
    auto* engine = GetEngine(state);
    char const* name = luaL_checkstring(state, 1);

    if (!engine)
    {
        lua_pushnil(state);
        return 1;
    }

    Player* player = ObjectAccessor::FindPlayerByName(name);
    if (!player)
    {
        lua_pushnil(state);
        return 1;
    }

    engine->PushPlayer(player);
    return 1;
}

int LuaGetPlayerByGUID(lua_State* state)
{
    auto* engine = GetEngine(state);
    ObjectGuid guid;
    if (luaL_testudata(state, 1, OBJECTGUID_METATABLE))
        guid = *CheckObjectGuid(state, 1);
    else
    {
        uint32 guidLow = static_cast<uint32>(luaL_checkinteger(state, 1));
        guid = ObjectGuid(HIGHGUID_PLAYER, guidLow);
    }

    Player* player = ObjectAccessor::FindPlayer(guid);
    if (engine)
        engine->PushPlayer(player);
    else
        lua_pushnil(state);

    return 1;
}

int LuaGetPlayerByGUIDLow(lua_State* state)
{
    return LuaGetPlayerByGUID(state);
}

int LuaCreateObjectGuid(lua_State* state)
{
    HighGuid high = static_cast<HighGuid>(luaL_checkinteger(state, 1));
    int argCount = lua_gettop(state);
    if (argCount >= 3)
    {
        uint32 entry = static_cast<uint32>(luaL_checkinteger(state, 2));
        uint32 counter = static_cast<uint32>(luaL_checkinteger(state, 3));
        PushObjectGuidValue(state, ObjectGuid(high, entry, counter));
        return 1;
    }

    uint32 counter = static_cast<uint32>(luaL_checkinteger(state, 2));
    PushObjectGuidValue(state, ObjectGuid(high, counter));
    return 1;
}

int LuaCreatePlayerGuid(lua_State* state)
{
    uint32 counter = static_cast<uint32>(luaL_checkinteger(state, 1));
    PushObjectGuidValue(state, ObjectGuid(HIGHGUID_PLAYER, counter));
    return 1;
}

int LuaCreateItemGuid(lua_State* state)
{
    uint32 counter = static_cast<uint32>(luaL_checkinteger(state, 1));
    PushObjectGuidValue(state, ObjectGuid(HIGHGUID_ITEM, counter));
    return 1;
}

int LuaCreateCreatureGuid(lua_State* state)
{
    uint32 entry = static_cast<uint32>(luaL_checkinteger(state, 1));
    uint32 counter = static_cast<uint32>(luaL_checkinteger(state, 2));
    PushObjectGuidValue(state, ObjectGuid(HIGHGUID_UNIT, entry, counter));
    return 1;
}

int LuaCreateGameObjectGuid(lua_State* state)
{
    uint32 entry = static_cast<uint32>(luaL_checkinteger(state, 1));
    uint32 counter = static_cast<uint32>(luaL_checkinteger(state, 2));
    PushObjectGuidValue(state, ObjectGuid(HIGHGUID_GAMEOBJECT, entry, counter));
    return 1;
}

int LuaCreatePacket(lua_State* state)
{
    uint32 opcode = static_cast<uint32>(luaL_checkinteger(state, 1));
    size_t size = static_cast<size_t>(luaL_checkinteger(state, 2));
    if (opcode >= NUM_MSG_TYPES)
        return luaL_argerror(state, 1, "valid opcode expected");

    PushWorldPacketValue(state, new WorldPacket(static_cast<uint16>(opcode), size), true);
    return 1;
}

int LuaGetPlayerGUID(lua_State* state)
{
    uint32 lowguid = static_cast<uint32>(luaL_checkinteger(state, 1));
    PushObjectGuidValue(state, ObjectGuid(HIGHGUID_PLAYER, lowguid));
    return 1;
}

int LuaGetItemGUID(lua_State* state)
{
    uint32 lowguid = static_cast<uint32>(luaL_checkinteger(state, 1));
    PushObjectGuidValue(state, ObjectGuid(HIGHGUID_ITEM, lowguid));
    return 1;
}

int LuaGetObjectGUID(lua_State* state)
{
    uint32 lowguid = static_cast<uint32>(luaL_checkinteger(state, 1));
    uint32 entry = static_cast<uint32>(luaL_checkinteger(state, 2));
    PushObjectGuidValue(state, ObjectGuid(HIGHGUID_GAMEOBJECT, entry, lowguid));
    return 1;
}

int LuaGetUnitGUID(lua_State* state)
{
    uint32 lowguid = static_cast<uint32>(luaL_checkinteger(state, 1));
    uint32 entry = static_cast<uint32>(luaL_checkinteger(state, 2));
    PushObjectGuidValue(state, ObjectGuid(HIGHGUID_UNIT, entry, lowguid));
    return 1;
}

int LuaGetGUIDLow(lua_State* state)
{
    ObjectGuid const* guid = CheckObjectGuid(state, 1);
    lua_pushinteger(state, guid ? guid->GetCounter() : 0);
    return 1;
}

int LuaGetGUIDType(lua_State* state)
{
    ObjectGuid const* guid = CheckObjectGuid(state, 1);
    lua_pushinteger(state, guid ? guid->GetHigh() : 0);
    return 1;
}

int LuaGetGUIDEntry(lua_State* state)
{
    ObjectGuid const* guid = CheckObjectGuid(state, 1);
    lua_pushinteger(state, guid ? guid->GetEntry() : 0);
    return 1;
}

int LuaSendWorldMessage(lua_State* state)
{
    char const* message = luaL_checkstring(state, 1);
    sWorld.SendServerMessage(SERVER_MSG_CUSTOM, message);
    return 0;
}

int ChatHandlerSendSysMessage(lua_State* state)
{
    LuaChatHandler* holder = CheckChatHandler(state, 1);
    if (Player* player = GetChatHandlerPlayer(holder))
    {
        ChatHandler handler(player);
        if (lua_isnumber(state, 2))
            handler.SendSysMessage(static_cast<int32>(luaL_checkinteger(state, 2)));
        else
            handler.SendSysMessage(luaL_checkstring(state, 2));
    }
    else if (!lua_isnoneornil(state, 2))
    {
        if (lua_isnumber(state, 2))
            sLog.outString("%s", sObjectMgr.GetMangosStringForDBCLocale(static_cast<int32>(lua_tointeger(state, 2))));
        else
            sLog.outString("%s", luaL_checkstring(state, 2));
    }

    return 0;
}

int ChatHandlerIsConsole(lua_State* state)
{
    LuaChatHandler* holder = CheckChatHandler(state, 1);
    lua_pushboolean(state, !GetChatHandlerPlayer(holder));
    return 1;
}

int ChatHandlerGetPlayer(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    LuaChatHandler* holder = CheckChatHandler(state, 1);
    if (engine)
        engine->PushPlayer(GetChatHandlerPlayer(holder));
    else
        lua_pushnil(state);
    return 1;
}

int ChatHandlerSendGlobalSysMessage(lua_State* state)
{
    char const* message = luaL_checkstring(state, 2);
    sWorld.SendGlobalText(message, nullptr);
    return 0;
}

int ChatHandlerSendGlobalGMSysMessage(lua_State* state)
{
    char const* message = luaL_checkstring(state, 2);
    sWorld.SendGMText(message);
    return 0;
}

int ChatHandlerHasLowerSecurity(lua_State* state)
{
    LuaChatHandler* holder = CheckChatHandler(state, 1);
    Player* target = CheckPlayer(state, 2);
    bool strong = lua_isnoneornil(state, 3) ? false : lua_toboolean(state, 3) != 0;

    AccountTypes sourceSecurity = SEC_CONSOLE;
    if (Player* player = GetChatHandlerPlayer(holder))
        sourceSecurity = player->GetSession()->GetSecurity();

    AccountTypes targetSecurity = target && target->GetSession() ? target->GetSession()->GetSecurity() : SEC_PLAYER;
    lua_pushboolean(state, strong ? sourceSecurity <= targetSecurity : sourceSecurity < targetSecurity);
    return 1;
}

int ChatHandlerHasLowerSecurityAccount(lua_State* state)
{
    LuaChatHandler* holder = CheckChatHandler(state, 1);
    uint32 accountId = static_cast<uint32>(luaL_checkinteger(state, 2));
    bool strong = lua_isnoneornil(state, 3) ? false : lua_toboolean(state, 3) != 0;

    AccountTypes sourceSecurity = SEC_CONSOLE;
    if (Player* player = GetChatHandlerPlayer(holder))
        sourceSecurity = player->GetSession()->GetSecurity();

    AccountTypes targetSecurity = sAccountMgr.GetSecurity(accountId);
    lua_pushboolean(state, strong ? sourceSecurity <= targetSecurity : sourceSecurity < targetSecurity);
    return 1;
}

int ChatHandlerGetSelectedPlayer(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    LuaChatHandler* holder = CheckChatHandler(state, 1);
    Player* selected = nullptr;

    if (Player* player = GetChatHandlerPlayer(holder))
    {
        LuaChatHandlerAccess handler(player);
        selected = handler.GetSelectedPlayer();
    }

    if (engine)
        engine->PushPlayer(selected);
    else
        lua_pushnil(state);
    return 1;
}

int ChatHandlerGetSelectedCreature(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    LuaChatHandler* holder = CheckChatHandler(state, 1);
    Creature* selected = nullptr;

    if (Player* player = GetChatHandlerPlayer(holder))
    {
        LuaChatHandlerAccess handler(player);
        selected = handler.GetSelectedCreature();
    }

    if (engine)
        engine->PushCreature(selected);
    else
        lua_pushnil(state);
    return 1;
}

int ChatHandlerGetSelectedUnit(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    LuaChatHandler* holder = CheckChatHandler(state, 1);
    Unit* selected = nullptr;

    if (Player* player = GetChatHandlerPlayer(holder))
    {
        LuaChatHandlerAccess handler(player);
        selected = handler.GetSelectedUnit();
    }

    if (engine)
        engine->PushUnit(selected);
    else
        lua_pushnil(state);
    return 1;
}

int ChatHandlerGetSelectedObject(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    LuaChatHandler* holder = CheckChatHandler(state, 1);
    WorldObject* selected = nullptr;

    if (Player* player = GetChatHandlerPlayer(holder))
    {
        LuaChatHandlerAccess handler(player);
        selected = handler.GetSelectedUnit();
        if (!selected && player->GetMap())
            selected = player->GetMap()->GetGameObject(player->GetSelectedGobj());
    }

    PushWorldObjectValue(state, engine, selected);
    return 1;
}

int ChatHandlerGetSelectedPlayerOrSelf(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    LuaChatHandler* holder = CheckChatHandler(state, 1);
    Player* player = GetChatHandlerPlayer(holder);
    Player* selected = nullptr;

    if (player)
    {
        LuaChatHandlerAccess handler(player);
        selected = handler.GetSelectedPlayer();
    }

    if (engine)
        engine->PushPlayer(selected ? selected : player);
    else
        lua_pushnil(state);
    return 1;
}

int ChatHandlerIsAvailable(lua_State* state)
{
    LuaChatHandler* holder = CheckChatHandler(state, 1);
    uint32 securityLevel = static_cast<uint32>(luaL_checkinteger(state, 2));

    AccountTypes sourceSecurity = SEC_CONSOLE;
    if (Player* player = GetChatHandlerPlayer(holder))
        sourceSecurity = player->GetSession()->GetSecurity();

    lua_pushboolean(state, static_cast<uint32>(sourceSecurity) >= securityLevel);
    return 1;
}

int ChatHandlerHasSentErrorMessage(lua_State* state)
{
    LuaChatHandler* holder = CheckChatHandler(state, 1);
    bool sent = false;

    if (Player* player = GetChatHandlerPlayer(holder))
    {
        ChatHandler handler(player);
        sent = handler.HasSentErrorMessage();
    }

    lua_pushboolean(state, sent);
    return 1;
}

int ChannelGetName(lua_State* state)
{
    LuaChannel* holder = CheckChannel(state, 1);
    if (holder)
        lua_pushlstring(state, holder->name.c_str(), holder->name.size());
    else
        lua_pushnil(state);
    return 1;
}

int ChannelGetId(lua_State* state)
{
    LuaChannel* holder = CheckChannel(state, 1);
    Channel* channel = ResolveChannel(holder);
    lua_pushinteger(state, channel ? channel->GetChannelId() : (holder ? holder->channelId : 0));
    return 1;
}

int ChannelIsValid(lua_State* state)
{
    lua_pushboolean(state, ResolveChannel(CheckChannel(state, 1)) != nullptr);
    return 1;
}

int ChannelIsConstant(lua_State* state)
{
    LuaChannel* holder = CheckChannel(state, 1);
    Channel* channel = ResolveChannel(holder);
    lua_pushboolean(state, channel ? channel->IsConstant() : (holder && holder->channelId != 0));
    return 1;
}

int ChannelIsAnnounce(lua_State* state)
{
    Channel* channel = ResolveChannel(CheckChannel(state, 1));
    lua_pushboolean(state, channel && channel->IsAnnounce());
    return 1;
}

int ChannelSetAnnounce(lua_State* state)
{
    Channel* channel = ResolveChannel(CheckChannel(state, 1));
    if (channel)
        channel->SetAnnounce(lua_toboolean(state, 2) != 0);
    return 0;
}

int ChannelIsLevelRestricted(lua_State* state)
{
    Channel* channel = ResolveChannel(CheckChannel(state, 1));
    lua_pushboolean(state, channel && channel->IsLevelRestricted());
    return 1;
}

int ChannelIsLFG(lua_State* state)
{
    Channel* channel = ResolveChannel(CheckChannel(state, 1));
    lua_pushboolean(state, channel && channel->IsLFG());
    return 1;
}

int ChannelGetPassword(lua_State* state)
{
    Channel* channel = ResolveChannel(CheckChannel(state, 1));
    if (!channel)
    {
        lua_pushnil(state);
        return 1;
    }

    std::string const& password = channel->GetPassword();
    lua_pushlstring(state, password.c_str(), password.size());
    return 1;
}

int ChannelSetPassword(lua_State* state)
{
    Channel* channel = ResolveChannel(CheckChannel(state, 1));
    char const* password = luaL_checkstring(state, 2);
    if (channel)
        channel->SetPassword(password);
    return 0;
}

int ChannelGetNumPlayers(lua_State* state)
{
    Channel* channel = ResolveChannel(CheckChannel(state, 1));
    lua_pushinteger(state, channel ? channel->GetNumPlayers() : 0);
    return 1;
}

int ChannelGetFlags(lua_State* state)
{
    Channel* channel = ResolveChannel(CheckChannel(state, 1));
    lua_pushinteger(state, channel ? channel->GetFlags() : 0);
    return 1;
}

int ChannelHasFlag(lua_State* state)
{
    Channel* channel = ResolveChannel(CheckChannel(state, 1));
    uint32 flag = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushboolean(state, channel && channel->HasFlag(static_cast<uint8>(flag)));
    return 1;
}

int ChannelGetSecurityLevel(lua_State* state)
{
    Channel* channel = ResolveChannel(CheckChannel(state, 1));
    lua_pushinteger(state, channel ? channel->GetSecurityLevel() : 0);
    return 1;
}

int ChannelSetSecurityLevel(lua_State* state)
{
    Channel* channel = ResolveChannel(CheckChannel(state, 1));
    uint32 securityLevel = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (channel)
        channel->SetSecurityLevel(static_cast<uint8>(securityLevel));
    return 0;
}

int ChannelGetTeam(lua_State* state)
{
    LuaChannel* holder = CheckChannel(state, 1);
    Channel* channel = ResolveChannel(holder);
    lua_pushinteger(state, channel ? channel->GetTeam() : (holder ? holder->team : TEAM_NONE));
    return 1;
}

int ChannelIsOn(lua_State* state)
{
    Channel* channel = ResolveChannel(CheckChannel(state, 1));
    ObjectGuid guid = CheckObjectGuidValue(state, 2);
    lua_pushboolean(state, channel && channel->IsOn(guid));
    return 1;
}

int ChannelIsBanned(lua_State* state)
{
    Channel* channel = ResolveChannel(CheckChannel(state, 1));
    ObjectGuid guid = CheckObjectGuidValue(state, 2);
    lua_pushboolean(state, channel && channel->IsBanned(guid));
    return 1;
}

int ChannelGetPlayerFlags(lua_State* state)
{
    Channel* channel = ResolveChannel(CheckChannel(state, 1));
    ObjectGuid guid = CheckObjectGuidValue(state, 2);
    lua_pushinteger(state, channel ? channel->GetPlayerFlags(guid) : 0);
    return 1;
}

int ChannelSetModerator(lua_State* state)
{
    Channel* channel = ResolveChannel(CheckChannel(state, 1));
    ObjectGuid guid = CheckObjectGuidValue(state, 2);
    bool set = lua_toboolean(state, 3) != 0;
    if (channel && channel->IsOn(guid))
        channel->SetModerator(guid, set);
    return 0;
}

int ChannelSetMute(lua_State* state)
{
    Channel* channel = ResolveChannel(CheckChannel(state, 1));
    ObjectGuid guid = CheckObjectGuidValue(state, 2);
    bool set = lua_toboolean(state, 3) != 0;
    if (channel && channel->IsOn(guid))
        channel->SetMute(guid, set);
    return 0;
}

int ChannelSendPacket(lua_State* state)
{
    Channel* channel = ResolveChannel(CheckChannel(state, 1));
    WorldPacket* packet = CheckWorldPacket(state, 2);
    ObjectGuid ignored;
    if (!lua_isnoneornil(state, 3))
        ignored = CheckObjectGuidValue(state, 3);

    if (channel && packet)
        channel->SendToAll(packet, ignored);
    return 0;
}

int ChannelSendToOne(lua_State* state)
{
    Channel* channel = ResolveChannel(CheckChannel(state, 1));
    WorldPacket* packet = CheckWorldPacket(state, 2);
    ObjectGuid receiver = CheckObjectGuidValue(state, 3);
    if (channel && packet)
        channel->SendToOne(packet, receiver);
    return 0;
}

int ChannelSay(lua_State* state)
{
    Channel* channel = ResolveChannel(CheckChannel(state, 1));
    ObjectGuid sender = CheckObjectGuidValue(state, 2);
    char const* message = luaL_checkstring(state, 3);
    uint32 lang = static_cast<uint32>(luaL_optinteger(state, 4, LANG_UNIVERSAL));
    bool skipCheck = lua_toboolean(state, 5) != 0;

    if (channel)
        channel->AsyncSay(sender, message, lang, skipCheck);
    return 0;
}

int ChannelGetString(lua_State* state)
{
    return ChannelGetName(state);
}

int ChannelGC(lua_State* state)
{
    auto* holder = static_cast<LuaChannel*>(luaL_checkudata(state, 1, CHANNEL_METATABLE));
    if (holder)
        holder->~LuaChannel();
    return 0;
}

int RollGetItemGUID(lua_State* state)
{
    LuaRoll* roll = CheckRoll(state, 1);
    lua_pushinteger(state, roll ? roll->itemGuid.GetCounter() : 0);
    return 1;
}

int RollGetItemGUIDObject(lua_State* state)
{
    LuaRoll* roll = CheckRoll(state, 1);
    PushObjectGuidValue(state, roll ? roll->itemGuid : ObjectGuid());
    return 1;
}

int RollGetLootedTargetGUID(lua_State* state)
{
    LuaRoll* roll = CheckRoll(state, 1);
    PushObjectGuidValue(state, roll ? roll->lootedTargetGuid : ObjectGuid());
    return 1;
}

int RollGetItemId(lua_State* state)
{
    LuaRoll* roll = CheckRoll(state, 1);
    lua_pushinteger(state, roll ? roll->itemId : 0);
    return 1;
}

int RollGetItemRandomPropId(lua_State* state)
{
    LuaRoll* roll = CheckRoll(state, 1);
    lua_pushinteger(state, roll ? roll->itemRandomPropId : 0);
    return 1;
}

int RollGetItemRandomSuffix(lua_State* state)
{
    LuaRoll* roll = CheckRoll(state, 1);
    lua_pushinteger(state, roll ? roll->itemRandomSuffix : 0);
    return 1;
}

int RollGetItemCount(lua_State* state)
{
    LuaRoll* roll = CheckRoll(state, 1);
    lua_pushinteger(state, roll ? roll->itemCount : 0);
    return 1;
}

int RollGetPlayerVote(lua_State* state)
{
    LuaRoll* roll = CheckRoll(state, 1);
    ObjectGuid guid = CheckObjectGuidValue(state, 2);

    if (roll)
    {
        for (auto const& vote : roll->playerVotes)
        {
            if (vote.first == guid)
            {
                lua_pushinteger(state, vote.second);
                return 1;
            }
        }
    }

    lua_pushnil(state);
    return 1;
}

int RollGetPlayerVoteGUIDs(lua_State* state)
{
    LuaRoll* roll = CheckRoll(state, 1);
    lua_newtable(state);

    if (roll)
    {
        int index = 1;
        for (auto const& vote : roll->playerVotes)
        {
            PushObjectGuidValue(state, vote.first);
            lua_rawseti(state, -2, index++);
        }
    }

    return 1;
}

int RollGetTotalPlayersRolling(lua_State* state)
{
    LuaRoll* roll = CheckRoll(state, 1);
    lua_pushinteger(state, roll ? roll->totalPlayersRolling : 0);
    return 1;
}

int RollGetTotalNeed(lua_State* state)
{
    LuaRoll* roll = CheckRoll(state, 1);
    lua_pushinteger(state, roll ? roll->totalNeed : 0);
    return 1;
}

int RollGetTotalGreed(lua_State* state)
{
    LuaRoll* roll = CheckRoll(state, 1);
    lua_pushinteger(state, roll ? roll->totalGreed : 0);
    return 1;
}

int RollGetTotalPass(lua_State* state)
{
    LuaRoll* roll = CheckRoll(state, 1);
    lua_pushinteger(state, roll ? roll->totalPass : 0);
    return 1;
}

int RollGetItemSlot(lua_State* state)
{
    LuaRoll* roll = CheckRoll(state, 1);
    lua_pushinteger(state, roll ? roll->itemSlot : 0);
    return 1;
}

int RollGetRollVoteMask(lua_State* state)
{
    LuaRoll* roll = CheckRoll(state, 1);
    lua_pushinteger(state, roll ? roll->rollVoteMask : 0);
    return 1;
}

int RollGC(lua_State* state)
{
    auto* holder = static_cast<LuaRoll*>(luaL_checkudata(state, 1, ROLL_METATABLE));
    if (holder)
        holder->~LuaRoll();
    return 0;
}

int LuaGetGameTime(lua_State* state)
{
    lua_pushinteger(state, sWorld.GetGameTime());
    return 1;
}

int LuaGetQuest(lua_State* state)
{
    uint32 questId = static_cast<uint32>(luaL_checkinteger(state, 1));
    PushQuest(state, sObjectMgr.GetQuestTemplate(questId));
    return 1;
}

int LuaGetItemTemplate(lua_State* state)
{
    uint32 itemId = static_cast<uint32>(luaL_checkinteger(state, 1));
    PushItemTemplateValue(state, sObjectMgr.GetItemPrototype(itemId));
    return 1;
}

int LuaGetItemLink(lua_State* state)
{
    uint32 entry = static_cast<uint32>(luaL_checkinteger(state, 1));
    uint32 locale = static_cast<uint32>(luaL_optinteger(state, 2, LOCALE_enUS));
    if (locale >= MAX_LOCALE)
        return luaL_argerror(state, 2, "valid LocaleConstant expected");

    ItemPrototype const* proto = sObjectMgr.GetItemPrototype(entry);
    if (!proto)
        return luaL_argerror(state, 1, "valid item entry expected");

    std::string name = proto->Name1;
    if (locale != LOCALE_enUS)
    {
        if (ItemLocale const* itemLocale = sObjectMgr.GetItemLocale(entry))
        {
            int localeIndex = sObjectMgr.GetIndexForLocale(static_cast<LocaleConstant>(locale));
            if (localeIndex >= 0 && itemLocale->Name.size() > static_cast<size_t>(localeIndex) && !itemLocale->Name[localeIndex].empty())
                name = itemLocale->Name[localeIndex];
        }
    }

    uint32 quality = proto->Quality < MAX_ITEM_QUALITY ? proto->Quality : ITEM_QUALITY_NORMAL;
    uint32 color = ItemQualityColors[quality];

    std::ostringstream link;
    link << "|c" << std::hex << std::nouppercase << color
        << "|Hitem:" << std::dec << entry
        << ":0:0:0:0:0:0:0|h[" << name << "]|h|r";
    std::string text = link.str();
    lua_pushlstring(state, text.c_str(), text.size());
    return 1;
}

int LuaGetAreaName(lua_State* state)
{
    uint32 areaId = static_cast<uint32>(luaL_checkinteger(state, 1));
    uint32 locale = static_cast<uint32>(luaL_optinteger(state, 2, LOCALE_enUS));
    if (locale >= MAX_LOCALE)
        return luaL_argerror(state, 2, "valid LocaleConstant expected");

    AreaEntry const* area = AreaEntry::GetById(areaId);
    if (!area)
        return luaL_argerror(state, 1, "valid area or zone id expected");

    std::string name = area->Name ? area->Name : "";
    if (locale != LOCALE_enUS)
        sObjectMgr.GetAreaLocaleString(areaId, sObjectMgr.GetIndexForLocale(static_cast<LocaleConstant>(locale)), &name);

    lua_pushlstring(state, name.c_str(), name.size());
    return 1;
}

int LuaGetActiveGameEvents(lua_State* state)
{
    lua_newtable(state);
    uint32 index = 1;
    for (uint16 eventId : sGameEventMgr.GetActiveEventList())
    {
        lua_pushinteger(state, eventId);
        lua_rawseti(state, -2, index++);
    }

    return 1;
}

int LuaIsGameEventActive(lua_State* state)
{
    uint16 eventId = static_cast<uint16>(luaL_checkinteger(state, 1));
    lua_pushboolean(state, sGameEventMgr.IsActiveEvent(eventId));
    return 1;
}

int LuaStartGameEvent(lua_State* state)
{
    uint16 eventId = static_cast<uint16>(luaL_checkinteger(state, 1));
    bool force = lua_isnoneornil(state, 2) ? false : lua_toboolean(state, 2) != 0;
    sGameEventMgr.StartEvent(eventId, force);
    return 0;
}

int LuaStopGameEvent(lua_State* state)
{
    uint16 eventId = static_cast<uint16>(luaL_checkinteger(state, 1));
    bool force = lua_isnoneornil(state, 2) ? false : lua_toboolean(state, 2) != 0;
    sGameEventMgr.StopEvent(eventId, force);
    return 0;
}

int LuaGetMapEntrance(lua_State* state)
{
    uint32 mapId = static_cast<uint32>(luaL_checkinteger(state, 1));
    AreaTriggerTeleport const* trigger = sObjectMgr.GetMapEntranceTrigger(mapId);
    if (!trigger)
    {
        lua_pushnil(state);
        return 1;
    }

    WorldLocation const& destination = trigger->destination;
    lua_pushnumber(state, destination.x);
    lua_pushnumber(state, destination.y);
    lua_pushnumber(state, destination.z);
    lua_pushnumber(state, destination.o);
    return 4;
}

int LuaGetGossipMenuOptionLocale(lua_State* state)
{
    uint32 menuId = static_cast<uint32>(luaL_checkinteger(state, 1));
    uint32 optionId = static_cast<uint32>(luaL_checkinteger(state, 2));
    uint32 locale = static_cast<uint32>(luaL_checkinteger(state, 3));
    if (locale >= MAX_LOCALE)
        return luaL_argerror(state, 3, "valid LocaleConstant expected");

    int localeIndex = sObjectMgr.GetIndexForLocale(static_cast<LocaleConstant>(locale));
    std::string optionText;
    std::string boxText;

    if (localeIndex >= 0 && locale != LOCALE_enUS)
    {
        if (GossipMenuItemsLocale const* gossipLocale = sObjectMgr.GetGossipMenuItemsLocale(MAKE_PAIR32(menuId, optionId)))
        {
            if (gossipLocale->OptionText.size() > static_cast<size_t>(localeIndex) && !gossipLocale->OptionText[localeIndex].empty())
                optionText = gossipLocale->OptionText[localeIndex];
            if (gossipLocale->BoxText.size() > static_cast<size_t>(localeIndex) && !gossipLocale->BoxText[localeIndex].empty())
                boxText = gossipLocale->BoxText[localeIndex];
        }
    }

    GossipMenuItemsMapBounds bounds = sObjectMgr.GetGossipMenuItemsMapBounds(menuId);
    for (auto itr = bounds.first; itr != bounds.second; ++itr)
    {
        GossipMenuItems const& item = itr->second;
        if (item.id != optionId)
            continue;

        if (optionText.empty())
        {
            if (item.option_broadcast_text)
                optionText = sObjectMgr.GetBroadcastText(item.option_broadcast_text, localeIndex, GENDER_MALE, false);
            else
                optionText = item.option_text;
        }

        if (boxText.empty())
        {
            if (item.box_broadcast_text)
                boxText = sObjectMgr.GetBroadcastText(item.box_broadcast_text, localeIndex, GENDER_MALE, false);
            else
                boxText = item.box_text;
        }

        break;
    }

    lua_pushlstring(state, optionText.c_str(), optionText.size());
    lua_pushlstring(state, boxText.c_str(), boxText.size());
    return 2;
}

int LuaGetOwnerHalaa(lua_State* state)
{
    lua_pushinteger(state, 0);
    lua_pushnumber(state, 0.0f);
    return 2;
}

int LuaSetOwnerHalaa(lua_State* state)
{
    uint16 teamId = static_cast<uint16>(luaL_checkinteger(state, 1));
    if (teamId > 1)
        return luaL_argerror(state, 1, "0 for Alliance or 1 for Horde expected");

    return 0;
}

int LuaGetCreatureTemplate(lua_State* state)
{
    uint32 entry = static_cast<uint32>(luaL_checkinteger(state, 1));
    PushCreatureTemplateValue(state, sObjectMgr.GetCreatureTemplate(entry));
    return 1;
}

int LuaGetGameObjectTemplate(lua_State* state)
{
    uint32 entry = static_cast<uint32>(luaL_checkinteger(state, 1));
    PushGameObjectTemplateValue(state, sObjectMgr.GetGameObjectInfo(entry));
    return 1;
}

int LuaGetSpellInfo(lua_State* state)
{
    uint32 spellId = static_cast<uint32>(luaL_checkinteger(state, 1));
    PushSpellInfo(state, sSpellMgr.GetSpellEntry(spellId));
    return 1;
}

int LuaLookupEntry(lua_State* state)
{
    std::string dbcName = luaL_checkstring(state, 1);
    uint32 id = static_cast<uint32>(luaL_checkinteger(state, 2));

    if (dbcName == "Spell" || dbcName == "SpellEntry")
    {
        PushSpellInfo(state, sSpellMgr.GetSpellEntry(id));
        return 1;
    }

    if (dbcName == "GemProperties")
        return 0;

    return luaL_error(state, "Invalid DBC name: %s", dbcName.c_str());
}

int LuaGetGuildById(lua_State* state)
{
    auto* engine = GetEngine(state);
    uint32 guildId = static_cast<uint32>(luaL_checkinteger(state, 1));
    Guild* guild = sGuildMgr.GetGuildById(guildId);
    if (engine)
        engine->PushGuild(guild);
    else
        lua_pushnil(state);
    return 1;
}

int LuaGetGuildByName(lua_State* state)
{
    auto* engine = GetEngine(state);
    char const* name = luaL_checkstring(state, 1);
    Guild* guild = sGuildMgr.GetGuildByName(name);
    if (engine)
        engine->PushGuild(guild);
    else
        lua_pushnil(state);
    return 1;
}

int LuaGetGuildByLeaderGUID(lua_State* state)
{
    auto* engine = GetEngine(state);
    ObjectGuid guid = CheckObjectGuidValue(state, 1);
    Guild* guild = sGuildMgr.GetGuildByLeader(guid);
    if (engine)
        engine->PushGuild(guild);
    else
        lua_pushnil(state);
    return 1;
}

int LuaAddTaxiPath(lua_State* state)
{
    luaL_checktype(state, 1, LUA_TTABLE);
    uint32 mountA = static_cast<uint32>(luaL_checkinteger(state, 2));
    uint32 mountH = static_cast<uint32>(luaL_checkinteger(state, 3));
    uint32 price = static_cast<uint32>(luaL_optinteger(state, 4, 0));
    uint32 pathId = static_cast<uint32>(luaL_optinteger(state, 5, 0));

    std::vector<TaxiPathNodeEntry> nodes;
    size_t count = lua_rawlen(state, 1);
    nodes.reserve(count);

    for (size_t i = 1; i <= count; ++i)
    {
        lua_rawgeti(state, 1, static_cast<int>(i));
        if (!lua_istable(state, -1))
        {
            lua_pop(state, 1);
            return luaL_argerror(state, 1, "all waypoints must be tables");
        }

        int waypoint = lua_absindex(state, -1);
        TaxiPathNodeEntry entry{};

        lua_rawgeti(state, waypoint, 1);
        if (lua_isnil(state, -1))
        {
            lua_pop(state, 2);
            return luaL_argerror(state, 1, "waypoint map id is required");
        }
        entry.mapid = static_cast<uint32>(luaL_checkinteger(state, -1));
        lua_pop(state, 1);

        lua_rawgeti(state, waypoint, 2);
        if (lua_isnil(state, -1))
        {
            lua_pop(state, 2);
            return luaL_argerror(state, 1, "waypoint x is required");
        }
        entry.x = static_cast<float>(luaL_checknumber(state, -1));
        lua_pop(state, 1);

        lua_rawgeti(state, waypoint, 3);
        if (lua_isnil(state, -1))
        {
            lua_pop(state, 2);
            return luaL_argerror(state, 1, "waypoint y is required");
        }
        entry.y = static_cast<float>(luaL_checknumber(state, -1));
        lua_pop(state, 1);

        lua_rawgeti(state, waypoint, 4);
        if (lua_isnil(state, -1))
        {
            lua_pop(state, 2);
            return luaL_argerror(state, 1, "waypoint z is required");
        }
        entry.z = static_cast<float>(luaL_checknumber(state, -1));
        lua_pop(state, 1);

        lua_rawgeti(state, waypoint, 5);
        entry.actionFlag = lua_isnil(state, -1) ? 0 : static_cast<uint32>(luaL_checkinteger(state, -1));
        lua_pop(state, 1);

        lua_rawgeti(state, waypoint, 6);
        entry.delay = lua_isnil(state, -1) ? 0 : static_cast<uint32>(luaL_checkinteger(state, -1));
        lua_pop(state, 1);

        nodes.push_back(entry);
        lua_pop(state, 1);
    }

    if (nodes.size() < 2)
    {
        lua_pushnil(state);
        return 1;
    }

    if (pathId)
    {
        if (sTaxiPathStore.LookupEntry(pathId) || (pathId < sTaxiPathNodesByPath.size() && !sTaxiPathNodesByPath[pathId].empty()))
            return luaL_argerror(state, 5, "taxi path id already exists");
    }
    else
    {
        pathId = std::max<uint32>(sTaxiPathStore.GetNumRows(), static_cast<uint32>(sTaxiPathNodesByPath.size()));
        while (sTaxiPathStore.LookupEntry(pathId) || (pathId < sTaxiPathNodesByPath.size() && !sTaxiPathNodesByPath[pathId].empty()))
            ++pathId;
    }

    uint32 nextNodeId = std::max<uint32>(sObjectMgr.GetMaxTaxiNodeId(), 1);
    while (sObjectMgr.GetTaxiNodeEntry(nextNodeId))
        ++nextNodeId;

    uint32 startNode = nextNodeId;
    uint32 nodeIndex = 0;

    if (sTaxiPathNodesByPath.size() <= pathId)
        sTaxiPathNodesByPath.resize(pathId + 1);

    sTaxiPathNodesByPath[pathId].clear();
    sTaxiPathNodesByPath[pathId].resize(nodes.size());

    for (TaxiPathNodeEntry& entry : nodes)
    {
        auto taxiNode = std::make_unique<TaxiNodesEntry>();
        taxiNode->ID = nextNodeId;
        taxiNode->map_id = entry.mapid;
        taxiNode->x = entry.x;
        taxiNode->y = entry.y;
        taxiNode->z = entry.z;
        taxiNode->name[0] = "Lua Taxi Node";
        taxiNode->MountCreatureID[0] = mountH;
        taxiNode->MountCreatureID[1] = mountA;
        sObjectMgr.AddTaxiNodeEntry(std::move(taxiNode));

        entry.path = pathId;
        entry.index = nodeIndex;
        auto storedNode = std::make_unique<TaxiPathNodeEntry>(entry);
        TaxiPathNodeEntry* storedNodePtr = storedNode.get();
        g_luaTaxiPathNodes.push_back(std::move(storedNode));
        sTaxiPathNodesByPath[pathId].set(nodeIndex, storedNodePtr);

        ++nextNodeId;
        ++nodeIndex;
    }

    auto pathEntry = std::make_unique<TaxiPathEntry>();
    pathEntry->ID = pathId;
    pathEntry->from = startNode;
    pathEntry->to = nextNodeId - 1;
    pathEntry->price = price;
    TaxiPathEntry* pathEntryPtr = pathEntry.get();
    g_luaTaxiPaths.push_back(std::move(pathEntry));

    sTaxiPathStore.SetEntry(pathId, pathEntryPtr);
    sTaxiPathSetBySource[startNode][nextNodeId - 1] = TaxiPathBySourceAndDestination(pathId, price);

    lua_pushinteger(state, pathId);
    return 1;
}

int LuaAddVendorItem(lua_State* state)
{
    uint32 entry = static_cast<uint32>(luaL_checkinteger(state, 1));
    uint32 item = static_cast<uint32>(luaL_checkinteger(state, 2));
    uint32 maxCount = static_cast<uint32>(luaL_optinteger(state, 3, 0));
    uint32 incrTime = static_cast<uint32>(luaL_optinteger(state, 4, 0));
    uint32 itemFlags = static_cast<uint32>(luaL_optinteger(state, 5, 0));

    if (sObjectMgr.IsVendorItemValid(false, "lua_vendor", entry, item, maxCount, incrTime, 0))
        sObjectMgr.AddVendorItem(entry, item, maxCount, incrTime, itemFlags);

    return 0;
}

int LuaVendorRemoveItem(lua_State* state)
{
    uint32 entry = static_cast<uint32>(luaL_checkinteger(state, 1));
    uint32 item = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (!sObjectMgr.GetCreatureTemplate(entry))
        return luaL_argerror(state, 1, "valid creature entry expected");

    sObjectMgr.RemoveVendorItem(entry, item);
    return 0;
}

int LuaVendorRemoveAllItems(lua_State* state)
{
    uint32 entry = static_cast<uint32>(luaL_checkinteger(state, 1));
    VendorItemData const* items = sObjectMgr.GetNpcVendorItemList(entry);
    if (!items || items->Empty())
        return 0;

    std::vector<uint32> entries;
    entries.reserve(items->m_items.size());
    for (VendorItem const* vendorItem : items->m_items)
        if (vendorItem)
            entries.push_back(vendorItem->item);

    for (uint32 item : entries)
        sObjectMgr.RemoveVendorItem(entry, item);

    return 0;
}

int LuaGetPlayersInWorld(lua_State* state)
{
    auto* engine = GetEngine(state);
    lua_newtable(state);
    if (!engine)
        return 1;

    uint32 index = 1;
    HashMapHolder<Player>::ReadGuard guard(HashMapHolder<Player>::GetLock());
    HashMapHolder<Player>::MapType const& players = HashMapHolder<Player>::GetContainer();
    for (auto const& itr : players)
    {
        Player* player = itr.second;
        if (!player || !player->IsInWorld())
            continue;

        engine->PushPlayer(player);
        lua_rawseti(state, -2, index++);
    }

    return 1;
}

int LuaGetPlayerCount(lua_State* state)
{
    uint32 count = 0;
    HashMapHolder<Player>::ReadGuard guard(HashMapHolder<Player>::GetLock());
    HashMapHolder<Player>::MapType const& players = HashMapHolder<Player>::GetContainer();
    for (auto const& itr : players)
    {
        Player* player = itr.second;
        if (player && player->IsInWorld())
            ++count;
    }

    lua_pushinteger(state, count);
    return 1;
}

int LuaGetMapById(lua_State* state)
{
    auto* engine = GetEngine(state);
    uint32 mapId = static_cast<uint32>(luaL_checkinteger(state, 1));
    uint32 instanceId = static_cast<uint32>(luaL_optinteger(state, 2, 0));
    Map* map = sMapMgr.FindMap(mapId, instanceId);
    if (engine)
        engine->PushMap(map);
    else
        lua_pushnil(state);
    return 1;
}

int LuaPerformIngameSpawn(lua_State* state)
{
    auto* engine = GetEngine(state);
    int spawnType = static_cast<int>(luaL_checkinteger(state, 1));
    uint32 entry = static_cast<uint32>(luaL_checkinteger(state, 2));
    uint32 mapId = static_cast<uint32>(luaL_checkinteger(state, 3));
    uint32 instanceId = static_cast<uint32>(luaL_checkinteger(state, 4));
    float x = static_cast<float>(luaL_checknumber(state, 5));
    float y = static_cast<float>(luaL_checknumber(state, 6));
    float z = static_cast<float>(luaL_checknumber(state, 7));
    float o = static_cast<float>(luaL_checknumber(state, 8));
    bool save = lua_isnoneornil(state, 9) ? false : lua_toboolean(state, 9) != 0;
    uint32 durationOrRespawn = static_cast<uint32>(luaL_optinteger(state, 10, 0));
    uint32 phase = static_cast<uint32>(luaL_optinteger(state, 11, 1));

    if (!phase)
    {
        lua_pushnil(state);
        return 1;
    }

    Map* map = sMapMgr.FindMap(mapId, instanceId);
    if (!map)
    {
        lua_pushnil(state);
        return 1;
    }

    if (spawnType == 1)
    {
        Creature* creature = nullptr;
        if (save)
        {
            CreatureInfo const* cinfo = sObjectMgr.GetCreatureTemplate(entry);
            if (!cinfo)
            {
                lua_pushnil(state);
                return 1;
            }

            uint32 lowGuid = sObjectMgr.GenerateStaticCreatureLowGuid();
            if (!lowGuid)
            {
                lua_pushnil(state);
                return 1;
            }

            creature = new Creature();
            CreatureCreatePos pos(map, x, y, z, o);
            if (!creature->Create(lowGuid, pos, cinfo, entry))
            {
                delete creature;
                lua_pushnil(state);
                return 1;
            }

            if (durationOrRespawn)
                creature->SetRespawnDelay(durationOrRespawn);
            creature->SetWorldMask(phase);
            creature->SaveToDB(map->GetId());
            lowGuid = creature->GetGUIDLow();

            if (!creature->LoadFromDB(lowGuid, map))
            {
                delete creature;
                lua_pushnil(state);
                return 1;
            }

            creature->SetWorldMask(phase);
            map->Add(creature);
            sObjectMgr.AddCreatureToGrid(lowGuid, sObjectMgr.GetCreatureData(lowGuid));
        }
        else
        {
            TempSummonType summonType = durationOrRespawn ? TEMPSUMMON_TIMED_OR_DEAD_DESPAWN : TEMPSUMMON_MANUAL_DESPAWN;
            creature = map->SummonCreature(entry, x, y, z, o, summonType, durationOrRespawn);
            if (creature)
                creature->SetWorldMask(phase);
        }

        if (engine)
            engine->PushCreature(creature);
        else
            lua_pushnil(state);
        return 1;
    }

    if (spawnType == 2)
    {
        GameObjectInfo const* info = sObjectMgr.GetGameObjectInfo(entry);
        if (!info || (info->displayId && !sGameObjectDisplayInfoStore.LookupEntry(info->displayId)))
        {
            lua_pushnil(state);
            return 1;
        }

        GameObject* go = nullptr;
        if (save)
        {
            uint32 lowGuid = sObjectMgr.GenerateStaticGameObjectLowGuid();
            if (!lowGuid)
            {
                lua_pushnil(state);
                return 1;
            }

            go = new GameObject();
            if (!go->Create(lowGuid, entry, map, x, y, z, o, 0.0f, 0.0f, 0.0f, 0.0f, GO_ANIMPROGRESS_DEFAULT, GO_STATE_READY))
            {
                delete go;
                lua_pushnil(state);
                return 1;
            }

            go->SetWorldMask(phase);
            go->SetRespawnTime(durationOrRespawn ? durationOrRespawn : 300);
            go->SaveToDB(map->GetId());

            if (!go->LoadFromDB(lowGuid, map))
            {
                delete go;
                lua_pushnil(state);
                return 1;
            }

            go->SetWorldMask(phase);
            map->Add(go);
            sObjectMgr.AddGameobjectToGrid(lowGuid, sObjectMgr.GetGOData(lowGuid));
        }
        else
            go = map->SummonGameObject(entry, x, y, z, o, 0.0f, 0.0f, 0.0f, 0.0f, durationOrRespawn, phase);

        if (engine)
            engine->PushGameObject(go);
        else
            lua_pushnil(state);
        return 1;
    }

    lua_pushnil(state);
    return 1;
}

int ObjectGetEntry(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    lua_pushinteger(state, object ? object->GetEntry() : 0);
    return 1;
}

int ObjectGetGUIDLow(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    lua_pushinteger(state, object ? object->GetGUIDLow() : 0);
    return 1;
}

int ObjectGetGUID(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    PushObjectGuidValue(state, object ? object->GetObjectGuid() : ObjectGuid());
    return 1;
}

int ObjectGetTypeId(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    lua_pushinteger(state, object ? object->GetTypeId() : TYPEID_OBJECT);
    return 1;
}

int ObjectIsPlayer(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    lua_pushboolean(state, object && object->IsPlayer());
    return 1;
}

int ObjectIsCreature(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    lua_pushboolean(state, object && object->IsCreature());
    return 1;
}

int ObjectIsUnit(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    lua_pushboolean(state, object && object->IsUnit());
    return 1;
}

int ObjectIsGameObject(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    lua_pushboolean(state, object && object->IsGameObject());
    return 1;
}

int ObjectIsItem(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    lua_pushboolean(state, object && (object->GetTypeId() == TYPEID_ITEM || object->GetTypeId() == TYPEID_CONTAINER));
    return 1;
}

int ObjectIsCorpse(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    lua_pushboolean(state, object && object->IsCorpse());
    return 1;
}

int ObjectIsDynamicObject(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    lua_pushboolean(state, object && object->GetObjectGuid().IsDynamicObject());
    return 1;
}

int ObjectIsWorldObject(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    lua_pushboolean(state, object && object->IsWorldObject());
    return 1;
}

int ObjectGetScale(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    lua_pushnumber(state, object ? object->GetObjectScale() : 0.0f);
    return 1;
}

int ObjectSetScale(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    float scale = static_cast<float>(luaL_checknumber(state, 2));
    if (object)
        object->SetObjectScale(scale);
    return 0;
}

int ObjectIsInWorld(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    lua_pushboolean(state, object && object->IsInWorld());
    return 1;
}

int ObjectGetUInt32Value(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    uint16 index = static_cast<uint16>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, object ? object->GetUInt32Value(index) : 0);
    return 1;
}

int ObjectGetInt32Value(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    uint16 index = static_cast<uint16>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, object ? object->GetInt32Value(index) : 0);
    return 1;
}

int ObjectGetFloatValue(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    uint16 index = static_cast<uint16>(luaL_checkinteger(state, 2));
    lua_pushnumber(state, object ? object->GetFloatValue(index) : 0.0f);
    return 1;
}

int ObjectGetByteValue(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    uint16 index = static_cast<uint16>(luaL_checkinteger(state, 2));
    uint8 offset = static_cast<uint8>(luaL_checkinteger(state, 3));
    lua_pushinteger(state, object ? object->GetByteValue(index, offset) : 0);
    return 1;
}

int ObjectGetUInt16Value(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    uint16 index = static_cast<uint16>(luaL_checkinteger(state, 2));
    uint8 offset = static_cast<uint8>(luaL_checkinteger(state, 3));
    lua_pushinteger(state, object ? object->GetUInt16Value(index, offset) : 0);
    return 1;
}

int ObjectGetUInt64Value(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    uint16 index = static_cast<uint16>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, object ? static_cast<lua_Integer>(object->GetUInt64Value(index)) : 0);
    return 1;
}

int ObjectSetUInt32Value(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    uint16 index = static_cast<uint16>(luaL_checkinteger(state, 2));
    uint32 value = static_cast<uint32>(luaL_checkinteger(state, 3));
    if (object)
        object->SetUInt32Value(index, value);
    return 0;
}

int ObjectSetInt32Value(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    uint16 index = static_cast<uint16>(luaL_checkinteger(state, 2));
    int32 value = static_cast<int32>(luaL_checkinteger(state, 3));
    if (object)
        object->SetInt32Value(index, value);
    return 0;
}

int ObjectUpdateUInt32Value(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    uint16 index = static_cast<uint16>(luaL_checkinteger(state, 2));
    uint32 value = static_cast<uint32>(luaL_checkinteger(state, 3));
    if (object)
        object->SetUInt32Value(index, value);
    return 0;
}

int ObjectSetFloatValue(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    uint16 index = static_cast<uint16>(luaL_checkinteger(state, 2));
    float value = static_cast<float>(luaL_checknumber(state, 3));
    if (object)
        object->SetFloatValue(index, value);
    return 0;
}

int ObjectSetByteValue(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    uint16 index = static_cast<uint16>(luaL_checkinteger(state, 2));
    uint8 offset = static_cast<uint8>(luaL_checkinteger(state, 3));
    uint8 value = static_cast<uint8>(luaL_checkinteger(state, 4));
    if (object)
        object->SetByteValue(index, offset, value);
    return 0;
}

int ObjectSetUInt16Value(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    uint16 index = static_cast<uint16>(luaL_checkinteger(state, 2));
    uint8 offset = static_cast<uint8>(luaL_checkinteger(state, 3));
    uint16 value = static_cast<uint16>(luaL_checkinteger(state, 4));
    if (object)
        object->SetUInt16Value(index, offset, value);
    return 0;
}

int ObjectSetInt16Value(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    uint16 index = static_cast<uint16>(luaL_checkinteger(state, 2));
    uint8 offset = static_cast<uint8>(luaL_checkinteger(state, 3));
    int16 value = static_cast<int16>(luaL_checkinteger(state, 4));
    if (object)
        object->SetInt16Value(index, offset, value);
    return 0;
}

int ObjectSetUInt64Value(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    uint16 index = static_cast<uint16>(luaL_checkinteger(state, 2));
    uint64 value = static_cast<uint64>(luaL_checkinteger(state, 3));
    if (object)
        object->SetUInt64Value(index, value);
    return 0;
}

int ObjectHasFlag(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    uint16 index = static_cast<uint16>(luaL_checkinteger(state, 2));
    uint32 flag = static_cast<uint32>(luaL_checkinteger(state, 3));
    lua_pushboolean(state, object && object->HasFlag(index, flag));
    return 1;
}

int ObjectSetFlag(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    uint16 index = static_cast<uint16>(luaL_checkinteger(state, 2));
    uint32 flag = static_cast<uint32>(luaL_checkinteger(state, 3));
    if (object)
        object->SetFlag(index, flag);
    return 0;
}

int ObjectRemoveFlag(lua_State* state)
{
    Object* object = CheckObject(state, 1);
    uint16 index = static_cast<uint16>(luaL_checkinteger(state, 2));
    uint32 flag = static_cast<uint32>(luaL_checkinteger(state, 3));
    if (object)
        object->RemoveFlag(index, flag);
    return 0;
}

int ObjectToPlayer(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    Object* object = CheckObject(state, 1);
    if (engine && object)
        engine->PushPlayer(object->ToPlayer());
    else
        lua_pushnil(state);
    return 1;
}

int ObjectToCreature(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    Object* object = CheckObject(state, 1);
    if (engine && object)
        engine->PushCreature(object->ToCreature());
    else
        lua_pushnil(state);
    return 1;
}

int ObjectToUnit(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    Object* object = CheckObject(state, 1);
    if (engine && object)
        engine->PushUnit(object->ToUnit());
    else
        lua_pushnil(state);
    return 1;
}

int ObjectToGameObject(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    Object* object = CheckObject(state, 1);
    if (engine && object)
        engine->PushGameObject(object->ToGameObject());
    else
        lua_pushnil(state);
    return 1;
}

int ObjectToCorpse(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    Object* object = CheckObject(state, 1);
    if (engine && object)
        engine->PushCorpse(object->ToCorpse());
    else
        lua_pushnil(state);
    return 1;
}

int ObjectToDynamicObject(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    Object* object = CheckObject(state, 1);
    if (engine && object)
        engine->PushDynamicObject(dynamic_cast<DynamicObject*>(object));
    else
        lua_pushnil(state);
    return 1;
}

int WorldObjectGetMapId(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    lua_pushinteger(state, object ? object->GetMapId() : 0);
    return 1;
}

int WorldObjectGetMap(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    WorldObject* object = CheckWorldObject(state, 1);
    if (engine && object && object->IsInWorld())
        engine->PushMap(object->GetMap());
    else
        lua_pushnil(state);
    return 1;
}

int WorldObjectGetPhaseMask(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    lua_pushinteger(state, object ? object->GetWorldMask() : 0);
    return 1;
}

int WorldObjectSetPhaseMask(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    uint32 phaseMask = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (object)
        object->SetWorldMask(phaseMask);
    return 0;
}

int WorldObjectGetInstanceId(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    lua_pushinteger(state, object ? object->GetInstanceId() : 0);
    return 1;
}

int WorldObjectGetZoneId(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    lua_pushinteger(state, object ? object->GetZoneId() : 0);
    return 1;
}

int WorldObjectGetAreaId(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    lua_pushinteger(state, object ? object->GetAreaId() : 0);
    return 1;
}

int WorldObjectGetX(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    lua_pushnumber(state, object ? object->GetPositionX() : 0.0f);
    return 1;
}

int WorldObjectGetY(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    lua_pushnumber(state, object ? object->GetPositionY() : 0.0f);
    return 1;
}

int WorldObjectGetZ(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    lua_pushnumber(state, object ? object->GetPositionZ() : 0.0f);
    return 1;
}

int WorldObjectGetO(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    lua_pushnumber(state, object ? object->GetOrientation() : 0.0f);
    return 1;
}

int WorldObjectGetLocation(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    lua_pushnumber(state, object ? object->GetPositionX() : 0.0f);
    lua_pushnumber(state, object ? object->GetPositionY() : 0.0f);
    lua_pushnumber(state, object ? object->GetPositionZ() : 0.0f);
    lua_pushnumber(state, object ? object->GetOrientation() : 0.0f);
    return 4;
}

int WorldObjectGetDistance(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    if (IsWorldObjectValue(state, 2))
    {
        WorldObject* other = CheckWorldObject(state, 2);
        lua_pushnumber(state, object && other ? object->GetDistance(other) : 0.0f);
        return 1;
    }

    float x = static_cast<float>(luaL_checknumber(state, 2));
    float y = static_cast<float>(luaL_checknumber(state, 3));
    float z = static_cast<float>(luaL_checknumber(state, 4));
    lua_pushnumber(state, object ? object->GetDistance(x, y, z) : 0.0f);
    return 1;
}

int WorldObjectGetDistance2d(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);

    if (IsWorldObjectValue(state, 2))
    {
        WorldObject* other = CheckWorldObject(state, 2);
        lua_pushnumber(state, object && other ? object->GetDistance2d(other) : 0.0f);
        return 1;
    }

    float x = static_cast<float>(luaL_checknumber(state, 2));
    float y = static_cast<float>(luaL_checknumber(state, 3));
    lua_pushnumber(state, object ? object->GetDistance2d(x, y) : 0.0f);
    return 1;
}

int WorldObjectGetExactDistance(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    if (IsWorldObjectValue(state, 2))
    {
        WorldObject* other = CheckWorldObject(state, 2);
        lua_pushnumber(state, object && other ? object->GetDistance3dToCenter(other) : 0.0f);
        return 1;
    }

    float x = static_cast<float>(luaL_checknumber(state, 2));
    float y = static_cast<float>(luaL_checknumber(state, 3));
    float z = static_cast<float>(luaL_checknumber(state, 4));
    lua_pushnumber(state, object ? object->GetDistance3dToCenter(x, y, z) : 0.0f);
    return 1;
}

int WorldObjectGetExactDistance2d(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    if (IsWorldObjectValue(state, 2))
    {
        WorldObject* other = CheckWorldObject(state, 2);
        lua_pushnumber(state, object && other ? object->GetDistance2dToCenter(other) : 0.0f);
        return 1;
    }

    float x = static_cast<float>(luaL_checknumber(state, 2));
    float y = static_cast<float>(luaL_checknumber(state, 3));
    lua_pushnumber(state, object ? object->GetDistance2dToCenter(x, y) : 0.0f);
    return 1;
}

int WorldObjectGetRelativePoint(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    float distance = static_cast<float>(luaL_checknumber(state, 2));
    float angle = static_cast<float>(luaL_checknumber(state, 3));

    float x = object ? object->GetPositionX() : 0.0f;
    float y = object ? object->GetPositionY() : 0.0f;
    float z = object ? object->GetPositionZ() : 0.0f;
    if (object)
        object->GetClosePoint(x, y, z, 0.0f, distance, angle);

    lua_pushnumber(state, x);
    lua_pushnumber(state, y);
    lua_pushnumber(state, z);
    return 3;
}

int WorldObjectGetAngle(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    if (IsWorldObjectValue(state, 2))
    {
        WorldObject* other = CheckWorldObject(state, 2);
        lua_pushnumber(state, object && other ? object->GetAngle(other) : 0.0f);
        return 1;
    }

    float x = static_cast<float>(luaL_checknumber(state, 2));
    float y = static_cast<float>(luaL_checknumber(state, 3));
    lua_pushnumber(state, object ? object->GetAngle(x, y) : 0.0f);
    return 1;
}

int WorldObjectIsWithinDist(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    WorldObject* other = CheckWorldObject(state, 2);
    float distance = static_cast<float>(luaL_checknumber(state, 3));
    bool is3D = lua_isnoneornil(state, 4) ? true : lua_toboolean(state, 4) != 0;
    lua_pushboolean(state, object && other && object->IsWithinDist(other, distance, is3D));
    return 1;
}

int WorldObjectIsWithinDist3d(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    float x = static_cast<float>(luaL_checknumber(state, 2));
    float y = static_cast<float>(luaL_checknumber(state, 3));
    float z = static_cast<float>(luaL_checknumber(state, 4));
    float distance = static_cast<float>(luaL_checknumber(state, 5));
    lua_pushboolean(state, object && object->IsWithinDist3d(x, y, z, distance));
    return 1;
}

int WorldObjectIsWithinDist2d(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    float x = static_cast<float>(luaL_checknumber(state, 2));
    float y = static_cast<float>(luaL_checknumber(state, 3));
    float distance = static_cast<float>(luaL_checknumber(state, 4));
    lua_pushboolean(state, object && object->IsWithinDist2d(x, y, distance));
    return 1;
}

int WorldObjectIsWithinDistInMap(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    WorldObject* other = CheckWorldObject(state, 2);
    float distance = static_cast<float>(luaL_checknumber(state, 3));
    bool is3D = lua_isnoneornil(state, 4) ? true : lua_toboolean(state, 4) != 0;
    lua_pushboolean(state, object && other && object->IsWithinDistInMap(other, distance, is3D));
    return 1;
}

int WorldObjectIsInMap(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    WorldObject* other = CheckWorldObject(state, 2);
    lua_pushboolean(state, object && other && object->IsInMap(other));
    return 1;
}

int WorldObjectIsInRange(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    WorldObject* other = CheckWorldObject(state, 2);
    float minRange = static_cast<float>(luaL_checknumber(state, 3));
    float maxRange = static_cast<float>(luaL_checknumber(state, 4));
    bool is3D = lua_isnoneornil(state, 5) ? true : lua_toboolean(state, 5) != 0;
    lua_pushboolean(state, object && other && object->IsInRange(other, minRange, maxRange, is3D));
    return 1;
}

int WorldObjectIsInRange2d(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    float x = static_cast<float>(luaL_checknumber(state, 2));
    float y = static_cast<float>(luaL_checknumber(state, 3));
    float minRange = static_cast<float>(luaL_checknumber(state, 4));
    float maxRange = static_cast<float>(luaL_checknumber(state, 5));
    lua_pushboolean(state, object && object->IsInRange2d(x, y, minRange, maxRange));
    return 1;
}

int WorldObjectIsInRange3d(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    float x = static_cast<float>(luaL_checknumber(state, 2));
    float y = static_cast<float>(luaL_checknumber(state, 3));
    float z = static_cast<float>(luaL_checknumber(state, 4));
    float minRange = static_cast<float>(luaL_checknumber(state, 5));
    float maxRange = static_cast<float>(luaL_checknumber(state, 6));
    lua_pushboolean(state, object && object->IsInRange3d(x, y, z, minRange, maxRange));
    return 1;
}

int WorldObjectIsInFront(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    WorldObject* other = CheckWorldObject(state, 2);
    float arc = static_cast<float>(luaL_optnumber(state, 3, M_PI_F));
    lua_pushboolean(state, object && other && object->HasInArc(other, arc));
    return 1;
}

int WorldObjectIsInBack(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    WorldObject* other = CheckWorldObject(state, 2);
    float arc = static_cast<float>(luaL_optnumber(state, 3, M_PI_F));
    lua_pushboolean(state, object && other && object->HasInArc(other, arc, M_PI_F));
    return 1;
}

int WorldObjectIsWithinLOS(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    WorldObject* other = CheckWorldObject(state, 2);
    lua_pushboolean(state, object && other && object->IsWithinLOSInMap(other));
    return 1;
}

int WorldObjectIsFriendlyTo(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    WorldObject* other = CheckWorldObject(state, 2);
    lua_pushboolean(state, object && other && object->IsFriendlyTo(other));
    return 1;
}

int WorldObjectIsHostileTo(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    WorldObject* other = CheckWorldObject(state, 2);
    lua_pushboolean(state, object && other && object->IsHostileTo(other));
    return 1;
}

int WorldObjectGetPlayersInRange(lua_State* state)
{
    auto* engine = GetEngine(state);
    WorldObject* object = CheckWorldObject(state, 1);
    float range = static_cast<float>(luaL_optnumber(state, 2, SIZE_OF_GRIDS));
    uint32 hostile = static_cast<uint32>(luaL_optinteger(state, 3, 0));
    uint32 dead = static_cast<uint32>(luaL_optinteger(state, 4, 1));

    std::list<Player*> players;
    GetPlayersInRange(object, range, hostile, dead, players);

    lua_newtable(state);
    uint32 index = 1;
    for (Player* player : players)
    {
        if (engine)
            engine->PushPlayer(player);
        else
            lua_pushnil(state);
        lua_rawseti(state, -2, index++);
    }

    return 1;
}

int WorldObjectGetCreaturesInRange(lua_State* state)
{
    auto* engine = GetEngine(state);
    WorldObject* object = CheckWorldObject(state, 1);
    float range = static_cast<float>(luaL_optnumber(state, 2, SIZE_OF_GRIDS));
    uint32 entry = static_cast<uint32>(luaL_optinteger(state, 3, 0));
    uint32 hostile = static_cast<uint32>(luaL_optinteger(state, 4, 0));
    uint32 dead = static_cast<uint32>(luaL_optinteger(state, 5, 1));

    std::list<Creature*> creatures;
    GetCreaturesInRange(object, range, entry, hostile, dead, creatures);

    lua_newtable(state);
    uint32 index = 1;
    for (Creature* creature : creatures)
    {
        if (engine)
            engine->PushCreature(creature);
        else
            lua_pushnil(state);
        lua_rawseti(state, -2, index++);
    }

    return 1;
}

int WorldObjectGetGameObjectsInRange(lua_State* state)
{
    auto* engine = GetEngine(state);
    WorldObject* object = CheckWorldObject(state, 1);
    float range = static_cast<float>(luaL_optnumber(state, 2, SIZE_OF_GRIDS));
    uint32 entry = static_cast<uint32>(luaL_optinteger(state, 3, 0));
    uint32 hostile = static_cast<uint32>(luaL_optinteger(state, 4, 0));

    std::list<GameObject*> gameObjects;
    GetGameObjectsInRange(object, range, entry, hostile, gameObjects);

    lua_newtable(state);
    uint32 index = 1;
    for (GameObject* go : gameObjects)
    {
        if (engine)
            engine->PushGameObject(go);
        else
            lua_pushnil(state);
        lua_rawseti(state, -2, index++);
    }

    return 1;
}

int WorldObjectGetNearestPlayer(lua_State* state)
{
    auto* engine = GetEngine(state);
    WorldObject* object = CheckWorldObject(state, 1);
    float range = static_cast<float>(luaL_optnumber(state, 2, SIZE_OF_GRIDS));
    uint32 hostile = static_cast<uint32>(luaL_optinteger(state, 3, 0));
    uint32 dead = static_cast<uint32>(luaL_optinteger(state, 4, 1));

    std::list<Player*> players;
    GetPlayersInRange(object, range, hostile, dead, players);
    Player* player = object ? SelectNearestPlayer(object, players) : nullptr;
    if (engine)
        engine->PushPlayer(player);
    else
        lua_pushnil(state);
    return 1;
}

int WorldObjectGetNearestCreature(lua_State* state)
{
    auto* engine = GetEngine(state);
    WorldObject* object = CheckWorldObject(state, 1);
    float range = static_cast<float>(luaL_optnumber(state, 2, SIZE_OF_GRIDS));
    uint32 entry = static_cast<uint32>(luaL_optinteger(state, 3, 0));
    uint32 hostile = static_cast<uint32>(luaL_optinteger(state, 4, 0));
    uint32 dead = static_cast<uint32>(luaL_optinteger(state, 5, 1));

    std::list<Creature*> creatures;
    GetCreaturesInRange(object, range, entry, hostile, dead, creatures);
    Creature* creature = object ? SelectNearestCreature(object, creatures) : nullptr;
    if (engine)
        engine->PushCreature(creature);
    else
        lua_pushnil(state);
    return 1;
}

int WorldObjectGetNearestGameObject(lua_State* state)
{
    auto* engine = GetEngine(state);
    WorldObject* object = CheckWorldObject(state, 1);
    float range = static_cast<float>(luaL_optnumber(state, 2, SIZE_OF_GRIDS));
    uint32 entry = static_cast<uint32>(luaL_optinteger(state, 3, 0));
    uint32 hostile = static_cast<uint32>(luaL_optinteger(state, 4, 0));

    std::list<GameObject*> gameObjects;
    GetGameObjectsInRange(object, range, entry, hostile, gameObjects);
    GameObject* go = object ? SelectNearestGameObject(object, gameObjects) : nullptr;
    if (engine)
        engine->PushGameObject(go);
    else
        lua_pushnil(state);
    return 1;
}

int WorldObjectFindNearestPlayer(lua_State* state)
{
    auto* engine = GetEngine(state);
    WorldObject* object = CheckWorldObject(state, 1);
    float range = static_cast<float>(luaL_optnumber(state, 2, 50.0f));
    Player* player = object ? object->FindNearestPlayer(range) : nullptr;
    if (engine)
        engine->PushPlayer(player);
    else
        lua_pushnil(state);
    return 1;
}

int WorldObjectFindNearestCreature(lua_State* state)
{
    auto* engine = GetEngine(state);
    WorldObject* object = CheckWorldObject(state, 1);
    uint32 entry = static_cast<uint32>(luaL_optinteger(state, 2, 0));
    float range = static_cast<float>(luaL_optnumber(state, 3, 50.0f));
    bool alive = lua_isnoneornil(state, 4) ? true : lua_toboolean(state, 4) != 0;
    Creature* creature = object ? object->FindNearestCreature(entry, range, alive) : nullptr;
    if (engine)
        engine->PushCreature(creature);
    else
        lua_pushnil(state);
    return 1;
}

int WorldObjectFindNearestGameObject(lua_State* state)
{
    auto* engine = GetEngine(state);
    WorldObject* object = CheckWorldObject(state, 1);
    uint32 entry = static_cast<uint32>(luaL_optinteger(state, 2, 0));
    float range = static_cast<float>(luaL_optnumber(state, 3, 50.0f));
    GameObject* go = object ? object->FindNearestGameObject(entry, range) : nullptr;
    if (engine)
        engine->PushGameObject(go);
    else
        lua_pushnil(state);
    return 1;
}

void CollectNearWorldObjects(WorldObject* object, float range, uint16 type, uint32 entry, uint32 hostile, uint32 dead, std::vector<WorldObject*>& objects)
{
    if (!object)
        return;

    bool anyType = type == 0;
    if ((anyType || (type & TYPEMASK_UNIT) || (type & TYPEMASK_PLAYER)) && entry == 0)
    {
        std::list<Player*> players;
        GetPlayersInRange(object, range, hostile, dead, players);
        objects.insert(objects.end(), players.begin(), players.end());
    }

    if (anyType || (type & TYPEMASK_UNIT))
    {
        std::list<Creature*> creatures;
        GetCreaturesInRange(object, range, entry, hostile, dead, creatures);
        objects.insert(objects.end(), creatures.begin(), creatures.end());
    }

    if (anyType || (type & TYPEMASK_GAMEOBJECT))
    {
        std::list<GameObject*> gameObjects;
        GetGameObjectsInRange(object, range, entry, hostile, gameObjects);
        objects.insert(objects.end(), gameObjects.begin(), gameObjects.end());
    }
}

int WorldObjectGetNearObject(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    WorldObject* object = CheckWorldObject(state, 1);
    float range = static_cast<float>(luaL_optnumber(state, 2, SIZE_OF_GRIDS));
    uint16 type = static_cast<uint16>(luaL_optinteger(state, 3, 0));
    uint32 entry = static_cast<uint32>(luaL_optinteger(state, 4, 0));
    uint32 hostile = static_cast<uint32>(luaL_optinteger(state, 5, 0));
    uint32 dead = static_cast<uint32>(luaL_optinteger(state, 6, 1));

    std::vector<WorldObject*> objects;
    CollectNearWorldObjects(object, range, type, entry, hostile, dead, objects);

    WorldObject* nearest = nullptr;
    float nearestDistance = 0.0f;
    for (WorldObject* candidate : objects)
    {
        if (candidate == object)
            continue;

        float distance = object->GetDistance(candidate);
        if (!nearest || distance < nearestDistance)
        {
            nearest = candidate;
            nearestDistance = distance;
        }
    }

    PushWorldObjectValue(state, engine, nearest);
    return 1;
}

int WorldObjectGetNearObjects(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    WorldObject* object = CheckWorldObject(state, 1);
    float range = static_cast<float>(luaL_optnumber(state, 2, SIZE_OF_GRIDS));
    uint16 type = static_cast<uint16>(luaL_optinteger(state, 3, 0));
    uint32 entry = static_cast<uint32>(luaL_optinteger(state, 4, 0));
    uint32 hostile = static_cast<uint32>(luaL_optinteger(state, 5, 0));
    uint32 dead = static_cast<uint32>(luaL_optinteger(state, 6, 1));

    std::vector<WorldObject*> objects;
    CollectNearWorldObjects(object, range, type, entry, hostile, dead, objects);

    lua_newtable(state);
    uint32 index = 1;
    for (WorldObject* candidate : objects)
    {
        if (candidate == object)
            continue;

        PushWorldObjectValue(state, engine, candidate);
        lua_rawseti(state, -2, index++);
    }

    return 1;
}

int WorldObjectRegisterEvent(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    luaL_checktype(state, 2, LUA_TFUNCTION);

    uint32 minDelay = 0;
    uint32 maxDelay = 0;
    if (lua_istable(state, 3))
    {
        lua_rawgeti(state, 3, 1);
        minDelay = static_cast<uint32>(luaL_checkinteger(state, -1));
        lua_rawgeti(state, 3, 2);
        maxDelay = static_cast<uint32>(luaL_checkinteger(state, -1));
        lua_pop(state, 2);
    }
    else
        minDelay = maxDelay = static_cast<uint32>(luaL_checkinteger(state, 3));

    if (minDelay > maxDelay)
        return luaL_argerror(state, 3, "min is bigger than max delay");

    uint32 repeats = static_cast<uint32>(luaL_optinteger(state, 4, 1));
    TurtleLuaEngine* engine = GetEngine(state);
    if (!engine || !object || !object->IsInWorld())
    {
        lua_pushnil(state);
        return 1;
    }

    lua_pushvalue(state, 2);
    int functionRef = luaL_ref(state, LUA_REGISTRYINDEX);
    lua_pushinteger(state, engine->CreateTimedEvent(functionRef, minDelay, maxDelay, repeats, object));
    return 1;
}

int WorldObjectRemoveEventById(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    uint32 eventId = static_cast<uint32>(luaL_checkinteger(state, 2));
    TurtleLuaEngine* engine = GetEngine(state);
    if (engine && object)
        engine->RemoveTimedEventForObject(eventId, object->GetObjectGuid());
    return 0;
}

int WorldObjectRemoveEvents(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    TurtleLuaEngine* engine = GetEngine(state);
    if (engine && object)
        engine->RemoveTimedEventsForObject(object->GetObjectGuid());
    return 0;
}

int WorldObjectSendPacket(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    WorldPacket* packet = CheckWorldPacket(state, 2);
    if (object && packet)
        object->SendMessageToSet(packet, true);
    return 0;
}

int WorldObjectSummonGameObject(lua_State* state)
{
    auto* engine = GetEngine(state);
    WorldObject* object = CheckWorldObject(state, 1);
    uint32 entry = static_cast<uint32>(luaL_checkinteger(state, 2));
    float x = static_cast<float>(luaL_optnumber(state, 3, object ? object->GetPositionX() : 0.0f));
    float y = static_cast<float>(luaL_optnumber(state, 4, object ? object->GetPositionY() : 0.0f));
    float z = static_cast<float>(luaL_optnumber(state, 5, object ? object->GetPositionZ() : 0.0f));
    float o = static_cast<float>(luaL_optnumber(state, 6, object ? object->GetOrientation() : 0.0f));
    uint32 respawnTime = static_cast<uint32>(luaL_optinteger(state, 7, 25000));

    GameObject* go = object ? object->SummonGameObject(entry, x, y, z, o, 0.0f, 0.0f, 0.0f, 0.0f, respawnTime) : nullptr;
    if (engine)
        engine->PushGameObject(go);
    else
        lua_pushnil(state);

    return 1;
}

int WorldObjectPlayMusic(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    uint32 musicId = static_cast<uint32>(luaL_checkinteger(state, 2));
    Player* target = CheckOptionalPlayer(state, 3);
    if (object)
        object->PlayDirectMusic(musicId, target);
    return 0;
}

int WorldObjectPlayDirectSound(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    uint32 soundId = static_cast<uint32>(luaL_checkinteger(state, 2));
    Player* target = CheckOptionalPlayer(state, 3);
    if (object && sObjectMgr.GetSoundEntry(soundId))
        object->PlayDirectSound(soundId, target);
    return 0;
}

int WorldObjectPlayDistanceSound(lua_State* state)
{
    WorldObject* object = CheckWorldObject(state, 1);
    uint32 soundId = static_cast<uint32>(luaL_checkinteger(state, 2));
    Player* target = CheckOptionalPlayer(state, 3);
    if (object && sObjectMgr.GetSoundEntry(soundId))
        object->PlayDistanceSound(soundId, target);
    return 0;
}

int PlayerGetName(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushstring(state, player ? player->GetName() : "");
    return 1;
}

int PlayerGetGUIDLow(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player ? player->GetGUIDLow() : 0);
    return 1;
}

int PlayerGetLevel(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player ? player->GetLevel() : 0);
    return 1;
}

int PlayerGetAccountId(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player && player->GetSession() ? player->GetSession()->GetAccountId() : 0);
    return 1;
}

int PlayerGetMapId(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player ? player->GetMapId() : 0);
    return 1;
}

int PlayerGetZoneId(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player ? player->GetZoneId() : 0);
    return 1;
}

int PlayerGetX(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushnumber(state, player ? player->GetPositionX() : 0.0f);
    return 1;
}

int PlayerGetY(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushnumber(state, player ? player->GetPositionY() : 0.0f);
    return 1;
}

int PlayerGetZ(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushnumber(state, player ? player->GetPositionZ() : 0.0f);
    return 1;
}

int PlayerGetO(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushnumber(state, player ? player->GetOrientation() : 0.0f);
    return 1;
}

int PlayerIsAlive(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushboolean(state, player && player->IsAlive());
    return 1;
}

int PlayerSendBroadcastMessage(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    char const* message = luaL_checkstring(state, 2);

    if (player && player->GetSession())
        ChatHandler(player->GetSession()).SendSysMessage(message);

    return 0;
}

int PlayerSendNotification(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    char const* message = luaL_checkstring(state, 2);

    if (player && player->GetSession())
        player->GetSession()->SendNotification("%s", message);

    return 0;
}

int PlayerSendAreaTriggerMessage(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    char const* message = luaL_checkstring(state, 2);

    if (player && player->GetSession())
        player->GetSession()->SendAreaTriggerMessage("%s", message);

    return 0;
}

int PlayerSendAddonMessage(lua_State* state)
{
    Player* sender = CheckPlayer(state, 1);
    char const* prefix = luaL_checkstring(state, 2);
    char const* message = luaL_checkstring(state, 3);

    Player* receiver = nullptr;
    if (luaL_testudata(state, 5, PLAYER_METATABLE))
        receiver = CheckPlayer(state, 5);
    else if (luaL_testudata(state, 4, PLAYER_METATABLE))
        receiver = CheckPlayer(state, 4);

    if (!sender || !sender->GetSession())
        return 0;

    if (receiver && receiver->GetSession())
        receiver->SendAddonMessage(prefix, message, sender);
    else
        sender->SendAddonMessage(prefix, message);

    return 0;
}

int PlayerSendCinematicStart(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 cinematicId = static_cast<uint32>(luaL_checkinteger(state, 2));

    if (player && player->GetSession())
        player->SendCinematicStart(cinematicId);

    return 0;
}

int PlayerSendUpdateWorldState(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 field = static_cast<uint32>(luaL_checkinteger(state, 2));
    uint32 value = static_cast<uint32>(luaL_checkinteger(state, 3));

    if (player)
        player->SendUpdateWorldState(field, value);

    return 0;
}

int PlayerSay(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    char const* text = luaL_checkstring(state, 2);
    uint32 language = static_cast<uint32>(luaL_optinteger(state, 3, LANG_UNIVERSAL));

    if (player)
        player->Say(text, language);

    return 0;
}

int PlayerYell(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    char const* text = luaL_checkstring(state, 2);
    uint32 language = static_cast<uint32>(luaL_optinteger(state, 3, LANG_UNIVERSAL));

    if (player)
        player->Yell(text, language);

    return 0;
}

int PlayerTextEmote(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    char const* text = luaL_checkstring(state, 2);

    if (player)
        player->TextEmote(text);

    return 0;
}

int PlayerHasSpellCooldown(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 spellId = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushboolean(state, player && player->HasSpellCooldown(spellId));
    return 1;
}

int PlayerGetSpellCooldownDelay(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 spellId = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, player ? player->GetSpellCooldownDelay(spellId) : 0);
    return 1;
}

int PlayerResetSpellCooldown(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 spellId = static_cast<uint32>(luaL_checkinteger(state, 2));
    bool update = lua_isnoneornil(state, 3) ? true : lua_toboolean(state, 3) != 0;

    if (player)
        player->RemoveSpellCooldown(spellId, update);

    return 0;
}

int PlayerModifyMoney(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    int32 amount = static_cast<int32>(luaL_checkinteger(state, 2));

    if (player)
        player->ModifyMoney(amount);

    return 0;
}

int PlayerAddItem(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 itemId = static_cast<uint32>(luaL_checkinteger(state, 2));
    uint32 count = static_cast<uint32>(luaL_optinteger(state, 3, 1));

    Item* item = player ? player->AddItem(itemId, count) : nullptr;
    lua_pushboolean(state, item != nullptr);
    return 1;
}

int PlayerTeleport(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 mapId = static_cast<uint32>(luaL_checkinteger(state, 2));
    float x = static_cast<float>(luaL_checknumber(state, 3));
    float y = static_cast<float>(luaL_checknumber(state, 4));
    float z = static_cast<float>(luaL_checknumber(state, 5));
    float o = static_cast<float>(luaL_optnumber(state, 6, 0.0));

    lua_pushboolean(state, player && player->TeleportTo(mapId, x, y, z, o));
    return 1;
}

int PlayerGetMoney(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player ? player->GetMoney() : 0);
    return 1;
}

int PlayerGetCoinage(lua_State* state)
{
    return PlayerGetMoney(state);
}

int PlayerSetMoney(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 money = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (player)
        player->SetMoney(money);
    return 0;
}

int PlayerGetXP(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player ? player->GetUInt32Value(PLAYER_XP) : 0);
    return 1;
}

int PlayerSetXP(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 xp = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (player)
        player->SetUInt32Value(PLAYER_XP, xp);
    return 0;
}

int PlayerGiveXP(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 xp = static_cast<uint32>(luaL_checkinteger(state, 2));
    Unit* victim = lua_isnoneornil(state, 3) ? nullptr : CheckUnit(state, 3);
    if (player)
        player->GiveXP(xp, victim);
    return 0;
}

int PlayerIsHorde(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushboolean(state, player && player->GetTeamId() == TEAM_HORDE);
    return 1;
}

int PlayerIsAlliance(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushboolean(state, player && player->GetTeamId() == TEAM_ALLIANCE);
    return 1;
}

int PlayerGetTeam(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player ? player->GetTeamId() : 0);
    return 1;
}

int PlayerGetTeamId(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player ? player->GetTeamId() : 0);
    return 1;
}

int PlayerGetGuildId(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player ? player->GetGuildId() : 0);
    return 1;
}

int PlayerGetGuildRank(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player ? player->GetRank() : 0);
    return 1;
}

int PlayerIsInGroup(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushboolean(state, player && player->GetGroup());
    return 1;
}

int PlayerIsInGuild(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushboolean(state, player && player->GetGuildId() != 0);
    return 1;
}

int PlayerIsGroupVisibleFor(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    Player* target = CheckPlayer(state, 2);
    lua_pushboolean(state, player && target && player->IsGroupVisibleFor(target));
    return 1;
}

int PlayerIsInSameGroupWith(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    Player* target = CheckPlayer(state, 2);
    lua_pushboolean(state, player && target && player->IsInSameGroupWith(target));
    return 1;
}

int PlayerIsInSameRaidWith(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    Player* target = CheckPlayer(state, 2);
    lua_pushboolean(state, player && target && player->IsInSameRaidWith(target));
    return 1;
}

int PlayerIsVisibleForPlayer(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    Player* target = CheckPlayer(state, 2);
    lua_pushboolean(state, player && target && player->IsVisibleGloballyFor(target));
    return 1;
}

int PlayerIsHonorOrXPTarget(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    Unit* target = CheckUnit(state, 2);
    lua_pushboolean(state, player && target && player->IsHonorOrXPTarget(target));
    return 1;
}

int PlayerGetGroup(lua_State* state)
{
    auto* engine = GetEngine(state);
    Player* player = CheckPlayer(state, 1);
    Group* group = player ? player->GetGroup() : nullptr;
    if (engine)
        engine->PushGroup(group);
    else
        lua_pushnil(state);
    return 1;
}

int PlayerGetOriginalGroup(lua_State* state)
{
    auto* engine = GetEngine(state);
    Player* player = CheckPlayer(state, 1);
    Group* group = player ? player->GetOriginalGroup() : nullptr;
    if (engine)
        engine->PushGroup(group);
    else
        lua_pushnil(state);
    return 1;
}

int PlayerGetGroupInvite(lua_State* state)
{
    auto* engine = GetEngine(state);
    Player* player = CheckPlayer(state, 1);
    Group* group = player ? player->GetGroupInvite() : nullptr;
    if (engine)
        engine->PushGroup(group);
    else
        lua_pushnil(state);
    return 1;
}

int PlayerGetSubGroup(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player ? player->GetSubGroup() : 0);
    return 1;
}

int PlayerGetOriginalSubGroup(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player ? player->GetOriginalSubGroup() : 0);
    return 1;
}

int PlayerGetTrader(lua_State* state)
{
    auto* engine = GetEngine(state);
    Player* player = CheckPlayer(state, 1);
    Player* trader = player ? player->GetTrader() : nullptr;
    if (engine)
        engine->PushPlayer(trader);
    else
        lua_pushnil(state);
    return 1;
}

int PlayerGetInGameTime(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player ? player->GetInGameTime() : 0);
    return 1;
}

int PlayerGetSpells(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_newtable(state);

    if (!player)
        return 1;

    uint32 index = 1;
    PlayerSpellMap const& spellMap = player->GetSpellMap();
    for (auto const& spellPair : spellMap)
    {
        if (spellPair.second.state == PLAYERSPELL_REMOVED)
            continue;

        if (!sSpellMgr.GetSpellEntry(spellPair.first))
            continue;

        lua_pushinteger(state, spellPair.first);
        lua_rawseti(state, -2, index++);
    }

    return 1;
}

int PlayerAdvanceSkillsToMax(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    if (player)
        player->UpdateSkillsToMaxSkillsForLevel();
    return 0;
}

int PlayerAdvanceSkill(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 skillId = static_cast<uint32>(luaL_checkinteger(state, 2));
    uint32 step = static_cast<uint32>(luaL_checkinteger(state, 3));

    if (player && skillId && step && player->HasSkill(skillId))
        player->UpdateSkill(skillId, step);

    return 0;
}

int PlayerAdvanceAllSkills(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 step = static_cast<uint32>(luaL_checkinteger(state, 2));

    if (!player || !step)
        return 0;

    for (uint32 i = 0; i < sSkillLineStore.GetNumRows(); ++i)
    {
        SkillLineEntry const* entry = sSkillLineStore.LookupEntry(i);
        if (!entry)
            continue;

        if (entry->categoryId == SKILL_CATEGORY_LANGUAGES || entry->categoryId == SKILL_CATEGORY_GENERIC)
            continue;

        if (player->HasSkill(entry->id))
            player->UpdateSkill(entry->id, step);
    }

    return 0;
}

int PlayerAddComboPoints(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    Unit* target = CheckUnit(state, 2);
    int8 count = static_cast<int8>(luaL_checkinteger(state, 3));

    if (player && target)
        player->AddComboPoints(target, count);

    return 0;
}

int PlayerClearComboPoints(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    if (player)
        player->ClearComboPoints();
    return 0;
}

int PlayerLearnTalent(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 talentId = static_cast<uint32>(luaL_checkinteger(state, 2));
    uint32 talentRank = static_cast<uint32>(luaL_checkinteger(state, 3));

    if (player)
        player->LearnTalent(talentId, talentRank);

    return 0;
}

int PlayerResetTalents(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    bool noCost = lua_isnoneornil(state, 2) ? true : lua_toboolean(state, 2) != 0;
    lua_pushboolean(state, player && player->ResetTalents(noCost));
    return 1;
}

int PlayerResetAllCooldowns(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    if (player)
        player->RemoveAllSpellCooldown();
    return 0;
}

int PlayerRemoveArenaSpellCooldowns(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    if (player)
        player->RemoveAllArenaSpellCooldown();
    return 0;
}

int PlayerIsImmuneToDamage(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    bool immune = player && (player->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE) || player->IsImmuneToDamage(SPELL_SCHOOL_MASK_ALL));
    lua_pushboolean(state, immune);
    return 1;
}

int PlayerSetBindPoint(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    float x = static_cast<float>(luaL_checknumber(state, 2));
    float y = static_cast<float>(luaL_checknumber(state, 3));
    float z = static_cast<float>(luaL_checknumber(state, 4));
    uint32 mapId = static_cast<uint32>(luaL_checkinteger(state, 5));
    uint32 areaId = static_cast<uint32>(luaL_checkinteger(state, 6));

    if (player)
        player->SetHomebindToLocation(WorldLocation(mapId, x, y, z), areaId);

    return 0;
}

int PlayerGetHomebind(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_newtable(state);

    lua_pushinteger(state, player ? player->GetHomeBindMap() : 0);
    lua_setfield(state, -2, "mapId");

    lua_pushinteger(state, player ? player->GetHomeBindAreaId() : 0);
    lua_setfield(state, -2, "areaId");

    return 1;
}

int PlayerCanCompleteRepeatableQuest(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    Quest const* quest = CheckQuestTemplateValue(state, 2);
    lua_pushboolean(state, player && quest && player->CanCompleteRepeatableQuest(quest));
    return 1;
}

int PlayerGetQuestLevel(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    Quest const* quest = CheckQuestTemplateValue(state, 2);
    lua_pushinteger(state, quest ? (player ? player->GetQuestLevelForPlayer(quest) : quest->GetQuestLevel()) : 0);
    return 1;
}

int PlayerGetReqKillOrCastCurrentCount(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 questId = CheckQuestIdValue(state, 2);
    int32 entry = static_cast<int32>(luaL_checkinteger(state, 3));

    Quest const* quest = sObjectMgr.GetQuestTemplate(questId);
    QuestStatusData const* statusData = player ? player->GetQuestStatusData(questId) : nullptr;
    if (!quest || !statusData)
    {
        lua_pushinteger(state, 0);
        return 1;
    }

    for (uint8 i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
    {
        int32 requiredEntry = quest->ReqCreatureOrGOId[i];
        if (requiredEntry == entry || (requiredEntry < 0 && -requiredEntry == entry))
        {
            lua_pushinteger(state, statusData->m_creatureOrGOcount[i]);
            return 1;
        }
    }

    lua_pushinteger(state, 0);
    return 1;
}

int PlayerGetGuild(lua_State* state)
{
    auto* engine = GetEngine(state);
    Player* player = CheckPlayer(state, 1);
    Guild* guild = player && player->GetGuildId() ? sGuildMgr.GetGuildById(player->GetGuildId()) : nullptr;
    if (engine)
        engine->PushGuild(guild);
    else
        lua_pushnil(state);
    return 1;
}

int PlayerGetGuildName(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    Guild* guild = player && player->GetGuildId() ? sGuildMgr.GetGuildById(player->GetGuildId()) : nullptr;
    if (guild)
        lua_pushstring(state, guild->GetName().c_str());
    else
        lua_pushnil(state);
    return 1;
}

int PlayerCompatNoop(lua_State* state)
{
    (void)CheckPlayer(state, 1);
    return 0;
}

int PlayerCompatReturnZero(lua_State* state)
{
    (void)CheckPlayer(state, 1);
    lua_pushinteger(state, 0);
    return 1;
}

int PlayerCompatReturnFalse(lua_State* state)
{
    (void)CheckPlayer(state, 1);
    lua_pushboolean(state, false);
    return 1;
}

int PlayerCompatReturnNil(lua_State* state)
{
    (void)CheckPlayer(state, 1);
    lua_pushnil(state);
    return 1;
}

int PlayerCanUninviteFromGroup(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    ObjectGuid uninvitedGuid = lua_isnoneornil(state, 2) && player ? player->GetObjectGuid() : CheckObjectGuidValue(state, 2);
    lua_pushboolean(state, player && player->CanUninviteFromGroup(uninvitedGuid) == ERR_PARTY_RESULT_OK);
    return 1;
}

int PlayerEquipItem(lua_State* state)
{
    auto* engine = GetEngine(state);
    Player* player = CheckPlayer(state, 1);
    uint32 slot = static_cast<uint32>(luaL_checkinteger(state, 3));
    Item* equipped = nullptr;

    if (player && slot < INVENTORY_SLOT_BAG_END)
    {
        uint16 dest = 0;

        if (luaL_testudata(state, 2, ITEM_METATABLE))
        {
            Item* item = CheckItem(state, 2);
            if (item && player->CanEquipItem(static_cast<uint8>(slot), dest, item, false) == EQUIP_ERR_OK)
            {
                player->RemoveItem(item->GetBagSlot(), item->GetSlot(), true);
                equipped = player->EquipItem(dest, item, true);
            }
        }
        else
        {
            uint32 entry = static_cast<uint32>(luaL_checkinteger(state, 2));
            if (player->CanEquipNewItem(static_cast<uint8>(slot), dest, entry, false) == EQUIP_ERR_OK)
                equipped = player->EquipNewItem(dest, entry, true);
        }

        if (equipped)
            player->AutoUnequipOffhandIfNeed();
    }

    if (engine)
        engine->PushItem(equipped);
    else
        lua_pushnil(state);

    return 1;
}

int PlayerGetCompletedQuestsCount(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player ? player->GetTotalQuestCount() : 0);
    return 1;
}

int PlayerGetCorpse(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    Player* player = CheckPlayer(state, 1);
    Corpse* corpse = player ? player->GetCorpse() : nullptr;
    if (engine)
        engine->PushCorpse(corpse);
    else
        lua_pushnil(state);
    return 1;
}

int CorpseGetOwnerGUID(lua_State* state)
{
    Corpse* corpse = CheckCorpse(state, 1);
    if (corpse)
        PushObjectGuidValue(state, corpse->GetOwnerGuid());
    else
        lua_pushnil(state);
    return 1;
}

int CorpseGetName(lua_State* state)
{
    Corpse* corpse = CheckCorpse(state, 1);
    lua_pushstring(state, corpse ? corpse->GetName() : "");
    return 1;
}

int CorpseGetGhostTime(lua_State* state)
{
    Corpse* corpse = CheckCorpse(state, 1);
    lua_pushinteger(state, corpse ? static_cast<lua_Integer>(corpse->GetGhostTime()) : 0);
    return 1;
}

int CorpseGetType(lua_State* state)
{
    Corpse* corpse = CheckCorpse(state, 1);
    lua_pushinteger(state, corpse ? corpse->GetType() : 0);
    return 1;
}

int CorpseResetGhostTime(lua_State* state)
{
    Corpse* corpse = CheckCorpse(state, 1);
    if (corpse)
        corpse->ResetGhostTime();
    return 0;
}

int CorpseSaveToDB(lua_State* state)
{
    Corpse* corpse = CheckCorpse(state, 1);
    if (corpse && corpse->GetType() != CORPSE_BONES)
        corpse->SaveToDB();
    return 0;
}

int DynamicObjectGetName(lua_State* state)
{
    DynamicObject* dynObj = CheckDynamicObject(state, 1);
    lua_pushstring(state, dynObj ? dynObj->GetName() : "");
    return 1;
}

int DynamicObjectGetCaster(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    DynamicObject* dynObj = CheckDynamicObject(state, 1);
    PushWorldObjectValue(state, engine, dynObj ? dynObj->GetCaster() : nullptr);
    return 1;
}

int DynamicObjectGetUnitCaster(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    DynamicObject* dynObj = CheckDynamicObject(state, 1);
    if (engine)
        engine->PushUnit(dynObj ? dynObj->GetUnitCaster() : nullptr);
    else
        lua_pushnil(state);
    return 1;
}

int DynamicObjectGetCasterGUID(lua_State* state)
{
    DynamicObject* dynObj = CheckDynamicObject(state, 1);
    PushObjectGuidValue(state, dynObj ? dynObj->GetCasterGuid() : ObjectGuid());
    return 1;
}

int DynamicObjectGetCasterGUIDLow(lua_State* state)
{
    DynamicObject* dynObj = CheckDynamicObject(state, 1);
    lua_pushinteger(state, dynObj ? dynObj->GetCasterGuid().GetCounter() : 0);
    return 1;
}

int DynamicObjectGetSpellId(lua_State* state)
{
    DynamicObject* dynObj = CheckDynamicObject(state, 1);
    lua_pushinteger(state, dynObj ? dynObj->GetSpellId() : 0);
    return 1;
}

int DynamicObjectGetEffIndex(lua_State* state)
{
    DynamicObject* dynObj = CheckDynamicObject(state, 1);
    lua_pushinteger(state, dynObj ? dynObj->GetEffIndex() : 0);
    return 1;
}

int DynamicObjectGetDuration(lua_State* state)
{
    DynamicObject* dynObj = CheckDynamicObject(state, 1);
    lua_pushinteger(state, dynObj ? dynObj->GetDuration() : 0);
    return 1;
}

int DynamicObjectGetRadius(lua_State* state)
{
    DynamicObject* dynObj = CheckDynamicObject(state, 1);
    lua_pushnumber(state, dynObj ? dynObj->GetRadius() : 0.0f);
    return 1;
}

int DynamicObjectGetType(lua_State* state)
{
    DynamicObject* dynObj = CheckDynamicObject(state, 1);
    lua_pushinteger(state, dynObj ? dynObj->GetType() : 0);
    return 1;
}

int DynamicObjectDelay(lua_State* state)
{
    DynamicObject* dynObj = CheckDynamicObject(state, 1);
    int32 delay = static_cast<int32>(luaL_checkinteger(state, 2));
    if (dynObj)
        dynObj->Delay(delay);
    return 0;
}

int DynamicObjectDelete(lua_State* state)
{
    DynamicObject* dynObj = CheckDynamicObject(state, 1);
    if (dynObj)
        dynObj->Delete();
    return 0;
}

int PlayerGetDifficulty(lua_State* state)
{
    (void)CheckPlayer(state, 1);
    lua_pushinteger(state, 0);
    return 1;
}

int PlayerGetGossipTextId(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    WorldObject* source = CheckWorldObject(state, 2);
    lua_pushinteger(state, player ? player->GetGossipTextId(source) : DEFAULT_GOSSIP_MESSAGE);
    return 1;
}

int PlayerGetHealthBonusFromStamina(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    float stamina = player ? player->GetStat(STAT_STAMINA) : 0.0f;
    float baseStam = std::min(stamina, 20.0f);
    float moreStam = stamina - baseStam;
    lua_pushnumber(state, baseStam + (moreStam * 10.0f));
    return 1;
}

int PlayerGetHonorPoints(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player ? static_cast<uint32>(std::max(0.0f, player->GetHonorMgr().GetRankPoints())) : 0);
    return 1;
}

int PlayerGetMailCount(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 count = 0;

    if (player && player->GetSession() && player->GetSession()->GetMasterPlayer())
        count = player->GetSession()->GetMasterPlayer()->GetMailSize();
    else if (player)
    {
        std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
            "SELECT COUNT(*) FROM mail WHERE receiver = '%u' AND (isDeleted IS NULL OR isDeleted = 0)",
            player->GetGUIDLow()));
        if (result)
            count = result->Fetch()[0].GetUInt32();
    }

    lua_pushinteger(state, count);
    return 1;
}

int PlayerGetMailItem(lua_State* state)
{
    auto* engine = GetEngine(state);
    Player* player = CheckPlayer(state, 1);
    uint32 itemGuidLow = 0;

    if (luaL_testudata(state, 2, OBJECTGUID_METATABLE))
    {
        ObjectGuid const* guid = CheckObjectGuid(state, 2);
        itemGuidLow = guid ? guid->GetCounter() : 0;
    }
    else
        itemGuidLow = static_cast<uint32>(luaL_checkinteger(state, 2));

    Item* item = nullptr;
    if (player && player->GetSession() && player->GetSession()->GetMasterPlayer() && itemGuidLow)
        item = player->GetSession()->GetMasterPlayer()->GetMItem(itemGuidLow);

    if (engine)
        engine->PushItem(item);
    else
        lua_pushnil(state);

    return 1;
}

int PlayerGetManaBonusFromIntellect(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    float intellect = player ? player->GetStat(STAT_INTELLECT) : 0.0f;
    float baseInt = std::min(intellect, 20.0f);
    float moreInt = intellect - baseInt;
    lua_pushnumber(state, baseInt + (moreInt * 15.0f));
    return 1;
}

int PlayerGetNextRandomRaidMember(lua_State* state)
{
    auto* engine = GetEngine(state);
    Player* player = CheckPlayer(state, 1);
    float radius = static_cast<float>(luaL_checknumber(state, 2));
    Player* member = player ? player->GetNextRandomRaidMember(radius) : nullptr;

    if (engine)
        engine->PushPlayer(member);
    else
        lua_pushnil(state);

    return 1;
}

int PlayerGetPhaseMaskForSpawn(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player ? player->GetWorldMask() : 0);
    return 1;
}

int PlayerGetShieldBlockValue(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player ? player->GetShieldBlockValue() : 0);
    return 1;
}

int PlayerGetSpecsCount(lua_State* state)
{
    (void)CheckPlayer(state, 1);
    lua_pushinteger(state, 1);
    return 1;
}

int PlayerGossipAddQuests(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    WorldObject* source = CheckWorldObject(state, 2);

    if (player && source)
    {
        if (source->GetTypeId() == TYPEID_UNIT)
        {
            if (source->GetUInt32Value(UNIT_NPC_FLAGS) & UNIT_NPC_FLAG_QUESTGIVER)
                player->PrepareQuestMenu(source->GetObjectGuid());
        }
        else if (GameObject* go = dynamic_cast<GameObject*>(source))
        {
            if (go->GetGoType() == GAMEOBJECT_TYPE_QUESTGIVER)
                player->PrepareQuestMenu(source->GetObjectGuid());
        }
    }

    return 0;
}

int PlayerGossipSendPOI(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    float x = static_cast<float>(luaL_checknumber(state, 2));
    float y = static_cast<float>(luaL_checknumber(state, 3));
    uint32 icon = static_cast<uint32>(luaL_checkinteger(state, 4));
    uint32 flags = static_cast<uint32>(luaL_checkinteger(state, 5));
    uint32 data = static_cast<uint32>(luaL_checkinteger(state, 6));
    char const* iconText = luaL_checkstring(state, 7);

    if (player)
        player->PlayerTalkClass->SendPointOfInterest(x, y, icon, flags, data, iconText);

    return 0;
}

int PlayerGroupInvite(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    Player* invited = CheckPlayer(state, 2);
    bool success = false;

    if (player && invited && !invited->GetGroup() && !invited->GetGroupInvite())
    {
        Group* group = player->GetGroup();
        if (group && group->isBGGroup())
            group = player->GetOriginalGroup();

        if (group)
            success = !group->IsFull() && group->AddInvite(invited);
        else
        {
            group = new Group;
            success = group->AddLeaderInvite(player) && group->AddInvite(invited);
            if (!success)
                delete group;
        }

        if (success && invited->GetSession())
        {
            WorldPacket data(SMSG_GROUP_INVITE, 10);
            data << player->GetName();
            invited->GetSession()->SendPacket(&data);
        }
    }

    lua_pushboolean(state, success);
    return 1;
}

int PlayerGroupCreate(lua_State* state)
{
    auto* engine = GetEngine(state);
    Player* player = CheckPlayer(state, 1);
    Player* invited = CheckPlayer(state, 2);
    Group* group = nullptr;

    if (player && invited && !player->GetGroup() && !invited->GetGroup())
    {
        if (player->GetGroupInvite())
            player->UninviteFromGroup();
        if (invited->GetGroupInvite())
            invited->UninviteFromGroup();

        group = new Group;
        if (!group->AddLeaderInvite(player))
        {
            delete group;
            group = nullptr;
        }
        else
        {
            group->RemoveInvite(player);
            if (!group->Create(player->GetObjectGuid(), player->GetName()))
            {
                delete group;
                group = nullptr;
            }
            else
            {
                sObjectMgr.AddGroup(group);
                if (!group->AddMember(invited->GetObjectGuid(), invited->GetName()))
                {
                    group->Disband(true);
                    group = nullptr;
                }
                else
                    group->BroadcastGroupUpdate();
            }
        }
    }

    if (engine)
        engine->PushGroup(group);
    else
        lua_pushnil(state);

    return 1;
}

int PlayerHasTalent(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 spellId = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushboolean(state, player && player->HasSpell(spellId));
    return 1;
}

int PlayerHasTitle(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 titleId = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushboolean(state, player && titleId <= 0xFFu && player->HasTitle(static_cast<uint8>(titleId)));
    return 1;
}

int PlayerLeaveBattleground(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    bool teleportToEntryPoint = lua_isnoneornil(state, 2) ? true : lua_toboolean(state, 2) != 0;
    if (player)
        player->LeaveBattleground(teleportToEntryPoint);
    return 0;
}

int PlayerLogoutPlayer(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    bool save = lua_isnoneornil(state, 2) ? true : lua_toboolean(state, 2) != 0;
    if (player && player->GetSession())
        player->GetSession()->LogoutPlayer(save);
    return 0;
}

int PlayerModifyHonorPoints(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    int32 amount = static_cast<int32>(luaL_checkinteger(state, 2));

    if (player)
    {
        HonorMgr& honor = player->GetHonorMgr();
        float updated = std::max(0.0f, honor.GetRankPoints() + static_cast<float>(amount));
        honor.SetRankPoints(updated);
        honor.SetRank(HonorMgr::CalculateRank(updated, honor.GetTotalHK()));
    }

    return 0;
}

int PlayerMute(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 muteSeconds = static_cast<uint32>(luaL_checkinteger(state, 2));

    if (player && player->GetSession())
    {
        time_t muteTime = time(nullptr) + muteSeconds;
        player->GetSession()->m_muteTime = muteTime;
        LoginDatabase.PExecute("UPDATE `account` SET `mutetime` = '%u' WHERE `id` = '%u'",
                               static_cast<uint32>(muteTime), player->GetSession()->GetAccountId());
    }

    return 0;
}

int PlayerRemovedInsignia(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    Player* looter = CheckPlayer(state, 2);
    if (player && looter)
        player->RemovedInsignia(looter, player->GetCorpse());
    return 0;
}

int PlayerRemoveFromBattlegroundRaid(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    if (player)
        player->RemoveFromBattleGroundRaid();
    return 0;
}

int PlayerRemoveFromGroup(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    if (player && player->GetGroup())
        player->RemoveFromGroup();
    return 0;
}

int PlayerResetTalentsCost(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player ? player->GetTalentResetCost() : 0);
    return 1;
}

int PlayerResetTypeCooldowns(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 category = static_cast<uint32>(luaL_checkinteger(state, 2));
    bool update = lua_isnoneornil(state, 3) ? true : lua_toboolean(state, 3) != 0;

    if (player)
    {
        std::vector<uint32> spells;
        for (auto const& cooldown : player->GetSpellCooldownMap())
            if (cooldown.second.cat == category)
                spells.push_back(cooldown.first);

        for (uint32 spellId : spells)
            player->RemoveSpellCooldown(spellId, update);
    }

    return 0;
}

int PlayerRunCommand(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    std::string command = luaL_checkstring(state, 2);

    if (player && player->GetSession() && !command.empty())
    {
        if (command[0] == '.' || command[0] == '!')
            command.erase(command.begin());

        ChatHandler handler(player->GetSession());
        handler.ParseCommands(command.c_str());
    }

    return 0;
}

int PlayerSendAuctionMenu(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    Unit* unit = CheckUnit(state, 2);
    if (player && player->GetSession() && unit)
        player->GetSession()->SendAuctionHello(unit);
    return 0;
}

int PlayerSendGuildInvite(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    Player* invited = CheckPlayer(state, 2);

    if (player && invited && player->GetSession() && invited->GetSession())
    {
        Guild* guild = player->GetGuildId() ? sGuildMgr.GetGuildById(player->GetGuildId()) : nullptr;
        if (guild && !invited->GetGuildId() && !invited->GetGuildIdInvited() && guild->HasRankRight(player->GetRank(), GR_RIGHT_INVITE))
        {
            invited->SetGuildIdInvited(player->GetGuildId());
            guild->LogGuildEvent(GUILD_EVENT_LOG_INVITE_PLAYER, player->GetObjectGuid(), invited->GetObjectGuid());

            WorldPacket data(SMSG_GUILD_INVITE, 18);
            data << player->GetName();
            data << guild->GetName();
            invited->GetSession()->SendPacket(&data);
        }
    }

    return 0;
}

int PlayerSendListInventory(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    WorldObject* object = CheckWorldObject(state, 2);
    uint8 menuType = static_cast<uint8>(luaL_optinteger(state, 3, VENDOR_MENU_ALL));

    if (player && player->GetSession() && object)
        player->GetSession()->SendListInventory(object->GetObjectGuid(), menuType);

    return 0;
}

int PlayerSendMovieStart(lua_State* state)
{
    return PlayerSendCinematicStart(state);
}

int PlayerSendPacket(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    WorldPacket* packet = CheckWorldPacket(state, 2);
    if (player && packet && player->GetSession())
        player->GetSession()->SendPacket(packet);
    return 0;
}

int PlayerSendQuestTemplate(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    Quest const* quest = CheckQuestTemplateValue(state, 2);
    bool activateAccept = lua_isnoneornil(state, 3) ? true : lua_toboolean(state, 3) != 0;

    if (player && quest)
        player->PlayerTalkClass->SendQuestGiverQuestDetails(quest, player->GetObjectGuid(), activateAccept);

    return 0;
}

int PlayerSendShowBank(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    WorldObject* object = CheckWorldObject(state, 2);
    if (player && player->GetSession() && object)
        player->GetSession()->SendShowBank(object->GetObjectGuid());
    return 0;
}

int PlayerSendShowMailBox(lua_State* state)
{
    (void)CheckPlayer(state, 1);
    if (!lua_isnoneornil(state, 2))
        (void)CheckObjectGuidValue(state, 2);
    return 0;
}

int PlayerSendSpiritResurrect(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    if (player && player->GetSession())
        player->GetSession()->SendSpiritResurrect();
    return 0;
}

int PlayerSendTabardVendorActivate(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    WorldObject* object = CheckWorldObject(state, 2);
    if (player && player->GetSession() && object)
        player->GetSession()->SendTabardVendorActivate(object->GetObjectGuid());
    return 0;
}

int PlayerSendTaxiMenu(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    Creature* creature = CheckCreature(state, 2);
    if (player && player->GetSession() && creature)
        player->GetSession()->SendTaxiMenu(creature);
    return 0;
}

int PlayerSendTrainerList(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    WorldObject* object = CheckWorldObject(state, 2);
    if (player && player->GetSession() && object)
        player->GetSession()->SendTrainerList(object->GetObjectGuid());
    return 0;
}

int PlayerSetFactionForRace(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint8 race = static_cast<uint8>(luaL_checkinteger(state, 2));
    if (player)
        player->SetFactionForRace(race);
    return 0;
}

int PlayerSetGender(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 gender = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (gender > GENDER_FEMALE)
        return luaL_argerror(state, 2, "valid Gender expected");

    if (player)
    {
        player->SetByteValue(UNIT_FIELD_BYTES_0, 2, gender);
        player->SetUInt16Value(PLAYER_BYTES_3, 0, uint16(gender) | (player->GetDrunkValue() & 0xFFFE));
    }

    return 0;
}

int PlayerSetGuildRank(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 rank = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (player && player->GetGuildId())
        player->SetRank(rank);
    return 0;
}

int PlayerSetHonorPoints(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 honorPoints = static_cast<uint32>(luaL_checkinteger(state, 2));

    if (player)
    {
        HonorMgr& honor = player->GetHonorMgr();
        honor.SetRankPoints(static_cast<float>(honorPoints));
        honor.SetRank(HonorMgr::CalculateRank(static_cast<float>(honorPoints), honor.GetTotalHK()));
    }

    return 0;
}

int PlayerSetKnownTitle(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 titleId = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (player && titleId < TITLE_MAX_LIMIT)
        player->AwardTitle(static_cast<int8>(titleId));
    return 0;
}

int PlayerSetPlayerLock(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    bool apply = lua_isnoneornil(state, 2) ? true : lua_toboolean(state, 2) != 0;

    if (player)
    {
        if (apply)
        {
            player->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PACIFIED | UNIT_FLAG_SILENCED);
            player->SetClientControl(player, 0);
        }
        else
        {
            player->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PACIFIED | UNIT_FLAG_SILENCED);
            player->SetClientControl(player, 1);
        }
    }

    return 0;
}

int PlayerStartTaxi(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 pathId = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (player)
        player->ActivateTaxiPathTo(pathId, 0, true);
    return 0;
}

int PlayerSummonPlayer(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    WorldObject* summoner = CheckWorldObject(state, 2);

    if (player && summoner)
    {
        player->SendSummonRequest(summoner->GetObjectGuid(), summoner->GetMapId(), summoner->GetZoneId(),
                                  summoner->GetPositionX(), summoner->GetPositionY(), summoner->GetPositionZ());
    }

    return 0;
}

int PlayerUnbindAllInstances(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    if (player)
    {
        auto& binds = player->GetBoundInstances();
        for (auto itr = binds.begin(); itr != binds.end();)
        {
            if (itr->first == player->GetMapId())
                ++itr;
            else
                player->UnbindInstance(itr);
        }
    }

    return 0;
}

int PlayerUnbindInstance(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 mapId = static_cast<uint32>(luaL_checkinteger(state, 2));
    bool unload = lua_toboolean(state, 3) != 0;
    if (player)
        player->UnbindInstance(mapId, unload);
    return 0;
}

int PlayerUnsetKnownTitle(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 titleId = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (player && titleId > 0 && titleId < TITLE_MAX_LIMIT)
        player->AwardTitle(-static_cast<int8>(titleId));
    return 0;
}

int PlayerWhisper(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    char const* message = luaL_checkstring(state, 2);
    uint32 language = static_cast<uint32>(luaL_optinteger(state, 3, LANG_UNIVERSAL));
    Player* receiver = CheckPlayer(state, 4);

    if (language != LANG_ADDON && language >= LANGUAGES_COUNT)
        return luaL_argerror(state, 3, "valid Language expected");

    if (player && receiver && receiver->GetSession())
    {
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, message, Language(language), player->GetChatTag(),
                                     player->GetObjectGuid(), player->GetName(),
                                     receiver->GetObjectGuid(), receiver->GetName());
        receiver->GetSession()->SendPacket(&data);
    }

    return 0;
}

int PlayerHasSkill(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint16 skillId = static_cast<uint16>(luaL_checkinteger(state, 2));
    lua_pushboolean(state, player && player->HasSkill(skillId));
    return 1;
}

int PlayerGetSkillValue(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint16 skillId = static_cast<uint16>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, player ? player->GetSkillValue(skillId) : 0);
    return 1;
}

int PlayerGetBaseSkillValue(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint16 skillId = static_cast<uint16>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, player ? player->GetSkillValueBase(skillId) : 0);
    return 1;
}

int PlayerGetPureSkillValue(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint16 skillId = static_cast<uint16>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, player ? player->GetSkillValuePure(skillId) : 0);
    return 1;
}

int PlayerGetMaxSkillValue(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint16 skillId = static_cast<uint16>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, player ? player->GetSkillMax(skillId) : 0);
    return 1;
}

int PlayerGetPureMaxSkillValue(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 skillId = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, player ? player->GetPureMaxSkillValue(skillId) : 0);
    return 1;
}

int PlayerGetSkillTempBonusValue(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint16 skillId = static_cast<uint16>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, player ? player->GetSkillBonusTemporary(skillId) : 0);
    return 1;
}

int PlayerGetSkillPermBonusValue(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint16 skillId = static_cast<uint16>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, player ? player->GetSkillBonusPermanent(skillId) : 0);
    return 1;
}

int PlayerSetSkill(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint16 skillId = static_cast<uint16>(luaL_checkinteger(state, 2));
    uint16 step = static_cast<uint16>(luaL_checkinteger(state, 3));
    uint16 current = static_cast<uint16>(luaL_checkinteger(state, 4));
    uint16 max = static_cast<uint16>(luaL_checkinteger(state, 5));

    if (player)
        player->SetSkill(skillId, current, max, step);

    return 0;
}

int PlayerGetReputation(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 factionId = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, player ? player->GetReputationMgr().GetReputation(factionId) : 0);
    return 1;
}

int PlayerGetReputationRank(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 factionId = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, player ? player->GetReputationRank(factionId) : REP_NEUTRAL);
    return 1;
}

int PlayerSetReputation(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 factionId = static_cast<uint32>(luaL_checkinteger(state, 2));
    int32 value = static_cast<int32>(luaL_checkinteger(state, 3));
    FactionEntry const* faction = sObjectMgr.GetFactionEntry(factionId);

    if (player && faction)
        player->GetReputationMgr().SetReputation(faction, value);

    return 0;
}

int PlayerGetTotalPlayedTime(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player ? player->GetTotalPlayedTime() : 0);
    return 1;
}

int PlayerGetLevelPlayedTime(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player ? player->GetLevelPlayedTime() : 0);
    return 1;
}

int PlayerIsInWater(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushboolean(state, player && player->IsInWater());
    return 1;
}

int PlayerIsMoving(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushboolean(state, player && player->IsMoving());
    return 1;
}

int PlayerIsFlying(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushboolean(state, player && player->IsFlying());
    return 1;
}

int PlayerCanFly(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushboolean(state, player && player->CanFly());
    return 1;
}

int PlayerIsFalling(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushboolean(state, player && player->IsFalling());
    return 1;
}

int PlayerIsDND(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushboolean(state, player && player->IsDND());
    return 1;
}

int PlayerIsAFK(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushboolean(state, player && player->IsAFK());
    return 1;
}

int PlayerIsGMVisible(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushboolean(state, player && player->IsGMVisible());
    return 1;
}

int PlayerIsTaxiCheater(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushboolean(state, player && player->IsTaxiCheater());
    return 1;
}

int PlayerIsGMChat(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushboolean(state, player && player->IsGMChat());
    return 1;
}

int PlayerIsAcceptingWhispers(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushboolean(state, player && player->IsAcceptWhispers());
    return 1;
}

int PlayerIsRested(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushboolean(state, player && player->IsRested());
    return 1;
}

int PlayerCanBlock(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushboolean(state, player && player->CanBlock());
    return 1;
}

int PlayerCanParry(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushboolean(state, player && player->CanParry());
    return 1;
}

int PlayerGetFreeTalentPoints(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player ? player->GetFreeTalentPoints() : 0);
    return 1;
}

int PlayerSetFreeTalentPoints(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 points = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (player)
        player->SetFreeTalentPoints(points);
    return 0;
}

int PlayerGetComboTarget(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    PushObjectGuidValue(state, player ? player->GetComboTargetGuid() : ObjectGuid());
    return 1;
}

int PlayerGetComboPoints(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player ? player->GetComboPoints() : 0);
    return 1;
}

int PlayerGetDrunkValue(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player ? player->GetDrunkValue() : 0);
    return 1;
}

int PlayerSetDrunkValue(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint16 value = static_cast<uint16>(luaL_checkinteger(state, 2));
    uint32 itemId = static_cast<uint32>(luaL_optinteger(state, 3, 0));
    if (player)
        player->SetDrunkValue(value, itemId);
    return 0;
}

int PlayerGetRestBonus(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushnumber(state, player ? player->GetRestBonus() : 0.0f);
    return 1;
}

int PlayerSetRestBonus(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    float bonus = static_cast<float>(luaL_checknumber(state, 2));
    if (player)
        player->SetRestBonus(bonus);
    return 0;
}

int PlayerGetXPRestBonus(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 xp = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, player ? player->GetXPRestBonus(xp) : 0);
    return 1;
}

int PlayerSetAcceptWhispers(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    bool accept = lua_isnoneornil(state, 2) ? true : lua_toboolean(state, 2) != 0;
    if (player)
        player->SetAcceptWhispers(accept);
    return 0;
}

int PlayerGetChatTag(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player ? player->GetChatTag() : 0);
    return 1;
}

int PlayerGetLatency(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player && player->GetSession() ? player->GetSession()->GetLatency() : 0);
    return 1;
}

int PlayerGetPlayerIP(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    if (player && player->GetSession())
        lua_pushstring(state, player->GetSession()->GetRemoteAddress().c_str());
    else
        lua_pushnil(state);
    return 1;
}

int PlayerGetAccountName(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    if (player && player->GetSession())
        lua_pushstring(state, player->GetSession()->GetUsername().c_str());
    else
        lua_pushnil(state);
    return 1;
}

int PlayerGetDbLocaleIndex(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player && player->GetSession() ? player->GetSession()->GetSessionDbLocaleIndex() : 0);
    return 1;
}

int PlayerGetDbcLocale(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player && player->GetSession() ? player->GetSession()->GetSessionDbcLocale() : 0);
    return 1;
}

int PlayerGetMap(lua_State* state)
{
    auto* engine = GetEngine(state);
    Player* player = CheckPlayer(state, 1);
    Map* map = player ? player->GetMap() : nullptr;
    if (engine)
        engine->PushMap(map);
    else
        lua_pushnil(state);
    return 1;
}

int PlayerGetItemByGUID(lua_State* state)
{
    auto* engine = GetEngine(state);
    Player* player = CheckPlayer(state, 1);
    ObjectGuid const* guid = CheckObjectGuid(state, 2);
    Item* item = player && guid ? player->GetItemByGuid(*guid) : nullptr;

    if (engine)
        engine->PushItem(item);
    else
        lua_pushnil(state);

    return 1;
}

int PlayerGetItemByPos(lua_State* state)
{
    auto* engine = GetEngine(state);
    Player* player = CheckPlayer(state, 1);
    Item* item = nullptr;

    if (player)
    {
        if (lua_gettop(state) >= 3)
        {
            uint8 bag = static_cast<uint8>(luaL_checkinteger(state, 2));
            uint8 slot = static_cast<uint8>(luaL_checkinteger(state, 3));
            item = player->GetItemByPos(bag, slot);
        }
        else
        {
            uint16 pos = static_cast<uint16>(luaL_checkinteger(state, 2));
            item = player->GetItemByPos(pos);
        }
    }

    if (engine)
        engine->PushItem(item);
    else
        lua_pushnil(state);

    return 1;
}

int PlayerGetItemByEntry(lua_State* state)
{
    auto* engine = GetEngine(state);
    Player* player = CheckPlayer(state, 1);
    uint32 entry = static_cast<uint32>(luaL_checkinteger(state, 2));
    Item* foundItem = nullptr;

    if (player)
    {
        player->ApplyForAllItems([entry, &foundItem](Item* item)
        {
            if (!foundItem && item && item->GetEntry() == entry)
                foundItem = item;
        }, true);
    }

    if (engine)
        engine->PushItem(foundItem);
    else
        lua_pushnil(state);

    return 1;
}

int PlayerGetEquippedItemBySlot(lua_State* state)
{
    auto* engine = GetEngine(state);
    Player* player = CheckPlayer(state, 1);
    uint8 slot = static_cast<uint8>(luaL_checkinteger(state, 2));
    Item* item = player ? player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot) : nullptr;
    if (engine)
        engine->PushItem(item);
    else
        lua_pushnil(state);
    return 1;
}

int PlayerGetGMRank(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player && player->GetSession() ? player->GetSession()->GetSecurity() : 0);
    return 1;
}

int PlayerIsGM(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushboolean(state, player && player->IsGameMaster());
    return 1;
}

int PlayerSetGM(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    bool on = lua_toboolean(state, 2) != 0;
    if (player)
        player->SetGameMaster(on);
    return 0;
}

int PlayerHasAtLoginFlag(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    AtLoginFlags flag = static_cast<AtLoginFlags>(luaL_checkinteger(state, 2));
    lua_pushboolean(state, player && player->HasAtLoginFlag(flag));
    return 1;
}

int PlayerSetAtLoginFlag(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    AtLoginFlags flag = static_cast<AtLoginFlags>(luaL_checkinteger(state, 2));
    if (player)
        player->SetAtLoginFlag(flag);
    return 0;
}

int PlayerCanSpeak(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushboolean(state, player && player->CanSpeak());
    return 1;
}

int PlayerToggleAFK(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushboolean(state, player && player->ToggleAFK());
    return 1;
}

int PlayerToggleDND(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushboolean(state, player && player->ToggleDND());
    return 1;
}

int PlayerSetGMVisible(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    bool on = lua_isnoneornil(state, 2) ? true : lua_toboolean(state, 2) != 0;
    if (player)
        player->SetGMVisible(on);
    return 0;
}

int PlayerSetGMChat(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    bool on = lua_isnoneornil(state, 2) ? true : lua_toboolean(state, 2) != 0;
    if (player)
        player->SetGMChat(on);
    return 0;
}

int PlayerSetTaxiCheat(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    bool on = lua_isnoneornil(state, 2) ? true : lua_toboolean(state, 2) != 0;
    if (player)
        player->SetTaxiCheater(on);
    return 0;
}

int PlayerSetPvPDeath(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    bool on = lua_isnoneornil(state, 2) ? true : lua_toboolean(state, 2) != 0;
    if (player)
        player->SetPvPDeath(on);
    return 0;
}

int PlayerInArena(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushboolean(state, player && player->InArena());
    return 1;
}

int PlayerInBattleground(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushboolean(state, player && player->InBattleGround());
    return 1;
}

int PlayerInBattlegroundQueue(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushboolean(state, player && player->InBattleGroundQueue());
    return 1;
}

int PlayerGetBattlegroundId(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player ? player->GetBattleGroundId() : 0);
    return 1;
}

int PlayerGetBattlegroundTypeId(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player ? player->GetBattleGroundTypeId() : BATTLEGROUND_TYPE_NONE);
    return 1;
}

int PlayerGetCurrentBattlegroundQueueSlot(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player ? player->GetCurrentBattlegroundQueueSlot() : 0);
    return 1;
}

int PlayerGetLifetimeKills(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    lua_pushinteger(state, player ? player->GetUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS) : 0);
    return 1;
}

int PlayerSetLifetimeKills(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 kills = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (player)
        player->SetUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS, kills);
    return 0;
}

int PlayerAddLifetimeKills(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 amount = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (player)
    {
        uint64 updated = static_cast<uint64>(player->GetUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS)) + amount;
        player->SetUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS, static_cast<uint32>(std::min<uint64>(updated, 0xFFFFFFFFu)));
    }
    return 0;
}

int PlayerRemoveLifetimeKills(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 amount = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (player)
    {
        uint32 current = player->GetUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS);
        player->SetUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS, amount > current ? 0 : current - amount);
    }
    return 0;
}

int PlayerResurrect(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    float healthPercent = static_cast<float>(luaL_optnumber(state, 2, 100.0f));
    bool sickness = lua_toboolean(state, 3) != 0;
    if (player)
        player->ResurrectPlayer(healthPercent / 100.0f, sickness);
    return 0;
}

int PlayerDurabilityRepairAll(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    bool cost = lua_isnoneornil(state, 2) ? true : lua_toboolean(state, 2) != 0;
    float discount = static_cast<float>(luaL_optnumber(state, 3, 1.0f));
    lua_pushinteger(state, player ? player->DurabilityRepairAll(cost, discount) : 0);
    return 1;
}

int PlayerDurabilityRepair(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint16 position = static_cast<uint16>(luaL_checkinteger(state, 2));
    bool cost = lua_isnoneornil(state, 3) ? true : lua_toboolean(state, 3) != 0;
    float discount = static_cast<float>(luaL_optnumber(state, 4, 1.0f));
    lua_pushinteger(state, player ? player->DurabilityRepair(position, cost, discount) : 0);
    return 1;
}

int PlayerDurabilityLoss(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    Item* item = CheckItem(state, 2);
    double percent = luaL_checknumber(state, 3);
    if (player && item)
        player->DurabilityLoss(item, percent);
    return 0;
}

int PlayerDurabilityLossAll(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    double percent = luaL_checknumber(state, 2);
    bool inventory = lua_isnoneornil(state, 3) ? true : lua_toboolean(state, 3) != 0;
    if (player)
        player->DurabilityLossAll(percent, inventory);
    return 0;
}

int PlayerDurabilityPointsLoss(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    Item* item = CheckItem(state, 2);
    int32 points = static_cast<int32>(luaL_checkinteger(state, 3));
    if (player && item)
        player->DurabilityPointsLoss(item, points);
    return 0;
}

int PlayerDurabilityPointsLossAll(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    int32 points = static_cast<int32>(luaL_checkinteger(state, 2));
    bool inventory = lua_isnoneornil(state, 3) ? true : lua_toboolean(state, 3) != 0;
    if (player)
        player->DurabilityPointsLossAll(points, inventory);
    return 0;
}

int PlayerDurabilityPointLossForEquipSlot(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 slot = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (slot >= EQUIPMENT_SLOT_END)
        return luaL_argerror(state, 2, "valid EquipmentSlots value expected");

    if (player)
        player->DurabilityPointLossForEquipSlot(static_cast<EquipmentSlots>(slot));
    return 0;
}

int PlayerSaveToDB(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    bool online = lua_isnoneornil(state, 2) ? true : lua_toboolean(state, 2) != 0;
    bool force = lua_toboolean(state, 3) != 0;
    bool direct = lua_toboolean(state, 4) != 0;
    lua_pushboolean(state, player && player->SaveToDB(online, force, direct));
    return 1;
}

int PlayerKickPlayer(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    if (player && player->GetSession())
        player->GetSession()->KickPlayer();
    return 0;
}

int PlayerHasItem(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 itemId = static_cast<uint32>(luaL_checkinteger(state, 2));
    uint32 count = static_cast<uint32>(luaL_optinteger(state, 3, 1));
    bool checkBank = lua_toboolean(state, 4) != 0;
    lua_pushboolean(state, player && player->HasItemCount(itemId, count, checkBank));
    return 1;
}

int PlayerGetItemCount(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 itemId = static_cast<uint32>(luaL_checkinteger(state, 2));
    bool checkBank = lua_toboolean(state, 3) != 0;
    lua_pushinteger(state, player ? player->GetItemCount(itemId, checkBank) : 0);
    return 1;
}

int PlayerCanUseItem(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    bool canUse = false;

    if (player)
    {
        if (luaL_testudata(state, 2, ITEM_METATABLE))
        {
            Item* item = CheckItem(state, 2);
            canUse = item && player->CanUseItem(item) == EQUIP_ERR_OK;
        }
        else
        {
            ItemPrototype const* proto = luaL_testudata(state, 2, ITEMTEMPLATE_METATABLE)
                ? CheckItemTemplate(state, 2)
                : sObjectMgr.GetItemPrototype(static_cast<uint32>(luaL_checkinteger(state, 2)));

            canUse = proto && player->CanUseItem(proto) == EQUIP_ERR_OK;
        }
    }

    lua_pushboolean(state, canUse);
    return 1;
}

int PlayerCanEquipItem(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 slot = static_cast<uint32>(luaL_checkinteger(state, 3));
    if (slot >= EQUIPMENT_SLOT_END)
    {
        lua_pushboolean(state, false);
        return 1;
    }

    bool canEquip = false;
    if (player)
    {
        uint16 dest = 0;
        if (luaL_testudata(state, 2, ITEM_METATABLE))
        {
            Item* item = CheckItem(state, 2);
            canEquip = item && player->CanEquipItem(static_cast<uint8>(slot), dest, item, false) == EQUIP_ERR_OK;
        }
        else
        {
            ItemPrototype const* proto = luaL_testudata(state, 2, ITEMTEMPLATE_METATABLE)
                ? CheckItemTemplate(state, 2)
                : sObjectMgr.GetItemPrototype(static_cast<uint32>(luaL_checkinteger(state, 2)));

            canEquip = proto && player->CanEquipItem(static_cast<uint8>(slot), dest, proto, nullptr, false) == EQUIP_ERR_OK;
        }
    }

    lua_pushboolean(state, canEquip);
    return 1;
}

int PlayerRemoveItem(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 itemId = static_cast<uint32>(luaL_checkinteger(state, 2));
    uint32 count = static_cast<uint32>(luaL_optinteger(state, 3, 1));
    bool checkBank = lua_toboolean(state, 4) != 0;
    if (player)
        player->DestroyItemCount(itemId, count, true, false, checkBank);
    return 0;
}

int PlayerHasSpell(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 spellId = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushboolean(state, player && player->HasSpell(spellId));
    return 1;
}

int PlayerLearnSpell(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 spellId = static_cast<uint32>(luaL_checkinteger(state, 2));
    bool dependent = lua_toboolean(state, 3) != 0;
    bool talent = lua_toboolean(state, 4) != 0;
    if (player)
        player->LearnSpell(spellId, dependent, talent);
    return 0;
}

int PlayerRemoveSpell(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 spellId = static_cast<uint32>(luaL_checkinteger(state, 2));
    bool disabled = lua_toboolean(state, 3) != 0;
    bool learnLowRank = lua_isnoneornil(state, 4) ? true : lua_toboolean(state, 4) != 0;
    if (player)
        player->RemoveSpell(spellId, disabled, learnLowRank);
    return 0;
}

int PlayerGetSelectedUnit(lua_State* state)
{
    auto* engine = GetEngine(state);
    Player* player = CheckPlayer(state, 1);
    Unit* selected = player ? player->GetSelectedUnit() : nullptr;
    if (engine)
        engine->PushUnit(selected);
    else
        lua_pushnil(state);
    return 1;
}

int PlayerGetSelectedPlayer(lua_State* state)
{
    auto* engine = GetEngine(state);
    Player* player = CheckPlayer(state, 1);
    Player* selected = player ? player->GetSelectedPlayer() : nullptr;
    if (engine)
        engine->PushPlayer(selected);
    else
        lua_pushnil(state);
    return 1;
}

int PlayerGetSelectedCreature(lua_State* state)
{
    auto* engine = GetEngine(state);
    Player* player = CheckPlayer(state, 1);
    Creature* selected = player ? player->GetSelectedCreature() : nullptr;
    if (engine)
        engine->PushCreature(selected);
    else
        lua_pushnil(state);
    return 1;
}

int PlayerGetSelectedGameObject(lua_State* state)
{
    auto* engine = GetEngine(state);
    Player* player = CheckPlayer(state, 1);
    GameObject* selected = player && player->GetMap() ? player->GetMap()->GetGameObject(player->GetSelectedGobj()) : nullptr;
    if (engine)
        engine->PushGameObject(selected);
    else
        lua_pushnil(state);
    return 1;
}

int PlayerGetSelectedObject(lua_State* state)
{
    auto* engine = GetEngine(state);
    Player* player = CheckPlayer(state, 1);
    WorldObject* selected = nullptr;
    if (player)
    {
        selected = player->GetSelectedUnit();
        if (!selected && player->GetMap())
            selected = player->GetMap()->GetGameObject(player->GetSelectedGobj());
    }

    PushWorldObjectValue(state, engine, selected);
    return 1;
}

int PlayerGetSelectionGUID(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    PushObjectGuidValue(state, player ? player->GetSelectionGuid() : ObjectGuid());
    return 1;
}

int PlayerGetSelectedGameObjectGUID(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    PushObjectGuidValue(state, player ? player->GetSelectedGobj() : ObjectGuid());
    return 1;
}

int PlayerCompleteQuest(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 questId = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (player)
        player->CompleteQuest(questId);
    return 0;
}

int PlayerIncompleteQuest(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 questId = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (player)
        player->IncompleteQuest(questId);
    return 0;
}

int PlayerGetQuestStatus(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 questId = CheckQuestIdValue(state, 2);
    lua_pushinteger(state, player ? player->GetQuestStatus(questId) : QUEST_STATUS_NONE);
    return 1;
}

int PlayerHasQuest(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 questId = CheckQuestIdValue(state, 2);
    lua_pushboolean(state, player && player->IsActiveQuest(questId));
    return 1;
}

int PlayerGetQuestRewardStatus(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 questId = CheckQuestIdValue(state, 2);
    lua_pushboolean(state, player && player->GetQuestRewardStatus(questId));
    return 1;
}

int PlayerCanCompleteQuest(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 questId = CheckQuestIdValue(state, 2);
    lua_pushboolean(state, player && player->CanCompleteQuest(questId));
    return 1;
}

int PlayerCanRewardQuest(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    Quest const* quest = CheckQuestTemplateValue(state, 2);
    bool canReward = false;

    if (player && quest)
    {
        if (lua_isnoneornil(state, 3))
            canReward = player->CanRewardQuest(quest, false);
        else
        {
            uint32 reward = static_cast<uint32>(luaL_checkinteger(state, 3));
            bool msg = lua_toboolean(state, 4) != 0;
            canReward = player->CanRewardQuest(quest, reward, msg);
        }
    }

    lua_pushboolean(state, canReward);
    return 1;
}

int PlayerHasQuestForGO(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    int32 entry = static_cast<int32>(luaL_checkinteger(state, 2));
    lua_pushboolean(state, player && player->HasQuestForGO(entry));
    return 1;
}

int PlayerHasQuestForItem(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 entry = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushboolean(state, player && player->HasQuestForItem(entry));
    return 1;
}

int PlayerCanShareQuest(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 questId = CheckQuestIdValue(state, 2);
    lua_pushboolean(state, player && player->CanShareQuest(questId));
    return 1;
}

int PlayerSetQuestStatus(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 questId = CheckQuestIdValue(state, 2);
    QuestStatus status = static_cast<QuestStatus>(luaL_checkinteger(state, 3));
    if (player)
        player->SetQuestStatus(questId, status);
    return 0;
}

int PlayerAddQuest(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    Quest const* quest = CheckQuestTemplateValue(state, 2);
    Object* questGiver = CheckObject(state, 3);

    if (player && quest)
        player->AddQuest(quest, questGiver ? questGiver : player);

    return 0;
}

int PlayerRewardQuest(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    Quest const* quest = CheckQuestTemplateValue(state, 2);
    uint32 reward = static_cast<uint32>(luaL_optinteger(state, 3, 0));
    WorldObject* questGiver = CheckWorldObject(state, 4);
    bool announce = lua_isnoneornil(state, 5) ? true : lua_toboolean(state, 5) != 0;

    if (player && quest)
        player->RewardQuest(quest, reward, questGiver ? questGiver : player, announce);

    return 0;
}

int PlayerFailQuest(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 questId = CheckQuestIdValue(state, 2);
    if (player)
        player->FailQuest(questId);
    return 0;
}

int PlayerRemoveQuest(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 questId = CheckQuestIdValue(state, 2);
    if (player)
        player->RemoveQuest(questId);
    return 0;
}

int PlayerKilledMonsterCredit(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 entry = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (player)
        player->KilledMonsterCredit(entry);
    return 0;
}

int PlayerTalkedToCreature(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 entry = static_cast<uint32>(luaL_checkinteger(state, 2));
    ObjectGuid guid;

    if (luaL_testudata(state, 3, CREATURE_METATABLE))
    {
        Creature* creature = CheckCreature(state, 3);
        if (creature)
            guid = creature->GetObjectGuid();
    }
    else if (luaL_testudata(state, 3, OBJECTGUID_METATABLE))
        guid = *CheckObjectGuid(state, 3);

    if (player)
        player->TalkedToCreature(entry, guid);
    return 0;
}

int PlayerKillPlayer(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    if (player)
        player->KillPlayer();
    return 0;
}

int PlayerSpawnBones(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    if (player)
        player->SpawnCorpseBones();
    return 0;
}

int PlayerAreaExploredOrEventHappens(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 questId = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (player)
        player->AreaExploredOrEventHappens(questId);
    return 0;
}

int PlayerGroupEventHappens(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 questId = static_cast<uint32>(luaL_checkinteger(state, 2));
    WorldObject* object = CheckWorldObject(state, 3);
    if (player)
        player->GroupEventHappens(questId, object ? object : player);
    return 0;
}

int PlayerGossipClearMenu(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    if (player)
        player->PlayerTalkClass->ClearMenus();
    return 0;
}

int PlayerGossipMenuAddItem(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint8 icon = static_cast<uint8>(luaL_optinteger(state, 2, GOSSIP_ICON_CHAT));
    char const* message = luaL_checkstring(state, 3);
    uint32 sender = static_cast<uint32>(luaL_optinteger(state, 4, 0));
    uint32 action = static_cast<uint32>(luaL_optinteger(state, 5, 0));
    bool coded = lua_toboolean(state, 6) != 0;
    char const* boxMessage = luaL_optstring(state, 7, "");

    if (player)
        player->PlayerTalkClass->GetGossipMenu().AddMenuItem(icon, message, sender, action, boxMessage, coded);

    return 0;
}

int PlayerGossipSendMenu(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    uint32 textId = static_cast<uint32>(luaL_optinteger(state, 2, DEFAULT_GOSSIP_MESSAGE));
    Object* object = CheckObject(state, 3);

    if (player)
        player->PlayerTalkClass->SendGossipMenu(textId, object ? object->GetObjectGuid() : player->GetObjectGuid());

    return 0;
}

int PlayerGossipComplete(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    if (player)
        player->PlayerTalkClass->CloseGossip();
    return 0;
}

int PlayerCastSpell(lua_State* state)
{
    Player* player = CheckPlayer(state, 1);
    Unit* target = CheckUnit(state, 2);
    uint32 spellId = static_cast<uint32>(luaL_checkinteger(state, 3));
    bool triggered = lua_toboolean(state, 4) != 0;

    lua_pushboolean(state, player && target && player->CastSpell(target, spellId, triggered) == SPELL_CAST_OK);
    return 1;
}

int UnitCastSpell(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    Unit* target = lua_isnoneornil(state, 2) ? nullptr : CheckUnit(state, 2);
    uint32 spellId = static_cast<uint32>(luaL_checkinteger(state, 3));
    bool triggered = lua_isnoneornil(state, 4) ? false : lua_toboolean(state, 4) != 0;

    SpellEntry const* spellInfo = sSpellMgr.GetSpellEntry(spellId);
    lua_pushboolean(state, unit && spellInfo && unit->CastSpell(target, spellInfo, triggered) == SPELL_CAST_OK);
    return 1;
}

int UnitCastCustomSpell(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    Unit* target = lua_isnoneornil(state, 2) ? nullptr : CheckUnit(state, 2);
    uint32 spellId = static_cast<uint32>(luaL_checkinteger(state, 3));
    bool triggered = lua_isnoneornil(state, 4) ? false : lua_toboolean(state, 4) != 0;
    bool hasBp0 = !lua_isnoneornil(state, 5);
    int32 bp0 = static_cast<int32>(luaL_optinteger(state, 5, 0));
    bool hasBp1 = !lua_isnoneornil(state, 6);
    int32 bp1 = static_cast<int32>(luaL_optinteger(state, 6, 0));
    bool hasBp2 = !lua_isnoneornil(state, 7);
    int32 bp2 = static_cast<int32>(luaL_optinteger(state, 7, 0));
    Item* castItem = lua_isnoneornil(state, 8) ? nullptr : CheckItem(state, 8);
    ObjectGuid originalCaster = lua_isnoneornil(state, 9) ? ObjectGuid() : *CheckObjectGuid(state, 9);
    bool addThreat = lua_isnoneornil(state, 10) ? true : lua_toboolean(state, 10) != 0;

    if (unit && sSpellMgr.GetSpellEntry(spellId))
    {
        unit->CastCustomSpell(target, spellId, hasBp0 ? &bp0 : nullptr, hasBp1 ? &bp1 : nullptr, hasBp2 ? &bp2 : nullptr,
                              triggered, castItem, nullptr, addThreat, originalCaster);
    }
    return 0;
}

int UnitCastSpellAoF(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    float x = static_cast<float>(luaL_checknumber(state, 2));
    float y = static_cast<float>(luaL_checknumber(state, 3));
    float z = static_cast<float>(luaL_checknumber(state, 4));
    uint32 spellId = static_cast<uint32>(luaL_checkinteger(state, 5));
    bool triggered = lua_isnoneornil(state, 6) ? true : lua_toboolean(state, 6) != 0;

    lua_pushboolean(state, unit && sSpellMgr.GetSpellEntry(spellId) && unit->CastSpell(x, y, z, spellId, triggered) == SPELL_CAST_OK);
    return 1;
}

int UnitNearTeleport(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    float x = static_cast<float>(luaL_checknumber(state, 2));
    float y = static_cast<float>(luaL_checknumber(state, 3));
    float z = static_cast<float>(luaL_checknumber(state, 4));
    float o = static_cast<float>(luaL_checknumber(state, 5));

    lua_pushboolean(state, unit && unit->NearTeleportTo(x, y, z, o));
    return 1;
}

int UnitSendChatMessageToPlayer(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    uint32 type = static_cast<uint32>(luaL_checkinteger(state, 2));
    uint32 language = static_cast<uint32>(luaL_checkinteger(state, 3));
    char const* message = luaL_checkstring(state, 4);
    Player* target = CheckPlayer(state, 5);

    if (type != CHAT_MSG_ADDON && type >= MAX_CHAT_MSG_TYPE)
        return luaL_argerror(state, 2, "valid ChatMsg expected");
    if (language != LANG_ADDON && language >= LANGUAGES_COUNT)
        return luaL_argerror(state, 3, "valid Language expected");

    if (unit && target && target->GetSession())
    {
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, ChatMsg(type), message, Language(language), CHAT_TAG_NONE,
                                     unit->GetObjectGuid(), unit->GetName(),
                                     target->GetObjectGuid(), target->GetName());
        target->GetSession()->SendPacket(&data);
    }

    return 0;
}

int UnitSendUnitSay(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    char const* message = luaL_checkstring(state, 2);
    uint32 language = static_cast<uint32>(luaL_optinteger(state, 3, LANG_UNIVERSAL));
    if (language != LANG_ADDON && language >= LANGUAGES_COUNT)
        return luaL_argerror(state, 3, "valid Language expected");

    if (unit && *message)
    {
        if (Player* player = unit->ToPlayer())
            player->Say(message, language);
        else
            unit->MonsterSay(message, language, unit);
    }

    return 0;
}

int UnitSendUnitYell(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    char const* message = luaL_checkstring(state, 2);
    uint32 language = static_cast<uint32>(luaL_optinteger(state, 3, LANG_UNIVERSAL));
    if (language != LANG_ADDON && language >= LANGUAGES_COUNT)
        return luaL_argerror(state, 3, "valid Language expected");

    if (unit && *message)
    {
        if (Player* player = unit->ToPlayer())
            player->Yell(message, language);
        else
            unit->MonsterYell(message, language, unit);
    }

    return 0;
}

int UnitSendUnitWhisper(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    char const* message = luaL_checkstring(state, 2);

    int receiverIndex = 4;
    int bossWhisperIndex = 5;
    if (luaL_testudata(state, 3, PLAYER_METATABLE))
    {
        receiverIndex = 3;
        bossWhisperIndex = 4;
    }
    else
    {
        uint32 language = static_cast<uint32>(luaL_optinteger(state, 3, LANG_UNIVERSAL));
        if (language != LANG_ADDON && language >= LANGUAGES_COUNT)
            return luaL_argerror(state, 3, "valid Language expected");
    }

    Player* receiver = CheckPlayer(state, receiverIndex);
    bool bossWhisper = lua_isnoneornil(state, bossWhisperIndex) ? false : lua_toboolean(state, bossWhisperIndex) != 0;
    if (unit && receiver && *message)
        unit->MonsterWhisper(message, receiver, bossWhisper);

    return 0;
}

int UnitSendUnitEmote(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    char const* message = luaL_checkstring(state, 2);
    Unit* receiver = lua_isnoneornil(state, 3) ? nullptr : CheckUnit(state, 3);
    bool bossEmote = lua_isnoneornil(state, 4) ? false : lua_toboolean(state, 4) != 0;

    if (unit && *message)
    {
        if (Player* player = unit->ToPlayer())
            player->TextEmote(message);
        else
            unit->MonsterTextEmote(message, receiver, bossEmote);
    }

    return 0;
}

int UnitGetHealth(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushinteger(state, unit ? unit->GetHealth() : 0);
    return 1;
}

int UnitGetMaxHealth(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushinteger(state, unit ? unit->GetMaxHealth() : 0);
    return 1;
}

int UnitSetHealth(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    uint32 health = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (unit)
        unit->SetHealth(health);
    return 0;
}

int UnitIsAlive(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsAlive());
    return 1;
}

int UnitIsDead(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsDead());
    return 1;
}

int UnitIsDying(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->GetDeathState() == JUST_DIED);
    return 1;
}

int UnitIsMounted(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsMounted());
    return 1;
}

int UnitIsRooted(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && (unit->IsRooted() || unit->IsInRoots()));
    return 1;
}

int UnitIsFeared(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsFeared());
    return 1;
}

int UnitIsConfused(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && (unit->HasUnitState(UNIT_STAT_CONFUSED) || unit->HasAuraType(SPELL_AURA_MOD_CONFUSE)));
    return 1;
}

int UnitIsPolymorphed(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsPolymorphed());
    return 1;
}

int UnitIsStopped(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsStopped());
    return 1;
}

int UnitIsTaxiFlying(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsTaxiFlying());
    return 1;
}

int UnitIsCharmed(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsCharmed());
    return 1;
}

int UnitIsAttackingPlayer(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    Unit* victim = unit ? unit->GetVictim() : nullptr;
    lua_pushboolean(state, victim && victim->GetCharmerOrOwnerPlayerOrPlayerItself());
    return 1;
}

int UnitGetStandState(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushinteger(state, unit ? unit->GetStandState() : 0);
    return 1;
}

int UnitSetStandState(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    uint8 standState = static_cast<uint8>(luaL_checkinteger(state, 2));
    if (unit)
        unit->SetStandState(standState);
    return 0;
}

int UnitGetLevel(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushinteger(state, unit ? unit->GetLevel() : 0);
    return 1;
}

int UnitSetLevel(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    uint32 level = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (unit)
        unit->SetLevel(level);
    return 0;
}

bool CheckPowerType(lua_State* state, int index, Powers& power)
{
    int32 value = static_cast<int32>(luaL_checkinteger(state, index));
    if (value < 0 || value >= MAX_POWERS)
    {
        luaL_error(state, "power type must be between 0 and %u", MAX_POWERS - 1);
        return false;
    }

    power = static_cast<Powers>(value);
    return true;
}

bool CheckMoveType(lua_State* state, int index, UnitMoveType& moveType)
{
    int32 value = static_cast<int32>(luaL_checkinteger(state, index));
    if (value < 0 || value >= MAX_MOVE_TYPE)
    {
        luaL_error(state, "move type must be between 0 and %u", MAX_MOVE_TYPE - 1);
        return false;
    }

    moveType = static_cast<UnitMoveType>(value);
    return true;
}

int UnitGetPower(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    Powers power = unit ? unit->GetPowerType() : POWER_MANA;
    if (!lua_isnoneornil(state, 2) && !CheckPowerType(state, 2, power))
        return 0;

    lua_pushinteger(state, unit ? unit->GetPower(power) : 0);
    return 1;
}

int UnitGetMaxPower(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    Powers power = unit ? unit->GetPowerType() : POWER_MANA;
    if (!lua_isnoneornil(state, 2) && !CheckPowerType(state, 2, power))
        return 0;

    lua_pushinteger(state, unit ? unit->GetMaxPower(power) : 0);
    return 1;
}

int UnitSetPower(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    Powers power = unit ? unit->GetPowerType() : POWER_MANA;
    uint32 valueIndex = 2;

    if (lua_gettop(state) >= 3)
    {
        if (!CheckPowerType(state, 2, power))
            return 0;
        valueIndex = 3;
    }

    uint32 value = static_cast<uint32>(luaL_checkinteger(state, valueIndex));
    if (unit)
        unit->SetPower(power, value);
    return 0;
}

int UnitSetMaxPower(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    Powers power = unit ? unit->GetPowerType() : POWER_MANA;
    uint32 valueIndex = 2;

    if (lua_gettop(state) >= 3)
    {
        if (!CheckPowerType(state, 2, power))
            return 0;
        valueIndex = 3;
    }

    uint32 value = static_cast<uint32>(luaL_checkinteger(state, valueIndex));
    if (unit)
        unit->SetMaxPower(power, value);
    return 0;
}

int UnitModifyPower(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    int32 value = static_cast<int32>(luaL_checkinteger(state, 2));
    Powers power = unit ? unit->GetPowerType() : POWER_MANA;

    if (!lua_isnoneornil(state, 3) && !CheckPowerType(state, 3, power))
        return 0;

    if (unit)
        unit->ModifyPower(power, value);
    return 0;
}

int UnitGetPowerType(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushinteger(state, unit ? unit->GetPowerType() : POWER_MANA);
    return 1;
}

int UnitSetPowerType(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    Powers power = POWER_MANA;
    if (!CheckPowerType(state, 2, power))
        return 0;

    if (unit)
        unit->SetPowerType(power);
    return 0;
}

int UnitGetPowerPct(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    Powers power = unit ? unit->GetPowerType() : POWER_MANA;
    if (!lua_isnoneornil(state, 2) && !CheckPowerType(state, 2, power))
        return 0;

    lua_pushnumber(state, unit ? unit->GetPowerPercent(power) : 0.0f);
    return 1;
}

int UnitGetHealthPct(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushnumber(state, unit ? unit->GetHealthPercent() : 0.0f);
    return 1;
}

int UnitIsFullHealth(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsFullHealth());
    return 1;
}

int UnitHealthAbovePct(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    int32 pct = static_cast<int32>(luaL_checkinteger(state, 2));
    lua_pushboolean(state, unit && unit->HealthAbovePct(pct));
    return 1;
}

int UnitHealthBelowPct(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    int32 pct = static_cast<int32>(luaL_checkinteger(state, 2));
    lua_pushboolean(state, unit && unit->HealthBelowPct(pct));
    return 1;
}

int UnitCountPctFromCurHealth(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    int32 pct = static_cast<int32>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, unit ? uint32(float(pct) * unit->GetHealth() / 100.0f) : 0);
    return 1;
}

int UnitCountPctFromMaxHealth(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    int32 pct = static_cast<int32>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, unit ? unit->CountPctFromMaxHealth(pct) : 0);
    return 1;
}

int UnitSetMaxHealth(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    uint32 health = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (unit)
        unit->SetMaxHealth(health);
    return 0;
}

int UnitGetRace(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushinteger(state, unit ? unit->GetRace() : 0);
    return 1;
}

int UnitGetClass(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushinteger(state, unit ? unit->GetClass() : 0);
    return 1;
}

int UnitGetRaceMask(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushinteger(state, unit ? unit->GetRaceMask() : 0);
    return 1;
}

int UnitGetClassMask(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushinteger(state, unit ? unit->GetClassMask() : 0);
    return 1;
}

int UnitGetClassAsString(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    int32 locale = static_cast<int32>(luaL_optinteger(state, 2, LOCALE_enUS));
    if (locale < 0 || locale >= static_cast<int32>(MAX_DBC_LOCALE))
        return luaL_argerror(state, 2, "valid LocaleConstant expected");

    ChrClassesEntry const* entry = unit ? sChrClassesStore.LookupEntry(unit->GetClass()) : nullptr;
    if (!entry || !entry->name[locale] || !*entry->name[locale])
        lua_pushnil(state);
    else
        lua_pushstring(state, entry->name[locale]);
    return 1;
}

int UnitGetRaceAsString(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    int32 locale = static_cast<int32>(luaL_optinteger(state, 2, LOCALE_enUS));
    if (locale < 0 || locale >= static_cast<int32>(MAX_DBC_LOCALE))
        return luaL_argerror(state, 2, "valid LocaleConstant expected");

    ChrRacesEntry const* entry = unit ? sChrRacesStore.LookupEntry(unit->GetRace()) : nullptr;
    if (!entry || !entry->name[locale] || !*entry->name[locale])
        lua_pushnil(state);
    else
        lua_pushstring(state, entry->name[locale]);
    return 1;
}

int UnitGetCreatureType(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushinteger(state, unit ? unit->GetCreatureType() : 0);
    return 1;
}

int UnitGetStat(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    uint32 stat = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (stat >= MAX_STATS)
        return luaL_argerror(state, 2, "valid Stats value expected");

    lua_pushnumber(state, unit ? unit->GetStat(Stats(stat)) : 0.0f);
    return 1;
}

int UnitGetBaseSpellPower(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    uint32 spellSchool = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (spellSchool >= MAX_SPELL_SCHOOL)
        return luaL_argerror(state, 2, "valid SpellSchools value expected");

    lua_pushinteger(state, unit && unit->IsPlayer() ? unit->GetUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + spellSchool) : 0);
    return 1;
}

int UnitGetGender(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushinteger(state, unit ? unit->GetGender() : 0);
    return 1;
}

int UnitGetUnitState(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushinteger(state, unit ? unit->GetUnitState() : 0);
    return 1;
}

int UnitHasUnitState(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    uint32 stateFlags = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushboolean(state, unit && unit->HasUnitState(stateFlags));
    return 1;
}

int UnitAddUnitState(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    uint32 stateFlags = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (unit)
        unit->AddUnitState(stateFlags);
    return 0;
}

int UnitClearUnitState(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    uint32 stateFlags = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (unit)
        unit->ClearUnitState(stateFlags);
    return 0;
}

int UnitGetSpeed(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    UnitMoveType moveType = MOVE_RUN;
    if (!CheckMoveType(state, 2, moveType))
        return 0;

    lua_pushnumber(state, unit ? unit->GetSpeed(moveType) : 0.0f);
    return 1;
}

int UnitGetSpeedRate(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    UnitMoveType moveType = MOVE_RUN;
    if (!CheckMoveType(state, 2, moveType))
        return 0;

    lua_pushnumber(state, unit ? unit->GetSpeedRate(moveType) : 0.0f);
    return 1;
}

int UnitSetSpeed(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    UnitMoveType moveType = MOVE_RUN;
    if (!CheckMoveType(state, 2, moveType))
        return 0;

    float speed = static_cast<float>(luaL_checknumber(state, 3));
    if (speed < 0.0f)
        return luaL_argerror(state, 3, "speed must be non-negative");

    if (unit)
        unit->SetSpeedRate(moveType, baseMoveSpeed[moveType] > 0.0f ? speed / baseMoveSpeed[moveType] : speed);
    return 0;
}

int UnitSetSpeedRate(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    UnitMoveType moveType = MOVE_RUN;
    if (!CheckMoveType(state, 2, moveType))
        return 0;

    float rate = static_cast<float>(luaL_checknumber(state, 3));
    if (rate < 0.0f)
        return luaL_argerror(state, 3, "rate must be non-negative");

    if (unit)
        unit->SetSpeedRate(moveType, rate);
    return 0;
}

int UnitGetMovementType(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushinteger(state, unit ? unit->GetMotionMaster()->GetCurrentMovementGeneratorType() : IDLE_MOTION_TYPE);
    return 1;
}

int UnitMoveStop(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    if (unit)
        unit->StopMoving();
    return 0;
}

int UnitMoveExpire(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    bool reset = lua_isnoneornil(state, 2) ? true : lua_toboolean(state, 2) != 0;
    if (unit)
        unit->GetMotionMaster()->MovementExpired(reset);
    return 0;
}

int UnitMoveClear(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    bool reset = lua_isnoneornil(state, 2) ? true : lua_toboolean(state, 2) != 0;
    bool all = lua_isnoneornil(state, 3) ? false : lua_toboolean(state, 3) != 0;
    if (unit)
        unit->GetMotionMaster()->Clear(reset, all);
    return 0;
}

int UnitMoveIdle(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    if (unit)
        unit->GetMotionMaster()->MoveIdle();
    return 0;
}

int UnitMoveRandom(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    float radius = static_cast<float>(luaL_checknumber(state, 2));
    uint32 expireTime = static_cast<uint32>(luaL_optinteger(state, 3, 0));
    if (radius < 0.0f)
        return luaL_argerror(state, 2, "radius must be non-negative");

    if (unit)
        unit->GetMotionMaster()->MoveRandom(false, radius, expireTime);
    return 0;
}

int UnitMoveHome(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    if (unit)
        unit->GetMotionMaster()->MoveTargetedHome();
    return 0;
}

int UnitMoveFollow(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    Unit* target = CheckUnit(state, 2);
    float dist = static_cast<float>(luaL_optnumber(state, 3, 0.0));
    float angle = static_cast<float>(luaL_optnumber(state, 4, 0.0));
    if (unit && target)
        unit->GetMotionMaster()->MoveFollow(target, dist, angle);
    return 0;
}

int UnitMoveChase(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    Unit* target = CheckUnit(state, 2);
    float dist = static_cast<float>(luaL_optnumber(state, 3, 0.0));
    float angle = static_cast<float>(luaL_optnumber(state, 4, 0.0));
    if (unit && target)
        unit->GetMotionMaster()->MoveChase(target, dist, angle);
    return 0;
}

int UnitMoveConfused(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    if (unit)
        unit->GetMotionMaster()->MoveConfused();
    return 0;
}

int UnitMoveFleeing(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    Unit* target = CheckUnit(state, 2);
    uint32 time = static_cast<uint32>(luaL_optinteger(state, 3, 0));
    if (unit && target)
        unit->GetMotionMaster()->MoveFleeing(target, time);
    return 0;
}

int UnitMoveTo(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    uint32 id = static_cast<uint32>(luaL_checkinteger(state, 2));
    float x = static_cast<float>(luaL_checknumber(state, 3));
    float y = static_cast<float>(luaL_checknumber(state, 4));
    float z = static_cast<float>(luaL_checknumber(state, 5));
    bool genPath = lua_isnoneornil(state, 6) ? true : lua_toboolean(state, 6) != 0;
    if (unit)
        unit->GetMotionMaster()->MovePoint(id, x, y, z, genPath ? MOVE_PATHFINDING : MOVE_NONE);
    return 0;
}

int UnitMoveJump(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    float x = static_cast<float>(luaL_checknumber(state, 2));
    float y = static_cast<float>(luaL_checknumber(state, 3));
    float z = static_cast<float>(luaL_checknumber(state, 4));
    float horizontalSpeed = static_cast<float>(luaL_checknumber(state, 5));
    float maxHeight = static_cast<float>(luaL_checknumber(state, 6));
    uint32 id = static_cast<uint32>(luaL_optinteger(state, 7, 0));
    if (unit)
        unit->GetMotionMaster()->MoveJump(x, y, z, horizontalSpeed, maxHeight, id);
    return 0;
}

int UnitGetAttackers(lua_State* state)
{
    auto* engine = GetEngine(state);
    Unit* unit = CheckUnit(state, 1);

    lua_newtable(state);
    if (!engine || !unit)
        return 1;

    uint32 index = 1;
    for (Unit* attacker : unit->GetAttackers())
    {
        engine->PushUnit(attacker);
        lua_rawseti(state, -2, index++);
    }

    return 1;
}

int UnitClearThreatList(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    if (unit && unit->CanHaveThreatList())
        unit->DeleteThreatList();
    return 0;
}

int UnitGetThreatList(lua_State* state)
{
    auto* engine = GetEngine(state);
    Unit* unit = CheckUnit(state, 1);
    if (!engine || !unit || !unit->CanHaveThreatList() || !unit->IsInWorld())
    {
        lua_pushnil(state);
        return 1;
    }

    lua_newtable(state);
    uint32 index = 1;
    for (HostileReference* ref : unit->GetThreatManager().getThreatList())
    {
        if (!ref)
            continue;

        if (Unit* target = unit->GetMap()->GetUnit(ref->getUnitGuid()))
        {
            engine->PushUnit(target);
            lua_rawseti(state, -2, index++);
        }
    }

    return 1;
}

int UnitAddThreat(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    Unit* victim = CheckUnit(state, 2);
    float threat = static_cast<float>(luaL_checknumber(state, 3));
    uint32 arg4 = static_cast<uint32>(luaL_optinteger(state, 4, 0));
    uint32 arg5 = static_cast<uint32>(luaL_optinteger(state, 5, 0));

    uint32 schoolMask = arg4;
    uint32 spellId = arg5;
    if (arg4 > SPELL_SCHOOL_MASK_ALL && arg5 <= SPELL_SCHOOL_MASK_ALL)
    {
        spellId = arg4;
        schoolMask = arg5;
    }

    if (schoolMask > SPELL_SCHOOL_MASK_ALL)
        return luaL_argerror(state, 4, "valid SpellSchoolMask expected");

    SpellEntry const* spellInfo = spellId ? sSpellMgr.GetSpellEntry(spellId) : nullptr;
    if (unit && victim)
        unit->AddThreat(victim, threat, false, SpellSchoolMask(schoolMask), spellInfo);

    return 0;
}

int UnitModifyThreatPct(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    Unit* victim = CheckUnit(state, 2);
    int32 pct = static_cast<int32>(luaL_checkinteger(state, 3));
    if (unit && victim && unit->CanHaveThreatList())
        unit->GetThreatManager().modifyThreatPercent(victim, pct);
    return 0;
}

int UnitClearThreat(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    Unit* victim = CheckUnit(state, 2);
    if (unit && victim && unit->CanHaveThreatList())
        unit->GetThreatManager().modifyThreatPercent(victim, -100);
    return 0;
}

int UnitResetAllThreat(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    if (!unit || !unit->CanHaveThreatList() || !unit->IsInWorld())
        return 0;

    for (HostileReference* ref : unit->GetThreatManager().getThreatList())
    {
        if (!ref)
            continue;

        Unit* target = unit->GetMap()->GetUnit(ref->getUnitGuid());
        if (target && unit->GetThreatManager().getThreat(target) != 0.0f)
            unit->GetThreatManager().modifyThreatPercent(target, -100);
    }

    return 0;
}

int UnitGetThreat(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    Unit* victim = CheckUnit(state, 2);
    lua_pushnumber(state, unit && victim && unit->CanHaveThreatList() ? unit->GetThreatManager().getThreat(victim) : 0.0f);
    return 1;
}

int UnitGetFriendlyUnitsInRange(lua_State* state)
{
    auto* engine = GetEngine(state);
    Unit* unit = CheckUnit(state, 1);
    float range = static_cast<float>(luaL_optnumber(state, 2, SIZE_OF_GRIDS));

    lua_newtable(state);
    if (!engine || !unit || !unit->IsInWorld())
        return 1;

    std::list<Unit*> units;
    MaNGOS::AnyFriendlyUnitInObjectRangeCheck check(unit, range);
    MaNGOS::UnitListSearcher<MaNGOS::AnyFriendlyUnitInObjectRangeCheck> searcher(units, check);
    Cell::VisitAllObjects(unit, searcher, range);

    uint32 index = 1;
    for (Unit* found : units)
    {
        if (!found || found == unit)
            continue;

        engine->PushUnit(found);
        lua_rawseti(state, -2, index++);
    }

    return 1;
}

int UnitGetUnfriendlyUnitsInRange(lua_State* state)
{
    auto* engine = GetEngine(state);
    Unit* unit = CheckUnit(state, 1);
    float range = static_cast<float>(luaL_optnumber(state, 2, SIZE_OF_GRIDS));

    lua_newtable(state);
    if (!engine || !unit || !unit->IsInWorld())
        return 1;

    std::list<Unit*> units;
    MaNGOS::AnyUnfriendlyUnitInObjectRangeCheck check(unit, unit, range);
    MaNGOS::UnitListSearcher<MaNGOS::AnyUnfriendlyUnitInObjectRangeCheck> searcher(units, check);
    Cell::VisitAllObjects(unit, searcher, range);

    uint32 index = 1;
    for (Unit* found : units)
    {
        if (!found || found == unit)
            continue;

        engine->PushUnit(found);
        lua_rawseti(state, -2, index++);
    }

    return 1;
}

int UnitGetCurrentSpell(lua_State* state)
{
    auto* engine = GetEngine(state);
    Unit* unit = CheckUnit(state, 1);
    uint32 type = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (type >= CURRENT_MAX_SPELL)
        return luaL_argerror(state, 2, "valid CurrentSpellTypes expected");

    if (engine)
        engine->PushSpell(unit ? unit->GetCurrentSpell(CurrentSpellTypes(type)) : nullptr);
    else
        lua_pushnil(state);

    return 1;
}

int UnitHandleStatModifier(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    int32 stat = static_cast<int32>(luaL_checkinteger(state, 2));
    int32 type = static_cast<int32>(luaL_checkinteger(state, 3));
    float value = static_cast<float>(luaL_checknumber(state, 4));
    bool apply = lua_isnoneornil(state, 5) ? false : lua_toboolean(state, 5) != 0;

    int32 unitMod = int32(UNIT_MOD_STAT_START) + stat;
    if (unitMod < int32(UNIT_MOD_STAT_START) || unitMod >= int32(UNIT_MOD_STAT_END))
        return luaL_argerror(state, 2, "valid Stats value expected");

    if (type < 0 || type >= MODIFIER_TYPE_END)
        return luaL_argerror(state, 3, "valid UnitModifierType expected");

    lua_pushboolean(state, unit && unit->HandleStatModifier(UnitMods(unitMod), UnitModifierType(type), value, apply));
    return 1;
}

int UnitIsInAccessiblePlaceFor(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    Creature* creature = CheckCreature(state, 2);
    lua_pushboolean(state, unit && creature && unit->IsInAccessablePlaceFor(creature));
    return 1;
}

int UnitIsOnVehicle(lua_State* state)
{
    lua_pushboolean(state, false);
    return 1;
}

int UnitGetVehicle(lua_State* state)
{
    lua_pushnil(state);
    return 1;
}

int UnitGetVehicleKit(lua_State* state)
{
    lua_pushnil(state);
    return 1;
}

int UnitHasAura(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    uint32 spellId = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushboolean(state, unit && unit->HasAura(spellId));
    return 1;
}

int UnitGetAura(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    uint32 spellId = static_cast<uint32>(luaL_checkinteger(state, 2));
    int32 effIndex = static_cast<int32>(luaL_optinteger(state, 3, EFFECT_INDEX_0));

    Aura* aura = nullptr;
    if (unit && effIndex >= 0 && effIndex < MAX_EFFECT_INDEX)
        aura = unit->GetAura(spellId, SpellEffectIndex(effIndex));

    PushAuraValue(state, aura);
    return 1;
}

int UnitAddAura(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    uint32 spellId = static_cast<uint32>(luaL_checkinteger(state, 2));
    Unit* caster = lua_isnoneornil(state, 3) ? unit : CheckUnit(state, 3);
    PushAuraValue(state, unit ? GetFirstAuraFromHolder(unit->AddAura(spellId, 0, caster)) : nullptr);
    return 1;
}

int UnitRemoveAura(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    uint32 spellId = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (unit)
        unit->RemoveAurasDueToSpell(spellId);
    return 0;
}

int UnitRemoveAllAuras(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    if (unit)
        unit->RemoveAllAuras();
    return 0;
}

int UnitRemoveArenaAuras(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    bool onleave = lua_isnoneornil(state, 2) ? false : lua_toboolean(state, 2) != 0;
    if (unit)
        unit->RemoveArenaAuras(onleave);
    return 0;
}

int UnitGetVictim(lua_State* state)
{
    auto* engine = GetEngine(state);
    Unit* unit = CheckUnit(state, 1);
    Unit* victim = unit ? unit->GetVictim() : nullptr;

    if (engine)
        engine->PushUnit(victim);
    else
        lua_pushnil(state);

    return 1;
}

int UnitAttackStop(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    bool targetSwitch = lua_toboolean(state, 2) != 0;
    lua_pushboolean(state, unit && unit->AttackStop(targetSwitch));
    return 1;
}

int UnitAttack(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    Unit* target = CheckUnit(state, 2);
    bool melee = lua_isnoneornil(state, 3) ? true : lua_toboolean(state, 3) != 0;
    lua_pushboolean(state, unit && target && unit->Attack(target, melee));
    return 1;
}

int UnitKill(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    Unit* victim = CheckUnit(state, 2);
    bool durabilityLoss = lua_isnoneornil(state, 3) ? false : lua_toboolean(state, 3) != 0;
    if (unit && victim)
        unit->Kill(victim, nullptr, durabilityLoss);
    return 0;
}

int UnitDealDamage(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    Unit* target = CheckUnit(state, 2);
    uint32 damage = static_cast<uint32>(luaL_checkinteger(state, 3));
    bool durabilityLoss = lua_isnoneornil(state, 4) ? true : lua_toboolean(state, 4) != 0;
    uint32 school = static_cast<uint32>(luaL_optinteger(state, 5, MAX_SPELL_SCHOOL));
    uint32 spellId = static_cast<uint32>(luaL_optinteger(state, 6, 0));

    if (school > MAX_SPELL_SCHOOL)
        return luaL_argerror(state, 5, "valid SpellSchool expected");

    if (!unit || !target)
        return 0;

    if (school == MAX_SPELL_SCHOOL)
    {
        unit->DealDamage(target, damage, nullptr, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, nullptr, durabilityLoss);
        unit->SendAttackStateUpdate(HITINFO_AFFECTS_VICTIM, target, 1, SPELL_SCHOOL_MASK_NORMAL, damage, 0, 0, VICTIMSTATE_NORMAL, 0);
        return 0;
    }

    SpellEntry const* spellInfo = spellId ? sSpellMgr.GetSpellEntry(spellId) : nullptr;
    if (spellId && !spellInfo)
        return 0;

    SpellSchoolMask schoolMask = SpellSchoolMask(1 << school);
    if (spellInfo)
    {
        SpellNonMeleeDamage damageInfo(unit, target, spellInfo->Id, SpellSchools(spellInfo->School));
        damageInfo.damage = damage;
        target->CalculateAbsorbResistBlock(unit, &damageInfo, spellInfo);
        unit->DealDamageMods(target, damageInfo.damage, &damageInfo.absorb);
        unit->SendSpellNonMeleeDamageLog(&damageInfo);
        unit->DealSpellDamage(&damageInfo, durabilityLoss);
        return 0;
    }

    if (schoolMask & SPELL_SCHOOL_MASK_NORMAL)
        damage = unit->CalcArmorReducedDamage(target, damage);

    uint32 logDamage = damage;
    uint32 absorb = 0;
    int32 resist = 0;
    target->CalculateDamageAbsorbAndResist(unit, schoolMask, DIRECT_DAMAGE, damage, &absorb, &resist, nullptr);

    if (resist < 0)
        damage += uint32(-resist);

    uint32 malus = absorb + (resist > 0 ? uint32(resist) : 0);
    damage = damage <= malus ? 0 : damage - malus;

    unit->DealDamageMods(target, damage, &absorb);
    unit->DealDamage(target, damage, nullptr, DIRECT_DAMAGE, schoolMask, nullptr, durabilityLoss);
    unit->SendAttackStateUpdate(HITINFO_AFFECTS_VICTIM, target, 0, schoolMask, logDamage, absorb, resist, VICTIMSTATE_NORMAL, 0);
    return 0;
}

int UnitDealHeal(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    Unit* target = CheckUnit(state, 2);
    uint32 spellId = static_cast<uint32>(luaL_checkinteger(state, 3));
    uint32 amount = static_cast<uint32>(luaL_checkinteger(state, 4));
    bool critical = lua_isnoneornil(state, 5) ? false : lua_toboolean(state, 5) != 0;

    SpellEntry const* spellInfo = sSpellMgr.GetSpellEntry(spellId);
    if (unit && target && spellInfo)
        unit->DealHeal(target, amount, spellInfo, critical);
    return 0;
}

int UnitIsInCombat(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsInCombat());
    return 1;
}

int UnitClearInCombat(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    if (unit)
        unit->ClearInCombat();
    return 0;
}

int UnitSetInCombatWith(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    Unit* enemy = CheckUnit(state, 2);
    if (unit && enemy)
        unit->SetInCombatWith(enemy);
    return 0;
}

int UnitSetOwnerGUID(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    ObjectGuid const* guid = CheckObjectGuid(state, 2);
    if (unit && guid)
        unit->SetOwnerGuid(*guid);
    return 0;
}

int UnitSetCreatorGUID(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    ObjectGuid const* guid = CheckObjectGuid(state, 2);
    if (unit && guid)
        unit->SetCreatorGuid(*guid);
    return 0;
}

int UnitSetPetGUID(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    ObjectGuid const* guid = CheckObjectGuid(state, 2);
    if (unit && guid)
        unit->SetPetGuid(*guid);
    return 0;
}

int UnitSetCritterGUID(lua_State* state)
{
    CheckUnit(state, 1);
    CheckObjectGuid(state, 2);
    return 0;
}

int UnitSetName(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    char const* name = luaL_checkstring(state, 2);
    if (unit && *name)
    {
        if (Player* player = unit->ToPlayer())
            player->SetName(name);
    }
    return 0;
}

int UnitSetImmuneTo(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    uint32 immunity = static_cast<uint32>(luaL_checkinteger(state, 2));
    bool apply = lua_isnoneornil(state, 3) ? true : lua_toboolean(state, 3) != 0;
    if (unit)
        unit->ApplySpellImmune(0, IMMUNITY_MECHANIC, immunity, apply);
    return 0;
}

int UnitIsInWater(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsInWater());
    return 1;
}

int UnitIsUnderWater(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsUnderwater());
    return 1;
}

int UnitIsMoving(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsMoving());
    return 1;
}

int UnitIsFlying(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsFlying());
    return 1;
}

int UnitIsCasting(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsNonMeleeSpellCasted());
    return 1;
}

int UnitIsPvPFlagged(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsPvP());
    return 1;
}

int UnitIsStandState(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->GetStandState() == UNIT_STAND_STATE_STAND);
    return 1;
}

int UnitIsVendor(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsVendor());
    return 1;
}

int UnitIsTrainer(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsTrainer());
    return 1;
}

int UnitIsQuestGiver(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsQuestGiver());
    return 1;
}

int UnitIsGossip(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsGossip());
    return 1;
}

int UnitIsTaxi(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsTaxi());
    return 1;
}

int UnitIsGuildMaster(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsGuildMaster());
    return 1;
}

int UnitIsBattleMaster(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsBattleMaster());
    return 1;
}

int UnitIsBanker(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsBanker());
    return 1;
}

int UnitIsInnkeeper(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsInnkeeper());
    return 1;
}

int UnitIsSpiritHealer(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsSpiritHealer());
    return 1;
}

int UnitIsSpiritGuide(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsSpiritGuide());
    return 1;
}

int UnitIsTabardDesigner(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsTabardDesigner());
    return 1;
}

int UnitIsAuctioneer(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsAuctioner());
    return 1;
}

int UnitIsArmorer(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsArmorer());
    return 1;
}

int UnitIsServiceProvider(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsServiceProvider());
    return 1;
}

int UnitIsSpiritService(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushboolean(state, unit && unit->IsSpiritService());
    return 1;
}

int UnitGetDisplayId(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushinteger(state, unit ? unit->GetDisplayId() : 0);
    return 1;
}

int UnitSetDisplayId(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    uint32 displayId = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (unit)
        unit->SetDisplayId(displayId);
    return 0;
}

int UnitSetNativeDisplayId(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    uint32 displayId = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (unit)
        unit->SetNativeDisplayId(displayId);
    return 0;
}

int UnitGetNativeDisplayId(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushinteger(state, unit ? unit->GetNativeDisplayId() : 0);
    return 1;
}

int UnitRestoreDisplayId(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    if (unit)
        unit->DeMorph();
    return 0;
}

int UnitDeMorph(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    if (unit)
        unit->DeMorph();
    return 0;
}

int UnitGetMountId(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushinteger(state, unit ? unit->GetMountID() : 0);
    return 1;
}

int UnitMount(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    uint32 displayId = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (unit)
        unit->Mount(displayId);
    return 0;
}

int UnitDismount(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    if (unit)
        unit->Unmount();
    return 0;
}

int UnitGetFaction(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushinteger(state, unit ? unit->GetFactionTemplateId() : 0);
    return 1;
}

int UnitSetFaction(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    uint32 faction = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (unit)
        unit->SetFactionTemplateId(faction);
    return 0;
}

int UnitRestoreFaction(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    if (unit)
        unit->RestoreFaction();
    return 0;
}

int UnitSetSheath(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    uint32 sheath = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (sheath > SHEATH_STATE_RANGED)
        return luaL_argerror(state, 2, "valid SheathState value expected");

    if (unit)
        unit->SetSheath(SheathState(sheath));
    return 0;
}

int UnitSetRooted(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    bool apply = lua_isnoneornil(state, 2) ? true : lua_toboolean(state, 2) != 0;
    if (unit)
        unit->SetRooted(apply);
    return 0;
}

int UnitSetConfused(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    bool apply = lua_isnoneornil(state, 2) ? true : lua_toboolean(state, 2) != 0;
    if (unit)
        unit->SetConfused(apply);
    return 0;
}

int UnitSetFeared(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    bool apply = lua_isnoneornil(state, 2) ? true : lua_toboolean(state, 2) != 0;
    if (unit)
        unit->SetFeared(apply);
    return 0;
}

int UnitSetFacing(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    float orientation = static_cast<float>(luaL_checknumber(state, 2));
    if (unit)
        unit->SetFacingTo(orientation);
    return 0;
}

int UnitSetFacingToObject(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    WorldObject* target = CheckWorldObject(state, 2);
    if (unit && target)
        unit->SetFacingToObject(target);
    return 0;
}

int UnitSetPvP(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    bool apply = lua_isnoneornil(state, 2) ? true : lua_toboolean(state, 2) != 0;
    if (unit)
        unit->SetPvP(apply);
    return 0;
}

int UnitSetFFA(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    bool apply = lua_isnoneornil(state, 2) ? true : lua_toboolean(state, 2) != 0;
    if (unit)
    {
        if (apply)
            unit->SetByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_FFA_PVP);
        else
            unit->RemoveByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_FFA_PVP);
    }
    return 0;
}

int UnitSetSanctuary(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    bool apply = lua_isnoneornil(state, 2) ? true : lua_toboolean(state, 2) != 0;
    if (unit)
    {
        if (apply)
        {
            unit->SetByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_UNK3);
            if (Player* player = unit->ToPlayer())
                player->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_SANCTUARY);
            unit->CombatStop();
            unit->CombatStopWithPets();
        }
        else
        {
            unit->RemoveByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_UNK3);
            if (Player* player = unit->ToPlayer())
                player->RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_SANCTUARY);
        }
    }
    return 0;
}

int UnitSetWaterWalk(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    bool enable = lua_isnoneornil(state, 2) ? true : lua_toboolean(state, 2) != 0;
    if (unit)
        unit->SetWaterWalking(enable);
    return 0;
}

int UnitStopSpellCast(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    uint32 spellId = static_cast<uint32>(luaL_optinteger(state, 2, 0));
    if (unit)
        unit->CastStop(spellId);
    return 0;
}

int UnitInterruptSpell(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    int32 spellType = static_cast<int32>(luaL_checkinteger(state, 2));
    bool delayed = lua_isnoneornil(state, 3) ? true : lua_toboolean(state, 3) != 0;

    switch (spellType)
    {
        case 0:
            spellType = CURRENT_MELEE_SPELL;
            break;
        case 1:
            spellType = CURRENT_GENERIC_SPELL;
            break;
        case 2:
            spellType = CURRENT_CHANNELED_SPELL;
            break;
        case 3:
            spellType = CURRENT_AUTOREPEAT_SPELL;
            break;
        default:
            return luaL_argerror(state, 2, "valid CurrentSpellTypes value expected");
    }

    if (unit)
        unit->InterruptSpell(CurrentSpellTypes(spellType), delayed);
    return 0;
}

int UnitPerformEmote(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    uint32 emoteId = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (unit)
        unit->HandleEmoteCommand(emoteId);
    return 0;
}

int UnitEmoteState(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    uint32 emoteId = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (unit)
        unit->HandleEmoteState(emoteId);
    return 0;
}

int UnitGetOwner(lua_State* state)
{
    auto* engine = GetEngine(state);
    Unit* unit = CheckUnit(state, 1);
    Unit* owner = unit ? unit->GetOwner() : nullptr;
    if (engine)
        engine->PushUnit(owner);
    else
    lua_pushnil(state);
    return 1;
}

int UnitGetOwnerGUID(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushinteger(state, unit ? unit->GetOwnerGuid().GetCounter() : 0);
    return 1;
}

int UnitGetCreatorGUID(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushinteger(state, unit ? unit->GetCreatorGuid().GetCounter() : 0);
    return 1;
}

int UnitGetPetGUID(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushinteger(state, unit ? unit->GetPetGuid().GetCounter() : 0);
    return 1;
}

int UnitGetCritterGUID(lua_State* state)
{
    lua_pushinteger(state, 0);
    return 1;
}

int UnitGetControllerGUID(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushinteger(state, unit ? unit->GetCharmerOrOwnerGuid().GetCounter() : 0);
    return 1;
}

int UnitGetControllerGUIDS(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushinteger(state, unit ? unit->GetCharmerOrOwnerOrOwnGuid().GetCounter() : 0);
    return 1;
}

int UnitGetCharmer(lua_State* state)
{
    auto* engine = GetEngine(state);
    Unit* unit = CheckUnit(state, 1);
    Unit* charmer = unit ? unit->GetCharmer() : nullptr;
    if (engine)
        engine->PushUnit(charmer);
    else
        lua_pushnil(state);
    return 1;
}

int UnitGetCharmerGUID(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushinteger(state, unit ? unit->GetCharmerGuid().GetCounter() : 0);
    return 1;
}

int UnitGetCharm(lua_State* state)
{
    auto* engine = GetEngine(state);
    Unit* unit = CheckUnit(state, 1);
    Unit* charm = unit ? unit->GetCharm() : nullptr;
    if (engine)
        engine->PushUnit(charm);
    else
        lua_pushnil(state);
    return 1;
}

int UnitGetCharmGUID(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushinteger(state, unit ? unit->GetCharmGuid().GetCounter() : 0);
    return 1;
}

int UnitGetCharmerOrOwner(lua_State* state)
{
    auto* engine = GetEngine(state);
    Unit* unit = CheckUnit(state, 1);
    Unit* owner = unit ? unit->GetCharmerOrOwner() : nullptr;
    if (engine)
        engine->PushUnit(owner);
    else
        lua_pushnil(state);
    return 1;
}

int UnitGetCharmerOrOwnerGUID(lua_State* state)
{
    Unit* unit = CheckUnit(state, 1);
    lua_pushinteger(state, unit ? unit->GetCharmerOrOwnerGuid().GetCounter() : 0);
    return 1;
}

int CreatureGetName(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    lua_pushstring(state, creature ? creature->GetName() : "");
    return 1;
}

int CreatureGetEntry(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    lua_pushinteger(state, creature ? creature->GetEntry() : 0);
    return 1;
}

int CreatureGetTemplate(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    PushCreatureTemplateValue(state, creature ? creature->GetCreatureInfo() : nullptr);
    return 1;
}

int CreatureGetGUIDLow(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    lua_pushinteger(state, creature ? creature->GetGUIDLow() : 0);
    return 1;
}

int CreatureSay(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    char const* message = luaL_checkstring(state, 2);
    uint32 lang = static_cast<uint32>(luaL_optinteger(state, 3, LANG_UNIVERSAL));
    if (creature)
        creature->MonsterSay(message, lang);
    return 0;
}

int CreatureYell(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    char const* message = luaL_checkstring(state, 2);
    uint32 lang = static_cast<uint32>(luaL_optinteger(state, 3, LANG_UNIVERSAL));
    if (creature)
        creature->MonsterYell(message, lang);
    return 0;
}

int CreatureWhisper(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    char const* message = luaL_checkstring(state, 2);
    Player* player = CheckPlayer(state, 3);
    if (creature && player)
        creature->MonsterWhisper(message, player);
    return 0;
}

int CreatureTextEmote(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    char const* message = luaL_checkstring(state, 2);
    Unit* target = lua_isnoneornil(state, 3) ? nullptr : CheckUnit(state, 3);
    bool bossEmote = lua_toboolean(state, 4) != 0;
    if (creature)
        creature->MonsterTextEmote(message, target, bossEmote);
    return 0;
}

int CreatureCastSpell(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    Unit* target = CheckUnit(state, 2);
    uint32 spellId = static_cast<uint32>(luaL_checkinteger(state, 3));
    bool triggered = lua_toboolean(state, 4) != 0;

    lua_pushboolean(state, creature && target && creature->CastSpell(target, spellId, triggered) == SPELL_CAST_OK);
    return 1;
}

int CreatureDespawnOrUnsummon(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    uint32 delay = static_cast<uint32>(luaL_optinteger(state, 2, 0));
    if (creature)
        creature->ForcedDespawn(delay);
    return 0;
}

int CreatureGetDBTableGUIDLow(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    lua_pushinteger(state, creature ? creature->GetDBTableGUIDLow() : 0);
    return 1;
}

int CreatureGetRespawnDelay(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    lua_pushinteger(state, creature ? creature->GetRespawnDelay() : 0);
    return 1;
}

int CreatureSetRespawnDelay(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    uint32 delay = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (creature)
        creature->SetRespawnDelay(delay);
    return 0;
}

int CreatureCompatNoop(lua_State* state)
{
    (void)CheckCreature(state, 1);
    return 0;
}

int CreatureCompatReturnZero(lua_State* state)
{
    (void)CheckCreature(state, 1);
    lua_pushinteger(state, 0);
    return 1;
}

int CreatureCompatReturnFalse(lua_State* state)
{
    (void)CheckCreature(state, 1);
    lua_pushboolean(state, false);
    return 1;
}

int CreatureCanWalk(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    lua_pushboolean(state, creature && creature->CanWalk());
    return 1;
}

int CreatureCanSwim(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    lua_pushboolean(state, creature && creature->CanSwim());
    return 1;
}

int CreatureCanFly(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    lua_pushboolean(state, creature && creature->CanFly());
    return 1;
}

int CreatureIsElite(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    lua_pushboolean(state, creature && creature->IsElite());
    return 1;
}

int CreatureIsWorldBoss(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    lua_pushboolean(state, creature && creature->IsWorldBoss());
    return 1;
}

int CreatureIsDungeonBoss(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    lua_pushboolean(state, creature && creature->IsWorldBoss());
    return 1;
}

int CreatureIsCivilian(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    lua_pushboolean(state, creature && creature->IsCivilian());
    return 1;
}

int CreatureIsRacialLeader(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    lua_pushboolean(state, creature && creature->IsRacialLeader());
    return 1;
}

int CreatureIsTrigger(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    lua_pushboolean(state, creature && creature->IsTrigger());
    return 1;
}

int CreatureIsGuard(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    lua_pushboolean(state, creature && creature->IsGuard());
    return 1;
}

int CreatureIsInEvadeMode(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    lua_pushboolean(state, creature && creature->IsInEvadeMode());
    return 1;
}

int CreatureIsRegeneratingHealth(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    lua_pushboolean(state, creature && creature->IsRegeneratingHealth());
    return 1;
}

int CreatureSetRegeneratingHealth(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    bool enable = lua_isnoneornil(state, 2) ? true : lua_toboolean(state, 2) != 0;
    if (creature)
    {
        if (enable)
            creature->AddCreatureState(CSTATE_REGEN_HEALTH);
        else
            creature->ClearCreatureState(CSTATE_REGEN_HEALTH);
    }
    return 0;
}

int CreatureCanCompleteQuest(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    uint32 questId = CheckQuestIdValue(state, 2);
    lua_pushboolean(state, creature && creature->HasInvolvedQuest(questId));
    return 1;
}

int CreatureHasQuest(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    uint32 questId = CheckQuestIdValue(state, 2);
    lua_pushboolean(state, creature && creature->HasQuest(questId));
    return 1;
}

int CreatureHasSpell(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    uint32 spellId = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushboolean(state, creature && creature->HasSpell(spellId));
    return 1;
}

int CreatureHasSpellCooldown(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    uint32 spellId = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushboolean(state, creature && creature->HasSpellCooldown(spellId));
    return 1;
}

int CreatureGetCreatureSpellCooldownDelay(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    uint32 spellId = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, creature && sSpellMgr.GetSpellEntry(spellId) ? creature->GetSpellCooldownDelay(spellId) : 0);
    return 1;
}

int CreatureHasCategoryCooldown(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    uint32 spellOrCategory = static_cast<uint32>(luaL_checkinteger(state, 2));
    SpellEntry const* spellInfo = sSpellMgr.GetSpellEntry(spellOrCategory);
    uint32 category = spellInfo ? spellInfo->Category : spellOrCategory;
    lua_pushboolean(state, creature && category && creature->HasSpellCategoryCooldown(category));
    return 1;
}

int CreatureGetNPCFlags(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    lua_pushinteger(state, creature ? creature->GetUInt32Value(UNIT_NPC_FLAGS) : 0);
    return 1;
}

int CreatureSetNPCFlags(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    uint32 flags = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (creature)
        creature->SetUInt32Value(UNIT_NPC_FLAGS, flags);
    return 0;
}

int CreatureGetUnitFlags(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    lua_pushinteger(state, creature ? creature->GetUInt32Value(UNIT_FIELD_FLAGS) : 0);
    return 1;
}

int CreatureSetUnitFlags(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    uint32 flags = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (creature)
        creature->SetUInt32Value(UNIT_FIELD_FLAGS, flags);
    return 0;
}

int CreatureGetCreatureFamily(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    CreatureInfo const* info = creature ? creature->GetCreatureInfo() : nullptr;
    lua_pushinteger(state, info ? info->beast_family : 0);
    return 1;
}

int CreatureGetRank(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    CreatureInfo const* info = creature ? creature->GetCreatureInfo() : nullptr;
    lua_pushinteger(state, info ? info->rank : 0);
    return 1;
}

int CreatureGetExtraFlags(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    CreatureInfo const* info = creature ? creature->GetCreatureInfo() : nullptr;
    lua_pushinteger(state, info ? info->flags_extra : 0);
    return 1;
}

int CreatureGetAIName(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    std::string name = creature ? creature->GetAIName() : std::string();
    lua_pushstring(state, name.c_str());
    return 1;
}

int CreatureGetScriptName(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    std::string name = creature ? creature->GetScriptName() : std::string();
    lua_pushstring(state, name.c_str());
    return 1;
}

int CreatureGetScriptId(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    lua_pushinteger(state, creature ? creature->GetScriptId() : 0);
    return 1;
}

int CreatureGetShieldBlockValue(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    lua_pushinteger(state, creature ? creature->GetShieldBlockValue() : 0);
    return 1;
}

int CreatureGetHomePosition(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float o = 0.0f;
    if (creature)
        creature->GetHomePosition(x, y, z, o);

    lua_pushnumber(state, x);
    lua_pushnumber(state, y);
    lua_pushnumber(state, z);
    lua_pushnumber(state, o);
    return 4;
}

int CreatureSetHomePosition(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    float x = static_cast<float>(luaL_checknumber(state, 2));
    float y = static_cast<float>(luaL_checknumber(state, 3));
    float z = static_cast<float>(luaL_checknumber(state, 4));
    float o = static_cast<float>(luaL_optnumber(state, 5, creature ? creature->GetOrientation() : 0.0f));
    if (creature)
        creature->SetHomePosition(x, y, z, o);
    return 0;
}

int CreatureGetWanderRadius(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    lua_pushnumber(state, creature ? creature->GetWanderDistance() : 0.0f);
    return 1;
}

int CreatureSetWanderRadius(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    float distance = static_cast<float>(luaL_checknumber(state, 2));
    if (creature)
        creature->SetWanderDistance(distance);
    return 0;
}

int CreatureGetDefaultMovementType(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    lua_pushinteger(state, creature ? creature->GetDefaultMovementType() : 0);
    return 1;
}

int CreatureSetDefaultMovementType(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    uint32 movementType = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (creature)
        creature->SetDefaultMovementType(static_cast<MovementGeneratorType>(movementType));
    return 0;
}

int CreatureGetCurrentWaypointId(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    lua_pushinteger(state, creature ? creature->GetMotionMaster()->getLastReachedWaypoint() : 0);
    return 1;
}

int CreatureMoveWaypoint(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    uint32 startPoint = static_cast<uint32>(luaL_optinteger(state, 2, 0));
    uint32 source = static_cast<uint32>(luaL_optinteger(state, 3, 0));
    uint32 initialDelay = static_cast<uint32>(luaL_optinteger(state, 4, 0));
    bool repeat = lua_isnoneornil(state, 5) ? true : lua_toboolean(state, 5) != 0;
    if (creature)
        creature->GetMotionMaster()->MoveWaypoint(startPoint, source, initialDelay, 0, 0, repeat);
    return 0;
}

int CreatureGetLootRecipient(lua_State* state)
{
    auto* engine = GetEngine(state);
    Creature* creature = CheckCreature(state, 1);
    Player* player = creature ? creature->GetLootRecipient() : nullptr;
    if (engine)
        engine->PushPlayer(player);
    else
        lua_pushnil(state);
    return 1;
}

int CreatureGetLootRecipientGroup(lua_State* state)
{
    auto* engine = GetEngine(state);
    Creature* creature = CheckCreature(state, 1);
    Group* group = creature ? creature->GetGroupLootRecipient() : nullptr;
    if (engine)
        engine->PushGroup(group);
    else
        lua_pushnil(state);
    return 1;
}

int CreatureHasLootRecipient(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    lua_pushboolean(state, creature && creature->HasLootRecipient());
    return 1;
}

int CreatureIsTappedBy(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    Player* player = CheckPlayer(state, 2);
    lua_pushboolean(state, creature && player && creature->IsTappedBy(player));
    return 1;
}

int CreatureGetAITarget(lua_State* state)
{
    auto* engine = GetEngine(state);
    Creature* creature = CheckCreature(state, 1);
    uint32 targetType = static_cast<uint32>(luaL_checkinteger(state, 2));
    bool playerOnly = lua_toboolean(state, 3) != 0;
    uint32 position = static_cast<uint32>(luaL_optinteger(state, 4, 0));
    float distance = static_cast<float>(luaL_optnumber(state, 5, 0.0f));
    int32 aura = static_cast<int32>(luaL_optinteger(state, 6, 0));

    if (targetType > 4)
        return luaL_argerror(state, 2, "SelectAggroTarget expected");

    if (!engine || !creature || !creature->CanHaveThreatList())
    {
        lua_pushnil(state);
        return 1;
    }

    std::list<Unit*> targetList;
    for (HostileReference* ref : creature->GetThreatManager().getThreatList())
    {
        Unit* target = ref ? ref->getTarget() : nullptr;
        if (!target && ref && creature->IsInWorld())
            target = creature->GetMap()->GetUnit(ref->getUnitGuid());

        if (!target)
            continue;
        if (playerOnly && target->GetTypeId() != TYPEID_PLAYER)
            continue;
        if (aura > 0 && !target->HasAura(static_cast<uint32>(aura)))
            continue;
        if (aura < 0 && target->HasAura(static_cast<uint32>(-aura)))
            continue;
        if (distance > 0.0f && !creature->IsWithinDist(target, distance))
            continue;
        if (distance < 0.0f && creature->IsWithinDist(target, -distance))
            continue;

        targetList.push_back(target);
    }

    if (targetList.empty() || position >= targetList.size())
    {
        lua_pushnil(state);
        return 1;
    }

    if (targetType == 3 || targetType == 4)
        targetList.sort([creature](Unit const* left, Unit const* right) { return creature->GetDistanceOrder(left, right); });

    Unit* selected = nullptr;
    switch (targetType)
    {
        case 1:
        case 3:
        {
            auto itr = targetList.begin();
            std::advance(itr, position);
            selected = *itr;
            break;
        }
        case 2:
        case 4:
        {
            auto ritr = targetList.rbegin();
            std::advance(ritr, position);
            selected = *ritr;
            break;
        }
        case 0:
        {
            uint32 maxIndex = static_cast<uint32>(targetList.size() - 1);
            uint32 randomPosition = position ? urand(0, position) : urand(0, maxIndex);
            auto itr = targetList.begin();
            std::advance(itr, randomPosition);
            selected = *itr;
            break;
        }
        default:
            break;
    }

    engine->PushUnit(selected);
    return 1;
}

int CreatureGetAITargets(lua_State* state)
{
    auto* engine = GetEngine(state);
    Creature* creature = CheckCreature(state, 1);

    lua_newtable(state);
    if (!engine || !creature || !creature->CanHaveThreatList())
        return 1;

    uint32 index = 1;
    for (HostileReference* ref : creature->GetThreatManager().getThreatList())
    {
        Unit* target = ref ? ref->getTarget() : nullptr;
        if (!target && ref && creature->IsInWorld())
            target = creature->GetMap()->GetUnit(ref->getUnitGuid());

        if (!target)
            continue;

        engine->PushUnit(target);
        lua_rawseti(state, -2, index++);
    }

    return 1;
}

int CreatureGetAITargetsCount(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    lua_pushinteger(state, creature && creature->CanHaveThreatList() ? creature->GetThreatManager().getThreatList().size() : 0);
    return 1;
}

int CreatureSelectVictim(lua_State* state)
{
    auto* engine = GetEngine(state);
    Creature* creature = CheckCreature(state, 1);
    Unit* victim = nullptr;
    if (creature)
    {
        creature->SelectHostileTarget();
        victim = creature->GetVictim();
    }

    if (engine)
        engine->PushUnit(victim);
    else
        lua_pushnil(state);
    return 1;
}

int CreatureAttackStart(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    Unit* target = CheckUnit(state, 2);
    if (creature && target && creature->AI())
        creature->AI()->AttackStart(target);
    return 0;
}

int CreatureCallAssistance(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    if (creature)
        creature->CallAssistance();
    return 0;
}

int CreatureCallForHelp(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    float radius = static_cast<float>(luaL_checknumber(state, 2));
    if (creature)
        creature->CallForHelp(radius);
    return 0;
}

int CreatureFleeToGetAssistance(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    if (creature)
        creature->DoFleeToGetAssistance();
    return 0;
}

int CreatureCanAssistTo(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    Unit* unit = CheckUnit(state, 2);
    Unit* enemy = CheckUnit(state, 3);
    bool checkFaction = lua_isnoneornil(state, 4) ? true : lua_toboolean(state, 4) != 0;
    lua_pushboolean(state, creature && unit && enemy && creature->CanAssistTo(unit, enemy, checkFaction));
    return 1;
}

int CreatureCanAggro(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    lua_pushboolean(state, creature && !creature->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_NPC));
    return 1;
}

int CreatureSetAggroEnabled(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    bool allow = lua_isnoneornil(state, 2) ? true : lua_toboolean(state, 2) != 0;
    if (creature)
    {
        if (allow)
            creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_NPC);
        else
            creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_NPC);
    }
    return 0;
}

int CreatureCanStartAttack(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    Unit* target = CheckUnit(state, 2);
    bool force = lua_isnoneornil(state, 3) ? true : lua_toboolean(state, 3) != 0;
    lua_pushboolean(state, creature && target && creature->canStartAttack(target, force));
    return 1;
}

int CreatureGetAggroRange(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    Unit* target = CheckUnit(state, 2);
    lua_pushnumber(state, creature && target ? creature->GetAttackDistance(target) : 0.0f);
    return 1;
}

int CreatureSetInCombatWithZone(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    bool initialPulse = lua_isnoneornil(state, 2) ? true : lua_toboolean(state, 2) != 0;
    if (creature)
        creature->SetInCombatWithZone(initialPulse);
    return 0;
}

int CreatureIsTargetableForAttack(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    bool mustBeDead = lua_toboolean(state, 2) != 0;
    bool targetable = creature && creature->IsTargetable(true, false, false, false);
    if (targetable)
        targetable = mustBeDead ? !creature->IsAlive() : creature->IsAlive();
    lua_pushboolean(state, targetable);
    return 1;
}

int CreatureRemoveCorpse(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    if (creature)
        creature->RemoveCorpse();
    return 0;
}

int CreatureRespawn(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    if (creature)
        creature->Respawn();
    return 0;
}

int CreatureSaveToDB(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    if (creature)
        creature->SaveToDB();
    return 0;
}

int CreatureUpdateEntry(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    uint32 entry = static_cast<uint32>(luaL_checkinteger(state, 2));
    uint32 dataGuidLow = static_cast<uint32>(luaL_optinteger(state, 3, 0));
    CreatureData const* data = dataGuidLow ? sObjectMgr.GetCreatureData(dataGuidLow) : nullptr;
    if (creature)
        creature->UpdateEntry(entry, data);
    return 0;
}

int CreatureSetDeathState(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    int32 deathState = static_cast<int32>(luaL_checkinteger(state, 2));
    if (creature)
        creature->SetDeathState(static_cast<DeathState>(deathState));
    return 0;
}

int CreatureSetReactState(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    uint32 reactState = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (creature)
        creature->SetReactState(static_cast<ReactStates>(reactState));
    return 0;
}

int CreatureSetWalk(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    bool enable = lua_isnoneornil(state, 2) ? true : lua_toboolean(state, 2) != 0;
    if (creature)
        creature->SetWalk(enable);
    return 0;
}

int CreatureSetHover(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    bool enable = lua_isnoneornil(state, 2) ? true : lua_toboolean(state, 2) != 0;
    if (creature)
        creature->SetHover(enable);
    return 0;
}

int CreatureSetEquipmentSlots(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    uint32 mainHand = static_cast<uint32>(luaL_checkinteger(state, 2));
    uint32 offHand = static_cast<uint32>(luaL_checkinteger(state, 3));
    uint32 ranged = static_cast<uint32>(luaL_checkinteger(state, 4));
    if (creature)
    {
        creature->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, mainHand);
        creature->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, offHand);
        creature->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, ranged);
    }
    return 0;
}

int CreatureSetNoCallAssistance(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    bool value = lua_isnoneornil(state, 2) ? true : lua_toboolean(state, 2) != 0;
    if (creature)
        creature->SetNoCallAssistance(value);
    return 0;
}

int CreatureSetNoSearchAssistance(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    bool value = lua_isnoneornil(state, 2) ? true : lua_toboolean(state, 2) != 0;
    if (creature)
        creature->SetNoSearchAssistance(value);
    return 0;
}

int CreatureHasSearchedAssistance(lua_State* state)
{
    Creature* creature = CheckCreature(state, 1);
    lua_pushboolean(state, creature && creature->HasSearchedAssistance());
    return 1;
}

int CreatureGetLootMode(lua_State* state)
{
    (void)CheckCreature(state, 1);
    lua_pushinteger(state, 1);
    return 1;
}

int CreatureHasLootMode(lua_State* state)
{
    (void)CheckCreature(state, 1);
    uint32 lootMode = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushboolean(state, (lootMode & 1) != 0);
    return 1;
}

int WorldObjectSummonCreature(lua_State* state)
{
    auto* engine = GetEngine(state);
    WorldObject* object = CheckWorldObject(state, 1);
    uint32 entry = static_cast<uint32>(luaL_checkinteger(state, 2));
    float x = static_cast<float>(luaL_optnumber(state, 3, object ? object->GetPositionX() : 0.0f));
    float y = static_cast<float>(luaL_optnumber(state, 4, object ? object->GetPositionY() : 0.0f));
    float z = static_cast<float>(luaL_optnumber(state, 5, object ? object->GetPositionZ() : 0.0f));
    float o = static_cast<float>(luaL_optnumber(state, 6, object ? object->GetOrientation() : 0.0f));
    TempSummonType summonType = static_cast<TempSummonType>(luaL_optinteger(state, 7, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN));
    uint32 despawnTime = static_cast<uint32>(luaL_optinteger(state, 8, 60000));

    Creature* creature = object ? object->SummonCreature(entry, x, y, z, o, summonType, despawnTime) : nullptr;
    if (engine)
        engine->PushCreature(creature);
    else
        lua_pushnil(state);

    return 1;
}

int GameObjectGetName(lua_State* state)
{
    GameObject* go = CheckGameObject(state, 1);
    lua_pushstring(state, go ? go->GetName() : "");
    return 1;
}

int GameObjectGetEntry(lua_State* state)
{
    GameObject* go = CheckGameObject(state, 1);
    lua_pushinteger(state, go ? go->GetEntry() : 0);
    return 1;
}

int GameObjectGetTemplate(lua_State* state)
{
    GameObject* go = CheckGameObject(state, 1);
    PushGameObjectTemplateValue(state, go ? go->GetGOInfo() : nullptr);
    return 1;
}

int GameObjectGetGUIDLow(lua_State* state)
{
    GameObject* go = CheckGameObject(state, 1);
    lua_pushinteger(state, go ? go->GetGUIDLow() : 0);
    return 1;
}

int GameObjectGetDBTableGUIDLow(lua_State* state)
{
    GameObject* go = CheckGameObject(state, 1);
    lua_pushinteger(state, go ? go->GetDBTableGUIDLow() : 0);
    return 1;
}

int GameObjectGetGoType(lua_State* state)
{
    GameObject* go = CheckGameObject(state, 1);
    lua_pushinteger(state, go ? go->GetGoType() : 0);
    return 1;
}

int GameObjectGetGoState(lua_State* state)
{
    GameObject* go = CheckGameObject(state, 1);
    lua_pushinteger(state, go ? go->GetGoState() : 0);
    return 1;
}

int GameObjectSetGoState(lua_State* state)
{
    GameObject* go = CheckGameObject(state, 1);
    GOState stateId = static_cast<GOState>(luaL_checkinteger(state, 2));
    if (go)
        go->SetGoState(stateId);
    return 0;
}

int GameObjectGetLootState(lua_State* state)
{
    GameObject* go = CheckGameObject(state, 1);
    lua_pushinteger(state, go ? go->getLootState() : 0);
    return 1;
}

int GameObjectGetLootRecipient(lua_State* state)
{
    (void)CheckGameObject(state, 1);
    lua_pushnil(state);
    return 1;
}

int GameObjectGetLootRecipientGroup(lua_State* state)
{
    (void)CheckGameObject(state, 1);
    lua_pushnil(state);
    return 1;
}

int GameObjectSetLootState(lua_State* state)
{
    GameObject* go = CheckGameObject(state, 1);
    LootState stateId = static_cast<LootState>(luaL_checkinteger(state, 2));
    if (go)
        go->SetLootState(stateId);
    return 0;
}

int GameObjectGetDisplayId(lua_State* state)
{
    GameObject* go = CheckGameObject(state, 1);
    lua_pushinteger(state, go ? go->GetDisplayId() : 0);
    return 1;
}

int GameObjectSetDisplayId(lua_State* state)
{
    GameObject* go = CheckGameObject(state, 1);
    uint32 displayId = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (go)
        go->SetDisplayId(displayId);
    return 0;
}

int GameObjectGetRespawnDelay(lua_State* state)
{
    GameObject* go = CheckGameObject(state, 1);
    lua_pushinteger(state, go ? go->GetRespawnDelay() : 0);
    return 1;
}

int GameObjectSetRespawnDelay(lua_State* state)
{
    GameObject* go = CheckGameObject(state, 1);
    uint32 delay = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (go)
        go->SetRespawnDelay(delay);
    return 0;
}

int GameObjectSetRespawnTime(lua_State* state)
{
    GameObject* go = CheckGameObject(state, 1);
    int32 delay = static_cast<int32>(luaL_checkinteger(state, 2));
    if (go)
        go->SetRespawnTime(delay);
    return 0;
}

int GameObjectIsSpawned(lua_State* state)
{
    GameObject* go = CheckGameObject(state, 1);
    lua_pushboolean(state, go && go->isSpawned());
    return 1;
}

int GameObjectHasQuest(lua_State* state)
{
    GameObject* go = CheckGameObject(state, 1);
    uint32 questId = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushboolean(state, go && go->HasQuest(questId));
    return 1;
}

int GameObjectIsTransport(lua_State* state)
{
    GameObject* go = CheckGameObject(state, 1);
    lua_pushboolean(state, go && go->IsTransport());
    return 1;
}

int GameObjectIsActive(lua_State* state)
{
    GameObject* go = CheckGameObject(state, 1);
    lua_pushboolean(state, go && go->isActiveObject());
    return 1;
}

int GameObjectIsDestructible(lua_State* state)
{
    (void)CheckGameObject(state, 1);
    lua_pushboolean(state, false);
    return 1;
}

int GameObjectIsVisible(lua_State* state)
{
    GameObject* go = CheckGameObject(state, 1);
    lua_pushboolean(state, go && go->IsVisible());
    return 1;
}

int GameObjectSetVisible(lua_State* state)
{
    GameObject* go = CheckGameObject(state, 1);
    bool visible = lua_toboolean(state, 2) != 0;
    if (go)
        go->SetVisible(visible);
    return 0;
}

int GameObjectGetOwner(lua_State* state)
{
    auto* engine = GetEngine(state);
    GameObject* go = CheckGameObject(state, 1);
    Unit* owner = go ? go->GetOwner() : nullptr;
    if (engine)
        engine->PushUnit(owner);
    else
        lua_pushnil(state);
    return 1;
}

int GameObjectUse(lua_State* state)
{
    GameObject* go = CheckGameObject(state, 1);
    Unit* user = CheckUnit(state, 2);
    if (go && user)
        go->Use(user);
    return 0;
}

int GameObjectUseDoorOrButton(lua_State* state)
{
    GameObject* go = CheckGameObject(state, 1);
    uint32 delay = static_cast<uint32>(luaL_optinteger(state, 2, 0));
    if (go)
        go->UseDoorOrButton(delay);
    return 0;
}

int GameObjectRespawn(lua_State* state)
{
    GameObject* go = CheckGameObject(state, 1);
    if (go)
        go->Respawn();
    return 0;
}

int GameObjectDespawn(lua_State* state)
{
    GameObject* go = CheckGameObject(state, 1);
    if (go)
        go->Despawn();
    return 0;
}

int GameObjectRefresh(lua_State* state)
{
    GameObject* go = CheckGameObject(state, 1);
    if (go)
        go->Refresh();
    return 0;
}

int GameObjectSaveToDB(lua_State* state)
{
    GameObject* go = CheckGameObject(state, 1);
    if (go)
        go->SaveToDB();
    return 0;
}

int GameObjectRemoveFromWorld(lua_State* state)
{
    auto* holder = static_cast<LuaGameObject*>(luaL_checkudata(state, 1, GAMEOBJECT_METATABLE));
    GameObject* go = holder ? holder->go : nullptr;
    bool deleteFromDb = lua_toboolean(state, 2) != 0;

    if (!go)
        return 0;

    if (deleteFromDb)
        go->DeleteFromDB();

    if (ObjectGuid ownerGuid = go->GetOwnerGuid())
    {
        if (Unit* owner = ObjectAccessor::GetUnit(*go, ownerGuid))
            owner->RemoveGameObject(go, true);
        else
        {
            go->SetRespawnTime(0);
            go->Delete();
        }
    }
    else
    {
        go->SetRespawnTime(0);
        go->Delete();
    }

    holder->go = nullptr;
    return 0;
}

int GameObjectAddLoot(lua_State* state)
{
    GameObject* go = CheckGameObject(state, 1);
    if (!go)
        return 0;

    int top = lua_gettop(state);
    int added = 0;
    for (int index = 2; index <= top; index += 2)
    {
        uint32 entry = static_cast<uint32>(luaL_checkinteger(state, index));
        uint32 amount = static_cast<uint32>(luaL_optinteger(state, index + 1, 1));
        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(entry);
        if (!proto)
            return luaL_error(state, "Item entry %u does not exist", entry);
        if (amount < 1)
            return luaL_error(state, "Item entry %u has invalid amount %u", entry, amount);

        uint32 maxStack = proto->GetMaxStackSize() ? proto->GetMaxStackSize() : 1;
        maxStack = std::min<uint32>(maxStack, 255);
        if (amount > maxStack)
            amount = maxStack;

        go->loot.AddItem(LootStoreItem(entry, 100.0f, 0, 0, amount, static_cast<uint8>(amount)));
        lua_pushinteger(state, 0);
        ++added;
    }

    return added;
}

uint32 CheckGameObjectTemplateDataIndex(lua_State* state, int index)
{
    int32 value = static_cast<int32>(luaL_checkinteger(state, index));
    if (value < 1 || value > 24)
        luaL_argerror(state, index, "index out of range");

    return static_cast<uint32>(value - 1);
}

int GameObjectTemplateGetEntry(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    lua_pushinteger(state, info ? info->id : 0);
    return 1;
}

int GameObjectTemplateGetType(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    lua_pushinteger(state, info ? info->type : 0);
    return 1;
}

int GameObjectTemplateGetDisplayId(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    lua_pushinteger(state, info ? info->displayId : 0);
    return 1;
}

int GameObjectTemplateGetName(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    lua_pushstring(state, info ? info->name.c_str() : "");
    return 1;
}

int GameObjectTemplateGetFaction(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    lua_pushinteger(state, info ? info->faction : 0);
    return 1;
}

int GameObjectTemplateGetFlags(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    lua_pushinteger(state, info ? info->flags : 0);
    return 1;
}

int GameObjectTemplateGetSize(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    lua_pushnumber(state, info ? info->size : 0.0f);
    return 1;
}

int GameObjectTemplateGetData(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    uint32 index = CheckGameObjectTemplateDataIndex(state, 2);
    lua_pushinteger(state, info ? info->raw.data[index] : 0);
    return 1;
}

int GameObjectTemplateGetDataCount(lua_State* state)
{
    lua_pushinteger(state, 24);
    return 1;
}

int GameObjectTemplateGetMinMoneyLoot(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    lua_pushinteger(state, info ? info->MinMoneyLoot : 0);
    return 1;
}

int GameObjectTemplateGetMaxMoneyLoot(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    lua_pushinteger(state, info ? info->MaxMoneyLoot : 0);
    return 1;
}

int GameObjectTemplateGetPhaseQuestId(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    lua_pushinteger(state, info ? info->PhaseQuestId : 0);
    return 1;
}

int GameObjectTemplateGetScriptId(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    lua_pushinteger(state, info ? info->ScriptId : 0);
    return 1;
}

int GameObjectTemplateIsDespawnAtAction(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    lua_pushboolean(state, info && info->IsDespawnAtAction());
    return 1;
}

int GameObjectTemplateIsUsableMounted(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    lua_pushboolean(state, info && info->IsUsableMounted());
    return 1;
}

int GameObjectTemplateGetLockId(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    lua_pushinteger(state, info ? info->GetLockId() : 0);
    return 1;
}

int GameObjectTemplateGetDespawnPossibility(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    lua_pushboolean(state, info && info->GetDespawnPossibility());
    return 1;
}

int GameObjectTemplateCannotBeUsedUnderImmunity(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    lua_pushboolean(state, info && info->CannotBeUsedUnderImmunity());
    return 1;
}

int GameObjectTemplateGetCharges(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    lua_pushinteger(state, info ? info->GetCharges() : 0);
    return 1;
}

int GameObjectTemplateGetCooldown(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    lua_pushinteger(state, info ? info->GetCooldown() : 0);
    return 1;
}

int GameObjectTemplateGetLinkedGameObjectEntry(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    lua_pushinteger(state, info ? info->GetLinkedGameObjectEntry() : 0);
    return 1;
}

int GameObjectTemplateGetAutoCloseTime(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    lua_pushinteger(state, info ? info->GetAutoCloseTime() : 0);
    return 1;
}

int GameObjectTemplateGetLootId(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    lua_pushinteger(state, info ? info->GetLootId() : 0);
    return 1;
}

int GameObjectTemplateGetGossipMenuId(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    lua_pushinteger(state, info ? info->GetGossipMenuId() : 0);
    return 1;
}

int GameObjectTemplateIsLargeGameObject(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    lua_pushboolean(state, info && info->IsLargeGameObject());
    return 1;
}

int GameObjectTemplateIsInfiniteGameObject(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    lua_pushboolean(state, info && info->IsInfiniteGameObject());
    return 1;
}

int GameObjectTemplateIsServerOnly(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    lua_pushboolean(state, info && info->IsServerOnly());
    return 1;
}

int GameObjectTemplateGetInteractionDistance(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    lua_pushnumber(state, info ? info->GetInteractionDistance() : 0.0f);
    return 1;
}

int GameObjectTemplateGetEventScriptId(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    lua_pushinteger(state, info ? info->GetEventScriptId() : 0);
    return 1;
}

int GameObjectTemplateGetDeactivateTime(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    lua_pushinteger(state, info ? info->GetDeactivateTime() : 0);
    return 1;
}

int GameObjectTemplateGetQuestId(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    int32 questId = 0;

    if (info)
    {
        switch (info->type)
        {
            case GAMEOBJECT_TYPE_CHEST:       questId = static_cast<int32>(info->chest.questId); break;
            case GAMEOBJECT_TYPE_GENERIC:     questId = static_cast<int32>(info->_generic.questID); break;
            case GAMEOBJECT_TYPE_SPELL_FOCUS: questId = static_cast<int32>(info->spellFocus.questID); break;
            case GAMEOBJECT_TYPE_GOOBER:      questId = info->goober.questId; break;
            default: break;
        }
    }

    lua_pushinteger(state, questId);
    return 1;
}

int GameObjectTemplateGetSpellId(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    uint32 spellId = 0;

    if (info)
    {
        switch (info->type)
        {
            case GAMEOBJECT_TYPE_TRAP:             spellId = info->trap.spellId; break;
            case GAMEOBJECT_TYPE_GOOBER:           spellId = info->goober.spellId; break;
            case GAMEOBJECT_TYPE_SUMMONING_RITUAL: spellId = info->summoningRitual.spellId; break;
            case GAMEOBJECT_TYPE_SPELLCASTER:      spellId = info->spellcaster.spellId; break;
            case GAMEOBJECT_TYPE_CAPTURE_POINT:    spellId = info->capturePoint.spell; break;
            case GAMEOBJECT_TYPE_AURA_GENERATOR:   spellId = info->auraGenerator.auraID1; break;
            default: break;
        }
    }

    lua_pushinteger(state, spellId);
    return 1;
}

int GameObjectTemplateGetRadius(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    uint32 radius = 0;

    if (info)
    {
        switch (info->type)
        {
            case GAMEOBJECT_TYPE_TRAP:           radius = info->trap.radius; break;
            case GAMEOBJECT_TYPE_AREADAMAGE:     radius = info->areadamage.radius; break;
            case GAMEOBJECT_TYPE_FLAGSTAND:      radius = info->flagstand.radius; break;
            case GAMEOBJECT_TYPE_FISHINGHOLE:    radius = info->fishinghole.radius; break;
            case GAMEOBJECT_TYPE_CAPTURE_POINT:  radius = info->capturePoint.radius; break;
            case GAMEOBJECT_TYPE_AURA_GENERATOR: radius = info->auraGenerator.radius; break;
            default: break;
        }
    }

    lua_pushinteger(state, radius);
    return 1;
}

int GameObjectTemplateGetLevel(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    uint32 level = 0;

    if (info)
    {
        switch (info->type)
        {
            case GAMEOBJECT_TYPE_CHEST:        level = info->chest.level; break;
            case GAMEOBJECT_TYPE_TRAP:         level = info->trap.level; break;
            case GAMEOBJECT_TYPE_MEETINGSTONE: level = info->meetingstone.minLevel; break;
            default: break;
        }
    }

    lua_pushinteger(state, level);
    return 1;
}

int GameObjectTemplateGetObjectGuid(lua_State* state)
{
    GameObjectInfo const* info = CheckGameObjectTemplate(state, 1);
    uint32 lowguid = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (info)
        PushObjectGuidValue(state, ObjectGuid(HIGHGUID_GAMEOBJECT, info->id, lowguid));
    else
        lua_pushnil(state);

    return 1;
}

int ItemGetName(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushstring(state, item && item->GetProto() ? item->GetProto()->Name1.c_str() : "");
    return 1;
}

int ItemGetEntry(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushinteger(state, item ? item->GetEntry() : 0);
    return 1;
}

int ItemGetTemplate(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    PushItemTemplateValue(state, item ? item->GetProto() : nullptr);
    return 1;
}

int ItemGetGUIDLow(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushinteger(state, item ? item->GetGUIDLow() : 0);
    return 1;
}

int ItemGetCount(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushinteger(state, item ? item->GetCount() : 0);
    return 1;
}

int ItemSetCount(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    uint32 count = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (item)
        item->SetCount(count);
    return 0;
}

int ItemGetBagSlot(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushinteger(state, item ? item->GetBagSlot() : 0);
    return 1;
}

int ItemGetSlot(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushinteger(state, item ? item->GetSlot() : 0);
    return 1;
}

int ItemIsEquipped(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushboolean(state, item && item->IsEquipped());
    return 1;
}

int ItemIsSoulBound(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushboolean(state, item && item->IsSoulBound());
    return 1;
}

int ItemIsAccountBound(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushboolean(state, item && item->IsAccountBound());
    return 1;
}

int ItemIsBoundByEnchant(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushboolean(state, item && item->IsBoundByEnchant());
    return 1;
}

int ItemIsNotBoundToPlayer(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    Player* player = CheckPlayer(state, 2);
    lua_pushboolean(state, item && player && item->IsBindedNotWith(player));
    return 1;
}

int ItemIsLocked(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    bool locked = item && item->GetProto() && item->GetProto()->LockID
        && !item->HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_UNLOCKED);
    lua_pushboolean(state, locked);
    return 1;
}

int ItemIsBag(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushboolean(state, item && item->IsBag());
    return 1;
}

int ItemIsNotEmptyBag(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    Bag* bag = item ? item->ToBag() : nullptr;
    lua_pushboolean(state, bag && !bag->IsEmpty());
    return 1;
}

int ItemIsBroken(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushboolean(state, item && item->IsBroken());
    return 1;
}

int ItemCanBeTraded(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushboolean(state, item && item->CanBeTraded());
    return 1;
}

int ItemIsInTrade(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushboolean(state, item && item->IsInTrade());
    return 1;
}

int ItemIsInBag(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushboolean(state, item && item->IsInBag());
    return 1;
}

int ItemHasQuest(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    uint32 questId = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushboolean(state, item && item->HasQuest(questId));
    return 1;
}

int ItemIsPotion(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushboolean(state, item && item->IsPotion());
    return 1;
}

int ItemIsConjuredConsumable(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushboolean(state, item && item->IsConjuredConsumable());
    return 1;
}

int ItemIsCurrencyToken(lua_State* state)
{
    (void)CheckItem(state, 1);
    lua_pushboolean(state, false);
    return 1;
}

int ItemIsWeaponVellum(lua_State* state)
{
    (void)CheckItem(state, 1);
    lua_pushboolean(state, false);
    return 1;
}

int ItemIsArmorVellum(lua_State* state)
{
    (void)CheckItem(state, 1);
    lua_pushboolean(state, false);
    return 1;
}

int ItemIsRefundExpired(lua_State* state)
{
    (void)CheckItem(state, 1);
    lua_pushboolean(state, false);
    return 1;
}

int ItemGetItemLink(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    ItemPrototype const* proto = item ? item->GetProto() : nullptr;
    if (!proto)
    {
        lua_pushstring(state, "");
        return 1;
    }

    uint32 quality = proto->Quality < MAX_ITEM_QUALITY ? proto->Quality : ITEM_QUALITY_NORMAL;
    uint32 color = ItemQualityColors[quality];

    std::ostringstream link;
    link << "|c" << std::hex << std::nouppercase << color
        << "|Hitem:" << std::dec << proto->ItemId
        << ":0:0:0:0:0:0:0|h[" << proto->Name1 << "]|h|r";
    std::string text = link.str();
    lua_pushlstring(state, text.c_str(), text.size());
    return 1;
}

int ItemGetOwnerGUID(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushinteger(state, item ? item->GetOwnerGuid().GetCounter() : 0);
    return 1;
}

int ItemGetOwner(lua_State* state)
{
    auto* engine = GetEngine(state);
    Item* item = CheckItem(state, 1);
    Player* owner = item ? item->GetOwner() : nullptr;
    if (engine)
        engine->PushPlayer(owner);
    else
        lua_pushnil(state);
    return 1;
}

int ItemGetMaxStackCount(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushinteger(state, item ? item->GetMaxStackCount() : 0);
    return 1;
}

int ItemGetEnchantmentId(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    uint32 slot = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (slot >= MAX_ENCHANTMENT_SLOT)
        return luaL_argerror(state, 2, "valid EnchantmentSlot expected");

    lua_pushinteger(state, item ? item->GetEnchantmentId(EnchantmentSlot(slot)) : 0);
    return 1;
}

int ItemGetSpellId(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    uint32 index = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (index >= MAX_ITEM_PROTO_SPELLS)
        return luaL_argerror(state, 2, "valid SpellIndex expected");

    lua_pushinteger(state, item && item->GetProto() ? item->GetProto()->Spells[index].SpellId : 0);
    return 1;
}

int ItemGetSpellTrigger(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    uint32 index = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (index >= MAX_ITEM_PROTO_SPELLS)
        return luaL_argerror(state, 2, "valid SpellIndex expected");

    lua_pushinteger(state, item && item->GetProto() ? item->GetProto()->Spells[index].SpellTrigger : 0);
    return 1;
}

int ItemGetClass(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushinteger(state, item && item->GetProto() ? item->GetProto()->Class : 0);
    return 1;
}

int ItemGetSubClass(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushinteger(state, item && item->GetProto() ? item->GetProto()->SubClass : 0);
    return 1;
}

int ItemGetDisplayId(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushinteger(state, item && item->GetProto() ? item->GetProto()->DisplayInfoID : 0);
    return 1;
}

int ItemGetQuality(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushinteger(state, item && item->GetProto() ? item->GetProto()->Quality : 0);
    return 1;
}

int ItemGetBuyCount(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushinteger(state, item && item->GetProto() ? item->GetProto()->BuyCount : 0);
    return 1;
}

int ItemGetBuyPrice(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushinteger(state, item && item->GetProto() ? item->GetProto()->BuyPrice : 0);
    return 1;
}

int ItemGetSellPrice(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushinteger(state, item && item->GetProto() ? item->GetProto()->SellPrice : 0);
    return 1;
}

int ItemGetInventoryType(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushinteger(state, item && item->GetProto() ? item->GetProto()->InventoryType : 0);
    return 1;
}

int ItemGetAllowableClass(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushinteger(state, item && item->GetProto() ? item->GetProto()->AllowableClass : 0);
    return 1;
}

int ItemGetAllowableRace(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushinteger(state, item && item->GetProto() ? item->GetProto()->AllowableRace : 0);
    return 1;
}

int ItemGetItemLevel(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushinteger(state, item && item->GetProto() ? item->GetProto()->ItemLevel : 0);
    return 1;
}

int ItemGetRequiredLevel(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushinteger(state, item && item->GetProto() ? item->GetProto()->RequiredLevel : 0);
    return 1;
}

int ItemGetStatsCount(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    uint32 statsCount = 0;
    if (item && item->GetProto())
    {
        for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            if (item->GetProto()->ItemStat[i].ItemStatType)
                ++statsCount;
        }
    }

    lua_pushinteger(state, statsCount);
    return 1;
}

int ItemGetRandomProperty(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushinteger(state, item && item->GetProto() ? item->GetProto()->RandomProperty : 0);
    return 1;
}

int ItemGetRandomSuffix(lua_State* state)
{
    (void)CheckItem(state, 1);
    lua_pushinteger(state, 0);
    return 1;
}

int ItemGetRandomPropertyId(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushinteger(state, item ? item->GetItemRandomPropertyId() : 0);
    return 1;
}

int ItemGetItemSet(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    lua_pushinteger(state, item && item->GetProto() ? item->GetProto()->ItemSet : 0);
    return 1;
}

int ItemGetBagSize(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    Bag* bag = item ? item->ToBag() : nullptr;
    lua_pushinteger(state, bag ? bag->GetBagSize() : 0);
    return 1;
}

int ItemSetOwner(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    Player* player = CheckPlayer(state, 2);
    if (item && player)
    {
        item->SetOwnerGuid(player->GetObjectGuid());
        item->SetState(ITEM_CHANGED, player);
    }
    return 0;
}

int ItemSetBinding(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    bool soulbound = lua_toboolean(state, 2) != 0;
    if (item)
    {
        item->SetBinding(soulbound);
        item->SetState(ITEM_CHANGED, item->GetOwner());
    }
    return 0;
}

int ItemSetEnchantment(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    uint32 enchant = static_cast<uint32>(luaL_checkinteger(state, 2));
    uint32 slot = static_cast<uint32>(luaL_checkinteger(state, 3));
    if (slot >= MAX_ENCHANTMENT_SLOT)
        return luaL_argerror(state, 3, "valid EnchantmentSlot expected");

    Player* owner = item ? item->GetOwner() : nullptr;
    if (!item || !owner || !sSpellItemEnchantmentStore.LookupEntry(enchant))
    {
        lua_pushboolean(state, false);
        return 1;
    }

    EnchantmentSlot enchantSlot = EnchantmentSlot(slot);
    owner->ApplyEnchantment(item, enchantSlot, false);
    item->SetEnchantment(enchantSlot, enchant, 0, 0);
    owner->ApplyEnchantment(item, enchantSlot, true);
    lua_pushboolean(state, true);
    return 1;
}

int ItemClearEnchantment(lua_State* state)
{
    Item* item = CheckItem(state, 1);
    uint32 slot = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (slot >= MAX_ENCHANTMENT_SLOT)
        return luaL_argerror(state, 2, "valid EnchantmentSlot expected");

    Player* owner = item ? item->GetOwner() : nullptr;
    EnchantmentSlot enchantSlot = EnchantmentSlot(slot);
    if (!item || !owner || !item->GetEnchantmentId(enchantSlot))
    {
        lua_pushboolean(state, false);
        return 1;
    }

    owner->ApplyEnchantment(item, enchantSlot, false);
    item->ClearEnchantment(enchantSlot);
    lua_pushboolean(state, true);
    return 1;
}

uint32 CheckItemTemplateArrayIndex(lua_State* state, int index, uint32 max)
{
    int32 value = static_cast<int32>(luaL_checkinteger(state, index));
    if (value < 1 || static_cast<uint32>(value) > max)
        luaL_argerror(state, index, "index out of range");

    return static_cast<uint32>(value - 1);
}

int ItemTemplateGetEntry(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->ItemId : 0);
    return 1;
}

int ItemTemplateGetName(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushstring(state, proto ? proto->Name1.c_str() : "");
    return 1;
}

int ItemTemplateGetDescription(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushstring(state, proto ? proto->Description.c_str() : "");
    return 1;
}

int ItemTemplateGetClass(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->Class : 0);
    return 1;
}

int ItemTemplateGetSubClass(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->SubClass : 0);
    return 1;
}

int ItemTemplateGetQuality(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->Quality : 0);
    return 1;
}

int ItemTemplateGetDisplayId(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->DisplayInfoID : 0);
    return 1;
}

int ItemTemplateGetFlags(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->Flags : 0);
    return 1;
}

int ItemTemplateGetExtraFlags(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->ExtraFlags : 0);
    return 1;
}

int ItemTemplateGetIcon(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    if (!proto || !proto->DisplayInfoID)
    {
        lua_pushstring(state, "");
        return 1;
    }

    ItemDisplayInfoEntry const* display = sItemDisplayInfoStore.LookupEntry(proto->DisplayInfoID);
    lua_pushstring(state, display && display->inventoryIcon ? display->inventoryIcon : "");
    return 1;
}

int ItemTemplateGetBuyCount(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->BuyCount : 0);
    return 1;
}

int ItemTemplateGetBuyPrice(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->BuyPrice : 0);
    return 1;
}

int ItemTemplateGetSellPrice(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->SellPrice : 0);
    return 1;
}

int ItemTemplateGetInventoryType(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->InventoryType : 0);
    return 1;
}

int ItemTemplateGetAllowableClass(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? static_cast<int32>(proto->AllowableClass) : 0);
    return 1;
}

int ItemTemplateGetAllowableRace(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? static_cast<int32>(proto->AllowableRace) : 0);
    return 1;
}

int ItemTemplateGetItemLevel(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->ItemLevel : 0);
    return 1;
}

int ItemTemplateGetRequiredLevel(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->RequiredLevel : 0);
    return 1;
}

int ItemTemplateGetRequiredSkill(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->RequiredSkill : 0);
    return 1;
}

int ItemTemplateGetRequiredSkillRank(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->RequiredSkillRank : 0);
    return 1;
}

int ItemTemplateGetRequiredSpell(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->RequiredSpell : 0);
    return 1;
}

int ItemTemplateGetRequiredReputationFaction(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->RequiredReputationFaction : 0);
    return 1;
}

int ItemTemplateGetRequiredReputationRank(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->RequiredReputationRank : 0);
    return 1;
}

int ItemTemplateGetMaxCount(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->MaxCount : 0);
    return 1;
}

int ItemTemplateGetStackable(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->Stackable : 0);
    return 1;
}

int ItemTemplateGetMaxStackSize(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->GetMaxStackSize() : 0);
    return 1;
}

int ItemTemplateGetContainerSlots(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->ContainerSlots : 0);
    return 1;
}

int ItemTemplateGetDelay(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->Delay : 0);
    return 1;
}

int ItemTemplateGetAmmoType(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->AmmoType : 0);
    return 1;
}

int ItemTemplateGetBlock(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->Block : 0);
    return 1;
}

int ItemTemplateGetArmor(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->Armor : 0);
    return 1;
}

int ItemTemplateGetBonding(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->Bonding : 0);
    return 1;
}

int ItemTemplateGetStartQuest(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->StartQuest : 0);
    return 1;
}

int ItemTemplateGetItemSet(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->ItemSet : 0);
    return 1;
}

int ItemTemplateGetMaxDurability(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->MaxDurability : 0);
    return 1;
}

int ItemTemplateGetBagFamily(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->BagFamily : 0);
    return 1;
}

int ItemTemplateGetFoodType(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->FoodType : 0);
    return 1;
}

int ItemTemplateGetOtherTeamEntry(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushinteger(state, proto ? proto->OtherTeamEntry : 0);
    return 1;
}

int ItemTemplateGetStatsCount(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    uint32 statsCount = 0;
    if (proto)
        for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
            if (proto->ItemStat[i].ItemStatType)
                ++statsCount;

    lua_pushinteger(state, statsCount);
    return 1;
}

int ItemTemplateGetStatType(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    uint32 index = CheckItemTemplateArrayIndex(state, 2, MAX_ITEM_PROTO_STATS);
    lua_pushinteger(state, proto ? proto->ItemStat[index].ItemStatType : 0);
    return 1;
}

int ItemTemplateGetStatValue(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    uint32 index = CheckItemTemplateArrayIndex(state, 2, MAX_ITEM_PROTO_STATS);
    lua_pushinteger(state, proto ? proto->ItemStat[index].ItemStatValue : 0);
    return 1;
}

int ItemTemplateGetSpellId(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    uint32 index = CheckItemTemplateArrayIndex(state, 2, MAX_ITEM_PROTO_SPELLS);
    lua_pushinteger(state, proto ? proto->Spells[index].SpellId : 0);
    return 1;
}

int ItemTemplateGetSpellTrigger(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    uint32 index = CheckItemTemplateArrayIndex(state, 2, MAX_ITEM_PROTO_SPELLS);
    lua_pushinteger(state, proto ? proto->Spells[index].SpellTrigger : 0);
    return 1;
}

int ItemTemplateGetSpellCharges(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    uint32 index = CheckItemTemplateArrayIndex(state, 2, MAX_ITEM_PROTO_SPELLS);
    lua_pushinteger(state, proto ? proto->Spells[index].SpellCharges : 0);
    return 1;
}

int ItemTemplateGetSpellCooldown(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    uint32 index = CheckItemTemplateArrayIndex(state, 2, MAX_ITEM_PROTO_SPELLS);
    lua_pushinteger(state, proto ? proto->Spells[index].SpellCooldown : 0);
    return 1;
}

int ItemTemplateGetSpellCategory(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    uint32 index = CheckItemTemplateArrayIndex(state, 2, MAX_ITEM_PROTO_SPELLS);
    lua_pushinteger(state, proto ? proto->Spells[index].SpellCategory : 0);
    return 1;
}

int ItemTemplateGetSpellCategoryCooldown(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    uint32 index = CheckItemTemplateArrayIndex(state, 2, MAX_ITEM_PROTO_SPELLS);
    lua_pushinteger(state, proto ? proto->Spells[index].SpellCategoryCooldown : 0);
    return 1;
}

int ItemTemplateGetRecoveryTimeForSpell(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    uint32 spellId = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, proto ? proto->GetRecoveryTimeForSpell(spellId) : 0);
    return 1;
}

int ItemTemplateGetCategoryRecoveryTimeForSpell(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    uint32 spellId = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, proto ? proto->GetCategoryRecoveryTimeForSpell(spellId) : 0);
    return 1;
}

int ItemTemplateIsQuestItem(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushboolean(state, proto && proto->IsQuestItem);
    return 1;
}

int ItemTemplateIsPotion(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushboolean(state, proto && proto->IsPotion());
    return 1;
}

int ItemTemplateIsConjuredConsumable(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushboolean(state, proto && proto->IsConjuredConsumable());
    return 1;
}

int ItemTemplateIsWeapon(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushboolean(state, proto && proto->IsWeapon());
    return 1;
}

int ItemTemplateIsRangedWeapon(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushboolean(state, proto && proto->IsRangedWeapon());
    return 1;
}

int ItemTemplateIsOffHandItem(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushboolean(state, proto && proto->IsOffHandItem());
    return 1;
}

int ItemTemplateHasSignature(lua_State* state)
{
    ItemPrototype const* proto = CheckItemTemplate(state, 1);
    lua_pushboolean(state, proto && proto->HasSignature());
    return 1;
}

uint32 CheckCreatureTemplateArrayIndex(lua_State* state, int index, uint32 max)
{
    int32 value = static_cast<int32>(luaL_checkinteger(state, index));
    if (value < 1 || static_cast<uint32>(value) > max)
        luaL_argerror(state, index, "index out of range");

    return static_cast<uint32>(value - 1);
}

int CreatureTemplateGetEntry(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->entry : 0);
    return 1;
}

int CreatureTemplateGetName(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushstring(state, info ? info->name.c_str() : "");
    return 1;
}

int CreatureTemplateGetSubName(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushstring(state, info ? info->subname.c_str() : "");
    return 1;
}

int CreatureTemplateGetDisplayId(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    uint32 index = CheckCreatureTemplateArrayIndex(state, 2, MAX_DISPLAY_IDS_PER_CREATURE);
    lua_pushinteger(state, info ? info->display_id[index] : 0);
    return 1;
}

int CreatureTemplateGetDisplayIdCount(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    uint32 count = 0;
    if (info)
        for (uint32 i = 0; i < MAX_DISPLAY_IDS_PER_CREATURE; ++i)
            if (info->display_id[i])
                ++count;

    lua_pushinteger(state, count);
    return 1;
}

int CreatureTemplateGetMountDisplayId(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->mount_display_id : 0);
    return 1;
}

int CreatureTemplateGetGossipMenuId(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->gossip_menu_id : 0);
    return 1;
}

int CreatureTemplateGetMinLevel(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->level_min : 0);
    return 1;
}

int CreatureTemplateGetMaxLevel(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->level_max : 0);
    return 1;
}

int CreatureTemplateGetMinHealth(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->health_min : 0);
    return 1;
}

int CreatureTemplateGetMaxHealth(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->health_max : 0);
    return 1;
}

int CreatureTemplateGetMinMana(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->mana_min : 0);
    return 1;
}

int CreatureTemplateGetMaxMana(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->mana_max : 0);
    return 1;
}

int CreatureTemplateGetArmor(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->armor : 0);
    return 1;
}

int CreatureTemplateGetFaction(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->faction : 0);
    return 1;
}

int CreatureTemplateGetNpcFlags(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->npc_flags : 0);
    return 1;
}

int CreatureTemplateGetWalkSpeed(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushnumber(state, info ? info->speed_walk : 0.0f);
    return 1;
}

int CreatureTemplateGetRunSpeed(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushnumber(state, info ? info->speed_run : 0.0f);
    return 1;
}

int CreatureTemplateGetScale(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushnumber(state, info ? info->scale : 0.0f);
    return 1;
}

int CreatureTemplateGetDetectionRange(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushnumber(state, info ? info->detection_range : 0.0f);
    return 1;
}

int CreatureTemplateGetCallForHelpRange(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushnumber(state, info ? info->call_for_help_range : 0.0f);
    return 1;
}

int CreatureTemplateGetLeashRange(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushnumber(state, info ? info->leash_range : 0.0f);
    return 1;
}

int CreatureTemplateGetRank(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->rank : 0);
    return 1;
}

int CreatureTemplateGetXpMultiplier(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushnumber(state, info ? info->xp_multiplier : 0.0f);
    return 1;
}

int CreatureTemplateGetMinDamage(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushnumber(state, info ? info->dmg_min : 0.0f);
    return 1;
}

int CreatureTemplateGetMaxDamage(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushnumber(state, info ? info->dmg_max : 0.0f);
    return 1;
}

int CreatureTemplateGetDamageSchool(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->dmg_school : 0);
    return 1;
}

int CreatureTemplateGetAttackPower(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->attack_power : 0);
    return 1;
}

int CreatureTemplateGetDamageMultiplier(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushnumber(state, info ? info->dmg_multiplier : 0.0f);
    return 1;
}

int CreatureTemplateGetBaseAttackTime(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->base_attack_time : 0);
    return 1;
}

int CreatureTemplateGetRangedAttackTime(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->ranged_attack_time : 0);
    return 1;
}

int CreatureTemplateGetUnitClass(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->unit_class : 0);
    return 1;
}

int CreatureTemplateGetUnitFlags(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->unit_flags : 0);
    return 1;
}

int CreatureTemplateGetDynamicFlags(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->dynamic_flags : 0);
    return 1;
}

int CreatureTemplateGetFamily(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->beast_family : 0);
    return 1;
}

int CreatureTemplateGetTrainerType(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->trainer_type : 0);
    return 1;
}

int CreatureTemplateGetTrainerSpell(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->trainer_spell : 0);
    return 1;
}

int CreatureTemplateGetTrainerClass(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->trainer_class : 0);
    return 1;
}

int CreatureTemplateGetTrainerRace(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->trainer_race : 0);
    return 1;
}

int CreatureTemplateGetRangedMinDamage(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushnumber(state, info ? info->ranged_dmg_min : 0.0f);
    return 1;
}

int CreatureTemplateGetRangedMaxDamage(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushnumber(state, info ? info->ranged_dmg_max : 0.0f);
    return 1;
}

int CreatureTemplateGetRangedAttackPower(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->ranged_attack_power : 0);
    return 1;
}

int CreatureTemplateGetType(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->type : 0);
    return 1;
}

int CreatureTemplateGetTypeFlags(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->type_flags : 0);
    return 1;
}

int CreatureTemplateGetLootId(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->loot_id : 0);
    return 1;
}

int CreatureTemplateGetPickpocketLootId(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->pickpocket_loot_id : 0);
    return 1;
}

int CreatureTemplateGetSkinningLootId(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->skinning_loot_id : 0);
    return 1;
}

int CreatureTemplateGetResistance(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    uint32 school = static_cast<uint32>(luaL_checkinteger(state, 2));
    int32 resistance = 0;

    if (info)
    {
        switch (school)
        {
            case SPELL_SCHOOL_NORMAL: resistance = static_cast<int32>(info->armor); break;
            case SPELL_SCHOOL_HOLY:   resistance = info->holy_res; break;
            case SPELL_SCHOOL_FIRE:   resistance = info->fire_res; break;
            case SPELL_SCHOOL_NATURE: resistance = info->nature_res; break;
            case SPELL_SCHOOL_FROST:  resistance = info->frost_res; break;
            case SPELL_SCHOOL_SHADOW: resistance = info->shadow_res; break;
            case SPELL_SCHOOL_ARCANE: resistance = info->arcane_res; break;
            default:
                return luaL_argerror(state, 2, "valid spell school expected");
        }
    }

    lua_pushinteger(state, resistance);
    return 1;
}

int CreatureTemplateGetHolyResistance(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->holy_res : 0);
    return 1;
}

int CreatureTemplateGetFireResistance(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->fire_res : 0);
    return 1;
}

int CreatureTemplateGetNatureResistance(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->nature_res : 0);
    return 1;
}

int CreatureTemplateGetFrostResistance(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->frost_res : 0);
    return 1;
}

int CreatureTemplateGetShadowResistance(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->shadow_res : 0);
    return 1;
}

int CreatureTemplateGetArcaneResistance(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->arcane_res : 0);
    return 1;
}

int CreatureTemplateGetSpellId(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    uint32 index = CheckCreatureTemplateArrayIndex(state, 2, CREATURE_MAX_SPELLS);
    lua_pushinteger(state, info ? info->spells[index] : 0);
    return 1;
}

int CreatureTemplateGetSpellListId(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->spell_list_id : 0);
    return 1;
}

int CreatureTemplateGetPetSpellListId(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->pet_spell_list_id : 0);
    return 1;
}

int CreatureTemplateGetSpawnSpellId(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->spawn_spell_id : 0);
    return 1;
}

uint32 GetCreatureTemplateAuraCount(CreatureInfo const* info)
{
    uint32 count = 0;
    if (info && info->auras)
        for (uint32 const* aura = info->auras; *aura; ++aura)
            ++count;

    return count;
}

int CreatureTemplateGetAuraCount(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, GetCreatureTemplateAuraCount(info));
    return 1;
}

int CreatureTemplateGetAuraId(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    uint32 count = GetCreatureTemplateAuraCount(info);
    uint32 index = CheckCreatureTemplateArrayIndex(state, 2, count);
    lua_pushinteger(state, info && info->auras ? info->auras[index] : 0);
    return 1;
}

int CreatureTemplateGetMinGold(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->gold_min : 0);
    return 1;
}

int CreatureTemplateGetMaxGold(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->gold_max : 0);
    return 1;
}

int CreatureTemplateGetAIName(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushstring(state, info ? info->ai_name.c_str() : "");
    return 1;
}

int CreatureTemplateGetMovementType(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->movement_type : 0);
    return 1;
}

int CreatureTemplateGetInhabitType(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->inhabit_type : 0);
    return 1;
}

int CreatureTemplateIsCivilian(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushboolean(state, info && info->civilian);
    return 1;
}

int CreatureTemplateIsRacialLeader(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushboolean(state, info && info->racial_leader);
    return 1;
}

int CreatureTemplateGetRegenerationFlags(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->regeneration : 0);
    return 1;
}

int CreatureTemplateGetEquipmentId(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->equipment_id : 0);
    return 1;
}

int CreatureTemplateGetTrainerId(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->trainer_id : 0);
    return 1;
}

int CreatureTemplateGetVendorId(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->vendor_id : 0);
    return 1;
}

int CreatureTemplateGetMechanicImmuneMask(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->mechanic_immune_mask : 0);
    return 1;
}

int CreatureTemplateGetSchoolImmuneMask(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->school_immune_mask : 0);
    return 1;
}

int CreatureTemplateGetImmunityFlags(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->immunity_flags : 0);
    return 1;
}

int CreatureTemplateGetFlagsExtra(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->flags_extra : 0);
    return 1;
}

int CreatureTemplateHasFlagExtra(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    uint32 flag = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushboolean(state, info && (info->flags_extra & flag));
    return 1;
}

int CreatureTemplateGetPhaseQuestId(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->phase_quest_id : 0);
    return 1;
}

int CreatureTemplateGetScriptId(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushinteger(state, info ? info->script_id : 0);
    return 1;
}

int CreatureTemplateIsTameable(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    lua_pushboolean(state, info && info->isTameable());
    return 1;
}

int CreatureTemplateGetObjectGuid(lua_State* state)
{
    CreatureInfo const* info = CheckCreatureTemplate(state, 1);
    uint32 lowguid = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (info)
        PushObjectGuidValue(state, info->GetObjectGuid(lowguid));
    else
        lua_pushnil(state);

    return 1;
}

int QuestGetId(lua_State* state)
{
    Quest const* quest = CheckQuest(state, 1);
    lua_pushinteger(state, quest ? quest->GetQuestId() : 0);
    return 1;
}

int QuestGetTitle(lua_State* state)
{
    Quest const* quest = CheckQuest(state, 1);
    lua_pushstring(state, quest ? quest->GetTitle().c_str() : "");
    return 1;
}

int QuestGetLevel(lua_State* state)
{
    Quest const* quest = CheckQuest(state, 1);
    lua_pushinteger(state, quest ? quest->GetQuestLevel() : 0);
    return 1;
}

int QuestGetMinLevel(lua_State* state)
{
    Quest const* quest = CheckQuest(state, 1);
    lua_pushinteger(state, quest ? quest->GetMinLevel() : 0);
    return 1;
}

int QuestGetMaxLevel(lua_State* state)
{
    Quest const* quest = CheckQuest(state, 1);
    lua_pushinteger(state, quest ? quest->GetMaxLevel() : 0);
    return 1;
}

int QuestGetFlags(lua_State* state)
{
    Quest const* quest = CheckQuest(state, 1);
    lua_pushinteger(state, quest ? quest->GetQuestFlags() : 0);
    return 1;
}

int QuestHasFlag(lua_State* state)
{
    Quest const* quest = CheckQuest(state, 1);
    uint32 flag = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushboolean(state, quest && quest->HasQuestFlag(static_cast<QuestFlags>(flag)));
    return 1;
}

int QuestIsRepeatable(lua_State* state)
{
    Quest const* quest = CheckQuest(state, 1);
    lua_pushboolean(state, quest && quest->IsRepeatable());
    return 1;
}

int QuestIsDaily(lua_State* state)
{
    Quest const* quest = CheckQuest(state, 1);
    lua_pushboolean(state, quest && quest->HasSpecialFlag(QUEST_SPECIAL_FLAG_DAILY));
    return 1;
}

int QuestGetNextQuestId(lua_State* state)
{
    Quest const* quest = CheckQuest(state, 1);
    lua_pushinteger(state, quest ? quest->GetNextQuestId() : 0);
    return 1;
}

int QuestGetPrevQuestId(lua_State* state)
{
    Quest const* quest = CheckQuest(state, 1);
    lua_pushinteger(state, quest ? quest->GetPrevQuestId() : 0);
    return 1;
}

int QuestGetNextQuestInChain(lua_State* state)
{
    Quest const* quest = CheckQuest(state, 1);
    lua_pushinteger(state, quest ? quest->GetNextQuestInChain() : 0);
    return 1;
}

int QuestGetType(lua_State* state)
{
    Quest const* quest = CheckQuest(state, 1);
    lua_pushinteger(state, quest ? quest->GetType() : 0);
    return 1;
}

int GroupGetId(lua_State* state)
{
    Group* group = CheckGroup(state, 1);
    lua_pushinteger(state, group ? group->GetId() : 0);
    return 1;
}

int GroupGetMembersCount(lua_State* state)
{
    Group* group = CheckGroup(state, 1);
    lua_pushinteger(state, group ? group->GetMembersCount() : 0);
    return 1;
}

int GroupIsRaid(lua_State* state)
{
    Group* group = CheckGroup(state, 1);
    lua_pushboolean(state, group && group->isRaidGroup());
    return 1;
}

int GroupIsFull(lua_State* state)
{
    Group* group = CheckGroup(state, 1);
    lua_pushboolean(state, group && group->IsFull());
    return 1;
}

int GroupIsBGGroup(lua_State* state)
{
    Group* group = CheckGroup(state, 1);
    lua_pushboolean(state, group && group->isBGGroup());
    return 1;
}

int GroupIsLFGGroup(lua_State* state)
{
    Group* group = CheckGroup(state, 1);
    lua_pushboolean(state, group && group->isInLFG());
    return 1;
}

int GroupGetLeaderGUID(lua_State* state)
{
    Group* group = CheckGroup(state, 1);
    PushObjectGuidValue(state, group ? group->GetLeaderGuid() : ObjectGuid());
    return 1;
}

int GroupGetLeaderName(lua_State* state)
{
    Group* group = CheckGroup(state, 1);
    lua_pushstring(state, group ? group->GetLeaderName() : "");
    return 1;
}

int GroupGetLeader(lua_State* state)
{
    auto* engine = GetEngine(state);
    Group* group = CheckGroup(state, 1);
    Player* leader = group ? ObjectAccessor::FindPlayer(group->GetLeaderGuid()) : nullptr;
    if (engine)
        engine->PushPlayer(leader);
    else
        lua_pushnil(state);
    return 1;
}

int GroupIsLeader(lua_State* state)
{
    Group* group = CheckGroup(state, 1);
    ObjectGuid guid = CheckObjectGuidValue(state, 2);
    lua_pushboolean(state, group && !guid.IsEmpty() && group->IsLeader(guid));
    return 1;
}

int GroupIsMember(lua_State* state)
{
    Group* group = CheckGroup(state, 1);
    ObjectGuid guid = CheckObjectGuidValue(state, 2);
    lua_pushboolean(state, group && !guid.IsEmpty() && group->IsMember(guid));
    return 1;
}

int GroupIsAssistant(lua_State* state)
{
    Group* group = CheckGroup(state, 1);
    ObjectGuid guid = CheckObjectGuidValue(state, 2);
    lua_pushboolean(state, group && !guid.IsEmpty() && group->IsAssistant(guid));
    return 1;
}

int GroupSameSubGroup(lua_State* state)
{
    Group* group = CheckGroup(state, 1);
    Player* player1 = CheckPlayer(state, 2);
    Player* player2 = CheckPlayer(state, 3);
    lua_pushboolean(state, group && player1 && player2 && group->SameSubGroup(player1, player2));
    return 1;
}

int GroupHasFreeSlotSubGroup(lua_State* state)
{
    Group* group = CheckGroup(state, 1);
    uint8 subGroup = static_cast<uint8>(luaL_checkinteger(state, 2));

    if (subGroup >= MAX_RAID_SUBGROUPS)
        return luaL_argerror(state, 2, "valid subGroup ID expected");

    lua_pushboolean(state, group && group->HasFreeSlotSubGroup(subGroup));
    return 1;
}

int GroupGetMemberGroup(lua_State* state)
{
    Group* group = CheckGroup(state, 1);
    ObjectGuid guid = CheckObjectGuidValue(state, 2);
    lua_pushinteger(state, group && !guid.IsEmpty() ? group->GetMemberGroup(guid) : 0);
    return 1;
}

int GroupGetMembers(lua_State* state)
{
    auto* engine = GetEngine(state);
    Group* group = CheckGroup(state, 1);
    lua_newtable(state);

    if (!engine || !group)
        return 1;

    uint32 index = 1;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        if (Player* player = ref->getSource())
        {
            engine->PushPlayer(player);
            lua_rawseti(state, -2, index++);
        }
    }

    return 1;
}

int GroupGetGUID(lua_State* state)
{
    Group* group = CheckGroup(state, 1);
    lua_pushinteger(state, group ? group->GetId() : 0);
    return 1;
}

int GroupGetMemberGUID(lua_State* state)
{
    Group* group = CheckGroup(state, 1);
    char const* name = luaL_checkstring(state, 2);
    PushObjectGuidValue(state, group ? group->GetMemberGuid(name) : ObjectGuid());
    return 1;
}

int GroupGetGroupType(lua_State* state)
{
    Group* group = CheckGroup(state, 1);
    uint32 type = 0;

    if (group)
    {
        if (group->isBGGroup())
            type |= 1;
        if (group->isRaidGroup())
            type |= 2;
        if (group->isInLFG())
            type |= 8;
    }

    lua_pushinteger(state, type);
    return 1;
}

int GroupSetLeader(lua_State* state)
{
    Group* group = CheckGroup(state, 1);
    ObjectGuid guid = CheckObjectGuidValue(state, 2);
    if (group && !guid.IsEmpty())
        group->ChangeLeader(guid);
    return 0;
}

int GroupAddMember(lua_State* state)
{
    Group* group = CheckGroup(state, 1);
    Player* player = CheckPlayer(state, 2);

    if (!group || !player || player->GetGroup() || !group->IsCreated() || group->IsFull())
    {
        lua_pushboolean(state, false);
        return 1;
    }

    if (player->GetGroupInvite())
        player->UninviteFromGroup();

    bool added = group->AddMember(player->GetObjectGuid(), player->GetName());
    if (added)
        group->BroadcastGroupUpdate();

    lua_pushboolean(state, added);
    return 1;
}

int GroupRemoveMember(lua_State* state)
{
    Group* group = CheckGroup(state, 1);
    ObjectGuid guid = CheckObjectGuidValue(state, 2);
    uint32 method = static_cast<uint32>(luaL_optinteger(state, 3, GROUP_LEAVE));

    if (!group || guid.IsEmpty())
    {
        lua_pushboolean(state, false);
        return 1;
    }

    uint8 removeMethod = method == 1 ? GROUP_KICK : GROUP_LEAVE;
    lua_pushboolean(state, group->RemoveMember(guid, removeMethod) != 0);
    return 1;
}

int GroupDisband(lua_State* state)
{
    Group* group = CheckGroup(state, 1);
    if (group)
        group->Disband();
    return 0;
}

int GroupConvertToRaid(lua_State* state)
{
    Group* group = CheckGroup(state, 1);
    if (group && !group->isRaidGroup())
        group->ConvertToRaid();
    return 0;
}

int GroupSetMembersGroup(lua_State* state)
{
    Group* group = CheckGroup(state, 1);
    ObjectGuid guid = CheckObjectGuidValue(state, 2);
    uint8 subGroup = static_cast<uint8>(luaL_checkinteger(state, 3));

    if (subGroup >= MAX_RAID_SUBGROUPS)
        return luaL_argerror(state, 3, "valid subGroup ID expected");

    if (group && !guid.IsEmpty() && group->HasFreeSlotSubGroup(subGroup))
        group->ChangeMembersGroup(guid, subGroup);

    return 0;
}

int GroupSetTargetIcon(lua_State* state)
{
    Group* group = CheckGroup(state, 1);
    uint8 icon = static_cast<uint8>(luaL_checkinteger(state, 2));
    ObjectGuid target = CheckObjectGuidValue(state, 3);

    if (icon >= TARGET_ICON_COUNT)
        return luaL_argerror(state, 2, "valid target icon expected");

    if (group)
        group->SetTargetIcon(icon, target);

    return 0;
}

int GroupSetMemberFlag(lua_State* state)
{
    Group* group = CheckGroup(state, 1);
    ObjectGuid guid = CheckObjectGuidValue(state, 2);
    bool apply = lua_toboolean(state, 3) != 0;
    uint32 flag = static_cast<uint32>(luaL_checkinteger(state, 4));

    if (!group || guid.IsEmpty())
        return 0;

    if (flag & 0x01)
        group->SetAssistant(guid, apply);
    if (apply && (flag & 0x02))
        group->SetMainTank(guid);
    if (apply && (flag & 0x04))
        group->SetMainAssistant(guid);

    return 0;
}

int GroupSendPacket(lua_State* state)
{
    Group* group = CheckGroup(state, 1);
    WorldPacket* packet = CheckWorldPacket(state, 2);
    bool ignorePlayersInBg = lua_toboolean(state, 3) != 0;
    ObjectGuid ignore = CheckObjectGuidValue(state, 4);
    if (group && packet)
        group->BroadcastPacket(packet, ignorePlayersInBg, -1, ignore);
    return 0;
}

int GuildGetId(lua_State* state)
{
    Guild* guild = CheckGuild(state, 1);
    lua_pushinteger(state, guild ? guild->GetId() : 0);
    return 1;
}

int GuildGetName(lua_State* state)
{
    Guild* guild = CheckGuild(state, 1);
    lua_pushstring(state, guild ? guild->GetName().c_str() : "");
    return 1;
}

int GuildGetMOTD(lua_State* state)
{
    Guild* guild = CheckGuild(state, 1);
    lua_pushstring(state, guild ? guild->GetMOTD().c_str() : "");
    return 1;
}

int GuildGetInfo(lua_State* state)
{
    Guild* guild = CheckGuild(state, 1);
    lua_pushstring(state, guild ? guild->GetInfo().c_str() : "");
    return 1;
}

int GuildGetLeaderGUID(lua_State* state)
{
    Guild* guild = CheckGuild(state, 1);
    PushObjectGuidValue(state, guild ? guild->GetLeaderGuid() : ObjectGuid());
    return 1;
}

int GuildGetLeader(lua_State* state)
{
    auto* engine = GetEngine(state);
    Guild* guild = CheckGuild(state, 1);
    Player* leader = guild ? ObjectAccessor::FindPlayer(guild->GetLeaderGuid()) : nullptr;
    if (engine)
        engine->PushPlayer(leader);
    else
        lua_pushnil(state);
    return 1;
}

int GuildGetMemberCount(lua_State* state)
{
    Guild* guild = CheckGuild(state, 1);
    lua_pushinteger(state, guild ? guild->GetMemberSize() : 0);
    return 1;
}

int GuildGetRankName(lua_State* state)
{
    Guild* guild = CheckGuild(state, 1);
    uint32 rank = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushstring(state, guild ? guild->GetRankName(rank).c_str() : "");
    return 1;
}

int GuildGetRank(lua_State* state)
{
    Guild* guild = CheckGuild(state, 1);
    Player* player = CheckPlayer(state, 2);
    lua_pushinteger(state, guild && player ? guild->GetRank(player->GetObjectGuid()) : -1);
    return 1;
}

int GuildGetOnlineMembers(lua_State* state)
{
    auto* engine = GetEngine(state);
    Guild* guild = CheckGuild(state, 1);
    lua_newtable(state);

    if (!engine || !guild)
        return 1;

    uint32 index = 1;
    auto addPlayer = [&](Player* player)
    {
        engine->PushPlayer(player);
        lua_rawseti(state, -2, index++);
    };

    guild->BroadcastWorker(addPlayer);
    return 1;
}

int GuildGetMembers(lua_State* state)
{
    return GuildGetOnlineMembers(state);
}

int GuildSendMessage(lua_State* state)
{
    Guild* guild = CheckGuild(state, 1);

    if (!guild)
        return 0;

    if (luaL_testudata(state, 2, PLAYER_METATABLE))
    {
        Player* player = CheckPlayer(state, 2);
        bool officerOnly = lua_toboolean(state, 3) != 0;
        char const* message = luaL_checkstring(state, 4);
        uint32 language = static_cast<uint32>(luaL_optinteger(state, 5, LANG_UNIVERSAL));

        if (player && player->GetSession())
        {
            if (officerOnly)
                guild->BroadcastToOfficers(player->GetSession(), message, language);
            else
                guild->BroadcastToGuild(player->GetSession(), message, language);
        }

        return 0;
    }

    char const* message = luaL_checkstring(state, 2);
    auto sendMessage = [message](Player* player)
    {
        if (player && player->GetSession())
            player->GetSession()->SendNotification("%s", message);
    };

    guild->BroadcastWorker(sendMessage);
    return 0;
}

int GuildSetLeader(lua_State* state)
{
    Guild* guild = CheckGuild(state, 1);
    Player* player = CheckPlayer(state, 2);
    if (guild && player)
        guild->SetLeader(player->GetObjectGuid());
    return 0;
}

int GuildSetBankTabText(lua_State* state)
{
    Guild* guild = CheckGuild(state, 1);
    uint8 rawTab = static_cast<uint8>(luaL_checkinteger(state, 2));
    std::string text = luaL_checkstring(state, 3);

    uint8 tab = rawTab == 0 ? 1 : rawTab;
    if (!guild || tab == 0 || tab > 5)
        return 0;

    CharacterDatabase.escape_string(text);
    CharacterDatabase.PExecute("UPDATE guild_bank_tabs SET name%u = '%s' WHERE guildid = '%u' AND isInferno = 0",
        tab, text.c_str(), guild->GetId());
    return 0;
}

int GuildSendPacket(lua_State* state)
{
    Guild* guild = CheckGuild(state, 1);
    WorldPacket* packet = CheckWorldPacket(state, 2);
    if (guild && packet)
        guild->BroadcastPacket(packet);
    return 0;
}

int GuildSendPacketToRanked(lua_State* state)
{
    Guild* guild = CheckGuild(state, 1);
    WorldPacket* packet = CheckWorldPacket(state, 2);
    uint32 rankId = static_cast<uint32>(luaL_optinteger(state, 3, 0));
    if (guild && packet)
        guild->BroadcastPacketToRank(packet, rankId);
    return 0;
}

int GuildDisband(lua_State* state)
{
    Guild* guild = CheckGuild(state, 1);
    if (guild)
        guild->Disband();
    return 0;
}

int GuildAddMember(lua_State* state)
{
    Guild* guild = CheckGuild(state, 1);
    Player* player = CheckPlayer(state, 2);
    uint32 rank = guild ? static_cast<uint32>(luaL_optinteger(state, 3, guild->GetLowestRank())) : 0;

    if (guild && player)
        guild->AddMember(player->GetObjectGuid(), rank);

    return 0;
}

int GuildDeleteMember(lua_State* state)
{
    Guild* guild = CheckGuild(state, 1);
    Player* player = CheckPlayer(state, 2);
    bool isDisbanding = lua_toboolean(state, 3) != 0;

    if (guild && player)
        guild->DelMember(player->GetObjectGuid(), isDisbanding);

    return 0;
}

int GuildSetMemberRank(lua_State* state)
{
    Guild* guild = CheckGuild(state, 1);
    Player* player = CheckPlayer(state, 2);
    uint32 rank = static_cast<uint32>(luaL_checkinteger(state, 3));

    if (guild && player)
    {
        if (MemberSlot* slot = guild->GetMemberSlot(player->GetObjectGuid()))
            slot->ChangeRank(rank);
    }

    return 0;
}

int GuildSetName(lua_State* state)
{
    Guild* guild = CheckGuild(state, 1);
    std::string name = luaL_checkstring(state, 2);
    if (guild)
        guild->Rename(name);
    return 0;
}

int GuildUpdateMemberData(lua_State* state)
{
    Guild* guild = CheckGuild(state, 1);
    Player* player = CheckPlayer(state, 2);
    uint8 dataId = static_cast<uint8>(luaL_checkinteger(state, 3));
    uint32 value = static_cast<uint32>(luaL_checkinteger(state, 4));

    if (guild && player)
    {
        if (MemberSlot* slot = guild->GetMemberSlot(player->GetObjectGuid()))
        {
            if (dataId == 0)
                slot->ZoneId = value;
            else if (dataId == 1)
                slot->Level = static_cast<uint8>(value);
        }
    }

    return 0;
}

int GuildMassInviteToEvent(lua_State* state)
{
    CheckGuild(state, 1);
    CheckPlayer(state, 2);
    return 0;
}

int GuildSwapItems(lua_State* state)
{
    CheckGuild(state, 1);
    CheckPlayer(state, 2);
    return 0;
}

int GuildSwapItemsWithInventory(lua_State* state)
{
    CheckGuild(state, 1);
    CheckPlayer(state, 2);
    return 0;
}

int GuildGetTotalBankMoney(lua_State* state)
{
    Guild* guild = CheckGuild(state, 1);
    uint32 money = 0;

    if (guild)
    {
        std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
            "SELECT money FROM guild_bank_money WHERE guildid = '%u' AND isInferno = 0",
            guild->GetId()));
        if (result)
            money = result->Fetch()[0].GetUInt32();
    }

    lua_pushinteger(state, money);
    return 1;
}

int GuildGetCreatedDate(lua_State* state)
{
    Guild* guild = CheckGuild(state, 1);
    uint32 value = 0;

    if (guild)
        value = guild->GetCreatedYear() * 10000 + guild->GetCreatedMonth() * 100 + guild->GetCreatedDay();

    lua_pushinteger(state, value);
    return 1;
}

int GuildResetTimes(lua_State* state)
{
    CheckGuild(state, 1);
    return 0;
}

int GuildModifyBankMoney(lua_State* state)
{
    Guild* guild = CheckGuild(state, 1);
    uint32 amount = static_cast<uint32>(luaL_checkinteger(state, 2));
    bool add = lua_toboolean(state, 3) != 0;
    bool applied = false;

    if (guild)
    {
        if (add)
        {
            applied = CharacterDatabase.PExecute(
                "UPDATE guild_bank_money SET money = money + %u WHERE guildid = '%u' AND isInferno = 0",
                amount, guild->GetId());
        }
        else
        {
            applied = CharacterDatabase.PExecute(
                "UPDATE guild_bank_money SET money = IF(money > %u, money - %u, 0) WHERE guildid = '%u' AND isInferno = 0",
                amount, amount, guild->GetId());
        }
    }

    lua_pushboolean(state, applied);
    return 1;
}

int MapGetId(lua_State* state)
{
    Map* map = CheckMap(state, 1);
    lua_pushinteger(state, map ? map->GetId() : 0);
    return 1;
}

int MapGetInstanceId(lua_State* state)
{
    Map* map = CheckMap(state, 1);
    lua_pushinteger(state, map ? map->GetInstanceId() : 0);
    return 1;
}

int MapGetName(lua_State* state)
{
    Map* map = CheckMap(state, 1);
    lua_pushstring(state, map ? map->GetMapName() : "");
    return 1;
}

int MapIsDungeon(lua_State* state)
{
    Map* map = CheckMap(state, 1);
    lua_pushboolean(state, map && map->IsDungeon());
    return 1;
}

int MapIsRaid(lua_State* state)
{
    Map* map = CheckMap(state, 1);
    lua_pushboolean(state, map && map->IsRaid());
    return 1;
}

int MapIsBattleground(lua_State* state)
{
    Map* map = CheckMap(state, 1);
    lua_pushboolean(state, map && map->IsBattleGround());
    return 1;
}

int MapIsContinent(lua_State* state)
{
    Map* map = CheckMap(state, 1);
    lua_pushboolean(state, map && map->IsContinent());
    return 1;
}

int MapIsArena(lua_State* state)
{
    (void)CheckMap(state, 1);
    lua_pushboolean(state, false);
    return 1;
}

int MapIsEmpty(lua_State* state)
{
    Map* map = CheckMap(state, 1);
    lua_pushboolean(state, !map || !map->HavePlayers());
    return 1;
}

int MapIsHeroic(lua_State* state)
{
    (void)CheckMap(state, 1);
    lua_pushboolean(state, false);
    return 1;
}

int MapGetDifficulty(lua_State* state)
{
    (void)CheckMap(state, 1);
    lua_pushinteger(state, 0);
    return 1;
}

int MapGetHeight(lua_State* state)
{
    Map* map = CheckMap(state, 1);
    float x = static_cast<float>(luaL_checknumber(state, 2));
    float y = static_cast<float>(luaL_checknumber(state, 3));
    float z = static_cast<float>(luaL_optnumber(state, 4, MAX_HEIGHT));

    if (!map || !MaNGOS::IsValidMapCoord(x, y, z))
    {
        lua_pushnil(state);
        return 1;
    }

    float height = map->GetHeight(x, y, z);
    if (height <= INVALID_HEIGHT)
        lua_pushnil(state);
    else
        lua_pushnumber(state, height);
    return 1;
}

int MapGetAreaId(lua_State* state)
{
    Map* map = CheckMap(state, 1);
    float x = static_cast<float>(luaL_checknumber(state, 2));
    float y = static_cast<float>(luaL_checknumber(state, 3));
    float z = static_cast<float>(luaL_checknumber(state, 4));

    if (!map || !MaNGOS::IsValidMapCoord(x, y, z) || !map->GetTerrain())
    {
        lua_pushinteger(state, 0);
        return 1;
    }

    lua_pushinteger(state, map->GetTerrain()->GetAreaId(x, y, z));
    return 1;
}

template <class Predicate>
int PushDungeonCreatures(lua_State* state, Map* map, Predicate predicate)
{
    auto* engine = GetEngine(state);
    lua_newtable(state);

    if (!engine || !map)
        return 1;

    DungeonMap* dungeon = dynamic_cast<DungeonMap*>(map);
    if (!dungeon)
        return 1;

    std::shared_lock<std::shared_mutex> lock(dungeon->GetObjectLock());
    auto const& constStore = dungeon->GetObjectStore();
    typedef typename std::remove_const<typename std::remove_reference<decltype(constStore)>::type>::type StoreType;
    StoreType& store = const_cast<StoreType&>(constStore);
    auto range = store.template range<Creature>();

    uint32 index = 1;
    for (auto itr = range.first; itr != range.second; ++itr)
    {
        Creature* creature = itr->second;
        if (!creature || !predicate(creature))
            continue;

        engine->PushCreature(creature);
        lua_rawseti(state, -2, index++);
    }

    return 1;
}

int MapGetCreatures(lua_State* state)
{
    Map* map = CheckMap(state, 1);
    return PushDungeonCreatures(state, map, [](Creature*) { return true; });
}

int MapGetCreaturesByAreaId(lua_State* state)
{
    Map* map = CheckMap(state, 1);
    int32 areaId = static_cast<int32>(luaL_optinteger(state, 2, -1));
    return PushDungeonCreatures(state, map, [areaId](Creature* creature)
    {
        return areaId < 0 || creature->GetAreaId() == static_cast<uint32>(areaId);
    });
}

int MapGetPlayers(lua_State* state)
{
    auto* engine = GetEngine(state);
    Map* map = CheckMap(state, 1);
    lua_newtable(state);

    if (!engine || !map)
        return 1;

    uint32 index = 1;
    for (MapReference const* ref = map->GetPlayers().getFirst(); ref; ref = ref->next())
    {
        if (Player* player = ref->getSource())
        {
            engine->PushPlayer(player);
            lua_rawseti(state, -2, index++);
        }
    }

    return 1;
}

int MapGetPlayerCount(lua_State* state)
{
    Map* map = CheckMap(state, 1);
    uint32 count = 0;
    if (map)
    {
        for (MapReference const* ref = map->GetPlayers().getFirst(); ref; ref = ref->next())
            if (ref->getSource())
                ++count;
    }
    lua_pushinteger(state, count);
    return 1;
}

int MapGetPlayer(lua_State* state)
{
    auto* engine = GetEngine(state);
    Map* map = CheckMap(state, 1);
    ObjectGuid const* guid = CheckObjectGuid(state, 2);
    Player* player = map && guid ? map->GetPlayer(*guid) : nullptr;

    if (engine)
        engine->PushPlayer(player);
    else
        lua_pushnil(state);

    return 1;
}

int MapGetCreature(lua_State* state)
{
    auto* engine = GetEngine(state);
    Map* map = CheckMap(state, 1);
    ObjectGuid const* guid = CheckObjectGuid(state, 2);
    Creature* creature = map && guid ? map->GetAnyTypeCreature(*guid) : nullptr;

    if (engine)
        engine->PushCreature(creature);
    else
        lua_pushnil(state);

    return 1;
}

int MapGetGameObject(lua_State* state)
{
    auto* engine = GetEngine(state);
    Map* map = CheckMap(state, 1);
    ObjectGuid const* guid = CheckObjectGuid(state, 2);
    GameObject* go = map && guid ? map->GetGameObject(*guid) : nullptr;

    if (engine)
        engine->PushGameObject(go);
    else
        lua_pushnil(state);

    return 1;
}

int MapGetDynamicObject(lua_State* state)
{
    auto* engine = GetEngine(state);
    Map* map = CheckMap(state, 1);
    ObjectGuid const* guid = CheckObjectGuid(state, 2);
    DynamicObject* dynObj = map && guid ? map->GetDynamicObject(*guid) : nullptr;

    if (engine)
        engine->PushDynamicObject(dynObj);
    else
        lua_pushnil(state);

    return 1;
}

int MapGetUnit(lua_State* state)
{
    auto* engine = GetEngine(state);
    Map* map = CheckMap(state, 1);
    ObjectGuid const* guid = CheckObjectGuid(state, 2);
    Unit* unit = map && guid ? map->GetUnit(*guid) : nullptr;

    if (engine)
        engine->PushUnit(unit);
    else
        lua_pushnil(state);

    return 1;
}

int MapGetWorldObject(lua_State* state)
{
    auto* engine = GetEngine(state);
    Map* map = CheckMap(state, 1);
    ObjectGuid const* guid = CheckObjectGuid(state, 2);
    WorldObject* object = map && guid ? map->GetWorldObject(*guid) : nullptr;
    PushWorldObjectValue(state, engine, object);
    return 1;
}

int MapGetWorldObjectOrPlayer(lua_State* state)
{
    auto* engine = GetEngine(state);
    Map* map = CheckMap(state, 1);
    ObjectGuid const* guid = CheckObjectGuid(state, 2);
    WorldObject* object = map && guid ? map->GetWorldObjectOrPlayer(*guid) : nullptr;
    PushWorldObjectValue(state, engine, object);
    return 1;
}

int MapSetWeather(lua_State* state)
{
    Map* map = CheckMap(state, 1);
    uint32 zoneId = static_cast<uint32>(luaL_checkinteger(state, 2));
    WeatherType type = static_cast<WeatherType>(luaL_checkinteger(state, 3));
    float grade = static_cast<float>(luaL_checknumber(state, 4));
    bool permanently = lua_isnoneornil(state, 5) ? false : lua_toboolean(state, 5) != 0;

    if (map)
        map->SetWeather(zoneId, type, grade, permanently);

    return 0;
}

int MapGetInstanceData(lua_State* state)
{
    Map* map = CheckMap(state, 1);
    PushInstanceDataValue(state, map ? map->GetInstanceData() : nullptr);
    return 1;
}

int MapSaveInstanceData(lua_State* state)
{
    Map* map = CheckMap(state, 1);
    if (map && map->GetInstanceData())
        map->GetInstanceData()->SaveToDB();
    return 0;
}

int InstanceDataGetMap(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    InstanceData* data = CheckInstanceData(state, 1);
    if (engine && data)
        engine->PushMap(data->instance);
    else
        lua_pushnil(state);
    return 1;
}

int InstanceDataGetMapId(lua_State* state)
{
    InstanceData* data = CheckInstanceData(state, 1);
    lua_pushinteger(state, data && data->instance ? data->instance->GetId() : 0);
    return 1;
}

int InstanceDataGetInstanceId(lua_State* state)
{
    InstanceData* data = CheckInstanceData(state, 1);
    lua_pushinteger(state, data && data->instance ? data->instance->GetInstanceId() : 0);
    return 1;
}

int InstanceDataIsEncounterInProgress(lua_State* state)
{
    InstanceData* data = CheckInstanceData(state, 1);
    lua_pushboolean(state, data && data->IsEncounterInProgress());
    return 1;
}

int InstanceDataGetData(lua_State* state)
{
    InstanceData* data = CheckInstanceData(state, 1);
    uint32 key = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, data ? data->GetData(key) : 0);
    return 1;
}

int InstanceDataSetData(lua_State* state)
{
    InstanceData* data = CheckInstanceData(state, 1);
    uint32 key = static_cast<uint32>(luaL_checkinteger(state, 2));
    uint32 value = static_cast<uint32>(luaL_checkinteger(state, 3));
    if (data)
        data->SetData(key, value);
    return 0;
}

int InstanceDataGetData64(lua_State* state)
{
    InstanceData* data = CheckInstanceData(state, 1);
    uint32 key = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, data ? static_cast<lua_Integer>(data->GetData64(key)) : 0);
    return 1;
}

int InstanceDataSetData64(lua_State* state)
{
    InstanceData* data = CheckInstanceData(state, 1);
    uint32 key = static_cast<uint32>(luaL_checkinteger(state, 2));
    uint64 value = static_cast<uint64>(luaL_checkinteger(state, 3));
    if (data)
        data->SetData64(key, value);
    return 0;
}

int InstanceDataGetGuid(lua_State* state)
{
    InstanceData* data = CheckInstanceData(state, 1);
    uint32 key = static_cast<uint32>(luaL_checkinteger(state, 2));
    PushObjectGuidValue(state, data ? data->GetGuid(key) : ObjectGuid());
    return 1;
}

int InstanceDataSetGuid(lua_State* state)
{
    InstanceData* data = CheckInstanceData(state, 1);
    uint32 key = static_cast<uint32>(luaL_checkinteger(state, 2));
    ObjectGuid guid = CheckObjectGuidValue(state, 3);
    if (data)
        data->SetGuid(key, guid);
    return 0;
}

int InstanceDataSave(lua_State* state)
{
    InstanceData* data = CheckInstanceData(state, 1);
    char const* saved = data ? data->Save() : nullptr;
    if (saved)
        lua_pushstring(state, saved);
    else
        lua_pushnil(state);
    return 1;
}

int InstanceDataLoad(lua_State* state)
{
    InstanceData* data = CheckInstanceData(state, 1);
    char const* saved = luaL_checkstring(state, 2);
    if (data)
        data->Load(saved);
    return 0;
}

int InstanceDataSaveToDB(lua_State* state)
{
    InstanceData* data = CheckInstanceData(state, 1);
    if (data)
        data->SaveToDB();
    return 0;
}

int BattleGroundGetName(lua_State* state)
{
    BattleGround* bg = CheckBattleGround(state, 1);
    lua_pushstring(state, bg ? bg->GetName() : "");
    return 1;
}

int BattleGroundGetAlivePlayersCountByTeam(lua_State* state)
{
    BattleGround* bg = CheckBattleGround(state, 1);
    Team team = static_cast<Team>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, bg ? bg->GetAlivePlayersCountByTeam(team) : 0);
    return 1;
}

int BattleGroundGetMap(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    BattleGround* bg = CheckBattleGround(state, 1);
    if (engine && bg && bg->GetInstanceID())
        engine->PushMap(bg->GetBgMap());
    else
        lua_pushnil(state);
    return 1;
}

int BattleGroundGetBonusHonorFromKillCount(lua_State* state)
{
    BattleGround* bg = CheckBattleGround(state, 1);
    uint32 kills = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, bg ? bg->GetBonusHonorFromKill(kills) : 0);
    return 1;
}

int BattleGroundGetEndTime(lua_State* state)
{
    BattleGround* bg = CheckBattleGround(state, 1);
    lua_pushinteger(state, bg ? bg->GetEndTime() : 0);
    return 1;
}

int BattleGroundGetFreeSlotsForTeam(lua_State* state)
{
    BattleGround* bg = CheckBattleGround(state, 1);
    Team team = static_cast<Team>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, bg ? bg->GetFreeSlotsForTeam(team) : 0);
    return 1;
}

int BattleGroundGetInstanceId(lua_State* state)
{
    BattleGround* bg = CheckBattleGround(state, 1);
    lua_pushinteger(state, bg ? bg->GetInstanceID() : 0);
    return 1;
}

int BattleGroundGetMapId(lua_State* state)
{
    BattleGround* bg = CheckBattleGround(state, 1);
    lua_pushinteger(state, bg ? bg->GetMapId() : 0);
    return 1;
}

int BattleGroundGetTypeId(lua_State* state)
{
    BattleGround* bg = CheckBattleGround(state, 1);
    lua_pushinteger(state, bg ? bg->GetTypeID() : 0);
    return 1;
}

int BattleGroundGetMaxLevel(lua_State* state)
{
    BattleGround* bg = CheckBattleGround(state, 1);
    lua_pushinteger(state, bg ? bg->GetMaxLevel() : 0);
    return 1;
}

int BattleGroundGetMinLevel(lua_State* state)
{
    BattleGround* bg = CheckBattleGround(state, 1);
    lua_pushinteger(state, bg ? bg->GetMinLevel() : 0);
    return 1;
}

int BattleGroundGetMaxPlayers(lua_State* state)
{
    BattleGround* bg = CheckBattleGround(state, 1);
    lua_pushinteger(state, bg ? bg->GetMaxPlayers() : 0);
    return 1;
}

int BattleGroundGetMinPlayers(lua_State* state)
{
    BattleGround* bg = CheckBattleGround(state, 1);
    lua_pushinteger(state, bg ? bg->GetMinPlayers() : 0);
    return 1;
}

int BattleGroundGetMaxPlayersPerTeam(lua_State* state)
{
    BattleGround* bg = CheckBattleGround(state, 1);
    lua_pushinteger(state, bg ? bg->GetMaxPlayersPerTeam() : 0);
    return 1;
}

int BattleGroundGetMinPlayersPerTeam(lua_State* state)
{
    BattleGround* bg = CheckBattleGround(state, 1);
    lua_pushinteger(state, bg ? bg->GetMinPlayersPerTeam() : 0);
    return 1;
}

int BattleGroundGetWinner(lua_State* state)
{
    BattleGround* bg = CheckBattleGround(state, 1);
    lua_pushinteger(state, bg ? bg->GetWinner() : 0);
    return 1;
}

int BattleGroundGetStatus(lua_State* state)
{
    BattleGround* bg = CheckBattleGround(state, 1);
    lua_pushinteger(state, bg ? bg->GetStatus() : 0);
    return 1;
}

int BattleGroundIsArena(lua_State* state)
{
    BattleGround* bg = CheckBattleGround(state, 1);
    lua_pushboolean(state, bg && bg->IsArena());
    return 1;
}

int TicketIsClosed(lua_State* state)
{
    GmTicket* ticket = CheckTicket(state, 1);
    lua_pushboolean(state, ticket && ticket->IsClosed());
    return 1;
}

int TicketIsCompleted(lua_State* state)
{
    GmTicket* ticket = CheckTicket(state, 1);
    lua_pushboolean(state, ticket && ticket->IsCompleted());
    return 1;
}

int TicketIsFromPlayer(lua_State* state)
{
    GmTicket* ticket = CheckTicket(state, 1);
    ObjectGuid guid = CheckObjectGuidValue(state, 2);
    lua_pushboolean(state, ticket && ticket->IsFromPlayer(guid));
    return 1;
}

int TicketIsAssigned(lua_State* state)
{
    GmTicket* ticket = CheckTicket(state, 1);
    lua_pushboolean(state, ticket && ticket->IsAssigned());
    return 1;
}

int TicketIsAssignedTo(lua_State* state)
{
    GmTicket* ticket = CheckTicket(state, 1);
    ObjectGuid guid = CheckObjectGuidValue(state, 2);
    lua_pushboolean(state, ticket && ticket->IsAssignedTo(guid));
    return 1;
}

int TicketIsAssignedNotTo(lua_State* state)
{
    GmTicket* ticket = CheckTicket(state, 1);
    ObjectGuid guid = CheckObjectGuidValue(state, 2);
    lua_pushboolean(state, ticket && ticket->IsAssignedNotTo(guid));
    return 1;
}

int TicketGetId(lua_State* state)
{
    GmTicket* ticket = CheckTicket(state, 1);
    lua_pushinteger(state, ticket ? ticket->GetId() : 0);
    return 1;
}

int TicketGetPlayer(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    GmTicket* ticket = CheckTicket(state, 1);
    if (engine && ticket)
        engine->PushPlayer(ticket->GetPlayer());
    else
        lua_pushnil(state);
    return 1;
}

int TicketGetPlayerName(lua_State* state)
{
    GmTicket* ticket = CheckTicket(state, 1);
    lua_pushstring(state, ticket ? ticket->GetPlayerName().c_str() : "");
    return 1;
}

int TicketGetMessage(lua_State* state)
{
    GmTicket* ticket = CheckTicket(state, 1);
    lua_pushstring(state, ticket ? ticket->GetMessage().c_str() : "");
    return 1;
}

int TicketGetAssignedPlayer(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    GmTicket* ticket = CheckTicket(state, 1);
    if (engine && ticket)
        engine->PushPlayer(ticket->GetAssignedPlayer());
    else
        lua_pushnil(state);
    return 1;
}

int TicketGetAssignedToGUID(lua_State* state)
{
    GmTicket* ticket = CheckTicket(state, 1);
    PushObjectGuidValue(state, ticket ? ticket->GetAssignedToGUID() : ObjectGuid());
    return 1;
}

int TicketGetLastModifiedTime(lua_State* state)
{
    GmTicket* ticket = CheckTicket(state, 1);
    lua_pushinteger(state, ticket ? ticket->GetLastModifiedTime() : 0);
    return 1;
}

int TicketGetResponse(lua_State* state)
{
    GmTicket* ticket = CheckTicket(state, 1);
    lua_pushstring(state, ticket ? ticket->GetResponse().c_str() : "");
    return 1;
}

int TicketGetChatLog(lua_State* state)
{
    GmTicket* ticket = CheckTicket(state, 1);
    lua_pushstring(state, ticket ? ticket->GetChatLog().c_str() : "");
    return 1;
}

int TicketSetAssignedTo(lua_State* state)
{
    GmTicket* ticket = CheckTicket(state, 1);
    ObjectGuid guid = CheckObjectGuidValue(state, 2);
    bool isAdmin = lua_toboolean(state, 3) != 0;
    if (ticket)
    {
        ticket->SetAssignedTo(guid, isAdmin);
        ticket->SaveToDB();
    }
    return 0;
}

int TicketSetResolvedBy(lua_State* state)
{
    GmTicket* ticket = CheckTicket(state, 1);
    ObjectGuid guid = CheckObjectGuidValue(state, 2);
    if (ticket)
    {
        if (!ticket->IsClosed())
            sTicketMgr.CloseTicket(ticket->GetId(), guid);
        else
        {
            ticket->SetClosedBy(guid);
            ticket->SaveToDB();
        }

        if (TurtleLuaEngine* engine = GetEngine(state))
            engine->OnTicketResolve(ticket);
    }
    return 0;
}

int TicketSetCompleted(lua_State* state)
{
    GmTicket* ticket = CheckTicket(state, 1);
    if (ticket)
    {
        ticket->SetCompleted();
        ticket->SaveToDB();
        if (TurtleLuaEngine* engine = GetEngine(state))
            engine->OnTicketResolve(ticket);
    }
    return 0;
}

int TicketSetMessage(lua_State* state)
{
    GmTicket* ticket = CheckTicket(state, 1);
    char const* message = luaL_checkstring(state, 2);
    if (ticket)
    {
        ticket->SetMessage(message);
        ticket->SaveToDB();
    }
    return 0;
}

int TicketSetComment(lua_State* state)
{
    GmTicket* ticket = CheckTicket(state, 1);
    char const* comment = luaL_checkstring(state, 2);
    if (ticket)
    {
        ticket->SetComment(comment);
        ticket->SaveToDB();
    }
    return 0;
}

int TicketSetViewed(lua_State* state)
{
    GmTicket* ticket = CheckTicket(state, 1);
    if (ticket)
    {
        ticket->SetViewed();
        ticket->SaveToDB();
    }
    return 0;
}

int TicketSetUnassigned(lua_State* state)
{
    GmTicket* ticket = CheckTicket(state, 1);
    if (ticket)
    {
        ticket->SetUnassigned();
        ticket->SaveToDB();
    }
    return 0;
}

int TicketSetPosition(lua_State* state)
{
    GmTicket* ticket = CheckTicket(state, 1);
    uint32 mapId = static_cast<uint32>(luaL_checkinteger(state, 2));
    float x = static_cast<float>(luaL_checknumber(state, 3));
    float y = static_cast<float>(luaL_checknumber(state, 4));
    float z = static_cast<float>(luaL_checknumber(state, 5));
    if (ticket)
    {
        ticket->SetPosition(mapId, x, y, z);
        ticket->SaveToDB();
    }
    return 0;
}

int TicketAppendResponse(lua_State* state)
{
    GmTicket* ticket = CheckTicket(state, 1);
    char const* response = luaL_checkstring(state, 2);
    if (ticket)
    {
        ticket->AppendResponse(response);
        ticket->SaveToDB();
    }
    return 0;
}

int TicketDeleteResponse(lua_State* state)
{
    GmTicket* ticket = CheckTicket(state, 1);
    if (ticket)
    {
        ticket->ResetResponse();
        ticket->SaveToDB();
    }
    return 0;
}

int AuraGetCaster(lua_State* state)
{
    auto* engine = GetEngine(state);
    Aura* aura = CheckAura(state, 1);
    if (engine)
        engine->PushUnit(aura ? aura->GetCaster() : nullptr);
    else
        lua_pushnil(state);
    return 1;
}

int AuraGetCasterGUID(lua_State* state)
{
    Aura* aura = CheckAura(state, 1);
    PushObjectGuidValue(state, aura ? aura->GetCasterGuid() : ObjectGuid());
    return 1;
}

int AuraGetCasterGUIDLow(lua_State* state)
{
    Aura* aura = CheckAura(state, 1);
    lua_pushinteger(state, aura ? aura->GetCasterGuid().GetCounter() : 0);
    return 1;
}

int AuraGetCasterLevel(lua_State* state)
{
    Aura* aura = CheckAura(state, 1);
    Unit* caster = aura ? aura->GetCaster() : nullptr;
    lua_pushinteger(state, caster ? caster->GetLevel() : 0);
    return 1;
}

int AuraGetDuration(lua_State* state)
{
    Aura* aura = CheckAura(state, 1);
    lua_pushinteger(state, aura ? aura->GetAuraDuration() : 0);
    return 1;
}

int AuraGetMaxDuration(lua_State* state)
{
    Aura* aura = CheckAura(state, 1);
    lua_pushinteger(state, aura ? aura->GetAuraMaxDuration() : 0);
    return 1;
}

int AuraGetAuraId(lua_State* state)
{
    Aura* aura = CheckAura(state, 1);
    lua_pushinteger(state, aura ? aura->GetId() : 0);
    return 1;
}

int AuraGetStackAmount(lua_State* state)
{
    Aura* aura = CheckAura(state, 1);
    lua_pushinteger(state, aura ? aura->GetStackAmount() : 0);
    return 1;
}

int AuraGetOwner(lua_State* state)
{
    auto* engine = GetEngine(state);
    Aura* aura = CheckAura(state, 1);
    if (engine)
        engine->PushUnit(aura ? aura->GetTarget() : nullptr);
    else
        lua_pushnil(state);
    return 1;
}

int AuraSetDuration(lua_State* state)
{
    Aura* aura = CheckAura(state, 1);
    int32 duration = static_cast<int32>(luaL_checkinteger(state, 2));
    if (aura)
    {
        aura->GetHolder()->SetAuraDuration(duration);
        aura->GetHolder()->UpdateAuraDuration();
    }
    return 0;
}

int AuraSetMaxDuration(lua_State* state)
{
    Aura* aura = CheckAura(state, 1);
    int32 duration = static_cast<int32>(luaL_checkinteger(state, 2));
    if (aura)
    {
        aura->GetHolder()->SetAuraMaxDuration(duration);
        aura->GetHolder()->UpdateAuraDuration();
    }
    return 0;
}

int AuraSetStackAmount(lua_State* state)
{
    Aura* aura = CheckAura(state, 1);
    uint32 stackAmount = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (aura)
        aura->GetHolder()->SetStackAmount(stackAmount);
    return 0;
}

int AuraRemove(lua_State* state)
{
    auto* holder = static_cast<LuaAura*>(luaL_checkudata(state, 1, AURA_METATABLE));
    Aura* aura = holder ? holder->aura : nullptr;
    if (aura)
    {
        if (Unit* target = aura->GetTarget())
            target->RemoveAura(aura);
        holder->aura = nullptr;
    }
    return 0;
}

int SpellGetCaster(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    Spell* spell = CheckSpell(state, 1);
    WorldObject* caster = spell ? spell->GetCaster() : nullptr;

    if (!engine || !caster)
    {
        lua_pushnil(state);
        return 1;
    }

    if (Unit* unit = dynamic_cast<Unit*>(caster))
        engine->PushUnit(unit);
    else if (GameObject* go = dynamic_cast<GameObject*>(caster))
        engine->PushGameObject(go);
    else
        lua_pushnil(state);

    return 1;
}

int SpellGetEntry(lua_State* state)
{
    Spell* spell = CheckSpell(state, 1);
    lua_pushinteger(state, spell && spell->m_spellInfo ? spell->m_spellInfo->Id : 0);
    return 1;
}

int SpellGetSpellInfo(lua_State* state)
{
    Spell* spell = CheckSpell(state, 1);
    PushSpellInfo(state, spell ? spell->m_spellInfo : nullptr);
    return 1;
}

int SpellGetCastTime(lua_State* state)
{
    Spell* spell = CheckSpell(state, 1);
    lua_pushinteger(state, spell ? spell->GetCastTime() : 0);
    return 1;
}

int SpellGetCastedTime(lua_State* state)
{
    Spell* spell = CheckSpell(state, 1);
    lua_pushinteger(state, spell ? spell->GetCastedTime() : 0);
    return 1;
}

int SpellGetPowerCost(lua_State* state)
{
    Spell* spell = CheckSpell(state, 1);
    lua_pushinteger(state, spell ? spell->GetPowerCost() : 0);
    return 1;
}

int SpellGetTargets(lua_State* state)
{
    Spell* spell = CheckSpell(state, 1);
    PushSpellTargets(state, spell ? &spell->m_targets : nullptr);
    return 1;
}

int SpellGetTarget(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    Spell* spell = CheckSpell(state, 1);
    Unit* target = spell ? spell->m_targets.getUnitTarget() : nullptr;

    if (engine && target)
        engine->PushUnit(target);
    else
        lua_pushnil(state);

    return 1;
}

int SpellGetGameObjectTarget(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    Spell* spell = CheckSpell(state, 1);
    GameObject* target = spell ? spell->m_targets.getGOTarget() : nullptr;

    if (engine && target)
        engine->PushGameObject(target);
    else
        lua_pushnil(state);

    return 1;
}

int SpellGetItemTarget(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    Spell* spell = CheckSpell(state, 1);
    Item* target = spell ? spell->m_targets.getItemTarget() : nullptr;

    if (engine && target)
        engine->PushItem(target);
    else
        lua_pushnil(state);

    return 1;
}

int SpellGetCastItem(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    Spell* spell = CheckSpell(state, 1);
    Item* item = spell ? spell->m_CastItem : nullptr;

    if (engine && item)
        engine->PushItem(item);
    else
        lua_pushnil(state);

    return 1;
}

int SpellIsTriggered(lua_State* state)
{
    Spell* spell = CheckSpell(state, 1);
    lua_pushboolean(state, spell && spell->IsTriggered());
    return 1;
}

int SpellIsCastByItem(lua_State* state)
{
    Spell* spell = CheckSpell(state, 1);
    lua_pushboolean(state, spell && spell->IsCastByItem());
    return 1;
}

int SpellIsAutoRepeat(lua_State* state)
{
    Spell* spell = CheckSpell(state, 1);
    lua_pushboolean(state, spell && spell->IsAutoRepeat());
    return 1;
}

int SpellSetAutoRepeat(lua_State* state)
{
    Spell* spell = CheckSpell(state, 1);
    bool repeat = !lua_isnoneornil(state, 2) && lua_toboolean(state, 2);
    if (spell)
        spell->SetAutoRepeat(repeat);
    return 0;
}

int SpellCast(lua_State* state)
{
    Spell* spell = CheckSpell(state, 1);
    bool skipCheck = !lua_isnoneornil(state, 2) && lua_toboolean(state, 2);
    if (spell)
        spell->cast(skipCheck);
    return 0;
}

int SpellFinish(lua_State* state)
{
    Spell* spell = CheckSpell(state, 1);
    bool ok = lua_isnoneornil(state, 2) || lua_toboolean(state, 2);
    if (spell)
        spell->finish(ok);
    return 0;
}

int SpellGetDuration(lua_State* state)
{
    Spell* spell = CheckSpell(state, 1);
    lua_pushinteger(state, spell && spell->m_spellInfo ? spell->m_spellInfo->GetDuration() : 0);
    return 1;
}

int SpellGetReagentCost(lua_State* state)
{
    Spell* spell = CheckSpell(state, 1);
    lua_newtable(state);

    SpellEntry const* spellInfo = spell ? spell->m_spellInfo : nullptr;
    if (!spellInfo)
        return 1;

    for (uint32 i = 0; i < MAX_SPELL_REAGENTS; ++i)
    {
        if (spellInfo->Reagent[i] <= 0)
            continue;

        ItemPrototype const* reagent = sObjectMgr.GetItemPrototype(static_cast<uint32>(spellInfo->Reagent[i]));
        if (!reagent)
            continue;

        PushItemTemplateValue(state, reagent);
        lua_pushinteger(state, spellInfo->ReagentCount[i]);
        lua_settable(state, -3);
    }

    return 1;
}

int SpellGetTargetDest(lua_State* state)
{
    Spell* spell = CheckSpell(state, 1);
    if (!spell || !(spell->m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION))
    {
        lua_pushnil(state);
        lua_pushnil(state);
        lua_pushnil(state);
        return 3;
    }

    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    spell->m_targets.getDestination(x, y, z);

    lua_pushnumber(state, x);
    lua_pushnumber(state, y);
    lua_pushnumber(state, z);
    return 3;
}

int SpellCancel(lua_State* state)
{
    Spell* spell = CheckSpell(state, 1);
    if (spell)
        spell->cancel();
    return 0;
}

uint32 CheckEffectIndex(lua_State* state, int arg)
{
    int32 index = static_cast<int32>(luaL_checkinteger(state, arg));
    if (index < 0 || index >= MAX_EFFECT_INDEX)
        return luaL_argerror(state, arg, "effect index must be 0, 1, or 2"), 0;

    return static_cast<uint32>(index);
}

bool HasLuaArgument(lua_State* state, int arg)
{
    return lua_gettop(state) >= arg && !lua_isnil(state, arg);
}

void PushIntegerArray(lua_State* state, int32 const* values, uint32 count)
{
    lua_newtable(state);
    for (uint32 i = 0; i < count; ++i)
    {
        lua_pushinteger(state, values ? values[i] : 0);
        lua_rawseti(state, -2, i + 1);
    }
}

void PushUnsignedArray(lua_State* state, uint32 const* values, uint32 count)
{
    lua_newtable(state);
    for (uint32 i = 0; i < count; ++i)
    {
        lua_pushinteger(state, values ? values[i] : 0);
        lua_rawseti(state, -2, i + 1);
    }
}

void PushFloatArray(lua_State* state, float const* values, uint32 count)
{
    lua_newtable(state);
    for (uint32 i = 0; i < count; ++i)
    {
        lua_pushnumber(state, values ? values[i] : 0.0f);
        lua_rawseti(state, -2, i + 1);
    }
}

void PushZeroArray(lua_State* state, uint32 count)
{
    lua_newtable(state);
    for (uint32 i = 0; i < count; ++i)
    {
        lua_pushinteger(state, 0);
        lua_rawseti(state, -2, i + 1);
    }
}

void PushLocaleStringArray(lua_State* state, std::array<std::string, MAX_DBC_LOCALE> const& values)
{
    lua_newtable(state);
    for (uint32 i = 0; i < MAX_DBC_LOCALE; ++i)
    {
        lua_pushstring(state, values[i].c_str());
        lua_rawseti(state, -2, i + 1);
    }
}

uint32 CheckReagentIndex(lua_State* state, int arg)
{
    int32 index = static_cast<int32>(luaL_checkinteger(state, arg));
    if (index < 0 || index >= MAX_SPELL_REAGENTS)
        return luaL_argerror(state, arg, "reagent index must be 0..7"), 0;

    return static_cast<uint32>(index);
}

uint32 CheckTotemIndex(lua_State* state, int arg)
{
    int32 index = static_cast<int32>(luaL_checkinteger(state, arg));
    if (index < 0 || index >= MAX_SPELL_TOTEMS)
        return luaL_argerror(state, arg, "totem index must be 0 or 1"), 0;

    return static_cast<uint32>(index);
}

uint32 CheckAttributeIndex(lua_State* state, int arg)
{
    int32 index = static_cast<int32>(luaL_optinteger(state, arg, 0));
    if (index < 0 || index > 4)
        return luaL_argerror(state, arg, "attribute index must be 0..4"), 0;

    return static_cast<uint32>(index);
}

uint32 GetSpellAttributesByIndex(SpellEntry const* spellInfo, uint32 index)
{
    if (!spellInfo)
        return 0;

    switch (index)
    {
        case 0: return spellInfo->Attributes;
        case 1: return spellInfo->AttributesEx;
        case 2: return spellInfo->AttributesEx2;
        case 3: return spellInfo->AttributesEx3;
        case 4: return spellInfo->AttributesEx4;
        default: return 0;
    }
}

uint32 CheckLocaleIndex(lua_State* state, int arg)
{
    int32 locale = static_cast<int32>(luaL_optinteger(state, arg, 0));
    if (locale < 0 || locale >= MAX_DBC_LOCALE)
        return luaL_argerror(state, arg, "locale index is out of range"), 0;

    return static_cast<uint32>(locale);
}

bool SpellInfoHasAreaTarget(SpellEntry const* spellInfo)
{
    if (!spellInfo)
        return false;

    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (Spells::IsAreaEffectTarget(SpellTarget(spellInfo->EffectImplicitTargetA[i])) ||
            Spells::IsAreaEffectTarget(SpellTarget(spellInfo->EffectImplicitTargetB[i])))
            return true;
    }

    return false;
}

bool SpellInfoNeedsExplicitUnitTargetValue(SpellEntry const* spellInfo)
{
    if (!spellInfo)
        return false;

    if (spellInfo->Targets & TARGET_FLAG_UNIT)
        return true;

    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (Spells::IsExplicitlySelectedUnitTarget(spellInfo->EffectImplicitTargetA[i]) ||
            Spells::IsExplicitlySelectedUnitTarget(spellInfo->EffectImplicitTargetB[i]))
            return true;
    }

    return false;
}

bool SpellInfoIsSelfCastValue(SpellEntry const* spellInfo)
{
    if (!spellInfo)
        return false;

    if (spellInfo->Targets & (TARGET_FLAG_UNIT | TARGET_FLAG_ITEM | TARGET_FLAG_OBJECT | TARGET_FLAG_CORPSE | TARGET_FLAG_UNIT_CORPSE | TARGET_FLAG_PVP_CORPSE))
        return false;

    bool hasExplicitTarget = false;
    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        uint32 targetA = spellInfo->EffectImplicitTargetA[i];
        uint32 targetB = spellInfo->EffectImplicitTargetB[i];
        if (!targetA && !targetB)
            continue;

        hasExplicitTarget = true;
        if ((targetA && targetA != TARGET_UNIT_CASTER) || (targetB && targetB != TARGET_UNIT_CASTER))
            return false;
    }

    return hasExplicitTarget || spellInfo->Targets == TARGET_FLAG_SELF;
}

uint32 SpellInfoGetEffectMechanicMaskValue(SpellEntry const* spellInfo, uint32 effectIndex)
{
    if (!spellInfo || effectIndex >= MAX_EFFECT_INDEX)
        return 0;

    uint32 mechanic = spellInfo->EffectMechanic[effectIndex] ? spellInfo->EffectMechanic[effectIndex] : spellInfo->Mechanic;
    return mechanic ? (1 << (mechanic - 1)) : 0;
}

bool SpellInfoCanDispelAuraValue(SpellEntry const* spellInfo, SpellEntry const* auraSpellInfo)
{
    if (!spellInfo || !auraSpellInfo || !auraSpellInfo->Dispel)
        return false;

    uint32 dispelMask = 0;
    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        if (spellInfo->Effect[i] == SPELL_EFFECT_DISPEL)
            dispelMask |= Spells::GetDispellMask(DispelType(spellInfo->EffectMiscValue[i]));

    return (dispelMask & (1 << auraSpellInfo->Dispel)) != 0;
}

bool SpellInfoCheckTargetCreatureTypeValue(SpellEntry const* spellInfo, Unit const* target)
{
    if (!spellInfo || !target || !spellInfo->TargetCreatureType)
        return true;

    return (spellInfo->TargetCreatureType & target->GetCreatureTypeMask()) != 0;
}

uint32 SpellInfoCheckTargetValue(SpellEntry const* spellInfo, Unit const* caster, WorldObject const* target, bool explicitTarget)
{
    if (!spellInfo || !caster)
        return SPELL_FAILED_BAD_TARGETS;

    if (explicitTarget && !target && SpellInfoNeedsExplicitUnitTargetValue(spellInfo))
        return SPELL_FAILED_BAD_TARGETS;

    Unit const* targetUnit = target ? dynamic_cast<Unit const*>(target) : nullptr;
    if (targetUnit)
    {
        if (!spellInfo->CanTargetAliveState(targetUnit->IsAlive()))
            return targetUnit->IsAlive() ? SPELL_FAILED_TARGET_NOT_DEAD : SPELL_FAILED_TARGETS_DEAD;

        if (!SpellInfoCheckTargetCreatureTypeValue(spellInfo, targetUnit))
            return SPELL_FAILED_BAD_TARGETS;
    }

    return SPELL_CAST_OK;
}

int SpellInfoGetId(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->Id : 0);
    return 1;
}

int SpellInfoGetName(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 locale = CheckLocaleIndex(state, 2);
    lua_pushstring(state, spellInfo ? spellInfo->SpellName[locale].c_str() : "");
    return 1;
}

int SpellInfoGetSpellName(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    if (spellInfo)
        PushLocaleStringArray(state, spellInfo->SpellName);
    else
        lua_newtable(state);
    return 1;
}

int SpellInfoGetRank(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 locale = CheckLocaleIndex(state, 2);
    lua_pushstring(state, spellInfo ? spellInfo->Rank[locale].c_str() : "");
    return 1;
}

int SpellInfoGetSchool(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->School : 0);
    return 1;
}

int SpellInfoGetSchoolMask(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->GetSpellSchoolMask() : 0);
    return 1;
}

int SpellInfoGetCategory(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->Category : 0);
    return 1;
}

int SpellInfoGetDispel(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->Dispel : 0);
    return 1;
}

int SpellInfoGetMechanic(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->GetMechanic() : 0);
    return 1;
}

int SpellInfoGetAllMechanicMask(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->GetAllSpellMechanicMask() : 0);
    return 1;
}

int SpellInfoGetAttributes(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 index = CheckAttributeIndex(state, 2);
    lua_pushinteger(state, GetSpellAttributesByIndex(spellInfo, index));
    return 1;
}

int SpellInfoHasAttribute(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 index = 0;
    uint32 flag = 0;
    if (lua_gettop(state) >= 3)
    {
        index = CheckAttributeIndex(state, 2);
        flag = static_cast<uint32>(luaL_checkinteger(state, 3));
    }
    else
        flag = static_cast<uint32>(luaL_checkinteger(state, 2));

    lua_pushboolean(state, (GetSpellAttributesByIndex(spellInfo, index) & flag) != 0);
    return 1;
}

int SpellInfoGetAttributesEx(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->AttributesEx : 0);
    return 1;
}

int SpellInfoGetAttributesEx2(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->AttributesEx2 : 0);
    return 1;
}

int SpellInfoGetAttributesEx3(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->AttributesEx3 : 0);
    return 1;
}

int SpellInfoGetAttributesEx4(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->AttributesEx4 : 0);
    return 1;
}

int SpellInfoGetAttributesEx5(lua_State* state)
{
    CheckSpellInfo(state, 1);
    lua_pushinteger(state, 0);
    return 1;
}

int SpellInfoGetAttributesEx6(lua_State* state)
{
    CheckSpellInfo(state, 1);
    lua_pushinteger(state, 0);
    return 1;
}

int SpellInfoGetAttributesEx7(lua_State* state)
{
    CheckSpellInfo(state, 1);
    lua_pushinteger(state, 0);
    return 1;
}

int SpellInfoGetStances(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->Stances : 0);
    return 1;
}

int SpellInfoGetStancesNot(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->StancesNot : 0);
    return 1;
}

int SpellInfoGetTargets(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->Targets : 0);
    return 1;
}

int SpellInfoGetTargetCreatureType(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->TargetCreatureType : 0);
    return 1;
}

int SpellInfoGetRequiresSpellFocus(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->RequiresSpellFocus : 0);
    return 1;
}

int SpellInfoGetFacingCasterFlags(lua_State* state)
{
    CheckSpellInfo(state, 1);
    lua_pushinteger(state, 0);
    return 1;
}

int SpellInfoGetCasterAuraState(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->CasterAuraState : 0);
    return 1;
}

int SpellInfoGetTargetAuraState(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->TargetAuraState : 0);
    return 1;
}

int SpellInfoGetCasterAuraStateNot(lua_State* state)
{
    CheckSpellInfo(state, 1);
    lua_pushinteger(state, 0);
    return 1;
}

int SpellInfoGetTargetAuraStateNot(lua_State* state)
{
    CheckSpellInfo(state, 1);
    lua_pushinteger(state, 0);
    return 1;
}

int SpellInfoGetCasterAuraSpell(lua_State* state)
{
    CheckSpellInfo(state, 1);
    lua_pushinteger(state, 0);
    return 1;
}

int SpellInfoGetTargetAuraSpell(lua_State* state)
{
    CheckSpellInfo(state, 1);
    lua_pushinteger(state, 0);
    return 1;
}

int SpellInfoGetExcludeCasterAuraSpell(lua_State* state)
{
    CheckSpellInfo(state, 1);
    lua_pushinteger(state, 0);
    return 1;
}

int SpellInfoGetExcludeTargetAuraSpell(lua_State* state)
{
    CheckSpellInfo(state, 1);
    lua_pushinteger(state, 0);
    return 1;
}

int SpellInfoGetCastingTimeIndex(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->CastingTimeIndex : 0);
    return 1;
}

int SpellInfoGetRecoveryTime(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->GetRecoveryTime() : 0);
    return 1;
}

int SpellInfoGetRawRecoveryTime(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->RecoveryTime : 0);
    return 1;
}

int SpellInfoGetCategoryRecoveryTime(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->CategoryRecoveryTime : 0);
    return 1;
}

int SpellInfoGetStartRecoveryTime(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->StartRecoveryTime : 0);
    return 1;
}

int SpellInfoGetStartRecoveryCategory(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->StartRecoveryCategory : 0);
    return 1;
}

int SpellInfoGetInterruptFlags(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->InterruptFlags : 0);
    return 1;
}

int SpellInfoGetAuraInterruptFlags(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->AuraInterruptFlags : 0);
    return 1;
}

int SpellInfoGetChannelInterruptFlags(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->ChannelInterruptFlags : 0);
    return 1;
}

int SpellInfoGetProcFlags(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->procFlags : 0);
    return 1;
}

int SpellInfoGetProcChance(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->procChance : 0);
    return 1;
}

int SpellInfoGetProcCharges(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->procCharges : 0);
    return 1;
}

int SpellInfoGetMaxLevel(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->maxLevel : 0);
    return 1;
}

int SpellInfoGetBaseLevel(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->baseLevel : 0);
    return 1;
}

int SpellInfoGetSpellLevel(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->spellLevel : 0);
    return 1;
}

int SpellInfoGetDurationIndex(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->DurationIndex : 0);
    return 1;
}

int SpellInfoGetDuration(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->GetDuration() : 0);
    return 1;
}

int SpellInfoGetPowerType(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->powerType : 0);
    return 1;
}

int SpellInfoGetManaCost(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->GetManaCost() : 0);
    return 1;
}

int SpellInfoGetManaCostPerlevel(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->manaCostPerlevel : 0);
    return 1;
}

int SpellInfoGetManaPerSecond(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->manaPerSecond : 0);
    return 1;
}

int SpellInfoGetManaPerSecondPerLevel(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->manaPerSecondPerLevel : 0);
    return 1;
}

int SpellInfoGetManaCostPercentage(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->ManaCostPercentage : 0);
    return 1;
}

int SpellInfoGetRangeIndex(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->rangeIndex : 0);
    return 1;
}

int SpellInfoGetSpeed(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushnumber(state, spellInfo ? spellInfo->speed : 0.0f);
    return 1;
}

int SpellInfoGetStackAmount(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->GetStackAmount() : 0);
    return 1;
}

int SpellInfoGetSpellFamilyName(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->GetSpellFamilyName() : 0);
    return 1;
}

int SpellInfoGetSpellFamilyFlags(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? static_cast<lua_Integer>(spellInfo->GetSpellFamilyFlags()) : 0);
    return 1;
}

int SpellInfoGetMaxAffectedTargets(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->MaxAffectedTargets : 0);
    return 1;
}

int SpellInfoGetMaxTargetLevel(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->MaxTargetLevel : 0);
    return 1;
}

int SpellInfoGetDmgClass(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->DmgClass : 0);
    return 1;
}

int SpellInfoGetPreventionType(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->PreventionType : 0);
    return 1;
}

int SpellInfoGetSpellIconID(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->SpellIconID : 0);
    return 1;
}

int SpellInfoGetSpellPriority(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->spellPriority : 0);
    return 1;
}

int SpellInfoGetActiveIconID(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->activeIconID : 0);
    return 1;
}

int SpellInfoGetSpellVisual(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->SpellVisual : 0);
    return 1;
}

int SpellInfoGetTotem(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 index = CheckTotemIndex(state, 2);
    lua_pushinteger(state, spellInfo ? spellInfo->Totem[index] : 0);
    return 1;
}

int SpellInfoGetTotemCategory(lua_State* state)
{
    CheckSpellInfo(state, 1);
    if (HasLuaArgument(state, 2))
    {
        CheckTotemIndex(state, 2);
        lua_pushinteger(state, 0);
    }
    else
        PushZeroArray(state, MAX_SPELL_TOTEMS);
    return 1;
}

int SpellInfoGetReagent(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 index = CheckReagentIndex(state, 2);
    lua_pushinteger(state, spellInfo ? spellInfo->Reagent[index] : 0);
    return 1;
}

int SpellInfoGetReagentCount(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 index = CheckReagentIndex(state, 2);
    lua_pushinteger(state, spellInfo ? spellInfo->ReagentCount[index] : 0);
    return 1;
}

int SpellInfoGetEquippedItemClass(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->EquippedItemClass : 0);
    return 1;
}

int SpellInfoGetEquippedItemSubClassMask(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->EquippedItemSubClassMask : 0);
    return 1;
}

int SpellInfoGetEquippedItemInventoryTypeMask(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->EquippedItemInventoryTypeMask : 0);
    return 1;
}

int SpellInfoGetEffect(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 index = CheckEffectIndex(state, 2);
    lua_pushinteger(state, spellInfo ? spellInfo->Effect[index] : 0);
    return 1;
}

int SpellInfoGetEffectDieSides(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 index = CheckEffectIndex(state, 2);
    lua_pushinteger(state, spellInfo ? spellInfo->EffectDieSides[index] : 0);
    return 1;
}

int SpellInfoGetEffectRealPointsPerLevel(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    if (HasLuaArgument(state, 2))
    {
        uint32 index = CheckEffectIndex(state, 2);
        lua_pushnumber(state, spellInfo ? spellInfo->EffectRealPointsPerLevel[index] : 0.0f);
    }
    else if (spellInfo)
        PushFloatArray(state, spellInfo->EffectRealPointsPerLevel, MAX_EFFECT_INDEX);
    else
        lua_newtable(state);
    return 1;
}

int SpellInfoGetEffectBaseDice(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 index = CheckEffectIndex(state, 2);
    lua_pushinteger(state, spellInfo ? spellInfo->EffectBaseDice[index] : 0);
    return 1;
}

int SpellInfoGetEffectBasePoints(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 index = CheckEffectIndex(state, 2);
    lua_pushinteger(state, spellInfo ? spellInfo->EffectBasePoints[index] : 0);
    return 1;
}

int SpellInfoGetEffectMechanic(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 index = CheckEffectIndex(state, 2);
    lua_pushinteger(state, spellInfo ? spellInfo->EffectMechanic[index] : 0);
    return 1;
}

int SpellInfoGetEffectImplicitTargetA(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 index = CheckEffectIndex(state, 2);
    lua_pushinteger(state, spellInfo ? spellInfo->EffectImplicitTargetA[index] : 0);
    return 1;
}

int SpellInfoGetEffectImplicitTargetB(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 index = CheckEffectIndex(state, 2);
    lua_pushinteger(state, spellInfo ? spellInfo->EffectImplicitTargetB[index] : 0);
    return 1;
}

int SpellInfoGetEffectRadiusIndex(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    if (HasLuaArgument(state, 2))
    {
        uint32 index = CheckEffectIndex(state, 2);
        lua_pushinteger(state, spellInfo ? spellInfo->EffectRadiusIndex[index] : 0);
    }
    else if (spellInfo)
        PushUnsignedArray(state, spellInfo->EffectRadiusIndex, MAX_EFFECT_INDEX);
    else
        lua_newtable(state);
    return 1;
}

int SpellInfoGetEffectApplyAuraName(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 index = CheckEffectIndex(state, 2);
    lua_pushinteger(state, spellInfo ? spellInfo->EffectApplyAuraName[index] : 0);
    return 1;
}

int SpellInfoGetEffectAmplitude(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 index = CheckEffectIndex(state, 2);
    lua_pushinteger(state, spellInfo ? spellInfo->EffectAmplitude[index] : 0);
    return 1;
}

int SpellInfoGetEffectMultipleValue(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 index = CheckEffectIndex(state, 2);
    lua_pushnumber(state, spellInfo ? spellInfo->EffectMultipleValue[index] : 0.0f);
    return 1;
}

int SpellInfoGetEffectValueMultiplier(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    if (HasLuaArgument(state, 2))
    {
        uint32 index = CheckEffectIndex(state, 2);
        lua_pushnumber(state, spellInfo ? spellInfo->EffectMultipleValue[index] : 0.0f);
    }
    else if (spellInfo)
        PushFloatArray(state, spellInfo->EffectMultipleValue, MAX_EFFECT_INDEX);
    else
        lua_newtable(state);
    return 1;
}

int SpellInfoGetEffectChainTarget(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 index = CheckEffectIndex(state, 2);
    lua_pushinteger(state, spellInfo ? spellInfo->EffectChainTarget[index] : 0);
    return 1;
}

int SpellInfoGetEffectItemType(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 index = CheckEffectIndex(state, 2);
    lua_pushinteger(state, spellInfo ? spellInfo->EffectItemType[index] : 0);
    return 1;
}

int SpellInfoGetEffectMiscValue(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 index = CheckEffectIndex(state, 2);
    lua_pushinteger(state, spellInfo ? spellInfo->EffectMiscValue[index] : 0);
    return 1;
}

int SpellInfoGetEffectMiscValueB(lua_State* state)
{
    CheckSpellInfo(state, 1);
    if (HasLuaArgument(state, 2))
    {
        CheckEffectIndex(state, 2);
        lua_pushinteger(state, 0);
    }
    else
        PushZeroArray(state, MAX_EFFECT_INDEX);
    return 1;
}

int SpellInfoGetEffectTriggerSpell(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 index = CheckEffectIndex(state, 2);
    lua_pushinteger(state, spellInfo ? spellInfo->EffectTriggerSpell[index] : 0);
    return 1;
}

int SpellInfoGetEffectPointsPerComboPoint(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 index = CheckEffectIndex(state, 2);
    lua_pushnumber(state, spellInfo ? spellInfo->EffectPointsPerComboPoint[index] : 0.0f);
    return 1;
}

int SpellInfoGetEffectSpellClassMask(lua_State* state)
{
    CheckSpellInfo(state, 1);
    if (HasLuaArgument(state, 2))
    {
        CheckEffectIndex(state, 2);
        lua_pushinteger(state, 0);
    }
    else
        PushZeroArray(state, MAX_EFFECT_INDEX);
    return 1;
}

int SpellInfoGetDamageMultiplier(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 index = CheckEffectIndex(state, 2);
    lua_pushnumber(state, spellInfo ? spellInfo->DmgMultiplier[index] : 0.0f);
    return 1;
}

int SpellInfoGetEffectDamageMultiplier(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    if (HasLuaArgument(state, 2))
    {
        uint32 index = CheckEffectIndex(state, 2);
        lua_pushnumber(state, spellInfo ? spellInfo->DmgMultiplier[index] : 0.0f);
    }
    else if (spellInfo)
        PushFloatArray(state, spellInfo->DmgMultiplier, MAX_EFFECT_INDEX);
    else
        lua_newtable(state);
    return 1;
}

int SpellInfoGetEffectBonusMultiplier(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    if (HasLuaArgument(state, 2))
    {
        uint32 index = CheckEffectIndex(state, 2);
        lua_pushnumber(state, spellInfo ? spellInfo->EffectBonusCoefficient[index] : 0.0f);
    }
    else if (spellInfo)
        PushFloatArray(state, spellInfo->EffectBonusCoefficient, MAX_EFFECT_INDEX);
    else
        lua_newtable(state);
    return 1;
}

int SpellInfoGetAreaGroupId(lua_State* state)
{
    CheckSpellInfo(state, 1);
    lua_pushinteger(state, 0);
    return 1;
}

int SpellInfoGetRuneCostID(lua_State* state)
{
    CheckSpellInfo(state, 1);
    lua_pushinteger(state, 0);
    return 1;
}

int SpellInfoCalculateSimpleValue(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 index = CheckEffectIndex(state, 2);
    lua_pushinteger(state, spellInfo ? spellInfo->CalculateSimpleValue(SpellEffectIndex(index)) : 0);
    return 1;
}

int SpellInfoHasAura(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 aura = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushboolean(state, spellInfo && spellInfo->HasAura(AuraType(aura)));
    return 1;
}

int SpellInfoHasEffect(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 effect = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushboolean(state, spellInfo && spellInfo->HasEffect(SpellEffects(effect)));
    return 1;
}

int SpellInfoIsPassive(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && spellInfo->IsPassiveSpell());
    return 1;
}

int SpellInfoIsPositive(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && spellInfo->IsPositiveSpell());
    return 1;
}

int SpellInfoIsBinary(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && spellInfo->IsBinary());
    return 1;
}

int SpellInfoIsDispel(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && spellInfo->IsDispel());
    return 1;
}

int SpellInfoIsNonPeriodicDispel(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && spellInfo->IsNonPeriodicDispel());
    return 1;
}

int SpellInfoIsCCSpell(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && spellInfo->IsCCSpell());
    return 1;
}

int SpellInfoIsCustomSpell(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && spellInfo->IsCustomSpell());
    return 1;
}

int SpellInfoIsPvEHeartBeat(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && spellInfo->IsPvEHeartBeat());
    return 1;
}

int SpellInfoHasAreaAuraEffect(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && spellInfo->HasAreaAuraEffect());
    return 1;
}

int SpellInfoIsAffectingArea(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && (spellInfo->IsAreaOfEffectSpell() || spellInfo->HasAreaAuraEffect() || SpellInfoHasAreaTarget(spellInfo)));
    return 1;
}

int SpellInfoIsExplicitDiscovery(lua_State* state)
{
    CheckSpellInfo(state, 1);
    lua_pushboolean(state, false);
    return 1;
}

int SpellInfoIsLootCrafting(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && (spellInfo->HasEffect(SPELL_EFFECT_CREATE_ITEM) || spellInfo->HasEffect(SPELL_EFFECT_ENCHANT_ITEM) || spellInfo->HasEffect(SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY)));
    return 1;
}

int SpellInfoIsProfessionOrRiding(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && SpellMgr::IsProfessionOrRidingSpell(spellInfo->Id));
    return 1;
}

int SpellInfoIsProfession(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && SpellMgr::IsProfessionSpell(spellInfo->Id));
    return 1;
}

int SpellInfoIsPrimaryProfession(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && SpellMgr::IsPrimaryProfessionSpell(spellInfo->Id));
    return 1;
}

int SpellInfoIsPrimaryProfessionFirstRank(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && sSpellMgr.IsPrimaryProfessionFirstRankSpell(spellInfo->Id));
    return 1;
}

int SpellInfoIsAbilityLearnedWithProfession(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    bool learnedWithProfession = false;
    if (spellInfo)
    {
        for (uint32 i = 0; i < MAX_EFFECT_INDEX && !learnedWithProfession; ++i)
        {
            if (spellInfo->Effect[i] == SPELL_EFFECT_SKILL || spellInfo->Effect[i] == SPELL_EFFECT_TRADE_SKILL)
                learnedWithProfession = true;
            else if (spellInfo->Effect[i] == SPELL_EFFECT_LEARN_SPELL)
            {
                if (SpellEntry const* learned = sSpellMgr.GetSpellEntry(spellInfo->EffectTriggerSpell[i]))
                    learnedWithProfession = SpellMgr::IsProfessionOrRidingSpell(learned->Id);
            }
        }
    }

    lua_pushboolean(state, learnedWithProfession);
    return 1;
}

int SpellInfoIsAbilityOfSkillType(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 skillType = static_cast<uint32>(luaL_checkinteger(state, 2));
    bool matches = false;
    if (spellInfo)
    {
        for (uint32 i = 0; i < MAX_EFFECT_INDEX && !matches; ++i)
            if ((spellInfo->Effect[i] == SPELL_EFFECT_SKILL || spellInfo->Effect[i] == SPELL_EFFECT_TRADE_SKILL) && static_cast<uint32>(spellInfo->EffectMiscValue[i]) == skillType)
                matches = true;
    }

    lua_pushboolean(state, matches);
    return 1;
}

int SpellInfoIsTargetingArea(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, SpellInfoHasAreaTarget(spellInfo));
    return 1;
}

int SpellInfoNeedsExplicitUnitTarget(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, SpellInfoNeedsExplicitUnitTargetValue(spellInfo));
    return 1;
}

int SpellInfoNeedsToBeTriggeredByCaster(lua_State* state)
{
    CheckSpellInfo(state, 1);
    if (!lua_isnoneornil(state, 2))
        CheckSpellInfo(state, 2);
    lua_pushboolean(state, false);
    return 1;
}

int SpellInfoIsSelfCast(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, SpellInfoIsSelfCastValue(spellInfo));
    return 1;
}

int SpellInfoIsAutocastable(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && spellInfo->HasAttribute(SPELL_ATTR_IS_ABILITY) && !spellInfo->IsPassiveSpell());
    return 1;
}

int SpellInfoIsStackableWithRanks(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && spellInfo->IsPassiveSpellStackableWithRanks());
    return 1;
}

int SpellInfoIsPassiveStackableWithRanks(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && spellInfo->IsPassiveSpellStackableWithRanks());
    return 1;
}

int SpellInfoIsMultiSlotAura(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && spellInfo->IsSpellAppliesAura() && !spellInfo->HasSingleTargetAura());
    return 1;
}

int SpellInfoIsCooldownStartedOnEvent(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && spellInfo->HasAttribute(SPELL_ATTR_DISABLED_WHILE_ACTIVE));
    return 1;
}

int SpellInfoIsDeathPersistent(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && spellInfo->IsDeathPersistentSpell());
    return 1;
}

int SpellInfoIsRequiringDeadTarget(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && spellInfo->IsDeathOnlySpell());
    return 1;
}

int SpellInfoIsAllowingDeadTarget(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && spellInfo->CanTargetDeadTarget());
    return 1;
}

int SpellInfoCanBeUsedInCombat(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && !spellInfo->IsNonCombatSpell());
    return 1;
}

int SpellInfoIsPositiveEffect(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 index = CheckEffectIndex(state, 2);
    lua_pushboolean(state, spellInfo && spellInfo->IsPositiveEffect(SpellEffectIndex(index)));
    return 1;
}

int SpellInfoIsChanneled(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && spellInfo->IsChanneledSpell());
    return 1;
}

int SpellInfoNeedsComboPoints(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && spellInfo->NeedsComboPoints());
    return 1;
}

int SpellInfoIsBreakingStealth(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && !spellInfo->IsPassiveSpell() && !spellInfo->HasAttribute(SPELL_ATTR_EX_NOT_BREAK_STEALTH));
    return 1;
}

int SpellInfoIsRangedWeaponSpell(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && ((spellInfo->Attributes & SPELL_ATTR_RANGED) || spellInfo->DmgClass == SPELL_DAMAGE_CLASS_RANGED || spellInfo->Category == SPELLCATEGORY_RANGED_WEAPON));
    return 1;
}

int SpellInfoIsAutoRepeatRangedSpell(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && spellInfo->IsAutoRepeatRangedSpell());
    return 1;
}

int SpellInfoIsAffectedBySpellMods(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && spellInfo->SpellFamilyName != SPELLFAMILY_GENERIC && spellInfo->SpellFamilyFlags != 0);
    return 1;
}

int SpellInfoCanPierceImmuneAura(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    if (!lua_isnoneornil(state, 2))
        CheckSpellInfo(state, 2);
    lua_pushboolean(state, spellInfo && spellInfo->HasAttribute(SPELL_ATTR_UNAFFECTED_BY_INVULNERABILITY));
    return 1;
}

int SpellInfoCanDispelAura(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    SpellEntry const* auraSpellInfo = lua_isnoneornil(state, 2) ? nullptr : CheckSpellInfo(state, 2);
    lua_pushboolean(state, SpellInfoCanDispelAuraValue(spellInfo, auraSpellInfo));
    return 1;
}

int SpellInfoIsSingleTarget(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushboolean(state, spellInfo && (spellInfo->HasSingleTargetAura() || (!SpellInfoHasAreaTarget(spellInfo) && SpellInfoNeedsExplicitUnitTargetValue(spellInfo))));
    return 1;
}

int SpellInfoIsAuraExclusiveBySpecificWith(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    SpellEntry const* other = lua_isnoneornil(state, 2) ? nullptr : CheckSpellInfo(state, 2);
    lua_pushboolean(state, spellInfo && other && Spells::IsSingleFromSpellSpecificPerTarget(Spells::GetSpellSpecific(spellInfo->Id), Spells::GetSpellSpecific(other->Id)));
    return 1;
}

int SpellInfoIsAuraExclusiveBySpecificPerCasterWith(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    SpellEntry const* other = lua_isnoneornil(state, 2) ? nullptr : CheckSpellInfo(state, 2);
    lua_pushboolean(state, spellInfo && other && Spells::IsSingleFromSpellSpecificPerTargetPerCaster(Spells::GetSpellSpecific(spellInfo->Id), Spells::GetSpellSpecific(other->Id)));
    return 1;
}

int SpellInfoCheckShapeshift(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 form = static_cast<uint32>(luaL_checkinteger(state, 2));
    uint32 formMask = form ? (1 << (form - 1)) : 0;
    bool allowed = spellInfo && (!spellInfo->Stances || (spellInfo->Stances & formMask)) && !(spellInfo->StancesNot & formMask);
    lua_pushinteger(state, allowed ? SPELL_CAST_OK : SPELL_FAILED_ONLY_SHAPESHIFT);
    return 1;
}

int SpellInfoCheckLocation(lua_State* state)
{
    CheckSpellInfo(state, 1);
    luaL_checkinteger(state, 2);
    luaL_checkinteger(state, 3);
    luaL_checkinteger(state, 4);
    if (!lua_isnoneornil(state, 5))
        CheckPlayer(state, 5);
    lua_pushinteger(state, SPELL_CAST_OK);
    return 1;
}

int SpellInfoCheckTarget(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    Unit* caster = CheckUnit(state, 2);
    WorldObject* target = lua_isnoneornil(state, 3) ? nullptr : CheckWorldObject(state, 3);
    bool implicit = lua_isnoneornil(state, 4) ? true : lua_toboolean(state, 4) != 0;
    lua_pushinteger(state, SpellInfoCheckTargetValue(spellInfo, caster, target, !implicit));
    return 1;
}

int SpellInfoCheckExplicitTarget(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    Unit* caster = CheckUnit(state, 2);
    WorldObject* target = lua_isnoneornil(state, 3) ? nullptr : CheckWorldObject(state, 3);
    if (!lua_isnoneornil(state, 4))
        CheckItem(state, 4);
    lua_pushinteger(state, SpellInfoCheckTargetValue(spellInfo, caster, target, true));
    return 1;
}

int SpellInfoCheckTargetCreatureType(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    Unit* target = CheckUnit(state, 2);
    lua_pushboolean(state, SpellInfoCheckTargetCreatureTypeValue(spellInfo, target));
    return 1;
}

int SpellInfoGetAllEffectsMechanicMask(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->GetAllSpellMechanicMask() : 0);
    return 1;
}

int SpellInfoGetEffectMechanicMask(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 index = CheckEffectIndex(state, 2);
    lua_pushinteger(state, SpellInfoGetEffectMechanicMaskValue(spellInfo, index));
    return 1;
}

int SpellInfoGetSpellMechanicMaskByEffectMask(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 effectMask = static_cast<uint32>(luaL_checkinteger(state, 2));
    lua_pushinteger(state, spellInfo ? spellInfo->GetSpellMechanicMask(effectMask) : 0);
    return 1;
}

int SpellInfoGetDispelMask(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    uint32 type = static_cast<uint32>(luaL_optinteger(state, 2, 0));
    if (type)
        lua_pushinteger(state, Spells::GetDispellMask(DispelType(type)));
    else
        lua_pushinteger(state, spellInfo && spellInfo->Dispel ? Spells::GetDispellMask(DispelType(spellInfo->Dispel)) : 0);
    return 1;
}

int SpellInfoGetExplicitTargetMask(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? spellInfo->Targets : 0);
    return 1;
}

int SpellInfoGetAuraState(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? (spellInfo->CasterAuraState ? spellInfo->CasterAuraState : spellInfo->TargetAuraState) : 0);
    return 1;
}

int SpellInfoGetSpellSpecific(lua_State* state)
{
    SpellEntry const* spellInfo = CheckSpellInfo(state, 1);
    lua_pushinteger(state, spellInfo ? Spells::GetSpellSpecific(spellInfo->Id) : 0);
    return 1;
}

int SpellTargetsGetTargetMask(lua_State* state)
{
    SpellCastTargets const* targets = CheckSpellTargets(state, 1);
    lua_pushinteger(state, targets ? targets->m_targetMask : 0);
    return 1;
}

int SpellTargetsGetUnitTarget(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    SpellCastTargets const* targets = CheckSpellTargets(state, 1);
    if (engine && targets)
        engine->PushUnit(targets->getUnitTarget());
    else
        lua_pushnil(state);
    return 1;
}

int SpellTargetsGetGameObjectTarget(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    SpellCastTargets const* targets = CheckSpellTargets(state, 1);
    if (engine && targets)
        engine->PushGameObject(targets->getGOTarget());
    else
        lua_pushnil(state);
    return 1;
}

int SpellTargetsGetItemTarget(lua_State* state)
{
    TurtleLuaEngine* engine = GetEngine(state);
    SpellCastTargets const* targets = CheckSpellTargets(state, 1);
    if (engine && targets)
        engine->PushItem(targets->getItemTarget());
    else
        lua_pushnil(state);
    return 1;
}

int SpellTargetsGetUnitTargetGUID(lua_State* state)
{
    SpellCastTargets const* targets = CheckSpellTargets(state, 1);
    PushObjectGuidValue(state, targets ? targets->getUnitTargetGuid() : ObjectGuid());
    return 1;
}

int SpellTargetsGetGameObjectTargetGUID(lua_State* state)
{
    SpellCastTargets const* targets = CheckSpellTargets(state, 1);
    PushObjectGuidValue(state, targets ? targets->getGOTargetGuid() : ObjectGuid());
    return 1;
}

int SpellTargetsGetItemTargetGUID(lua_State* state)
{
    SpellCastTargets const* targets = CheckSpellTargets(state, 1);
    PushObjectGuidValue(state, targets ? targets->getItemTargetGuid() : ObjectGuid());
    return 1;
}

int SpellTargetsGetUnitTargetGUIDLow(lua_State* state)
{
    SpellCastTargets const* targets = CheckSpellTargets(state, 1);
    lua_pushinteger(state, targets ? targets->getUnitTargetGuid().GetCounter() : 0);
    return 1;
}

int SpellTargetsGetGameObjectTargetGUIDLow(lua_State* state)
{
    SpellCastTargets const* targets = CheckSpellTargets(state, 1);
    lua_pushinteger(state, targets ? targets->getGOTargetGuid().GetCounter() : 0);
    return 1;
}

int SpellTargetsGetItemTargetGUIDLow(lua_State* state)
{
    SpellCastTargets const* targets = CheckSpellTargets(state, 1);
    lua_pushinteger(state, targets ? targets->getItemTargetGuid().GetCounter() : 0);
    return 1;
}

int SpellTargetsGetItemTargetEntry(lua_State* state)
{
    SpellCastTargets const* targets = CheckSpellTargets(state, 1);
    lua_pushinteger(state, targets ? targets->getItemTargetEntry() : 0);
    return 1;
}

int SpellTargetsGetSource(lua_State* state)
{
    SpellCastTargets const* targets = CheckSpellTargets(state, 1);
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    if (targets)
        targets->getSource(x, y, z);

    lua_pushnumber(state, x);
    lua_pushnumber(state, y);
    lua_pushnumber(state, z);
    return 3;
}

int SpellTargetsGetDestination(lua_State* state)
{
    SpellCastTargets const* targets = CheckSpellTargets(state, 1);
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    if (targets)
        targets->getDestination(x, y, z);

    lua_pushnumber(state, x);
    lua_pushnumber(state, y);
    lua_pushnumber(state, z);
    return 3;
}

int SpellTargetsGetStringTarget(lua_State* state)
{
    SpellCastTargets const* targets = CheckSpellTargets(state, 1);
    lua_pushstring(state, targets ? targets->m_strTarget.c_str() : "");
    return 1;
}

int SpellTargetsIsEmpty(lua_State* state)
{
    SpellCastTargets const* targets = CheckSpellTargets(state, 1);
    lua_pushboolean(state, !targets || targets->IsEmpty());
    return 1;
}

int WorldPacketGetOpcode(lua_State* state)
{
    WorldPacket* packet = RequireWorldPacket(state, 1);
    lua_pushinteger(state, packet->GetOpcode());
    return 1;
}

int WorldPacketGetSize(lua_State* state)
{
    WorldPacket* packet = RequireWorldPacket(state, 1);
    lua_pushinteger(state, packet->size());
    return 1;
}

int WorldPacketSetOpcode(lua_State* state)
{
    WorldPacket* packet = RequireWorldPacket(state, 1);
    uint32 opcode = static_cast<uint32>(luaL_checkinteger(state, 2));
    if (opcode >= NUM_MSG_TYPES)
        return luaL_argerror(state, 2, "valid opcode expected");

    packet->SetOpcode(static_cast<uint16>(opcode));
    return 0;
}

template <typename T>
int WorldPacketReadInteger(lua_State* state)
{
    WorldPacket* packet = RequireWorldPacket(state, 1);
    T value = 0;
    try
    {
        (*packet) >> value;
    }
    catch (ByteBufferException const&)
    {
        return luaL_error(state, "packet read out of range");
    }

    lua_pushinteger(state, static_cast<lua_Integer>(value));
    return 1;
}

template <typename T>
int WorldPacketReadNumber(lua_State* state)
{
    WorldPacket* packet = RequireWorldPacket(state, 1);
    T value = 0;
    try
    {
        (*packet) >> value;
    }
    catch (ByteBufferException const&)
    {
        return luaL_error(state, "packet read out of range");
    }

    lua_pushnumber(state, static_cast<lua_Number>(value));
    return 1;
}

int WorldPacketReadByte(lua_State* state)
{
    return WorldPacketReadInteger<int8>(state);
}

int WorldPacketReadUByte(lua_State* state)
{
    return WorldPacketReadInteger<uint8>(state);
}

int WorldPacketReadShort(lua_State* state)
{
    return WorldPacketReadInteger<int16>(state);
}

int WorldPacketReadUShort(lua_State* state)
{
    return WorldPacketReadInteger<uint16>(state);
}

int WorldPacketReadLong(lua_State* state)
{
    return WorldPacketReadInteger<int32>(state);
}

int WorldPacketReadULong(lua_State* state)
{
    return WorldPacketReadInteger<uint32>(state);
}

int WorldPacketReadFloat(lua_State* state)
{
    return WorldPacketReadNumber<float>(state);
}

int WorldPacketReadDouble(lua_State* state)
{
    return WorldPacketReadNumber<double>(state);
}

int WorldPacketReadGUID(lua_State* state)
{
    WorldPacket* packet = RequireWorldPacket(state, 1);
    ObjectGuid guid;
    try
    {
        (*packet) >> guid;
    }
    catch (ByteBufferException const&)
    {
        return luaL_error(state, "packet read out of range");
    }

    PushObjectGuidValue(state, guid);
    return 1;
}

int WorldPacketReadString(lua_State* state)
{
    WorldPacket* packet = RequireWorldPacket(state, 1);
    std::string value;
    try
    {
        (*packet) >> value;
    }
    catch (ByteBufferException const&)
    {
        return luaL_error(state, "packet read out of range");
    }

    lua_pushlstring(state, value.c_str(), value.size());
    return 1;
}

int WorldPacketWriteGUID(lua_State* state)
{
    WorldPacket* packet = RequireWorldPacket(state, 1);
    ObjectGuid guid = CheckObjectGuidValue(state, 2);
    (*packet) << guid;
    return 0;
}

int WorldPacketWriteString(lua_State* state)
{
    WorldPacket* packet = RequireWorldPacket(state, 1);
    size_t length = 0;
    char const* value = luaL_checklstring(state, 2, &length);
    (*packet) << std::string(value, length);
    return 0;
}

int WorldPacketWriteByte(lua_State* state)
{
    WorldPacket* packet = RequireWorldPacket(state, 1);
    (*packet) << static_cast<int8>(luaL_checkinteger(state, 2));
    return 0;
}

int WorldPacketWriteUByte(lua_State* state)
{
    WorldPacket* packet = RequireWorldPacket(state, 1);
    (*packet) << static_cast<uint8>(luaL_checkinteger(state, 2));
    return 0;
}

int WorldPacketWriteShort(lua_State* state)
{
    WorldPacket* packet = RequireWorldPacket(state, 1);
    (*packet) << static_cast<int16>(luaL_checkinteger(state, 2));
    return 0;
}

int WorldPacketWriteUShort(lua_State* state)
{
    WorldPacket* packet = RequireWorldPacket(state, 1);
    (*packet) << static_cast<uint16>(luaL_checkinteger(state, 2));
    return 0;
}

int WorldPacketWriteLong(lua_State* state)
{
    WorldPacket* packet = RequireWorldPacket(state, 1);
    (*packet) << static_cast<int32>(luaL_checkinteger(state, 2));
    return 0;
}

int WorldPacketWriteULong(lua_State* state)
{
    WorldPacket* packet = RequireWorldPacket(state, 1);
    (*packet) << static_cast<uint32>(luaL_checkinteger(state, 2));
    return 0;
}

int WorldPacketWriteFloat(lua_State* state)
{
    WorldPacket* packet = RequireWorldPacket(state, 1);
    (*packet) << static_cast<float>(luaL_checknumber(state, 2));
    return 0;
}

int WorldPacketWriteDouble(lua_State* state)
{
    WorldPacket* packet = RequireWorldPacket(state, 1);
    (*packet) << static_cast<double>(luaL_checknumber(state, 2));
    return 0;
}

int WorldPacketGC(lua_State* state)
{
    auto* holder = static_cast<LuaWorldPacket*>(luaL_checkudata(state, 1, WORLDPACKET_METATABLE));
    if (holder && holder->owned)
    {
        delete holder->packet;
        holder->packet = nullptr;
        holder->owned = false;
    }

    return 0;
}

int ObjectGuidGetCounter(lua_State* state)
{
    ObjectGuid const* guid = CheckObjectGuid(state, 1);
    lua_pushinteger(state, guid ? guid->GetCounter() : 0);
    return 1;
}

int ObjectGuidGetEntry(lua_State* state)
{
    ObjectGuid const* guid = CheckObjectGuid(state, 1);
    lua_pushinteger(state, guid ? guid->GetEntry() : 0);
    return 1;
}

int ObjectGuidGetHigh(lua_State* state)
{
    ObjectGuid const* guid = CheckObjectGuid(state, 1);
    lua_pushinteger(state, guid ? guid->GetHigh() : 0);
    return 1;
}

int ObjectGuidGetTypeId(lua_State* state)
{
    ObjectGuid const* guid = CheckObjectGuid(state, 1);
    lua_pushinteger(state, guid ? guid->GetTypeId() : TYPEID_OBJECT);
    return 1;
}

int ObjectGuidGetMaxCounter(lua_State* state)
{
    ObjectGuid const* guid = CheckObjectGuid(state, 1);
    lua_pushinteger(state, guid ? guid->GetMaxCounter() : 0);
    return 1;
}

int ObjectGuidGetTypeName(lua_State* state)
{
    ObjectGuid const* guid = CheckObjectGuid(state, 1);
    lua_pushstring(state, guid ? guid->GetTypeName() : "None");
    return 1;
}

int ObjectGuidGetString(lua_State* state)
{
    ObjectGuid const* guid = CheckObjectGuid(state, 1);
    std::string value = guid ? guid->GetString() : "None";
    lua_pushlstring(state, value.c_str(), value.size());
    return 1;
}

int ObjectGuidGetRawValueString(lua_State* state)
{
    ObjectGuid const* guid = CheckObjectGuid(state, 1);
    std::string value = std::to_string(guid ? guid->GetRawValue() : 0);
    lua_pushlstring(state, value.c_str(), value.size());
    return 1;
}

int ObjectGuidIsEmpty(lua_State* state)
{
    ObjectGuid const* guid = CheckObjectGuid(state, 1);
    lua_pushboolean(state, !guid || guid->IsEmpty());
    return 1;
}

int ObjectGuidIsPlayer(lua_State* state)
{
    ObjectGuid const* guid = CheckObjectGuid(state, 1);
    lua_pushboolean(state, guid && guid->IsPlayer());
    return 1;
}

int ObjectGuidIsCreature(lua_State* state)
{
    ObjectGuid const* guid = CheckObjectGuid(state, 1);
    lua_pushboolean(state, guid && guid->IsCreature());
    return 1;
}

int ObjectGuidIsPet(lua_State* state)
{
    ObjectGuid const* guid = CheckObjectGuid(state, 1);
    lua_pushboolean(state, guid && guid->IsPet());
    return 1;
}

int ObjectGuidIsCreatureOrPet(lua_State* state)
{
    ObjectGuid const* guid = CheckObjectGuid(state, 1);
    lua_pushboolean(state, guid && guid->IsCreatureOrPet());
    return 1;
}

int ObjectGuidIsUnit(lua_State* state)
{
    ObjectGuid const* guid = CheckObjectGuid(state, 1);
    lua_pushboolean(state, guid && guid->IsUnit());
    return 1;
}

int ObjectGuidIsItem(lua_State* state)
{
    ObjectGuid const* guid = CheckObjectGuid(state, 1);
    lua_pushboolean(state, guid && guid->IsItem());
    return 1;
}

int ObjectGuidIsGameObject(lua_State* state)
{
    ObjectGuid const* guid = CheckObjectGuid(state, 1);
    lua_pushboolean(state, guid && guid->IsGameObject());
    return 1;
}

int ObjectGuidIsDynamicObject(lua_State* state)
{
    ObjectGuid const* guid = CheckObjectGuid(state, 1);
    lua_pushboolean(state, guid && guid->IsDynamicObject());
    return 1;
}

int ObjectGuidIsCorpse(lua_State* state)
{
    ObjectGuid const* guid = CheckObjectGuid(state, 1);
    lua_pushboolean(state, guid && guid->IsCorpse());
    return 1;
}

int ObjectGuidIsTransport(lua_State* state)
{
    ObjectGuid const* guid = CheckObjectGuid(state, 1);
    lua_pushboolean(state, guid && guid->IsTransport());
    return 1;
}

int ObjectGuidIsMOTransport(lua_State* state)
{
    ObjectGuid const* guid = CheckObjectGuid(state, 1);
    lua_pushboolean(state, guid && guid->IsMOTransport());
    return 1;
}

int ObjectGuidEquals(lua_State* state)
{
    ObjectGuid const* left = CheckObjectGuid(state, 1);
    bool equal = false;
    if (left && luaL_testudata(state, 2, OBJECTGUID_METATABLE))
    {
        ObjectGuid const* right = CheckObjectGuid(state, 2);
        equal = right && *left == *right;
    }

    lua_pushboolean(state, equal);
    return 1;
}

int ObjectGuidGC(lua_State* state)
{
    auto* holder = static_cast<LuaObjectGuid*>(luaL_checkudata(state, 1, OBJECTGUID_METATABLE));
    if (holder)
        holder->guid.~ObjectGuid();
    return 0;
}

void SetMethod(lua_State* state, char const* name, lua_CFunction function)
{
    lua_pushcfunction(state, function);
    lua_setfield(state, -2, name);
}

void SetObjectCompatMethods(lua_State* state)
{
    SetMethod(state, "GetInt32Value", &ObjectGetInt32Value);
    SetMethod(state, "GetFloatValue", &ObjectGetFloatValue);
    SetMethod(state, "GetByteValue", &ObjectGetByteValue);
    SetMethod(state, "GetUInt16Value", &ObjectGetUInt16Value);
    SetMethod(state, "GetUInt64Value", &ObjectGetUInt64Value);
    SetMethod(state, "SetInt32Value", &ObjectSetInt32Value);
    SetMethod(state, "UpdateUInt32Value", &ObjectUpdateUInt32Value);
    SetMethod(state, "SetFloatValue", &ObjectSetFloatValue);
    SetMethod(state, "SetByteValue", &ObjectSetByteValue);
    SetMethod(state, "SetUInt16Value", &ObjectSetUInt16Value);
    SetMethod(state, "SetInt16Value", &ObjectSetInt16Value);
    SetMethod(state, "SetUInt64Value", &ObjectSetUInt64Value);
    SetMethod(state, "IsCorpse", &ObjectIsCorpse);
    SetMethod(state, "IsDynamicObject", &ObjectIsDynamicObject);
    SetMethod(state, "ToCorpse", &ObjectToCorpse);
    SetMethod(state, "ToDynamicObject", &ObjectToDynamicObject);
    SetMethod(state, "ToCreature", &ObjectToCreature);
    SetMethod(state, "ToGameObject", &ObjectToGameObject);
    SetMethod(state, "ToPlayer", &ObjectToPlayer);
    SetMethod(state, "ToUnit", &ObjectToUnit);
}

void SetWorldObjectCompatMethods(lua_State* state)
{
    SetMethod(state, "GetNearObject", &WorldObjectGetNearObject);
    SetMethod(state, "GetNearObjects", &WorldObjectGetNearObjects);
    SetMethod(state, "RegisterEvent", &WorldObjectRegisterEvent);
    SetMethod(state, "RemoveEventById", &WorldObjectRemoveEventById);
    SetMethod(state, "RemoveEvents", &WorldObjectRemoveEvents);
    SetMethod(state, "SendPacket", &WorldObjectSendPacket);
}

void SetGlobalInteger(lua_State* state, char const* name, lua_Integer value)
{
    lua_pushinteger(state, value);
    lua_setglobal(state, name);
}
}

TurtleLuaEngine sTurtleLuaEngine;

TurtleLuaEngine::TurtleLuaEngine()
    : _state(nullptr), _enabled(false), _reloadPending(false), _stateId(1), _nextTimedEventId(1)
{
}

TurtleLuaEngine::~TurtleLuaEngine()
{
    Shutdown();
}

void TurtleLuaEngine::Initialize()
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    _enabled = sConfig.GetBoolDefault("Eluna.Enabled", true);
    _scriptPath = sConfig.GetStringDefault("Eluna.ScriptPath", "lua_scripts");

    if (!_enabled)
    {
        sLog.outString("[Lua] Eluna compatibility layer disabled.");
        return;
    }

    OpenState();
    LoadScripts();
    CallServerEvent(ELUNA_EVENT_ON_LUA_STATE_OPEN);
    CallServerEvent(WORLD_EVENT_ON_STARTUP);
}

void TurtleLuaEngine::Shutdown()
{
    std::lock_guard<std::recursive_mutex> guard(_lock);
    if (IsEnabled())
        CallServerEvent(WORLD_EVENT_ON_SHUTDOWN);
    CloseState();
    _enabled = false;
    _reloadPending = false;
}

void TurtleLuaEngine::Reload()
{
    std::lock_guard<std::recursive_mutex> guard(_lock);
    _reloadPending = true;
}

void TurtleLuaEngine::Update(uint32 diff)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!_enabled)
        return;

    if (_reloadPending)
    {
        _reloadPending = false;

        sLog.outString("[Lua] Reloading Lua scripts...");
        CloseState();
        OpenState();
        LoadScripts();
        CallServerEvent(ELUNA_EVENT_ON_LUA_STATE_OPEN);
    }

    if (!IsEnabled())
        return;

    UpdateHttpResponses();
    UpdateAsyncQueries();
    UpdateTimedEvents(diff);
    CallServerEvent(WORLD_EVENT_ON_UPDATE, diff);
}

void TurtleLuaEngine::EnqueueAsyncQueryResult(uint64 stateId, int functionRef, QueryNamedResult* rawResult)
{
    std::unique_ptr<QueryNamedResult> result(rawResult);
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!_state || stateId != _stateId)
        return;

    std::lock_guard<std::mutex> queueGuard(_asyncQueryLock);
    _asyncQueries.push_back(PendingAsyncQuery{ functionRef, result.release() });
}

void TurtleLuaEngine::UpdateAsyncQueries()
{
    std::vector<PendingAsyncQuery> readyQueries;
    {
        std::lock_guard<std::mutex> queueGuard(_asyncQueryLock);
        readyQueries.swap(_asyncQueries);
    }

    for (PendingAsyncQuery& query : readyQueries)
    {
        std::unique_ptr<QueryNamedResult> result(query.result);

        if (!_state)
            continue;

        lua_rawgeti(_state, LUA_REGISTRYINDEX, query.functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            luaL_unref(_state, LUA_REGISTRYINDEX, query.functionRef);
            continue;
        }

        PushQueryResult(_state, result.release());
        if (lua_pcall(_state, 1, 0, 0) != LUA_OK)
        {
            LogError("async database query");
            lua_pop(_state, 1);
        }

        luaL_unref(_state, LUA_REGISTRYINDEX, query.functionRef);
    }
}

void TurtleLuaEngine::ClearAsyncQueries()
{
    std::vector<PendingAsyncQuery> pendingQueries;
    {
        std::lock_guard<std::mutex> queueGuard(_asyncQueryLock);
        pendingQueries.swap(_asyncQueries);
    }

    for (PendingAsyncQuery& query : pendingQueries)
    {
        delete query.result;
        if (_state)
            luaL_unref(_state, LUA_REGISTRYINDEX, query.functionRef);
    }
}

void TurtleLuaEngine::EnqueueHttpResponse(uint64 stateId, int functionRef, int statusCode, std::string body, std::vector<std::pair<std::string, std::string>> headers)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!_state || stateId != _stateId)
        return;

    std::lock_guard<std::mutex> queueGuard(_httpResponseLock);
    _httpResponses.push_back(PendingHttpResponse{ functionRef, statusCode, std::move(body), std::move(headers) });
}

void TurtleLuaEngine::UpdateHttpResponses()
{
    std::vector<PendingHttpResponse> readyResponses;
    {
        std::lock_guard<std::mutex> queueGuard(_httpResponseLock);
        readyResponses.swap(_httpResponses);
    }

    for (PendingHttpResponse& response : readyResponses)
    {
        if (!_state)
            continue;

        lua_rawgeti(_state, LUA_REGISTRYINDEX, response.functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            luaL_unref(_state, LUA_REGISTRYINDEX, response.functionRef);
            continue;
        }

        lua_pushinteger(_state, response.statusCode);
        lua_pushlstring(_state, response.body.data(), response.body.size());
        lua_newtable(_state);
        for (auto const& header : response.headers)
        {
            lua_pushstring(_state, header.first.c_str());
            lua_pushstring(_state, header.second.c_str());
            lua_settable(_state, -3);
        }

        if (lua_pcall(_state, 3, 0, 0) != LUA_OK)
        {
            LogError("http request");
            lua_pop(_state, 1);
        }

        luaL_unref(_state, LUA_REGISTRYINDEX, response.functionRef);
    }
}

void TurtleLuaEngine::ClearHttpResponses()
{
    std::vector<PendingHttpResponse> pendingResponses;
    {
        std::lock_guard<std::mutex> queueGuard(_httpResponseLock);
        pendingResponses.swap(_httpResponses);
    }

    if (!_state)
        return;

    for (PendingHttpResponse& response : pendingResponses)
        luaL_unref(_state, LUA_REGISTRYINDEX, response.functionRef);
}

void TurtleLuaEngine::OpenState()
{
    CloseState();

    _state = luaL_newstate();
    if (!_state)
    {
        sLog.outError("[Lua] Could not create Lua state.");
        return;
    }

    luaL_openlibs(_state);

    lua_pushlightuserdata(_state, this);
    lua_setfield(_state, LUA_REGISTRYINDEX, "TurtleLuaEngine");

    RegisterGlobals();
    RegisterPlayerMetatable();
    RegisterCreatureMetatable();
    RegisterGameObjectMetatable();
    RegisterCorpseMetatable();
    RegisterDynamicObjectMetatable();
    RegisterGameObjectTemplateMetatable();
    RegisterItemMetatable();
    RegisterItemTemplateMetatable();
    RegisterCreatureTemplateMetatable();
    RegisterQuestMetatable();
    RegisterGroupMetatable();
    RegisterGuildMetatable();
    RegisterMapMetatable();
    RegisterInstanceDataMetatable();
    RegisterBattleGroundMetatable();
    RegisterTicketMetatable();
    RegisterAuraMetatable();
    RegisterSpellMetatable();
    RegisterSpellInfoMetatable();
    RegisterSpellTargetsMetatable();
    RegisterWorldPacketMetatable();
    RegisterQueryMetatable();
    RegisterObjectGuidMetatable();
    RegisterChatHandlerMetatable();
    RegisterChannelMetatable();
    RegisterRollMetatable();
}

void TurtleLuaEngine::CloseState()
{
    if (_state)
        CallServerEvent(ELUNA_EVENT_ON_LUA_STATE_CLOSE);

    ++_stateId;

    _serverEvents.clear();
    _playerEvents.clear();
    _battleGroundEvents.clear();
    _ticketEvents.clear();
    _mapEvents.clear();
    _instanceEvents.clear();
    _creatureEvents.clear();
    _uniqueCreatureEvents.clear();
    _gameObjectEvents.clear();
    _itemEvents.clear();
    _spellEvents.clear();
    _packetEvents.clear();
    _creatureGossipEvents.clear();
    _gameObjectGossipEvents.clear();
    _itemGossipEvents.clear();
    _playerGossipEvents.clear();
    ClearAsyncQueries();
    ClearHttpResponses();
    _timedEvents.clear();

    if (_state)
    {
        lua_close(_state);
        _state = nullptr;
    }
}

void TurtleLuaEngine::RegisterGlobals()
{
    lua_register(_state, "RegisterPlayerEvent", &LuaRegisterPlayerEvent);
    lua_register(_state, "RegisterServerEvent", &LuaRegisterServerEvent);
    lua_register(_state, "RegisterGroupEvent", &LuaRegisterGroupEvent);
    lua_register(_state, "RegisterGuildEvent", &LuaRegisterGuildEvent);
    lua_register(_state, "RegisterBGEvent", &LuaRegisterBGEvent);
    lua_register(_state, "RegisterTicketEvent", &LuaRegisterTicketEvent);
    lua_register(_state, "RegisterPacketEvent", &LuaRegisterPacketEvent);
    lua_register(_state, "RegisterMapEvent", &LuaRegisterMapEvent);
    lua_register(_state, "RegisterInstanceEvent", &LuaRegisterInstanceEvent);
    lua_register(_state, "RegisterCreatureEvent", &LuaRegisterCreatureEvent);
    lua_register(_state, "RegisterUniqueCreatureEvent", &LuaRegisterUniqueCreatureEvent);
    lua_register(_state, "RegisterGameObjectEvent", &LuaRegisterGameObjectEvent);
    lua_register(_state, "RegisterItemEvent", &LuaRegisterItemEvent);
    lua_register(_state, "RegisterSpellEvent", &LuaRegisterSpellEvent);
    lua_register(_state, "RegisterCreatureGossipEvent", &LuaRegisterCreatureGossipEvent);
    lua_register(_state, "RegisterGameObjectGossipEvent", &LuaRegisterGameObjectGossipEvent);
    lua_register(_state, "RegisterItemGossipEvent", &LuaRegisterItemGossipEvent);
    lua_register(_state, "RegisterPlayerGossipEvent", &LuaRegisterPlayerGossipEvent);
    lua_register(_state, "ClearPlayerEvents", &LuaClearPlayerEvents);
    lua_register(_state, "ClearServerEvents", &LuaClearServerEvents);
    lua_register(_state, "ClearGroupEvents", &LuaClearGroupEvents);
    lua_register(_state, "ClearGuildEvents", &LuaClearGuildEvents);
    lua_register(_state, "ClearBattleGroundEvents", &LuaClearBattleGroundEvents);
    lua_register(_state, "ClearTicketEvents", &LuaClearTicketEvents);
    lua_register(_state, "ClearPacketEvents", &LuaClearPacketEvents);
    lua_register(_state, "ClearMapEvents", &LuaClearMapEvents);
    lua_register(_state, "ClearInstanceEvents", &LuaClearInstanceEvents);
    lua_register(_state, "ClearCreatureEvents", &LuaClearCreatureEvents);
    lua_register(_state, "ClearUniqueCreatureEvents", &LuaClearUniqueCreatureEvents);
    lua_register(_state, "ClearGameObjectEvents", &LuaClearGameObjectEvents);
    lua_register(_state, "ClearItemEvents", &LuaClearItemEvents);
    lua_register(_state, "ClearSpellEvents", &LuaClearSpellEvents);
    lua_register(_state, "ClearCreatureGossipEvents", &LuaClearCreatureGossipEvents);
    lua_register(_state, "ClearGameObjectGossipEvents", &LuaClearGameObjectGossipEvents);
    lua_register(_state, "ClearItemGossipEvents", &LuaClearItemGossipEvents);
    lua_register(_state, "ClearPlayerGossipEvents", &LuaClearPlayerGossipEvents);
    lua_register(_state, "GetLuaEngine", &LuaGetLuaEngine);
    lua_register(_state, "GetCoreName", &LuaGetCoreName);
    lua_register(_state, "GetRealmID", &LuaGetRealmID);
    lua_register(_state, "GetCoreVersion", &LuaGetCoreVersion);
    lua_register(_state, "GetCoreExpansion", &LuaGetCoreExpansion);
    lua_register(_state, "GetStateMap", &LuaGetStateMap);
    lua_register(_state, "GetStateMapId", &LuaGetStateMapId);
    lua_register(_state, "GetStateInstanceId", &LuaGetStateInstanceId);
    lua_register(_state, "IsCompatibilityMode", &LuaIsCompatibilityMode);
    lua_register(_state, "bit_and", &LuaBitAnd);
    lua_register(_state, "bit_or", &LuaBitOr);
    lua_register(_state, "bit_lshift", &LuaBitLShift);
    lua_register(_state, "bit_rshift", &LuaBitRShift);
    lua_register(_state, "bit_xor", &LuaBitXor);
    lua_register(_state, "bit_not", &LuaBitNot);
    lua_register(_state, "CreateLongLong", &LuaCreateLongLong);
    lua_register(_state, "CreateULongLong", &LuaCreateULongLong);
    lua_register(_state, "CreateInt64", &LuaCreateLongLong);
    lua_register(_state, "CreateUint64", &LuaCreateULongLong);
    lua_register(_state, "GetCurrTime", &LuaGetCurrTime);
    lua_register(_state, "GetTimeDiff", &LuaGetTimeDiff);
    lua_register(_state, "IsInventoryPos", &LuaIsInventoryPos);
    lua_register(_state, "IsEquipmentPos", &LuaIsEquipmentPos);
    lua_register(_state, "IsBankPos", &LuaIsBankPos);
    lua_register(_state, "IsBagPos", &LuaIsBagPos);
    lua_register(_state, "CreateLuaEvent", &LuaCreateLuaEvent);
    lua_register(_state, "RemoveEventById", &LuaRemoveEventById);
    lua_register(_state, "RemoveEvents", &LuaRemoveEvents);
    lua_register(_state, "ReloadEluna", &LuaReloadEluna);
    lua_register(_state, "RunCommand", &LuaRunCommand);
    lua_register(_state, "Kick", &LuaKick);
    lua_register(_state, "Ban", &LuaBan);
    lua_register(_state, "SaveAllPlayers", &LuaSaveAllPlayers);
    lua_register(_state, "SendMail", &LuaSendMail);
    lua_register(_state, "GetPlayerByName", &LuaGetPlayerByName);
    lua_register(_state, "GetPlayerByGUID", &LuaGetPlayerByGUID);
    lua_register(_state, "GetPlayerByGUIDLow", &LuaGetPlayerByGUIDLow);
    lua_register(_state, "CreateObjectGuid", &LuaCreateObjectGuid);
    lua_register(_state, "CreatePlayerGuid", &LuaCreatePlayerGuid);
    lua_register(_state, "CreateItemGuid", &LuaCreateItemGuid);
    lua_register(_state, "CreateCreatureGuid", &LuaCreateCreatureGuid);
    lua_register(_state, "CreateGameObjectGuid", &LuaCreateGameObjectGuid);
    lua_register(_state, "CreatePacket", &LuaCreatePacket);
    lua_register(_state, "GetPlayerGUID", &LuaGetPlayerGUID);
    lua_register(_state, "GetItemGUID", &LuaGetItemGUID);
    lua_register(_state, "GetObjectGUID", &LuaGetObjectGUID);
    lua_register(_state, "GetUnitGUID", &LuaGetUnitGUID);
    lua_register(_state, "GetGUIDLow", &LuaGetGUIDLow);
    lua_register(_state, "GetGUIDType", &LuaGetGUIDType);
    lua_register(_state, "GetGUIDEntry", &LuaGetGUIDEntry);
    lua_register(_state, "SendWorldMessage", &LuaSendWorldMessage);
    lua_register(_state, "GetGameTime", &LuaGetGameTime);
    lua_register(_state, "GetQuest", &LuaGetQuest);
    lua_register(_state, "GetItemTemplate", &LuaGetItemTemplate);
    lua_register(_state, "GetItemPrototype", &LuaGetItemTemplate);
    lua_register(_state, "GetItemLink", &LuaGetItemLink);
    lua_register(_state, "GetAreaName", &LuaGetAreaName);
    lua_register(_state, "GetActiveGameEvents", &LuaGetActiveGameEvents);
    lua_register(_state, "IsGameEventActive", &LuaIsGameEventActive);
    lua_register(_state, "StartGameEvent", &LuaStartGameEvent);
    lua_register(_state, "StopGameEvent", &LuaStopGameEvent);
    lua_register(_state, "GetMapEntrance", &LuaGetMapEntrance);
    lua_register(_state, "GetGossipMenuOptionLocale", &LuaGetGossipMenuOptionLocale);
    lua_register(_state, "GetOwnerHalaa", &LuaGetOwnerHalaa);
    lua_register(_state, "SetOwnerHalaa", &LuaSetOwnerHalaa);
    lua_register(_state, "GetCreatureTemplate", &LuaGetCreatureTemplate);
    lua_register(_state, "GetCreatureInfo", &LuaGetCreatureTemplate);
    lua_register(_state, "GetGameObjectTemplate", &LuaGetGameObjectTemplate);
    lua_register(_state, "GetGameObjectInfo", &LuaGetGameObjectTemplate);
    lua_register(_state, "GetGOTemplate", &LuaGetGameObjectTemplate);
    lua_register(_state, "GetGOInfo", &LuaGetGameObjectTemplate);
    lua_register(_state, "GetSpellInfo", &LuaGetSpellInfo);
    lua_register(_state, "GetSpellEntry", &LuaGetSpellInfo);
    lua_register(_state, "LookupEntry", &LuaLookupEntry);
    lua_register(_state, "GetGuildById", &LuaGetGuildById);
    lua_register(_state, "GetGuildByName", &LuaGetGuildByName);
    lua_register(_state, "GetGuildByLeaderGUID", &LuaGetGuildByLeaderGUID);
    lua_register(_state, "GetPlayersInWorld", &LuaGetPlayersInWorld);
    lua_register(_state, "GetPlayerCount", &LuaGetPlayerCount);
    lua_register(_state, "GetMapById", &LuaGetMapById);
    lua_register(_state, "PerformIngameSpawn", &LuaPerformIngameSpawn);
    lua_register(_state, "AddTaxiPath", &LuaAddTaxiPath);
    lua_register(_state, "AddVendorItem", &LuaAddVendorItem);
    lua_register(_state, "VendorRemoveItem", &LuaVendorRemoveItem);
    lua_register(_state, "VendorRemoveAllItems", &LuaVendorRemoveAllItems);
    lua_register(_state, "WorldDBQuery", &LuaWorldDBQuery);
    lua_register(_state, "CharDBQuery", &LuaCharDBQuery);
    lua_register(_state, "CharacterDBQuery", &LuaCharDBQuery);
    lua_register(_state, "AuthDBQuery", &LuaLoginDBQuery);
    lua_register(_state, "LoginDBQuery", &LuaLoginDBQuery);
    lua_register(_state, "WorldDBQueryAsync", &LuaWorldDBQueryAsync);
    lua_register(_state, "CharDBQueryAsync", &LuaCharDBQueryAsync);
    lua_register(_state, "CharacterDBQueryAsync", &LuaCharDBQueryAsync);
    lua_register(_state, "AuthDBQueryAsync", &LuaLoginDBQueryAsync);
    lua_register(_state, "LoginDBQueryAsync", &LuaLoginDBQueryAsync);
    lua_register(_state, "WorldDBExecute", &LuaWorldDBExecute);
    lua_register(_state, "CharDBExecute", &LuaCharDBExecute);
    lua_register(_state, "CharacterDBExecute", &LuaCharDBExecute);
    lua_register(_state, "AuthDBExecute", &LuaLoginDBExecute);
    lua_register(_state, "LoginDBExecute", &LuaLoginDBExecute);
    lua_register(_state, "HttpRequest", &LuaHttpRequest);
    lua_register(_state, "PrintInfo", &LuaPrintInfo);
    lua_register(_state, "PrintError", &LuaPrintError);
    lua_register(_state, "PrintDebug", &LuaPrintDebug);
    lua_register(_state, "print", &LuaPrint);

    SetGlobalInteger(_state, "HIGHGUID_ITEM", HIGHGUID_ITEM);
    SetGlobalInteger(_state, "HIGHGUID_PLAYER", HIGHGUID_PLAYER);
    SetGlobalInteger(_state, "HIGHGUID_GAMEOBJECT", HIGHGUID_GAMEOBJECT);
    SetGlobalInteger(_state, "HIGHGUID_TRANSPORT", HIGHGUID_TRANSPORT);
    SetGlobalInteger(_state, "HIGHGUID_UNIT", HIGHGUID_UNIT);
    SetGlobalInteger(_state, "HIGHGUID_PET", HIGHGUID_PET);
    SetGlobalInteger(_state, "HIGHGUID_DYNAMICOBJECT", HIGHGUID_DYNAMICOBJECT);
    SetGlobalInteger(_state, "HIGHGUID_CORPSE", HIGHGUID_CORPSE);
    SetGlobalInteger(_state, "HIGHGUID_MO_TRANSPORT", HIGHGUID_MO_TRANSPORT);
}

void TurtleLuaEngine::RegisterPlayerMetatable()
{
    luaL_newmetatable(_state, PLAYER_METATABLE);

    lua_newtable(_state);
    SetMethod(_state, "GetName", &PlayerGetName);
    SetMethod(_state, "GetEntry", &ObjectGetEntry);
    SetMethod(_state, "GetGUIDLow", &PlayerGetGUIDLow);
    SetMethod(_state, "GetGUID", &ObjectGetGUID);
    SetMethod(_state, "GetGuid", &ObjectGetGUID);
    SetMethod(_state, "GetObjectGuid", &ObjectGetGUID);
    SetMethod(_state, "GetTypeId", &ObjectGetTypeId);
    SetMethod(_state, "GetTypeID", &ObjectGetTypeId);
    SetMethod(_state, "IsPlayer", &ObjectIsPlayer);
    SetMethod(_state, "IsCreature", &ObjectIsCreature);
    SetMethod(_state, "IsUnit", &ObjectIsUnit);
    SetMethod(_state, "IsGameObject", &ObjectIsGameObject);
    SetMethod(_state, "IsItem", &ObjectIsItem);
    SetMethod(_state, "IsWorldObject", &ObjectIsWorldObject);
    SetMethod(_state, "GetScale", &ObjectGetScale);
    SetMethod(_state, "SetScale", &ObjectSetScale);
    SetMethod(_state, "IsInWorld", &ObjectIsInWorld);
    SetMethod(_state, "GetUInt32Value", &ObjectGetUInt32Value);
    SetMethod(_state, "SetUInt32Value", &ObjectSetUInt32Value);
    SetMethod(_state, "HasFlag", &ObjectHasFlag);
    SetMethod(_state, "SetFlag", &ObjectSetFlag);
    SetMethod(_state, "RemoveFlag", &ObjectRemoveFlag);
    SetObjectCompatMethods(_state);
    SetWorldObjectCompatMethods(_state);
    SetMethod(_state, "GetLevel", &PlayerGetLevel);
    SetMethod(_state, "GetAccountId", &PlayerGetAccountId);
    SetMethod(_state, "GetMapId", &PlayerGetMapId);
    SetMethod(_state, "GetMapID", &PlayerGetMapId);
    SetMethod(_state, "GetPhaseMask", &WorldObjectGetPhaseMask);
    SetMethod(_state, "SetPhaseMask", &WorldObjectSetPhaseMask);
    SetMethod(_state, "GetInstanceId", &WorldObjectGetInstanceId);
    SetMethod(_state, "GetInstanceID", &WorldObjectGetInstanceId);
    SetMethod(_state, "GetZoneId", &PlayerGetZoneId);
    SetMethod(_state, "GetZoneID", &PlayerGetZoneId);
    SetMethod(_state, "GetAreaId", &WorldObjectGetAreaId);
    SetMethod(_state, "GetAreaID", &WorldObjectGetAreaId);
    SetMethod(_state, "GetX", &PlayerGetX);
    SetMethod(_state, "GetY", &PlayerGetY);
    SetMethod(_state, "GetZ", &PlayerGetZ);
    SetMethod(_state, "GetO", &PlayerGetO);
    SetMethod(_state, "GetLocation", &WorldObjectGetLocation);
    SetMethod(_state, "IsAlive", &UnitIsAlive);
    SetMethod(_state, "IsDead", &UnitIsDead);
    SetMethod(_state, "IsDying", &UnitIsDying);
    SetMethod(_state, "IsMounted", &UnitIsMounted);
    SetMethod(_state, "IsRooted", &UnitIsRooted);
    SetMethod(_state, "IsFeared", &UnitIsFeared);
    SetMethod(_state, "IsConfused", &UnitIsConfused);
    SetMethod(_state, "IsPolymorphed", &UnitIsPolymorphed);
    SetMethod(_state, "IsStopped", &UnitIsStopped);
    SetMethod(_state, "IsTaxiFlying", &UnitIsTaxiFlying);
    SetMethod(_state, "IsCharmed", &UnitIsCharmed);
    SetMethod(_state, "IsAttackingPlayer", &UnitIsAttackingPlayer);
    SetMethod(_state, "GetStandState", &UnitGetStandState);
    SetMethod(_state, "SetStandState", &UnitSetStandState);
    SetMethod(_state, "GetHealth", &UnitGetHealth);
    SetMethod(_state, "GetMaxHealth", &UnitGetMaxHealth);
    SetMethod(_state, "SetHealth", &UnitSetHealth);
    SetMethod(_state, "GetHealthPct", &UnitGetHealthPct);
    SetMethod(_state, "GetHealthPercent", &UnitGetHealthPct);
    SetMethod(_state, "IsFullHealth", &UnitIsFullHealth);
    SetMethod(_state, "HealthAbovePct", &UnitHealthAbovePct);
    SetMethod(_state, "HealthBelowPct", &UnitHealthBelowPct);
    SetMethod(_state, "CountPctFromCurHealth", &UnitCountPctFromCurHealth);
    SetMethod(_state, "CountPctFromMaxHealth", &UnitCountPctFromMaxHealth);
    SetMethod(_state, "SetMaxHealth", &UnitSetMaxHealth);
    SetMethod(_state, "SetLevel", &UnitSetLevel);
    SetMethod(_state, "GetPower", &UnitGetPower);
    SetMethod(_state, "GetMaxPower", &UnitGetMaxPower);
    SetMethod(_state, "SetPower", &UnitSetPower);
    SetMethod(_state, "SetMaxPower", &UnitSetMaxPower);
    SetMethod(_state, "ModifyPower", &UnitModifyPower);
    SetMethod(_state, "GetPowerType", &UnitGetPowerType);
    SetMethod(_state, "SetPowerType", &UnitSetPowerType);
    SetMethod(_state, "GetPowerPct", &UnitGetPowerPct);
    SetMethod(_state, "GetRace", &UnitGetRace);
    SetMethod(_state, "GetClass", &UnitGetClass);
    SetMethod(_state, "GetRaceMask", &UnitGetRaceMask);
    SetMethod(_state, "GetClassMask", &UnitGetClassMask);
    SetMethod(_state, "GetRaceAsString", &UnitGetRaceAsString);
    SetMethod(_state, "GetClassAsString", &UnitGetClassAsString);
    SetMethod(_state, "GetCreatureType", &UnitGetCreatureType);
    SetMethod(_state, "GetStat", &UnitGetStat);
    SetMethod(_state, "GetBaseSpellPower", &UnitGetBaseSpellPower);
    SetMethod(_state, "GetGender", &UnitGetGender);
    SetMethod(_state, "GetUnitState", &UnitGetUnitState);
    SetMethod(_state, "HasUnitState", &UnitHasUnitState);
    SetMethod(_state, "AddUnitState", &UnitAddUnitState);
    SetMethod(_state, "ClearUnitState", &UnitClearUnitState);
    SetMethod(_state, "GetSpeed", &UnitGetSpeed);
    SetMethod(_state, "GetSpeedRate", &UnitGetSpeedRate);
    SetMethod(_state, "SetSpeed", &UnitSetSpeed);
    SetMethod(_state, "SetSpeedRate", &UnitSetSpeedRate);
    SetMethod(_state, "GetMovementType", &UnitGetMovementType);
    SetMethod(_state, "MoveStop", &UnitMoveStop);
    SetMethod(_state, "MoveExpire", &UnitMoveExpire);
    SetMethod(_state, "MoveClear", &UnitMoveClear);
    SetMethod(_state, "MoveIdle", &UnitMoveIdle);
    SetMethod(_state, "MoveRandom", &UnitMoveRandom);
    SetMethod(_state, "MoveHome", &UnitMoveHome);
    SetMethod(_state, "MoveFollow", &UnitMoveFollow);
    SetMethod(_state, "MoveChase", &UnitMoveChase);
    SetMethod(_state, "MoveConfused", &UnitMoveConfused);
    SetMethod(_state, "MoveFleeing", &UnitMoveFleeing);
    SetMethod(_state, "MoveTo", &UnitMoveTo);
    SetMethod(_state, "MoveJump", &UnitMoveJump);
    SetMethod(_state, "NearTeleport", &UnitNearTeleport);
    SetMethod(_state, "GetAttackers", &UnitGetAttackers);
    SetMethod(_state, "ClearThreatList", &UnitClearThreatList);
    SetMethod(_state, "GetThreatList", &UnitGetThreatList);
    SetMethod(_state, "AddThreat", &UnitAddThreat);
    SetMethod(_state, "ModifyThreatPct", &UnitModifyThreatPct);
    SetMethod(_state, "ModifyThreatPercent", &UnitModifyThreatPct);
    SetMethod(_state, "ClearThreat", &UnitClearThreat);
    SetMethod(_state, "ResetAllThreat", &UnitResetAllThreat);
    SetMethod(_state, "GetThreat", &UnitGetThreat);
    SetMethod(_state, "GetFriendlyUnitsInRange", &UnitGetFriendlyUnitsInRange);
    SetMethod(_state, "GetUnfriendlyUnitsInRange", &UnitGetUnfriendlyUnitsInRange);
    SetMethod(_state, "GetCurrentSpell", &UnitGetCurrentSpell);
    SetMethod(_state, "HandleStatModifier", &UnitHandleStatModifier);
    SetMethod(_state, "IsInAccessiblePlaceFor", &UnitIsInAccessiblePlaceFor);
    SetMethod(_state, "IsOnVehicle", &UnitIsOnVehicle);
    SetMethod(_state, "GetVehicle", &UnitGetVehicle);
    SetMethod(_state, "GetVehicleKit", &UnitGetVehicleKit);
    SetMethod(_state, "HasAura", &UnitHasAura);
    SetMethod(_state, "GetAura", &UnitGetAura);
    SetMethod(_state, "AddAura", &UnitAddAura);
    SetMethod(_state, "RemoveAura", &UnitRemoveAura);
    SetMethod(_state, "RemoveAurasDueToSpell", &UnitRemoveAura);
    SetMethod(_state, "RemoveAllAuras", &UnitRemoveAllAuras);
    SetMethod(_state, "RemoveArenaAuras", &UnitRemoveArenaAuras);
    SetMethod(_state, "GetVictim", &UnitGetVictim);
    SetMethod(_state, "Attack", &UnitAttack);
    SetMethod(_state, "AttackStop", &UnitAttackStop);
    SetMethod(_state, "Kill", &UnitKill);
    SetMethod(_state, "DealDamage", &UnitDealDamage);
    SetMethod(_state, "DealHeal", &UnitDealHeal);
    SetMethod(_state, "IsInCombat", &UnitIsInCombat);
    SetMethod(_state, "ClearInCombat", &UnitClearInCombat);
    SetMethod(_state, "SetInCombatWith", &UnitSetInCombatWith);
    SetMethod(_state, "SetOwnerGUID", &UnitSetOwnerGUID);
    SetMethod(_state, "SetOwnerGuid", &UnitSetOwnerGUID);
    SetMethod(_state, "SetCreatorGUID", &UnitSetCreatorGUID);
    SetMethod(_state, "SetCreatorGuid", &UnitSetCreatorGUID);
    SetMethod(_state, "SetPetGUID", &UnitSetPetGUID);
    SetMethod(_state, "SetPetGuid", &UnitSetPetGUID);
    SetMethod(_state, "SetCritterGUID", &UnitSetCritterGUID);
    SetMethod(_state, "SetCritterGuid", &UnitSetCritterGUID);
    SetMethod(_state, "SetName", &UnitSetName);
    SetMethod(_state, "SetImmuneTo", &UnitSetImmuneTo);
    SetMethod(_state, "IsUnderWater", &UnitIsUnderWater);
    SetMethod(_state, "IsUnderwater", &UnitIsUnderWater);
    SetMethod(_state, "IsCasting", &UnitIsCasting);
    SetMethod(_state, "IsPvPFlagged", &UnitIsPvPFlagged);
    SetMethod(_state, "IsStandState", &UnitIsStandState);
    SetMethod(_state, "IsVendor", &UnitIsVendor);
    SetMethod(_state, "IsTrainer", &UnitIsTrainer);
    SetMethod(_state, "IsQuestGiver", &UnitIsQuestGiver);
    SetMethod(_state, "IsGossip", &UnitIsGossip);
    SetMethod(_state, "IsTaxi", &UnitIsTaxi);
    SetMethod(_state, "IsGuildMaster", &UnitIsGuildMaster);
    SetMethod(_state, "IsBattleMaster", &UnitIsBattleMaster);
    SetMethod(_state, "IsBanker", &UnitIsBanker);
    SetMethod(_state, "IsInnkeeper", &UnitIsInnkeeper);
    SetMethod(_state, "IsSpiritHealer", &UnitIsSpiritHealer);
    SetMethod(_state, "IsSpiritGuide", &UnitIsSpiritGuide);
    SetMethod(_state, "IsTabardDesigner", &UnitIsTabardDesigner);
    SetMethod(_state, "IsAuctioneer", &UnitIsAuctioneer);
    SetMethod(_state, "IsArmorer", &UnitIsArmorer);
    SetMethod(_state, "IsServiceProvider", &UnitIsServiceProvider);
    SetMethod(_state, "IsSpiritService", &UnitIsSpiritService);
    SetMethod(_state, "GetDisplayId", &UnitGetDisplayId);
    SetMethod(_state, "SetDisplayId", &UnitSetDisplayId);
    SetMethod(_state, "GetNativeDisplayId", &UnitGetNativeDisplayId);
    SetMethod(_state, "SetNativeDisplayId", &UnitSetNativeDisplayId);
    SetMethod(_state, "RestoreDisplayId", &UnitRestoreDisplayId);
    SetMethod(_state, "DeMorph", &UnitDeMorph);
    SetMethod(_state, "GetMountId", &UnitGetMountId);
    SetMethod(_state, "GetMountID", &UnitGetMountId);
    SetMethod(_state, "Mount", &UnitMount);
    SetMethod(_state, "Dismount", &UnitDismount);
    SetMethod(_state, "Unmount", &UnitDismount);
    SetMethod(_state, "GetFaction", &UnitGetFaction);
    SetMethod(_state, "SetFaction", &UnitSetFaction);
    SetMethod(_state, "RestoreFaction", &UnitRestoreFaction);
    SetMethod(_state, "SetSheath", &UnitSetSheath);
    SetMethod(_state, "SetRooted", &UnitSetRooted);
    SetMethod(_state, "SetConfused", &UnitSetConfused);
    SetMethod(_state, "SetFeared", &UnitSetFeared);
    SetMethod(_state, "SetFacing", &UnitSetFacing);
    SetMethod(_state, "SetFacingToObject", &UnitSetFacingToObject);
    SetMethod(_state, "SetPvP", &UnitSetPvP);
    SetMethod(_state, "SetFFA", &UnitSetFFA);
    SetMethod(_state, "SetSanctuary", &UnitSetSanctuary);
    SetMethod(_state, "SetWaterWalk", &UnitSetWaterWalk);
    SetMethod(_state, "StopSpellCast", &UnitStopSpellCast);
    SetMethod(_state, "InterruptSpell", &UnitInterruptSpell);
    SetMethod(_state, "PerformEmote", &UnitPerformEmote);
    SetMethod(_state, "EmoteState", &UnitEmoteState);
    SetMethod(_state, "GetOwner", &UnitGetOwner);
    SetMethod(_state, "GetOwnerGUID", &UnitGetOwnerGUID);
    SetMethod(_state, "GetOwnerGuid", &UnitGetOwnerGUID);
    SetMethod(_state, "GetCreatorGUID", &UnitGetCreatorGUID);
    SetMethod(_state, "GetCreatorGuid", &UnitGetCreatorGUID);
    SetMethod(_state, "GetMinionGUID", &UnitGetPetGUID);
    SetMethod(_state, "GetMinionGuid", &UnitGetPetGUID);
    SetMethod(_state, "GetPetGUID", &UnitGetPetGUID);
    SetMethod(_state, "GetPetGuid", &UnitGetPetGUID);
    SetMethod(_state, "GetCritterGUID", &UnitGetCritterGUID);
    SetMethod(_state, "GetCritterGuid", &UnitGetCritterGUID);
    SetMethod(_state, "GetControllerGUID", &UnitGetControllerGUID);
    SetMethod(_state, "GetControllerGuid", &UnitGetControllerGUID);
    SetMethod(_state, "GetControllerGUIDS", &UnitGetControllerGUIDS);
    SetMethod(_state, "GetControllerGuids", &UnitGetControllerGUIDS);
    SetMethod(_state, "GetCharmer", &UnitGetCharmer);
    SetMethod(_state, "GetCharmerGUID", &UnitGetCharmerGUID);
    SetMethod(_state, "GetCharmerGuid", &UnitGetCharmerGUID);
    SetMethod(_state, "GetCharm", &UnitGetCharm);
    SetMethod(_state, "GetCharmGUID", &UnitGetCharmGUID);
    SetMethod(_state, "GetCharmGuid", &UnitGetCharmGUID);
    SetMethod(_state, "GetCharmerOrOwner", &UnitGetCharmerOrOwner);
    SetMethod(_state, "GetCharmerOrOwnerGUID", &UnitGetCharmerOrOwnerGUID);
    SetMethod(_state, "GetCharmerOrOwnerGuid", &UnitGetCharmerOrOwnerGUID);
    SetMethod(_state, "GetDistance", &WorldObjectGetDistance);
    SetMethod(_state, "GetDistance2d", &WorldObjectGetDistance2d);
    SetMethod(_state, "GetDistance2D", &WorldObjectGetDistance2d);
    SetMethod(_state, "GetExactDistance", &WorldObjectGetExactDistance);
    SetMethod(_state, "GetExactDistance2d", &WorldObjectGetExactDistance2d);
    SetMethod(_state, "GetExactDistance2D", &WorldObjectGetExactDistance2d);
    SetMethod(_state, "GetRelativePoint", &WorldObjectGetRelativePoint);
    SetMethod(_state, "GetAngle", &WorldObjectGetAngle);
    SetMethod(_state, "IsWithinDist", &WorldObjectIsWithinDist);
    SetMethod(_state, "IsWithinDist3d", &WorldObjectIsWithinDist3d);
    SetMethod(_state, "IsWithinDist3D", &WorldObjectIsWithinDist3d);
    SetMethod(_state, "IsWithinDist2d", &WorldObjectIsWithinDist2d);
    SetMethod(_state, "IsWithinDist2D", &WorldObjectIsWithinDist2d);
    SetMethod(_state, "IsWithinDistInMap", &WorldObjectIsWithinDistInMap);
    SetMethod(_state, "IsInMap", &WorldObjectIsInMap);
    SetMethod(_state, "IsInRange", &WorldObjectIsInRange);
    SetMethod(_state, "IsInRange2d", &WorldObjectIsInRange2d);
    SetMethod(_state, "IsInRange2D", &WorldObjectIsInRange2d);
    SetMethod(_state, "IsInRange3d", &WorldObjectIsInRange3d);
    SetMethod(_state, "IsInRange3D", &WorldObjectIsInRange3d);
    SetMethod(_state, "IsInFront", &WorldObjectIsInFront);
    SetMethod(_state, "IsInBack", &WorldObjectIsInBack);
    SetMethod(_state, "IsWithinLOS", &WorldObjectIsWithinLOS);
    SetMethod(_state, "IsInLineOfSight", &WorldObjectIsWithinLOS);
    SetMethod(_state, "IsFriendlyTo", &WorldObjectIsFriendlyTo);
    SetMethod(_state, "IsHostileTo", &WorldObjectIsHostileTo);
    SetMethod(_state, "GetPlayersInRange", &WorldObjectGetPlayersInRange);
    SetMethod(_state, "GetCreaturesInRange", &WorldObjectGetCreaturesInRange);
    SetMethod(_state, "GetGameObjectsInRange", &WorldObjectGetGameObjectsInRange);
    SetMethod(_state, "GetNearestPlayer", &WorldObjectGetNearestPlayer);
    SetMethod(_state, "GetNearestCreature", &WorldObjectGetNearestCreature);
    SetMethod(_state, "GetNearestGameObject", &WorldObjectGetNearestGameObject);
    SetMethod(_state, "FindNearestPlayer", &WorldObjectFindNearestPlayer);
    SetMethod(_state, "FindNearestCreature", &WorldObjectFindNearestCreature);
    SetMethod(_state, "FindNearestGameObject", &WorldObjectFindNearestGameObject);
    SetMethod(_state, "SummonCreature", &WorldObjectSummonCreature);
    SetMethod(_state, "SpawnCreature", &WorldObjectSummonCreature);
    SetMethod(_state, "SummonGameObject", &WorldObjectSummonGameObject);
    SetMethod(_state, "PlayMusic", &WorldObjectPlayMusic);
    SetMethod(_state, "PlayDirectSound", &WorldObjectPlayDirectSound);
    SetMethod(_state, "PlayDistanceSound", &WorldObjectPlayDistanceSound);
    SetMethod(_state, "GetMoney", &PlayerGetMoney);
    SetMethod(_state, "GetCoinage", &PlayerGetCoinage);
    SetMethod(_state, "SetMoney", &PlayerSetMoney);
    SetMethod(_state, "SetCoinage", &PlayerSetMoney);
    SetMethod(_state, "GetXP", &PlayerGetXP);
    SetMethod(_state, "SetXP", &PlayerSetXP);
    SetMethod(_state, "GiveXP", &PlayerGiveXP);
    SetMethod(_state, "IsHorde", &PlayerIsHorde);
    SetMethod(_state, "IsAlliance", &PlayerIsAlliance);
    SetMethod(_state, "GetTeam", &PlayerGetTeam);
    SetMethod(_state, "GetTeamId", &PlayerGetTeamId);
    SetMethod(_state, "GetGuildId", &PlayerGetGuildId);
    SetMethod(_state, "GetGuildRank", &PlayerGetGuildRank);
    SetMethod(_state, "IsInGroup", &PlayerIsInGroup);
    SetMethod(_state, "IsInGuild", &PlayerIsInGuild);
    SetMethod(_state, "IsGroupVisibleFor", &PlayerIsGroupVisibleFor);
    SetMethod(_state, "IsInSameGroupWith", &PlayerIsInSameGroupWith);
    SetMethod(_state, "IsInSameRaidWith", &PlayerIsInSameRaidWith);
    SetMethod(_state, "IsVisibleForPlayer", &PlayerIsVisibleForPlayer);
    SetMethod(_state, "IsHonorOrXPTarget", &PlayerIsHonorOrXPTarget);
    SetMethod(_state, "IsImmuneToDamage", &PlayerIsImmuneToDamage);
    SetMethod(_state, "GetGroup", &PlayerGetGroup);
    SetMethod(_state, "GetOriginalGroup", &PlayerGetOriginalGroup);
    SetMethod(_state, "GetGroupInvite", &PlayerGetGroupInvite);
    SetMethod(_state, "GetSubGroup", &PlayerGetSubGroup);
    SetMethod(_state, "GetOriginalSubGroup", &PlayerGetOriginalSubGroup);
    SetMethod(_state, "GetGuild", &PlayerGetGuild);
    SetMethod(_state, "GetGuildName", &PlayerGetGuildName);
    SetMethod(_state, "AddBonusTalent", &PlayerCompatNoop);
    SetMethod(_state, "CanTitanGrip", &PlayerCompatReturnFalse);
    SetMethod(_state, "CanUninviteFromGroup", &PlayerCanUninviteFromGroup);
    SetMethod(_state, "EquipItem", &PlayerEquipItem);
    SetMethod(_state, "GetAchievementCriteriaProgress", &PlayerCompatReturnNil);
    SetMethod(_state, "GetAchievementPoints", &PlayerCompatReturnZero);
    SetMethod(_state, "GetActiveSpec", &PlayerCompatReturnZero);
    SetMethod(_state, "GetArenaPoints", &PlayerCompatReturnZero);
    SetMethod(_state, "GetBonusTalentCount", &PlayerCompatReturnZero);
    SetMethod(_state, "GetChampioningFaction", &PlayerCompatReturnZero);
    SetMethod(_state, "GetCompletedAchievementsCount", &PlayerCompatReturnZero);
    SetMethod(_state, "GetCompletedQuestsCount", &PlayerGetCompletedQuestsCount);
    SetMethod(_state, "GetCorpse", &PlayerGetCorpse);
    SetMethod(_state, "GetDifficulty", &PlayerGetDifficulty);
    SetMethod(_state, "GetGlyph", &PlayerCompatReturnZero);
    SetMethod(_state, "GetGossipTextId", &PlayerGetGossipTextId);
    SetMethod(_state, "GetHealthBonusFromStamina", &PlayerGetHealthBonusFromStamina);
    SetMethod(_state, "GetHonorPoints", &PlayerGetHonorPoints);
    SetMethod(_state, "GetMailCount", &PlayerGetMailCount);
    SetMethod(_state, "GetMailItem", &PlayerGetMailItem);
    SetMethod(_state, "GetManaBonusFromIntellect", &PlayerGetManaBonusFromIntellect);
    SetMethod(_state, "GetNextRandomRaidMember", &PlayerGetNextRandomRaidMember);
    SetMethod(_state, "GetPhaseMaskForSpawn", &PlayerGetPhaseMaskForSpawn);
    SetMethod(_state, "GetPlayerSettingValue", &PlayerCompatReturnZero);
    SetMethod(_state, "GetShieldBlockValue", &PlayerGetShieldBlockValue);
    SetMethod(_state, "GetSpecsCount", &PlayerGetSpecsCount);
    SetMethod(_state, "GossipAddQuests", &PlayerGossipAddQuests);
    SetMethod(_state, "GossipSendPOI", &PlayerGossipSendPOI);
    SetMethod(_state, "GroupCreate", &PlayerGroupCreate);
    SetMethod(_state, "GroupInvite", &PlayerGroupInvite);
    SetMethod(_state, "HasAchieved", &PlayerCompatReturnFalse);
    SetMethod(_state, "HasCasterSpec", &PlayerCompatReturnFalse);
    SetMethod(_state, "HasHealSpec", &PlayerCompatReturnFalse);
    SetMethod(_state, "HasMeleeSpec", &PlayerCompatReturnFalse);
    SetMethod(_state, "HasTalent", &PlayerHasTalent);
    SetMethod(_state, "HasTankSpec", &PlayerCompatReturnFalse);
    SetMethod(_state, "HasTitle", &PlayerHasTitle);
    SetMethod(_state, "IsInArenaTeam", &PlayerCompatReturnFalse);
    SetMethod(_state, "LeaveBattleground", &PlayerLeaveBattleground);
    SetMethod(_state, "LogoutPlayer", &PlayerLogoutPlayer);
    SetMethod(_state, "ModifyArenaPoints", &PlayerCompatNoop);
    SetMethod(_state, "ModifyHonorPoints", &PlayerModifyHonorPoints);
    SetMethod(_state, "Mute", &PlayerMute);
    SetMethod(_state, "RemoveBonusTalent", &PlayerCompatNoop);
    SetMethod(_state, "RemovedInsignia", &PlayerRemovedInsignia);
    SetMethod(_state, "RemoveFromBattlegroundRaid", &PlayerRemoveFromBattlegroundRaid);
    SetMethod(_state, "RemoveFromGroup", &PlayerRemoveFromGroup);
    SetMethod(_state, "ResetAchievements", &PlayerCompatNoop);
    SetMethod(_state, "ResetPetTalents", &PlayerCompatNoop);
    SetMethod(_state, "ResetTalentsCost", &PlayerResetTalentsCost);
    SetMethod(_state, "ResetTypeCooldowns", &PlayerResetTypeCooldowns);
    SetMethod(_state, "RunCommand", &PlayerRunCommand);
    SetMethod(_state, "SendAuctionMenu", &PlayerSendAuctionMenu);
    SetMethod(_state, "SendGuildInvite", &PlayerSendGuildInvite);
    SetMethod(_state, "SendListInventory", &PlayerSendListInventory);
    SetMethod(_state, "SendMovieStart", &PlayerSendMovieStart);
    SetMethod(_state, "SendPacket", &PlayerSendPacket);
    SetMethod(_state, "SendQuestTemplate", &PlayerSendQuestTemplate);
    SetMethod(_state, "SendShowBank", &PlayerSendShowBank);
    SetMethod(_state, "SendShowMailBox", &PlayerSendShowMailBox);
    SetMethod(_state, "SendSpiritResurrect", &PlayerSendSpiritResurrect);
    SetMethod(_state, "SendTabardVendorActivate", &PlayerSendTabardVendorActivate);
    SetMethod(_state, "SendTaxiMenu", &PlayerSendTaxiMenu);
    SetMethod(_state, "SendTrainerList", &PlayerSendTrainerList);
    SetMethod(_state, "SetAchievement", &PlayerCompatNoop);
    SetMethod(_state, "SetArenaPoints", &PlayerCompatNoop);
    SetMethod(_state, "SetBonusTalentCount", &PlayerCompatNoop);
    SetMethod(_state, "SetFactionForRace", &PlayerSetFactionForRace);
    SetMethod(_state, "SetGender", &PlayerSetGender);
    SetMethod(_state, "SetGlyph", &PlayerCompatNoop);
    SetMethod(_state, "SetGuildRank", &PlayerSetGuildRank);
    SetMethod(_state, "SetHonorPoints", &PlayerSetHonorPoints);
    SetMethod(_state, "SetKnownTitle", &PlayerSetKnownTitle);
    SetMethod(_state, "SetPlayerLock", &PlayerSetPlayerLock);
    SetMethod(_state, "SetSpellPower", &PlayerCompatNoop);
    SetMethod(_state, "StartTaxi", &PlayerStartTaxi);
    SetMethod(_state, "SummonPlayer", &PlayerSummonPlayer);
    SetMethod(_state, "UnbindAllInstances", &PlayerUnbindAllInstances);
    SetMethod(_state, "UnbindInstance", &PlayerUnbindInstance);
    SetMethod(_state, "UnsetKnownTitle", &PlayerUnsetKnownTitle);
    SetMethod(_state, "UpdatePlayerSetting", &PlayerCompatNoop);
    SetMethod(_state, "Whisper", &PlayerWhisper);
    SetMethod(_state, "GetTrader", &PlayerGetTrader);
    SetMethod(_state, "HasSkill", &PlayerHasSkill);
    SetMethod(_state, "GetSkillValue", &PlayerGetSkillValue);
    SetMethod(_state, "GetBaseSkillValue", &PlayerGetBaseSkillValue);
    SetMethod(_state, "GetPureSkillValue", &PlayerGetPureSkillValue);
    SetMethod(_state, "GetMaxSkillValue", &PlayerGetMaxSkillValue);
    SetMethod(_state, "GetPureMaxSkillValue", &PlayerGetPureMaxSkillValue);
    SetMethod(_state, "GetSkillTempBonusValue", &PlayerGetSkillTempBonusValue);
    SetMethod(_state, "GetSkillPermBonusValue", &PlayerGetSkillPermBonusValue);
    SetMethod(_state, "SetSkill", &PlayerSetSkill);
    SetMethod(_state, "AdvanceSkillsToMax", &PlayerAdvanceSkillsToMax);
    SetMethod(_state, "AdvanceSkill", &PlayerAdvanceSkill);
    SetMethod(_state, "AdvanceAllSkills", &PlayerAdvanceAllSkills);
    SetMethod(_state, "GetReputation", &PlayerGetReputation);
    SetMethod(_state, "GetReputationRank", &PlayerGetReputationRank);
    SetMethod(_state, "SetReputation", &PlayerSetReputation);
    SetMethod(_state, "GetTotalPlayedTime", &PlayerGetTotalPlayedTime);
    SetMethod(_state, "GetLevelPlayedTime", &PlayerGetLevelPlayedTime);
    SetMethod(_state, "IsInWater", &PlayerIsInWater);
    SetMethod(_state, "IsMoving", &PlayerIsMoving);
    SetMethod(_state, "IsFlying", &PlayerIsFlying);
    SetMethod(_state, "CanFly", &PlayerCanFly);
    SetMethod(_state, "IsFalling", &PlayerIsFalling);
    SetMethod(_state, "IsDND", &PlayerIsDND);
    SetMethod(_state, "IsAFK", &PlayerIsAFK);
    SetMethod(_state, "CanSpeak", &PlayerCanSpeak);
    SetMethod(_state, "ToggleAFK", &PlayerToggleAFK);
    SetMethod(_state, "ToggleDND", &PlayerToggleDND);
    SetMethod(_state, "IsGMVisible", &PlayerIsGMVisible);
    SetMethod(_state, "SetGMVisible", &PlayerSetGMVisible);
    SetMethod(_state, "IsTaxiCheater", &PlayerIsTaxiCheater);
    SetMethod(_state, "SetTaxiCheat", &PlayerSetTaxiCheat);
    SetMethod(_state, "IsGMChat", &PlayerIsGMChat);
    SetMethod(_state, "SetGMChat", &PlayerSetGMChat);
    SetMethod(_state, "IsAcceptingWhispers", &PlayerIsAcceptingWhispers);
    SetMethod(_state, "IsRested", &PlayerIsRested);
    SetMethod(_state, "HasAtLoginFlag", &PlayerHasAtLoginFlag);
    SetMethod(_state, "SetAtLoginFlag", &PlayerSetAtLoginFlag);
    SetMethod(_state, "SetPvPDeath", &PlayerSetPvPDeath);
    SetMethod(_state, "InArena", &PlayerInArena);
    SetMethod(_state, "InBattleground", &PlayerInBattleground);
    SetMethod(_state, "InBattleGround", &PlayerInBattleground);
    SetMethod(_state, "InBattlegroundQueue", &PlayerInBattlegroundQueue);
    SetMethod(_state, "InBattleGroundQueue", &PlayerInBattlegroundQueue);
    SetMethod(_state, "GetBattlegroundId", &PlayerGetBattlegroundId);
    SetMethod(_state, "GetBattleGroundId", &PlayerGetBattlegroundId);
    SetMethod(_state, "GetBattlegroundTypeId", &PlayerGetBattlegroundTypeId);
    SetMethod(_state, "GetBattleGroundTypeId", &PlayerGetBattlegroundTypeId);
    SetMethod(_state, "GetCurrentBattlegroundQueueSlot", &PlayerGetCurrentBattlegroundQueueSlot);
    SetMethod(_state, "GetLifetimeKills", &PlayerGetLifetimeKills);
    SetMethod(_state, "SetLifetimeKills", &PlayerSetLifetimeKills);
    SetMethod(_state, "AddLifetimeKills", &PlayerAddLifetimeKills);
    SetMethod(_state, "RemoveLifetimeKills", &PlayerRemoveLifetimeKills);
    SetMethod(_state, "CanBlock", &PlayerCanBlock);
    SetMethod(_state, "CanParry", &PlayerCanParry);
    SetMethod(_state, "GetFreeTalentPoints", &PlayerGetFreeTalentPoints);
    SetMethod(_state, "SetFreeTalentPoints", &PlayerSetFreeTalentPoints);
    SetMethod(_state, "LearnTalent", &PlayerLearnTalent);
    SetMethod(_state, "ResetTalents", &PlayerResetTalents);
    SetMethod(_state, "GetComboTarget", &PlayerGetComboTarget);
    SetMethod(_state, "GetComboPoints", &PlayerGetComboPoints);
    SetMethod(_state, "AddComboPoints", &PlayerAddComboPoints);
    SetMethod(_state, "ClearComboPoints", &PlayerClearComboPoints);
    SetMethod(_state, "GetDrunkValue", &PlayerGetDrunkValue);
    SetMethod(_state, "SetDrunkValue", &PlayerSetDrunkValue);
    SetMethod(_state, "GetRestBonus", &PlayerGetRestBonus);
    SetMethod(_state, "SetRestBonus", &PlayerSetRestBonus);
    SetMethod(_state, "GetXPRestBonus", &PlayerGetXPRestBonus);
    SetMethod(_state, "SetAcceptWhispers", &PlayerSetAcceptWhispers);
    SetMethod(_state, "GetChatTag", &PlayerGetChatTag);
    SetMethod(_state, "GetLatency", &PlayerGetLatency);
    SetMethod(_state, "GetPlayerIP", &PlayerGetPlayerIP);
    SetMethod(_state, "GetAccountName", &PlayerGetAccountName);
    SetMethod(_state, "GetDbLocaleIndex", &PlayerGetDbLocaleIndex);
    SetMethod(_state, "GetDbcLocale", &PlayerGetDbcLocale);
    SetMethod(_state, "GetInGameTime", &PlayerGetInGameTime);
    SetMethod(_state, "GetMap", &PlayerGetMap);
    SetMethod(_state, "GetItemByGUID", &PlayerGetItemByGUID);
    SetMethod(_state, "GetItemByGuid", &PlayerGetItemByGUID);
    SetMethod(_state, "GetItemByPos", &PlayerGetItemByPos);
    SetMethod(_state, "GetItemByEntry", &PlayerGetItemByEntry);
    SetMethod(_state, "GetEquippedItemBySlot", &PlayerGetEquippedItemBySlot);
    SetMethod(_state, "GetGMRank", &PlayerGetGMRank);
    SetMethod(_state, "IsGM", &PlayerIsGM);
    SetMethod(_state, "IsGameMaster", &PlayerIsGM);
    SetMethod(_state, "SetGM", &PlayerSetGM);
    SetMethod(_state, "SetGameMaster", &PlayerSetGM);
    SetMethod(_state, "SendBroadcastMessage", &PlayerSendBroadcastMessage);
    SetMethod(_state, "SendSysMessage", &PlayerSendBroadcastMessage);
    SetMethod(_state, "SendNotification", &PlayerSendNotification);
    SetMethod(_state, "SendAreaTriggerMessage", &PlayerSendAreaTriggerMessage);
    SetMethod(_state, "SendAreaTrigger", &PlayerSendAreaTriggerMessage);
    SetMethod(_state, "SendAddonMessage", &PlayerSendAddonMessage);
    SetMethod(_state, "SendCinematicStart", &PlayerSendCinematicStart);
    SetMethod(_state, "SendUpdateWorldState", &PlayerSendUpdateWorldState);
    SetMethod(_state, "Say", &PlayerSay);
    SetMethod(_state, "Yell", &PlayerYell);
    SetMethod(_state, "TextEmote", &PlayerTextEmote);
    SetMethod(_state, "SendUnitSay", &UnitSendUnitSay);
    SetMethod(_state, "SendUnitYell", &UnitSendUnitYell);
    SetMethod(_state, "SendUnitWhisper", &UnitSendUnitWhisper);
    SetMethod(_state, "SendUnitEmote", &UnitSendUnitEmote);
    SetMethod(_state, "SendChatMessageToPlayer", &UnitSendChatMessageToPlayer);
    SetMethod(_state, "ModifyMoney", &PlayerModifyMoney);
    SetMethod(_state, "AddItem", &PlayerAddItem);
    SetMethod(_state, "SetBindPoint", &PlayerSetBindPoint);
    SetMethod(_state, "GetHomebind", &PlayerGetHomebind);
    SetMethod(_state, "Teleport", &PlayerTeleport);
    SetMethod(_state, "TeleportTo", &PlayerTeleport);
    SetMethod(_state, "CastSpell", &UnitCastSpell);
    SetMethod(_state, "CastCustomSpell", &UnitCastCustomSpell);
    SetMethod(_state, "CastSpellAoF", &UnitCastSpellAoF);
    SetMethod(_state, "SaveToDB", &PlayerSaveToDB);
    SetMethod(_state, "KickPlayer", &PlayerKickPlayer);
    SetMethod(_state, "ResurrectPlayer", &PlayerResurrect);
    SetMethod(_state, "KillPlayer", &PlayerKillPlayer);
    SetMethod(_state, "SpawnBones", &PlayerSpawnBones);
    SetMethod(_state, "SpawnCorpseBones", &PlayerSpawnBones);
    SetMethod(_state, "DurabilityRepair", &PlayerDurabilityRepair);
    SetMethod(_state, "DurabilityRepairAll", &PlayerDurabilityRepairAll);
    SetMethod(_state, "DurabilityLoss", &PlayerDurabilityLoss);
    SetMethod(_state, "DurabilityLossAll", &PlayerDurabilityLossAll);
    SetMethod(_state, "DurabilityPointsLoss", &PlayerDurabilityPointsLoss);
    SetMethod(_state, "DurabilityPointsLossAll", &PlayerDurabilityPointsLossAll);
    SetMethod(_state, "DurabilityPointLossForEquipSlot", &PlayerDurabilityPointLossForEquipSlot);
    SetMethod(_state, "HasItem", &PlayerHasItem);
    SetMethod(_state, "HasItemCount", &PlayerHasItem);
    SetMethod(_state, "GetItemCount", &PlayerGetItemCount);
    SetMethod(_state, "CanUseItem", &PlayerCanUseItem);
    SetMethod(_state, "CanEquipItem", &PlayerCanEquipItem);
    SetMethod(_state, "RemoveItem", &PlayerRemoveItem);
    SetMethod(_state, "DestroyItemCount", &PlayerRemoveItem);
    SetMethod(_state, "HasSpell", &PlayerHasSpell);
    SetMethod(_state, "HasSpellCooldown", &PlayerHasSpellCooldown);
    SetMethod(_state, "GetSpellCooldownDelay", &PlayerGetSpellCooldownDelay);
    SetMethod(_state, "ResetSpellCooldown", &PlayerResetSpellCooldown);
    SetMethod(_state, "ResetAllCooldowns", &PlayerResetAllCooldowns);
    SetMethod(_state, "RemoveArenaSpellCooldowns", &PlayerRemoveArenaSpellCooldowns);
    SetMethod(_state, "GetSpells", &PlayerGetSpells);
    SetMethod(_state, "LearnSpell", &PlayerLearnSpell);
    SetMethod(_state, "RemoveSpell", &PlayerRemoveSpell);
    SetMethod(_state, "GetSelection", &PlayerGetSelectedUnit);
    SetMethod(_state, "GetSelectedUnit", &PlayerGetSelectedUnit);
    SetMethod(_state, "GetSelectedPlayer", &PlayerGetSelectedPlayer);
    SetMethod(_state, "GetSelectedCreature", &PlayerGetSelectedCreature);
    SetMethod(_state, "GetSelectedGameObject", &PlayerGetSelectedGameObject);
    SetMethod(_state, "GetSelectedGO", &PlayerGetSelectedGameObject);
    SetMethod(_state, "GetSelectedObject", &PlayerGetSelectedObject);
    SetMethod(_state, "GetSelectedWorldObject", &PlayerGetSelectedObject);
    SetMethod(_state, "GetSelectionGUID", &PlayerGetSelectionGUID);
    SetMethod(_state, "GetSelectionGuid", &PlayerGetSelectionGUID);
    SetMethod(_state, "GetSelectedGUID", &PlayerGetSelectionGUID);
    SetMethod(_state, "GetSelectedGuid", &PlayerGetSelectionGUID);
    SetMethod(_state, "GetSelectedGameObjectGUID", &PlayerGetSelectedGameObjectGUID);
    SetMethod(_state, "GetSelectedGameObjectGuid", &PlayerGetSelectedGameObjectGUID);
    SetMethod(_state, "GetSelectedGOGUID", &PlayerGetSelectedGameObjectGUID);
    SetMethod(_state, "GetSelectedGOGuid", &PlayerGetSelectedGameObjectGUID);
    SetMethod(_state, "CompleteQuest", &PlayerCompleteQuest);
    SetMethod(_state, "IncompleteQuest", &PlayerIncompleteQuest);
    SetMethod(_state, "GetQuestStatus", &PlayerGetQuestStatus);
    SetMethod(_state, "HasQuest", &PlayerHasQuest);
    SetMethod(_state, "GetQuestRewardStatus", &PlayerGetQuestRewardStatus);
    SetMethod(_state, "CanCompleteQuest", &PlayerCanCompleteQuest);
    SetMethod(_state, "CanCompleteRepeatableQuest", &PlayerCanCompleteRepeatableQuest);
    SetMethod(_state, "CanRewardQuest", &PlayerCanRewardQuest);
    SetMethod(_state, "GetQuestLevel", &PlayerGetQuestLevel);
    SetMethod(_state, "GetReqKillOrCastCurrentCount", &PlayerGetReqKillOrCastCurrentCount);
    SetMethod(_state, "HasQuestForGO", &PlayerHasQuestForGO);
    SetMethod(_state, "HasQuestForItem", &PlayerHasQuestForItem);
    SetMethod(_state, "CanShareQuest", &PlayerCanShareQuest);
    SetMethod(_state, "SetQuestStatus", &PlayerSetQuestStatus);
    SetMethod(_state, "AddQuest", &PlayerAddQuest);
    SetMethod(_state, "RewardQuest", &PlayerRewardQuest);
    SetMethod(_state, "FailQuest", &PlayerFailQuest);
    SetMethod(_state, "RemoveQuest", &PlayerRemoveQuest);
    SetMethod(_state, "RemoveActiveQuest", &PlayerRemoveQuest);
    SetMethod(_state, "KilledMonsterCredit", &PlayerKilledMonsterCredit);
    SetMethod(_state, "TalkedToCreature", &PlayerTalkedToCreature);
    SetMethod(_state, "AreaExploredOrEventHappens", &PlayerAreaExploredOrEventHappens);
    SetMethod(_state, "GroupEventHappens", &PlayerGroupEventHappens);
    SetMethod(_state, "GossipClearMenu", &PlayerGossipClearMenu);
    SetMethod(_state, "GossipMenuAddItem", &PlayerGossipMenuAddItem);
    SetMethod(_state, "GossipSendMenu", &PlayerGossipSendMenu);
    SetMethod(_state, "GossipComplete", &PlayerGossipComplete);
    SetMethod(_state, "SendGossipMenu", &PlayerGossipSendMenu);
    SetMethod(_state, "CloseGossip", &PlayerGossipComplete);
    SetMethod(_state, "SummonCreature", &WorldObjectSummonCreature);
    SetMethod(_state, "SummonGameObject", &WorldObjectSummonGameObject);

    lua_setfield(_state, -2, "__index");
    lua_pop(_state, 1);
}

void TurtleLuaEngine::RegisterCreatureMetatable()
{
    luaL_newmetatable(_state, CREATURE_METATABLE);

    lua_newtable(_state);
    SetMethod(_state, "GetName", &CreatureGetName);
    SetMethod(_state, "GetEntry", &CreatureGetEntry);
    SetMethod(_state, "GetTemplate", &CreatureGetTemplate);
    SetMethod(_state, "GetCreatureTemplate", &CreatureGetTemplate);
    SetMethod(_state, "GetCreatureInfo", &CreatureGetTemplate);
    SetMethod(_state, "GetGUIDLow", &CreatureGetGUIDLow);
    SetMethod(_state, "GetGUID", &ObjectGetGUID);
    SetMethod(_state, "GetGuid", &ObjectGetGUID);
    SetMethod(_state, "GetObjectGuid", &ObjectGetGUID);
    SetMethod(_state, "GetDBTableGUIDLow", &CreatureGetDBTableGUIDLow);
    SetMethod(_state, "GetTypeId", &ObjectGetTypeId);
    SetMethod(_state, "GetTypeID", &ObjectGetTypeId);
    SetMethod(_state, "IsPlayer", &ObjectIsPlayer);
    SetMethod(_state, "IsCreature", &ObjectIsCreature);
    SetMethod(_state, "IsUnit", &ObjectIsUnit);
    SetMethod(_state, "IsGameObject", &ObjectIsGameObject);
    SetMethod(_state, "IsItem", &ObjectIsItem);
    SetMethod(_state, "IsWorldObject", &ObjectIsWorldObject);
    SetMethod(_state, "GetScale", &ObjectGetScale);
    SetMethod(_state, "SetScale", &ObjectSetScale);
    SetMethod(_state, "IsInWorld", &ObjectIsInWorld);
    SetMethod(_state, "GetUInt32Value", &ObjectGetUInt32Value);
    SetMethod(_state, "SetUInt32Value", &ObjectSetUInt32Value);
    SetMethod(_state, "HasFlag", &ObjectHasFlag);
    SetMethod(_state, "SetFlag", &ObjectSetFlag);
    SetMethod(_state, "RemoveFlag", &ObjectRemoveFlag);
    SetObjectCompatMethods(_state);
    SetWorldObjectCompatMethods(_state);
    SetMethod(_state, "GetMapId", &WorldObjectGetMapId);
    SetMethod(_state, "GetMapID", &WorldObjectGetMapId);
    SetMethod(_state, "GetMap", &WorldObjectGetMap);
    SetMethod(_state, "GetPhaseMask", &WorldObjectGetPhaseMask);
    SetMethod(_state, "SetPhaseMask", &WorldObjectSetPhaseMask);
    SetMethod(_state, "GetInstanceId", &WorldObjectGetInstanceId);
    SetMethod(_state, "GetInstanceID", &WorldObjectGetInstanceId);
    SetMethod(_state, "GetZoneId", &WorldObjectGetZoneId);
    SetMethod(_state, "GetZoneID", &WorldObjectGetZoneId);
    SetMethod(_state, "GetAreaId", &WorldObjectGetAreaId);
    SetMethod(_state, "GetAreaID", &WorldObjectGetAreaId);
    SetMethod(_state, "GetX", &WorldObjectGetX);
    SetMethod(_state, "GetY", &WorldObjectGetY);
    SetMethod(_state, "GetZ", &WorldObjectGetZ);
    SetMethod(_state, "GetO", &WorldObjectGetO);
    SetMethod(_state, "GetLocation", &WorldObjectGetLocation);
    SetMethod(_state, "GetDistance", &WorldObjectGetDistance);
    SetMethod(_state, "GetDistance2d", &WorldObjectGetDistance2d);
    SetMethod(_state, "GetDistance2D", &WorldObjectGetDistance2d);
    SetMethod(_state, "GetExactDistance", &WorldObjectGetExactDistance);
    SetMethod(_state, "GetExactDistance2d", &WorldObjectGetExactDistance2d);
    SetMethod(_state, "GetExactDistance2D", &WorldObjectGetExactDistance2d);
    SetMethod(_state, "GetRelativePoint", &WorldObjectGetRelativePoint);
    SetMethod(_state, "GetAngle", &WorldObjectGetAngle);
    SetMethod(_state, "IsWithinDist", &WorldObjectIsWithinDist);
    SetMethod(_state, "IsWithinDist3d", &WorldObjectIsWithinDist3d);
    SetMethod(_state, "IsWithinDist3D", &WorldObjectIsWithinDist3d);
    SetMethod(_state, "IsWithinDist2d", &WorldObjectIsWithinDist2d);
    SetMethod(_state, "IsWithinDist2D", &WorldObjectIsWithinDist2d);
    SetMethod(_state, "IsWithinDistInMap", &WorldObjectIsWithinDistInMap);
    SetMethod(_state, "IsInMap", &WorldObjectIsInMap);
    SetMethod(_state, "IsInRange", &WorldObjectIsInRange);
    SetMethod(_state, "IsInRange2d", &WorldObjectIsInRange2d);
    SetMethod(_state, "IsInRange2D", &WorldObjectIsInRange2d);
    SetMethod(_state, "IsInRange3d", &WorldObjectIsInRange3d);
    SetMethod(_state, "IsInRange3D", &WorldObjectIsInRange3d);
    SetMethod(_state, "IsInFront", &WorldObjectIsInFront);
    SetMethod(_state, "IsInBack", &WorldObjectIsInBack);
    SetMethod(_state, "IsWithinLOS", &WorldObjectIsWithinLOS);
    SetMethod(_state, "IsInLineOfSight", &WorldObjectIsWithinLOS);
    SetMethod(_state, "IsFriendlyTo", &WorldObjectIsFriendlyTo);
    SetMethod(_state, "IsHostileTo", &WorldObjectIsHostileTo);
    SetMethod(_state, "GetPlayersInRange", &WorldObjectGetPlayersInRange);
    SetMethod(_state, "GetCreaturesInRange", &WorldObjectGetCreaturesInRange);
    SetMethod(_state, "GetGameObjectsInRange", &WorldObjectGetGameObjectsInRange);
    SetMethod(_state, "GetNearestPlayer", &WorldObjectGetNearestPlayer);
    SetMethod(_state, "GetNearestCreature", &WorldObjectGetNearestCreature);
    SetMethod(_state, "GetNearestGameObject", &WorldObjectGetNearestGameObject);
    SetMethod(_state, "FindNearestPlayer", &WorldObjectFindNearestPlayer);
    SetMethod(_state, "FindNearestCreature", &WorldObjectFindNearestCreature);
    SetMethod(_state, "FindNearestGameObject", &WorldObjectFindNearestGameObject);
    SetMethod(_state, "GetLevel", &UnitGetLevel);
    SetMethod(_state, "IsAlive", &UnitIsAlive);
    SetMethod(_state, "IsDead", &UnitIsDead);
    SetMethod(_state, "IsDying", &UnitIsDying);
    SetMethod(_state, "IsMounted", &UnitIsMounted);
    SetMethod(_state, "IsRooted", &UnitIsRooted);
    SetMethod(_state, "IsFeared", &UnitIsFeared);
    SetMethod(_state, "IsConfused", &UnitIsConfused);
    SetMethod(_state, "IsPolymorphed", &UnitIsPolymorphed);
    SetMethod(_state, "IsStopped", &UnitIsStopped);
    SetMethod(_state, "IsTaxiFlying", &UnitIsTaxiFlying);
    SetMethod(_state, "IsCharmed", &UnitIsCharmed);
    SetMethod(_state, "IsAttackingPlayer", &UnitIsAttackingPlayer);
    SetMethod(_state, "GetStandState", &UnitGetStandState);
    SetMethod(_state, "SetStandState", &UnitSetStandState);
    SetMethod(_state, "GetHealth", &UnitGetHealth);
    SetMethod(_state, "GetMaxHealth", &UnitGetMaxHealth);
    SetMethod(_state, "SetHealth", &UnitSetHealth);
    SetMethod(_state, "GetHealthPct", &UnitGetHealthPct);
    SetMethod(_state, "GetHealthPercent", &UnitGetHealthPct);
    SetMethod(_state, "IsFullHealth", &UnitIsFullHealth);
    SetMethod(_state, "HealthAbovePct", &UnitHealthAbovePct);
    SetMethod(_state, "HealthBelowPct", &UnitHealthBelowPct);
    SetMethod(_state, "CountPctFromCurHealth", &UnitCountPctFromCurHealth);
    SetMethod(_state, "CountPctFromMaxHealth", &UnitCountPctFromMaxHealth);
    SetMethod(_state, "SetMaxHealth", &UnitSetMaxHealth);
    SetMethod(_state, "SetLevel", &UnitSetLevel);
    SetMethod(_state, "GetPower", &UnitGetPower);
    SetMethod(_state, "GetMaxPower", &UnitGetMaxPower);
    SetMethod(_state, "SetPower", &UnitSetPower);
    SetMethod(_state, "SetMaxPower", &UnitSetMaxPower);
    SetMethod(_state, "ModifyPower", &UnitModifyPower);
    SetMethod(_state, "GetPowerType", &UnitGetPowerType);
    SetMethod(_state, "SetPowerType", &UnitSetPowerType);
    SetMethod(_state, "GetPowerPct", &UnitGetPowerPct);
    SetMethod(_state, "GetRace", &UnitGetRace);
    SetMethod(_state, "GetClass", &UnitGetClass);
    SetMethod(_state, "GetRaceMask", &UnitGetRaceMask);
    SetMethod(_state, "GetClassMask", &UnitGetClassMask);
    SetMethod(_state, "GetRaceAsString", &UnitGetRaceAsString);
    SetMethod(_state, "GetClassAsString", &UnitGetClassAsString);
    SetMethod(_state, "GetCreatureType", &UnitGetCreatureType);
    SetMethod(_state, "GetStat", &UnitGetStat);
    SetMethod(_state, "GetBaseSpellPower", &UnitGetBaseSpellPower);
    SetMethod(_state, "GetGender", &UnitGetGender);
    SetMethod(_state, "GetUnitState", &UnitGetUnitState);
    SetMethod(_state, "HasUnitState", &UnitHasUnitState);
    SetMethod(_state, "AddUnitState", &UnitAddUnitState);
    SetMethod(_state, "ClearUnitState", &UnitClearUnitState);
    SetMethod(_state, "GetSpeed", &UnitGetSpeed);
    SetMethod(_state, "GetSpeedRate", &UnitGetSpeedRate);
    SetMethod(_state, "SetSpeed", &UnitSetSpeed);
    SetMethod(_state, "SetSpeedRate", &UnitSetSpeedRate);
    SetMethod(_state, "GetMovementType", &UnitGetMovementType);
    SetMethod(_state, "MoveStop", &UnitMoveStop);
    SetMethod(_state, "MoveExpire", &UnitMoveExpire);
    SetMethod(_state, "MoveClear", &UnitMoveClear);
    SetMethod(_state, "MoveIdle", &UnitMoveIdle);
    SetMethod(_state, "MoveRandom", &UnitMoveRandom);
    SetMethod(_state, "MoveHome", &UnitMoveHome);
    SetMethod(_state, "MoveFollow", &UnitMoveFollow);
    SetMethod(_state, "MoveChase", &UnitMoveChase);
    SetMethod(_state, "MoveConfused", &UnitMoveConfused);
    SetMethod(_state, "MoveFleeing", &UnitMoveFleeing);
    SetMethod(_state, "MoveTo", &UnitMoveTo);
    SetMethod(_state, "MoveJump", &UnitMoveJump);
    SetMethod(_state, "NearTeleport", &UnitNearTeleport);
    SetMethod(_state, "GetAttackers", &UnitGetAttackers);
    SetMethod(_state, "ClearThreatList", &UnitClearThreatList);
    SetMethod(_state, "GetThreatList", &UnitGetThreatList);
    SetMethod(_state, "AddThreat", &UnitAddThreat);
    SetMethod(_state, "ModifyThreatPct", &UnitModifyThreatPct);
    SetMethod(_state, "ModifyThreatPercent", &UnitModifyThreatPct);
    SetMethod(_state, "ClearThreat", &UnitClearThreat);
    SetMethod(_state, "ResetAllThreat", &UnitResetAllThreat);
    SetMethod(_state, "GetThreat", &UnitGetThreat);
    SetMethod(_state, "GetFriendlyUnitsInRange", &UnitGetFriendlyUnitsInRange);
    SetMethod(_state, "GetUnfriendlyUnitsInRange", &UnitGetUnfriendlyUnitsInRange);
    SetMethod(_state, "GetCurrentSpell", &UnitGetCurrentSpell);
    SetMethod(_state, "HandleStatModifier", &UnitHandleStatModifier);
    SetMethod(_state, "IsInAccessiblePlaceFor", &UnitIsInAccessiblePlaceFor);
    SetMethod(_state, "IsOnVehicle", &UnitIsOnVehicle);
    SetMethod(_state, "GetVehicle", &UnitGetVehicle);
    SetMethod(_state, "GetVehicleKit", &UnitGetVehicleKit);
    SetMethod(_state, "HasAura", &UnitHasAura);
    SetMethod(_state, "GetAura", &UnitGetAura);
    SetMethod(_state, "AddAura", &UnitAddAura);
    SetMethod(_state, "RemoveAura", &UnitRemoveAura);
    SetMethod(_state, "RemoveAurasDueToSpell", &UnitRemoveAura);
    SetMethod(_state, "RemoveAllAuras", &UnitRemoveAllAuras);
    SetMethod(_state, "RemoveArenaAuras", &UnitRemoveArenaAuras);
    SetMethod(_state, "GetVictim", &UnitGetVictim);
    SetMethod(_state, "Attack", &UnitAttack);
    SetMethod(_state, "AttackStop", &UnitAttackStop);
    SetMethod(_state, "Kill", &UnitKill);
    SetMethod(_state, "DealDamage", &UnitDealDamage);
    SetMethod(_state, "DealHeal", &UnitDealHeal);
    SetMethod(_state, "IsInCombat", &UnitIsInCombat);
    SetMethod(_state, "ClearInCombat", &UnitClearInCombat);
    SetMethod(_state, "SetInCombatWith", &UnitSetInCombatWith);
    SetMethod(_state, "SetOwnerGUID", &UnitSetOwnerGUID);
    SetMethod(_state, "SetOwnerGuid", &UnitSetOwnerGUID);
    SetMethod(_state, "SetCreatorGUID", &UnitSetCreatorGUID);
    SetMethod(_state, "SetCreatorGuid", &UnitSetCreatorGUID);
    SetMethod(_state, "SetPetGUID", &UnitSetPetGUID);
    SetMethod(_state, "SetPetGuid", &UnitSetPetGUID);
    SetMethod(_state, "SetCritterGUID", &UnitSetCritterGUID);
    SetMethod(_state, "SetCritterGuid", &UnitSetCritterGUID);
    SetMethod(_state, "SetName", &UnitSetName);
    SetMethod(_state, "SetImmuneTo", &UnitSetImmuneTo);
    SetMethod(_state, "IsInWater", &UnitIsInWater);
    SetMethod(_state, "IsUnderWater", &UnitIsUnderWater);
    SetMethod(_state, "IsUnderwater", &UnitIsUnderWater);
    SetMethod(_state, "IsMoving", &UnitIsMoving);
    SetMethod(_state, "IsFlying", &UnitIsFlying);
    SetMethod(_state, "IsCasting", &UnitIsCasting);
    SetMethod(_state, "IsPvPFlagged", &UnitIsPvPFlagged);
    SetMethod(_state, "IsStandState", &UnitIsStandState);
    SetMethod(_state, "IsVendor", &UnitIsVendor);
    SetMethod(_state, "IsTrainer", &UnitIsTrainer);
    SetMethod(_state, "IsQuestGiver", &UnitIsQuestGiver);
    SetMethod(_state, "IsGossip", &UnitIsGossip);
    SetMethod(_state, "IsTaxi", &UnitIsTaxi);
    SetMethod(_state, "IsGuildMaster", &UnitIsGuildMaster);
    SetMethod(_state, "IsBattleMaster", &UnitIsBattleMaster);
    SetMethod(_state, "IsBanker", &UnitIsBanker);
    SetMethod(_state, "IsInnkeeper", &UnitIsInnkeeper);
    SetMethod(_state, "IsSpiritHealer", &UnitIsSpiritHealer);
    SetMethod(_state, "IsSpiritGuide", &UnitIsSpiritGuide);
    SetMethod(_state, "IsTabardDesigner", &UnitIsTabardDesigner);
    SetMethod(_state, "IsAuctioneer", &UnitIsAuctioneer);
    SetMethod(_state, "IsArmorer", &UnitIsArmorer);
    SetMethod(_state, "IsServiceProvider", &UnitIsServiceProvider);
    SetMethod(_state, "IsSpiritService", &UnitIsSpiritService);
    SetMethod(_state, "GetDisplayId", &UnitGetDisplayId);
    SetMethod(_state, "SetDisplayId", &UnitSetDisplayId);
    SetMethod(_state, "GetNativeDisplayId", &UnitGetNativeDisplayId);
    SetMethod(_state, "SetNativeDisplayId", &UnitSetNativeDisplayId);
    SetMethod(_state, "RestoreDisplayId", &UnitRestoreDisplayId);
    SetMethod(_state, "DeMorph", &UnitDeMorph);
    SetMethod(_state, "GetMountId", &UnitGetMountId);
    SetMethod(_state, "GetMountID", &UnitGetMountId);
    SetMethod(_state, "Mount", &UnitMount);
    SetMethod(_state, "Dismount", &UnitDismount);
    SetMethod(_state, "Unmount", &UnitDismount);
    SetMethod(_state, "GetFaction", &UnitGetFaction);
    SetMethod(_state, "SetFaction", &UnitSetFaction);
    SetMethod(_state, "RestoreFaction", &UnitRestoreFaction);
    SetMethod(_state, "SetSheath", &UnitSetSheath);
    SetMethod(_state, "SetRooted", &UnitSetRooted);
    SetMethod(_state, "SetConfused", &UnitSetConfused);
    SetMethod(_state, "SetFeared", &UnitSetFeared);
    SetMethod(_state, "SetFacing", &UnitSetFacing);
    SetMethod(_state, "SetFacingToObject", &UnitSetFacingToObject);
    SetMethod(_state, "SetPvP", &UnitSetPvP);
    SetMethod(_state, "SetFFA", &UnitSetFFA);
    SetMethod(_state, "SetSanctuary", &UnitSetSanctuary);
    SetMethod(_state, "SetWaterWalk", &UnitSetWaterWalk);
    SetMethod(_state, "StopSpellCast", &UnitStopSpellCast);
    SetMethod(_state, "InterruptSpell", &UnitInterruptSpell);
    SetMethod(_state, "PerformEmote", &UnitPerformEmote);
    SetMethod(_state, "EmoteState", &UnitEmoteState);
    SetMethod(_state, "GetOwner", &UnitGetOwner);
    SetMethod(_state, "GetOwnerGUID", &UnitGetOwnerGUID);
    SetMethod(_state, "GetOwnerGuid", &UnitGetOwnerGUID);
    SetMethod(_state, "GetCreatorGUID", &UnitGetCreatorGUID);
    SetMethod(_state, "GetCreatorGuid", &UnitGetCreatorGUID);
    SetMethod(_state, "GetMinionGUID", &UnitGetPetGUID);
    SetMethod(_state, "GetMinionGuid", &UnitGetPetGUID);
    SetMethod(_state, "GetPetGUID", &UnitGetPetGUID);
    SetMethod(_state, "GetPetGuid", &UnitGetPetGUID);
    SetMethod(_state, "GetCritterGUID", &UnitGetCritterGUID);
    SetMethod(_state, "GetCritterGuid", &UnitGetCritterGUID);
    SetMethod(_state, "GetControllerGUID", &UnitGetControllerGUID);
    SetMethod(_state, "GetControllerGuid", &UnitGetControllerGUID);
    SetMethod(_state, "GetControllerGUIDS", &UnitGetControllerGUIDS);
    SetMethod(_state, "GetControllerGuids", &UnitGetControllerGUIDS);
    SetMethod(_state, "GetCharmer", &UnitGetCharmer);
    SetMethod(_state, "GetCharmerGUID", &UnitGetCharmerGUID);
    SetMethod(_state, "GetCharmerGuid", &UnitGetCharmerGUID);
    SetMethod(_state, "GetCharm", &UnitGetCharm);
    SetMethod(_state, "GetCharmGUID", &UnitGetCharmGUID);
    SetMethod(_state, "GetCharmGuid", &UnitGetCharmGUID);
    SetMethod(_state, "GetCharmerOrOwner", &UnitGetCharmerOrOwner);
    SetMethod(_state, "GetCharmerOrOwnerGUID", &UnitGetCharmerOrOwnerGUID);
    SetMethod(_state, "GetCharmerOrOwnerGuid", &UnitGetCharmerOrOwnerGUID);
    SetMethod(_state, "Say", &CreatureSay);
    SetMethod(_state, "Yell", &CreatureYell);
    SetMethod(_state, "Whisper", &CreatureWhisper);
    SetMethod(_state, "SendUnitSay", &UnitSendUnitSay);
    SetMethod(_state, "SendUnitYell", &UnitSendUnitYell);
    SetMethod(_state, "SendUnitWhisper", &UnitSendUnitWhisper);
    SetMethod(_state, "TextEmote", &CreatureTextEmote);
    SetMethod(_state, "SendUnitEmote", &UnitSendUnitEmote);
    SetMethod(_state, "SendChatMessageToPlayer", &UnitSendChatMessageToPlayer);
    SetMethod(_state, "CastSpell", &UnitCastSpell);
    SetMethod(_state, "CastCustomSpell", &UnitCastCustomSpell);
    SetMethod(_state, "CastSpellAoF", &UnitCastSpellAoF);
    SetMethod(_state, "DespawnOrUnsummon", &CreatureDespawnOrUnsummon);
    SetMethod(_state, "ForcedDespawn", &CreatureDespawnOrUnsummon);
    SetMethod(_state, "GetRespawnDelay", &CreatureGetRespawnDelay);
    SetMethod(_state, "SetRespawnDelay", &CreatureSetRespawnDelay);
    SetMethod(_state, "IsRegeneratingHealth", &CreatureIsRegeneratingHealth);
    SetMethod(_state, "SetRegeneratingHealth", &CreatureSetRegeneratingHealth);
    SetMethod(_state, "IsReputationGainDisabled", &CreatureCompatReturnFalse);
    SetMethod(_state, "SetDisableReputationGain", &CreatureCompatNoop);
    SetMethod(_state, "CanCompleteQuest", &CreatureCanCompleteQuest);
    SetMethod(_state, "IsTargetableForAttack", &CreatureIsTargetableForAttack);
    SetMethod(_state, "CanAssistTo", &CreatureCanAssistTo);
    SetMethod(_state, "HasSearchedAssistance", &CreatureHasSearchedAssistance);
    SetMethod(_state, "IsTappedBy", &CreatureIsTappedBy);
    SetMethod(_state, "HasLootRecipient", &CreatureHasLootRecipient);
    SetMethod(_state, "CanAggro", &CreatureCanAggro);
    SetMethod(_state, "CanSwim", &CreatureCanSwim);
    SetMethod(_state, "CanWalk", &CreatureCanWalk);
    SetMethod(_state, "CanFly", &CreatureCanFly);
    SetMethod(_state, "IsInEvadeMode", &CreatureIsInEvadeMode);
    SetMethod(_state, "IsElite", &CreatureIsElite);
    SetMethod(_state, "IsGuard", &CreatureIsGuard);
    SetMethod(_state, "IsCivilian", &CreatureIsCivilian);
    SetMethod(_state, "IsRacialLeader", &CreatureIsRacialLeader);
    SetMethod(_state, "IsTrigger", &CreatureIsTrigger);
    SetMethod(_state, "IsDamageEnoughForLootingAndReward", &CreatureCompatReturnFalse);
    SetMethod(_state, "CanStartAttack", &CreatureCanStartAttack);
    SetMethod(_state, "HasLootMode", &CreatureHasLootMode);
    SetMethod(_state, "IsDungeonBoss", &CreatureIsDungeonBoss);
    SetMethod(_state, "IsWorldBoss", &CreatureIsWorldBoss);
    SetMethod(_state, "HasCategoryCooldown", &CreatureHasCategoryCooldown);
    SetMethod(_state, "HasSpell", &CreatureHasSpell);
    SetMethod(_state, "HasQuest", &CreatureHasQuest);
    SetMethod(_state, "HasSpellCooldown", &CreatureHasSpellCooldown);
    SetMethod(_state, "GetWaypointPath", &CreatureCompatReturnZero);
    SetMethod(_state, "GetCurrentWaypointId", &CreatureGetCurrentWaypointId);
    SetMethod(_state, "GetDefaultMovementType", &CreatureGetDefaultMovementType);
    SetMethod(_state, "GetAggroRange", &CreatureGetAggroRange);
    SetMethod(_state, "GetLootRecipientGroup", &CreatureGetLootRecipientGroup);
    SetMethod(_state, "GetLootRecipient", &CreatureGetLootRecipient);
    SetMethod(_state, "GetScriptName", &CreatureGetScriptName);
    SetMethod(_state, "GetAIName", &CreatureGetAIName);
    SetMethod(_state, "GetScriptId", &CreatureGetScriptId);
    SetMethod(_state, "GetCreatureSpellCooldownDelay", &CreatureGetCreatureSpellCooldownDelay);
    SetMethod(_state, "GetCorpseDelay", &CreatureCompatReturnZero);
    SetMethod(_state, "GetHomePosition", &CreatureGetHomePosition);
    SetMethod(_state, "GetAITarget", &CreatureGetAITarget);
    SetMethod(_state, "GetAITargets", &CreatureGetAITargets);
    SetMethod(_state, "GetAITargetsCount", &CreatureGetAITargetsCount);
    SetMethod(_state, "GetNPCFlags", &CreatureGetNPCFlags);
    SetMethod(_state, "GetUnitFlags", &CreatureGetUnitFlags);
    SetMethod(_state, "GetUnitFlagsTwo", &CreatureCompatReturnZero);
    SetMethod(_state, "GetExtraFlags", &CreatureGetExtraFlags);
    SetMethod(_state, "GetRank", &CreatureGetRank);
    SetMethod(_state, "GetShieldBlockValue", &CreatureGetShieldBlockValue);
    SetMethod(_state, "GetLootMode", &CreatureGetLootMode);
    SetMethod(_state, "SetNPCFlags", &CreatureSetNPCFlags);
    SetMethod(_state, "SetUnitFlags", &CreatureSetUnitFlags);
    SetMethod(_state, "SetUnitFlagsTwo", &CreatureCompatNoop);
    SetMethod(_state, "SetReactState", &CreatureSetReactState);
    SetMethod(_state, "SetDisableGravity", &CreatureCompatNoop);
    SetMethod(_state, "SetLootMode", &CreatureCompatNoop);
    SetMethod(_state, "SetDeathState", &CreatureSetDeathState);
    SetMethod(_state, "SetWalk", &CreatureSetWalk);
    SetMethod(_state, "SetEquipmentSlots", &CreatureSetEquipmentSlots);
    SetMethod(_state, "SetAggroEnabled", &CreatureSetAggroEnabled);
    SetMethod(_state, "SetHomePosition", &CreatureSetHomePosition);
    SetMethod(_state, "SetWanderRadius", &CreatureSetWanderRadius);
    SetMethod(_state, "SetDefaultMovementType", &CreatureSetDefaultMovementType);
    SetMethod(_state, "SetNoSearchAssistance", &CreatureSetNoSearchAssistance);
    SetMethod(_state, "SetNoCallAssistance", &CreatureSetNoCallAssistance);
    SetMethod(_state, "SetHover", &CreatureSetHover);
    SetMethod(_state, "GetWanderRadius", &CreatureGetWanderRadius);
    SetMethod(_state, "Respawn", &CreatureRespawn);
    SetMethod(_state, "RemoveCorpse", &CreatureRemoveCorpse);
    SetMethod(_state, "MoveWaypoint", &CreatureMoveWaypoint);
    SetMethod(_state, "CallAssistance", &CreatureCallAssistance);
    SetMethod(_state, "CallForHelp", &CreatureCallForHelp);
    SetMethod(_state, "FleeToGetAssistance", &CreatureFleeToGetAssistance);
    SetMethod(_state, "AttackStart", &CreatureAttackStart);
    SetMethod(_state, "SaveToDB", &CreatureSaveToDB);
    SetMethod(_state, "SelectVictim", &CreatureSelectVictim);
    SetMethod(_state, "UpdateEntry", &CreatureUpdateEntry);
    SetMethod(_state, "ResetLootMode", &CreatureCompatNoop);
    SetMethod(_state, "RemoveLootMode", &CreatureCompatNoop);
    SetMethod(_state, "AddLootMode", &CreatureCompatNoop);
    SetMethod(_state, "GetCreatureFamily", &CreatureGetCreatureFamily);
    SetMethod(_state, "SetInCombatWithZone", &CreatureSetInCombatWithZone);
    SetMethod(_state, "SummonCreature", &WorldObjectSummonCreature);
    SetMethod(_state, "SpawnCreature", &WorldObjectSummonCreature);
    SetMethod(_state, "SummonGameObject", &WorldObjectSummonGameObject);
    SetMethod(_state, "PlayMusic", &WorldObjectPlayMusic);
    SetMethod(_state, "PlayDirectSound", &WorldObjectPlayDirectSound);
    SetMethod(_state, "PlayDistanceSound", &WorldObjectPlayDistanceSound);
    lua_setfield(_state, -2, "__index");

    lua_pop(_state, 1);
}

void TurtleLuaEngine::RegisterGameObjectMetatable()
{
    luaL_newmetatable(_state, GAMEOBJECT_METATABLE);

    lua_newtable(_state);
    SetMethod(_state, "GetName", &GameObjectGetName);
    SetMethod(_state, "GetEntry", &GameObjectGetEntry);
    SetMethod(_state, "GetTemplate", &GameObjectGetTemplate);
    SetMethod(_state, "GetGameObjectTemplate", &GameObjectGetTemplate);
    SetMethod(_state, "GetGameObjectInfo", &GameObjectGetTemplate);
    SetMethod(_state, "GetGOTemplate", &GameObjectGetTemplate);
    SetMethod(_state, "GetGOInfo", &GameObjectGetTemplate);
    SetMethod(_state, "GetGUIDLow", &GameObjectGetGUIDLow);
    SetMethod(_state, "GetGUID", &ObjectGetGUID);
    SetMethod(_state, "GetGuid", &ObjectGetGUID);
    SetMethod(_state, "GetObjectGuid", &ObjectGetGUID);
    SetMethod(_state, "GetDBTableGUIDLow", &GameObjectGetDBTableGUIDLow);
    SetMethod(_state, "GetTypeId", &ObjectGetTypeId);
    SetMethod(_state, "GetTypeID", &ObjectGetTypeId);
    SetMethod(_state, "IsPlayer", &ObjectIsPlayer);
    SetMethod(_state, "IsCreature", &ObjectIsCreature);
    SetMethod(_state, "IsUnit", &ObjectIsUnit);
    SetMethod(_state, "IsGameObject", &ObjectIsGameObject);
    SetMethod(_state, "IsItem", &ObjectIsItem);
    SetMethod(_state, "IsWorldObject", &ObjectIsWorldObject);
    SetMethod(_state, "GetScale", &ObjectGetScale);
    SetMethod(_state, "SetScale", &ObjectSetScale);
    SetMethod(_state, "IsInWorld", &ObjectIsInWorld);
    SetMethod(_state, "GetUInt32Value", &ObjectGetUInt32Value);
    SetMethod(_state, "SetUInt32Value", &ObjectSetUInt32Value);
    SetMethod(_state, "HasFlag", &ObjectHasFlag);
    SetMethod(_state, "SetFlag", &ObjectSetFlag);
    SetMethod(_state, "RemoveFlag", &ObjectRemoveFlag);
    SetObjectCompatMethods(_state);
    SetWorldObjectCompatMethods(_state);
    SetMethod(_state, "GetMapId", &WorldObjectGetMapId);
    SetMethod(_state, "GetMapID", &WorldObjectGetMapId);
    SetMethod(_state, "GetMap", &WorldObjectGetMap);
    SetMethod(_state, "GetPhaseMask", &WorldObjectGetPhaseMask);
    SetMethod(_state, "SetPhaseMask", &WorldObjectSetPhaseMask);
    SetMethod(_state, "GetInstanceId", &WorldObjectGetInstanceId);
    SetMethod(_state, "GetInstanceID", &WorldObjectGetInstanceId);
    SetMethod(_state, "GetZoneId", &WorldObjectGetZoneId);
    SetMethod(_state, "GetZoneID", &WorldObjectGetZoneId);
    SetMethod(_state, "GetAreaId", &WorldObjectGetAreaId);
    SetMethod(_state, "GetAreaID", &WorldObjectGetAreaId);
    SetMethod(_state, "GetX", &WorldObjectGetX);
    SetMethod(_state, "GetY", &WorldObjectGetY);
    SetMethod(_state, "GetZ", &WorldObjectGetZ);
    SetMethod(_state, "GetO", &WorldObjectGetO);
    SetMethod(_state, "GetLocation", &WorldObjectGetLocation);
    SetMethod(_state, "GetDistance", &WorldObjectGetDistance);
    SetMethod(_state, "GetDistance2d", &WorldObjectGetDistance2d);
    SetMethod(_state, "GetDistance2D", &WorldObjectGetDistance2d);
    SetMethod(_state, "GetExactDistance", &WorldObjectGetExactDistance);
    SetMethod(_state, "GetExactDistance2d", &WorldObjectGetExactDistance2d);
    SetMethod(_state, "GetExactDistance2D", &WorldObjectGetExactDistance2d);
    SetMethod(_state, "GetRelativePoint", &WorldObjectGetRelativePoint);
    SetMethod(_state, "GetAngle", &WorldObjectGetAngle);
    SetMethod(_state, "IsWithinDist", &WorldObjectIsWithinDist);
    SetMethod(_state, "IsWithinDist3d", &WorldObjectIsWithinDist3d);
    SetMethod(_state, "IsWithinDist3D", &WorldObjectIsWithinDist3d);
    SetMethod(_state, "IsWithinDist2d", &WorldObjectIsWithinDist2d);
    SetMethod(_state, "IsWithinDist2D", &WorldObjectIsWithinDist2d);
    SetMethod(_state, "IsWithinDistInMap", &WorldObjectIsWithinDistInMap);
    SetMethod(_state, "IsInMap", &WorldObjectIsInMap);
    SetMethod(_state, "IsInRange", &WorldObjectIsInRange);
    SetMethod(_state, "IsInRange2d", &WorldObjectIsInRange2d);
    SetMethod(_state, "IsInRange2D", &WorldObjectIsInRange2d);
    SetMethod(_state, "IsInRange3d", &WorldObjectIsInRange3d);
    SetMethod(_state, "IsInRange3D", &WorldObjectIsInRange3d);
    SetMethod(_state, "IsInFront", &WorldObjectIsInFront);
    SetMethod(_state, "IsInBack", &WorldObjectIsInBack);
    SetMethod(_state, "IsWithinLOS", &WorldObjectIsWithinLOS);
    SetMethod(_state, "IsInLineOfSight", &WorldObjectIsWithinLOS);
    SetMethod(_state, "IsFriendlyTo", &WorldObjectIsFriendlyTo);
    SetMethod(_state, "IsHostileTo", &WorldObjectIsHostileTo);
    SetMethod(_state, "GetPlayersInRange", &WorldObjectGetPlayersInRange);
    SetMethod(_state, "GetCreaturesInRange", &WorldObjectGetCreaturesInRange);
    SetMethod(_state, "GetGameObjectsInRange", &WorldObjectGetGameObjectsInRange);
    SetMethod(_state, "GetNearestPlayer", &WorldObjectGetNearestPlayer);
    SetMethod(_state, "GetNearestCreature", &WorldObjectGetNearestCreature);
    SetMethod(_state, "GetNearestGameObject", &WorldObjectGetNearestGameObject);
    SetMethod(_state, "FindNearestPlayer", &WorldObjectFindNearestPlayer);
    SetMethod(_state, "FindNearestCreature", &WorldObjectFindNearestCreature);
    SetMethod(_state, "FindNearestGameObject", &WorldObjectFindNearestGameObject);
    SetMethod(_state, "SummonCreature", &WorldObjectSummonCreature);
    SetMethod(_state, "SpawnCreature", &WorldObjectSummonCreature);
    SetMethod(_state, "SummonGameObject", &WorldObjectSummonGameObject);
    SetMethod(_state, "PlayMusic", &WorldObjectPlayMusic);
    SetMethod(_state, "PlayDirectSound", &WorldObjectPlayDirectSound);
    SetMethod(_state, "PlayDistanceSound", &WorldObjectPlayDistanceSound);
    SetMethod(_state, "GetGoType", &GameObjectGetGoType);
    SetMethod(_state, "GetGoState", &GameObjectGetGoState);
    SetMethod(_state, "SetGoState", &GameObjectSetGoState);
    SetMethod(_state, "GetLootState", &GameObjectGetLootState);
    SetMethod(_state, "GetLootRecipient", &GameObjectGetLootRecipient);
    SetMethod(_state, "GetLootRecipientGroup", &GameObjectGetLootRecipientGroup);
    SetMethod(_state, "SetLootState", &GameObjectSetLootState);
    SetMethod(_state, "AddLoot", &GameObjectAddLoot);
    SetMethod(_state, "GetDisplayId", &GameObjectGetDisplayId);
    SetMethod(_state, "SetDisplayId", &GameObjectSetDisplayId);
    SetMethod(_state, "GetRespawnDelay", &GameObjectGetRespawnDelay);
    SetMethod(_state, "SetRespawnDelay", &GameObjectSetRespawnDelay);
    SetMethod(_state, "SetRespawnTime", &GameObjectSetRespawnTime);
    SetMethod(_state, "IsSpawned", &GameObjectIsSpawned);
    SetMethod(_state, "HasQuest", &GameObjectHasQuest);
    SetMethod(_state, "IsTransport", &GameObjectIsTransport);
    SetMethod(_state, "IsActive", &GameObjectIsActive);
    SetMethod(_state, "IsDestructible", &GameObjectIsDestructible);
    SetMethod(_state, "IsVisible", &GameObjectIsVisible);
    SetMethod(_state, "SetVisible", &GameObjectSetVisible);
    SetMethod(_state, "GetOwner", &GameObjectGetOwner);
    SetMethod(_state, "Use", &GameObjectUse);
    SetMethod(_state, "UseDoorOrButton", &GameObjectUseDoorOrButton);
    SetMethod(_state, "Respawn", &GameObjectRespawn);
    SetMethod(_state, "Despawn", &GameObjectDespawn);
    SetMethod(_state, "Refresh", &GameObjectRefresh);
    SetMethod(_state, "SaveToDB", &GameObjectSaveToDB);
    SetMethod(_state, "RemoveFromWorld", &GameObjectRemoveFromWorld);
    lua_setfield(_state, -2, "__index");

    lua_pop(_state, 1);
}

void TurtleLuaEngine::RegisterCorpseMetatable()
{
    luaL_newmetatable(_state, CORPSE_METATABLE);

    lua_newtable(_state);
    SetMethod(_state, "GetName", &CorpseGetName);
    SetMethod(_state, "GetEntry", &ObjectGetEntry);
    SetMethod(_state, "GetGUIDLow", &ObjectGetGUIDLow);
    SetMethod(_state, "GetGUID", &ObjectGetGUID);
    SetMethod(_state, "GetGuid", &ObjectGetGUID);
    SetMethod(_state, "GetObjectGuid", &ObjectGetGUID);
    SetMethod(_state, "GetTypeId", &ObjectGetTypeId);
    SetMethod(_state, "GetTypeID", &ObjectGetTypeId);
    SetMethod(_state, "IsPlayer", &ObjectIsPlayer);
    SetMethod(_state, "IsCreature", &ObjectIsCreature);
    SetMethod(_state, "IsUnit", &ObjectIsUnit);
    SetMethod(_state, "IsGameObject", &ObjectIsGameObject);
    SetMethod(_state, "IsItem", &ObjectIsItem);
    SetMethod(_state, "IsCorpse", &ObjectIsCorpse);
    SetMethod(_state, "IsWorldObject", &ObjectIsWorldObject);
    SetMethod(_state, "GetScale", &ObjectGetScale);
    SetMethod(_state, "SetScale", &ObjectSetScale);
    SetMethod(_state, "IsInWorld", &ObjectIsInWorld);
    SetMethod(_state, "GetUInt32Value", &ObjectGetUInt32Value);
    SetMethod(_state, "SetUInt32Value", &ObjectSetUInt32Value);
    SetMethod(_state, "HasFlag", &ObjectHasFlag);
    SetMethod(_state, "SetFlag", &ObjectSetFlag);
    SetMethod(_state, "RemoveFlag", &ObjectRemoveFlag);
    SetObjectCompatMethods(_state);
    SetWorldObjectCompatMethods(_state);
    SetMethod(_state, "GetMapId", &WorldObjectGetMapId);
    SetMethod(_state, "GetMapID", &WorldObjectGetMapId);
    SetMethod(_state, "GetMap", &WorldObjectGetMap);
    SetMethod(_state, "GetPhaseMask", &WorldObjectGetPhaseMask);
    SetMethod(_state, "SetPhaseMask", &WorldObjectSetPhaseMask);
    SetMethod(_state, "GetInstanceId", &WorldObjectGetInstanceId);
    SetMethod(_state, "GetInstanceID", &WorldObjectGetInstanceId);
    SetMethod(_state, "GetZoneId", &WorldObjectGetZoneId);
    SetMethod(_state, "GetZoneID", &WorldObjectGetZoneId);
    SetMethod(_state, "GetAreaId", &WorldObjectGetAreaId);
    SetMethod(_state, "GetAreaID", &WorldObjectGetAreaId);
    SetMethod(_state, "GetX", &WorldObjectGetX);
    SetMethod(_state, "GetY", &WorldObjectGetY);
    SetMethod(_state, "GetZ", &WorldObjectGetZ);
    SetMethod(_state, "GetO", &WorldObjectGetO);
    SetMethod(_state, "GetLocation", &WorldObjectGetLocation);
    SetMethod(_state, "GetDistance", &WorldObjectGetDistance);
    SetMethod(_state, "GetDistance2d", &WorldObjectGetDistance2d);
    SetMethod(_state, "GetDistance2D", &WorldObjectGetDistance2d);
    SetMethod(_state, "GetExactDistance", &WorldObjectGetExactDistance);
    SetMethod(_state, "GetExactDistance2d", &WorldObjectGetExactDistance2d);
    SetMethod(_state, "GetExactDistance2D", &WorldObjectGetExactDistance2d);
    SetMethod(_state, "GetRelativePoint", &WorldObjectGetRelativePoint);
    SetMethod(_state, "GetAngle", &WorldObjectGetAngle);
    SetMethod(_state, "IsWithinDist", &WorldObjectIsWithinDist);
    SetMethod(_state, "IsWithinDist3d", &WorldObjectIsWithinDist3d);
    SetMethod(_state, "IsWithinDist3D", &WorldObjectIsWithinDist3d);
    SetMethod(_state, "IsWithinDist2d", &WorldObjectIsWithinDist2d);
    SetMethod(_state, "IsWithinDist2D", &WorldObjectIsWithinDist2d);
    SetMethod(_state, "IsWithinDistInMap", &WorldObjectIsWithinDistInMap);
    SetMethod(_state, "IsInMap", &WorldObjectIsInMap);
    SetMethod(_state, "IsInRange", &WorldObjectIsInRange);
    SetMethod(_state, "IsInRange2d", &WorldObjectIsInRange2d);
    SetMethod(_state, "IsInRange2D", &WorldObjectIsInRange2d);
    SetMethod(_state, "IsInRange3d", &WorldObjectIsInRange3d);
    SetMethod(_state, "IsInRange3D", &WorldObjectIsInRange3d);
    SetMethod(_state, "IsInFront", &WorldObjectIsInFront);
    SetMethod(_state, "IsInBack", &WorldObjectIsInBack);
    SetMethod(_state, "IsWithinLOS", &WorldObjectIsWithinLOS);
    SetMethod(_state, "IsInLineOfSight", &WorldObjectIsWithinLOS);
    SetMethod(_state, "IsFriendlyTo", &WorldObjectIsFriendlyTo);
    SetMethod(_state, "IsHostileTo", &WorldObjectIsHostileTo);
    SetMethod(_state, "GetPlayersInRange", &WorldObjectGetPlayersInRange);
    SetMethod(_state, "GetCreaturesInRange", &WorldObjectGetCreaturesInRange);
    SetMethod(_state, "GetGameObjectsInRange", &WorldObjectGetGameObjectsInRange);
    SetMethod(_state, "GetNearestPlayer", &WorldObjectGetNearestPlayer);
    SetMethod(_state, "GetNearestCreature", &WorldObjectGetNearestCreature);
    SetMethod(_state, "GetNearestGameObject", &WorldObjectGetNearestGameObject);
    SetMethod(_state, "FindNearestPlayer", &WorldObjectFindNearestPlayer);
    SetMethod(_state, "FindNearestCreature", &WorldObjectFindNearestCreature);
    SetMethod(_state, "FindNearestGameObject", &WorldObjectFindNearestGameObject);
    SetMethod(_state, "GetOwnerGUID", &CorpseGetOwnerGUID);
    SetMethod(_state, "GetOwnerGuid", &CorpseGetOwnerGUID);
    SetMethod(_state, "GetGhostTime", &CorpseGetGhostTime);
    SetMethod(_state, "GetType", &CorpseGetType);
    SetMethod(_state, "ResetGhostTime", &CorpseResetGhostTime);
    SetMethod(_state, "SaveToDB", &CorpseSaveToDB);
    lua_setfield(_state, -2, "__index");

    lua_pop(_state, 1);
}

void TurtleLuaEngine::RegisterDynamicObjectMetatable()
{
    luaL_newmetatable(_state, DYNAMICOBJECT_METATABLE);

    lua_newtable(_state);
    SetMethod(_state, "GetName", &DynamicObjectGetName);
    SetMethod(_state, "GetEntry", &ObjectGetEntry);
    SetMethod(_state, "GetGUIDLow", &ObjectGetGUIDLow);
    SetMethod(_state, "GetGUID", &ObjectGetGUID);
    SetMethod(_state, "GetGuid", &ObjectGetGUID);
    SetMethod(_state, "GetObjectGuid", &ObjectGetGUID);
    SetMethod(_state, "GetTypeId", &ObjectGetTypeId);
    SetMethod(_state, "GetTypeID", &ObjectGetTypeId);
    SetMethod(_state, "IsPlayer", &ObjectIsPlayer);
    SetMethod(_state, "IsCreature", &ObjectIsCreature);
    SetMethod(_state, "IsUnit", &ObjectIsUnit);
    SetMethod(_state, "IsGameObject", &ObjectIsGameObject);
    SetMethod(_state, "IsItem", &ObjectIsItem);
    SetMethod(_state, "IsCorpse", &ObjectIsCorpse);
    SetMethod(_state, "IsDynamicObject", &ObjectIsDynamicObject);
    SetMethod(_state, "IsWorldObject", &ObjectIsWorldObject);
    SetMethod(_state, "GetScale", &ObjectGetScale);
    SetMethod(_state, "SetScale", &ObjectSetScale);
    SetMethod(_state, "IsInWorld", &ObjectIsInWorld);
    SetMethod(_state, "GetUInt32Value", &ObjectGetUInt32Value);
    SetMethod(_state, "SetUInt32Value", &ObjectSetUInt32Value);
    SetMethod(_state, "HasFlag", &ObjectHasFlag);
    SetMethod(_state, "SetFlag", &ObjectSetFlag);
    SetMethod(_state, "RemoveFlag", &ObjectRemoveFlag);
    SetObjectCompatMethods(_state);
    SetWorldObjectCompatMethods(_state);
    SetMethod(_state, "GetMapId", &WorldObjectGetMapId);
    SetMethod(_state, "GetMapID", &WorldObjectGetMapId);
    SetMethod(_state, "GetMap", &WorldObjectGetMap);
    SetMethod(_state, "GetPhaseMask", &WorldObjectGetPhaseMask);
    SetMethod(_state, "SetPhaseMask", &WorldObjectSetPhaseMask);
    SetMethod(_state, "GetInstanceId", &WorldObjectGetInstanceId);
    SetMethod(_state, "GetInstanceID", &WorldObjectGetInstanceId);
    SetMethod(_state, "GetZoneId", &WorldObjectGetZoneId);
    SetMethod(_state, "GetZoneID", &WorldObjectGetZoneId);
    SetMethod(_state, "GetAreaId", &WorldObjectGetAreaId);
    SetMethod(_state, "GetAreaID", &WorldObjectGetAreaId);
    SetMethod(_state, "GetX", &WorldObjectGetX);
    SetMethod(_state, "GetY", &WorldObjectGetY);
    SetMethod(_state, "GetZ", &WorldObjectGetZ);
    SetMethod(_state, "GetO", &WorldObjectGetO);
    SetMethod(_state, "GetLocation", &WorldObjectGetLocation);
    SetMethod(_state, "GetDistance", &WorldObjectGetDistance);
    SetMethod(_state, "GetDistance2d", &WorldObjectGetDistance2d);
    SetMethod(_state, "GetDistance2D", &WorldObjectGetDistance2d);
    SetMethod(_state, "GetExactDistance", &WorldObjectGetExactDistance);
    SetMethod(_state, "GetExactDistance2d", &WorldObjectGetExactDistance2d);
    SetMethod(_state, "GetExactDistance2D", &WorldObjectGetExactDistance2d);
    SetMethod(_state, "GetRelativePoint", &WorldObjectGetRelativePoint);
    SetMethod(_state, "GetAngle", &WorldObjectGetAngle);
    SetMethod(_state, "IsWithinDist", &WorldObjectIsWithinDist);
    SetMethod(_state, "IsWithinDist3d", &WorldObjectIsWithinDist3d);
    SetMethod(_state, "IsWithinDist3D", &WorldObjectIsWithinDist3d);
    SetMethod(_state, "IsWithinDist2d", &WorldObjectIsWithinDist2d);
    SetMethod(_state, "IsWithinDist2D", &WorldObjectIsWithinDist2d);
    SetMethod(_state, "IsWithinDistInMap", &WorldObjectIsWithinDistInMap);
    SetMethod(_state, "IsInMap", &WorldObjectIsInMap);
    SetMethod(_state, "IsInRange", &WorldObjectIsInRange);
    SetMethod(_state, "IsInRange2d", &WorldObjectIsInRange2d);
    SetMethod(_state, "IsInRange2D", &WorldObjectIsInRange2d);
    SetMethod(_state, "IsInRange3d", &WorldObjectIsInRange3d);
    SetMethod(_state, "IsInRange3D", &WorldObjectIsInRange3d);
    SetMethod(_state, "IsInFront", &WorldObjectIsInFront);
    SetMethod(_state, "IsInBack", &WorldObjectIsInBack);
    SetMethod(_state, "IsWithinLOS", &WorldObjectIsWithinLOS);
    SetMethod(_state, "IsInLineOfSight", &WorldObjectIsWithinLOS);
    SetMethod(_state, "IsFriendlyTo", &WorldObjectIsFriendlyTo);
    SetMethod(_state, "IsHostileTo", &WorldObjectIsHostileTo);
    SetMethod(_state, "GetPlayersInRange", &WorldObjectGetPlayersInRange);
    SetMethod(_state, "GetCreaturesInRange", &WorldObjectGetCreaturesInRange);
    SetMethod(_state, "GetGameObjectsInRange", &WorldObjectGetGameObjectsInRange);
    SetMethod(_state, "GetNearestPlayer", &WorldObjectGetNearestPlayer);
    SetMethod(_state, "GetNearestCreature", &WorldObjectGetNearestCreature);
    SetMethod(_state, "GetNearestGameObject", &WorldObjectGetNearestGameObject);
    SetMethod(_state, "FindNearestPlayer", &WorldObjectFindNearestPlayer);
    SetMethod(_state, "FindNearestCreature", &WorldObjectFindNearestCreature);
    SetMethod(_state, "FindNearestGameObject", &WorldObjectFindNearestGameObject);
    SetMethod(_state, "GetCaster", &DynamicObjectGetCaster);
    SetMethod(_state, "GetUnitCaster", &DynamicObjectGetUnitCaster);
    SetMethod(_state, "GetCasterGUID", &DynamicObjectGetCasterGUID);
    SetMethod(_state, "GetCasterGuid", &DynamicObjectGetCasterGUID);
    SetMethod(_state, "GetCasterGUIDLow", &DynamicObjectGetCasterGUIDLow);
    SetMethod(_state, "GetCasterGuidLow", &DynamicObjectGetCasterGUIDLow);
    SetMethod(_state, "GetSpellId", &DynamicObjectGetSpellId);
    SetMethod(_state, "GetSpellID", &DynamicObjectGetSpellId);
    SetMethod(_state, "GetEffIndex", &DynamicObjectGetEffIndex);
    SetMethod(_state, "GetEffectIndex", &DynamicObjectGetEffIndex);
    SetMethod(_state, "GetDuration", &DynamicObjectGetDuration);
    SetMethod(_state, "GetRadius", &DynamicObjectGetRadius);
    SetMethod(_state, "GetType", &DynamicObjectGetType);
    SetMethod(_state, "Delay", &DynamicObjectDelay);
    SetMethod(_state, "Delete", &DynamicObjectDelete);
    SetMethod(_state, "Remove", &DynamicObjectDelete);
    lua_setfield(_state, -2, "__index");

    lua_pop(_state, 1);
}

void TurtleLuaEngine::RegisterGameObjectTemplateMetatable()
{
    luaL_newmetatable(_state, GAMEOBJECTTEMPLATE_METATABLE);

    lua_newtable(_state);
    SetMethod(_state, "GetId", &GameObjectTemplateGetEntry);
    SetMethod(_state, "GetEntry", &GameObjectTemplateGetEntry);
    SetMethod(_state, "GetType", &GameObjectTemplateGetType);
    SetMethod(_state, "GetGoType", &GameObjectTemplateGetType);
    SetMethod(_state, "GetGOType", &GameObjectTemplateGetType);
    SetMethod(_state, "GetDisplayId", &GameObjectTemplateGetDisplayId);
    SetMethod(_state, "GetDisplayID", &GameObjectTemplateGetDisplayId);
    SetMethod(_state, "GetName", &GameObjectTemplateGetName);
    SetMethod(_state, "GetFaction", &GameObjectTemplateGetFaction);
    SetMethod(_state, "GetFactionTemplate", &GameObjectTemplateGetFaction);
    SetMethod(_state, "GetFlags", &GameObjectTemplateGetFlags);
    SetMethod(_state, "GetSize", &GameObjectTemplateGetSize);
    SetMethod(_state, "GetData", &GameObjectTemplateGetData);
    SetMethod(_state, "GetRawData", &GameObjectTemplateGetData);
    SetMethod(_state, "GetDataCount", &GameObjectTemplateGetDataCount);
    SetMethod(_state, "GetMinMoneyLoot", &GameObjectTemplateGetMinMoneyLoot);
    SetMethod(_state, "GetMaxMoneyLoot", &GameObjectTemplateGetMaxMoneyLoot);
    SetMethod(_state, "GetMinGold", &GameObjectTemplateGetMinMoneyLoot);
    SetMethod(_state, "GetMaxGold", &GameObjectTemplateGetMaxMoneyLoot);
    SetMethod(_state, "GetPhaseQuestId", &GameObjectTemplateGetPhaseQuestId);
    SetMethod(_state, "GetPhaseQuestID", &GameObjectTemplateGetPhaseQuestId);
    SetMethod(_state, "GetScriptId", &GameObjectTemplateGetScriptId);
    SetMethod(_state, "GetScriptID", &GameObjectTemplateGetScriptId);
    SetMethod(_state, "IsDespawnAtAction", &GameObjectTemplateIsDespawnAtAction);
    SetMethod(_state, "IsUsableMounted", &GameObjectTemplateIsUsableMounted);
    SetMethod(_state, "GetLockId", &GameObjectTemplateGetLockId);
    SetMethod(_state, "GetLockID", &GameObjectTemplateGetLockId);
    SetMethod(_state, "GetDespawnPossibility", &GameObjectTemplateGetDespawnPossibility);
    SetMethod(_state, "CannotBeUsedUnderImmunity", &GameObjectTemplateCannotBeUsedUnderImmunity);
    SetMethod(_state, "GetCharges", &GameObjectTemplateGetCharges);
    SetMethod(_state, "GetCooldown", &GameObjectTemplateGetCooldown);
    SetMethod(_state, "GetLinkedGameObjectEntry", &GameObjectTemplateGetLinkedGameObjectEntry);
    SetMethod(_state, "GetLinkedTrapId", &GameObjectTemplateGetLinkedGameObjectEntry);
    SetMethod(_state, "GetLinkedTrapID", &GameObjectTemplateGetLinkedGameObjectEntry);
    SetMethod(_state, "GetAutoCloseTime", &GameObjectTemplateGetAutoCloseTime);
    SetMethod(_state, "GetLootId", &GameObjectTemplateGetLootId);
    SetMethod(_state, "GetLootID", &GameObjectTemplateGetLootId);
    SetMethod(_state, "GetGossipMenuId", &GameObjectTemplateGetGossipMenuId);
    SetMethod(_state, "GetGossipMenuID", &GameObjectTemplateGetGossipMenuId);
    SetMethod(_state, "IsLargeGameObject", &GameObjectTemplateIsLargeGameObject);
    SetMethod(_state, "IsInfiniteGameObject", &GameObjectTemplateIsInfiniteGameObject);
    SetMethod(_state, "IsServerOnly", &GameObjectTemplateIsServerOnly);
    SetMethod(_state, "GetInteractionDistance", &GameObjectTemplateGetInteractionDistance);
    SetMethod(_state, "GetEventScriptId", &GameObjectTemplateGetEventScriptId);
    SetMethod(_state, "GetEventScriptID", &GameObjectTemplateGetEventScriptId);
    SetMethod(_state, "GetDeactivateTime", &GameObjectTemplateGetDeactivateTime);
    SetMethod(_state, "GetQuestId", &GameObjectTemplateGetQuestId);
    SetMethod(_state, "GetQuestID", &GameObjectTemplateGetQuestId);
    SetMethod(_state, "GetSpellId", &GameObjectTemplateGetSpellId);
    SetMethod(_state, "GetSpellID", &GameObjectTemplateGetSpellId);
    SetMethod(_state, "GetRadius", &GameObjectTemplateGetRadius);
    SetMethod(_state, "GetLevel", &GameObjectTemplateGetLevel);
    SetMethod(_state, "GetObjectGuid", &GameObjectTemplateGetObjectGuid);
    SetMethod(_state, "GetObjectGUID", &GameObjectTemplateGetObjectGuid);
    lua_setfield(_state, -2, "__index");

    lua_pop(_state, 1);
}

void TurtleLuaEngine::RegisterItemMetatable()
{
    luaL_newmetatable(_state, ITEM_METATABLE);

    lua_newtable(_state);
    SetMethod(_state, "GetName", &ItemGetName);
    SetMethod(_state, "GetEntry", &ItemGetEntry);
    SetMethod(_state, "GetTemplate", &ItemGetTemplate);
    SetMethod(_state, "GetItemTemplate", &ItemGetTemplate);
    SetMethod(_state, "GetProto", &ItemGetTemplate);
    SetMethod(_state, "GetGUIDLow", &ItemGetGUIDLow);
    SetMethod(_state, "GetGUID", &ObjectGetGUID);
    SetMethod(_state, "GetGuid", &ObjectGetGUID);
    SetMethod(_state, "GetObjectGuid", &ObjectGetGUID);
    SetMethod(_state, "GetTypeId", &ObjectGetTypeId);
    SetMethod(_state, "GetTypeID", &ObjectGetTypeId);
    SetMethod(_state, "IsPlayer", &ObjectIsPlayer);
    SetMethod(_state, "IsCreature", &ObjectIsCreature);
    SetMethod(_state, "IsUnit", &ObjectIsUnit);
    SetMethod(_state, "IsGameObject", &ObjectIsGameObject);
    SetMethod(_state, "IsItem", &ObjectIsItem);
    SetMethod(_state, "IsWorldObject", &ObjectIsWorldObject);
    SetMethod(_state, "GetScale", &ObjectGetScale);
    SetMethod(_state, "SetScale", &ObjectSetScale);
    SetMethod(_state, "IsInWorld", &ObjectIsInWorld);
    SetMethod(_state, "GetUInt32Value", &ObjectGetUInt32Value);
    SetMethod(_state, "SetUInt32Value", &ObjectSetUInt32Value);
    SetMethod(_state, "HasFlag", &ObjectHasFlag);
    SetMethod(_state, "SetFlag", &ObjectSetFlag);
    SetMethod(_state, "RemoveFlag", &ObjectRemoveFlag);
    SetObjectCompatMethods(_state);
    SetMethod(_state, "GetCount", &ItemGetCount);
    SetMethod(_state, "SetCount", &ItemSetCount);
    SetMethod(_state, "GetBagSlot", &ItemGetBagSlot);
    SetMethod(_state, "GetSlot", &ItemGetSlot);
    SetMethod(_state, "IsEquipped", &ItemIsEquipped);
    SetMethod(_state, "IsSoulBound", &ItemIsSoulBound);
    SetMethod(_state, "IsBound", &ItemIsSoulBound);
    SetMethod(_state, "IsAccountBound", &ItemIsAccountBound);
    SetMethod(_state, "IsBoundAccountWide", &ItemIsAccountBound);
    SetMethod(_state, "IsBoundByEnchant", &ItemIsBoundByEnchant);
    SetMethod(_state, "IsNotBoundToPlayer", &ItemIsNotBoundToPlayer);
    SetMethod(_state, "IsLocked", &ItemIsLocked);
    SetMethod(_state, "IsBag", &ItemIsBag);
    SetMethod(_state, "IsNotEmptyBag", &ItemIsNotEmptyBag);
    SetMethod(_state, "IsBroken", &ItemIsBroken);
    SetMethod(_state, "CanBeTraded", &ItemCanBeTraded);
    SetMethod(_state, "IsInTrade", &ItemIsInTrade);
    SetMethod(_state, "IsInBag", &ItemIsInBag);
    SetMethod(_state, "HasQuest", &ItemHasQuest);
    SetMethod(_state, "IsPotion", &ItemIsPotion);
    SetMethod(_state, "IsConjuredConsumable", &ItemIsConjuredConsumable);
    SetMethod(_state, "IsCurrencyToken", &ItemIsCurrencyToken);
    SetMethod(_state, "IsWeaponVellum", &ItemIsWeaponVellum);
    SetMethod(_state, "IsArmorVellum", &ItemIsArmorVellum);
    SetMethod(_state, "IsRefundExpired", &ItemIsRefundExpired);
    SetMethod(_state, "GetItemLink", &ItemGetItemLink);
    SetMethod(_state, "GetOwnerGUID", &ItemGetOwnerGUID);
    SetMethod(_state, "GetOwnerGuid", &ItemGetOwnerGUID);
    SetMethod(_state, "GetOwner", &ItemGetOwner);
    SetMethod(_state, "GetMaxStackCount", &ItemGetMaxStackCount);
    SetMethod(_state, "GetEnchantmentId", &ItemGetEnchantmentId);
    SetMethod(_state, "GetSpellId", &ItemGetSpellId);
    SetMethod(_state, "GetSpellTrigger", &ItemGetSpellTrigger);
    SetMethod(_state, "GetClass", &ItemGetClass);
    SetMethod(_state, "GetSubClass", &ItemGetSubClass);
    SetMethod(_state, "GetDisplayId", &ItemGetDisplayId);
    SetMethod(_state, "GetQuality", &ItemGetQuality);
    SetMethod(_state, "GetBuyCount", &ItemGetBuyCount);
    SetMethod(_state, "GetBuyPrice", &ItemGetBuyPrice);
    SetMethod(_state, "GetSellPrice", &ItemGetSellPrice);
    SetMethod(_state, "GetInventoryType", &ItemGetInventoryType);
    SetMethod(_state, "GetAllowableClass", &ItemGetAllowableClass);
    SetMethod(_state, "GetAllowableRace", &ItemGetAllowableRace);
    SetMethod(_state, "GetItemLevel", &ItemGetItemLevel);
    SetMethod(_state, "GetRequiredLevel", &ItemGetRequiredLevel);
    SetMethod(_state, "GetStatsCount", &ItemGetStatsCount);
    SetMethod(_state, "GetRandomProperty", &ItemGetRandomProperty);
    SetMethod(_state, "GetRandomPropertyId", &ItemGetRandomPropertyId);
    SetMethod(_state, "GetRandomSuffix", &ItemGetRandomSuffix);
    SetMethod(_state, "GetItemSet", &ItemGetItemSet);
    SetMethod(_state, "GetBagSize", &ItemGetBagSize);
    SetMethod(_state, "SetOwner", &ItemSetOwner);
    SetMethod(_state, "SetBinding", &ItemSetBinding);
    SetMethod(_state, "SetEnchantment", &ItemSetEnchantment);
    SetMethod(_state, "ClearEnchantment", &ItemClearEnchantment);
    SetMethod(_state, "SaveToDB", [](lua_State* state) -> int
    {
        Item* item = CheckItem(state, 1);
        bool direct = lua_toboolean(state, 2) != 0;
        if (item)
            item->SaveToDB(direct);
        return 0;
    });
    lua_setfield(_state, -2, "__index");

    lua_pop(_state, 1);
}

void TurtleLuaEngine::RegisterItemTemplateMetatable()
{
    luaL_newmetatable(_state, ITEMTEMPLATE_METATABLE);

    lua_newtable(_state);
    SetMethod(_state, "GetId", &ItemTemplateGetEntry);
    SetMethod(_state, "GetEntry", &ItemTemplateGetEntry);
    SetMethod(_state, "GetItemId", &ItemTemplateGetEntry);
    SetMethod(_state, "GetName", &ItemTemplateGetName);
    SetMethod(_state, "GetDescription", &ItemTemplateGetDescription);
    SetMethod(_state, "GetClass", &ItemTemplateGetClass);
    SetMethod(_state, "GetSubClass", &ItemTemplateGetSubClass);
    SetMethod(_state, "GetQuality", &ItemTemplateGetQuality);
    SetMethod(_state, "GetDisplayId", &ItemTemplateGetDisplayId);
    SetMethod(_state, "GetDisplayID", &ItemTemplateGetDisplayId);
    SetMethod(_state, "GetFlags", &ItemTemplateGetFlags);
    SetMethod(_state, "GetExtraFlags", &ItemTemplateGetExtraFlags);
    SetMethod(_state, "GetIcon", &ItemTemplateGetIcon);
    SetMethod(_state, "GetBuyCount", &ItemTemplateGetBuyCount);
    SetMethod(_state, "GetBuyPrice", &ItemTemplateGetBuyPrice);
    SetMethod(_state, "GetSellPrice", &ItemTemplateGetSellPrice);
    SetMethod(_state, "GetInventoryType", &ItemTemplateGetInventoryType);
    SetMethod(_state, "GetAllowableClass", &ItemTemplateGetAllowableClass);
    SetMethod(_state, "GetAllowableRace", &ItemTemplateGetAllowableRace);
    SetMethod(_state, "GetItemLevel", &ItemTemplateGetItemLevel);
    SetMethod(_state, "GetRequiredLevel", &ItemTemplateGetRequiredLevel);
    SetMethod(_state, "GetRequiredSkill", &ItemTemplateGetRequiredSkill);
    SetMethod(_state, "GetRequiredSkillRank", &ItemTemplateGetRequiredSkillRank);
    SetMethod(_state, "GetRequiredSpell", &ItemTemplateGetRequiredSpell);
    SetMethod(_state, "GetRequiredReputationFaction", &ItemTemplateGetRequiredReputationFaction);
    SetMethod(_state, "GetRequiredReputationRank", &ItemTemplateGetRequiredReputationRank);
    SetMethod(_state, "GetMaxCount", &ItemTemplateGetMaxCount);
    SetMethod(_state, "GetStackable", &ItemTemplateGetStackable);
    SetMethod(_state, "GetMaxStackSize", &ItemTemplateGetMaxStackSize);
    SetMethod(_state, "GetContainerSlots", &ItemTemplateGetContainerSlots);
    SetMethod(_state, "GetDelay", &ItemTemplateGetDelay);
    SetMethod(_state, "GetAmmoType", &ItemTemplateGetAmmoType);
    SetMethod(_state, "GetBlock", &ItemTemplateGetBlock);
    SetMethod(_state, "GetArmor", &ItemTemplateGetArmor);
    SetMethod(_state, "GetBonding", &ItemTemplateGetBonding);
    SetMethod(_state, "GetStartQuest", &ItemTemplateGetStartQuest);
    SetMethod(_state, "GetItemSet", &ItemTemplateGetItemSet);
    SetMethod(_state, "GetMaxDurability", &ItemTemplateGetMaxDurability);
    SetMethod(_state, "GetBagFamily", &ItemTemplateGetBagFamily);
    SetMethod(_state, "GetFoodType", &ItemTemplateGetFoodType);
    SetMethod(_state, "GetOtherTeamEntry", &ItemTemplateGetOtherTeamEntry);
    SetMethod(_state, "GetStatsCount", &ItemTemplateGetStatsCount);
    SetMethod(_state, "GetStatType", &ItemTemplateGetStatType);
    SetMethod(_state, "GetStatValue", &ItemTemplateGetStatValue);
    SetMethod(_state, "GetSpellId", &ItemTemplateGetSpellId);
    SetMethod(_state, "GetSpellID", &ItemTemplateGetSpellId);
    SetMethod(_state, "GetSpellTrigger", &ItemTemplateGetSpellTrigger);
    SetMethod(_state, "GetSpellCharges", &ItemTemplateGetSpellCharges);
    SetMethod(_state, "GetSpellCooldown", &ItemTemplateGetSpellCooldown);
    SetMethod(_state, "GetSpellCategory", &ItemTemplateGetSpellCategory);
    SetMethod(_state, "GetSpellCategoryCooldown", &ItemTemplateGetSpellCategoryCooldown);
    SetMethod(_state, "GetRecoveryTimeForSpell", &ItemTemplateGetRecoveryTimeForSpell);
    SetMethod(_state, "GetCategoryRecoveryTimeForSpell", &ItemTemplateGetCategoryRecoveryTimeForSpell);
    SetMethod(_state, "IsQuestItem", &ItemTemplateIsQuestItem);
    SetMethod(_state, "IsPotion", &ItemTemplateIsPotion);
    SetMethod(_state, "IsConjuredConsumable", &ItemTemplateIsConjuredConsumable);
    SetMethod(_state, "IsWeapon", &ItemTemplateIsWeapon);
    SetMethod(_state, "IsRangedWeapon", &ItemTemplateIsRangedWeapon);
    SetMethod(_state, "IsOffHandItem", &ItemTemplateIsOffHandItem);
    SetMethod(_state, "HasSignature", &ItemTemplateHasSignature);
    lua_setfield(_state, -2, "__index");

    lua_pop(_state, 1);
}

void TurtleLuaEngine::RegisterCreatureTemplateMetatable()
{
    luaL_newmetatable(_state, CREATURETEMPLATE_METATABLE);

    lua_newtable(_state);
    SetMethod(_state, "GetId", &CreatureTemplateGetEntry);
    SetMethod(_state, "GetEntry", &CreatureTemplateGetEntry);
    SetMethod(_state, "GetName", &CreatureTemplateGetName);
    SetMethod(_state, "GetSubName", &CreatureTemplateGetSubName);
    SetMethod(_state, "GetSubname", &CreatureTemplateGetSubName);
    SetMethod(_state, "GetDisplayId", &CreatureTemplateGetDisplayId);
    SetMethod(_state, "GetDisplayID", &CreatureTemplateGetDisplayId);
    SetMethod(_state, "GetDisplayIdCount", &CreatureTemplateGetDisplayIdCount);
    SetMethod(_state, "GetDisplayIDCount", &CreatureTemplateGetDisplayIdCount);
    SetMethod(_state, "GetMountDisplayId", &CreatureTemplateGetMountDisplayId);
    SetMethod(_state, "GetMountDisplayID", &CreatureTemplateGetMountDisplayId);
    SetMethod(_state, "GetGossipMenuId", &CreatureTemplateGetGossipMenuId);
    SetMethod(_state, "GetGossipMenuID", &CreatureTemplateGetGossipMenuId);
    SetMethod(_state, "GetMinLevel", &CreatureTemplateGetMinLevel);
    SetMethod(_state, "GetMaxLevel", &CreatureTemplateGetMaxLevel);
    SetMethod(_state, "GetMinHealth", &CreatureTemplateGetMinHealth);
    SetMethod(_state, "GetMaxHealth", &CreatureTemplateGetMaxHealth);
    SetMethod(_state, "GetMinMana", &CreatureTemplateGetMinMana);
    SetMethod(_state, "GetMaxMana", &CreatureTemplateGetMaxMana);
    SetMethod(_state, "GetArmor", &CreatureTemplateGetArmor);
    SetMethod(_state, "GetFaction", &CreatureTemplateGetFaction);
    SetMethod(_state, "GetFactionTemplate", &CreatureTemplateGetFaction);
    SetMethod(_state, "GetNpcFlags", &CreatureTemplateGetNpcFlags);
    SetMethod(_state, "GetNPCFlags", &CreatureTemplateGetNpcFlags);
    SetMethod(_state, "GetWalkSpeed", &CreatureTemplateGetWalkSpeed);
    SetMethod(_state, "GetRunSpeed", &CreatureTemplateGetRunSpeed);
    SetMethod(_state, "GetScale", &CreatureTemplateGetScale);
    SetMethod(_state, "GetDetectionRange", &CreatureTemplateGetDetectionRange);
    SetMethod(_state, "GetCallForHelpRange", &CreatureTemplateGetCallForHelpRange);
    SetMethod(_state, "GetLeashRange", &CreatureTemplateGetLeashRange);
    SetMethod(_state, "GetRank", &CreatureTemplateGetRank);
    SetMethod(_state, "GetXpMultiplier", &CreatureTemplateGetXpMultiplier);
    SetMethod(_state, "GetXPMultiplier", &CreatureTemplateGetXpMultiplier);
    SetMethod(_state, "GetMinDamage", &CreatureTemplateGetMinDamage);
    SetMethod(_state, "GetMaxDamage", &CreatureTemplateGetMaxDamage);
    SetMethod(_state, "GetDamageSchool", &CreatureTemplateGetDamageSchool);
    SetMethod(_state, "GetAttackPower", &CreatureTemplateGetAttackPower);
    SetMethod(_state, "GetDamageMultiplier", &CreatureTemplateGetDamageMultiplier);
    SetMethod(_state, "GetBaseAttackTime", &CreatureTemplateGetBaseAttackTime);
    SetMethod(_state, "GetRangedAttackTime", &CreatureTemplateGetRangedAttackTime);
    SetMethod(_state, "GetUnitClass", &CreatureTemplateGetUnitClass);
    SetMethod(_state, "GetUnitFlags", &CreatureTemplateGetUnitFlags);
    SetMethod(_state, "GetDynamicFlags", &CreatureTemplateGetDynamicFlags);
    SetMethod(_state, "GetFamily", &CreatureTemplateGetFamily);
    SetMethod(_state, "GetCreatureFamily", &CreatureTemplateGetFamily);
    SetMethod(_state, "GetTrainerType", &CreatureTemplateGetTrainerType);
    SetMethod(_state, "GetTrainerSpell", &CreatureTemplateGetTrainerSpell);
    SetMethod(_state, "GetTrainerClass", &CreatureTemplateGetTrainerClass);
    SetMethod(_state, "GetTrainerRace", &CreatureTemplateGetTrainerRace);
    SetMethod(_state, "GetRangedMinDamage", &CreatureTemplateGetRangedMinDamage);
    SetMethod(_state, "GetRangedMaxDamage", &CreatureTemplateGetRangedMaxDamage);
    SetMethod(_state, "GetRangedAttackPower", &CreatureTemplateGetRangedAttackPower);
    SetMethod(_state, "GetType", &CreatureTemplateGetType);
    SetMethod(_state, "GetCreatureType", &CreatureTemplateGetType);
    SetMethod(_state, "GetTypeFlags", &CreatureTemplateGetTypeFlags);
    SetMethod(_state, "GetCreatureTypeFlags", &CreatureTemplateGetTypeFlags);
    SetMethod(_state, "GetLootId", &CreatureTemplateGetLootId);
    SetMethod(_state, "GetLootID", &CreatureTemplateGetLootId);
    SetMethod(_state, "GetPickpocketLootId", &CreatureTemplateGetPickpocketLootId);
    SetMethod(_state, "GetPickpocketLootID", &CreatureTemplateGetPickpocketLootId);
    SetMethod(_state, "GetSkinningLootId", &CreatureTemplateGetSkinningLootId);
    SetMethod(_state, "GetSkinningLootID", &CreatureTemplateGetSkinningLootId);
    SetMethod(_state, "GetResistance", &CreatureTemplateGetResistance);
    SetMethod(_state, "GetHolyResistance", &CreatureTemplateGetHolyResistance);
    SetMethod(_state, "GetFireResistance", &CreatureTemplateGetFireResistance);
    SetMethod(_state, "GetNatureResistance", &CreatureTemplateGetNatureResistance);
    SetMethod(_state, "GetFrostResistance", &CreatureTemplateGetFrostResistance);
    SetMethod(_state, "GetShadowResistance", &CreatureTemplateGetShadowResistance);
    SetMethod(_state, "GetArcaneResistance", &CreatureTemplateGetArcaneResistance);
    SetMethod(_state, "GetSpellId", &CreatureTemplateGetSpellId);
    SetMethod(_state, "GetSpellID", &CreatureTemplateGetSpellId);
    SetMethod(_state, "GetSpellListId", &CreatureTemplateGetSpellListId);
    SetMethod(_state, "GetSpellListID", &CreatureTemplateGetSpellListId);
    SetMethod(_state, "GetPetSpellListId", &CreatureTemplateGetPetSpellListId);
    SetMethod(_state, "GetPetSpellListID", &CreatureTemplateGetPetSpellListId);
    SetMethod(_state, "GetSpawnSpellId", &CreatureTemplateGetSpawnSpellId);
    SetMethod(_state, "GetSpawnSpellID", &CreatureTemplateGetSpawnSpellId);
    SetMethod(_state, "GetAuraCount", &CreatureTemplateGetAuraCount);
    SetMethod(_state, "GetAurasCount", &CreatureTemplateGetAuraCount);
    SetMethod(_state, "GetAuraId", &CreatureTemplateGetAuraId);
    SetMethod(_state, "GetAuraID", &CreatureTemplateGetAuraId);
    SetMethod(_state, "GetMinGold", &CreatureTemplateGetMinGold);
    SetMethod(_state, "GetMaxGold", &CreatureTemplateGetMaxGold);
    SetMethod(_state, "GetGoldMin", &CreatureTemplateGetMinGold);
    SetMethod(_state, "GetGoldMax", &CreatureTemplateGetMaxGold);
    SetMethod(_state, "GetAIName", &CreatureTemplateGetAIName);
    SetMethod(_state, "GetMovementType", &CreatureTemplateGetMovementType);
    SetMethod(_state, "GetInhabitType", &CreatureTemplateGetInhabitType);
    SetMethod(_state, "IsCivilian", &CreatureTemplateIsCivilian);
    SetMethod(_state, "IsRacialLeader", &CreatureTemplateIsRacialLeader);
    SetMethod(_state, "GetRegenerationFlags", &CreatureTemplateGetRegenerationFlags);
    SetMethod(_state, "GetEquipmentId", &CreatureTemplateGetEquipmentId);
    SetMethod(_state, "GetEquipmentID", &CreatureTemplateGetEquipmentId);
    SetMethod(_state, "GetTrainerId", &CreatureTemplateGetTrainerId);
    SetMethod(_state, "GetTrainerID", &CreatureTemplateGetTrainerId);
    SetMethod(_state, "GetVendorId", &CreatureTemplateGetVendorId);
    SetMethod(_state, "GetVendorID", &CreatureTemplateGetVendorId);
    SetMethod(_state, "GetMechanicImmuneMask", &CreatureTemplateGetMechanicImmuneMask);
    SetMethod(_state, "GetSchoolImmuneMask", &CreatureTemplateGetSchoolImmuneMask);
    SetMethod(_state, "GetImmunityFlags", &CreatureTemplateGetImmunityFlags);
    SetMethod(_state, "GetFlagsExtra", &CreatureTemplateGetFlagsExtra);
    SetMethod(_state, "HasFlagExtra", &CreatureTemplateHasFlagExtra);
    SetMethod(_state, "GetPhaseQuestId", &CreatureTemplateGetPhaseQuestId);
    SetMethod(_state, "GetPhaseQuestID", &CreatureTemplateGetPhaseQuestId);
    SetMethod(_state, "GetScriptId", &CreatureTemplateGetScriptId);
    SetMethod(_state, "GetScriptID", &CreatureTemplateGetScriptId);
    SetMethod(_state, "IsTameable", &CreatureTemplateIsTameable);
    SetMethod(_state, "GetObjectGuid", &CreatureTemplateGetObjectGuid);
    SetMethod(_state, "GetObjectGUID", &CreatureTemplateGetObjectGuid);
    lua_setfield(_state, -2, "__index");

    lua_pop(_state, 1);
}

void TurtleLuaEngine::RegisterQuestMetatable()
{
    luaL_newmetatable(_state, QUEST_METATABLE);

    lua_newtable(_state);
    SetMethod(_state, "GetId", &QuestGetId);
    SetMethod(_state, "GetQuestId", &QuestGetId);
    SetMethod(_state, "GetTitle", &QuestGetTitle);
    SetMethod(_state, "GetLevel", &QuestGetLevel);
    SetMethod(_state, "GetQuestLevel", &QuestGetLevel);
    SetMethod(_state, "GetMinLevel", &QuestGetMinLevel);
    SetMethod(_state, "GetMaxLevel", &QuestGetMaxLevel);
    SetMethod(_state, "GetFlags", &QuestGetFlags);
    SetMethod(_state, "GetQuestFlags", &QuestGetFlags);
    SetMethod(_state, "HasFlag", &QuestHasFlag);
    SetMethod(_state, "IsRepeatable", &QuestIsRepeatable);
    SetMethod(_state, "IsDaily", &QuestIsDaily);
    SetMethod(_state, "GetNextQuestId", &QuestGetNextQuestId);
    SetMethod(_state, "GetNextQuestID", &QuestGetNextQuestId);
    SetMethod(_state, "GetPrevQuestId", &QuestGetPrevQuestId);
    SetMethod(_state, "GetPrevQuestID", &QuestGetPrevQuestId);
    SetMethod(_state, "GetNextQuestInChain", &QuestGetNextQuestInChain);
    SetMethod(_state, "GetType", &QuestGetType);
    lua_setfield(_state, -2, "__index");

    lua_pop(_state, 1);
}

void TurtleLuaEngine::RegisterGroupMetatable()
{
    luaL_newmetatable(_state, GROUP_METATABLE);

    lua_newtable(_state);
    SetMethod(_state, "GetId", &GroupGetId);
    SetMethod(_state, "GetMembersCount", &GroupGetMembersCount);
    SetMethod(_state, "GetMemberCount", &GroupGetMembersCount);
    SetMethod(_state, "IsRaid", &GroupIsRaid);
    SetMethod(_state, "IsRaidGroup", &GroupIsRaid);
    SetMethod(_state, "IsFull", &GroupIsFull);
    SetMethod(_state, "IsBGGroup", &GroupIsBGGroup);
    SetMethod(_state, "IsLFGGroup", &GroupIsLFGGroup);
    SetMethod(_state, "GetLeaderGUID", &GroupGetLeaderGUID);
    SetMethod(_state, "GetLeaderGuid", &GroupGetLeaderGUID);
    SetMethod(_state, "GetLeaderName", &GroupGetLeaderName);
    SetMethod(_state, "GetLeader", &GroupGetLeader);
    SetMethod(_state, "IsLeader", &GroupIsLeader);
    SetMethod(_state, "IsMember", &GroupIsMember);
    SetMethod(_state, "IsAssistant", &GroupIsAssistant);
    SetMethod(_state, "SameSubGroup", &GroupSameSubGroup);
    SetMethod(_state, "HasFreeSlotSubGroup", &GroupHasFreeSlotSubGroup);
    SetMethod(_state, "GetMemberGroup", &GroupGetMemberGroup);
    SetMethod(_state, "GetMembers", &GroupGetMembers);
    SetMethod(_state, "GetGUID", &GroupGetGUID);
    SetMethod(_state, "GetGuid", &GroupGetGUID);
    SetMethod(_state, "GetMemberGUID", &GroupGetMemberGUID);
    SetMethod(_state, "GetMemberGuid", &GroupGetMemberGUID);
    SetMethod(_state, "GetGroupType", &GroupGetGroupType);
    SetMethod(_state, "SetLeader", &GroupSetLeader);
    SetMethod(_state, "AddMember", &GroupAddMember);
    SetMethod(_state, "RemoveMember", &GroupRemoveMember);
    SetMethod(_state, "Disband", &GroupDisband);
    SetMethod(_state, "ConvertToRaid", &GroupConvertToRaid);
    SetMethod(_state, "SetMembersGroup", &GroupSetMembersGroup);
    SetMethod(_state, "SetTargetIcon", &GroupSetTargetIcon);
    SetMethod(_state, "SetMemberFlag", &GroupSetMemberFlag);
    SetMethod(_state, "SendPacket", &GroupSendPacket);
    lua_setfield(_state, -2, "__index");

    lua_pop(_state, 1);
}

void TurtleLuaEngine::RegisterGuildMetatable()
{
    luaL_newmetatable(_state, GUILD_METATABLE);

    lua_newtable(_state);
    SetMethod(_state, "GetId", &GuildGetId);
    SetMethod(_state, "GetName", &GuildGetName);
    SetMethod(_state, "GetMOTD", &GuildGetMOTD);
    SetMethod(_state, "GetMotd", &GuildGetMOTD);
    SetMethod(_state, "GetInfo", &GuildGetInfo);
    SetMethod(_state, "GetLeaderGUID", &GuildGetLeaderGUID);
    SetMethod(_state, "GetLeaderGuid", &GuildGetLeaderGUID);
    SetMethod(_state, "GetLeader", &GuildGetLeader);
    SetMethod(_state, "GetMemberCount", &GuildGetMemberCount);
    SetMethod(_state, "GetMembersCount", &GuildGetMemberCount);
    SetMethod(_state, "GetMembers", &GuildGetMembers);
    SetMethod(_state, "GetRankName", &GuildGetRankName);
    SetMethod(_state, "GetRank", &GuildGetRank);
    SetMethod(_state, "GetOnlineMembers", &GuildGetOnlineMembers);
    SetMethod(_state, "SendMessage", &GuildSendMessage);
    SetMethod(_state, "Broadcast", &GuildSendMessage);
    SetMethod(_state, "SetLeader", &GuildSetLeader);
    SetMethod(_state, "SetBankTabText", &GuildSetBankTabText);
    SetMethod(_state, "SendPacket", &GuildSendPacket);
    SetMethod(_state, "SendPacketToRanked", &GuildSendPacketToRanked);
    SetMethod(_state, "Disband", &GuildDisband);
    SetMethod(_state, "AddMember", &GuildAddMember);
    SetMethod(_state, "DeleteMember", &GuildDeleteMember);
    SetMethod(_state, "SetMemberRank", &GuildSetMemberRank);
    SetMethod(_state, "SetName", &GuildSetName);
    SetMethod(_state, "UpdateMemberData", &GuildUpdateMemberData);
    SetMethod(_state, "MassInviteToEvent", &GuildMassInviteToEvent);
    SetMethod(_state, "SwapItems", &GuildSwapItems);
    SetMethod(_state, "SwapItemsWithInventory", &GuildSwapItemsWithInventory);
    SetMethod(_state, "GetTotalBankMoney", &GuildGetTotalBankMoney);
    SetMethod(_state, "GetCreatedDate", &GuildGetCreatedDate);
    SetMethod(_state, "ResetTimes", &GuildResetTimes);
    SetMethod(_state, "ModifyBankMoney", &GuildModifyBankMoney);
    lua_setfield(_state, -2, "__index");

    lua_pop(_state, 1);
}

void TurtleLuaEngine::RegisterMapMetatable()
{
    luaL_newmetatable(_state, MAP_METATABLE);

    lua_newtable(_state);
    SetMethod(_state, "GetId", &MapGetId);
    SetMethod(_state, "GetMapId", &MapGetId);
    SetMethod(_state, "GetInstanceId", &MapGetInstanceId);
    SetMethod(_state, "GetName", &MapGetName);
    SetMethod(_state, "GetMapName", &MapGetName);
    SetMethod(_state, "IsDungeon", &MapIsDungeon);
    SetMethod(_state, "IsRaid", &MapIsRaid);
    SetMethod(_state, "IsBattleground", &MapIsBattleground);
    SetMethod(_state, "IsBattleGround", &MapIsBattleground);
    SetMethod(_state, "IsArena", &MapIsArena);
    SetMethod(_state, "IsContinent", &MapIsContinent);
    SetMethod(_state, "IsEmpty", &MapIsEmpty);
    SetMethod(_state, "IsHeroic", &MapIsHeroic);
    SetMethod(_state, "GetDifficulty", &MapGetDifficulty);
    SetMethod(_state, "GetHeight", &MapGetHeight);
    SetMethod(_state, "GetAreaId", &MapGetAreaId);
    SetMethod(_state, "GetAreaID", &MapGetAreaId);
    SetMethod(_state, "GetCreatures", &MapGetCreatures);
    SetMethod(_state, "GetCreaturesByAreaId", &MapGetCreaturesByAreaId);
    SetMethod(_state, "GetCreaturesByAreaID", &MapGetCreaturesByAreaId);
    SetMethod(_state, "GetPlayers", &MapGetPlayers);
    SetMethod(_state, "GetPlayerCount", &MapGetPlayerCount);
    SetMethod(_state, "GetPlayersCount", &MapGetPlayerCount);
    SetMethod(_state, "GetPlayer", &MapGetPlayer);
    SetMethod(_state, "GetCreature", &MapGetCreature);
    SetMethod(_state, "GetAnyTypeCreature", &MapGetCreature);
    SetMethod(_state, "GetGameObject", &MapGetGameObject);
    SetMethod(_state, "GetGO", &MapGetGameObject);
    SetMethod(_state, "GetDynamicObject", &MapGetDynamicObject);
    SetMethod(_state, "GetDynObject", &MapGetDynamicObject);
    SetMethod(_state, "GetUnit", &MapGetUnit);
    SetMethod(_state, "GetWorldObject", &MapGetWorldObject);
    SetMethod(_state, "GetWorldObjectOrPlayer", &MapGetWorldObjectOrPlayer);
    SetMethod(_state, "SetWeather", &MapSetWeather);
    SetMethod(_state, "GetInstanceData", &MapGetInstanceData);
    SetMethod(_state, "SaveInstanceData", &MapSaveInstanceData);
    lua_setfield(_state, -2, "__index");

    lua_pop(_state, 1);
}

void TurtleLuaEngine::RegisterInstanceDataMetatable()
{
    luaL_newmetatable(_state, INSTANCEDATA_METATABLE);

    lua_newtable(_state);
    SetMethod(_state, "GetMap", &InstanceDataGetMap);
    SetMethod(_state, "GetMapId", &InstanceDataGetMapId);
    SetMethod(_state, "GetInstanceId", &InstanceDataGetInstanceId);
    SetMethod(_state, "GetInstanceID", &InstanceDataGetInstanceId);
    SetMethod(_state, "IsEncounterInProgress", &InstanceDataIsEncounterInProgress);
    SetMethod(_state, "GetData", &InstanceDataGetData);
    SetMethod(_state, "SetData", &InstanceDataSetData);
    SetMethod(_state, "GetData64", &InstanceDataGetData64);
    SetMethod(_state, "SetData64", &InstanceDataSetData64);
    SetMethod(_state, "GetGuid", &InstanceDataGetGuid);
    SetMethod(_state, "GetGUID", &InstanceDataGetGuid);
    SetMethod(_state, "SetGuid", &InstanceDataSetGuid);
    SetMethod(_state, "SetGUID", &InstanceDataSetGuid);
    SetMethod(_state, "Save", &InstanceDataSave);
    SetMethod(_state, "Load", &InstanceDataLoad);
    SetMethod(_state, "SaveToDB", &InstanceDataSaveToDB);
    lua_setfield(_state, -2, "__index");

    lua_pop(_state, 1);
}

void TurtleLuaEngine::RegisterBattleGroundMetatable()
{
    luaL_newmetatable(_state, BATTLEGROUND_METATABLE);

    lua_newtable(_state);
    SetMethod(_state, "GetName", &BattleGroundGetName);
    SetMethod(_state, "GetAlivePlayersCountByTeam", &BattleGroundGetAlivePlayersCountByTeam);
    SetMethod(_state, "GetMap", &BattleGroundGetMap);
    SetMethod(_state, "GetBonusHonorFromKillCount", &BattleGroundGetBonusHonorFromKillCount);
    SetMethod(_state, "GetEndTime", &BattleGroundGetEndTime);
    SetMethod(_state, "GetFreeSlotsForTeam", &BattleGroundGetFreeSlotsForTeam);
    SetMethod(_state, "GetInstanceId", &BattleGroundGetInstanceId);
    SetMethod(_state, "GetInstanceID", &BattleGroundGetInstanceId);
    SetMethod(_state, "GetMapId", &BattleGroundGetMapId);
    SetMethod(_state, "GetMapID", &BattleGroundGetMapId);
    SetMethod(_state, "GetTypeId", &BattleGroundGetTypeId);
    SetMethod(_state, "GetTypeID", &BattleGroundGetTypeId);
    SetMethod(_state, "GetBgTypeId", &BattleGroundGetTypeId);
    SetMethod(_state, "GetBGTypeId", &BattleGroundGetTypeId);
    SetMethod(_state, "GetMaxLevel", &BattleGroundGetMaxLevel);
    SetMethod(_state, "GetMinLevel", &BattleGroundGetMinLevel);
    SetMethod(_state, "GetMaxPlayers", &BattleGroundGetMaxPlayers);
    SetMethod(_state, "GetMinPlayers", &BattleGroundGetMinPlayers);
    SetMethod(_state, "GetMaxPlayersPerTeam", &BattleGroundGetMaxPlayersPerTeam);
    SetMethod(_state, "GetMinPlayersPerTeam", &BattleGroundGetMinPlayersPerTeam);
    SetMethod(_state, "GetWinner", &BattleGroundGetWinner);
    SetMethod(_state, "GetStatus", &BattleGroundGetStatus);
    SetMethod(_state, "IsArena", &BattleGroundIsArena);
    lua_setfield(_state, -2, "__index");

    lua_pop(_state, 1);
}

void TurtleLuaEngine::RegisterTicketMetatable()
{
    luaL_newmetatable(_state, TICKET_METATABLE);

    lua_newtable(_state);
    SetMethod(_state, "IsClosed", &TicketIsClosed);
    SetMethod(_state, "IsCompleted", &TicketIsCompleted);
    SetMethod(_state, "IsFromPlayer", &TicketIsFromPlayer);
    SetMethod(_state, "IsAssigned", &TicketIsAssigned);
    SetMethod(_state, "IsAssignedTo", &TicketIsAssignedTo);
    SetMethod(_state, "IsAssignedNotTo", &TicketIsAssignedNotTo);
    SetMethod(_state, "GetId", &TicketGetId);
    SetMethod(_state, "GetID", &TicketGetId);
    SetMethod(_state, "GetPlayer", &TicketGetPlayer);
    SetMethod(_state, "GetPlayerName", &TicketGetPlayerName);
    SetMethod(_state, "GetMessage", &TicketGetMessage);
    SetMethod(_state, "GetAssignedPlayer", &TicketGetAssignedPlayer);
    SetMethod(_state, "GetAssignedToGUID", &TicketGetAssignedToGUID);
    SetMethod(_state, "GetAssignedToGuid", &TicketGetAssignedToGUID);
    SetMethod(_state, "GetLastModifiedTime", &TicketGetLastModifiedTime);
    SetMethod(_state, "GetResponse", &TicketGetResponse);
    SetMethod(_state, "GetChatLog", &TicketGetChatLog);
    SetMethod(_state, "SetAssignedTo", &TicketSetAssignedTo);
    SetMethod(_state, "SetResolvedBy", &TicketSetResolvedBy);
    SetMethod(_state, "SetCompleted", &TicketSetCompleted);
    SetMethod(_state, "SetMessage", &TicketSetMessage);
    SetMethod(_state, "SetComment", &TicketSetComment);
    SetMethod(_state, "SetViewed", &TicketSetViewed);
    SetMethod(_state, "SetUnassigned", &TicketSetUnassigned);
    SetMethod(_state, "SetPosition", &TicketSetPosition);
    SetMethod(_state, "AppendResponse", &TicketAppendResponse);
    SetMethod(_state, "DeleteResponse", &TicketDeleteResponse);
    lua_setfield(_state, -2, "__index");

    lua_pop(_state, 1);
}

void TurtleLuaEngine::RegisterAuraMetatable()
{
    luaL_newmetatable(_state, AURA_METATABLE);

    lua_newtable(_state);
    SetMethod(_state, "GetCaster", &AuraGetCaster);
    SetMethod(_state, "GetCasterGUID", &AuraGetCasterGUID);
    SetMethod(_state, "GetCasterGuid", &AuraGetCasterGUID);
    SetMethod(_state, "GetCasterGUIDLow", &AuraGetCasterGUIDLow);
    SetMethod(_state, "GetCasterGuidLow", &AuraGetCasterGUIDLow);
    SetMethod(_state, "GetCasterLevel", &AuraGetCasterLevel);
    SetMethod(_state, "GetDuration", &AuraGetDuration);
    SetMethod(_state, "GetMaxDuration", &AuraGetMaxDuration);
    SetMethod(_state, "GetAuraId", &AuraGetAuraId);
    SetMethod(_state, "GetAuraID", &AuraGetAuraId);
    SetMethod(_state, "GetId", &AuraGetAuraId);
    SetMethod(_state, "GetSpellId", &AuraGetAuraId);
    SetMethod(_state, "GetStackAmount", &AuraGetStackAmount);
    SetMethod(_state, "GetOwner", &AuraGetOwner);
    SetMethod(_state, "SetDuration", &AuraSetDuration);
    SetMethod(_state, "SetMaxDuration", &AuraSetMaxDuration);
    SetMethod(_state, "SetStackAmount", &AuraSetStackAmount);
    SetMethod(_state, "Remove", &AuraRemove);
    lua_setfield(_state, -2, "__index");

    lua_pop(_state, 1);
}

void TurtleLuaEngine::RegisterSpellMetatable()
{
    luaL_newmetatable(_state, SPELL_METATABLE);

    lua_newtable(_state);
    SetMethod(_state, "GetCaster", &SpellGetCaster);
    SetMethod(_state, "GetEntry", &SpellGetEntry);
    SetMethod(_state, "GetId", &SpellGetEntry);
    SetMethod(_state, "GetSpellId", &SpellGetEntry);
    SetMethod(_state, "GetSpellInfo", &SpellGetSpellInfo);
    SetMethod(_state, "GetSpellEntry", &SpellGetSpellInfo);
    SetMethod(_state, "GetCastTime", &SpellGetCastTime);
    SetMethod(_state, "GetCastedTime", &SpellGetCastedTime);
    SetMethod(_state, "GetPowerCost", &SpellGetPowerCost);
    SetMethod(_state, "GetReagentCost", &SpellGetReagentCost);
    SetMethod(_state, "GetDuration", &SpellGetDuration);
    SetMethod(_state, "GetTargetDest", &SpellGetTargetDest);
    SetMethod(_state, "GetTargets", &SpellGetTargets);
    SetMethod(_state, "GetTarget", &SpellGetTarget);
    SetMethod(_state, "GetUnitTarget", &SpellGetTarget);
    SetMethod(_state, "GetGameObjectTarget", &SpellGetGameObjectTarget);
    SetMethod(_state, "GetGOTarget", &SpellGetGameObjectTarget);
    SetMethod(_state, "GetItemTarget", &SpellGetItemTarget);
    SetMethod(_state, "GetCastItem", &SpellGetCastItem);
    SetMethod(_state, "IsTriggered", &SpellIsTriggered);
    SetMethod(_state, "IsCastByItem", &SpellIsCastByItem);
    SetMethod(_state, "IsAutoRepeat", &SpellIsAutoRepeat);
    SetMethod(_state, "SetAutoRepeat", &SpellSetAutoRepeat);
    SetMethod(_state, "Cast", &SpellCast);
    SetMethod(_state, "Cancel", &SpellCancel);
    SetMethod(_state, "Finish", &SpellFinish);
    lua_setfield(_state, -2, "__index");
    lua_pop(_state, 1);
}

void TurtleLuaEngine::RegisterSpellInfoMetatable()
{
    luaL_newmetatable(_state, SPELLINFO_METATABLE);

    lua_newtable(_state);
    SetMethod(_state, "GetId", &SpellInfoGetId);
    SetMethod(_state, "GetEntry", &SpellInfoGetId);
    SetMethod(_state, "GetSpellId", &SpellInfoGetId);
    SetMethod(_state, "GetName", &SpellInfoGetName);
    SetMethod(_state, "GetSpellName", &SpellInfoGetSpellName);
    SetMethod(_state, "GetRank", &SpellInfoGetRank);
    SetMethod(_state, "GetSchool", &SpellInfoGetSchool);
    SetMethod(_state, "GetSchoolMask", &SpellInfoGetSchoolMask);
    SetMethod(_state, "GetSpellSchoolMask", &SpellInfoGetSchoolMask);
    SetMethod(_state, "GetCategory", &SpellInfoGetCategory);
    SetMethod(_state, "GetDispel", &SpellInfoGetDispel);
    SetMethod(_state, "GetMechanic", &SpellInfoGetMechanic);
    SetMethod(_state, "GetAllMechanicMask", &SpellInfoGetAllMechanicMask);
    SetMethod(_state, "GetAllSpellMechanicMask", &SpellInfoGetAllMechanicMask);
    SetMethod(_state, "GetAttributes", &SpellInfoGetAttributes);
    SetMethod(_state, "HasAttribute", &SpellInfoHasAttribute);
    SetMethod(_state, "GetAttributesEx", &SpellInfoGetAttributesEx);
    SetMethod(_state, "GetAttributesEx2", &SpellInfoGetAttributesEx2);
    SetMethod(_state, "GetAttributesEx3", &SpellInfoGetAttributesEx3);
    SetMethod(_state, "GetAttributesEx4", &SpellInfoGetAttributesEx4);
    SetMethod(_state, "GetAttributesEx5", &SpellInfoGetAttributesEx5);
    SetMethod(_state, "GetAttributesEx6", &SpellInfoGetAttributesEx6);
    SetMethod(_state, "GetAttributesEx7", &SpellInfoGetAttributesEx7);
    SetMethod(_state, "GetStances", &SpellInfoGetStances);
    SetMethod(_state, "GetStancesNot", &SpellInfoGetStancesNot);
    SetMethod(_state, "GetTargets", &SpellInfoGetTargets);
    SetMethod(_state, "GetTargetCreatureType", &SpellInfoGetTargetCreatureType);
    SetMethod(_state, "GetRequiresSpellFocus", &SpellInfoGetRequiresSpellFocus);
    SetMethod(_state, "GetFacingCasterFlags", &SpellInfoGetFacingCasterFlags);
    SetMethod(_state, "GetCasterAuraState", &SpellInfoGetCasterAuraState);
    SetMethod(_state, "GetTargetAuraState", &SpellInfoGetTargetAuraState);
    SetMethod(_state, "GetCasterAuraStateNot", &SpellInfoGetCasterAuraStateNot);
    SetMethod(_state, "GetTargetAuraStateNot", &SpellInfoGetTargetAuraStateNot);
    SetMethod(_state, "GetCasterAuraSpell", &SpellInfoGetCasterAuraSpell);
    SetMethod(_state, "GetTargetAuraSpell", &SpellInfoGetTargetAuraSpell);
    SetMethod(_state, "GetExcludeCasterAuraSpell", &SpellInfoGetExcludeCasterAuraSpell);
    SetMethod(_state, "GetExcludeTargetAuraSpell", &SpellInfoGetExcludeTargetAuraSpell);
    SetMethod(_state, "GetCastingTimeIndex", &SpellInfoGetCastingTimeIndex);
    SetMethod(_state, "GetRecoveryTime", &SpellInfoGetRecoveryTime);
    SetMethod(_state, "GetRawRecoveryTime", &SpellInfoGetRawRecoveryTime);
    SetMethod(_state, "GetCategoryRecoveryTime", &SpellInfoGetCategoryRecoveryTime);
    SetMethod(_state, "GetStartRecoveryTime", &SpellInfoGetStartRecoveryTime);
    SetMethod(_state, "GetStartRecoveryCategory", &SpellInfoGetStartRecoveryCategory);
    SetMethod(_state, "GetInterruptFlags", &SpellInfoGetInterruptFlags);
    SetMethod(_state, "GetAuraInterruptFlags", &SpellInfoGetAuraInterruptFlags);
    SetMethod(_state, "GetChannelInterruptFlags", &SpellInfoGetChannelInterruptFlags);
    SetMethod(_state, "GetProcFlags", &SpellInfoGetProcFlags);
    SetMethod(_state, "GetProcChance", &SpellInfoGetProcChance);
    SetMethod(_state, "GetProcCharges", &SpellInfoGetProcCharges);
    SetMethod(_state, "GetMaxLevel", &SpellInfoGetMaxLevel);
    SetMethod(_state, "GetBaseLevel", &SpellInfoGetBaseLevel);
    SetMethod(_state, "GetSpellLevel", &SpellInfoGetSpellLevel);
    SetMethod(_state, "GetDurationIndex", &SpellInfoGetDurationIndex);
    SetMethod(_state, "GetDuration", &SpellInfoGetDuration);
    SetMethod(_state, "GetPowerType", &SpellInfoGetPowerType);
    SetMethod(_state, "GetManaCost", &SpellInfoGetManaCost);
    SetMethod(_state, "GetManaCostPerlevel", &SpellInfoGetManaCostPerlevel);
    SetMethod(_state, "GetManaPerSecond", &SpellInfoGetManaPerSecond);
    SetMethod(_state, "GetManaPerSecondPerLevel", &SpellInfoGetManaPerSecondPerLevel);
    SetMethod(_state, "GetManaCostPercentage", &SpellInfoGetManaCostPercentage);
    SetMethod(_state, "GetRangeIndex", &SpellInfoGetRangeIndex);
    SetMethod(_state, "GetSpeed", &SpellInfoGetSpeed);
    SetMethod(_state, "GetStackAmount", &SpellInfoGetStackAmount);
    SetMethod(_state, "GetSpellFamilyName", &SpellInfoGetSpellFamilyName);
    SetMethod(_state, "GetSpellFamilyFlags", &SpellInfoGetSpellFamilyFlags);
    SetMethod(_state, "GetMaxTargetLevel", &SpellInfoGetMaxTargetLevel);
    SetMethod(_state, "GetMaxAffectedTargets", &SpellInfoGetMaxAffectedTargets);
    SetMethod(_state, "GetDmgClass", &SpellInfoGetDmgClass);
    SetMethod(_state, "GetPreventionType", &SpellInfoGetPreventionType);
    SetMethod(_state, "GetSpellIconID", &SpellInfoGetSpellIconID);
    SetMethod(_state, "GetSpellIconId", &SpellInfoGetSpellIconID);
    SetMethod(_state, "GetSpellPriority", &SpellInfoGetSpellPriority);
    SetMethod(_state, "GetActiveIconID", &SpellInfoGetActiveIconID);
    SetMethod(_state, "GetActiveIconId", &SpellInfoGetActiveIconID);
    SetMethod(_state, "GetSpellVisual", &SpellInfoGetSpellVisual);
    SetMethod(_state, "GetTotem", &SpellInfoGetTotem);
    SetMethod(_state, "GetTotemCategory", &SpellInfoGetTotemCategory);
    SetMethod(_state, "GetReagent", &SpellInfoGetReagent);
    SetMethod(_state, "GetReagentCount", &SpellInfoGetReagentCount);
    SetMethod(_state, "GetEquippedItemClass", &SpellInfoGetEquippedItemClass);
    SetMethod(_state, "GetEquippedItemSubClassMask", &SpellInfoGetEquippedItemSubClassMask);
    SetMethod(_state, "GetEquippedItemInventoryTypeMask", &SpellInfoGetEquippedItemInventoryTypeMask);
    SetMethod(_state, "GetEffect", &SpellInfoGetEffect);
    SetMethod(_state, "GetEffectDieSides", &SpellInfoGetEffectDieSides);
    SetMethod(_state, "GetEffectRealPointsPerLevel", &SpellInfoGetEffectRealPointsPerLevel);
    SetMethod(_state, "GetEffectBaseDice", &SpellInfoGetEffectBaseDice);
    SetMethod(_state, "GetEffectBasePoints", &SpellInfoGetEffectBasePoints);
    SetMethod(_state, "GetEffectMechanic", &SpellInfoGetEffectMechanic);
    SetMethod(_state, "GetEffectImplicitTargetA", &SpellInfoGetEffectImplicitTargetA);
    SetMethod(_state, "GetEffectImplicitTargetB", &SpellInfoGetEffectImplicitTargetB);
    SetMethod(_state, "GetEffectRadiusIndex", &SpellInfoGetEffectRadiusIndex);
    SetMethod(_state, "GetEffectApplyAuraName", &SpellInfoGetEffectApplyAuraName);
    SetMethod(_state, "GetEffectAmplitude", &SpellInfoGetEffectAmplitude);
    SetMethod(_state, "GetEffectMultipleValue", &SpellInfoGetEffectMultipleValue);
    SetMethod(_state, "GetEffectValueMultiplier", &SpellInfoGetEffectValueMultiplier);
    SetMethod(_state, "GetEffectChainTarget", &SpellInfoGetEffectChainTarget);
    SetMethod(_state, "GetEffectItemType", &SpellInfoGetEffectItemType);
    SetMethod(_state, "GetEffectMiscValue", &SpellInfoGetEffectMiscValue);
    SetMethod(_state, "GetEffectMiscValueB", &SpellInfoGetEffectMiscValueB);
    SetMethod(_state, "GetEffectTriggerSpell", &SpellInfoGetEffectTriggerSpell);
    SetMethod(_state, "GetEffectPointsPerComboPoint", &SpellInfoGetEffectPointsPerComboPoint);
    SetMethod(_state, "GetEffectSpellClassMask", &SpellInfoGetEffectSpellClassMask);
    SetMethod(_state, "GetDamageMultiplier", &SpellInfoGetDamageMultiplier);
    SetMethod(_state, "GetEffectDamageMultiplier", &SpellInfoGetEffectDamageMultiplier);
    SetMethod(_state, "GetEffectBonusMultiplier", &SpellInfoGetEffectBonusMultiplier);
    SetMethod(_state, "GetAreaGroupId", &SpellInfoGetAreaGroupId);
    SetMethod(_state, "GetRuneCostID", &SpellInfoGetRuneCostID);
    SetMethod(_state, "CalculateSimpleValue", &SpellInfoCalculateSimpleValue);
    SetMethod(_state, "HasAura", &SpellInfoHasAura);
    SetMethod(_state, "HasEffect", &SpellInfoHasEffect);
    SetMethod(_state, "IsPassive", &SpellInfoIsPassive);
    SetMethod(_state, "IsPassiveSpell", &SpellInfoIsPassive);
    SetMethod(_state, "IsPositive", &SpellInfoIsPositive);
    SetMethod(_state, "IsPositiveSpell", &SpellInfoIsPositive);
    SetMethod(_state, "IsBinary", &SpellInfoIsBinary);
    SetMethod(_state, "IsDispel", &SpellInfoIsDispel);
    SetMethod(_state, "IsNonPeriodicDispel", &SpellInfoIsNonPeriodicDispel);
    SetMethod(_state, "IsCCSpell", &SpellInfoIsCCSpell);
    SetMethod(_state, "IsCustomSpell", &SpellInfoIsCustomSpell);
    SetMethod(_state, "IsPvEHeartBeat", &SpellInfoIsPvEHeartBeat);
    SetMethod(_state, "HasAreaAuraEffect", &SpellInfoHasAreaAuraEffect);
    SetMethod(_state, "IsAffectingArea", &SpellInfoIsAffectingArea);
    SetMethod(_state, "IsExplicitDiscovery", &SpellInfoIsExplicitDiscovery);
    SetMethod(_state, "IsLootCrafting", &SpellInfoIsLootCrafting);
    SetMethod(_state, "IsProfessionOrRiding", &SpellInfoIsProfessionOrRiding);
    SetMethod(_state, "IsProfession", &SpellInfoIsProfession);
    SetMethod(_state, "IsPrimaryProfession", &SpellInfoIsPrimaryProfession);
    SetMethod(_state, "IsPrimaryProfessionFirstRank", &SpellInfoIsPrimaryProfessionFirstRank);
    SetMethod(_state, "IsAbilityLearnedWithProfession", &SpellInfoIsAbilityLearnedWithProfession);
    SetMethod(_state, "IsAbilityOfSkillType", &SpellInfoIsAbilityOfSkillType);
    SetMethod(_state, "IsTargetingArea", &SpellInfoIsTargetingArea);
    SetMethod(_state, "NeedsExplicitUnitTarget", &SpellInfoNeedsExplicitUnitTarget);
    SetMethod(_state, "NeedsToBeTriggeredByCaster", &SpellInfoNeedsToBeTriggeredByCaster);
    SetMethod(_state, "IsSelfCast", &SpellInfoIsSelfCast);
    SetMethod(_state, "IsAutocastable", &SpellInfoIsAutocastable);
    SetMethod(_state, "IsStackableWithRanks", &SpellInfoIsStackableWithRanks);
    SetMethod(_state, "IsPassiveStackableWithRanks", &SpellInfoIsPassiveStackableWithRanks);
    SetMethod(_state, "IsMultiSlotAura", &SpellInfoIsMultiSlotAura);
    SetMethod(_state, "IsCooldownStartedOnEvent", &SpellInfoIsCooldownStartedOnEvent);
    SetMethod(_state, "IsDeathPersistent", &SpellInfoIsDeathPersistent);
    SetMethod(_state, "IsRequiringDeadTarget", &SpellInfoIsRequiringDeadTarget);
    SetMethod(_state, "IsAllowingDeadTarget", &SpellInfoIsAllowingDeadTarget);
    SetMethod(_state, "CanBeUsedInCombat", &SpellInfoCanBeUsedInCombat);
    SetMethod(_state, "IsPositiveEffect", &SpellInfoIsPositiveEffect);
    SetMethod(_state, "IsChanneled", &SpellInfoIsChanneled);
    SetMethod(_state, "NeedsComboPoints", &SpellInfoNeedsComboPoints);
    SetMethod(_state, "IsBreakingStealth", &SpellInfoIsBreakingStealth);
    SetMethod(_state, "IsRangedWeaponSpell", &SpellInfoIsRangedWeaponSpell);
    SetMethod(_state, "IsAutoRepeatRangedSpell", &SpellInfoIsAutoRepeatRangedSpell);
    SetMethod(_state, "IsAffectedBySpellMods", &SpellInfoIsAffectedBySpellMods);
    SetMethod(_state, "CanPierceImmuneAura", &SpellInfoCanPierceImmuneAura);
    SetMethod(_state, "CanDispelAura", &SpellInfoCanDispelAura);
    SetMethod(_state, "IsSingleTarget", &SpellInfoIsSingleTarget);
    SetMethod(_state, "IsAuraExclusiveBySpecificWith", &SpellInfoIsAuraExclusiveBySpecificWith);
    SetMethod(_state, "IsAuraExclusiveBySpecificPerCasterWith", &SpellInfoIsAuraExclusiveBySpecificPerCasterWith);
    SetMethod(_state, "CheckShapeshift", &SpellInfoCheckShapeshift);
    SetMethod(_state, "CheckLocation", &SpellInfoCheckLocation);
    SetMethod(_state, "CheckTarget", &SpellInfoCheckTarget);
    SetMethod(_state, "CheckExplicitTarget", &SpellInfoCheckExplicitTarget);
    SetMethod(_state, "CheckTargetCreatureType", &SpellInfoCheckTargetCreatureType);
    SetMethod(_state, "GetAllEffectsMechanicMask", &SpellInfoGetAllEffectsMechanicMask);
    SetMethod(_state, "GetEffectMechanicMask", &SpellInfoGetEffectMechanicMask);
    SetMethod(_state, "GetSpellMechanicMaskByEffectMask", &SpellInfoGetSpellMechanicMaskByEffectMask);
    SetMethod(_state, "GetDispelMask", &SpellInfoGetDispelMask);
    SetMethod(_state, "GetExplicitTargetMask", &SpellInfoGetExplicitTargetMask);
    SetMethod(_state, "GetAuraState", &SpellInfoGetAuraState);
    SetMethod(_state, "GetSpellSpecific", &SpellInfoGetSpellSpecific);
    lua_setfield(_state, -2, "__index");

    lua_pop(_state, 1);
}

void TurtleLuaEngine::RegisterSpellTargetsMetatable()
{
    luaL_newmetatable(_state, SPELLTARGETS_METATABLE);

    lua_newtable(_state);
    SetMethod(_state, "GetTargetMask", &SpellTargetsGetTargetMask);
    SetMethod(_state, "GetUnitTarget", &SpellTargetsGetUnitTarget);
    SetMethod(_state, "GetGameObjectTarget", &SpellTargetsGetGameObjectTarget);
    SetMethod(_state, "GetGOTarget", &SpellTargetsGetGameObjectTarget);
    SetMethod(_state, "GetItemTarget", &SpellTargetsGetItemTarget);
    SetMethod(_state, "GetUnitTargetGUID", &SpellTargetsGetUnitTargetGUID);
    SetMethod(_state, "GetUnitTargetGuid", &SpellTargetsGetUnitTargetGUID);
    SetMethod(_state, "GetUnitTargetGUIDLow", &SpellTargetsGetUnitTargetGUIDLow);
    SetMethod(_state, "GetUnitTargetGuidLow", &SpellTargetsGetUnitTargetGUIDLow);
    SetMethod(_state, "GetGameObjectTargetGUID", &SpellTargetsGetGameObjectTargetGUID);
    SetMethod(_state, "GetGameObjectTargetGuid", &SpellTargetsGetGameObjectTargetGUID);
    SetMethod(_state, "GetGameObjectTargetGUIDLow", &SpellTargetsGetGameObjectTargetGUIDLow);
    SetMethod(_state, "GetGameObjectTargetGuidLow", &SpellTargetsGetGameObjectTargetGUIDLow);
    SetMethod(_state, "GetGOTargetGUID", &SpellTargetsGetGameObjectTargetGUID);
    SetMethod(_state, "GetGOTargetGuid", &SpellTargetsGetGameObjectTargetGUID);
    SetMethod(_state, "GetGOTargetGUIDLow", &SpellTargetsGetGameObjectTargetGUIDLow);
    SetMethod(_state, "GetGOTargetGuidLow", &SpellTargetsGetGameObjectTargetGUIDLow);
    SetMethod(_state, "GetItemTargetGUID", &SpellTargetsGetItemTargetGUID);
    SetMethod(_state, "GetItemTargetGuid", &SpellTargetsGetItemTargetGUID);
    SetMethod(_state, "GetItemTargetGUIDLow", &SpellTargetsGetItemTargetGUIDLow);
    SetMethod(_state, "GetItemTargetGuidLow", &SpellTargetsGetItemTargetGUIDLow);
    SetMethod(_state, "GetItemTargetEntry", &SpellTargetsGetItemTargetEntry);
    SetMethod(_state, "GetSource", &SpellTargetsGetSource);
    SetMethod(_state, "GetDestination", &SpellTargetsGetDestination);
    SetMethod(_state, "GetStringTarget", &SpellTargetsGetStringTarget);
    SetMethod(_state, "IsEmpty", &SpellTargetsIsEmpty);
    lua_setfield(_state, -2, "__index");

    lua_pop(_state, 1);
}

void TurtleLuaEngine::RegisterWorldPacketMetatable()
{
    luaL_newmetatable(_state, WORLDPACKET_METATABLE);

    lua_newtable(_state);
    SetMethod(_state, "GetOpcode", &WorldPacketGetOpcode);
    SetMethod(_state, "GetSize", &WorldPacketGetSize);
    SetMethod(_state, "SetOpcode", &WorldPacketSetOpcode);
    SetMethod(_state, "ReadByte", &WorldPacketReadByte);
    SetMethod(_state, "ReadUByte", &WorldPacketReadUByte);
    SetMethod(_state, "ReadShort", &WorldPacketReadShort);
    SetMethod(_state, "ReadUShort", &WorldPacketReadUShort);
    SetMethod(_state, "ReadLong", &WorldPacketReadLong);
    SetMethod(_state, "ReadULong", &WorldPacketReadULong);
    SetMethod(_state, "ReadFloat", &WorldPacketReadFloat);
    SetMethod(_state, "ReadDouble", &WorldPacketReadDouble);
    SetMethod(_state, "ReadGUID", &WorldPacketReadGUID);
    SetMethod(_state, "ReadGuid", &WorldPacketReadGUID);
    SetMethod(_state, "ReadString", &WorldPacketReadString);
    SetMethod(_state, "WriteGUID", &WorldPacketWriteGUID);
    SetMethod(_state, "WriteGuid", &WorldPacketWriteGUID);
    SetMethod(_state, "WriteString", &WorldPacketWriteString);
    SetMethod(_state, "WriteByte", &WorldPacketWriteByte);
    SetMethod(_state, "WriteUByte", &WorldPacketWriteUByte);
    SetMethod(_state, "WriteShort", &WorldPacketWriteShort);
    SetMethod(_state, "WriteUShort", &WorldPacketWriteUShort);
    SetMethod(_state, "WriteLong", &WorldPacketWriteLong);
    SetMethod(_state, "WriteULong", &WorldPacketWriteULong);
    SetMethod(_state, "WriteFloat", &WorldPacketWriteFloat);
    SetMethod(_state, "WriteDouble", &WorldPacketWriteDouble);
    lua_setfield(_state, -2, "__index");

    SetMethod(_state, "__gc", &WorldPacketGC);

    lua_pop(_state, 1);
}

void TurtleLuaEngine::RegisterQueryMetatable()
{
    luaL_newmetatable(_state, ELUNAQUERY_METATABLE);

    lua_newtable(_state);
    SetMethod(_state, "IsNull", &QueryIsNull);
    SetMethod(_state, "IsNULL", &QueryIsNull);
    SetMethod(_state, "GetColumnCount", &QueryGetColumnCount);
    SetMethod(_state, "GetFieldCount", &QueryGetColumnCount);
    SetMethod(_state, "GetRowCount", &QueryGetRowCount);
    SetMethod(_state, "GetBool", &QueryGetBool);
    SetMethod(_state, "GetUInt8", &QueryGetUInt8);
    SetMethod(_state, "GetUInt16", &QueryGetUInt16);
    SetMethod(_state, "GetUInt32", &QueryGetUInt32);
    SetMethod(_state, "GetUInt64", &QueryGetUInt64);
    SetMethod(_state, "GetInt8", &QueryGetInt8);
    SetMethod(_state, "GetInt16", &QueryGetInt16);
    SetMethod(_state, "GetInt32", &QueryGetInt32);
    SetMethod(_state, "GetInt64", &QueryGetInt64);
    SetMethod(_state, "GetFloat", &QueryGetFloat);
    SetMethod(_state, "GetDouble", &QueryGetDouble);
    SetMethod(_state, "GetString", &QueryGetString);
    SetMethod(_state, "NextRow", &QueryNextRow);
    SetMethod(_state, "GetRow", &QueryGetRow);
    lua_setfield(_state, -2, "__index");

    SetMethod(_state, "__gc", &QueryGC);

    lua_pop(_state, 1);
}

void TurtleLuaEngine::RegisterObjectGuidMetatable()
{
    luaL_newmetatable(_state, OBJECTGUID_METATABLE);

    lua_newtable(_state);
    SetMethod(_state, "GetCounter", &ObjectGuidGetCounter);
    SetMethod(_state, "GetGUIDLow", &ObjectGuidGetCounter);
    SetMethod(_state, "GetGuidLow", &ObjectGuidGetCounter);
    SetMethod(_state, "GetEntry", &ObjectGuidGetEntry);
    SetMethod(_state, "GetHigh", &ObjectGuidGetHigh);
    SetMethod(_state, "GetHighGuid", &ObjectGuidGetHigh);
    SetMethod(_state, "GetTypeId", &ObjectGuidGetTypeId);
    SetMethod(_state, "GetTypeID", &ObjectGuidGetTypeId);
    SetMethod(_state, "GetMaxCounter", &ObjectGuidGetMaxCounter);
    SetMethod(_state, "GetTypeName", &ObjectGuidGetTypeName);
    SetMethod(_state, "GetString", &ObjectGuidGetString);
    SetMethod(_state, "ToString", &ObjectGuidGetString);
    SetMethod(_state, "GetRawValue", &ObjectGuidGetRawValueString);
    SetMethod(_state, "GetRawValueString", &ObjectGuidGetRawValueString);
    SetMethod(_state, "IsEmpty", &ObjectGuidIsEmpty);
    SetMethod(_state, "IsPlayer", &ObjectGuidIsPlayer);
    SetMethod(_state, "IsCreature", &ObjectGuidIsCreature);
    SetMethod(_state, "IsPet", &ObjectGuidIsPet);
    SetMethod(_state, "IsCreatureOrPet", &ObjectGuidIsCreatureOrPet);
    SetMethod(_state, "IsUnit", &ObjectGuidIsUnit);
    SetMethod(_state, "IsItem", &ObjectGuidIsItem);
    SetMethod(_state, "IsGameObject", &ObjectGuidIsGameObject);
    SetMethod(_state, "IsDynamicObject", &ObjectGuidIsDynamicObject);
    SetMethod(_state, "IsCorpse", &ObjectGuidIsCorpse);
    SetMethod(_state, "IsTransport", &ObjectGuidIsTransport);
    SetMethod(_state, "IsMOTransport", &ObjectGuidIsMOTransport);
    SetMethod(_state, "Equals", &ObjectGuidEquals);
    lua_setfield(_state, -2, "__index");

    SetMethod(_state, "__tostring", &ObjectGuidGetString);
    SetMethod(_state, "__eq", &ObjectGuidEquals);
    SetMethod(_state, "__gc", &ObjectGuidGC);

    lua_pop(_state, 1);
}

void TurtleLuaEngine::RegisterChatHandlerMetatable()
{
    luaL_newmetatable(_state, CHATHANDLER_METATABLE);

    lua_newtable(_state);
    SetMethod(_state, "SendSysMessage", &ChatHandlerSendSysMessage);
    SetMethod(_state, "IsConsole", &ChatHandlerIsConsole);
    SetMethod(_state, "GetPlayer", &ChatHandlerGetPlayer);
    SetMethod(_state, "SendGlobalSysMessage", &ChatHandlerSendGlobalSysMessage);
    SetMethod(_state, "SendGlobalGMSysMessage", &ChatHandlerSendGlobalGMSysMessage);
    SetMethod(_state, "HasLowerSecurity", &ChatHandlerHasLowerSecurity);
    SetMethod(_state, "HasLowerSecurityAccount", &ChatHandlerHasLowerSecurityAccount);
    SetMethod(_state, "GetSelectedPlayer", &ChatHandlerGetSelectedPlayer);
    SetMethod(_state, "GetSelectedCreature", &ChatHandlerGetSelectedCreature);
    SetMethod(_state, "GetSelectedUnit", &ChatHandlerGetSelectedUnit);
    SetMethod(_state, "GetSelectedObject", &ChatHandlerGetSelectedObject);
    SetMethod(_state, "GetSelectedPlayerOrSelf", &ChatHandlerGetSelectedPlayerOrSelf);
    SetMethod(_state, "IsAvailable", &ChatHandlerIsAvailable);
    SetMethod(_state, "HasSentErrorMessage", &ChatHandlerHasSentErrorMessage);
    lua_setfield(_state, -2, "__index");

    lua_pop(_state, 1);
}

void TurtleLuaEngine::RegisterChannelMetatable()
{
    luaL_newmetatable(_state, CHANNEL_METATABLE);

    lua_newtable(_state);
    SetMethod(_state, "GetName", &ChannelGetName);
    SetMethod(_state, "GetId", &ChannelGetId);
    SetMethod(_state, "GetChannelId", &ChannelGetId);
    SetMethod(_state, "GetChannelID", &ChannelGetId);
    SetMethod(_state, "GetChannelDBId", &ChannelGetId);
    SetMethod(_state, "GetChannelDBID", &ChannelGetId);
    SetMethod(_state, "IsValid", &ChannelIsValid);
    SetMethod(_state, "IsConstant", &ChannelIsConstant);
    SetMethod(_state, "IsAnnounce", &ChannelIsAnnounce);
    SetMethod(_state, "SetAnnounce", &ChannelSetAnnounce);
    SetMethod(_state, "IsLevelRestricted", &ChannelIsLevelRestricted);
    SetMethod(_state, "IsLFG", &ChannelIsLFG);
    SetMethod(_state, "GetPassword", &ChannelGetPassword);
    SetMethod(_state, "SetPassword", &ChannelSetPassword);
    SetMethod(_state, "GetNumPlayers", &ChannelGetNumPlayers);
    SetMethod(_state, "GetPlayerCount", &ChannelGetNumPlayers);
    SetMethod(_state, "GetFlags", &ChannelGetFlags);
    SetMethod(_state, "HasFlag", &ChannelHasFlag);
    SetMethod(_state, "GetSecurityLevel", &ChannelGetSecurityLevel);
    SetMethod(_state, "SetSecurityLevel", &ChannelSetSecurityLevel);
    SetMethod(_state, "GetTeam", &ChannelGetTeam);
    SetMethod(_state, "IsOn", &ChannelIsOn);
    SetMethod(_state, "IsBanned", &ChannelIsBanned);
    SetMethod(_state, "GetPlayerFlags", &ChannelGetPlayerFlags);
    SetMethod(_state, "SetModerator", &ChannelSetModerator);
    SetMethod(_state, "SetMute", &ChannelSetMute);
    SetMethod(_state, "SendPacket", &ChannelSendPacket);
    SetMethod(_state, "SendToAll", &ChannelSendPacket);
    SetMethod(_state, "SendToOne", &ChannelSendToOne);
    SetMethod(_state, "Say", &ChannelSay);
    SetMethod(_state, "SendMessage", &ChannelSay);
    lua_setfield(_state, -2, "__index");

    SetMethod(_state, "__tostring", &ChannelGetString);
    SetMethod(_state, "__gc", &ChannelGC);

    lua_pop(_state, 1);
}

void TurtleLuaEngine::RegisterRollMetatable()
{
    luaL_newmetatable(_state, ROLL_METATABLE);

    lua_newtable(_state);
    SetMethod(_state, "GetItemGUID", &RollGetItemGUID);
    SetMethod(_state, "GetItemGuid", &RollGetItemGUID);
    SetMethod(_state, "GetItemGUIDLow", &RollGetItemGUID);
    SetMethod(_state, "GetItemGuidLow", &RollGetItemGUID);
    SetMethod(_state, "GetItemObjectGuid", &RollGetItemGUIDObject);
    SetMethod(_state, "GetItemObjectGUID", &RollGetItemGUIDObject);
    SetMethod(_state, "GetLootedTargetGUID", &RollGetLootedTargetGUID);
    SetMethod(_state, "GetLootedTargetGuid", &RollGetLootedTargetGUID);
    SetMethod(_state, "GetLootSourceGUID", &RollGetLootedTargetGUID);
    SetMethod(_state, "GetLootSourceGuid", &RollGetLootedTargetGUID);
    SetMethod(_state, "GetItemId", &RollGetItemId);
    SetMethod(_state, "GetItemEntry", &RollGetItemId);
    SetMethod(_state, "GetItemRandomPropId", &RollGetItemRandomPropId);
    SetMethod(_state, "GetItemRandomSuffix", &RollGetItemRandomSuffix);
    SetMethod(_state, "GetItemCount", &RollGetItemCount);
    SetMethod(_state, "GetPlayerVote", &RollGetPlayerVote);
    SetMethod(_state, "GetPlayerVoteGUIDs", &RollGetPlayerVoteGUIDs);
    SetMethod(_state, "GetPlayerVoteGuids", &RollGetPlayerVoteGUIDs);
    SetMethod(_state, "GetTotalPlayersRolling", &RollGetTotalPlayersRolling);
    SetMethod(_state, "GetTotalNeed", &RollGetTotalNeed);
    SetMethod(_state, "GetTotalGreed", &RollGetTotalGreed);
    SetMethod(_state, "GetTotalPass", &RollGetTotalPass);
    SetMethod(_state, "GetItemSlot", &RollGetItemSlot);
    SetMethod(_state, "GetRollVoteMask", &RollGetRollVoteMask);
    lua_setfield(_state, -2, "__index");

    SetMethod(_state, "__gc", &RollGC);

    lua_pop(_state, 1);
}

void TurtleLuaEngine::RegisterUnitMetatable()
{
}

void TurtleLuaEngine::LoadScripts()
{
    if (!_state)
        return;

    namespace fs = std::filesystem;

    std::error_code ec;
    if (!fs::exists(_scriptPath, ec))
    {
        fs::create_directories(_scriptPath, ec);
        sLog.outString("[Lua] Script directory '%s' created. No Lua scripts loaded.", _scriptPath.c_str());
        return;
    }

    uint32 count = 0;
    for (auto const& entry : fs::recursive_directory_iterator(_scriptPath, ec))
    {
        if (ec)
            break;

        if (!entry.is_regular_file())
            continue;

        if (entry.path().extension() != ".lua")
            continue;

        LoadScriptFile(entry.path().string());
        ++count;
    }

    sLog.outString("[Lua] Loaded %u Lua script%s from '%s'.", count, count == 1 ? "" : "s", _scriptPath.c_str());
}

void TurtleLuaEngine::LoadScriptFile(std::string const& path)
{
    if (luaL_loadfile(_state, path.c_str()) != LUA_OK)
    {
        LogError(path.c_str());
        lua_pop(_state, 1);
        return;
    }

    if (lua_pcall(_state, 0, 0, 0) != LUA_OK)
    {
        LogError(path.c_str());
        lua_pop(_state, 1);
    }
}

void TurtleLuaEngine::RegisterPlayerEvent(uint32 eventId, int functionRef)
{
    _playerEvents[eventId].push_back(functionRef);
}

void TurtleLuaEngine::RegisterServerEvent(uint32 eventId, int functionRef)
{
    _serverEvents[eventId].push_back(functionRef);
}

void TurtleLuaEngine::RegisterGroupEvent(uint32 eventId, int functionRef)
{
    _groupEvents[eventId].push_back(functionRef);
}

void TurtleLuaEngine::RegisterGuildEvent(uint32 eventId, int functionRef)
{
    _guildEvents[eventId].push_back(functionRef);
}

void TurtleLuaEngine::RegisterBattleGroundEvent(uint32 eventId, int functionRef)
{
    _battleGroundEvents[eventId].push_back(functionRef);
}

void TurtleLuaEngine::RegisterTicketEvent(uint32 eventId, int functionRef)
{
    _ticketEvents[eventId].push_back(functionRef);
}

void TurtleLuaEngine::RegisterMapEvent(uint32 mapId, uint32 eventId, int functionRef)
{
    _mapEvents[mapId][eventId].push_back(functionRef);
}

void TurtleLuaEngine::RegisterInstanceEvent(uint32 instanceId, uint32 eventId, int functionRef)
{
    _instanceEvents[instanceId][eventId].push_back(functionRef);
}

void TurtleLuaEngine::RegisterCreatureEvent(uint32 entry, uint32 eventId, int functionRef)
{
    _creatureEvents[entry][eventId].push_back(functionRef);
}

void TurtleLuaEngine::RegisterUniqueCreatureEvent(ObjectGuid const& guid, uint32 instanceId, uint32 eventId, int functionRef)
{
    _uniqueCreatureEvents[UniqueCreatureEventKey(guid, instanceId)][eventId].push_back(functionRef);
}

void TurtleLuaEngine::RegisterGameObjectEvent(uint32 entry, uint32 eventId, int functionRef)
{
    _gameObjectEvents[entry][eventId].push_back(functionRef);
}

void TurtleLuaEngine::RegisterItemEvent(uint32 entry, uint32 eventId, int functionRef)
{
    _itemEvents[entry][eventId].push_back(functionRef);
}

void TurtleLuaEngine::RegisterSpellEvent(uint32 entry, uint32 eventId, int functionRef)
{
    _spellEvents[entry][eventId].push_back(functionRef);
}

void TurtleLuaEngine::RegisterPacketEvent(uint32 opcode, uint32 eventId, int functionRef)
{
    _packetEvents[opcode][eventId].push_back(functionRef);
}

void TurtleLuaEngine::RegisterCreatureGossipEvent(uint32 entry, uint32 eventId, int functionRef)
{
    _creatureGossipEvents[entry][eventId].push_back(functionRef);
}

void TurtleLuaEngine::RegisterGameObjectGossipEvent(uint32 entry, uint32 eventId, int functionRef)
{
    _gameObjectGossipEvents[entry][eventId].push_back(functionRef);
}

void TurtleLuaEngine::RegisterItemGossipEvent(uint32 entry, uint32 eventId, int functionRef)
{
    _itemGossipEvents[entry][eventId].push_back(functionRef);
}

void TurtleLuaEngine::RegisterPlayerGossipEvent(uint32 entry, uint32 eventId, int functionRef)
{
    _playerGossipEvents[entry][eventId].push_back(functionRef);
}

void TurtleLuaEngine::ClearPlayerEvents(uint32 eventId, bool allEvents)
{
    ClearEventStore(_state, _playerEvents, eventId, allEvents);
}

void TurtleLuaEngine::ClearServerEvents(uint32 eventId, bool allEvents)
{
    ClearEventStore(_state, _serverEvents, eventId, allEvents);
}

void TurtleLuaEngine::ClearGroupEvents(uint32 eventId, bool allEvents)
{
    ClearEventStore(_state, _groupEvents, eventId, allEvents);
}

void TurtleLuaEngine::ClearGuildEvents(uint32 eventId, bool allEvents)
{
    ClearEventStore(_state, _guildEvents, eventId, allEvents);
}

void TurtleLuaEngine::ClearBattleGroundEvents(uint32 eventId, bool allEvents)
{
    ClearEventStore(_state, _battleGroundEvents, eventId, allEvents);
}

void TurtleLuaEngine::ClearTicketEvents(uint32 eventId, bool allEvents)
{
    ClearEventStore(_state, _ticketEvents, eventId, allEvents);
}

void TurtleLuaEngine::ClearMapEvents(uint32 mapId, uint32 eventId, bool allEvents)
{
    ClearEntryEventStore(_state, _mapEvents, mapId, eventId, allEvents);
}

void TurtleLuaEngine::ClearInstanceEvents(uint32 instanceId, uint32 eventId, bool allEvents)
{
    ClearEntryEventStore(_state, _instanceEvents, instanceId, eventId, allEvents);
}

void TurtleLuaEngine::ClearCreatureEvents(uint32 entry, uint32 eventId, bool allEvents)
{
    ClearEntryEventStore(_state, _creatureEvents, entry, eventId, allEvents);
}

void TurtleLuaEngine::ClearUniqueCreatureEvents(ObjectGuid const& guid, uint32 instanceId, uint32 eventId, bool allEvents)
{
    auto key = UniqueCreatureEventKey(guid, instanceId);
    auto entryItr = _uniqueCreatureEvents.find(key);
    if (entryItr == _uniqueCreatureEvents.end())
        return;

    if (allEvents)
    {
        for (auto& eventPair : entryItr->second)
            UnrefFunctionRefs(_state, eventPair.second);

        _uniqueCreatureEvents.erase(entryItr);
        return;
    }

    auto eventItr = entryItr->second.find(eventId);
    if (eventItr == entryItr->second.end())
        return;

    UnrefFunctionRefs(_state, eventItr->second);
    entryItr->second.erase(eventItr);

    if (entryItr->second.empty())
        _uniqueCreatureEvents.erase(entryItr);
}

void TurtleLuaEngine::ClearGameObjectEvents(uint32 entry, uint32 eventId, bool allEvents)
{
    ClearEntryEventStore(_state, _gameObjectEvents, entry, eventId, allEvents);
}

void TurtleLuaEngine::ClearItemEvents(uint32 entry, uint32 eventId, bool allEvents)
{
    ClearEntryEventStore(_state, _itemEvents, entry, eventId, allEvents);
}

void TurtleLuaEngine::ClearSpellEvents(uint32 entry, uint32 eventId, bool allEvents)
{
    ClearEntryEventStore(_state, _spellEvents, entry, eventId, allEvents);
}

void TurtleLuaEngine::ClearPacketEvents(uint32 opcode, uint32 eventId, bool allEvents)
{
    ClearEntryEventStore(_state, _packetEvents, opcode, eventId, allEvents);
}

void TurtleLuaEngine::ClearCreatureGossipEvents(uint32 entry, uint32 eventId, bool allEvents)
{
    ClearEntryEventStore(_state, _creatureGossipEvents, entry, eventId, allEvents);
}

void TurtleLuaEngine::ClearGameObjectGossipEvents(uint32 entry, uint32 eventId, bool allEvents)
{
    ClearEntryEventStore(_state, _gameObjectGossipEvents, entry, eventId, allEvents);
}

void TurtleLuaEngine::ClearItemGossipEvents(uint32 entry, uint32 eventId, bool allEvents)
{
    ClearEntryEventStore(_state, _itemGossipEvents, entry, eventId, allEvents);
}

void TurtleLuaEngine::ClearPlayerGossipEvents(uint32 entry, uint32 eventId, bool allEvents)
{
    ClearEntryEventStore(_state, _playerGossipEvents, entry, eventId, allEvents);
}

uint32 TurtleLuaEngine::CreateTimedEvent(int functionRef, uint32 delay, uint32 repeats)
{
    TimedEvent event;
    event.id = _nextTimedEventId++;
    if (_nextTimedEventId == 0)
        _nextTimedEventId = 1;

    event.functionRef = functionRef;
    event.minDelay = delay ? delay : 1;
    event.maxDelay = event.minDelay;
    event.delay = event.minDelay;
    event.elapsed = 0;
    event.remaining = repeats ? static_cast<int32>(repeats) : -1;
    event.hasObject = false;
    event.objectGuid.Clear();
    event.mapId = 0;
    event.instanceId = 0;
    _timedEvents.push_back(event);
    return event.id;
}

uint32 TurtleLuaEngine::CreateTimedEvent(int functionRef, uint32 minDelay, uint32 maxDelay, uint32 repeats, WorldObject* object)
{
    TimedEvent event;
    event.id = _nextTimedEventId++;
    if (_nextTimedEventId == 0)
        _nextTimedEventId = 1;

    event.functionRef = functionRef;
    event.minDelay = minDelay;
    event.maxDelay = maxDelay;
    if (event.minDelay == 0 && event.maxDelay == 0)
    {
        event.minDelay = 1;
        event.maxDelay = 1;
    }
    event.delay = GenerateTimedEventDelay(event);
    event.elapsed = 0;
    event.remaining = repeats ? static_cast<int32>(repeats) : -1;
    event.hasObject = object != nullptr;
    event.objectGuid = object ? object->GetObjectGuid() : ObjectGuid();
    event.mapId = object ? object->GetMapId() : 0;
    event.instanceId = object ? object->GetInstanceId() : 0;
    _timedEvents.push_back(event);
    return event.id;
}

bool TurtleLuaEngine::RemoveTimedEvent(uint32 eventId)
{
    auto itr = std::find_if(_timedEvents.begin(), _timedEvents.end(), [eventId](TimedEvent const& event)
    {
        return event.id == eventId;
    });

    if (itr == _timedEvents.end())
        return false;

    if (_state)
        luaL_unref(_state, LUA_REGISTRYINDEX, itr->functionRef);

    _timedEvents.erase(itr);
    return true;
}

bool TurtleLuaEngine::RemoveTimedEventForObject(uint32 eventId, ObjectGuid const& objectGuid)
{
    auto itr = std::find_if(_timedEvents.begin(), _timedEvents.end(), [eventId, &objectGuid](TimedEvent const& event)
    {
        return event.id == eventId && event.hasObject && event.objectGuid == objectGuid;
    });

    if (itr == _timedEvents.end())
        return false;

    if (_state)
        luaL_unref(_state, LUA_REGISTRYINDEX, itr->functionRef);

    _timedEvents.erase(itr);
    return true;
}

uint32 TurtleLuaEngine::RemoveTimedEventsForObject(ObjectGuid const& objectGuid)
{
    uint32 removed = 0;
    for (auto itr = _timedEvents.begin(); itr != _timedEvents.end();)
    {
        if (itr->hasObject && itr->objectGuid == objectGuid)
        {
            if (_state)
                luaL_unref(_state, LUA_REGISTRYINDEX, itr->functionRef);
            itr = _timedEvents.erase(itr);
            ++removed;
        }
        else
            ++itr;
    }

    return removed;
}

uint32 TurtleLuaEngine::RemoveTimedEvents(bool allEvents)
{
    uint32 removed = 0;
    for (auto itr = _timedEvents.begin(); itr != _timedEvents.end();)
    {
        if (allEvents || !itr->hasObject)
        {
            if (_state)
                luaL_unref(_state, LUA_REGISTRYINDEX, itr->functionRef);
            itr = _timedEvents.erase(itr);
            ++removed;
        }
        else
            ++itr;
    }

    return removed;
}

uint32 TurtleLuaEngine::GenerateTimedEventDelay(TimedEvent const& event) const
{
    uint32 delay = event.minDelay == event.maxDelay ? event.minDelay : urand(event.minDelay, event.maxDelay);
    return delay ? delay : 1;
}

WorldObject* TurtleLuaEngine::ResolveTimedEventObject(TimedEvent const& event)
{
    if (!event.hasObject || event.objectGuid.IsEmpty())
        return nullptr;

    if (event.objectGuid.IsPlayer())
    {
        Player* player = ObjectAccessor::FindPlayer(event.objectGuid);
        return player && player->IsInWorld() ? player : nullptr;
    }

    Map* map = sMapMgr.FindMap(event.mapId, event.instanceId);
    if (!map)
        return nullptr;

    WorldObject* object = map->GetWorldObjectOrPlayer(event.objectGuid);
    return object && object->IsInWorld() ? object : nullptr;
}

void TurtleLuaEngine::PushPlayer(Player* player)
{
    if (!player)
    {
        lua_pushnil(_state);
        return;
    }

    auto* holder = static_cast<LuaPlayer*>(lua_newuserdata(_state, sizeof(LuaPlayer)));
    holder->player = player;

    luaL_getmetatable(_state, PLAYER_METATABLE);
    lua_setmetatable(_state, -2);
}

void TurtleLuaEngine::PushCreature(Creature* creature)
{
    if (!creature)
    {
        lua_pushnil(_state);
        return;
    }

    auto* holder = static_cast<LuaCreature*>(lua_newuserdata(_state, sizeof(LuaCreature)));
    holder->creature = creature;

    luaL_getmetatable(_state, CREATURE_METATABLE);
    lua_setmetatable(_state, -2);
}

void TurtleLuaEngine::PushGameObject(GameObject* go)
{
    if (!go)
    {
        lua_pushnil(_state);
        return;
    }

    auto* holder = static_cast<LuaGameObject*>(lua_newuserdata(_state, sizeof(LuaGameObject)));
    holder->go = go;

    luaL_getmetatable(_state, GAMEOBJECT_METATABLE);
    lua_setmetatable(_state, -2);
}

void TurtleLuaEngine::PushCorpse(Corpse* corpse)
{
    if (!corpse)
    {
        lua_pushnil(_state);
        return;
    }

    auto* holder = static_cast<LuaCorpse*>(lua_newuserdata(_state, sizeof(LuaCorpse)));
    holder->corpse = corpse;

    luaL_getmetatable(_state, CORPSE_METATABLE);
    lua_setmetatable(_state, -2);
}

void TurtleLuaEngine::PushDynamicObject(DynamicObject* dynObj)
{
    if (!dynObj)
    {
        lua_pushnil(_state);
        return;
    }

    auto* holder = static_cast<LuaDynamicObject*>(lua_newuserdata(_state, sizeof(LuaDynamicObject)));
    holder->dynObj = dynObj;

    luaL_getmetatable(_state, DYNAMICOBJECT_METATABLE);
    lua_setmetatable(_state, -2);
}

void TurtleLuaEngine::PushItem(Item* item)
{
    if (!item)
    {
        lua_pushnil(_state);
        return;
    }

    auto* holder = static_cast<LuaItem*>(lua_newuserdata(_state, sizeof(LuaItem)));
    holder->item = item;

    luaL_getmetatable(_state, ITEM_METATABLE);
    lua_setmetatable(_state, -2);
}

void TurtleLuaEngine::PushGroup(Group* group)
{
    if (!group)
    {
        lua_pushnil(_state);
        return;
    }

    auto* holder = static_cast<LuaGroup*>(lua_newuserdata(_state, sizeof(LuaGroup)));
    holder->group = group;

    luaL_getmetatable(_state, GROUP_METATABLE);
    lua_setmetatable(_state, -2);
}

void TurtleLuaEngine::PushGuild(Guild* guild)
{
    if (!guild)
    {
        lua_pushnil(_state);
        return;
    }

    auto* holder = static_cast<LuaGuild*>(lua_newuserdata(_state, sizeof(LuaGuild)));
    holder->guild = guild;

    luaL_getmetatable(_state, GUILD_METATABLE);
    lua_setmetatable(_state, -2);
}

void TurtleLuaEngine::PushMap(Map* map)
{
    if (!map)
    {
        lua_pushnil(_state);
        return;
    }

    auto* holder = static_cast<LuaMap*>(lua_newuserdata(_state, sizeof(LuaMap)));
    holder->map = map;

    luaL_getmetatable(_state, MAP_METATABLE);
    lua_setmetatable(_state, -2);
}

void TurtleLuaEngine::PushBattleGround(BattleGround* bg)
{
    PushBattleGroundValue(_state, bg);
}

void TurtleLuaEngine::PushTicket(GmTicket* ticket)
{
    PushTicketValue(_state, ticket);
}

void TurtleLuaEngine::PushSpell(Spell* spell)
{
    if (!spell)
    {
        lua_pushnil(_state);
        return;
    }

    auto* holder = static_cast<LuaSpell*>(lua_newuserdata(_state, sizeof(LuaSpell)));
    holder->spell = spell;

    luaL_getmetatable(_state, SPELL_METATABLE);
    lua_setmetatable(_state, -2);
}

void TurtleLuaEngine::PushObjectGuid(ObjectGuid const& guid)
{
    PushObjectGuidValue(_state, guid);
}

void TurtleLuaEngine::PushUnit(Unit* unit)
{
    if (!unit)
    {
        lua_pushnil(_state);
        return;
    }

    if (Player* player = dynamic_cast<Player*>(unit))
        PushPlayer(player);
    else if (Creature* creature = dynamic_cast<Creature*>(unit))
        PushCreature(creature);
    else
        lua_pushnil(_state);
}

bool TurtleLuaEngine::CallPlayerEvent(uint32 eventId, Player* player)
{
    auto itr = _playerEvents.find(eventId);
    if (itr == _playerEvents.end())
        return true;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, eventId);
        PushPlayer(player);

        if (lua_pcall(_state, 2, 0, 0) != LUA_OK)
        {
            LogError("player event");
            lua_pop(_state, 1);
        }
    }

    return true;
}

bool TurtleLuaEngine::CallPacketFunctionRefs(std::vector<int> const& functionRefs, uint32 eventId, WorldPacket& packet, Player* player, char const* context)
{
    bool allow = true;

    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, eventId);
        PushWorldPacketValue(_state, new WorldPacket(packet), true);
        PushPlayer(player);

        if (lua_pcall(_state, 3, 2, 0) != LUA_OK)
        {
            LogError(context);
            lua_pop(_state, 1);
            continue;
        }

        if (lua_isboolean(_state, -2) && !lua_toboolean(_state, -2))
            allow = false;

        if (auto* holder = static_cast<LuaWorldPacket*>(luaL_testudata(_state, -1, WORLDPACKET_METATABLE)))
        {
            if (holder->packet)
                packet = *holder->packet;
        }

        lua_pop(_state, 2);
    }

    return allow;
}

bool TurtleLuaEngine::CallServerPacketEvent(uint32 eventId, WorldPacket& packet, Player* player, char const* context)
{
    auto itr = _serverEvents.find(eventId);
    if (itr == _serverEvents.end())
        return true;

    std::vector<int> functionRefs = itr->second;
    return CallPacketFunctionRefs(functionRefs, eventId, packet, player, context);
}

bool TurtleLuaEngine::CallPacketEvent(uint32 opcode, uint32 eventId, WorldPacket& packet, Player* player, char const* context)
{
    auto opcodeItr = _packetEvents.find(opcode);
    if (opcodeItr == _packetEvents.end())
        return true;

    auto eventItr = opcodeItr->second.find(eventId);
    if (eventItr == opcodeItr->second.end())
        return true;

    std::vector<int> functionRefs = eventItr->second;
    return CallPacketFunctionRefs(functionRefs, eventId, packet, player, context);
}

void TurtleLuaEngine::CallServerEvent(uint32 eventId)
{
    auto itr = _serverEvents.find(eventId);
    if (itr == _serverEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, eventId);

        if (lua_pcall(_state, 1, 0, 0) != LUA_OK)
        {
            LogError("server event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::CallServerEvent(uint32 eventId, uint32 arg)
{
    auto itr = _serverEvents.find(eventId);
    if (itr == _serverEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, eventId);
        lua_pushinteger(_state, arg);

        if (lua_pcall(_state, 2, 0, 0) != LUA_OK)
        {
            LogError("server event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::CallServerMapEvent(uint32 eventId, Map* map, Player* player, uint32 diff)
{
    auto itr = _serverEvents.find(eventId);
    if (itr == _serverEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, eventId);
        PushMap(map);

        int argCount = 2;
        if (eventId == MAP_EVENT_ON_PLAYER_ENTER || eventId == MAP_EVENT_ON_PLAYER_LEAVE)
        {
            PushPlayer(player);
            argCount = 3;
        }
        else if (eventId == MAP_EVENT_ON_UPDATE)
        {
            lua_pushinteger(_state, diff);
            argCount = 3;
        }

        if (lua_pcall(_state, argCount, 0, 0) != LUA_OK)
        {
            LogError("map server event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::CallBattleGroundEvent(uint32 eventId, BattleGround* /*bg*/, int argCount)
{
    int base = lua_gettop(_state) - argCount;
    auto itr = _battleGroundEvents.find(eventId);
    if (itr == _battleGroundEvents.end())
    {
        lua_settop(_state, base);
        return;
    }

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        for (int i = 1; i <= argCount; ++i)
            lua_pushvalue(_state, base + i);

        if (lua_pcall(_state, argCount, 0, 0) != LUA_OK)
        {
            LogError("battleground event");
            lua_pop(_state, 1);
        }
    }

    lua_settop(_state, base);
}

void TurtleLuaEngine::CallTicketEvent(uint32 eventId, GmTicket* /*ticket*/, int argCount)
{
    int base = lua_gettop(_state) - argCount;
    auto itr = _ticketEvents.find(eventId);
    if (itr == _ticketEvents.end())
    {
        lua_settop(_state, base);
        return;
    }

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        for (int i = 1; i <= argCount; ++i)
            lua_pushvalue(_state, base + i);

        if (lua_pcall(_state, argCount, 0, 0) != LUA_OK)
        {
            LogError("ticket event");
            lua_pop(_state, 1);
        }
    }

    lua_settop(_state, base);
}

std::vector<int> TurtleLuaEngine::CollectInstanceEventRefs(Map* map, uint32 eventId)
{
    std::vector<int> refs;
    if (!map)
        return refs;

    auto mapItr = _mapEvents.find(map->GetId());
    if (mapItr != _mapEvents.end())
    {
        auto eventItr = mapItr->second.find(eventId);
        if (eventItr != mapItr->second.end())
            refs.insert(refs.end(), eventItr->second.begin(), eventItr->second.end());
    }

    auto instanceItr = _instanceEvents.find(map->GetInstanceId());
    if (instanceItr != _instanceEvents.end())
    {
        auto eventItr = instanceItr->second.find(eventId);
        if (eventItr != instanceItr->second.end())
            refs.insert(refs.end(), eventItr->second.begin(), eventItr->second.end());
    }

    return refs;
}

bool TurtleLuaEngine::CallInstanceEvent(Map* map, uint32 eventId, int argCount)
{
    int base = lua_gettop(_state) - argCount;
    std::vector<int> functionRefs = CollectInstanceEventRefs(map, eventId);
    if (functionRefs.empty())
    {
        lua_settop(_state, base);
        return false;
    }

    bool handled = true;

    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        for (int i = 1; i <= argCount; ++i)
            lua_pushvalue(_state, base + i);

        if (lua_pcall(_state, argCount, LUA_MULTRET, 0) != LUA_OK)
        {
            LogError("instance event");
            lua_pop(_state, 1);
            continue;
        }

        int results = lua_gettop(_state) - base - argCount;
        int firstResult = base + argCount + 1;
        if (results >= 1 && lua_isboolean(_state, firstResult) && !lua_toboolean(_state, firstResult))
            handled = false;

        lua_settop(_state, base + argCount);

        if (!handled)
            break;
    }

    lua_settop(_state, base);
    return handled;
}

void TurtleLuaEngine::OnMapCreate(Map* map)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !map)
        return;

    CallServerMapEvent(MAP_EVENT_ON_CREATE, map, nullptr, 0);
}

void TurtleLuaEngine::OnMapDestroy(Map* map)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !map)
        return;

    CallServerMapEvent(MAP_EVENT_ON_DESTROY, map, nullptr, 0);
}

void TurtleLuaEngine::OnMapPlayerEnter(Map* map, Player* player)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !map || !player)
        return;

    CallServerMapEvent(MAP_EVENT_ON_PLAYER_ENTER, map, player, 0);
}

void TurtleLuaEngine::OnMapPlayerLeave(Map* map, Player* player)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !map || !player)
        return;

    CallServerMapEvent(MAP_EVENT_ON_PLAYER_LEAVE, map, player, 0);
}

void TurtleLuaEngine::OnMapUpdate(Map* map, uint32 diff)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !map)
        return;

    CallServerMapEvent(MAP_EVENT_ON_UPDATE, map, nullptr, diff);
}

void TurtleLuaEngine::OnInstanceInitialize(Map* map)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !map || !map->GetInstanceData())
        return;

    lua_pushinteger(_state, INSTANCE_EVENT_ON_INITIALIZE);
    PushInstanceDataValue(_state, map->GetInstanceData());
    PushMap(map);
    CallInstanceEvent(map, INSTANCE_EVENT_ON_INITIALIZE, 3);
}

void TurtleLuaEngine::OnInstanceLoad(Map* map)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !map || !map->GetInstanceData())
        return;

    lua_pushinteger(_state, INSTANCE_EVENT_ON_LOAD);
    PushInstanceDataValue(_state, map->GetInstanceData());
    PushMap(map);
    CallInstanceEvent(map, INSTANCE_EVENT_ON_LOAD, 3);
}

void TurtleLuaEngine::OnInstanceUpdate(Map* map, uint32 diff)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !map || !map->GetInstanceData())
        return;

    lua_pushinteger(_state, INSTANCE_EVENT_ON_UPDATE);
    PushInstanceDataValue(_state, map->GetInstanceData());
    PushMap(map);
    lua_pushinteger(_state, diff);
    CallInstanceEvent(map, INSTANCE_EVENT_ON_UPDATE, 4);
}

void TurtleLuaEngine::OnInstancePlayerEnter(Map* map, Player* player)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !map || !map->GetInstanceData() || !player)
        return;

    lua_pushinteger(_state, INSTANCE_EVENT_ON_PLAYER_ENTER);
    PushInstanceDataValue(_state, map->GetInstanceData());
    PushMap(map);
    PushPlayer(player);
    CallInstanceEvent(map, INSTANCE_EVENT_ON_PLAYER_ENTER, 4);
}

void TurtleLuaEngine::OnBattleGroundCreate(BattleGround* bg)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !bg || !bg->GetInstanceID())
        return;

    lua_pushinteger(_state, BG_EVENT_ON_CREATE);
    PushBattleGround(bg);
    lua_pushinteger(_state, bg->GetTypeID());
    lua_pushinteger(_state, bg->GetInstanceID());
    CallBattleGroundEvent(BG_EVENT_ON_CREATE, bg, 4);
}

void TurtleLuaEngine::OnBattleGroundStart(BattleGround* bg)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !bg || !bg->GetInstanceID())
        return;

    lua_pushinteger(_state, BG_EVENT_ON_START);
    PushBattleGround(bg);
    lua_pushinteger(_state, bg->GetTypeID());
    lua_pushinteger(_state, bg->GetInstanceID());
    CallBattleGroundEvent(BG_EVENT_ON_START, bg, 4);
}

void TurtleLuaEngine::OnBattleGroundEnd(BattleGround* bg, uint32 winner)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !bg || !bg->GetInstanceID())
        return;

    lua_pushinteger(_state, BG_EVENT_ON_END);
    PushBattleGround(bg);
    lua_pushinteger(_state, bg->GetTypeID());
    lua_pushinteger(_state, bg->GetInstanceID());
    lua_pushinteger(_state, winner);
    CallBattleGroundEvent(BG_EVENT_ON_END, bg, 5);
}

void TurtleLuaEngine::OnBattleGroundPreDestroy(BattleGround* bg)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !bg || !bg->GetInstanceID())
        return;

    lua_pushinteger(_state, BG_EVENT_ON_PRE_DESTROY);
    PushBattleGround(bg);
    lua_pushinteger(_state, bg->GetTypeID());
    lua_pushinteger(_state, bg->GetInstanceID());
    CallBattleGroundEvent(BG_EVENT_ON_PRE_DESTROY, bg, 4);
}

void TurtleLuaEngine::OnTicketCreate(GmTicket* ticket)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !ticket)
        return;

    lua_pushinteger(_state, TICKET_EVENT_ON_CREATE);
    PushTicket(ticket);
    CallTicketEvent(TICKET_EVENT_ON_CREATE, ticket, 2);
}

void TurtleLuaEngine::OnTicketUpdateLastChange(GmTicket* ticket, std::string const& message)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !ticket)
        return;

    lua_pushinteger(_state, TICKET_EVENT_UPDATE_LAST_CHANGE);
    PushTicket(ticket);
    lua_pushstring(_state, message.c_str());
    CallTicketEvent(TICKET_EVENT_UPDATE_LAST_CHANGE, ticket, 3);
}

void TurtleLuaEngine::OnTicketClose(GmTicket* ticket)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !ticket)
        return;

    lua_pushinteger(_state, TICKET_EVENT_ON_CLOSE);
    PushTicket(ticket);
    CallTicketEvent(TICKET_EVENT_ON_CLOSE, ticket, 2);
}

void TurtleLuaEngine::OnTicketResolve(GmTicket* ticket)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !ticket)
        return;

    lua_pushinteger(_state, TICKET_EVENT_ON_RESOLVE);
    PushTicket(ticket);
    CallTicketEvent(TICKET_EVENT_ON_RESOLVE, ticket, 2);
}

void TurtleLuaEngine::OnGroupCreate(Group* group, ObjectGuid const& leaderGuid, uint32 groupType)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);
    if (!IsEnabled() || !group)
        return;

    auto itr = _groupEvents.find(GROUP_EVENT_ON_CREATE);
    if (itr == _groupEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, GROUP_EVENT_ON_CREATE);
        PushGroup(group);
        PushObjectGuid(leaderGuid);
        lua_pushinteger(_state, groupType);

        if (lua_pcall(_state, 4, 0, 0) != LUA_OK)
        {
            LogError("group create event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnGroupMemberAdd(Group* group, ObjectGuid const& guid)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);
    if (!IsEnabled() || !group)
        return;

    auto itr = _groupEvents.find(GROUP_EVENT_ON_MEMBER_ADD);
    if (itr == _groupEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, GROUP_EVENT_ON_MEMBER_ADD);
        PushGroup(group);
        PushObjectGuid(guid);

        if (lua_pcall(_state, 3, 0, 0) != LUA_OK)
        {
            LogError("group member add event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnGroupMemberInvite(Group* group, ObjectGuid const& guid)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);
    if (!IsEnabled() || !group)
        return;

    auto itr = _groupEvents.find(GROUP_EVENT_ON_MEMBER_INVITE);
    if (itr == _groupEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, GROUP_EVENT_ON_MEMBER_INVITE);
        PushGroup(group);
        PushObjectGuid(guid);

        if (lua_pcall(_state, 3, 0, 0) != LUA_OK)
        {
            LogError("group member invite event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnGroupMemberRemove(Group* group, ObjectGuid const& guid, uint8 removeMethod)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);
    if (!IsEnabled() || !group)
        return;

    auto itr = _groupEvents.find(GROUP_EVENT_ON_MEMBER_REMOVE);
    if (itr == _groupEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, GROUP_EVENT_ON_MEMBER_REMOVE);
        PushGroup(group);
        PushObjectGuid(guid);
        lua_pushinteger(_state, removeMethod);
        lua_pushnil(_state);
        lua_pushnil(_state);

        if (lua_pcall(_state, 6, 0, 0) != LUA_OK)
        {
            LogError("group member remove event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnGroupLeaderChange(Group* group, ObjectGuid const& newLeaderGuid, ObjectGuid const& oldLeaderGuid)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);
    if (!IsEnabled() || !group)
        return;

    auto itr = _groupEvents.find(GROUP_EVENT_ON_LEADER_CHANGE);
    if (itr == _groupEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, GROUP_EVENT_ON_LEADER_CHANGE);
        PushGroup(group);
        PushObjectGuid(newLeaderGuid);
        PushObjectGuid(oldLeaderGuid);

        if (lua_pcall(_state, 4, 0, 0) != LUA_OK)
        {
            LogError("group leader change event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnGroupDisband(Group* group)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);
    if (!IsEnabled() || !group)
        return;

    auto itr = _groupEvents.find(GROUP_EVENT_ON_DISBAND);
    if (itr == _groupEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, GROUP_EVENT_ON_DISBAND);
        PushGroup(group);

        if (lua_pcall(_state, 2, 0, 0) != LUA_OK)
        {
            LogError("group disband event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnGuildCreate(Guild* guild, Player* leader, std::string const& name)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);
    if (!IsEnabled() || !guild)
        return;

    auto itr = _guildEvents.find(GUILD_EVENT_ON_CREATE);
    if (itr == _guildEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, GUILD_EVENT_ON_CREATE);
        PushGuild(guild);
        PushPlayer(leader);
        lua_pushlstring(_state, name.c_str(), name.size());

        if (lua_pcall(_state, 4, 0, 0) != LUA_OK)
        {
            LogError("guild create event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnGuildMemberAdd(Guild* guild, Player* player, uint32 rank)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);
    if (!IsEnabled() || !guild)
        return;

    auto itr = _guildEvents.find(GUILD_EVENT_ON_ADD_MEMBER);
    if (itr == _guildEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, GUILD_EVENT_ON_ADD_MEMBER);
        PushGuild(guild);
        PushPlayer(player);
        lua_pushinteger(_state, rank);

        if (lua_pcall(_state, 4, 0, 0) != LUA_OK)
        {
            LogError("guild member add event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnGuildMemberRemove(Guild* guild, Player* player, bool isDisbanding)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);
    if (!IsEnabled() || !guild)
        return;

    auto itr = _guildEvents.find(GUILD_EVENT_ON_REMOVE_MEMBER);
    if (itr == _guildEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, GUILD_EVENT_ON_REMOVE_MEMBER);
        PushGuild(guild);
        PushPlayer(player);
        lua_pushboolean(_state, isDisbanding);

        if (lua_pcall(_state, 4, 0, 0) != LUA_OK)
        {
            LogError("guild member remove event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnGuildMOTDChange(Guild* guild, std::string const& motd)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);
    if (!IsEnabled() || !guild)
        return;

    auto itr = _guildEvents.find(GUILD_EVENT_ON_MOTD_CHANGE);
    if (itr == _guildEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, GUILD_EVENT_ON_MOTD_CHANGE);
        PushGuild(guild);
        lua_pushlstring(_state, motd.c_str(), motd.size());

        if (lua_pcall(_state, 3, 0, 0) != LUA_OK)
        {
            LogError("guild motd change event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnGuildInfoChange(Guild* guild, std::string const& info)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);
    if (!IsEnabled() || !guild)
        return;

    auto itr = _guildEvents.find(GUILD_EVENT_ON_INFO_CHANGE);
    if (itr == _guildEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, GUILD_EVENT_ON_INFO_CHANGE);
        PushGuild(guild);
        lua_pushlstring(_state, info.c_str(), info.size());

        if (lua_pcall(_state, 3, 0, 0) != LUA_OK)
        {
            LogError("guild info change event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnGuildDisband(Guild* guild)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);
    if (!IsEnabled() || !guild)
        return;

    auto itr = _guildEvents.find(GUILD_EVENT_ON_DISBAND);
    if (itr == _guildEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, GUILD_EVENT_ON_DISBAND);
        PushGuild(guild);

        if (lua_pcall(_state, 2, 0, 0) != LUA_OK)
        {
            LogError("guild disband event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::UpdateTimedEvents(uint32 diff)
{
    if (_timedEvents.empty())
        return;

    std::vector<uint32> eventIds;
    eventIds.reserve(_timedEvents.size());
    for (TimedEvent const& event : _timedEvents)
        eventIds.push_back(event.id);

    for (uint32 eventId : eventIds)
    {
        auto itr = std::find_if(_timedEvents.begin(), _timedEvents.end(), [eventId](TimedEvent const& event)
        {
            return event.id == eventId;
        });

        if (itr == _timedEvents.end())
            continue;

        TimedEvent& event = *itr;
        event.elapsed += diff;

        if (event.elapsed < event.delay)
        {
            continue;
        }

        event.elapsed = 0;
        int functionRef = event.functionRef;
        uint32 delay = event.delay;
        int32 remaining = event.remaining;
        bool hasObject = event.hasObject;
        WorldObject* boundObject = nullptr;

        if (hasObject)
        {
            boundObject = ResolveTimedEventObject(event);
            if (!boundObject)
            {
                luaL_unref(_state, LUA_REGISTRYINDEX, functionRef);
                _timedEvents.erase(itr);
                continue;
            }
        }

        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        lua_pushinteger(_state, eventId);
        lua_pushinteger(_state, delay);
        lua_pushinteger(_state, remaining < 0 ? 0 : remaining);
        if (hasObject)
            PushWorldObjectValue(_state, this, boundObject);

        bool remove = false;
        if (lua_pcall(_state, hasObject ? 4 : 3, 0, 0) != LUA_OK)
        {
            LogError("lua timed event");
            lua_pop(_state, 1);
            remove = true;
        }
        else if (remaining > 0)
        {
            --remaining;
            remove = remaining == 0;
        }

        itr = std::find_if(_timedEvents.begin(), _timedEvents.end(), [eventId](TimedEvent const& current)
        {
            return current.id == eventId;
        });

        if (itr == _timedEvents.end())
            continue;

        if (remove)
        {
            luaL_unref(_state, LUA_REGISTRYINDEX, functionRef);
            _timedEvents.erase(itr);
        }
        else
        {
            itr->remaining = remaining;
            itr->delay = GenerateTimedEventDelay(*itr);
            itr->elapsed = 0;
        }
    }
}

void TurtleLuaEngine::OnPlayerLogin(Player* player)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (IsEnabled())
        CallPlayerEvent(PLAYER_EVENT_ON_LOGIN, player);
}

void TurtleLuaEngine::OnPlayerLogout(Player* player)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (IsEnabled())
        CallPlayerEvent(PLAYER_EVENT_ON_LOGOUT, player);
}

void TurtleLuaEngine::OnPlayerCharacterCreate(Player* player)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (IsEnabled())
        CallPlayerEvent(PLAYER_EVENT_ON_CHARACTER_CREATE, player);
}

void TurtleLuaEngine::OnPlayerCharacterDelete(uint32 guidLow)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled())
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_CHARACTER_DELETE);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_CHARACTER_DELETE);
        lua_pushinteger(_state, guidLow);

        if (lua_pcall(_state, 2, 0, 0) != LUA_OK)
        {
            LogError("player character delete event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnPlayerSpellCast(Player* player, Spell* spell, bool skipCheck)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player || !spell)
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_SPELL_CAST);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_SPELL_CAST);
        PushPlayer(player);
        PushSpell(spell);
        lua_pushboolean(_state, skipCheck);

        if (lua_pcall(_state, 4, 0, 0) != LUA_OK)
        {
            LogError("player spell cast event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnPlayerKillPlayer(Player* killer, Player* killed)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !killer || !killed)
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_KILL_PLAYER);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_KILL_PLAYER);
        PushPlayer(killer);
        PushPlayer(killed);

        if (lua_pcall(_state, 3, 0, 0) != LUA_OK)
        {
            LogError("player kill player event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnPlayerKillCreature(Player* killer, Creature* killed)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !killer || !killed)
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_KILL_CREATURE);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_KILL_CREATURE);
        PushPlayer(killer);
        PushCreature(killed);

        if (lua_pcall(_state, 3, 0, 0) != LUA_OK)
        {
            LogError("player kill creature event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnPlayerKilledByCreature(Creature* killer, Player* killed)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !killer || !killed)
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_KILLED_BY_CREATURE);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_KILLED_BY_CREATURE);
        PushCreature(killer);
        PushPlayer(killed);

        if (lua_pcall(_state, 3, 0, 0) != LUA_OK)
        {
            LogError("player killed by creature event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnPlayerDuelRequest(Player* target, Player* challenger)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !target || !challenger)
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_DUEL_REQUEST);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_DUEL_REQUEST);
        PushPlayer(target);
        PushPlayer(challenger);

        if (lua_pcall(_state, 3, 0, 0) != LUA_OK)
        {
            LogError("player duel request event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnPlayerDuelStart(Player* player1, Player* player2)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player1 || !player2)
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_DUEL_START);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_DUEL_START);
        PushPlayer(player1);
        PushPlayer(player2);

        if (lua_pcall(_state, 3, 0, 0) != LUA_OK)
        {
            LogError("player duel start event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnPlayerDuelEnd(Player* winner, Player* loser, uint32 type)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !winner || !loser)
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_DUEL_END);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_DUEL_END);
        PushPlayer(winner);
        PushPlayer(loser);
        lua_pushinteger(_state, type);

        if (lua_pcall(_state, 4, 0, 0) != LUA_OK)
        {
            LogError("player duel end event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnPlayerGiveXP(Player* player, uint32& amount, Unit* victim, uint8 source)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player)
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_GIVE_XP);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        int before = lua_gettop(_state);
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_settop(_state, before);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_GIVE_XP);
        PushPlayer(player);
        lua_pushinteger(_state, amount);
        PushUnit(victim);
        lua_pushinteger(_state, source);

        if (lua_pcall(_state, 5, LUA_MULTRET, 0) != LUA_OK)
        {
            LogError("player give xp event");
            lua_pop(_state, 1);
            continue;
        }

        int results = lua_gettop(_state) - before;
        if (results >= 1)
        {
            int first = before + 1;
            if (lua_isnumber(_state, first))
            {
                lua_Integer newAmount = lua_tointeger(_state, first);
                amount = newAmount > 0 ? static_cast<uint32>(newAmount) : 0;
            }
        }

        lua_settop(_state, before);
    }
}

void TurtleLuaEngine::OnPlayerLevelChange(Player* player, uint32 oldLevel)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player)
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_LEVEL_CHANGE);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_LEVEL_CHANGE);
        PushPlayer(player);
        lua_pushinteger(_state, oldLevel);

        if (lua_pcall(_state, 3, 0, 0) != LUA_OK)
        {
            LogError("player level change event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnPlayerMoneyChange(Player* player, int32& amount)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player)
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_MONEY_CHANGE);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        int before = lua_gettop(_state);
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_settop(_state, before);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_MONEY_CHANGE);
        PushPlayer(player);
        lua_pushinteger(_state, amount);

        if (lua_pcall(_state, 3, LUA_MULTRET, 0) != LUA_OK)
        {
            LogError("player money change event");
            lua_pop(_state, 1);
            continue;
        }

        int results = lua_gettop(_state) - before;
        if (results >= 1)
        {
            int first = before + 1;
            if (lua_isnumber(_state, first))
                amount = static_cast<int32>(lua_tointeger(_state, first));
        }

        lua_settop(_state, before);
    }
}

bool TurtleLuaEngine::OnPlayerReputationChange(Player* player, uint32 factionId, int32& standing, bool incremental)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player)
        return true;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_REPUTATION_CHANGE);
    if (itr == _playerEvents.end())
        return true;

    bool allow = true;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        int before = lua_gettop(_state);
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_settop(_state, before);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_REPUTATION_CHANGE);
        PushPlayer(player);
        lua_pushinteger(_state, factionId);
        lua_pushinteger(_state, standing);
        lua_pushboolean(_state, incremental);

        if (lua_pcall(_state, 5, LUA_MULTRET, 0) != LUA_OK)
        {
            LogError("player reputation change event");
            lua_pop(_state, 1);
            continue;
        }

        int results = lua_gettop(_state) - before;
        if (results >= 1)
        {
            int first = before + 1;
            if (lua_isboolean(_state, first) && !lua_toboolean(_state, first))
                allow = false;
            else if (lua_isnumber(_state, first))
            {
                standing = static_cast<int32>(lua_tointeger(_state, first));
                if (standing == -1)
                    allow = false;
            }
        }

        lua_settop(_state, before);

        if (!allow)
            break;
    }

    return allow;
}

void TurtleLuaEngine::OnPlayerTalentsChange(Player* player, uint32 points)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player)
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_TALENTS_CHANGE);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_TALENTS_CHANGE);
        PushPlayer(player);
        lua_pushinteger(_state, points);

        if (lua_pcall(_state, 3, 0, 0) != LUA_OK)
        {
            LogError("player talents change event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnPlayerTalentsReset(Player* player, bool noCost)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player)
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_TALENTS_RESET);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_TALENTS_RESET);
        PushPlayer(player);
        lua_pushboolean(_state, noCost);

        if (lua_pcall(_state, 3, 0, 0) != LUA_OK)
        {
            LogError("player talents reset event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnPlayerLearnTalents(Player* player, uint32 talentId, uint32 talentRank, uint32 spellId)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player)
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_LEARN_TALENTS);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_LEARN_TALENTS);
        PushPlayer(player);
        lua_pushinteger(_state, talentId);
        lua_pushinteger(_state, talentRank);
        lua_pushinteger(_state, spellId);

        if (lua_pcall(_state, 5, 0, 0) != LUA_OK)
        {
            LogError("player learn talents event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnPlayerFirstLogin(Player* player)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (IsEnabled())
        CallPlayerEvent(PLAYER_EVENT_ON_FIRST_LOGIN, player);
}

void TurtleLuaEngine::OnPlayerRepop(Player* player)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (IsEnabled())
        CallPlayerEvent(PLAYER_EVENT_ON_REPOP, player);
}

void TurtleLuaEngine::OnPlayerResurrect(Player* player)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (IsEnabled())
        CallPlayerEvent(PLAYER_EVENT_ON_RESURRECT, player);
}

void TurtleLuaEngine::OnPlayerEquip(Player* player, Item* item, uint8 bag, uint8 slot)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player)
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_EQUIP);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_EQUIP);
        PushPlayer(player);
        PushItem(item);
        lua_pushinteger(_state, bag);
        lua_pushinteger(_state, slot);

        if (lua_pcall(_state, 5, 0, 0) != LUA_OK)
        {
            LogError("player equip event");
            lua_pop(_state, 1);
        }
    }
}

uint32 TurtleLuaEngine::OnPlayerCanUseItem(Player const* player, uint32 itemEntry)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player)
        return EQUIP_ERR_OK;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_CAN_USE_ITEM);
    if (itr == _playerEvents.end())
        return EQUIP_ERR_OK;

    uint32 result = EQUIP_ERR_OK;
    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_CAN_USE_ITEM);
        PushPlayer(const_cast<Player*>(player));
        lua_pushinteger(_state, itemEntry);

        if (lua_pcall(_state, 3, 1, 0) != LUA_OK)
        {
            LogError("player can use item event");
            lua_pop(_state, 1);
            continue;
        }

        if (lua_isnumber(_state, -1))
            result = static_cast<uint32>(lua_tointeger(_state, -1));
        lua_pop(_state, 1);
    }

    return result;
}

void TurtleLuaEngine::CallPlayerItemEvent(uint32 eventId, char const* context, Player* player, Item* item, uint32 count)
{
    if (!IsEnabled() || !player)
        return;

    auto itr = _playerEvents.find(eventId);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, eventId);
        PushPlayer(player);
        PushItem(item);
        lua_pushinteger(_state, count);

        if (lua_pcall(_state, 4, 0, 0) != LUA_OK)
        {
            LogError(context);
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnPlayerLootItem(Player* player, Item* item, uint32 count, ObjectGuid const& guid)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player)
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_LOOT_ITEM);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_LOOT_ITEM);
        PushPlayer(player);
        PushItem(item);
        lua_pushinteger(_state, count);
        PushObjectGuid(guid);

        if (lua_pcall(_state, 5, 0, 0) != LUA_OK)
        {
            LogError("player loot item event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnPlayerLootMoney(Player* player, uint32 amount)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player)
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_LOOT_MONEY);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_LOOT_MONEY);
        PushPlayer(player);
        lua_pushinteger(_state, amount);

        if (lua_pcall(_state, 3, 0, 0) != LUA_OK)
        {
            LogError("player loot money event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnPlayerQuestRewardItem(Player* player, Item* item, uint32 count)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);
    CallPlayerItemEvent(PLAYER_EVENT_ON_QUEST_REWARD_ITEM, "player quest reward item event", player, item, count);
}

void TurtleLuaEngine::OnPlayerCreateItem(Player* player, Item* item, uint32 count)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);
    CallPlayerItemEvent(PLAYER_EVENT_ON_CREATE_ITEM, "player create item event", player, item, count);
}

void TurtleLuaEngine::OnPlayerStoreNewItem(Player* player, Item* item, uint32 count)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);
    CallPlayerItemEvent(PLAYER_EVENT_ON_STORE_NEW_ITEM, "player store new item event", player, item, count);
}

void TurtleLuaEngine::OnPlayerQuestAbandon(Player* player, uint32 questId)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player)
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_QUEST_ABANDON);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_QUEST_ABANDON);
        PushPlayer(player);
        lua_pushinteger(_state, questId);

        if (lua_pcall(_state, 3, 0, 0) != LUA_OK)
        {
            LogError("player quest abandon event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnPetAddedToWorld(Player* player, Creature* pet)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player)
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_PET_ADDED_TO_WORLD);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_PET_ADDED_TO_WORLD);
        PushPlayer(player);
        PushCreature(pet);

        if (lua_pcall(_state, 3, 0, 0) != LUA_OK)
        {
            LogError("player pet added to world event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnPlayerLearnSpell(Player* player, uint32 spellId)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player)
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_LEARN_SPELL);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_LEARN_SPELL);
        PushPlayer(player);
        lua_pushinteger(_state, spellId);

        if (lua_pcall(_state, 3, 0, 0) != LUA_OK)
        {
            LogError("player learn spell event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnPlayerFFAPvPChange(Player* player, bool hasFfaPvp)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player)
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_FFAPVP_CHANGE);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_FFAPVP_CHANGE);
        PushPlayer(player);
        lua_pushboolean(_state, hasFfaPvp);

        if (lua_pcall(_state, 3, 0, 0) != LUA_OK)
        {
            LogError("player FFA PvP change event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnPlayerPetKill(Player* player, Creature* killed)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player || !killed)
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_PET_KILL);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_PET_KILL);
        PushPlayer(player);
        PushCreature(killed);

        if (lua_pcall(_state, 3, 0, 0) != LUA_OK)
        {
            LogError("player pet kill event");
            lua_pop(_state, 1);
        }
    }
}

bool TurtleLuaEngine::OnPlayerCommand(Player* player, std::string const& command)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled())
        return true;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_COMMAND);
    if (itr == _playerEvents.end())
        return true;

    bool allowed = true;
    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_COMMAND);
        PushPlayer(player);
        lua_pushlstring(_state, command.c_str(), command.size());
        PushChatHandlerValue(_state, player);

        if (lua_pcall(_state, 4, 1, 0) != LUA_OK)
        {
            LogError("player command event");
            lua_pop(_state, 1);
            continue;
        }

        if (lua_isboolean(_state, -1) && !lua_toboolean(_state, -1))
            allowed = false;
        lua_pop(_state, 1);
    }

    return allowed;
}

bool TurtleLuaEngine::OnPlayerCanInitTrade(Player* player, Player* target)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player || !target)
        return true;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_CAN_INIT_TRADE);
    if (itr == _playerEvents.end())
        return true;

    bool allowed = true;
    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_CAN_INIT_TRADE);
        PushPlayer(player);
        PushPlayer(target);

        if (lua_pcall(_state, 3, 1, 0) != LUA_OK)
        {
            LogError("player can init trade event");
            lua_pop(_state, 1);
            continue;
        }

        if (lua_isboolean(_state, -1) && !lua_toboolean(_state, -1))
            allowed = false;
        lua_pop(_state, 1);
    }

    return allowed;
}

bool TurtleLuaEngine::OnPlayerCanSendMail(Player* player, ObjectGuid const& receiverGuid, ObjectGuid const& mailboxGuid, std::string const& subject, std::string const& body, uint32 money, uint32 cod, Item* item)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player)
        return true;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_CAN_SEND_MAIL);
    if (itr == _playerEvents.end())
        return true;

    bool allowed = true;
    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_CAN_SEND_MAIL);
        PushPlayer(player);
        PushObjectGuid(receiverGuid);
        PushObjectGuid(mailboxGuid);
        lua_pushlstring(_state, subject.c_str(), subject.size());
        lua_pushlstring(_state, body.c_str(), body.size());
        lua_pushinteger(_state, money);
        lua_pushinteger(_state, cod);
        PushItem(item);

        if (lua_pcall(_state, 9, 1, 0) != LUA_OK)
        {
            LogError("player can send mail event");
            lua_pop(_state, 1);
            continue;
        }

        if (lua_isboolean(_state, -1) && !lua_toboolean(_state, -1))
            allowed = false;
        lua_pop(_state, 1);
    }

    return allowed;
}

void TurtleLuaEngine::OnPlayerEnterCombat(Player* player, Unit* enemy)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player)
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_ENTER_COMBAT);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_ENTER_COMBAT);
        PushPlayer(player);
        PushUnit(enemy);

        if (lua_pcall(_state, 3, 0, 0) != LUA_OK)
        {
            LogError("player enter combat event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnPlayerLeaveCombat(Player* player)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (IsEnabled())
        CallPlayerEvent(PLAYER_EVENT_ON_LEAVE_COMBAT, player);
}

void TurtleLuaEngine::OnPlayerCompleteQuest(Player* player, Quest const* quest)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player || !quest)
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_COMPLETE_QUEST);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_COMPLETE_QUEST);
        PushPlayer(player);
        PushQuest(_state, quest);

        if (lua_pcall(_state, 3, 0, 0) != LUA_OK)
        {
            LogError("player complete quest event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnPlayerGroupRollRewardItem(Player* player, Item* item, uint32 count, uint32 voteType, Roll const* roll)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player || !item)
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_GROUP_ROLL_REWARD_ITEM);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_GROUP_ROLL_REWARD_ITEM);
        PushPlayer(player);
        PushItem(item);
        lua_pushinteger(_state, count);
        lua_pushinteger(_state, voteType);
        PushRollValue(_state, roll, item, count);

        if (lua_pcall(_state, 6, 0, 0) != LUA_OK)
        {
            LogError("player group roll reward item event");
            lua_pop(_state, 1);
        }
    }
}

bool TurtleLuaEngine::OnPacketReceive(WorldSession* session, WorldPacket& packet)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled())
        return true;

    Player* player = session ? session->GetPlayer() : nullptr;
    bool allow = true;

    if (!CallServerPacketEvent(SERVER_EVENT_ON_PACKET_RECEIVE, packet, player, "server packet receive event"))
        allow = false;

    if (!CallPacketEvent(packet.GetOpcode(), PACKET_EVENT_ON_PACKET_RECEIVE, packet, player, "packet receive event"))
        allow = false;

    return allow;
}

bool TurtleLuaEngine::OnPacketSend(WorldSession* session, WorldPacket& packet)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled())
        return true;

    Player* player = session ? session->GetPlayer() : nullptr;
    bool allow = true;

    if (!CallServerPacketEvent(SERVER_EVENT_ON_PACKET_SEND, packet, player, "server packet send event"))
        allow = false;

    if (!CallPacketEvent(packet.GetOpcode(), PACKET_EVENT_ON_PACKET_SEND, packet, player, "packet send event"))
        allow = false;

    return allow;
}

void TurtleLuaEngine::OnPlayerBGDesertion(Player* player, uint32 type)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player)
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_BG_DESERTION);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_BG_DESERTION);
        PushPlayer(player);
        lua_pushinteger(_state, type);

        if (lua_pcall(_state, 3, 0, 0) != LUA_OK)
        {
            LogError("player bg desertion event");
            lua_pop(_state, 1);
        }
    }
}

bool TurtleLuaEngine::CallPlayerChatEvent(uint32 eventId, char const* context, Player* player, uint32 type, uint32 lang, std::string& message, Player* receiver, Group* group, Guild* guild, Channel* channel)
{
    if (!player)
        return true;

    auto itr = _playerEvents.find(eventId);
    if (itr == _playerEvents.end())
        return true;

    bool allow = true;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        int before = lua_gettop(_state);
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_settop(_state, before);
            continue;
        }

        lua_pushinteger(_state, eventId);
        PushPlayer(player);
        lua_pushlstring(_state, message.c_str(), message.size());
        lua_pushinteger(_state, ToElunaChatType(type));
        lua_pushinteger(_state, lang);
        switch (eventId)
        {
            case PLAYER_EVENT_ON_WHISPER:
                PushPlayer(receiver);
                break;
            case PLAYER_EVENT_ON_GROUP_CHAT:
                PushGroup(group);
                break;
            case PLAYER_EVENT_ON_GUILD_CHAT:
                PushGuild(guild);
                break;
            case PLAYER_EVENT_ON_CHANNEL_CHAT:
                lua_pushinteger(_state, channel ? channel->GetChannelId() : 0);
                PushChannelValue(_state, channel);
                break;
            default:
                break;
        }

        int argCount = eventId == PLAYER_EVENT_ON_CHAT ? 5 : (eventId == PLAYER_EVENT_ON_CHANNEL_CHAT ? 7 : 6);
        if (lua_pcall(_state, argCount, LUA_MULTRET, 0) != LUA_OK)
        {
            LogError(context);
            lua_pop(_state, 1);
            continue;
        }

        int results = lua_gettop(_state) - before;
        if (results >= 1)
        {
            int first = before + 1;
            if (lua_isboolean(_state, first) && !lua_toboolean(_state, first))
                allow = false;
            else if (lua_isstring(_state, first))
                message = lua_tostring(_state, first);
        }

        if (results >= 2)
        {
            int second = before + 2;
            if (lua_isstring(_state, second))
                message = lua_tostring(_state, second);
        }

        lua_settop(_state, before);

        if (!allow)
            break;
    }

    return allow;
}

bool TurtleLuaEngine::OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& message)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled())
        return true;

    return CallPlayerChatEvent(PLAYER_EVENT_ON_CHAT, "player chat event", player, type, lang, message, nullptr, nullptr, nullptr, nullptr);
}

bool TurtleLuaEngine::OnPlayerWhisper(Player* player, uint32 type, uint32 lang, std::string& message, Player* receiver)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled())
        return true;

    return CallPlayerChatEvent(PLAYER_EVENT_ON_WHISPER, "player whisper event", player, type, lang, message, receiver, nullptr, nullptr, nullptr);
}

bool TurtleLuaEngine::OnPlayerGroupChat(Player* player, uint32 type, uint32 lang, std::string& message, Group* group)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled())
        return true;

    return CallPlayerChatEvent(PLAYER_EVENT_ON_GROUP_CHAT, "player group chat event", player, type, lang, message, nullptr, group, nullptr, nullptr);
}

bool TurtleLuaEngine::OnPlayerGuildChat(Player* player, uint32 type, uint32 lang, std::string& message, Guild* guild)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled())
        return true;

    return CallPlayerChatEvent(PLAYER_EVENT_ON_GUILD_CHAT, "player guild chat event", player, type, lang, message, nullptr, nullptr, guild, nullptr);
}

bool TurtleLuaEngine::OnPlayerChannelChat(Player* player, uint32 type, uint32 lang, std::string& message, Channel* channel)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled())
        return true;

    return CallPlayerChatEvent(PLAYER_EVENT_ON_CHANNEL_CHAT, "player channel chat event", player, type, lang, message, nullptr, nullptr, nullptr, channel);
}

bool TurtleLuaEngine::OnAddonMessage(Player* sender, uint32 type, std::string const& message, Player* receiver, Guild* guild, Group* group, uint32 channelId, bool hasChannelTarget)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled())
        return true;

    auto itr = _serverEvents.find(ADDON_EVENT_ON_MESSAGE);
    if (itr == _serverEvents.end())
        return true;

    std::string prefix;
    std::string content;
    bool hasContent = false;

    std::string::size_type delimiter = message.find('\t');
    if (delimiter == std::string::npos)
        prefix = message;
    else
    {
        prefix = message.substr(0, delimiter);
        content = message.substr(delimiter + 1);
        hasContent = true;
    }

    bool allow = true;
    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, ADDON_EVENT_ON_MESSAGE);
        PushPlayer(sender);
        lua_pushinteger(_state, type);
        lua_pushlstring(_state, prefix.c_str(), prefix.size());
        if (hasContent)
            lua_pushlstring(_state, content.c_str(), content.size());
        else
            lua_pushnil(_state);

        if (receiver)
            PushPlayer(receiver);
        else if (guild)
            PushGuild(guild);
        else if (group)
            PushGroup(group);
        else if (hasChannelTarget)
            lua_pushinteger(_state, channelId);
        else
            lua_pushnil(_state);

        if (lua_pcall(_state, 6, 1, 0) != LUA_OK)
        {
            LogError("addon message event");
            lua_pop(_state, 1);
            continue;
        }

        if (lua_isboolean(_state, -1) && !lua_toboolean(_state, -1))
            allow = false;

        lua_pop(_state, 1);
    }

    return allow;
}

void TurtleLuaEngine::OnPlayerEmote(Player* player, uint32 emote)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player)
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_EMOTE);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_EMOTE);
        PushPlayer(player);
        lua_pushinteger(_state, emote);

        if (lua_pcall(_state, 3, 0, 0) != LUA_OK)
        {
            LogError("player emote event");
            lua_pop(_state, 1);
        }
    }
}

bool TurtleLuaEngine::OnPlayerCanGroupInvite(Player* player, std::string const& memberName)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player)
        return true;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_CAN_GROUP_INVITE);
    if (itr == _playerEvents.end())
        return true;

    bool allowed = true;
    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_CAN_GROUP_INVITE);
        PushPlayer(player);
        lua_pushlstring(_state, memberName.c_str(), memberName.size());

        if (lua_pcall(_state, 3, 1, 0) != LUA_OK)
        {
            LogError("player can group invite event");
            lua_pop(_state, 1);
            continue;
        }

        if (lua_isboolean(_state, -1) && !lua_toboolean(_state, -1))
            allowed = false;
        lua_pop(_state, 1);
    }

    return allowed;
}

bool TurtleLuaEngine::OnPlayerCanResurrect(Player* player)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player)
        return true;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_CAN_RESURRECT);
    if (itr == _playerEvents.end())
        return true;

    bool allowed = true;
    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_CAN_RESURRECT);
        PushPlayer(player);

        if (lua_pcall(_state, 2, 1, 0) != LUA_OK)
        {
            LogError("player can resurrect event");
            lua_pop(_state, 1);
            continue;
        }

        if (lua_isboolean(_state, -1) && !lua_toboolean(_state, -1))
            allowed = false;
        lua_pop(_state, 1);
    }

    return allowed;
}

bool TurtleLuaEngine::OnPlayerCanUpdateSkill(Player* player, uint32 skillId)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player)
        return true;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_CAN_UPDATE_SKILL);
    if (itr == _playerEvents.end())
        return true;

    bool allowed = true;
    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_CAN_UPDATE_SKILL);
        PushPlayer(player);
        lua_pushinteger(_state, skillId);

        if (lua_pcall(_state, 3, 1, 0) != LUA_OK)
        {
            LogError("player can update skill event");
            lua_pop(_state, 1);
            continue;
        }

        if (lua_isboolean(_state, -1) && !lua_toboolean(_state, -1))
            allowed = false;
        lua_pop(_state, 1);
    }

    return allowed;
}

void TurtleLuaEngine::OnPlayerBeforeUpdateSkill(Player* player, uint32 skillId, uint32& value, uint32 max, uint32 step)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player)
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_BEFORE_UPDATE_SKILL);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_BEFORE_UPDATE_SKILL);
        PushPlayer(player);
        lua_pushinteger(_state, skillId);
        lua_pushinteger(_state, value);
        lua_pushinteger(_state, max);
        lua_pushinteger(_state, step);

        if (lua_pcall(_state, 6, 1, 0) != LUA_OK)
        {
            LogError("player before update skill event");
            lua_pop(_state, 1);
            continue;
        }

        if (lua_isnumber(_state, -1))
        {
            lua_Integer newValue = lua_tointeger(_state, -1);
            value = newValue > 0 ? static_cast<uint32>(newValue) : 0;
        }
        lua_pop(_state, 1);
    }
}

void TurtleLuaEngine::OnPlayerUpdateSkill(Player* player, uint32 skillId, uint32 value, uint32 max, uint32 step, uint32 newValue)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player)
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_UPDATE_SKILL);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_UPDATE_SKILL);
        PushPlayer(player);
        lua_pushinteger(_state, skillId);
        lua_pushinteger(_state, value);
        lua_pushinteger(_state, max);
        lua_pushinteger(_state, step);
        lua_pushinteger(_state, newValue);

        if (lua_pcall(_state, 7, 0, 0) != LUA_OK)
        {
            LogError("player update skill event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnPlayerTextEmote(Player* player, uint32 textEmote, uint32 emoteNum, ObjectGuid const& guid)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled())
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_TEXT_EMOTE);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_TEXT_EMOTE);
        PushPlayer(player);
        lua_pushinteger(_state, textEmote);
        lua_pushinteger(_state, emoteNum);
        PushObjectGuid(guid);

        if (lua_pcall(_state, 5, 0, 0) != LUA_OK)
        {
            LogError("player text emote event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnPlayerSave(Player* player)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (IsEnabled())
        CallPlayerEvent(PLAYER_EVENT_ON_SAVE, player);
}

void TurtleLuaEngine::OnPlayerBindToInstance(Player* player, uint32 difficulty, uint32 mapId, bool permanent)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled())
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_BIND_TO_INSTANCE);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_BIND_TO_INSTANCE);
        PushPlayer(player);
        lua_pushinteger(_state, difficulty);
        lua_pushinteger(_state, mapId);
        lua_pushboolean(_state, permanent);

        if (lua_pcall(_state, 5, 0, 0) != LUA_OK)
        {
            LogError("player bind to instance event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnPlayerUpdateZone(Player* player, uint32 newZone, uint32 newArea)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled())
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_UPDATE_ZONE);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_UPDATE_ZONE);
        PushPlayer(player);
        lua_pushinteger(_state, newZone);
        lua_pushinteger(_state, newArea);

        if (lua_pcall(_state, 4, 0, 0) != LUA_OK)
        {
            LogError("player update zone event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnPlayerMapChanged(Player* player)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (IsEnabled())
        CallPlayerEvent(PLAYER_EVENT_ON_MAP_CHANGE, player);
}

void TurtleLuaEngine::OnPlayerUpdateArea(Player* player, uint32 oldArea, uint32 newArea)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled())
        return;

    auto itr = _playerEvents.find(PLAYER_EVENT_ON_UPDATE_AREA);
    if (itr == _playerEvents.end())
        return;

    std::vector<int> functionRefs = itr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        lua_pushinteger(_state, PLAYER_EVENT_ON_UPDATE_AREA);
        PushPlayer(player);
        lua_pushinteger(_state, oldArea);
        lua_pushinteger(_state, newArea);

        if (lua_pcall(_state, 4, 0, 0) != LUA_OK)
        {
            LogError("player update area event");
            lua_pop(_state, 1);
        }
    }
}

void TurtleLuaEngine::OnCreatureEnterCombat(Creature* creature, Unit* target)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !creature)
        return;

    lua_pushinteger(_state, CREATURE_EVENT_ON_ENTER_COMBAT);
    PushCreature(creature);
    PushUnit(target);
    CallCreatureEvent(creature, CREATURE_EVENT_ON_ENTER_COMBAT, 3);
}

void TurtleLuaEngine::OnCreatureLeaveCombat(Creature* creature)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !creature)
        return;

    OnCreatureReset(creature);

    lua_pushinteger(_state, CREATURE_EVENT_ON_LEAVE_COMBAT);
    PushCreature(creature);
    CallCreatureEvent(creature, CREATURE_EVENT_ON_LEAVE_COMBAT, 2);
}

void TurtleLuaEngine::OnCreatureTargetDied(Creature* creature, Unit* victim)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !creature)
        return;

    lua_pushinteger(_state, CREATURE_EVENT_ON_TARGET_DIED);
    PushCreature(creature);
    PushUnit(victim);
    CallCreatureEvent(creature, CREATURE_EVENT_ON_TARGET_DIED, 3);
}

void TurtleLuaEngine::OnCreatureDied(Creature* creature, Unit* killer)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !creature)
        return;

    OnCreatureReset(creature);

    lua_pushinteger(_state, CREATURE_EVENT_ON_DIED);
    PushCreature(creature);
    PushUnit(killer);
    CallCreatureEvent(creature, CREATURE_EVENT_ON_DIED, 3);
}

void TurtleLuaEngine::OnCreatureSpawn(Creature* creature)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !creature)
        return;

    OnCreatureReset(creature);

    lua_pushinteger(_state, CREATURE_EVENT_ON_SPAWN);
    PushCreature(creature);
    CallCreatureEvent(creature, CREATURE_EVENT_ON_SPAWN, 2);
}

void TurtleLuaEngine::OnCreatureAdd(Creature* creature)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !creature)
        return;

    lua_pushinteger(_state, CREATURE_EVENT_ON_ADD);
    PushCreature(creature);
    CallCreatureEvent(creature, CREATURE_EVENT_ON_ADD, 2);

    Map* map = creature->GetMap();
    if (map && map->GetInstanceData())
    {
        lua_pushinteger(_state, INSTANCE_EVENT_ON_CREATURE_CREATE);
        PushInstanceDataValue(_state, map->GetInstanceData());
        PushMap(map);
        PushCreature(creature);
        CallInstanceEvent(map, INSTANCE_EVENT_ON_CREATURE_CREATE, 4);
    }
}

void TurtleLuaEngine::OnCreatureRemove(Creature* creature)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !creature)
        return;

    lua_pushinteger(_state, CREATURE_EVENT_ON_REMOVE);
    PushCreature(creature);
    CallCreatureEvent(creature, CREATURE_EVENT_ON_REMOVE, 2);
}

bool TurtleLuaEngine::OnCreatureReachWP(Creature* creature, uint32 type, uint32 id)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !creature)
        return false;

    std::vector<int> functionRefs = CollectCreatureEventRefs(creature, CREATURE_EVENT_ON_REACH_WP);
    if (functionRefs.empty())
        return false;

    bool stopNormalAction = false;

    for (int functionRef : functionRefs)
    {
        int before = lua_gettop(_state);
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        lua_pushinteger(_state, CREATURE_EVENT_ON_REACH_WP);
        PushCreature(creature);
        lua_pushinteger(_state, type);
        lua_pushinteger(_state, id);

        if (lua_pcall(_state, 4, LUA_MULTRET, 0) != LUA_OK)
        {
            LogError("creature reach waypoint event");
            lua_pop(_state, 1);
            continue;
        }

        int results = lua_gettop(_state) - before;
        if (results >= 1)
        {
            int first = before + 1;
            if (lua_isboolean(_state, first) && lua_toboolean(_state, first))
                stopNormalAction = true;
        }

        lua_settop(_state, before);

        if (stopNormalAction)
            break;
    }

    return stopNormalAction;
}

void TurtleLuaEngine::OnCreatureReset(Creature* creature)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !creature)
        return;

    lua_pushinteger(_state, CREATURE_EVENT_ON_RESET);
    PushCreature(creature);
    CallCreatureEvent(creature, CREATURE_EVENT_ON_RESET, 2);
}

bool TurtleLuaEngine::OnCreatureReachHome(Creature* creature)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !creature)
        return false;

    std::vector<int> functionRefs = CollectCreatureEventRefs(creature, CREATURE_EVENT_ON_REACH_HOME);
    if (functionRefs.empty())
        return false;

    bool stopNormalAction = false;

    for (int functionRef : functionRefs)
    {
        int before = lua_gettop(_state);
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        lua_pushinteger(_state, CREATURE_EVENT_ON_REACH_HOME);
        PushCreature(creature);

        if (lua_pcall(_state, 2, LUA_MULTRET, 0) != LUA_OK)
        {
            LogError("creature reach home event");
            lua_pop(_state, 1);
            continue;
        }

        int results = lua_gettop(_state) - before;
        if (results >= 1)
        {
            int first = before + 1;
            if (lua_isboolean(_state, first) && lua_toboolean(_state, first))
                stopNormalAction = true;
        }

        lua_settop(_state, before);

        if (stopNormalAction)
            break;
    }

    return stopNormalAction;
}

void TurtleLuaEngine::OnCreatureAIUpdate(Creature* creature, uint32 diff)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !creature)
        return;

    lua_pushinteger(_state, CREATURE_EVENT_ON_AIUPDATE);
    PushCreature(creature);
    lua_pushinteger(_state, diff);
    CallCreatureEvent(creature, CREATURE_EVENT_ON_AIUPDATE, 3);
}

bool TurtleLuaEngine::OnCreatureDamageTaken(Creature* creature, Unit* attacker, uint32& damage)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !creature)
        return false;

    std::vector<int> functionRefs = CollectCreatureEventRefs(creature, CREATURE_EVENT_ON_DAMAGE_TAKEN);
    if (functionRefs.empty())
        return false;

    bool stopNormalAction = false;

    for (int functionRef : functionRefs)
    {
        int before = lua_gettop(_state);
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        lua_pushinteger(_state, CREATURE_EVENT_ON_DAMAGE_TAKEN);
        PushCreature(creature);
        PushUnit(attacker);
        lua_pushinteger(_state, damage);

        if (lua_pcall(_state, 4, LUA_MULTRET, 0) != LUA_OK)
        {
            LogError("creature damage taken event");
            lua_pop(_state, 1);
            continue;
        }

        int results = lua_gettop(_state) - before;
        if (results >= 1)
        {
            int first = before + 1;
            if (lua_isboolean(_state, first) && lua_toboolean(_state, first))
                stopNormalAction = true;
            else if (lua_isnumber(_state, first))
                damage = static_cast<uint32>(lua_tointeger(_state, first));
        }

        if (results >= 2)
        {
            int second = before + 2;
            if (lua_isnumber(_state, second))
                damage = static_cast<uint32>(lua_tointeger(_state, second));
        }

        lua_settop(_state, before);

        if (stopNormalAction)
            break;
    }

    return stopNormalAction;
}

bool TurtleLuaEngine::OnCreatureOwnerAttacked(Creature* creature, Unit* target)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !creature || !target)
        return false;

    lua_pushinteger(_state, CREATURE_EVENT_ON_OWNER_ATTACKED);
    PushCreature(creature);
    PushUnit(target);
    return CallCreatureEvent(creature, CREATURE_EVENT_ON_OWNER_ATTACKED, 3);
}

bool TurtleLuaEngine::OnCreatureOwnerAttackedAt(Creature* creature, Unit* attacker)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !creature || !attacker)
        return false;

    lua_pushinteger(_state, CREATURE_EVENT_ON_OWNER_ATTACKED_AT);
    PushCreature(creature);
    PushUnit(attacker);
    return CallCreatureEvent(creature, CREATURE_EVENT_ON_OWNER_ATTACKED_AT, 3);
}

bool TurtleLuaEngine::OnCreatureHitBySpell(Creature* creature, WorldObject* caster, uint32 spellId)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !creature)
        return false;

    lua_pushinteger(_state, CREATURE_EVENT_ON_HIT_BY_SPELL);
    PushCreature(creature);
    if (Unit* casterUnit = dynamic_cast<Unit*>(caster))
        PushUnit(casterUnit);
    else if (GameObject* casterGo = dynamic_cast<GameObject*>(caster))
        PushGameObject(casterGo);
    else
        lua_pushnil(_state);
    lua_pushinteger(_state, spellId);

    return CallCreatureEvent(creature, CREATURE_EVENT_ON_HIT_BY_SPELL, 4);
}

bool TurtleLuaEngine::OnCreatureSpellHitTarget(Creature* creature, Unit* target, uint32 spellId)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !creature)
        return false;

    std::vector<int> functionRefs = CollectCreatureEventRefs(creature, CREATURE_EVENT_ON_SPELL_HIT_TARGET);
    if (functionRefs.empty())
        return false;

    bool stopNormalAction = false;

    for (int functionRef : functionRefs)
    {
        int before = lua_gettop(_state);
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        lua_pushinteger(_state, CREATURE_EVENT_ON_SPELL_HIT_TARGET);
        PushCreature(creature);
        PushUnit(target);
        lua_pushinteger(_state, spellId);

        if (lua_pcall(_state, 4, LUA_MULTRET, 0) != LUA_OK)
        {
            LogError("creature spell hit target event");
            lua_pop(_state, 1);
            continue;
        }

        int results = lua_gettop(_state) - before;
        if (results >= 1)
        {
            int first = before + 1;
            if (lua_isboolean(_state, first) && lua_toboolean(_state, first))
                stopNormalAction = true;
        }

        lua_settop(_state, before);

        if (stopNormalAction)
            break;
    }

    return stopNormalAction;
}

bool TurtleLuaEngine::OnCreatureJustSummoned(Creature* creature, Creature* summon)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !creature || !summon)
        return false;

    lua_pushinteger(_state, CREATURE_EVENT_ON_JUST_SUMMONED_CREATURE);
    PushCreature(creature);
    PushCreature(summon);
    return CallCreatureEvent(creature, CREATURE_EVENT_ON_JUST_SUMMONED_CREATURE, 3);
}

bool TurtleLuaEngine::OnCreatureSummonedCreatureDespawn(Creature* creature, Creature* summon)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !creature || !summon)
        return false;

    lua_pushinteger(_state, CREATURE_EVENT_ON_SUMMONED_CREATURE_DESPAWN);
    PushCreature(creature);
    PushCreature(summon);
    return CallCreatureEvent(creature, CREATURE_EVENT_ON_SUMMONED_CREATURE_DESPAWN, 3);
}

bool TurtleLuaEngine::OnCreatureMoveInLOS(Creature* creature, Unit* who)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !creature || !who)
        return false;

    lua_pushinteger(_state, CREATURE_EVENT_ON_MOVE_IN_LOS);
    PushCreature(creature);
    PushUnit(who);
    return CallCreatureEvent(creature, CREATURE_EVENT_ON_MOVE_IN_LOS, 3);
}

bool TurtleLuaEngine::OnCreatureReceiveEmote(Creature* creature, Player* player, uint32 emoteId)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !creature || !player)
        return false;

    lua_pushinteger(_state, CREATURE_EVENT_ON_RECEIVE_EMOTE);
    PushCreature(creature);
    PushPlayer(player);
    lua_pushinteger(_state, emoteId);

    return CallCreatureEvent(creature, CREATURE_EVENT_ON_RECEIVE_EMOTE, 4);
}

bool TurtleLuaEngine::OnCreatureCorpseRemoved(Creature* creature, uint32& respawnDelay)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !creature)
        return false;

    std::vector<int> functionRefs = CollectCreatureEventRefs(creature, CREATURE_EVENT_ON_CORPSE_REMOVED);
    if (functionRefs.empty())
        return false;

    bool stopNormalAction = false;

    for (int functionRef : functionRefs)
    {
        int before = lua_gettop(_state);
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        lua_pushinteger(_state, CREATURE_EVENT_ON_CORPSE_REMOVED);
        PushCreature(creature);
        lua_pushinteger(_state, respawnDelay);

        if (lua_pcall(_state, 3, LUA_MULTRET, 0) != LUA_OK)
        {
            LogError("creature corpse removed event");
            lua_pop(_state, 1);
            continue;
        }

        int results = lua_gettop(_state) - before;
        if (results >= 1)
        {
            int first = before + 1;
            if (lua_isboolean(_state, first) && lua_toboolean(_state, first))
                stopNormalAction = true;
            else if (lua_isnumber(_state, first))
                respawnDelay = static_cast<uint32>(lua_tointeger(_state, first));
        }

        if (results >= 2)
        {
            int second = before + 2;
            if (lua_isnumber(_state, second))
                respawnDelay = static_cast<uint32>(lua_tointeger(_state, second));
        }

        lua_settop(_state, before);

        if (stopNormalAction)
            break;
    }

    return stopNormalAction;
}

bool TurtleLuaEngine::OnCreatureDummyEffect(WorldObject* caster, uint32 spellId, uint32 effIndex, Creature* target)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !target)
        return false;

    lua_pushinteger(_state, CREATURE_EVENT_ON_DUMMY_EFFECT);
    if (Unit* casterUnit = dynamic_cast<Unit*>(caster))
        PushUnit(casterUnit);
    else if (GameObject* casterGo = dynamic_cast<GameObject*>(caster))
        PushGameObject(casterGo);
    else
        lua_pushnil(_state);
    lua_pushinteger(_state, spellId);
    lua_pushinteger(_state, effIndex);
    PushCreature(target);

    return CallCreatureEvent(target, CREATURE_EVENT_ON_DUMMY_EFFECT, 5);
}

std::vector<int> TurtleLuaEngine::CollectCreatureEventRefs(Creature* creature, uint32 eventId)
{
    std::vector<int> refs;
    if (!creature)
        return refs;

    auto entryItr = _creatureEvents.find(creature->GetEntry());
    if (entryItr != _creatureEvents.end())
    {
        auto eventItr = entryItr->second.find(eventId);
        if (eventItr != entryItr->second.end())
            refs.insert(refs.end(), eventItr->second.begin(), eventItr->second.end());
    }

    auto uniqueItr = _uniqueCreatureEvents.find(UniqueCreatureEventKey(creature->GetObjectGuid(), creature->GetInstanceId()));
    if (uniqueItr != _uniqueCreatureEvents.end())
    {
        auto eventItr = uniqueItr->second.find(eventId);
        if (eventItr != uniqueItr->second.end())
            refs.insert(refs.end(), eventItr->second.begin(), eventItr->second.end());
    }

    return refs;
}

bool TurtleLuaEngine::CallCreatureEvent(Creature* creature, uint32 eventId, int argCount)
{
    int base = lua_gettop(_state) - argCount;
    std::vector<int> functionRefs = CollectCreatureEventRefs(creature, eventId);
    if (functionRefs.empty())
    {
        lua_settop(_state, base);
        return false;
    }

    bool handled = true;

    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        for (int i = 1; i <= argCount; ++i)
            lua_pushvalue(_state, base + i);

        if (lua_pcall(_state, argCount, LUA_MULTRET, 0) != LUA_OK)
        {
            LogError("creature event");
            lua_pop(_state, 1);
            continue;
        }

        int results = lua_gettop(_state) - base - argCount;
        int firstResult = base + argCount + 1;
        if (results >= 1 && lua_isboolean(_state, firstResult) && !lua_toboolean(_state, firstResult))
            handled = false;

        lua_settop(_state, base + argCount);

        if (!handled)
            break;
    }

    lua_settop(_state, base);
    return handled;
}

bool TurtleLuaEngine::CallEntryEvent(std::map<uint32, std::map<uint32, std::vector<int>>>& store, uint32 entry, uint32 eventId, int argCount)
{
    int base = lua_gettop(_state) - argCount;
    auto entryItr = store.find(entry);
    if (entryItr == store.end())
    {
        lua_settop(_state, base);
        return false;
    }

    auto eventItr = entryItr->second.find(eventId);
    if (eventItr == entryItr->second.end())
    {
        lua_settop(_state, base);
        return false;
    }

    bool handled = true;

    std::vector<int> functionRefs = eventItr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        for (int i = 1; i <= argCount; ++i)
            lua_pushvalue(_state, base + i);

        if (lua_pcall(_state, argCount, LUA_MULTRET, 0) != LUA_OK)
        {
            LogError("entry event");
            lua_pop(_state, 1);
            continue;
        }

        int results = lua_gettop(_state) - base - argCount;
        int firstResult = base + argCount + 1;
        if (results >= 1 && lua_isboolean(_state, firstResult) && !lua_toboolean(_state, firstResult))
            handled = false;

        lua_settop(_state, base + argCount);

        if (!handled)
            break;
    }

    lua_settop(_state, base);
    return handled;
}

bool TurtleLuaEngine::CallEntryEventForBoolean(std::map<uint32, std::map<uint32, std::vector<int>>>& store, uint32 entry, uint32 eventId, int argCount, bool expectedValue)
{
    int base = lua_gettop(_state) - argCount;
    auto entryItr = store.find(entry);
    if (entryItr == store.end())
    {
        lua_settop(_state, base);
        return false;
    }

    auto eventItr = entryItr->second.find(eventId);
    if (eventItr == entryItr->second.end())
    {
        lua_settop(_state, base);
        return false;
    }

    bool matched = false;

    std::vector<int> functionRefs = eventItr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        for (int i = 1; i <= argCount; ++i)
            lua_pushvalue(_state, base + i);

        if (lua_pcall(_state, argCount, LUA_MULTRET, 0) != LUA_OK)
        {
            LogError("entry event bool");
            lua_pop(_state, 1);
            continue;
        }

        int results = lua_gettop(_state) - base - argCount;
        int firstResult = base + argCount + 1;
        if (results >= 1 && lua_isboolean(_state, firstResult) && (lua_toboolean(_state, firstResult) != 0) == expectedValue)
            matched = true;

        lua_settop(_state, base + argCount);

        if (matched)
            break;
    }

    lua_settop(_state, base);
    return matched;
}

void TurtleLuaEngine::CallEntryEventIgnoreResult(std::map<uint32, std::map<uint32, std::vector<int>>>& store, uint32 entry, uint32 eventId, int argCount)
{
    int base = lua_gettop(_state) - argCount;
    auto entryItr = store.find(entry);
    if (entryItr == store.end())
    {
        lua_settop(_state, base);
        return;
    }

    auto eventItr = entryItr->second.find(eventId);
    if (eventItr == entryItr->second.end())
    {
        lua_settop(_state, base);
        return;
    }

    std::vector<int> functionRefs = eventItr->second;
    for (int functionRef : functionRefs)
    {
        lua_rawgeti(_state, LUA_REGISTRYINDEX, functionRef);
        if (!lua_isfunction(_state, -1))
        {
            lua_pop(_state, 1);
            continue;
        }

        for (int i = 1; i <= argCount; ++i)
            lua_pushvalue(_state, base + i);

        if (lua_pcall(_state, argCount, LUA_MULTRET, 0) != LUA_OK)
        {
            LogError("entry event");
            lua_pop(_state, 1);
            continue;
        }

        lua_settop(_state, base + argCount);
    }

    lua_settop(_state, base);
}

bool TurtleLuaEngine::OnCreatureGossipHello(Player* player, Creature* creature)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player || !creature)
        return false;

    lua_pushinteger(_state, GOSSIP_EVENT_ON_HELLO);
    PushPlayer(player);
    PushCreature(creature);

    return CallEntryEvent(_creatureGossipEvents, creature->GetEntry(), GOSSIP_EVENT_ON_HELLO, 3);
}

bool TurtleLuaEngine::OnCreatureGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action, char const* code)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player || !creature)
        return false;

    lua_pushinteger(_state, GOSSIP_EVENT_ON_SELECT);
    PushPlayer(player);
    PushCreature(creature);
    lua_pushinteger(_state, sender);
    lua_pushinteger(_state, action);
    if (code)
        lua_pushstring(_state, code);
    else
        lua_pushnil(_state);

    return CallEntryEvent(_creatureGossipEvents, creature->GetEntry(), GOSSIP_EVENT_ON_SELECT, 6);
}

bool TurtleLuaEngine::OnGameObjectGossipHello(Player* player, GameObject* go)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player || !go)
        return false;

    lua_pushinteger(_state, GOSSIP_EVENT_ON_HELLO);
    PushPlayer(player);
    PushGameObject(go);

    return CallEntryEvent(_gameObjectGossipEvents, go->GetEntry(), GOSSIP_EVENT_ON_HELLO, 3);
}

bool TurtleLuaEngine::OnGameObjectGossipSelect(Player* player, GameObject* go, uint32 sender, uint32 action, char const* code)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player || !go)
        return false;

    lua_pushinteger(_state, GOSSIP_EVENT_ON_SELECT);
    PushPlayer(player);
    PushGameObject(go);
    lua_pushinteger(_state, sender);
    lua_pushinteger(_state, action);
    if (code)
        lua_pushstring(_state, code);
    else
        lua_pushnil(_state);

    return CallEntryEvent(_gameObjectGossipEvents, go->GetEntry(), GOSSIP_EVENT_ON_SELECT, 6);
}

bool TurtleLuaEngine::OnItemGossipHello(Player* player, Item* item, SpellCastTargets& /*targets*/)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player || !item)
        return false;

    auto entryItr = _itemGossipEvents.find(item->GetEntry());
    if (entryItr == _itemGossipEvents.end() || entryItr->second.find(GOSSIP_EVENT_ON_HELLO) == entryItr->second.end())
        return false;

    player->PlayerTalkClass->ClearMenus();

    lua_pushinteger(_state, GOSSIP_EVENT_ON_HELLO);
    PushPlayer(player);
    PushItem(item);

    return CallEntryEventForBoolean(_itemGossipEvents, item->GetEntry(), GOSSIP_EVENT_ON_HELLO, 3, false);
}

bool TurtleLuaEngine::OnItemGossipSelect(Player* player, Item* item, uint32 sender, uint32 action, char const* code)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player || !item)
        return false;

    auto entryItr = _itemGossipEvents.find(item->GetEntry());
    if (entryItr == _itemGossipEvents.end() || entryItr->second.find(GOSSIP_EVENT_ON_SELECT) == entryItr->second.end())
        return false;

    player->PlayerTalkClass->ClearMenus();

    lua_pushinteger(_state, GOSSIP_EVENT_ON_SELECT);
    PushPlayer(player);
    PushItem(item);
    lua_pushinteger(_state, sender);
    lua_pushinteger(_state, action);
    if (code)
        lua_pushstring(_state, code);
    else
        lua_pushnil(_state);

    return CallEntryEvent(_itemGossipEvents, item->GetEntry(), GOSSIP_EVENT_ON_SELECT, 6);
}

bool TurtleLuaEngine::OnPlayerGossipSelect(Player* player, uint32 sender, uint32 action, char const* code)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player)
        return false;

    uint32 entry = player->GetObjectGuid().GetCounter();
    auto entryItr = _playerGossipEvents.find(entry);
    if (entryItr == _playerGossipEvents.end() || entryItr->second.find(GOSSIP_EVENT_ON_SELECT) == entryItr->second.end())
        return false;

    player->PlayerTalkClass->ClearMenus();

    lua_pushinteger(_state, GOSSIP_EVENT_ON_SELECT);
    PushPlayer(player);
    lua_pushinteger(_state, sender);
    lua_pushinteger(_state, action);
    if (code)
        lua_pushstring(_state, code);
    else
        lua_pushnil(_state);

    return CallEntryEvent(_playerGossipEvents, entry, GOSSIP_EVENT_ON_SELECT, 5);
}

bool TurtleLuaEngine::OnGameObjectUse(Player* player, GameObject* go)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player || !go)
        return false;

    lua_pushinteger(_state, GAMEOBJECT_EVENT_ON_USE);
    PushGameObject(go);
    PushPlayer(player);

    return CallEntryEvent(_gameObjectEvents, go->GetEntry(), GAMEOBJECT_EVENT_ON_USE, 3);
}

void TurtleLuaEngine::OnGameObjectLootStateChanged(GameObject* go, uint32 state)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !go)
        return;

    lua_pushinteger(_state, GAMEOBJECT_EVENT_ON_LOOT_STATE_CHANGE);
    PushGameObject(go);
    lua_pushinteger(_state, state);

    CallEntryEvent(_gameObjectEvents, go->GetEntry(), GAMEOBJECT_EVENT_ON_LOOT_STATE_CHANGE, 3);
}

void TurtleLuaEngine::OnGameObjectGoStateChanged(GameObject* go, uint32 state)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !go)
        return;

    lua_pushinteger(_state, GAMEOBJECT_EVENT_ON_GO_STATE_CHANGED);
    PushGameObject(go);
    lua_pushinteger(_state, state);

    CallEntryEvent(_gameObjectEvents, go->GetEntry(), GAMEOBJECT_EVENT_ON_GO_STATE_CHANGED, 3);
}

void TurtleLuaEngine::OnGameObjectAIUpdate(GameObject* go, uint32 diff)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !go)
        return;

    auto entryItr = _gameObjectEvents.find(go->GetEntry());
    if (entryItr == _gameObjectEvents.end() || entryItr->second.find(GAMEOBJECT_EVENT_ON_AIUPDATE) == entryItr->second.end())
        return;

    lua_pushinteger(_state, GAMEOBJECT_EVENT_ON_AIUPDATE);
    PushGameObject(go);
    lua_pushinteger(_state, diff);
    CallEntryEvent(_gameObjectEvents, go->GetEntry(), GAMEOBJECT_EVENT_ON_AIUPDATE, 3);
}

void TurtleLuaEngine::OnGameObjectSpawn(GameObject* go)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !go)
        return;

    lua_pushinteger(_state, GAMEOBJECT_EVENT_ON_SPAWN);
    PushGameObject(go);
    CallEntryEvent(_gameObjectEvents, go->GetEntry(), GAMEOBJECT_EVENT_ON_SPAWN, 2);
}

void TurtleLuaEngine::OnGameObjectAdd(GameObject* go)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !go)
        return;

    lua_pushinteger(_state, GAMEOBJECT_EVENT_ON_ADD);
    PushGameObject(go);
    CallEntryEvent(_gameObjectEvents, go->GetEntry(), GAMEOBJECT_EVENT_ON_ADD, 2);

    Map* map = go->GetMap();
    if (map && map->GetInstanceData())
    {
        lua_pushinteger(_state, INSTANCE_EVENT_ON_GAMEOBJECT_CREATE);
        PushInstanceDataValue(_state, map->GetInstanceData());
        PushMap(map);
        PushGameObject(go);
        CallInstanceEvent(map, INSTANCE_EVENT_ON_GAMEOBJECT_CREATE, 4);
    }
}

void TurtleLuaEngine::OnGameObjectRemove(GameObject* go)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !go)
        return;

    lua_pushinteger(_state, GAMEOBJECT_EVENT_ON_REMOVE);
    PushGameObject(go);
    CallEntryEvent(_gameObjectEvents, go->GetEntry(), GAMEOBJECT_EVENT_ON_REMOVE, 2);
}

bool TurtleLuaEngine::OnGameObjectDummyEffect(WorldObject* caster, uint32 spellId, uint32 effIndex, GameObject* target)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !target)
        return false;

    lua_pushinteger(_state, GAMEOBJECT_EVENT_ON_DUMMY_EFFECT);
    if (Unit* casterUnit = dynamic_cast<Unit*>(caster))
        PushUnit(casterUnit);
    else if (GameObject* casterGo = dynamic_cast<GameObject*>(caster))
        PushGameObject(casterGo);
    else
        lua_pushnil(_state);
    lua_pushinteger(_state, spellId);
    lua_pushinteger(_state, effIndex);
    PushGameObject(target);

    return CallEntryEvent(_gameObjectEvents, target->GetEntry(), GAMEOBJECT_EVENT_ON_DUMMY_EFFECT, 5);
}

bool TurtleLuaEngine::OnItemUse(Player* player, Item* item, SpellCastTargets& targets)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player || !item)
        return false;

    lua_pushinteger(_state, ITEM_EVENT_ON_USE);
    PushPlayer(player);
    PushItem(item);
    PushSpellTargets(_state, &targets);

    return CallEntryEventForBoolean(_itemEvents, item->GetEntry(), ITEM_EVENT_ON_USE, 4, false);
}

bool TurtleLuaEngine::OnItemDummyEffect(WorldObject* caster, uint32 spellId, uint32 effIndex, Item* target)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !target)
        return false;

    lua_pushinteger(_state, ITEM_EVENT_ON_DUMMY_EFFECT);
    if (Unit* casterUnit = dynamic_cast<Unit*>(caster))
        PushUnit(casterUnit);
    else if (GameObject* casterGo = dynamic_cast<GameObject*>(caster))
        PushGameObject(casterGo);
    else
        lua_pushnil(_state);
    lua_pushinteger(_state, spellId);
    lua_pushinteger(_state, effIndex);
    PushItem(target);

    return CallEntryEvent(_itemEvents, target->GetEntry(), ITEM_EVENT_ON_DUMMY_EFFECT, 5);
}

bool TurtleLuaEngine::OnItemExpire(Player* player, ItemPrototype const* proto)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player || !proto)
        return false;

    lua_pushinteger(_state, ITEM_EVENT_ON_EXPIRE);
    PushPlayer(player);
    lua_pushinteger(_state, proto->ItemId);

    return CallEntryEventForBoolean(_itemEvents, proto->ItemId, ITEM_EVENT_ON_EXPIRE, 3, true);
}

bool TurtleLuaEngine::OnItemRemove(Player* player, Item* item)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player || !item)
        return false;

    lua_pushinteger(_state, ITEM_EVENT_ON_REMOVE);
    PushPlayer(player);
    PushItem(item);

    return CallEntryEventForBoolean(_itemEvents, item->GetEntry(), ITEM_EVENT_ON_REMOVE, 3, true);
}

void TurtleLuaEngine::OnSpellPrepare(WorldObject* caster, Spell* spell)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !spell || !spell->m_spellInfo)
        return;

    lua_pushinteger(_state, SPELL_EVENT_ON_PREPARE);
    if (Unit* casterUnit = dynamic_cast<Unit*>(caster))
        PushUnit(casterUnit);
    else if (GameObject* casterGo = dynamic_cast<GameObject*>(caster))
        PushGameObject(casterGo);
    else
        lua_pushnil(_state);
    PushSpell(spell);

    CallEntryEventIgnoreResult(_spellEvents, spell->m_spellInfo->Id, SPELL_EVENT_ON_PREPARE, 3);
}

void TurtleLuaEngine::OnSpellCast(WorldObject* caster, Spell* spell, bool skipCheck)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !spell || !spell->m_spellInfo)
        return;

    lua_pushinteger(_state, SPELL_EVENT_ON_CAST);
    if (Unit* casterUnit = dynamic_cast<Unit*>(caster))
        PushUnit(casterUnit);
    else if (GameObject* casterGo = dynamic_cast<GameObject*>(caster))
        PushGameObject(casterGo);
    else
        lua_pushnil(_state);
    PushSpell(spell);
    lua_pushboolean(_state, skipCheck);

    CallEntryEventIgnoreResult(_spellEvents, spell->m_spellInfo->Id, SPELL_EVENT_ON_CAST, 4);
}

void TurtleLuaEngine::OnSpellCastCancel(WorldObject* caster, Spell* spell, bool bySelf)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !spell || !spell->m_spellInfo)
        return;

    lua_pushinteger(_state, SPELL_EVENT_ON_CAST_CANCEL);
    if (Unit* casterUnit = dynamic_cast<Unit*>(caster))
        PushUnit(casterUnit);
    else if (GameObject* casterGo = dynamic_cast<GameObject*>(caster))
        PushGameObject(casterGo);
    else
        lua_pushnil(_state);
    PushSpell(spell);
    lua_pushboolean(_state, bySelf);

    CallEntryEventIgnoreResult(_spellEvents, spell->m_spellInfo->Id, SPELL_EVENT_ON_CAST_CANCEL, 4);
}

bool TurtleLuaEngine::OnCreatureQuestAccept(Player* player, Creature* creature, Quest const* quest)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player || !creature || !quest)
        return false;

    lua_pushinteger(_state, CREATURE_EVENT_ON_QUEST_ACCEPT);
    PushPlayer(player);
    PushCreature(creature);
    PushQuest(_state, quest);

    return CallCreatureEvent(creature, CREATURE_EVENT_ON_QUEST_ACCEPT, 4);
}

bool TurtleLuaEngine::OnCreatureQuestReward(Player* player, Creature* creature, Quest const* quest)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player || !creature || !quest)
        return false;

    lua_pushinteger(_state, CREATURE_EVENT_ON_QUEST_REWARD);
    PushPlayer(player);
    PushCreature(creature);
    PushQuest(_state, quest);

    return CallCreatureEvent(creature, CREATURE_EVENT_ON_QUEST_REWARD, 4);
}

bool TurtleLuaEngine::OnGameObjectQuestAccept(Player* player, GameObject* go, Quest const* quest)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player || !go || !quest)
        return false;

    lua_pushinteger(_state, GAMEOBJECT_EVENT_ON_QUEST_ACCEPT);
    PushPlayer(player);
    PushGameObject(go);
    PushQuest(_state, quest);

    return CallEntryEvent(_gameObjectEvents, go->GetEntry(), GAMEOBJECT_EVENT_ON_QUEST_ACCEPT, 4);
}

bool TurtleLuaEngine::OnGameObjectQuestReward(Player* player, GameObject* go, Quest const* quest)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player || !go || !quest)
        return false;

    lua_pushinteger(_state, GAMEOBJECT_EVENT_ON_QUEST_REWARD);
    PushPlayer(player);
    PushGameObject(go);
    PushQuest(_state, quest);

    return CallEntryEvent(_gameObjectEvents, go->GetEntry(), GAMEOBJECT_EVENT_ON_QUEST_REWARD, 4);
}

bool TurtleLuaEngine::OnItemQuestAccept(Player* player, Item* item, Quest const* quest)
{
    std::lock_guard<std::recursive_mutex> guard(_lock);

    if (!IsEnabled() || !player || !item || !quest)
        return false;

    lua_pushinteger(_state, ITEM_EVENT_ON_QUEST_ACCEPT);
    PushPlayer(player);
    PushItem(item);
    PushQuest(_state, quest);

    return CallEntryEvent(_itemEvents, item->GetEntry(), ITEM_EVENT_ON_QUEST_ACCEPT, 4);
}

void TurtleLuaEngine::LogError(char const* context)
{
    char const* message = lua_tostring(_state, -1);
    sLog.outError("[Lua] %s: %s", context ? context : "error", message ? message : "unknown Lua error");
}
