#ifndef TURTLE_LUA_ENGINE_H
#define TURTLE_LUA_ENGINE_H

#include "Common.h"
#include "ObjectGuid.h"

#include <map>
#include <mutex>
#include <string>
#include <vector>

struct lua_State;
class Player;
class Creature;
class GameObject;
class Corpse;
class Item;
class Unit;
class WorldObject;
class Quest;
class Group;
class Guild;
class Map;
class Spell;
class Roll;
class InstanceData;
class WorldPacket;
class WorldSession;
class SpellCastTargets;
class ObjectGuid;
struct ItemPrototype;

enum TurtleLuaPlayerEvents
{
    PLAYER_EVENT_ON_CHARACTER_CREATE = 1,
    PLAYER_EVENT_ON_CHARACTER_DELETE = 2,
    PLAYER_EVENT_ON_LOGIN = 3,
    PLAYER_EVENT_ON_LOGOUT = 4,
    PLAYER_EVENT_ON_SPELL_CAST = 5,
    PLAYER_EVENT_ON_KILL_PLAYER = 6,
    PLAYER_EVENT_ON_KILL_CREATURE = 7,
    PLAYER_EVENT_ON_KILLED_BY_CREATURE = 8,
    PLAYER_EVENT_ON_DUEL_REQUEST = 9,
    PLAYER_EVENT_ON_DUEL_START = 10,
    PLAYER_EVENT_ON_DUEL_END = 11,
    PLAYER_EVENT_ON_GIVE_XP = 12,
    PLAYER_EVENT_ON_LEVEL_CHANGE = 13,
    PLAYER_EVENT_ON_MONEY_CHANGE = 14,
    PLAYER_EVENT_ON_REPUTATION_CHANGE = 15,
    PLAYER_EVENT_ON_TALENTS_CHANGE = 16,
    PLAYER_EVENT_ON_TALENTS_RESET = 17,
    PLAYER_EVENT_ON_CHAT = 18,
    PLAYER_EVENT_ON_WHISPER = 19,
    PLAYER_EVENT_ON_GROUP_CHAT = 20,
    PLAYER_EVENT_ON_GUILD_CHAT = 21,
    PLAYER_EVENT_ON_CHANNEL_CHAT = 22,
    PLAYER_EVENT_ON_EMOTE = 23,
    PLAYER_EVENT_ON_TEXT_EMOTE = 24,
    PLAYER_EVENT_ON_SAVE = 25,
    PLAYER_EVENT_ON_BIND_TO_INSTANCE = 26,
    PLAYER_EVENT_ON_UPDATE_ZONE = 27,
    PLAYER_EVENT_ON_MAP_CHANGE = 28,
    PLAYER_EVENT_ON_EQUIP = 29,
    PLAYER_EVENT_ON_FIRST_LOGIN = 30,
    PLAYER_EVENT_ON_CAN_USE_ITEM = 31,
    PLAYER_EVENT_ON_LOOT_ITEM = 32,
    PLAYER_EVENT_ON_ENTER_COMBAT = 33,
    PLAYER_EVENT_ON_LEAVE_COMBAT = 34,
    PLAYER_EVENT_ON_REPOP = 35,
    PLAYER_EVENT_ON_RESURRECT = 36,
    PLAYER_EVENT_ON_LOOT_MONEY = 37,
    PLAYER_EVENT_ON_QUEST_ABANDON = 38,
    PLAYER_EVENT_ON_LEARN_TALENTS = 39,
    PLAYER_EVENT_ON_COMMAND = 42,
    PLAYER_EVENT_ON_PET_ADDED_TO_WORLD = 43,
    PLAYER_EVENT_ON_LEARN_SPELL = 44,
    PLAYER_EVENT_ON_FFAPVP_CHANGE = 46,
    PLAYER_EVENT_ON_UPDATE_AREA = 47,
    PLAYER_EVENT_ON_CAN_INIT_TRADE = 48,
    PLAYER_EVENT_ON_CAN_SEND_MAIL = 49,
    PLAYER_EVENT_ON_QUEST_REWARD_ITEM = 51,
    PLAYER_EVENT_ON_CREATE_ITEM = 52,
    PLAYER_EVENT_ON_STORE_NEW_ITEM = 53,
    PLAYER_EVENT_ON_COMPLETE_QUEST = 54,
    PLAYER_EVENT_ON_CAN_GROUP_INVITE = 55,
    PLAYER_EVENT_ON_GROUP_ROLL_REWARD_ITEM = 56,
    PLAYER_EVENT_ON_BG_DESERTION = 57,
    PLAYER_EVENT_ON_PET_KILL = 58,
    PLAYER_EVENT_ON_CAN_RESURRECT = 59,
    PLAYER_EVENT_ON_CAN_UPDATE_SKILL = 60,
    PLAYER_EVENT_ON_BEFORE_UPDATE_SKILL = 61,
    PLAYER_EVENT_ON_UPDATE_SKILL = 62,
};

enum TurtleLuaServerEvents
{
    SERVER_EVENT_ON_PACKET_RECEIVE = 5,
    SERVER_EVENT_ON_PACKET_RECEIVE_UNKNOWN = 6,
    SERVER_EVENT_ON_PACKET_SEND = 7,
    WORLD_EVENT_ON_UPDATE = 13,
    WORLD_EVENT_ON_STARTUP = 14,
    WORLD_EVENT_ON_SHUTDOWN = 15,
    ELUNA_EVENT_ON_LUA_STATE_CLOSE = 16,
    ELUNA_EVENT_ON_LUA_STATE_OPEN = 33,
};

enum TurtleLuaPacketEvents
{
    PACKET_EVENT_ON_PACKET_RECEIVE = 5,
    PACKET_EVENT_ON_PACKET_RECEIVE_UNKNOWN = 6,
    PACKET_EVENT_ON_PACKET_SEND = 7,
};

enum TurtleLuaCreatureEvents
{
    CREATURE_EVENT_ON_ENTER_COMBAT = 1,
    CREATURE_EVENT_ON_LEAVE_COMBAT = 2,
    CREATURE_EVENT_ON_TARGET_DIED = 3,
    CREATURE_EVENT_ON_DIED = 4,
    CREATURE_EVENT_ON_SPAWN = 5,
    CREATURE_EVENT_ON_REACH_WP = 6,
    CREATURE_EVENT_ON_RECEIVE_EMOTE = 8,
    CREATURE_EVENT_ON_DAMAGE_TAKEN = 9,
    CREATURE_EVENT_ON_OWNER_ATTACKED = 12,
    CREATURE_EVENT_ON_OWNER_ATTACKED_AT = 13,
    CREATURE_EVENT_ON_HIT_BY_SPELL = 14,
    CREATURE_EVENT_ON_SPELL_HIT_TARGET = 15,
    CREATURE_EVENT_ON_JUST_SUMMONED_CREATURE = 19,
    CREATURE_EVENT_ON_SUMMONED_CREATURE_DESPAWN = 20,
    CREATURE_EVENT_ON_RESET = 23,
    CREATURE_EVENT_ON_REACH_HOME = 24,
    CREATURE_EVENT_ON_CORPSE_REMOVED = 26,
    CREATURE_EVENT_ON_MOVE_IN_LOS = 27,
    CREATURE_EVENT_ON_AIUPDATE = 7,
    CREATURE_EVENT_ON_DUMMY_EFFECT = 30,
    CREATURE_EVENT_ON_QUEST_ACCEPT = 31,
    CREATURE_EVENT_ON_QUEST_REWARD = 34,
    CREATURE_EVENT_ON_DIALOG_STATUS = 35,
    CREATURE_EVENT_ON_ADD = 36,
    CREATURE_EVENT_ON_REMOVE = 37,
};

enum TurtleLuaGameObjectEvents
{
    GAMEOBJECT_EVENT_ON_AIUPDATE = 1,
    GAMEOBJECT_EVENT_ON_SPAWN = 2,
    GAMEOBJECT_EVENT_ON_DUMMY_EFFECT = 3,
    GAMEOBJECT_EVENT_ON_QUEST_ACCEPT = 4,
    GAMEOBJECT_EVENT_ON_QUEST_REWARD = 5,
    GAMEOBJECT_EVENT_ON_DIALOG_STATUS = 6,
    GAMEOBJECT_EVENT_ON_LOOT_STATE_CHANGE = 9,
    GAMEOBJECT_EVENT_ON_GO_STATE_CHANGED = 10,
    GAMEOBJECT_EVENT_ON_ADD = 12,
    GAMEOBJECT_EVENT_ON_REMOVE = 13,
    GAMEOBJECT_EVENT_ON_USE = 14,
};

enum TurtleLuaItemEvents
{
    ITEM_EVENT_ON_DUMMY_EFFECT = 1,
    ITEM_EVENT_ON_USE = 2,
    ITEM_EVENT_ON_QUEST_ACCEPT = 3,
    ITEM_EVENT_ON_EXPIRE = 4,
    ITEM_EVENT_ON_REMOVE = 5,
};

enum TurtleLuaGossipEvents
{
    GOSSIP_EVENT_ON_HELLO = 1,
    GOSSIP_EVENT_ON_SELECT = 2,
};

enum TurtleLuaSpellEvents
{
    SPELL_EVENT_ON_PREPARE = 1,
    SPELL_EVENT_ON_CAST = 2,
    SPELL_EVENT_ON_CAST_CANCEL = 3,
};

class TurtleLuaEngine
{
public:
    TurtleLuaEngine();
    ~TurtleLuaEngine();

    void Initialize();
    void Shutdown();
    void Reload();
    void Update(uint32 diff);

    void OnPlayerLogin(Player* player);
    void OnPlayerLogout(Player* player);
    void OnPlayerCharacterCreate(Player* player);
    void OnPlayerCharacterDelete(uint32 guidLow);
    void OnPlayerSpellCast(Player* player, Spell* spell, bool skipCheck);
    void OnPlayerKillPlayer(Player* killer, Player* killed);
    void OnPlayerKillCreature(Player* killer, Creature* killed);
    void OnPlayerKilledByCreature(Creature* killer, Player* killed);
    void OnPlayerDuelRequest(Player* target, Player* challenger);
    void OnPlayerDuelStart(Player* player1, Player* player2);
    void OnPlayerDuelEnd(Player* winner, Player* loser, uint32 type);
    void OnPlayerGiveXP(Player* player, uint32& amount, Unit* victim, uint8 source);
    void OnPlayerLevelChange(Player* player, uint32 oldLevel);
    void OnPlayerMoneyChange(Player* player, int32& amount);
    bool OnPlayerReputationChange(Player* player, uint32 factionId, int32& standing, bool incremental);
    void OnPlayerTalentsChange(Player* player, uint32 points);
    void OnPlayerTalentsReset(Player* player, bool noCost);
    void OnPlayerLearnTalents(Player* player, uint32 talentId, uint32 talentRank, uint32 spellId);
    void OnPlayerFirstLogin(Player* player);
    void OnPlayerRepop(Player* player);
    void OnPlayerResurrect(Player* player);
    void OnPlayerEquip(Player* player, Item* item, uint8 bag, uint8 slot);
    uint32 OnPlayerCanUseItem(Player const* player, uint32 itemEntry);
    void OnPlayerLootItem(Player* player, Item* item, uint32 count, ObjectGuid const& guid);
    void OnPlayerLootMoney(Player* player, uint32 amount);
    void OnPlayerQuestAbandon(Player* player, uint32 questId);
    void OnPetAddedToWorld(Player* player, Creature* pet);
    void OnPlayerLearnSpell(Player* player, uint32 spellId);
    void OnPlayerFFAPvPChange(Player* player, bool hasFfaPvp);
    void OnPlayerPetKill(Player* player, Creature* killed);
    bool OnPlayerCommand(Player* player, std::string const& command);
    bool OnPlayerCanInitTrade(Player* player, Player* target);
    bool OnPlayerCanSendMail(Player* player, ObjectGuid const& receiverGuid, ObjectGuid const& mailboxGuid, std::string const& subject, std::string const& body, uint32 money, uint32 cod, Item* item);
    void OnPlayerEnterCombat(Player* player, Unit* enemy);
    void OnPlayerLeaveCombat(Player* player);
    void OnPlayerQuestRewardItem(Player* player, Item* item, uint32 count);
    void OnPlayerCreateItem(Player* player, Item* item, uint32 count);
    void OnPlayerStoreNewItem(Player* player, Item* item, uint32 count);
    void OnPlayerCompleteQuest(Player* player, Quest const* quest);
    void OnPlayerGroupRollRewardItem(Player* player, Item* item, uint32 count, uint32 voteType, Roll const* roll);
    bool OnPacketReceive(WorldSession* session, WorldPacket& packet);
    bool OnPacketSend(WorldSession* session, WorldPacket& packet);
    void OnPlayerBGDesertion(Player* player, uint32 type);
    bool OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& message);
    bool OnPlayerWhisper(Player* player, uint32 type, uint32 lang, std::string& message, Player* receiver);
    bool OnPlayerGroupChat(Player* player, uint32 type, uint32 lang, std::string& message, Group* group);
    bool OnPlayerGuildChat(Player* player, uint32 type, uint32 lang, std::string& message, Guild* guild);
    bool OnPlayerChannelChat(Player* player, uint32 type, uint32 lang, std::string& message, std::string const& channelName);
    void OnPlayerEmote(Player* player, uint32 emote);
    bool OnPlayerCanGroupInvite(Player* player, std::string const& memberName);
    bool OnPlayerCanResurrect(Player* player);
    bool OnPlayerCanUpdateSkill(Player* player, uint32 skillId);
    void OnPlayerBeforeUpdateSkill(Player* player, uint32 skillId, uint32& value, uint32 max, uint32 step);
    void OnPlayerUpdateSkill(Player* player, uint32 skillId, uint32 value, uint32 max, uint32 step, uint32 newValue);
    void OnPlayerTextEmote(Player* player, uint32 textEmote, uint32 emoteNum, ObjectGuid const& guid);
    void OnPlayerSave(Player* player);
    void OnPlayerBindToInstance(Player* player, uint32 difficulty, uint32 mapId, bool permanent);
    void OnPlayerUpdateZone(Player* player, uint32 newZone, uint32 newArea);
    void OnPlayerMapChanged(Player* player);
    void OnPlayerUpdateArea(Player* player, uint32 oldArea, uint32 newArea);
    void OnCreatureEnterCombat(Creature* creature, Unit* target);
    void OnCreatureLeaveCombat(Creature* creature);
    void OnCreatureTargetDied(Creature* creature, Unit* victim);
    void OnCreatureDied(Creature* creature, Unit* killer);
    void OnCreatureSpawn(Creature* creature);
    void OnCreatureAdd(Creature* creature);
    void OnCreatureRemove(Creature* creature);
    bool OnCreatureReachWP(Creature* creature, uint32 type, uint32 id);
    void OnCreatureReset(Creature* creature);
    bool OnCreatureReachHome(Creature* creature);
    void OnCreatureAIUpdate(Creature* creature, uint32 diff);
    bool OnCreatureDamageTaken(Creature* creature, Unit* attacker, uint32& damage);
    bool OnCreatureOwnerAttacked(Creature* creature, Unit* target);
    bool OnCreatureOwnerAttackedAt(Creature* creature, Unit* attacker);
    bool OnCreatureHitBySpell(Creature* creature, WorldObject* caster, uint32 spellId);
    bool OnCreatureSpellHitTarget(Creature* creature, Unit* target, uint32 spellId);
    bool OnCreatureJustSummoned(Creature* creature, Creature* summon);
    bool OnCreatureSummonedCreatureDespawn(Creature* creature, Creature* summon);
    bool OnCreatureMoveInLOS(Creature* creature, Unit* who);
    bool OnCreatureReceiveEmote(Creature* creature, Player* player, uint32 emoteId);
    bool OnCreatureCorpseRemoved(Creature* creature, uint32& respawnDelay);
    bool OnCreatureDummyEffect(WorldObject* caster, uint32 spellId, uint32 effIndex, Creature* target);

    void RegisterPlayerEvent(uint32 eventId, int functionRef);
    void RegisterServerEvent(uint32 eventId, int functionRef);
    void RegisterCreatureEvent(uint32 entry, uint32 eventId, int functionRef);
    void RegisterGameObjectEvent(uint32 entry, uint32 eventId, int functionRef);
    void RegisterItemEvent(uint32 entry, uint32 eventId, int functionRef);
    void RegisterSpellEvent(uint32 entry, uint32 eventId, int functionRef);
    void RegisterPacketEvent(uint32 opcode, uint32 eventId, int functionRef);
    void RegisterCreatureGossipEvent(uint32 entry, uint32 eventId, int functionRef);
    void RegisterGameObjectGossipEvent(uint32 entry, uint32 eventId, int functionRef);
    void RegisterItemGossipEvent(uint32 entry, uint32 eventId, int functionRef);
    void ClearPlayerEvents(uint32 eventId, bool allEvents);
    void ClearServerEvents(uint32 eventId, bool allEvents);
    void ClearCreatureEvents(uint32 entry, uint32 eventId, bool allEvents);
    void ClearGameObjectEvents(uint32 entry, uint32 eventId, bool allEvents);
    void ClearItemEvents(uint32 entry, uint32 eventId, bool allEvents);
    void ClearSpellEvents(uint32 entry, uint32 eventId, bool allEvents);
    void ClearPacketEvents(uint32 opcode, uint32 eventId, bool allEvents);
    void ClearCreatureGossipEvents(uint32 entry, uint32 eventId, bool allEvents);
    void ClearGameObjectGossipEvents(uint32 entry, uint32 eventId, bool allEvents);
    void ClearItemGossipEvents(uint32 entry, uint32 eventId, bool allEvents);

    bool OnCreatureGossipHello(Player* player, Creature* creature);
    bool OnCreatureGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action, char const* code);
    bool OnGameObjectGossipHello(Player* player, GameObject* go);
    bool OnGameObjectGossipSelect(Player* player, GameObject* go, uint32 sender, uint32 action, char const* code);
    bool OnItemGossipHello(Player* player, Item* item, SpellCastTargets& targets);
    bool OnItemGossipSelect(Player* player, Item* item, uint32 sender, uint32 action, char const* code);
    bool OnGameObjectUse(Player* player, GameObject* go);
    void OnGameObjectLootStateChanged(GameObject* go, uint32 state);
    void OnGameObjectGoStateChanged(GameObject* go, uint32 state);
    void OnGameObjectAIUpdate(GameObject* go, uint32 diff);
    void OnGameObjectSpawn(GameObject* go);
    void OnGameObjectAdd(GameObject* go);
    void OnGameObjectRemove(GameObject* go);
    bool OnGameObjectDummyEffect(WorldObject* caster, uint32 spellId, uint32 effIndex, GameObject* target);
    bool OnItemUse(Player* player, Item* item, SpellCastTargets& targets);
    bool OnItemDummyEffect(WorldObject* caster, uint32 spellId, uint32 effIndex, Item* target);
    bool OnItemExpire(Player* player, ItemPrototype const* proto);
    bool OnItemRemove(Player* player, Item* item);
    void OnSpellPrepare(WorldObject* caster, Spell* spell);
    void OnSpellCast(WorldObject* caster, Spell* spell, bool skipCheck);
    void OnSpellCastCancel(WorldObject* caster, Spell* spell, bool bySelf);
    bool OnCreatureQuestAccept(Player* player, Creature* creature, Quest const* quest);
    bool OnCreatureQuestReward(Player* player, Creature* creature, Quest const* quest);
    bool OnGameObjectQuestAccept(Player* player, GameObject* go, Quest const* quest);
    bool OnGameObjectQuestReward(Player* player, GameObject* go, Quest const* quest);
    bool OnItemQuestAccept(Player* player, Item* item, Quest const* quest);
    bool IsEnabled() const { return _enabled && _state; }

    void PushPlayer(Player* player);
    void PushCreature(Creature* creature);
    void PushGameObject(GameObject* go);
    void PushCorpse(Corpse* corpse);
    void PushItem(Item* item);
    void PushUnit(Unit* unit);
    void PushGroup(Group* group);
    void PushGuild(Guild* guild);
    void PushMap(Map* map);
    void PushSpell(Spell* spell);
    void PushObjectGuid(ObjectGuid const& guid);

    uint32 CreateTimedEvent(int functionRef, uint32 delay, uint32 repeats);
    uint32 CreateTimedEvent(int functionRef, uint32 minDelay, uint32 maxDelay, uint32 repeats, WorldObject* object);
    bool RemoveTimedEvent(uint32 eventId);
    bool RemoveTimedEventForObject(uint32 eventId, ObjectGuid const& objectGuid);
    uint32 RemoveTimedEventsForObject(ObjectGuid const& objectGuid);

private:
    struct TimedEvent
    {
        uint32 id;
        int functionRef;
        uint32 delay;
        uint32 minDelay;
        uint32 maxDelay;
        uint32 elapsed;
        int32 remaining;
        bool hasObject;
        ObjectGuid objectGuid;
        uint32 mapId;
        uint32 instanceId;
    };

    void OpenState();
    void CloseState();
    void LoadScripts();
    void LoadScriptFile(std::string const& path);
    void RegisterGlobals();
    void RegisterPlayerMetatable();
    void RegisterCreatureMetatable();
    void RegisterGameObjectMetatable();
    void RegisterCorpseMetatable();
    void RegisterGameObjectTemplateMetatable();
    void RegisterItemMetatable();
    void RegisterItemTemplateMetatable();
    void RegisterCreatureTemplateMetatable();
    void RegisterQuestMetatable();
    void RegisterGroupMetatable();
    void RegisterGuildMetatable();
    void RegisterMapMetatable();
    void RegisterInstanceDataMetatable();
    void RegisterAuraMetatable();
    void RegisterSpellMetatable();
    void RegisterSpellInfoMetatable();
    void RegisterSpellTargetsMetatable();
    void RegisterWorldPacketMetatable();
    void RegisterObjectGuidMetatable();
    void RegisterChatHandlerMetatable();
    void RegisterRollMetatable();
    void RegisterUnitMetatable();

    void LogError(char const* context);
    uint32 GenerateTimedEventDelay(TimedEvent const& event) const;
    WorldObject* ResolveTimedEventObject(TimedEvent const& event);
    bool CallPlayerEvent(uint32 eventId, Player* player);
    bool CallPlayerChatEvent(uint32 eventId, char const* context, Player* player, uint32 type, uint32 lang, std::string& message, Player* receiver, Group* group, Guild* guild, char const* channelName);
    void CallPlayerItemEvent(uint32 eventId, char const* context, Player* player, Item* item, uint32 count);
    bool CallServerPacketEvent(uint32 eventId, WorldPacket& packet, Player* player, char const* context);
    bool CallPacketEvent(uint32 opcode, uint32 eventId, WorldPacket& packet, Player* player, char const* context);
    bool CallPacketFunctionRefs(std::vector<int> const& functionRefs, uint32 eventId, WorldPacket& packet, Player* player, char const* context);
    void CallServerEvent(uint32 eventId);
    void CallServerEvent(uint32 eventId, uint32 arg);
    bool CallEntryEvent(std::map<uint32, std::map<uint32, std::vector<int>>>& store, uint32 entry, uint32 eventId, int argCount);
    bool CallEntryEventForBoolean(std::map<uint32, std::map<uint32, std::vector<int>>>& store, uint32 entry, uint32 eventId, int argCount, bool expectedValue);
    void CallEntryEventIgnoreResult(std::map<uint32, std::map<uint32, std::vector<int>>>& store, uint32 entry, uint32 eventId, int argCount);
    void UpdateTimedEvents(uint32 diff);

    lua_State* _state;
    bool _enabled;
    bool _reloadPending;
    uint32 _nextTimedEventId;
    std::string _scriptPath;
    std::recursive_mutex _lock;
    std::map<uint32, std::vector<int>> _serverEvents;
    std::map<uint32, std::vector<int>> _playerEvents;
    std::map<uint32, std::map<uint32, std::vector<int>>> _creatureEvents;
    std::map<uint32, std::map<uint32, std::vector<int>>> _gameObjectEvents;
    std::map<uint32, std::map<uint32, std::vector<int>>> _itemEvents;
    std::map<uint32, std::map<uint32, std::vector<int>>> _spellEvents;
    std::map<uint32, std::map<uint32, std::vector<int>>> _packetEvents;
    std::map<uint32, std::map<uint32, std::vector<int>>> _creatureGossipEvents;
    std::map<uint32, std::map<uint32, std::vector<int>>> _gameObjectGossipEvents;
    std::map<uint32, std::map<uint32, std::vector<int>>> _itemGossipEvents;
    std::vector<TimedEvent> _timedEvents;
};

extern TurtleLuaEngine sTurtleLuaEngine;

#endif
