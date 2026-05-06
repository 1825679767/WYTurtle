# Lua 移植状态记录

这份文档记录 `E:\GIT\WYwow\modules\mod-eluna` 的 3.3.5 Eluna 功能移植到
`E:\GIT\WYTurtle` 这份 Turtle/MaNGOS 1.12 源码后的当前状态。

当前实现不是把 AzerothCore 3.3.5 的 Eluna 原样照搬，而是做了一套适配
Turtle 1.12 核心结构的 Lua 兼容层。它已经可以加载自定义 Lua 脚本，并支持
常用事件、Map/Instance 生命周期、战场事件、工单事件、Gossip、任务对象、物品模板、生物模板、GameObject 模板、法术模板、施法目标、ObjectGuid、WorldPacket、Roll、InstanceData、BattleGround、Ticket、玩家消息/提示发送、数据库访问、定时器和大量常用对象方法。

## 当前实现位置

```text
E:\GIT\WYTurtle\src\game\LuaEngine\TurtleLuaEngine.h
E:\GIT\WYTurtle\src\game\LuaEngine\TurtleLuaEngine.cpp
E:\GIT\WYTurtle\lua_scripts\example.lua
```

Lua 5.2 构建目录：

```text
E:\GIT\WYTurtle\dep\lualib\lua
```

安装输出目录：

```text
E:\TurtleBY
```

## 编译状态

当前状态：上一次已经通过 Release 编译；按当前要求，本轮后续新增函数暂不做中途完整编译，等 Lua 功能补齐后再统一编译和安装验证。

最近新增并验证：

- Creature 3.3.5 参考方法名补齐：本轮把 `CreatureMethods.h` 中 87 个参考方法名全部注册进 `RegisterCreatureMetatable()`；当前差异扫描结果为 `ref=87 target=340 missing=0`。真实接入的接口包括移动能力、精英/世界 Boss/守卫/触发器/平民/阵营领袖判断、任务和法术查询、法术冷却、NPC flags、Unit flags、模板字段、AI 名称、脚本名、脚本 ID、格挡值、归巢点、游荡半径、默认移动类型、当前路径点、拾取归属、仇恨目标列表、AI 选目标、攻击/呼救/协助、可否起手攻击、仇恨开关、进入全区战斗、尸体移除、重生、保存到数据库、变更 entry、死亡状态、反应状态、行走/悬浮、装备槽、禁止呼救/禁止搜索协助等。
- Creature 1.12 兼容占位：Turtle 1.12 没有 3.3.5 的 LootMode、`UNIT_FIELD_FLAGS_2`、声望奖励禁用字段、公开尸体延迟 getter、Creature 数字 waypoint path 和 3.3.5 DungeonBoss 独立标记，所以这些接口当前按兼容方式处理：`GetLootMode()` 固定返回默认模式 `1`，`HasLootMode(1)` 返回真，其他 loot mode 修改函数为 no-op；`GetUnitFlagsTwo()` / `GetWaypointPath()` / `GetCorpseDelay()` 返回 `0`；`SetUnitFlagsTwo()`、`SetDisableGravity()`、`SetDisableReputationGain()` 为 no-op；`IsReputationGainDisabled()`、`IsDamageEnoughForLootingAndReward()` 返回 `false`；`IsDungeonBoss()` 暂按 `IsWorldBoss()` 近似。
- Player 3.3.5 参考方法名补齐：本轮把 `PlayerMethods.h` 里剩余的 82 个缺失方法名全部注册进 `RegisterPlayerMetatable()`；当前差异扫描结果为 `ref=254 target=521 missing=0`。其中真实/近似接入的接口包括 `CanUninviteFromGroup`、`EquipItem`、`GetCompletedQuestsCount`、`GetCorpse`、`GetGossipTextId`、`GetHealthBonusFromStamina`、`GetHonorPoints`、`GetMailCount`、`GetMailItem`、`GetManaBonusFromIntellect`、`GetNextRandomRaidMember`、`GetPhaseMaskForSpawn`、`GetShieldBlockValue`、`GossipAddQuests`、`GossipSendPOI`、`GroupCreate`、`GroupInvite`、`HasTalent`、`HasTitle`、`LeaveBattleground`、`LogoutPlayer`、`ModifyHonorPoints`、`Mute`、`RemovedInsignia`、`RemoveFromBattlegroundRaid`、`RemoveFromGroup`、`ResetTalentsCost`、`ResetTypeCooldowns`、`RunCommand`、`SendAuctionMenu`、`SendGuildInvite`、`SendListInventory`、`SendMovieStart`、`SendQuestTemplate`、`SendShowBank`、`SendSpiritResurrect`、`SendTabardVendorActivate`、`SendTaxiMenu`、`SendTrainerList`、`SetFactionForRace`、`SetGender`、`SetGuildRank`、`SetHonorPoints`、`SetKnownTitle`、`SetPlayerLock`、`StartTaxi`、`SummonPlayer`、`UnbindAllInstances`、`UnbindInstance`、`UnsetKnownTitle`、`Whisper`。本批只做差异扫描和 Lua 示例语法检查，不做完整 CMake 编译。
- Player 版本兼容占位：`CanTitanGrip`、成就系统、竞技场点数、雕文、双天赋/专精、职责专精判断、冠军战袍阵营、玩家设置、额外天赋点、`SendShowMailBox` 等接口在 Turtle 1.12 里没有对应 3.3.5 数据结构或安全直接入口，因此当前注册为返回 `0` / `false` / `nil` 或 no-op。这样旧 Eluna 脚本至少不会因为函数不存在而报错；后续如果确认 Turtle 有对应自定义系统，再把占位改成真实实现。
- Player 特别说明：`GetCorpse()` 现在返回 `Corpse` 对象或 `nil`，和 3.3.5 Eluna 保持一致；需要尸体 GUID 时可继续调用 `corpse:GetGUID()` / `corpse:GetOwnerGUID()`；`GetTeam()` 按 3.3.5 Eluna 语义返回 TeamId，联盟为 `0`、部落为 `1`；`GetHonorPoints()` 暂时映射到 Turtle 1.12 荣誉 RankPoints；`SendMovieStart()` 暂按 `SendCinematicStart()` 兼容处理；`SendShowMailBox()` 当前是 no-op，因为 1.12 客户端/核心没有 3.3.5 的直接打开邮箱包。
- Player 分组/技能/法术/任务补充：`GetOriginalGroup`、`GetGroupInvite`、`GetSubGroup`、`GetOriginalSubGroup`、`GetTrader`、`GetInGameTime`、`GetSpells`、`AdvanceSkillsToMax`、`AdvanceSkill`、`AdvanceAllSkills`、`AddComboPoints`、`ClearComboPoints`、`LearnTalent`、`ResetTalents`、`ResetAllCooldowns`、`RemoveArenaSpellCooldowns`、`IsImmuneToDamage`、`SetBindPoint`、`GetHomebind`、`CanCompleteRepeatableQuest`、`GetQuestLevel`、`GetReqKillOrCastCurrentCount`。本批只做差异扫描和 Lua 示例语法检查，未做完整 CMake 编译。
- Player 法术强度补充：`SetSpellPower(value, apply)` 已按 Turtle 现有 `.modify spellpower` 路径接入；`apply=true` 时用核心自定义法术 `18058` 应用法伤加成，`apply=false` 或不填时移除该加成。
- Player 任务关联：`HasQuestForGO`、`HasQuestForItem`、`CanShareQuest`、`SetQuestStatus`、`FailQuest`、`RemoveQuest`、`TalkedToCreature`。
- Player 状态/聊天/战场：`CanSpeak`、`ToggleAFK`、`ToggleDND`、`SetGMVisible`、`SetGMChat`、`SetTaxiCheat`、`SetPvPDeath`、`InArena`、`InBattleground`、`InBattlegroundQueue`、`GetBattlegroundId`、`GetBattlegroundTypeId`。
- Player 物品/耐久/击杀：`CanUseItem`、`CanEquipItem`、`DurabilityRepair`、`DurabilityRepairAll`、`DurabilityLoss`、`DurabilityLossAll`、`DurabilityPointsLoss`、`DurabilityPointsLossAll`、`DurabilityPointLossForEquipSlot`、`GetLifetimeKills`、`SetLifetimeKills`、`AddLifetimeKills`、`RemoveLifetimeKills`、`KillPlayer`、`SpawnBones`。
- Object 通用兼容：补齐 `GetInt32Value`、`GetFloatValue`、`GetByteValue`、`GetUInt16Value`、`GetUInt64Value`、`SetInt32Value`、`UpdateUInt32Value`、`SetFloatValue`、`SetByteValue`、`SetUInt16Value`、`SetInt16Value`、`SetUInt64Value` 和 `ToPlayer` / `ToCreature` / `ToUnit` / `ToGameObject` / `ToCorpse` / `ToDynamicObject`。
- WorldObject 通用兼容：`GetLocation`、`GetPhaseMask`、`SetPhaseMask`、`GetPlayersInRange`、`GetCreaturesInRange`、`GetGameObjectsInRange`、`GetNearestPlayer`、`GetNearestCreature`、`GetNearestGameObject`、`GetNearObject`、`GetNearObjects`、`IsInMap`、`IsInRange`、`IsInRange2d`、`IsInRange3d`、`IsInFront`、`IsInBack`、`SpawnCreature`、`PlayMusic`、`PlayDirectSound`、`PlayDistanceSound`。
- Unit 通用兼容：`GetPowerPct`、`SetMaxPower`、`ModifyPower`、`GetRaceMask`、`GetClassMask`、`GetCreatureType`、`GetStat`、`GetMovementType`、`GetAttackers`、`GetCreatorGUID`、`GetMinionGUID`、`GetPetGUID`、`GetCritterGUID`、`GetControllerGUID`、`GetControllerGUIDS`、`HealthAbovePct`、`HealthBelowPct`、`IsFullHealth`、`IsInWater`、`IsUnderWater`、`IsMoving`、`IsFlying`、`IsCasting`、`IsPvPFlagged`、`IsStandState`、`IsVendor`、`IsTrainer`、`IsQuestGiver`、`IsGossip`、`IsTaxi`、`IsGuildMaster`、`IsBattleMaster`、`IsBanker`、`IsInnkeeper`、`IsSpiritHealer`、`IsSpiritGuide`、`IsTabardDesigner`、`IsAuctioneer`、`IsArmorer`、`IsServiceProvider`、`IsSpiritService`、`AddUnitState`、`ClearUnitState`、`ClearInCombat`、`StopSpellCast`、`InterruptSpell`、`PerformEmote`、`EmoteState`、`DeMorph`、`RestoreFaction`、`SetNativeDisplayId`、`RestoreDisplayId`、`SetSheath`、`SetRooted`、`SetConfused`、`SetFeared`、`SetFacing`、`SetFacingToObject`、`SetPvP`。
- Unit 仇恨兼容：`GetThreatList`、`GetThreat`、`AddThreat`、`ModifyThreatPct`、`ModifyThreatPercent`、`ClearThreat`、`ResetAllThreat`、`ClearThreatList`。
- Unit 状态/移动补充：`IsDying`、`IsCharmed`、`IsAttackingPlayer`、`GetRaceAsString`、`GetClassAsString`、`GetBaseSpellPower`、`SetPowerType`、`SetSpeed`、`SetSpeedRate`、`SetWaterWalk`、`SetFFA`、`SetInCombatWith`。
- Unit 移动控制：`MoveStop`、`MoveExpire`、`MoveClear`、`MoveIdle`、`MoveRandom`、`MoveHome`、`MoveFollow`、`MoveChase`、`MoveConfused`、`MoveFleeing`、`MoveTo`、`MoveJump`。
- Unit 施法兼容：`CastSpell`、`CastCustomSpell`、`CastSpellAoF`、`NearTeleport`。
- Unit 聊天兼容：`SendUnitSay`、`SendUnitYell`、`SendUnitWhisper`、`SendUnitEmote`、`SendChatMessageToPlayer`。
- Unit GUID/状态设置：`SetOwnerGUID`、`SetCreatorGUID`、`SetPetGUID`、`SetCritterGUID`、`SetName`、`SetImmuneTo`、`SetSanctuary`。
- Unit 查询/战斗补充：`GetFriendlyUnitsInRange`、`GetUnfriendlyUnitsInRange`、`GetCurrentSpell`、`HandleStatModifier`、`IsInAccessiblePlaceFor`、`RemoveArenaAuras`、`DealDamage`、`DealHeal`。
- Unit 载具兼容空入口：`IsOnVehicle`、`GetVehicle`、`GetVehicleKit`。Turtle 1.12 没有真实 Vehicle 系统，所以这些接口只用于兼容旧脚本。
- Aura 基础对象封装：`Unit:GetAura`、`Unit:AddAura` 返回 `Aura` 对象，支持读取施法者、持续时间、最大持续时间、光环 ID、层数、拥有者，并支持设置持续时间、最大持续时间、层数和移除。
- Corpse 对象封装：`Player:GetCorpse()`、`Object:ToCorpse()` 和地图按 GUID 查询现在可以返回 `Corpse` 对象；`CorpseMethods.h` 参考方法差异为 `ref=5 target=67 missing=0`。`SaveToDB()` 对骨骸类型当前做 no-op，避免触发 Turtle 1.12 的骨骸保存断言。
- Item 兼容补齐：`GetItemLink`、`GetRandomSuffix`、`IsCurrencyToken`、`IsWeaponVellum`、`IsArmorVellum`、`IsRefundExpired`。
- Quest 兼容补齐：`HasFlag`、`IsDaily`、`GetNextQuestId`、`GetPrevQuestId`、`GetNextQuestInChain`、`GetType`。
- Map 兼容补齐：`IsArena`、`IsEmpty`、`IsHeroic`、`GetDifficulty`、`GetHeight`、`GetAreaId`、`GetCreatures`、`GetCreaturesByAreaId`、`SetWeather`、`GetInstanceData`、`SaveInstanceData`。
- InstanceData 基础对象封装：`Map:GetInstanceData()` 现在会返回 Turtle 副本脚本对象，支持 `GetData` / `SetData` / `GetData64` / `SetData64` / `GetGuid` / `SetGuid` / `SaveToDB` 等核心真实入口。
- GameObject 兼容补齐：`HasQuest`、`IsTransport`、`IsActive`、`IsDestructible`、`GetLootRecipient`、`GetLootRecipientGroup`、`AddLoot`、`SaveToDB`、`RemoveFromWorld`、`UseDoorOrButton`、`Despawn`、`SetRespawnTime`。
- ItemTemplate 图标补齐：核心现在加载 `ItemDisplayInfo.dbc` 的物品图标字段，`template:GetIcon()` 会按物品 `DisplayInfoID` 返回真实图标名，例如 `INV_Misc_QuestionMark` 这一类客户端图标路径名。
- Spell 兼容补齐：`IsAutoRepeat`、`SetAutoRepeat`、`Cast`、`Finish`、`GetDuration`、`GetReagentCost`、`GetTargetDest`。
- Group 兼容补齐：`GetGUID`、`GetMemberGUID`、`GetGroupType`、`IsLFGGroup`、`IsAssistant`、`SameSubGroup`、`HasFreeSlotSubGroup`、`SetLeader`、`AddMember`、`RemoveMember`、`Disband`、`ConvertToRaid`、`SetMembersGroup`、`SetTargetIcon`、`SetMemberFlag`、`SendPacket`。
- Roll 基础对象封装：玩家事件 `56` 的最后一个参数现在返回 `Roll` 对象，不再是 `nil`；可读取掷骰物品、来源 GUID、参与玩家投票、need/greed/pass 统计、物品槽位和投票掩码。
- Guild 兼容补齐：`GetMembers`、`SetLeader`、`SetBankTabText`、`SendPacket`、`SendPacketToRanked`、`Disband`、`AddMember`、`DeleteMember`、`SetMemberRank`、`SetName`、`UpdateMemberData`、`MassInviteToEvent`、`SwapItems`、`SwapItemsWithInventory`、`GetTotalBankMoney`、`GetCreatedDate`、`ResetTimes`、`ModifyBankMoney`。
- WorldPacket 基础对象封装和包事件：`CreatePacket(opcode, size)` 现在返回 `WorldPacket` 对象，支持 opcode / size 查询、opcode 修改、基础整数/浮点/GUID/字符串读写；`Player`、`Group`、`Guild`、`WorldObject` 的 `SendPacket` 入口已经可以发送该对象；`RegisterPacketEvent(opcode, 5/7, function)` 已接入客户端入包和服务端出包拦截。
- AddOn 消息事件：`RegisterServerEvent(30, function(event, sender, type, prefix, msg, target) end)` 已接入客户端插件消息，能在 Turtle 内置插件消息处理前拦截；`target` 可以是接收玩家、公会、队伍、频道 ID 或 `nil`。
- Channel 基础对象封装：玩家频道聊天事件 `22` 现在按 3.3.5 Eluna 风格继续传频道数字 ID，并额外追加 `Channel` 对象；对象可读取频道名、频道 ID、人数、flags、安全等级、阵营、密码和成员状态，也能设置公告/密码/安全等级、设置成员主持/禁言、广播包或发送频道消息。
- DynamicObject 基础对象封装：地图按 GUID 反查和通用 `WorldObject` 返回路径现在可以返回 `DynamicObject` 对象；脚本可读取施法者、施法者 GUID、法术 ID、效果索引、持续时间、半径和动态对象类型，也可调用 `Delay()` / `Delete()`。
- ElunaQuery 数据库结果对象：`WorldDBQuery` / `CharDBQuery` / `AuthDBQuery` 等现在返回 `ElunaQuery` 对象，支持 3.3.5 Eluna 风格的 `GetUInt32(0)`、`GetString(0)`、`NextRow()`、`GetRow()` 等对象方法；列下标按 Eluna 从 `0` 开始。`WorldDBQueryAsync` / `CharDBQueryAsync` / `AuthDBQueryAsync` 也已接入，数据库线程完成后会切回 Lua 世界线程调用 callback。
- HTTP 请求：`HttpRequest` 已按 3.3.5 Eluna 参数形式接入，HTTP 工作在线程里执行，完成后回到 Lua 世界线程调用 `(status, body, headers)` callback。
- 事件注册语义补齐：所有全局 `Register*Event` 现在都支持可选 `shots` 参数并返回取消函数；`shots = 0` 或不填表示永久有效，`shots > 0` 表示触发指定次数后自动停用，返回的取消函数可手动停用这次注册。
- 全局兼容函数补齐：新增核心信息、Lua 状态信息、bit 位运算、毫秒时间、背包位置判断、日志打印、全局命令执行和全局定时事件清理入口。`GetCoreExpansion()` 按 Turtle 1.12 返回 `0`，`GetStateMap()` / `GetStateMapId()` / `GetStateInstanceId()` 按当前单 Lua 状态返回 `nil` / `-1` / `0`，`IsCompatibilityMode()` 返回 `true`。
- 全局工具/管理函数补充：新增 `CreateLongLong`、`CreateULongLong`、`CreateInt64`、`CreateUint64`、`GetItemLink`、`GetAreaName`、`GetGuildByLeaderGUID`、`Kick`、`Ban`、`SaveAllPlayers`、`SendMail`、`AddVendorItem`、`VendorRemoveItem`、`VendorRemoveAllItems`。其中 `Ban()` 调用 Turtle 当前异步封禁流程，合法请求会先返回 `3` 表示已进入处理队列；`AddVendorItem` 的第 5 个参数在 Turtle 1.12 中按 `itemflags` 使用，不是 3.3.5 的 extended cost。
- 全局游戏事件/地图/Gossip 函数补充：新增 `GetActiveGameEvents`、`IsGameEventActive`、`StartGameEvent`、`StopGameEvent`、`GetMapEntrance`、`GetGossipMenuOptionLocale`，均接入 Turtle 当前 `GameEventMgr` / `ObjectMgr` 真实接口。
- 全局版本兼容占位：新增 `GetOwnerHalaa` / `SetOwnerHalaa`，Turtle 1.12 没有 TBC/WotLK 的纳格兰 Halaa 系统，因此当前只保持脚本兼容，不改变游戏状态。
- 全局动态出生函数补充：新增 `PerformIngameSpawn`，支持临时/保存的生物和游戏物体出生；临时出生走地图召唤接口，保存出生走 Turtle 现有静态 GUID、保存到 DB、加入网格的路径。
- 全局 DBC 查询函数补充：新增 `LookupEntry`，当前支持 `Spell` / `SpellEntry` 查询并返回 `SpellInfo` 对象；`GemProperties` 在 Turtle 1.12 中没有对应 DBC，作为兼容名称查询不到时返回空。
- 全局 Group/Guild 事件补充：新增 `RegisterGroupEvent`、`RegisterGuildEvent`、`ClearGroupEvents`、`ClearGuildEvents`，并在 `Group.cpp` / `Guild.cpp` 接入真实触发点。
- 全局玩家 Gossip 事件补充：新增 `RegisterPlayerGossipEvent`、`ClearPlayerGossipEvents`，并在客户端选择玩家自身 Gossip 菜单项时回调 Lua。
- 全局唯一 Creature 事件补充：新增 `RegisterUniqueCreatureEvent`、`ClearUniqueCreatureEvents`，按 `ObjectGuid + instanceId + eventId` 绑定单个已经刷出的生物；它和 `RegisterCreatureEvent` 共用现有 Creature 事件触发点，同一事件会先执行 entry 绑定，再执行唯一生物绑定，支持 `shots` 和返回取消函数。
- 全局 Map/Instance 副本事件补充：新增 `RegisterMapEvent`、`RegisterInstanceEvent`、`ClearMapEvents`、`ClearInstanceEvents`。`RegisterMapEvent(mapId, eventId, function)` 按地图 ID 绑定该地图所有副本实例；`RegisterInstanceEvent(instanceId, eventId, function)` 按运行时实例 ID 绑定单个实例。同一副本事件会先执行 mapId 绑定，再执行 instanceId 绑定。当前已触发事件 `1` 初始化、`2` 加载、`3` 更新、`4` 玩家进入、`5` Creature 创建、`6` GameObject 创建；事件 `7` 检查战斗进度还没有统一安全触发点，暂不触发。
- 全局战场事件补充：新增 `RegisterBGEvent`、`ClearBattleGroundEvents`，并接入战场创建、开始、结束和销毁前事件；新增 `BattleGround` 对象封装，当前 `BattleGroundMethods.h` 参考方法差异为 `ref=17 target=19 missing=0`，支持 `shots` 和返回取消函数。
- 全局工单事件补充：新增 `RegisterTicketEvent`、`ClearTicketEvents`，并接入工单创建、玩家更新文本、关闭事件；新增 `Ticket` 对象封装，当前 `TicketMethods.h` 参考方法差异为 `ref=25 target=25 missing=0`。Turtle 1.12 没有独立的自动 resolve 核心流程，`4` 号 resolve 事件当前会在 Lua 调用 `ticket:SetResolvedBy()` 或 `ticket:SetCompleted()` 时触发。
- 全局出租路径函数补充：新增 `AddTaxiPath(waypoints, mountA, mountH[, price[, pathId]])`，会在运行时创建临时 TaxiNode、TaxiPath 和 TaxiPathNode 数据，并返回可传给 `player:StartTaxi(pathId)` 的路径 ID。Lua 的 `Player:StartTaxi()` 当前按脚本入口跳过已知飞行点检查，以便自定义路径可直接使用。
- SpellInfo 3.3.5 参考方法名补齐：`HasAreaAuraEffect`、`IsAffectingArea`、`IsTargetingArea`、`NeedsExplicitUnitTarget`、`GetSpellSpecific`、`GetDispelMask`、`CheckTarget`、`CheckExplicitTarget` 等。当前 `SpellInfoMethods.h` 参考方法差异为 `missing=0`，其中部分检查按 Turtle 1.12 能力做兼容近似。
- SpellEntry 旧接口兼容补齐：`SpellEntryMethods.h` 的 92 个参考方法名已经并入 `SpellInfo` 元表，当前差异扫描为 `ref=92 target=165 missing=0`。本批补上了 `GetSpellName`、`GetDurationIndex`、`GetManaCostPerlevel`、`GetManaPerSecond`、`GetEquippedItemClass`、`GetEffectRealPointsPerLevel`、`GetEffectRadiusIndex`、`GetEffectDamageMultiplier`、`GetEffectBonusMultiplier`、`GetTotemCategory`、`GetAreaGroupId`、`GetRuneCostID` 等兼容入口；WotLK 专属字段按 Turtle 1.12 能力返回 `0` 或全 0 table。

已验证命令：

```powershell
cmake --build E:\GIT\WYTurtle\build_vs2022 --config Release --target mangosd -- /m
```

本轮新增 `WorldPacket` 后的轻量验证：

```text
WorldPacketMethods.h: ref=23 target=24 missing=0
全部已扫描方法组 missing=0
E:\TurtleBY\lua52_compiler.exe -p E:\GIT\WYTurtle\lua_scripts\example.lua
```

安装命令：

```powershell
cmake --build E:\GIT\WYTurtle\build_vs2022 --config Release --target INSTALL -- /m
```

## 配置项

```ini
Eluna.Enabled = 1
Eluna.ScriptPath = "lua_scripts"
```

`Eluna.ScriptPath` 如果不是绝对路径，就按 `mangosd.exe` 的当前工作目录解析。

## 全局函数

当前 Lua 中可用：

```lua
RegisterPlayerEvent(eventId, function[, shots])
RegisterServerEvent(eventId, function[, shots])
RegisterGroupEvent(eventId, function[, shots])
RegisterGuildEvent(eventId, function[, shots])
RegisterBGEvent(eventId, function[, shots])
RegisterTicketEvent(eventId, function[, shots])
RegisterPacketEvent(opcode, eventId, function[, shots])
RegisterMapEvent(mapId, eventId, function[, shots])
RegisterInstanceEvent(instanceId, eventId, function[, shots])
RegisterCreatureEvent(entry, eventId, function[, shots])
RegisterUniqueCreatureEvent(guidOrCreature, instanceId, eventId, function[, shots])
RegisterGameObjectEvent(entry, eventId, function[, shots])
RegisterItemEvent(entry, eventId, function[, shots])
RegisterSpellEvent(spellId, eventId, function[, shots])
RegisterCreatureGossipEvent(entry, eventId, function[, shots])
RegisterGameObjectGossipEvent(entry, eventId, function[, shots])
RegisterItemGossipEvent(entry, eventId, function[, shots])
RegisterPlayerGossipEvent(playerGuidLow, eventId, function[, shots])
ClearPlayerEvents([eventId])
ClearServerEvents([eventId])
ClearGroupEvents([eventId])
ClearGuildEvents([eventId])
ClearBattleGroundEvents([eventId])
ClearTicketEvents([eventId])
ClearPacketEvents(opcode, [eventId])
ClearMapEvents(mapId, [eventId])
ClearInstanceEvents(instanceId, [eventId])
ClearCreatureEvents(entry, [eventId])
ClearUniqueCreatureEvents(guidOrCreature, instanceId, [eventId])
ClearGameObjectEvents(entry, [eventId])
ClearItemEvents(entry, [eventId])
ClearSpellEvents(spellId, [eventId])
ClearCreatureGossipEvents(entry, [eventId])
ClearGameObjectGossipEvents(entry, [eventId])
ClearItemGossipEvents(entry, [eventId])
ClearPlayerGossipEvents(playerGuidLow, [eventId])
GetLuaEngine()
GetCoreName()
GetRealmID()
GetCoreVersion()
GetCoreExpansion()
GetStateMap()
GetStateMapId()
GetStateInstanceId()
IsCompatibilityMode()
bit_and(a, b)
bit_or(a, b)
bit_lshift(a, b)
bit_rshift(a, b)
bit_xor(a, b)
bit_not(a)
CreateLongLong([value])
CreateULongLong([value])
CreateInt64([value])
CreateUint64([value])
GetCurrTime()
GetTimeDiff(oldTime)
IsInventoryPos(bag, slot)
IsEquipmentPos(bag, slot)
IsBankPos(bag, slot)
IsBagPos(bag, slot)
CreateLuaEvent(function, delayMs, repeats)
RemoveEventById(eventId)
RemoveEvents([allEvents])
ReloadEluna()
RunCommand(command)
Kick(player)
Ban(mode, nameOrIP, durationSeconds[, reason[, author]])
SaveAllPlayers()
SendMail(subject, body, receiverGuidLow[, senderGuidLow[, stationery[, delay[, money[, cod[, itemEntry, amount]]]]]])
GetPlayerByName(name)
GetPlayerByGUID(guidOrGuidLow)
GetPlayerByGUIDLow(guidLow)
CreateObjectGuid(high, counter)
CreateObjectGuid(high, entry, counter)
CreatePlayerGuid(counter)
CreateItemGuid(counter)
CreateCreatureGuid(entry, counter)
CreateGameObjectGuid(entry, counter)
CreatePacket(opcode, size)
GetPlayerGUID(lowguid)
GetItemGUID(lowguid)
GetObjectGUID(lowguid, entry)
GetUnitGUID(lowguid, entry)
GetGUIDLow(guid)
GetGUIDType(guid)
GetGUIDEntry(guid)
SendWorldMessage(message)
GetGameTime()
GetQuest(questId)
GetItemTemplate(itemId)
GetItemPrototype(itemId)
GetItemLink(itemId[, locale])
GetAreaName(areaOrZoneId[, locale])
GetActiveGameEvents()
IsGameEventActive(eventId)
StartGameEvent(eventId[, force])
StopGameEvent(eventId[, force])
GetMapEntrance(mapId)
GetGossipMenuOptionLocale(menuId, optionId, locale)
GetOwnerHalaa()
SetOwnerHalaa(teamId)
GetCreatureTemplate(entry)
GetCreatureInfo(entry)
GetGameObjectTemplate(entry)
GetGameObjectInfo(entry)
GetGOTemplate(entry)
GetGOInfo(entry)
GetSpellInfo(spellId)
GetSpellEntry(spellId)
LookupEntry(dbcName, id)
GetGuildById(guildId)
GetGuildByName(name)
GetGuildByLeaderGUID(guid)
GetPlayersInWorld()
GetPlayerCount()
GetMapById(mapId, instanceId)
PerformIngameSpawn(spawnType, entry, mapId, instanceId, x, y, z, o[, save[, durationOrRespawn[, phaseMask]]])
AddTaxiPath(waypoints, mountA, mountH[, price[, pathId]])
AddVendorItem(creatureEntry, itemEntry[, maxCount[, incrTime[, itemFlags]]])
VendorRemoveItem(creatureEntry, itemEntry)
VendorRemoveAllItems(creatureEntry)
WorldDBQuery(sql)
CharDBQuery(sql)
CharacterDBQuery(sql)
AuthDBQuery(sql)
LoginDBQuery(sql)
WorldDBQueryAsync(sql, callback)
CharDBQueryAsync(sql, callback)
CharacterDBQueryAsync(sql, callback)
AuthDBQueryAsync(sql, callback)
LoginDBQueryAsync(sql, callback)
WorldDBExecute(sql)
CharDBExecute(sql)
CharacterDBExecute(sql)
AuthDBExecute(sql)
LoginDBExecute(sql)
HttpRequest(method, url, callback)
HttpRequest(method, url, headers, callback)
HttpRequest(method, url, body, contentType, callback)
HttpRequest(method, url, body, contentType, headers, callback)
PrintInfo(...)
PrintError(...)
PrintDebug(...)
print(...)
```

说明：

- `CreateLuaEvent(function, delayMs, repeats)` 返回定时器 ID。
- `repeats = 0` 表示无限重复。
- `RemoveEvents(false)` 只清理全局 `CreateLuaEvent` 创建的定时事件；`RemoveEvents(true)` 会连同对象绑定事件一起清理。
- `RegisterGroupEvent` / `ClearGroupEvents` 当前已接通到组队创建、邀请、加入、移除、队长变化、解散等核心触发点。
- `RegisterGuildEvent` / `ClearGuildEvents` 当前已接通到公会创建、成员加入、成员移除、MOTD 变更、信息变更、解散等核心触发点。
- `RegisterBGEvent` / `ClearBattleGroundEvents` 当前已接通到战场创建、开始、结束和销毁前触发点。
- `RegisterTicketEvent` / `ClearTicketEvents` 当前已接通到工单创建、玩家更新文本、关闭触发点；resolve 事件由 Lua 兼容 setter 触发。
- `RegisterPlayerGossipEvent(playerGuidLow, 2, function)` 当前接通玩家自身 Gossip 菜单选择事件；玩家 Gossip 没有 `1` 号 hello 触发，脚本需要自己调用 `player:GossipMenuAddItem()` / `player:GossipSendMenu()` 打开菜单。
- Group 事件参数：`1` 加入 `(event, group, guid)`；`2` 邀请 `(event, group, guid)`；`3` 移除 `(event, group, guid, method, nil, nil)`；`4` 队长变化 `(event, group, newLeaderGuid, oldLeaderGuid)`；`5` 解散 `(event, group)`；`6` 创建 `(event, group, leaderGuid, groupType)`。
- Guild 事件参数：`1` 加入 `(event, guild, player, rank)`；`2` 移除 `(event, guild, player, isDisbanding)`；`3` MOTD 变化 `(event, guild, newMotd)`；`4` 信息变化 `(event, guild, newInfo)`；`5` 创建 `(event, guild, leader, name)`；`6` 解散 `(event, guild)`。离线成员无法取得 `Player` 对象时会传 `nil`。
- BattleGround 事件参数：`1` 开始 `(event, bg, bgId, instanceId)`；`2` 结束 `(event, bg, bgId, instanceId, winner)`；`3` 创建 `(event, bg, bgId, instanceId)`；`4` 销毁前 `(event, bg, bgId, instanceId)`。
- Ticket 事件参数：`1` 创建 `(event, ticket)`；`2` 玩家更新文本 `(event, ticket, message)`；`3` 关闭 `(event, ticket)`；`4` resolve `(event, ticket)`。
- `RunCommand(command)` 按控制台权限把 GM 命令加入世界线程队列执行，命令前面的 `.` / `!` 可以带也可以不带。
- `Kick(player)` 会断开指定玩家当前会话。
- `Ban(mode, nameOrIP, durationSeconds[, reason[, author]])` 中 `mode` 为 `0` 账号、`1` 角色、`2` IP；Turtle 的封禁流程是异步查询账号/角色后再落库，所以合法请求通常先返回 `3`。
- `SaveAllPlayers()` 会保存当前所有在线玩家。
- `SendMail(...)` 使用 Turtle 的邮件系统发送文本、金币、COD 和物品附件；Turtle 1.12 当前 `MAX_MAIL_ITEMS = 1`，所以一次最多发送 1 个物品附件，成功创建附件时返回附件物品的低位 GUID。
- `GetLuaEngine()` 为兼容旧 Eluna 脚本返回 `ElunaEngine`；`GetCoreName()` 返回 `Turtle WoW`；`GetCoreExpansion()` 在 Turtle 1.12 下返回 `0`。
- `GetCurrTime()` 返回服务器毫秒计时，`GetTimeDiff(oldTime)` 返回 `oldTime` 到当前的毫秒差。
- `CreateLongLong()` / `CreateULongLong()` 以及 3.3.5 公开别名 `CreateInt64()` / `CreateUint64()` 当前按 Lua 数值返回，能兼容常见数字/字符串入参；超过 Lua 数值安全范围的超大 `uint64` 后续还需要更完整的 userdata 包装。
- `GetItemLink(itemId[, locale])` 会生成可点击物品链接；`locale` 使用 Turtle 的 `LocaleConstant`。
- `GetAreaName(areaOrZoneId[, locale])` 从 `area_template` / 区域本地化表取名称，找不到会按 Eluna 风格报参数错误。
- `GetActiveGameEvents()` 返回当前激活的游戏事件 ID 数组；`IsGameEventActive(eventId)` 返回指定事件是否激活。
- `StartGameEvent(eventId[, force])` / `StopGameEvent(eventId[, force])` 调用 Turtle 的游戏事件管理器；`force=true` 对应核心里的强制覆盖启动/停止。
- `GetMapEntrance(mapId)` 找到入口时返回 `x, y, z, o` 四个坐标值，找不到返回 `nil`。
- `GetGossipMenuOptionLocale(menuId, optionId, locale)` 返回指定 gossip 菜单选项的本地化选项文本和弹窗文本；没有本地化时返回默认文本。
- `GetOwnerHalaa()` / `SetOwnerHalaa(teamId)` 是 1.12 兼容占位；`GetOwnerHalaa()` 固定返回 `0, 0.0`，`SetOwnerHalaa(0/1)` 不执行状态变更。
- `GetGuildByLeaderGUID(guid)` 支持传 `ObjectGuid`，也兼容传玩家低位 GUID 数字。
- `AddVendorItem` / `VendorRemoveItem` / `VendorRemoveAllItems` 会同步更新内存商人列表和 `npc_vendor` 表；Turtle 1.12 没有 3.3.5 extended cost 字段，第 5 个参数当前映射为 `itemflags`。
- `PrintInfo` / `PrintError` / `PrintDebug` 分别写入 info、error、debug 日志；`print(...)` 仍写普通 Lua 日志。
- `GetPlayersInWorld()` 返回当前在线且在世界中的玩家对象数组。
- `GetPlayerCount()` 返回当前在线且在世界中的玩家数量。
- `GetMapById(mapId, instanceId)` 只查找已经加载的地图，找不到时返回 `nil`。
- `PerformIngameSpawn(1, ...)` 出生生物，`PerformIngameSpawn(2, ...)` 出生游戏物体；`save=true` 会写入 world 数据库，`durationOrRespawn` 对临时出生表示消失时间、对保存出生表示重生时间，`phaseMask` 当前映射到 Turtle 的 world mask。
- `AddTaxiPath(waypoints, mountA, mountH[, price[, pathId]])` 的 `waypoints` 格式为 `{ {mapId, x, y, z[, actionFlag[, delay]]}, ... }`，至少需要 2 个点。`mountA` 是联盟坐骑 Creature entry，`mountH` 是部落坐骑 Creature entry；`pathId` 不传时自动选择运行时可用 ID，传入已存在 ID 会报错，避免覆盖原始飞行路线。
- `GetPlayerByGUID()` 现在可以传 `ObjectGuid` 对象，也可以继续传玩家 `guidLow` 数字。
- `GetPlayerByGUIDLow(guidLow)` 是按玩家低位 GUID 找在线玩家的兼容别名。
- `CreateObjectGuid(high, counter)` 用于没有 entry 部分的 GUID，例如玩家和物品。
- `CreateObjectGuid(high, entry, counter)` 用于有 entry 部分的 GUID，例如生物和 GameObject。
- `CreatePacket(opcode, size)` 创建一个 Lua 持有的 `WorldPacket` 对象；`opcode` 必须小于 Turtle 1.12 的 `NUM_MSG_TYPES`，`size` 是预留字节大小。
- `GetPlayerGUID(lowguid)`、`GetItemGUID(lowguid)`、`GetObjectGUID(lowguid, entry)`、`GetUnitGUID(lowguid, entry)` 是 3.3.5 Eluna 风格的 GUID 构造函数；注意 `GetObjectGUID` / `GetUnitGUID` 的参数顺序是低位 GUID 在前、entry 在后。
- `GetGUIDLow(guid)`、`GetGUIDType(guid)`、`GetGUIDEntry(guid)` 用于从 `ObjectGuid` 里拆出低位、HighGuid 类型和 entry。
- `SendWorldMessage(message)` 已按 3.3.5 Eluna 行为发送服务器消息；Turtle 1.12 中使用等价的 `SERVER_MSG_CUSTOM`，不再走普通 `CHAT_MSG_SYSTEM` 聊天包。
- `GetItemTemplate(itemId)` / `GetItemPrototype(itemId)` 返回只读物品模板对象，找不到时返回 `nil`。
- `GetCreatureTemplate(entry)` / `GetCreatureInfo(entry)` 返回只读生物模板对象，找不到时返回 `nil`。
- `GetGameObjectTemplate(entry)` / `GetGameObjectInfo(entry)` / `GetGOTemplate(entry)` / `GetGOInfo(entry)` 返回只读 GameObject 模板对象，找不到时返回 `nil`。
- `LookupEntry("Spell", spellId)` / `LookupEntry("SpellEntry", spellId)` 返回只读 `SpellInfo` 对象；`LookupEntry("GemProperties", id)` 在 Turtle 1.12 当前返回空，因为 1.12 没有宝石 DBC 系统。
- 数据库同步查询函数返回 `ElunaQuery` 对象，找不到结果时返回 `nil`；对象方法里的列下标按 3.3.5 Eluna 习惯从 `0` 开始。
- 数据库异步查询函数会在数据库线程完成 SQL 后，把 callback 切回 Lua 世界线程执行；callback 参数为 `ElunaQuery` 或 `nil`，不会在数据库线程里直接调用 Lua。
- `HttpRequest()` 支持 `GET`、`HEAD`、`POST`、`PUT`、`PATCH`、`DELETE`、`OPTIONS`；callback 参数为 `status, body, headers`。请求失败或 URL 无法解析时 `status` 为 `0`，`body` 中会带错误说明。
- 数据库执行函数返回 `true` 或 `false`。

## ElunaQuery 方法

`WorldDBQuery(sql)`、`CharDBQuery(sql)` / `CharacterDBQuery(sql)`、`AuthDBQuery(sql)` / `LoginDBQuery(sql)` 返回数据库查询结果对象；没有结果时返回 `nil`。

`WorldDBQueryAsync(sql, callback)`、`CharDBQueryAsync(sql, callback)` / `CharacterDBQueryAsync(sql, callback)`、`AuthDBQueryAsync(sql, callback)` / `LoginDBQueryAsync(sql, callback)` 异步执行查询；callback 在 Lua 世界线程执行，参数同样是 `ElunaQuery` 或 `nil`。如果查询完成前 Lua 状态重载或关闭，该回调会被丢弃，避免旧 Lua 引用误进新状态。

```lua
local query = WorldDBQuery("SELECT entry, name FROM creature_template LIMIT 1")
if query then
    print(query:GetUInt32(0), query:GetString(1))
end

WorldDBQueryAsync("SELECT entry, name FROM creature_template LIMIT 1", function(query)
    if query then
        print(query:GetUInt32(0), query:GetString(1))
    end
end)
```

可用方法：

```lua
query:IsNull(column)
query:IsNULL(column)
query:GetColumnCount()
query:GetFieldCount()
query:GetRowCount()
query:GetBool(column)
query:GetUInt8(column)
query:GetUInt16(column)
query:GetUInt32(column)
query:GetUInt64(column)
query:GetInt8(column)
query:GetInt16(column)
query:GetInt32(column)
query:GetInt64(column)
query:GetFloat(column)
query:GetDouble(column)
query:GetString(column)
query:NextRow()
query:GetRow()
```

说明：

- `column` 从 `0` 开始，这一点和旧 Eluna 保持一致。
- `NextRow()` 会移动到下一行；查询刚返回时已经位于第一行，不要在读取第一行前先调用 `NextRow()`。
- `GetRow()` 返回当前行 table，同时支持 `row[1]` 这种 Lua 数组访问和 `row["字段名"]` 这种字段名访问。
- `GetUInt64()` / `GetInt64()` 在 Lua 里以整数返回；如果脚本需要完全避免大整数精度差异，可以用 `GetString(column)` 自己处理。

## HttpRequest

`HttpRequest` 是异步 HTTP 客户端入口，支持 3.3.5 Eluna 的常见调用形式。请求在线程里执行，callback 回到 Lua 世界线程执行。

```lua
HttpRequest("GET", "https://example.com/", function(status, body, headers)
    print(status, body)
end)

HttpRequest("POST", "https://example.com/api", "{\"ok\":true}", "application/json", {
    Accept = "application/json"
}, function(status, body, headers)
    print(status, headers["content-type"] or "")
end)
```

说明：

- callback 参数固定为 `(status, body, headers)`。
- `headers` 是响应头 table；重复响应头会按 Lua table 规则保留最后写入的值。
- 请求失败或 URL 无法解析时 `status = 0`，`body` 里会放错误说明。
- 如果请求完成前 Lua 状态重载或关闭，该回调会被丢弃，避免旧 Lua 引用误进新状态。

可用的 `HighGuid` 常量：

```lua
HIGHGUID_ITEM
HIGHGUID_PLAYER
HIGHGUID_GAMEOBJECT
HIGHGUID_TRANSPORT
HIGHGUID_UNIT
HIGHGUID_PET
HIGHGUID_DYNAMICOBJECT
HIGHGUID_CORPSE
HIGHGUID_MO_TRANSPORT
```

事件清理函数说明：

- 所有 `Register*Event` 都会返回取消函数；保存后调用它可以只停用这一次注册。可选 `shots` 参数为触发次数，`0` 或不填表示不限制次数。
- `ClearPlayerEvents()` 清理所有玩家事件；传入 `eventId` 时只清理指定玩家事件。
- `ClearServerEvents()` 清理所有服务端事件；传入 `eventId` 时只清理指定服务端事件。
- `ClearBattleGroundEvents()` 清理所有战场事件；传入 `eventId` 时只清理指定战场事件。
- `ClearTicketEvents()` 清理所有工单事件；传入 `eventId` 时只清理指定工单事件。
- `ClearPacketEvents(opcode)` 清理指定 opcode 的所有包事件；传入 `eventId` 时只清理该 opcode 的指定包事件。
- `ClearMapEvents(mapId)` 清理指定地图 ID 绑定的所有副本生命周期事件；传入 `eventId` 时只清理该地图的指定副本事件。
- `ClearInstanceEvents(instanceId)` 清理指定运行时实例 ID 绑定的所有副本生命周期事件；传入 `eventId` 时只清理该实例的指定事件。
- `ClearCreatureEvents(entry)` 清理该生物模板的所有 Creature 事件；传入 `eventId` 时只清理该模板的指定事件。
- `ClearUniqueCreatureEvents(guidOrCreature, instanceId)` 清理指定生物实例的所有 Creature 事件；传入 `eventId` 时只清理这个生物实例的指定事件。`guidOrCreature` 可以传 `Creature` 对象或 `ObjectGuid`；`instanceId` 要和目标所在地图实例一致，普通大陆通常是 `0`。
- `ClearGameObjectEvents(entry)`、`ClearItemEvents(entry)` 逻辑相同，分别清理指定物体或物品模板事件。
- `ClearSpellEvents(spellId)` 清理指定法术 ID 的动态施法事件；传入 `eventId` 时只清理指定事件。
- `ClearCreatureGossipEvents(entry)`、`ClearGameObjectGossipEvents(entry)`、`ClearItemGossipEvents(entry)` 清理指定模板的 Gossip 事件。
- 事件正在执行时调用清理函数是允许的；当前分发会跳过已经被清理的回调引用。

## 已接入事件

### 服务端事件

```lua
RegisterServerEvent(13, function(event, diff) end) -- 世界 Update
RegisterServerEvent(14, function(event) end)       -- 服务端启动 / Lua 初始化后
RegisterServerEvent(15, function(event) end)       -- 服务端关闭
RegisterServerEvent(16, function(event) end)       -- Lua 状态关闭
RegisterServerEvent(17, function(event, map) end)  -- 地图对象创建
RegisterServerEvent(18, function(event, map) end)  -- 地图对象销毁
RegisterServerEvent(21, function(event, map, player) end) -- 玩家进入地图
RegisterServerEvent(22, function(event, map, player) end) -- 玩家离开地图
RegisterServerEvent(23, function(event, map, diff) end)   -- 地图 Update
RegisterServerEvent(30, function(event, sender, type, prefix, msg, target) end) -- AddOn 消息
RegisterServerEvent(33, function(event) end)       -- Lua 状态打开
```

`RegisterServerEvent(19/20)` 对应的网格加载/卸载事件目前还没有接入，避免在网格线程和对象生命周期里不安全地调用 Lua。

### Map / Instance 副本生命周期事件

```lua
RegisterMapEvent(mapId, 1, function(event, instanceData, map) end)             -- 副本脚本初始化
RegisterMapEvent(mapId, 2, function(event, instanceData, map) end)             -- 副本脚本加载
RegisterMapEvent(mapId, 3, function(event, instanceData, map, diff) end)       -- 副本脚本 Update
RegisterMapEvent(mapId, 4, function(event, instanceData, map, player) end)     -- 玩家进入副本地图
RegisterMapEvent(mapId, 5, function(event, instanceData, map, creature) end)   -- Creature 创建
RegisterMapEvent(mapId, 6, function(event, instanceData, map, gameObject) end) -- GameObject 创建

RegisterInstanceEvent(instanceId, 4, function(event, instanceData, map, player) end)
ClearMapEvents(mapId, 4)
ClearInstanceEvents(instanceId, 4)
```

说明：`RegisterMapEvent` 的第一个参数是地图 ID，会作用于该地图所有副本实例；`RegisterInstanceEvent` 的第一个参数是运行时实例 ID，只作用于某一个已创建实例。两者绑定同一个事件时，先执行 mapId 绑定，再执行 instanceId 绑定。当前事件 `7`（检查战斗进度）没有统一安全触发点，暂不触发。

### 战场事件

```lua
RegisterBGEvent(3, function(event, bg, bgId, instanceId) end)         -- 战场地图创建后
RegisterBGEvent(1, function(event, bg, bgId, instanceId) end)         -- 战场正式开始
RegisterBGEvent(2, function(event, bg, bgId, instanceId, winner) end) -- 战场结束
RegisterBGEvent(4, function(event, bg, bgId, instanceId) end)         -- 战场销毁前

ClearBattleGroundEvents(1)
ClearBattleGroundEvents()
```

说明：`bgId` 是核心的战场类型 ID，`instanceId` 是运行时地图实例 ID。`winner` 使用核心队伍值；普通结束会传核心赢家，`EndNow()` 这类强制结束路径当前传 `TEAM_NONE`。

### 工单事件

```lua
RegisterTicketEvent(1, function(event, ticket) end)          -- 工单创建
RegisterTicketEvent(2, function(event, ticket, message) end) -- 玩家更新工单文本
RegisterTicketEvent(3, function(event, ticket) end)          -- 工单关闭
RegisterTicketEvent(4, function(event, ticket) end)          -- Lua 标记完成/解决

ClearTicketEvents(2)
ClearTicketEvents()
```

说明：Turtle 当前的工单系统没有独立的自动 `resolve` 核心路径；`4` 号事件会在 Lua 调用 `ticket:SetResolvedBy(guid)` 或 `ticket:SetCompleted()` 时触发。

服务端包事件也已接入，作用于所有 opcode：

```lua
RegisterServerEvent(5, function(event, packet, player) end) -- 客户端包进入核心处理前
RegisterServerEvent(7, function(event, packet, player) end) -- 服务端包发送到客户端前
```

`player` 在角色还未进入世界或会话没有角色对象时为 `nil`。回调返回 `false` 会阻止这个包继续处理或继续发送；收包和发包回调都可以把第二个返回值设为新的 `WorldPacket`，用于替换后续核心看到或真正发出的包。

AddOn 消息事件 `30` 在 `LANG_ADDON` 聊天消息进入 Turtle 内置插件消息处理前触发。`prefix` 是制表符 `\t` 之前的内容，`msg` 是制表符之后的内容；没有制表符时 `msg` 为 `nil`。`target` 会按消息类型返回：私聊目标玩家、公会对象、队伍对象、频道 ID，或无法解析时返回 `nil`。回调返回 `false` 会阻止该 AddOn 消息继续进入核心处理和广播。

### 玩家事件

```lua
RegisterPlayerEvent(1, function(event, player) end)
RegisterPlayerEvent(2, function(event, guidLow) end)
RegisterPlayerEvent(3, function(event, player) end)
RegisterPlayerEvent(4, function(event, player) end)
RegisterPlayerEvent(5, function(event, player, spell, skipCheck) end)
RegisterPlayerEvent(6, function(event, killer, killed) end)
RegisterPlayerEvent(7, function(event, killer, killed) end)
RegisterPlayerEvent(8, function(event, killer, killed) end)
RegisterPlayerEvent(9, function(event, target, challenger) end)
RegisterPlayerEvent(10, function(event, player1, player2) end)
RegisterPlayerEvent(11, function(event, winner, loser, type) end)
RegisterPlayerEvent(12, function(event, player, amount, victim, source) end)
RegisterPlayerEvent(13, function(event, player, oldLevel) end)
RegisterPlayerEvent(14, function(event, player, amount) end)
RegisterPlayerEvent(15, function(event, player, factionId, standing, incremental) end)
RegisterPlayerEvent(16, function(event, player, points) end)
RegisterPlayerEvent(17, function(event, player, noCost) end)
RegisterPlayerEvent(18, function(event, player, message, chatType, language) end)
RegisterPlayerEvent(19, function(event, player, message, chatType, language, receiver) end)
RegisterPlayerEvent(20, function(event, player, message, chatType, language, group) end)
RegisterPlayerEvent(21, function(event, player, message, chatType, language, guild) end)
RegisterPlayerEvent(22, function(event, player, message, chatType, language, channelId, channel) end)
RegisterPlayerEvent(23, function(event, player, emote) end)
RegisterPlayerEvent(24, function(event, player, textEmote, emoteNum, guid) end)
RegisterPlayerEvent(25, function(event, player) end)
RegisterPlayerEvent(26, function(event, player, difficulty, mapId, permanent) end)
RegisterPlayerEvent(27, function(event, player, newZone, newArea) end)
RegisterPlayerEvent(28, function(event, player) end)
RegisterPlayerEvent(29, function(event, player, item, bag, slot) end)
RegisterPlayerEvent(30, function(event, player) end)
RegisterPlayerEvent(31, function(event, player, itemEntry) end)
RegisterPlayerEvent(32, function(event, player, item, count, guid) end)
RegisterPlayerEvent(33, function(event, player, enemy) end)
RegisterPlayerEvent(34, function(event, player) end)
RegisterPlayerEvent(35, function(event, player) end)
RegisterPlayerEvent(36, function(event, player) end)
RegisterPlayerEvent(37, function(event, player, amount) end)
RegisterPlayerEvent(38, function(event, player, questId) end)
RegisterPlayerEvent(39, function(event, player, talentId, talentRank, spellId) end)
RegisterPlayerEvent(42, function(event, player, command, chatHandler) end)
RegisterPlayerEvent(43, function(event, player, pet) end)
RegisterPlayerEvent(44, function(event, player, spellId) end)
RegisterPlayerEvent(46, function(event, player, hasFfaPvp) end)
RegisterPlayerEvent(47, function(event, player, oldArea, newArea) end)
RegisterPlayerEvent(48, function(event, player, target) end)
RegisterPlayerEvent(49, function(event, player, receiverGuid, mailboxGuid, subject, body, money, cod, item) end)
RegisterPlayerEvent(51, function(event, player, item, count) end)
RegisterPlayerEvent(52, function(event, player, item, count) end)
RegisterPlayerEvent(53, function(event, player, item, count) end)
RegisterPlayerEvent(54, function(event, player, quest) end)
RegisterPlayerEvent(55, function(event, player, memberName) end)
RegisterPlayerEvent(56, function(event, player, item, count, voteType, roll) end)
RegisterPlayerEvent(57, function(event, player, type) end)
RegisterPlayerEvent(58, function(event, player, killed) end)
RegisterPlayerEvent(59, function(event, player) end)
RegisterPlayerEvent(60, function(event, player, skillId) end)
RegisterPlayerEvent(61, function(event, player, skillId, value, max, step) end)
RegisterPlayerEvent(62, function(event, player, skillId, value, max, step, newValue) end)
```

玩家聊天事件返回值：

```lua
return false              -- 拦截聊天
return "新的聊天内容"     -- 改写聊天内容
```

聊天事件里的 `chatType` 已按 3.3.5 Eluna/WotLK 数值传给 Lua，而不是 Turtle 1.12
客户端原始数值。常用值包括：普通说话 `1`、队伍 `2`、团队 `3`、公会 `4`、
官员 `5`、喊话 `6`、密语 `7`、频道 `17`。这样从 3.3.5 移过来的聊天脚本
可以继续使用原来的 `type` 判断。

这个返回值规则当前适用于 `18` 普通聊天、`19` 密语、`20` 队伍/团队/战场小队聊天、
`21` 公会/官员聊天、`22` 频道聊天。`23` 是动作表情事件，只通知脚本，不接收返回值。

玩家事件说明：

- `1` 角色创建成功后触发，参数是新建角色的 Player 对象。该角色此时已经保存到数据库，但还没有真正进入世界，脚本里不要做依赖地图可见对象的操作。
- `2` 角色删除前触发，参数是被删除角色的 `guidLow`。这个事件保持 3.3.5 Eluna 的低位数字形式；需要对象时可用 `CreatePlayerGuid(guidLow)` 转成 `ObjectGuid`。
- `3` 玩家登录。
- `4` 玩家登出。
- `5` 玩家施法，参数是玩家、`Spell` 对象和 `skipCheck`。这个事件会和按法术 ID 注册的 `RegisterSpellEvent(spellId, 2, ...)` 在同一次施法中分别触发。
- `6` 玩家击杀玩家，参数是击杀者玩家和被击杀玩家。
- `7` 玩家击杀 Creature，参数是击杀者玩家和被击杀 Creature；宠物、召唤物等归属击杀会尽量使用核心判定出的玩家归属。
- `8` 玩家被 Creature 击杀，参数是击杀 Creature 和死亡玩家。玩家或玩家宠物造成的击杀不会走这个事件。
- `9` 玩家发起决斗请求，参数是被挑战者和挑战者。
- `10` 决斗正式开始，参数是挑战发起者和另一名玩家。
- `11` 决斗结束，参数是核心判定的胜者、败者和结束类型。`DUEL_INTERRUPTED` 这类中断没有真正胜负，脚本应结合 `type` 判断。
- `12` 玩家获得经验前触发，参数是经验值、受害者单位和来源。当前 Turtle 1.12 核心没有 3.3.5 的经验来源枚举，所以 `source` 暂时固定传 `0`。返回数字可以改写本次经验。
- `13` 玩家等级变化，参数是玩家和旧等级；触发时玩家对象已经应用新等级。
- `14` 玩家金币变化前触发，`amount` 是本次变化的铜币差值，正数为增加、负数为扣除。返回数字可以改写本次差值。不要在这个回调里再次调用 `SetMoney()` / `ModifyMoney()`，需要改钱时直接 `return 新差值`。
- `15` 玩家声望变化前触发，参数是阵营 ID、变化值或目标 standing、是否增量。返回数字可以改写本次值；返回 `-1` 或 `false` 可以阻止本次声望变化。
- `16` 玩家可用天赋点变化，参数是新的可用点数。
- `17` 玩家重置天赋成功后触发，`noCost` 表示本次是否免金币消耗。
- `18` 玩家普通聊天；当前支持拦截和改写消息。
- `19` 玩家发送密语前触发，参数是发送者、消息、聊天类型、语言和接收者玩家对象。返回 `false` 可以阻止发送，返回字符串可以改写密语内容。
- `20` 玩家发送队伍、团队、团队领袖、团队警告、战场、战场领袖聊天前触发，参数里最后一个是 `Group` 对象。返回 `false` 可以阻止发送，返回字符串可以改写消息。
- `21` 玩家发送公会或官员聊天前触发，参数里最后一个是 `Guild` 对象。返回 `false` 可以阻止发送，返回字符串可以改写消息。
- `22` 玩家发送频道聊天前触发，第 6 个参数 `channelId` 是 3.3.5 Eluna 风格的频道数字；第 7 个参数是 Turtle 额外追加的 `Channel` 对象，需要频道名时用 `channel:GetName()` 或 `tostring(channel)`。返回 `false` 可以阻止发送，返回字符串可以改写消息。
- `23` 玩家发送动作表情包 `CMSG_EMOTE` 时触发，参数是核心 emote 数字。当前 Turtle 客户端路径只允许核心已经硬编码的 `EMOTE_ONESHOT_NONE` 和 `EMOTE_ONESHOT_WAVE` 进入这条事件；聊天框 `/me 文本` 仍走 `18` 普通聊天里的 `CHAT_MSG_EMOTE`，普通目标文字表情继续走 `24`。
- `24` 玩家文字表情，`guid` 是目标对象的 `ObjectGuid` 对象。需要低位数字时用 `guid:GetGUIDLow()`。
- `25` 玩家保存前触发。内部加了重入保护，避免 Lua 回调里调用 `player:SaveToDB()` 后无限递归。
- `26` 玩家绑定到副本进度时触发，参数是 `difficulty`、mapId 和是否永久绑定。Turtle 1.12 没有 3.3.5 的副本难度系统，所以 `difficulty` 当前固定传 `0`。加载数据库已有绑定时不会触发，只有实际新绑定或更新绑定时触发。
- `27` 玩家切换 Zone 时触发，参数是新 Zone 和新 Area。
- `28` 玩家完成地图切换并加入新地图后触发。
- `29` 玩家成功装备物品后触发，参数是物品对象、bag 和 slot。当前只在角色已经在世界中且本次装备需要客户端更新时触发，避免角色加载装备时误触发。
- `30` 玩家首次登录角色时触发，挂在 `AT_LOGIN_FIRST` 首登标记处理路径里。触发后核心会移除这个首登标记，所以同一个角色正常只触发一次。
- `31` 核心判断玩家是否可使用某个物品模板时触发，参数是 itemEntry。返回 `InventoryResult` 数字可以阻止使用；返回 `0` 或不返回表示允许继续。
- `32` 玩家拾取物品后触发，参数是物品对象、本次数量和战利品来源对象 `guid`。需要低位数字时用 `guid:GetGUIDLow()`。
- `33` 玩家进入战斗时触发，`enemy` 是触发本次进战的单位；某些核心辅助进战路径可能传 `nil`。
- `34` 玩家离开战斗时触发。
- `35` 玩家释放灵魂/回墓地时触发。Turtle 核心里的 `RepopAtGraveyard()` 有时也会被活着的玩家路径调用，当前 Lua 层只在调用前玩家处于死亡状态时触发。
- `36` 玩家成功复活后触发。触发时玩家已经恢复为存活状态，生命/能量、鬼魂光环和可见性已经由核心处理。
- `37` 玩家从拾取中获得金币后触发，`amount` 是这次拾取到的铜币数量。修改金币请继续用 `14` 金币变化事件。
- `38` 玩家主动放弃任务后触发，参数是 questId。当前挂在 `CMSG_QUESTLOG_REMOVE_QUEST` 处理路径里，不会因为任务失败或核心内部移除任务而触发。
- `39` 玩家学习天赋后触发，参数是天赋 ID、天赋 rank 和实际学习的 spellId。
- `42` 玩家或控制台尝试执行命令时触发，参数是玩家对象或 `nil`、命令文本和 `chatHandler`。`ChatHandler` 已封装常用方法：`SendSysMessage`、`IsConsole`、`GetPlayer`、`SendGlobalSysMessage`、`SendGlobalGMSysMessage`、`HasLowerSecurity`、`HasLowerSecurityAccount`、`GetSelectedPlayer`、`GetSelectedCreature`、`GetSelectedUnit`、`GetSelectedObject`、`GetSelectedPlayerOrSelf`、`IsAvailable`、`HasSentErrorMessage`。返回 `false` 可以拦截命令并阻止核心继续解析，适合做 Lua 自定义 GM 命令。
- `43` 玩家宠物或玩家召唤物加入世界后触发，参数是玩家和宠物/召唤物 Creature 对象。当前覆盖 `Pet::AddToWorld()`，并补充覆盖玩家作为召唤者的临时召唤物加入世界。
- `44` 玩家学习法术后触发，参数是 spellId。当前只在角色已经在世界中并且核心确认本次确实学到新法术时触发；学习天赋法术时通常会同时触发 `39` 和 `44`。
- `46` 玩家 FFA PvP 状态变化后触发，`hasFfaPvp` 表示变化后的状态。
- `47` 玩家切换 Area 时触发，参数是旧 Area 和新 Area。
- `48` 玩家发起交易前触发，参数是发起者和目标玩家。返回 `false` 可以阻止本次交易。
- `49` 玩家发送邮件前触发，参数是收件人 `ObjectGuid`、邮箱 `ObjectGuid`、标题、正文、铜币、COD 铜币和附件物品对象。返回 `false` 可以阻止本次邮件发送。
- `51` 玩家获得任务奖励物品后触发，参数是物品对象和本次数量。它会和通用的 `52` / `53` 在同一次奖励里先后触发。
- `52` 玩家创建新物品后触发，参数是物品对象和本次数量。当前覆盖 `StoreNewItem()` 和 `EquipNewItem()` 成功路径，不在角色加载背包时触发。
- `53` 玩家把新物品放入背包后触发，参数是物品对象和本次数量。当前覆盖 `StoreNewItem()` 成功路径，不覆盖直接装备到装备栏的 `EquipNewItem()`。
- `54` 玩家完成任务后触发，参数是 `Quest` 对象；触发点在核心把任务状态设为完成之后。
- `55` 玩家发出组队邀请前触发，参数是邀请者玩家和被邀请角色名字符串。返回 `false` 可以阻止本次邀请；当前 Turtle 适配层先传角色名，不传被邀请者 Player 对象，以贴近 3.3.5 Eluna 的 `memberName` 参数。
- `56` 队伍掷骰获胜并实际把物品放进背包后触发，参数是获胜玩家、物品对象、数量、voteType 和 roll。当前 `voteType` 使用核心 `ROLL_NEED` / `ROLL_GREED` 数值；最后一个参数现在是 `Roll` 对象，可读取本次掷骰的物品、来源和参与玩家投票信息。
- `57` 玩家因主动离开战场或竞技场而获得逃亡惩罚前触发，`type=0` 表示离开战场，`type=5` 表示离开竞技场。Turtle 目前先覆盖这个最常见路径，排队超时/拒绝进入等 3.3.5 细分类型后续再补。
- `58` 玩家宠物、召唤物或被玩家控制的 Creature 击杀 Creature 时触发，参数是归属玩家和被击杀 Creature。这个事件不接收返回值；玩家本人直接击杀仍走 `7`。
- `59` 玩家接受复活请求、核心真正复活前触发，参数是玩家。返回 `false` 可以阻止本次复活，并清理当前复活请求，避免请求残留。
- `60` 玩家技能尝试增长前触发，参数是玩家和 skillId。返回 `false` 可以阻止本次技能增长尝试；覆盖普通技能增长和专业/采集/钓鱼这类概率增长路径。
- `61` 玩家技能增长计算前触发，参数是当前技能值、上限和本次 step。返回数字可以改写参与计算的当前技能值；如果返回 `0` 或大于等于上限，本次增长会被核心自然跳过。
- `62` 玩家技能实际增长成功后触发，参数是增长前参与计算的 value、max、step 和最终 newValue。概率增长未命中时不会触发。

### Creature 事件

```lua
RegisterCreatureEvent(entry, 1, function(event, creature, target) end)
RegisterCreatureEvent(entry, 2, function(event, creature) end)
RegisterCreatureEvent(entry, 3, function(event, creature, victim) end)
RegisterCreatureEvent(entry, 4, function(event, creature, killer) end)
RegisterCreatureEvent(entry, 5, function(event, creature) end)
RegisterCreatureEvent(entry, 6, function(event, creature, movementType, pointId) end)
RegisterCreatureEvent(entry, 7, function(event, creature, diff) end)
RegisterCreatureEvent(entry, 8, function(event, creature, player, emoteId) end)
RegisterCreatureEvent(entry, 9, function(event, creature, attacker, damage) end)
RegisterCreatureEvent(entry, 12, function(event, creature, target) end)
RegisterCreatureEvent(entry, 13, function(event, creature, attacker) end)
RegisterCreatureEvent(entry, 14, function(event, creature, caster, spellId) end)
RegisterCreatureEvent(entry, 15, function(event, creature, target, spellId) end)
RegisterCreatureEvent(entry, 19, function(event, creature, summon) end)
RegisterCreatureEvent(entry, 20, function(event, creature, summon) end)
RegisterCreatureEvent(entry, 23, function(event, creature) end)
RegisterCreatureEvent(entry, 24, function(event, creature) end)
RegisterCreatureEvent(entry, 26, function(event, creature, respawnDelay) end)
RegisterCreatureEvent(entry, 27, function(event, creature, unit) end)
RegisterCreatureEvent(entry, 30, function(event, caster, spellId, effIndex, creature) end)
RegisterCreatureEvent(entry, 36, function(event, creature) end)
RegisterCreatureEvent(entry, 37, function(event, creature) end)
```

对应：进战、脱战、击杀目标、死亡、重生、移动点到达、AI Update、接收玩家表情、受到伤害、主人攻击目标、主人被攻击、被法术命中、法术命中目标、成功召唤生物、召唤生物消失、重置、回到出生点、尸体移除、单位进入视野、DummyEffect、进入世界、移出世界。

`7` 调用频率很高，不建议在这里做大量数据库查询。

移动点到达事件 `6` 传入的是核心 `MovementInform(type, id)`，其中 `type` 可能是路径点、定点移动、追逐或跟随等移动类型；返回 `true` 可以跳过核心原本的 `AI()->MovementInform(type, id)` 回调。

法术命中目标事件 `15` 在生物施法命中目标时触发；返回 `true` 可以跳过核心原本的 `AI()->SpellHitTarget(target, spellInfo)` 回调。

成功召唤生物事件 `19` 在 `Creature` 作为召唤者召唤出 `Creature` 后触发；返回 `true` 可以跳过核心原本的 `AI()->JustSummoned(summon)` 回调。

召唤生物消失事件 `20` 在临时召唤物或守护宠物通知召唤者消失时触发；返回 `true` 可以跳过核心原本的 `AI()->SummonedCreatureDespawn(summon)` 回调。

单位进入视野事件 `27` 在核心准备调用 `AI()->MoveInLineOfSight(unit)` 前触发；返回 `true` 可以跳过核心原本的视野 AI。这个事件触发频率很高，不建议在里面做数据库查询或复杂循环。

DummyEffect 事件 `30` 由核心的 `ScriptMgr::OnEffectDummy` 生物目标入口触发；返回 `false` 会继续交给原核心脚本处理。

生命周期事件 `36` / `37` 在生物加入世界和从世界移除时触发。此时对象仍可被 Lua 读取，但不建议在 `37` 里再次强制删除或传送同一个生物。

伤害事件可以修改伤害：

```lua
RegisterCreatureEvent(12345, 9, function(event, creature, attacker, damage)
    return damage / 2
end)
```

也可以返回两个值，第一个 `true` 表示停止核心后续伤害处理，第二个数字表示新伤害：

```lua
return true, 0
```

主人攻击目标事件 `12` 和主人被攻击事件 `13` 主要用于宠物、守护者、临时召唤物这类有主人的生物；返回 `true` 可以跳过核心原本的 `OwnerAttacked` / `OwnerAttackedBy` AI 回调。

尸体移除事件可以修改本次重生延迟，单位是秒：

```lua
RegisterCreatureEvent(12345, 26, function(event, creature, respawnDelay)
    return 60
end)
```

也可以返回两个值，第一个 `true` 表示跳过核心原本的 `AI()->CorpseRemoved` 回调，第二个数字表示新重生延迟：

```lua
return true, 60
```

重置事件 `23` 会在死亡、脱战重置、重生时触发。回家事件 `24` 会在生物完成回到出生点动作后触发；如果返回 `true`，会跳过核心原本的 `AI()->JustReachedHome()` 回调。

### Quest 事件

任务事件现在传的是 `Quest` 对象，不再是旧文档里的 `questId` 数字。

```lua
RegisterCreatureEvent(entry, 31, function(event, player, creature, quest) end)
RegisterCreatureEvent(entry, 34, function(event, player, creature, quest) end)
RegisterGameObjectEvent(entry, 4, function(event, player, gameobject, quest) end)
RegisterGameObjectEvent(entry, 5, function(event, player, gameobject, quest) end)
RegisterItemEvent(entry, 3, function(event, player, item, quest) end)
```

示例：

```lua
RegisterCreatureEvent(12345, 31, function(event, player, creature, quest)
    player:SendBroadcastMessage("接到任务：" .. quest:GetTitle())
end)
```

### Spell 事件

这部分监听的是一次正在释放中的动态 `Spell` 对象，不是 `GetSpellInfo()` 返回的只读 DBC 模板。

```lua
RegisterSpellEvent(spellId, 1, function(event, caster, spell) end)
RegisterSpellEvent(spellId, 2, function(event, caster, spell, skipCheck) end)
RegisterSpellEvent(spellId, 3, function(event, caster, spell, bySelf) end)
```

事件含义：

- `1` 在核心完成施法检查、计算消耗/读条时间/持续时间后，正式发送施法前触发。
- `2` 在核心进入 `Spell::cast()` 并刷新目标指针后触发。
- `3` 在核心取消施法时触发。
- 当前返回值不会改变核心施法流程；如需中断施法，可以在回调里调用 `spell:Cancel()`。

### Gossip 和使用事件

```lua
RegisterCreatureGossipEvent(entry, 1, function(event, player, creature) end)
RegisterCreatureGossipEvent(entry, 2, function(event, player, creature, sender, action, code) end)
RegisterGameObjectGossipEvent(entry, 1, function(event, player, gameobject) end)
RegisterGameObjectGossipEvent(entry, 2, function(event, player, gameobject, sender, action, code) end)
RegisterItemGossipEvent(entry, 1, function(event, player, item) end)
RegisterItemGossipEvent(entry, 2, function(event, player, item, sender, action, code) end)
RegisterGameObjectEvent(entry, 1, function(event, gameobject, diff) end)
RegisterGameObjectEvent(entry, 2, function(event, gameobject) end)
RegisterGameObjectEvent(entry, 3, function(event, caster, spellId, effIndex, gameobject) end)
RegisterGameObjectEvent(entry, 9, function(event, gameobject, lootState) end)
RegisterGameObjectEvent(entry, 10, function(event, gameobject, goState) end)
RegisterGameObjectEvent(entry, 12, function(event, gameobject) end)
RegisterGameObjectEvent(entry, 13, function(event, gameobject) end)
RegisterGameObjectEvent(entry, 14, function(event, gameobject, player) end)
RegisterItemEvent(entry, 1, function(event, caster, spellId, effIndex, item) end)
RegisterItemEvent(entry, 2, function(event, player, item, target) end)
RegisterItemEvent(entry, 4, function(event, player, itemId) end)
RegisterItemEvent(entry, 5, function(event, player, item) end)
```

`Item` 使用事件里的 `target` 现在是 `SpellCastTargets` 对象，只保证在当前回调执行期间有效。
默认会继续释放物品自带法术；如果 Lua 明确 `return false`，会阻止这次物品法术释放。

Item 事件：

- `1` 由核心 `ScriptMgr::OnEffectDummy` 的 Item 目标入口触发，参数为施法者、法术 ID、效果下标、目标物品。
- `2` 在右键使用物品时触发，`target` 是 `SpellCastTargets` 对象。
- `4` 在限时物品即将到期并被核心删除前触发；返回 `true` 可以阻止本次到期删除。
- `5` 在 `Player::DestroyItem` 实际删除物品前触发；返回 `true` 可以阻止本次删除。
- 到期和删除事件属于危险入口，不建议在回调里再次删除同一个物品，除非脚本明确 `return true` 接管后续流程。

Item Gossip：

- `RegisterItemGossipEvent(entry, 1, ...)` 会在玩家右键使用该物品时触发。
- 回调里可以用 `player:GossipClearMenu()`、`player:GossipMenuAddItem(...)`、`player:GossipSendMenu(textId, item)` 打开物品菜单。
- 物品菜单被点击后，会进入 `RegisterItemGossipEvent(entry, 2, ...)`，参数为 `event, player, item, sender, action, code`。
- 如果同一个物品也有使用法术，默认仍会继续释放物品法术；在物品 Gossip Hello 里明确 `return false` 可以阻止这次物品法术释放。

GameObject 状态事件：

- `1` 在 GameObject 更新时触发，参数 `diff` 是本次更新间隔毫秒数。这个事件触发频率很高，不建议在里面做数据库查询或复杂循环。
- `2` 在默认刷新物体的重生计时到期、核心把 GameObject 重新加入地图后触发；初始地图加载或临时召唤进入世界使用 `12`。
- `3` 由核心的 `ScriptMgr::OnEffectDummy` GameObject 目标入口触发；返回 `false` 会继续交给原核心脚本处理。
- `9` 在已经进入世界的 GameObject 的 `LootState` 改变时触发。
- `10` 在已经进入世界的 GameObject 的 `GOState` 改变时触发。
- `12` / `13` 在 GameObject 加入世界和从世界移除时触发。
- 初始化阶段的状态设置不会触发 Lua，避免地图加载时误执行脚本。

## WorldPacket 方法

`WorldPacket` 是服务端和客户端之间的底层网络包对象。当前已经完成基础对象封装，可以由 `CreatePacket(opcode, size)` 创建，并通过 `Player`、`Group`、`Guild`、`WorldObject` 的 `SendPacket(packet)` 发送，也可以在包事件里读取和替换核心正在处理的包。

可用方法：

```lua
packet:GetOpcode()
packet:GetSize()
packet:SetOpcode(opcode)
packet:ReadByte()
packet:ReadUByte()
packet:ReadShort()
packet:ReadUShort()
packet:ReadLong()
packet:ReadULong()
packet:ReadFloat()
packet:ReadDouble()
packet:ReadGUID()
packet:ReadGuid()
packet:ReadString()
packet:WriteGUID(guidOrObjectOrPlayerLowGuid)
packet:WriteGuid(guidOrObjectOrPlayerLowGuid)
packet:WriteString(value)
packet:WriteByte(value)
packet:WriteUByte(value)
packet:WriteShort(value)
packet:WriteUShort(value)
packet:WriteLong(value)
packet:WriteULong(value)
packet:WriteFloat(value)
packet:WriteDouble(value)
```

说明：

- `ReadGUID()` 返回 `ObjectGuid` 对象；`WriteGUID()` 可以接收 `ObjectGuid`、世界对象，或玩家低位 GUID 数字。
- 读包越界会抛出 Lua 错误 `packet read out of range`，避免 C++ 的 `ByteBufferException` 直接穿出。
- 这是底层包接口，脚本必须按 1.12 客户端 opcode 的字段顺序写入；opcode 或字段写错可能导致客户端断开、界面异常或客户端崩溃。
- 包事件里拿到的是当前包的副本。只读取它不会改变核心原始包；如果要修改核心继续处理或真正发出的内容，需要 `return true, newPacket` 或 `return nil, newPacket` 把新的 `WorldPacket` 作为第二个返回值传回。
- 包事件返回 `false` 会拦截这个包。收包事件中表示核心不再执行对应 opcode handler；发包事件中表示不发送给客户端。

### 包事件

```lua
-- 5 = PACKET_EVENT_ON_PACKET_RECEIVE
-- 7 = PACKET_EVENT_ON_PACKET_SEND
RegisterPacketEvent(opcode, 5, function(event, packet, player)
    -- 返回 false 可以拦截客户端入包
end)

RegisterPacketEvent(opcode, 7, function(event, packet, player)
    -- 返回 false 可以阻止这个服务端包发给客户端
end)

ClearPacketEvents(opcode)
ClearPacketEvents(opcode, 5)
```

也可以用 `RegisterServerEvent(5, ...)` 和 `RegisterServerEvent(7, ...)` 监听所有 opcode。`6` 是 3.3.5 Eluna 里的未知包事件枚举，当前 Turtle 这份核心的未知 opcode 会在队列阶段被跳过，所以没有单独触发。

## ObjectGuid 方法

`ObjectGuid` 是 GUID 值对象，不是世界里的实体指针，可以安全保存低位、类型和字符串信息。
对象本身不代表目标一定还在世界里；需要实体对象时仍应通过玩家、生物、物品、地图等接口重新查找。

获取方式：

```lua
local guid = player:GetGUID()
local playerGuid = CreatePlayerGuid(guidLow)
local itemGuid = CreateItemGuid(itemGuidLow)
local creatureGuid = CreateCreatureGuid(entry, counter)
local goGuid = CreateGameObjectGuid(entry, counter)
local elunaStyleCreatureGuid = GetUnitGUID(lowguid, entry)
local low = GetGUIDLow(guid)
local high = GetGUIDType(guid)
local entry = GetGUIDEntry(guid)
```

可用方法：

```lua
guid:GetCounter()
guid:GetGUIDLow()
guid:GetEntry()
guid:GetHigh()
guid:GetHighGuid()
guid:GetTypeId()
guid:GetTypeID()
guid:GetMaxCounter()
guid:GetTypeName()
guid:GetString()
guid:ToString()
guid:GetRawValue()        -- 返回十进制字符串，避免 Lua number 精度丢失
guid:GetRawValueString()
guid:IsEmpty()
guid:IsPlayer()
guid:IsCreature()
guid:IsPet()
guid:IsCreatureOrPet()
guid:IsUnit()
guid:IsItem()
guid:IsGameObject()
guid:IsDynamicObject()
guid:IsCorpse()
guid:IsTransport()
guid:IsMOTransport()
guid:Equals(otherGuid)
```

`tostring(guid)` 等同于 `guid:ToString()`，两个 `ObjectGuid` 也可以用 `==` 比较。

## 通用对象方法

`Player`、`Creature`、`GameObject`、`Item` 支持：

```lua
obj:GetEntry()
obj:GetGUID()
obj:GetGuid()
obj:GetObjectGuid()
obj:GetGUIDLow()
obj:GetTypeId()
obj:GetTypeID()
obj:IsPlayer()
obj:IsCreature()
obj:IsUnit()
obj:IsGameObject()
obj:IsItem()
obj:IsDynamicObject()
obj:IsWorldObject()
obj:GetScale()
obj:SetScale(scale)
obj:IsInWorld()
obj:GetUInt32Value(index)
obj:GetInt32Value(index)
obj:GetFloatValue(index)
obj:GetByteValue(index, offset)
obj:GetUInt16Value(index, offset)
obj:GetUInt64Value(index)
obj:SetUInt32Value(index, value)
obj:SetInt32Value(index, value)
obj:UpdateUInt32Value(index, value)
obj:SetFloatValue(index, value)
obj:SetByteValue(index, offset, value)
obj:SetUInt16Value(index, offset, value)
obj:SetInt16Value(index, offset, value)
obj:SetUInt64Value(index, value)
obj:HasFlag(index, flag)
obj:SetFlag(index, flag)
obj:RemoveFlag(index, flag)
obj:ToPlayer()
obj:ToCreature()
obj:ToUnit()
obj:ToGameObject()
obj:ToCorpse()
obj:ToDynamicObject()
```

`Player`、`Creature`、`GameObject`、`Corpse`、`DynamicObject` 支持：

```lua
obj:GetMapId()
obj:GetMapID()
obj:GetMap()
obj:GetPhaseMask()
obj:SetPhaseMask(phaseMask, update)
obj:GetInstanceId()
obj:GetInstanceID()
obj:GetZoneId()
obj:GetZoneID()
obj:GetAreaId()
obj:GetAreaID()
obj:GetX()
obj:GetY()
obj:GetZ()
obj:GetO()
obj:GetLocation()
obj:GetDistance(other)
obj:GetDistance(x, y, z)
obj:GetDistance2d(other)
obj:GetDistance2d(x, y)
obj:GetExactDistance(other)
obj:GetExactDistance(x, y, z)
obj:GetExactDistance2d(other)
obj:GetExactDistance2d(x, y)
obj:GetRelativePoint(distance, angle)
obj:GetAngle(other)
obj:GetAngle(x, y)
obj:IsWithinDist(other, distance, is3D)
obj:IsWithinDist3d(x, y, z, distance)
obj:IsWithinDist2d(x, y, distance)
obj:IsWithinDistInMap(other, distance, is3D)
obj:IsInMap(other)
obj:IsInRange(other, minRange, maxRange, is3D)
obj:IsInRange2d(x, y, minRange, maxRange)
obj:IsInRange3d(x, y, z, minRange, maxRange)
obj:IsInFront(other, arc)
obj:IsInBack(other, arc)
obj:IsWithinLOS(other)
obj:IsFriendlyTo(other)
obj:IsHostileTo(other)
obj:GetPlayersInRange(range, hostile, dead)
obj:GetCreaturesInRange(range, entry, hostile, dead)
obj:GetGameObjectsInRange(range, entry, hostile)
obj:GetNearestPlayer(range, hostile, dead)
obj:GetNearestCreature(range, entry, hostile, dead)
obj:GetNearestGameObject(range, entry, hostile)
obj:GetNearObject(range, typeMask, entry, hostile, dead)
obj:GetNearObjects(range, typeMask, entry, hostile, dead)
obj:FindNearestPlayer(range)
obj:FindNearestCreature(entry, range, alive)
obj:FindNearestGameObject(entry, range)
obj:SummonCreature(entry, x, y, z, o, summonType, despawnTime)
obj:SpawnCreature(entry, x, y, z, o, summonType, despawnTime)
obj:SummonGameObject(entry, x, y, z, o, respawnTime)
obj:PlayMusic(musicId, player)
obj:PlayDirectSound(soundId, player)
obj:PlayDistanceSound(soundId, player)
obj:RegisterEvent(function, delayMs, repeats)
obj:RemoveEventById(eventId)
obj:RemoveEvents()
obj:SendPacket(packet)
```

查找不到对象时，现在会正确返回 `nil`。
`ToPlayer()`、`ToCreature()`、`ToUnit()`、`ToGameObject()`、`ToCorpse()` 会按真实对象类型转换，转换失败时返回 `nil`。
`Get*Value` / `Set*Value` 是底层 UpdateField 读写接口，index 和 offset 必须按 Turtle 1.12 的 `UpdateFields.h` 使用，写错可能直接改坏对象状态。
`GetPhaseMask` / `SetPhaseMask` 在 Turtle 1.12 中映射到核心的 `WorldMask`；`SetPhaseMask` 的 `update` 参数保留为 Eluna 兼容参数，但当前 Turtle 核心没有 3.3.5 那种同名可见性刷新接口。
`GetNearestCreature` / `GetNearestGameObject` 使用 Eluna 参数顺序，旧的 `FindNearestCreature(entry, range, alive)` 和 `FindNearestGameObject(entry, range)` 仍然保留。
`GetNearObject` / `GetNearObjects` 当前枚举已加载的 `Player`、`Creature`、`GameObject`；`Corpse` 对象已可由玩家、地图和 `ToCorpse()` 等路径取得。
`hostile` 参数约定为 `0` 不过滤、`1` 敌对、`2` 友方；`dead` 参数约定为 `0` 不过滤、`1` 存活、`2` 死亡。
`RegisterEvent` / `RemoveEventById` / `RemoveEvents` 已实现对象绑定定时事件。回调参数为 `(eventId, delay, repeats, object)`；`delay` 可以是毫秒数，也可以是 `{minDelay, maxDelay}` 随机延迟表；`repeats = 0` 表示无限重复。绑定对象消失、退出世界或玩家下线后，对应事件会自动移除。当前实现由 Lua 引擎统一更新，不完全模拟 3.3.5 Eluna 中 Creature/GameObject 只有进入可见范围才计时的细节。`SendPacket` 现在可以发送由 `CreatePacket()` 创建的 `WorldPacket`。

## Player 方法

常用方法：

```lua
player:GetName()
player:GetAccountId()
player:GetLevel()
player:SetLevel(level)
player:IsAlive()
player:GetHealth()
player:GetMaxHealth()
player:GetHealthPct()
player:GetHealthPercent()
player:IsFullHealth()
player:HealthAbovePct(pct)
player:HealthBelowPct(pct)
player:CountPctFromCurHealth(pct)
player:CountPctFromMaxHealth(pct)
player:SetHealth(value)
player:SetMaxHealth(value)
player:GetPower(powerType)
player:GetMaxPower(powerType)
player:SetPower(powerType, value)
player:SetMaxPower(powerType, value)
player:ModifyPower(amount, powerType)
player:GetPowerType()
player:SetPowerType(powerType)
player:GetPowerPct(powerType)
player:GetRace()
player:GetClass()
player:GetRaceMask()
player:GetClassMask()
player:GetRaceAsString(locale)
player:GetClassAsString(locale)
player:GetCreatureType()
player:GetStat(stat)
player:HandleStatModifier(stat, modifierType, value, apply)
player:GetBaseSpellPower(spellSchool)
player:GetGender()
player:IsDead()
player:IsDying()
player:IsMounted()
player:IsOnVehicle()
player:GetVehicle()
player:GetVehicleKit()
player:IsRooted()
player:IsFeared()
player:IsConfused()
player:IsPolymorphed()
player:IsStopped()
player:IsTaxiFlying()
player:IsCharmed()
player:IsAttackingPlayer()
player:GetStandState()
player:SetStandState(state)
player:GetUnitState()
player:HasUnitState(stateFlags)
player:AddUnitState(stateFlags)
player:ClearUnitState(stateFlags)
player:GetSpeed(moveType)
player:GetSpeedRate(moveType)
player:SetSpeed(moveType, speed)
player:SetSpeedRate(moveType, rate)
player:GetMovementType()
player:MoveStop()
player:MoveExpire(reset)
player:MoveClear(reset, all)
player:MoveIdle()
player:MoveRandom(radius, expireTime)
player:MoveHome()
player:MoveFollow(target, dist, angle)
player:MoveChase(target, dist, angle)
player:MoveConfused()
player:MoveFleeing(target, time)
player:MoveTo(id, x, y, z, genPath)
player:MoveJump(x, y, z, horizontalSpeed, maxHeight, id)
player:NearTeleport(x, y, z, orientation)
player:GetAttackers()
player:GetThreatList()
player:GetThreat(victim)
player:AddThreat(victim, threat, schoolMask, spellId)
player:ModifyThreatPct(victim, percent)
player:ModifyThreatPercent(victim, percent)
player:ClearThreat(victim)
player:ResetAllThreat()
player:ClearThreatList()
player:GetFriendlyUnitsInRange(range)
player:GetUnfriendlyUnitsInRange(range)
player:HasAura(spellId)
player:GetAura(spellId, effectIndex)
player:AddAura(spellId, caster)
player:RemoveAura(spellId)
player:RemoveAllAuras()
player:RemoveArenaAuras(onleave)
player:GetVictim()
player:GetSelection()
player:GetSelectedUnit()
player:GetSelectedPlayer()
player:GetSelectedCreature()
player:GetSelectedGameObject()
player:GetSelectedGO()
player:GetSelectedObject()
player:GetSelectedWorldObject()
player:GetSelectionGUID()
player:GetSelectedGUID()
player:GetSelectedGameObjectGUID()
player:GetSelectedGOGUID()
player:Attack(target, melee)
player:AttackStop()
player:Kill(target)
player:DealDamage(target, damage, durabilityLoss, school, spellId)
player:DealHeal(target, spellId, amount, critical)
player:IsInCombat()
player:ClearInCombat()
player:SetInCombatWith(enemy)
player:SetOwnerGUID(guid)
player:SetCreatorGUID(guid)
player:SetPetGUID(guid)
player:SetCritterGUID(guid)
player:SetName(name)
player:SetImmuneTo(mechanic, apply)
player:IsUnderWater()
player:IsUnderwater()
player:IsCasting()
player:IsPvPFlagged()
player:IsStandState()
player:IsVendor()
player:IsTrainer()
player:IsQuestGiver()
player:IsGossip()
player:IsTaxi()
player:IsGuildMaster()
player:IsBattleMaster()
player:IsBanker()
player:IsInnkeeper()
player:IsSpiritHealer()
player:IsSpiritGuide()
player:IsTabardDesigner()
player:IsAuctioneer()
player:IsArmorer()
player:IsServiceProvider()
player:IsSpiritService()
player:CastSpell(target, spellId, triggered)
player:CastCustomSpell(target, spellId, triggered, bp0, bp1, bp2, castItem, originalCaster, addThreat)
player:CastSpellAoF(x, y, z, spellId, triggered)
player:SendUnitSay(message, language)
player:SendUnitYell(message, language)
player:SendUnitWhisper(message, language, receiver, bossWhisper)
player:SendUnitEmote(message, receiver, bossEmote)
player:SendChatMessageToPlayer(chatType, language, message, receiver)
player:GetDisplayId()
player:SetDisplayId(displayId)
player:GetNativeDisplayId()
player:SetNativeDisplayId(displayId)
player:RestoreDisplayId()
player:DeMorph()
player:GetMountId()
player:Mount(displayId)
player:Dismount()
player:GetFaction()
player:SetFaction(factionId)
player:RestoreFaction()
player:SetSheath(sheathState)
player:SetRooted(apply)
player:SetConfused(apply)
player:SetFeared(apply)
player:SetFacing(orientation)
player:SetFacingToObject(target)
player:SetPvP(apply)
player:SetFFA(apply)
player:SetSanctuary(apply)
player:SetWaterWalk(enable)
player:StopSpellCast(spellId)
player:GetCurrentSpell(spellType)
player:InterruptSpell(spellType, delayed)
player:PerformEmote(emoteId)
player:EmoteState(emoteId)
player:GetOwner()
player:GetOwnerGUID()
player:GetCreatorGUID()
player:GetMinionGUID()
player:GetPetGUID()
player:GetCritterGUID()
player:GetControllerGUID()
player:GetControllerGUIDS()
player:GetCharmer()
player:GetCharmerGUID()
player:GetCharm()
player:GetCharmGUID()
player:GetCharmerOrOwner()
player:GetCharmerOrOwnerGUID()
```

移动速度参数 `moveType` 使用 Turtle/MaNGOS 的移动类型：`0` 走路，`1` 跑步，`2` 后退跑，`3` 游泳，`4` 后退游泳，`5` 转向速率。
`SetPower` / `SetMaxPower` 支持 `unit:SetPower(powerType, value)`，也兼容只传 `value` 时使用单位当前能量类型；`ModifyPower(amount, powerType)` 的 `powerType` 可省略。
`SetPowerType(powerType)` 会直接切换单位当前能量类型；真实脚本中要确认当前单位支持该能量类型。
`GetRaceAsString(locale)` / `GetClassAsString(locale)` 从 DBC 里读取名称，`locale` 可省略，当前 Turtle 1.12 只支持 `0..7` 的 DBC 语言下标。
`GetBaseSpellPower(spellSchool)` 目前只对玩家返回 `PLAYER_FIELD_MOD_DAMAGE_DONE_POS` 的数值，Creature 调用返回 `0`。
`SetSpellPower(value, apply)` 使用 Turtle 当前 GM 命令同一条核心路径：先移除 `18058` 法术加成，`apply=true` 且 `value` 非 0 时再按 `value` 重新施加法伤加成；不填 `apply` 时按 3.3.5 Eluna 默认值视为移除。
`SetSpeed(moveType, speed)` 用 Turtle 的基础速度换算成速度倍率后调用核心 `SetSpeedRate`；`SetSpeedRate(moveType, rate)` 直接设置倍率。
移动控制接口直接调用 Turtle/MaNGOS 的 `MotionMaster`。`MoveTo(id, x, y, z, genPath)` 的 `genPath` 默认 `true`；`MoveJump` 在 1.12 核心里使用水平速度和最大高度，不是 3.3.5 的完整移动参数集。
`CastSpell(target, spellId, triggered)` 支持 `target` 传 `nil`，当前适配层额外返回 `true/false` 表示核心是否接受施法；旧脚本忽略返回值即可。
`CastCustomSpell` 的 `bp0/bp1/bp2` 传 `nil` 表示不覆盖该效果基础点数，`originalCaster` 需要传本适配层的 `ObjectGuid` 对象。
`CastSpellAoF` 是区域坐标施法，`triggered` 默认 `true`；`NearTeleport` 只在当前地图内近距离传送，返回是否成功。
`SendUnitSay` / `SendUnitYell` 对 Player 使用玩家聊天，对 Creature 使用怪物聊天；`SendUnitWhisper` 在 Turtle 1.12 中走怪物私语包，`language` 参数只做兼容占位。
`SendChatMessageToPlayer(chatType, language, message, receiver)` 直接组装聊天包发给指定玩家，`chatType` 使用核心 `ChatMsg` 数值。
`SetOwnerGUID`、`SetCreatorGUID`、`SetPetGUID` 接收本适配层 `ObjectGuid` 对象；`SetCritterGUID` 在 Turtle 1.12 没有对应核心字段，当前只是兼容入口，不改变实际状态。
`SetName(name)` 当前只会修改 Player 内存名，Creature 没有安全的运行时改名入口；不建议用于正式改名流程。
`SetImmuneTo(mechanic, apply)` 映射到核心 `IMMUNITY_MECHANIC`；`SetSanctuary(apply)` 会修改 1.12 对应字节标记，Player 还会同步 `PLAYER_FLAGS_SANCTUARY`。
`SetWaterWalk(enable)`、`SetFFA(apply)`、`SetInCombatWith(enemy)` 都会直接改变单位状态，建议只在明确的事件流程里使用。
`InterruptSpell(spellType, delayed)` 使用 Eluna 外部参数：`0` 近战、`1` 通用、`2` 引导、`3` 自动射击。
`GetStat(stat)` 的 `stat` 是 `0..4`，对应力量、敏捷、耐力、智力、精神。
`HandleStatModifier(stat, modifierType, value, apply)` 同样只支持 `stat` 为 `0..4`，会直接套用或移除对应属性修正；`modifierType` 使用 Turtle/MaNGOS 的 `UnitModifierType`。
`GetFriendlyUnitsInRange(range)` / `GetUnfriendlyUnitsInRange(range)` 返回当前地图内附近 Player/Creature 单位数组，按核心的友方/敌方、存活、可见和距离检查过滤，并排除自己。
`GetCurrentSpell(spellType)` 返回当前正在释放的动态 `Spell` 对象，找不到时返回 `nil`。这个对象只在当前核心状态有效，不要保存到全局变量或定时器里长期使用。
`IsInAccessiblePlaceFor(creature)` 需要第二个参数是 Creature 对象，用于检查当前单位是否处在该 Creature 可到达的位置。
Turtle 1.12 没有真实 Vehicle 系统，所以 `IsOnVehicle()` 固定返回 `false`，`GetVehicle()` / `GetVehicleKit()` 固定返回 `nil`，仅作为 3.3.5 脚本兼容入口。
`GetAura(spellId, effectIndex)` 返回 `Aura` 对象，找不到时返回 `nil`；`effectIndex` 使用核心下标 `0..2`，不传时默认 `0`。
`AddAura(spellId, caster)` 当前沿用本适配层已有参数顺序：给当前单位添加光环，第三个参数是可选施法者；返回值已经从布尔值升级为 `Aura` 对象，失败时返回 `nil`。
`RemoveAllAuras()` 会移除单位身上的全部光环，天赋、种族、被动光环也可能受影响，脚本里要谨慎使用。
`RemoveArenaAuras(onleave)` 会清理竞技场相关光环，`onleave` 默认 `false`。它会改变角色状态，不要在普通查询命令里随手调用。
`DealDamage` / `DealHeal` 会产生真实战斗效果，包含伤害、治疗、吸收/抗性、战斗日志等核心路径；建议只在明确的战斗事件或技能逻辑里使用。
仇恨接口主要给 Creature 使用；Player 也挂了同名方法用于兼容旧脚本，但玩家通常没有仇恨列表，所以多数调用会返回 `nil`、`0` 或没有实际效果。
`GetThreatList()` 返回当前仇恨列表里的单位数组；没有仇恨列表或单位不在地图中时返回 `nil`。`GetThreat(victim)` 返回对指定目标的仇恨值。
`AddThreat(victim, threat, schoolMask, spellId)` 的 `schoolMask` 和 `spellId` 可省略；为了兼容已有脚本，也接受旧顺序 `AddThreat(victim, threat, spellId, schoolMask)`。
`ClearThreat(victim)` 和 `ResetAllThreat()` 在 Turtle 1.12 里通过把目标仇恨降低 100% 来适配，不是 3.3.5 Eluna 的原生实现；`ClearThreatList()` 会直接清空整个仇恨列表，战斗脚本里要谨慎使用。
这一组 Unit GUID 方法当前沿用本适配层已有 `GetOwnerGUID` 等方法的行为，返回低位 counter 数字；`GetCritterGUID()` 在 Turtle 1.12 核心没有对应字段，当前返回 `0`。
这部分 `Unit` 方法同样适用于 `Creature`。

选中目标说明：

- `player:GetSelection()` 是 3.3.5 Eluna 风格别名，等同于 `player:GetSelectedUnit()`，返回当前选中的 `Player` 或 `Creature`，没有单位目标时返回 `nil`。
- `player:GetSelectedPlayer()`、`player:GetSelectedCreature()` 会按类型返回对象，类型不匹配时返回 `nil`。
- `player:GetSelectedGameObject()` / `player:GetSelectedGO()` 读取当前选中的 GameObject。
- `player:GetSelectedObject()` / `player:GetSelectedWorldObject()` 会优先返回选中单位，没有单位时再返回选中 GameObject。
- `player:GetSelectionGUID()` / `player:GetSelectedGUID()` 返回当前选中单位的 `ObjectGuid`；`player:GetSelectedGameObjectGUID()` / `player:GetSelectedGOGUID()` 返回选中 GameObject 的 `ObjectGuid`。

玩家专属：

```lua
player:SendBroadcastMessage(message)
player:SendSysMessage(message)
player:SendNotification(message)
player:SendAreaTriggerMessage(message)
player:SendAreaTrigger(message)
player:SendAddonMessage(prefix, message)
player:SendAddonMessage(prefix, message, receiver)
player:SendAddonMessage(prefix, message, channel, receiver)
player:SendCinematicStart(cinematicId)
player:SendUpdateWorldState(field, value)
player:Say(message, language)
player:Yell(message, language)
player:TextEmote(message)
player:GetMoney()
player:SetMoney(value)
player:ModifyMoney(amount)
player:GetXP()
player:SetXP(value)
player:GiveXP(value, victim)
player:IsHorde()
player:IsAlliance()
player:GetTeam()
player:GetTeamId()
player:GetGuildId()
player:GetGuildRank()
player:IsInGroup()
player:IsInGuild()
player:GetGroup()
player:GetOriginalGroup()
player:GetGroupInvite()
player:GetSubGroup()
player:GetOriginalSubGroup()
player:GetGuild()
player:GetGuildName()
player:GetTrader()
player:HasSkill(skillId)
player:GetSkillValue(skillId)
player:GetBaseSkillValue(skillId)
player:GetPureSkillValue(skillId)
player:GetMaxSkillValue(skillId)
player:GetPureMaxSkillValue(skillId)
player:GetSkillTempBonusValue(skillId)
player:GetSkillPermBonusValue(skillId)
player:SetSkill(skillId, step, current, max)
player:AdvanceSkillsToMax()
player:AdvanceSkill(skillId, step)
player:AdvanceAllSkills(step)
player:GetReputation(factionId)
player:GetReputationRank(factionId)
player:SetReputation(factionId, value)
player:GetTotalPlayedTime()
player:GetLevelPlayedTime()
player:IsInWater()
player:IsMoving()
player:IsFlying()
player:CanFly()
player:IsFalling()
player:IsDND()
player:IsAFK()
player:CanSpeak()
player:ToggleAFK()
player:ToggleDND()
player:IsGMVisible()
player:SetGMVisible(on)
player:IsTaxiCheater()
player:SetTaxiCheat(on)
player:IsGMChat()
player:SetGMChat(on)
player:HasAtLoginFlag(flag)
player:SetAtLoginFlag(flag)
player:SetPvPDeath(on)
player:IsRested()
player:CanBlock()
player:CanParry()
player:IsImmuneToDamage()
player:GetFreeTalentPoints()
player:SetFreeTalentPoints(points)
player:LearnTalent(talentId, rank)
player:ResetTalents(noCost)
player:GetComboTarget()
player:GetComboPoints()
player:AddComboPoints(target, count)
player:ClearComboPoints()
player:GetDrunkValue()
player:SetDrunkValue(value, itemId)
player:GetRestBonus()
player:SetRestBonus(value)
player:GetXPRestBonus()
player:SetAcceptWhispers(on)
player:GetChatTag()
player:GetLatency()
player:GetPlayerIP()
player:GetAccountName()
player:GetDbLocaleIndex()
player:GetDbcLocale()
player:GetInGameTime()
player:InArena()
player:InBattleground()
player:InBattleGround()
player:InBattlegroundQueue()
player:InBattleGroundQueue()
player:GetBattlegroundId()
player:GetBattleGroundId()
player:GetBattlegroundTypeId()
player:GetBattleGroundTypeId()
player:GetCurrentBattlegroundQueueSlot()
player:GetLifetimeKills()
player:SetLifetimeKills(value)
player:AddLifetimeKills(value)
player:RemoveLifetimeKills(value)
player:GetMap()
player:GetItemByGUID(guid)
player:GetItemByGuid(guid)
player:GetItemByPos(bag, slot)
player:GetItemByPos(pos)
player:GetItemByEntry(entry)
player:GetEquippedItemBySlot(slot)
player:GetGMRank()
player:IsGM()
player:SetGM(on)
player:AddItem(itemId, count)
player:HasItem(itemId, count, checkBank)
player:GetItemCount(itemId, checkBank)
player:CanUseItem(itemOrTemplateOrEntry)
player:CanEquipItem(itemOrTemplateOrEntry, slot)
player:RemoveItem(itemId, count, checkBank)
player:HasSpell(spellId)
player:HasSpellCooldown(spellId)
player:GetSpellCooldownDelay(spellId)
player:ResetSpellCooldown(spellId, update)
player:ResetAllCooldowns()
player:RemoveArenaSpellCooldowns()
player:GetSpells()
player:LearnSpell(spellId, dependent, talent)
player:RemoveSpell(spellId, disabled, learnLowRank)
player:SetBindPoint(x, y, z, mapId, areaId)
player:GetHomebind()
player:Teleport(mapId, x, y, z, o)
player:SaveToDB(online, force, direct)
player:KickPlayer()
player:ResurrectPlayer(healthPercent, sickness)
player:KillPlayer()
player:SpawnBones()
player:SpawnCorpseBones()
player:DurabilityRepair(position, cost, discount)
player:DurabilityRepairAll(cost, discount)
player:DurabilityLoss(item, percent)
player:DurabilityLossAll(percent, inventory)
player:DurabilityPointsLoss(item, points)
player:DurabilityPointsLossAll(points, inventory)
player:DurabilityPointLossForEquipSlot(slot)
```

消息发送说明：

- `player:SendBroadcastMessage()` / `player:SendSysMessage()` 发送聊天框系统消息。
- `player:SendNotification()` 发送客户端中上方通知。
- `player:SendAreaTriggerMessage()` / `player:SendAreaTrigger()` 发送区域触发样式的屏幕提示。
- `player:SendAddonMessage(prefix, message)` 会把 `prefix .. "\t" .. message` 以 Turtle 1.12 已有的插件消息格式发给自己。
- `player:SendAddonMessage(prefix, message, receiver)` 或 `player:SendAddonMessage(prefix, message, channel, receiver)` 会发给指定玩家；`channel` 参数为了兼容 3.3.5 Eluna 脚本可以保留传入，但 Turtle 当前实现会复用核心自己的插件消息通道，不按这个数值切换频道。
- `player:SendCinematicStart(cinematicId)` 会触发客户端播放 DBC 中的过场镜头 ID。
- `player:SendUpdateWorldState(field, value)` 更新客户端世界状态字段。
- `player:Say()`、`player:Yell()`、`player:TextEmote()` 走核心聊天/表情广播，`language` 默认 `LANG_UNIVERSAL`。

物品查找说明：

- `player:GetItemByGUID(guid)` / `player:GetItemByGuid(guid)` 只接收 `ObjectGuid`，可用 `GetItemGUID(lowguid)` 或 `CreateItemGuid(lowguid)` 构造。
- `player:GetItemByPos(bag, slot)` 按背包和槽位取物品；只传一个参数时按核心打包后的 `pos` 取物品。
- `player:GetItemByEntry(entry)` 会在装备、背包、钥匙链和银行里找第一件匹配物品，找不到返回 `nil`。
- `player:GetEquippedItemBySlot(slot)` 按装备栏位取当前装备，找不到返回 `nil`。
- `player:GetItemCount(itemId, checkBank)` 返回物品数量；`checkBank = true` 时包含银行。
- `player:CanUseItem()` 和 `player:CanEquipItem()` 支持传 `Item` 对象、`ItemTemplate` 对象或 item entry 数字；`CanEquipItem` 的 `slot` 使用核心装备栏位。
- `DurabilityRepairAll(cost, discount)` 现在默认 `cost = true`，和 Eluna 默认行为一致；`DurabilityLoss*` / `DurabilityPointsLoss*` 会直接改物品耐久，脚本里要谨慎调用。

状态查询说明：

- `player:GetOriginalGroup()` / `player:GetOriginalSubGroup()` 读取战场等场景里保存的原队伍；没有时返回 `nil` 或 `0`。
- `player:GetGroupInvite()` 返回当前邀请中的队伍对象，没有邀请时返回 `nil`。
- `player:GetTrader()` 返回当前交易对象里的另一名玩家，没有交易时返回 `nil`。
- `player:GetInGameTime()` 返回角色本次在线计时，单位按核心保存值。
- `player:GetHomebind()` 当前返回 `{ mapId = ..., areaId = ... }`；Turtle 1.12 的炉石坐标字段是 `Player` 私有成员，Lua 适配层先不直接暴露 `x/y/z`。
- `player:IsImmuneToDamage()` 会检查 `UNIT_FLAG_IMMUNE` 和当前所有伤害学派免疫，作为 1.12 近似实现。
- `player:GetSpells()` 返回当前已学习、未被删除且模板存在的法术 ID 数组。
- `player:ResetAllCooldowns()` 和 `player:RemoveArenaSpellCooldowns()` 会真实清理法术冷却；后者使用 Turtle 核心已有“竞技场清冷却”规则。
- `player:AdvanceSkillsToMax()`、`player:AdvanceSkill(skillId, step)`、`player:AdvanceAllSkills(step)` 会真实提高技能熟练度，正式脚本里谨慎调用。
- `player:LearnTalent(talentId, rank)` 和 `player:ResetTalents(noCost)` 调用 Turtle 核心天赋逻辑；当前 `ResetTalents` 额外返回 `true/false` 表示核心是否实际重置成功。
- `player:AddComboPoints(target, count)` / `player:ClearComboPoints()` 会真实改变玩家连击点。
- `player:SetBindPoint(x, y, z, mapId, areaId)` 会修改玩家炉石绑定位置，不只是弹确认窗口。
- `player:HasQuest()`、`player:GetQuestStatus()`、`player:GetQuestRewardStatus()`、`player:CanCompleteQuest()` 可以传任务 ID，也可以传 `GetQuest()` 返回的 `Quest` 对象。
- `player:CanRewardQuest(questOrId, reward, msg)` 的 `reward` 和 `msg` 是可选参数；不传时只做静默检查。
- 技能相关函数使用核心技能 ID，例如采矿、草药学、武器熟练度等。
- `player:SetSkill(skillId, step, current, max)` 的参数顺序按 Eluna 写法，内部会适配 Turtle 的 `SetSkill(skillId, current, max, step)`。
- `player:GetReputation(factionId)` 返回声望点数，`player:GetReputationRank(factionId)` 返回核心 `ReputationRank` 数字。
- `player:SetReputation(factionId, value)` 会查 `FactionEntry`，存在时写入玩家声望。
- 移动状态函数读取核心当前状态，`player:CanFly()` 在 Turtle 1.12 中等同于当前是否处于飞行状态。
- `CanSpeak`、`ToggleAFK`、`ToggleDND`、`SetGMVisible`、`SetGMChat`、`SetTaxiCheat`、`SetPvPDeath` 都是对 Turtle `Player` 现有状态接口的直接封装。
- 战场相关函数只读取当前核心记录：是否在战场/竞技场/排队、战场实例 ID、战场类型 ID。
- `GetLifetimeKills` 和对应 setter/add/remove 使用 1.12 的 `PLAYER_FIELD_LIFETIME_HONORABLE_KILLS` 字段。
- `player:GetComboTarget()` 返回 `ObjectGuid`，没有连击点目标时返回空 GUID。
- `player:GetPlayerIP()` 和 `player:GetAccountName()` 来自当前 `WorldSession`，玩家不在线或没有会话时返回 `nil`。

任务和 Gossip：

```lua
player:AddQuest(questOrId, questGiver)
player:CompleteQuest(questId)
player:IncompleteQuest(questId)
player:GetQuestStatus(questId)
player:HasQuest(questId)
player:GetQuestRewardStatus(questId)
player:CanCompleteQuest(questId)
player:CanCompleteRepeatableQuest(questOrId)
player:CanRewardQuest(questOrId, reward, msg)
player:GetQuestLevel(questOrId)
player:GetReqKillOrCastCurrentCount(questOrId, creatureOrGameObjectEntry)
player:HasQuestForGO(gameObjectEntry)
player:HasQuestForItem(itemEntry)
player:CanShareQuest(questOrId)
player:SetQuestStatus(questOrId, status)
player:RewardQuest(questOrId, reward, questGiver, announce)
player:FailQuest(questOrId)
player:RemoveQuest(questOrId)
player:RemoveActiveQuest(questOrId)
player:KilledMonsterCredit(entry)
player:TalkedToCreature(entry, creatureOrGuid)
player:AreaExploredOrEventHappens(questId)
player:GroupEventHappens(questId, object)
player:GossipClearMenu()
player:GossipMenuAddItem(icon, text, sender, action, coded, boxText)
player:GossipSendMenu(textId, object)
player:GossipComplete()
```

`HasQuestForGO` / `HasQuestForItem` 用于检查玩家是否因为任务需要对应 GameObject 或物品。`CanCompleteRepeatableQuest(questOrId)` 调用 Turtle 核心重复任务完成检查。

`GetQuestLevel(questOrId)` 返回按玩家等级修正后的任务等级；`GetReqKillOrCastCurrentCount(questOrId, entry)` 返回当前杀怪、施法或交互类目标进度。GameObject 目标在 1.12 核心里通常以负 entry 存在，这里同时兼容传正数 GameObject entry。

`TalkedToCreature(entry, creatureOrGuid)` 可以传生物对象，也可以传 `ObjectGuid`。

`RemoveActiveQuest` 目前作为 `RemoveQuest` 的兼容别名；Turtle 1.12 没有 3.3.5 那个完全同名的 `RemoveActiveQuest` 核心接口。

`GossipSendMenu` 的 `object` 现在可以传 `Player`、`Creature`、`GameObject` 或 `Item`。物品 Gossip 脚本里要传当前 `item`，这样客户端点击菜单时才会带回物品 GUID。

## Creature 方法

除通用 `Unit` / `WorldObject` 方法外，当前可用：

```lua
creature:GetName()
creature:GetTemplate()
creature:GetCreatureTemplate()
creature:GetCreatureInfo()
creature:GetDBTableGUIDLow()
creature:Say(message, language)
creature:Yell(message, language)
creature:Whisper(message, player)
creature:TextEmote(message, target, bossEmote)
creature:CastSpell(target, spellId, triggered)
creature:DespawnOrUnsummon(delay)
creature:ForcedDespawn(delay)
creature:GetRespawnDelay()
creature:SetRespawnDelay(delay)
```

说明：

- `creature:GetTemplate()` / `creature:GetCreatureTemplate()` / `creature:GetCreatureInfo()` 返回只读 `CreatureTemplate` 模板对象。

## Aura 方法

`Aura` 是单位身上的光环实例对象，可以通过 `unit:GetAura(spellId, effectIndex)` 取得，也可以由 `unit:AddAura(spellId, caster)` 返回。
它对应核心里当前仍存在的光环指针，脚本中不要把它长期保存到全局变量或定时器里；光环被移除后，请重新用 `GetAura` 查询。

当前可用：

```lua
aura:GetCaster()
aura:GetCasterGUID()
aura:GetCasterGuid()
aura:GetCasterGUIDLow()
aura:GetCasterGuidLow()
aura:GetCasterLevel()
aura:GetDuration()
aura:GetMaxDuration()
aura:GetAuraId()
aura:GetAuraID()
aura:GetId()
aura:GetSpellId()
aura:GetStackAmount()
aura:GetOwner()
aura:SetDuration(duration)
aura:SetMaxDuration(duration)
aura:SetStackAmount(stackAmount)
aura:Remove()
```

说明：

- `GetCaster()` 返回施法单位，施法者不在世界里或已经不存在时返回 `nil`。
- `GetCasterGUID()` / `GetCasterGuid()` 返回 `ObjectGuid` 对象；需要旧式低位数字时用 `GetCasterGUIDLow()`。
- `GetOwner()` 返回当前拥有这个光环的单位。
- `SetDuration()` / `SetMaxDuration()` 的单位是毫秒，会同步核心的光环持续时间字段。
- `Remove()` 会移除这个光环实例；如果它是该法术 holder 上最后一个效果，会一并移除整个 holder。

## CreatureTemplate 方法

可以通过 `GetCreatureTemplate(entry)`、`GetCreatureInfo(entry)` 或 `creature:GetTemplate()` 取得生物模板对象。
这是 `creature_template` / `CreatureInfo` 的只读数据，不是地图上已经刷出的动态生物实例。

```lua
template:GetId()
template:GetEntry()
template:GetName()
template:GetSubName()
template:GetSubname()
template:GetDisplayId(index)
template:GetDisplayID(index)
template:GetDisplayIdCount()
template:GetMountDisplayId()
template:GetGossipMenuId()
template:GetMinLevel()
template:GetMaxLevel()
template:GetMinHealth()
template:GetMaxHealth()
template:GetMinMana()
template:GetMaxMana()
template:GetArmor()
template:GetFaction()
template:GetFactionTemplate()
template:GetNpcFlags()
template:GetNPCFlags()
template:GetWalkSpeed()
template:GetRunSpeed()
template:GetScale()
template:GetDetectionRange()
template:GetCallForHelpRange()
template:GetLeashRange()
template:GetRank()
template:GetXpMultiplier()
template:GetXPMultiplier()
template:GetMinDamage()
template:GetMaxDamage()
template:GetDamageSchool()
template:GetAttackPower()
template:GetDamageMultiplier()
template:GetBaseAttackTime()
template:GetRangedAttackTime()
template:GetUnitClass()
template:GetUnitFlags()
template:GetDynamicFlags()
template:GetFamily()
template:GetCreatureFamily()
template:GetTrainerType()
template:GetTrainerSpell()
template:GetTrainerClass()
template:GetTrainerRace()
template:GetRangedMinDamage()
template:GetRangedMaxDamage()
template:GetRangedAttackPower()
template:GetType()
template:GetCreatureType()
template:GetTypeFlags()
template:GetCreatureTypeFlags()
template:GetLootId()
template:GetPickpocketLootId()
template:GetSkinningLootId()
template:GetResistance(school)
template:GetHolyResistance()
template:GetFireResistance()
template:GetNatureResistance()
template:GetFrostResistance()
template:GetShadowResistance()
template:GetArcaneResistance()
template:GetSpellId(index)
template:GetSpellListId()
template:GetPetSpellListId()
template:GetSpawnSpellId()
template:GetAuraCount()
template:GetAuraId(index)
template:GetMinGold()
template:GetMaxGold()
template:GetGoldMin()
template:GetGoldMax()
template:GetAIName()
template:GetMovementType()
template:GetInhabitType()
template:IsCivilian()
template:IsRacialLeader()
template:GetRegenerationFlags()
template:GetEquipmentId()
template:GetTrainerId()
template:GetVendorId()
template:GetMechanicImmuneMask()
template:GetSchoolImmuneMask()
template:GetImmunityFlags()
template:GetFlagsExtra()
template:HasFlagExtra(flag)
template:GetPhaseQuestId()
template:GetScriptId()
template:IsTameable()
template:GetObjectGuid(lowguid)
```

说明：

- `CreatureTemplate` 的 `GetDisplayId(index)`、`GetSpellId(index)`、`GetAuraId(index)` 下标使用 Lua 风格的 `1` 开始。
- `GetDisplayId(index)` 的 `index` 范围是 `1` 到 `4`；`GetSpellId(index)` 的范围是 `1` 到 `4`。
- `GetAuraId(index)` 按模板 `auras` 字段实际数量读取，使用前可以先调用 `GetAuraCount()`。
- `GetResistance(school)` 的 school 使用核心 `SPELL_SCHOOL_*` 数值：`0` 是护甲，`1` 到 `6` 分别是神圣、火焰、自然、冰霜、暗影、奥术。
- `GetObjectGuid(lowguid)` 用当前模板 entry 和传入的低位 GUID 构造 `ObjectGuid`，常用于和地图 GUID 反查函数配合。

## GameObject 方法

```lua
gameobject:GetName()
gameobject:GetDBTableGUIDLow()
gameobject:GetTemplate()
gameobject:GetGameObjectTemplate()
gameobject:GetGameObjectInfo()
gameobject:GetGOTemplate()
gameobject:GetGOInfo()
gameobject:GetGoType()
gameobject:GetGoState()
gameobject:SetGoState(state)
gameobject:GetLootState()
gameobject:GetLootRecipient()
gameobject:GetLootRecipientGroup()
gameobject:SetLootState(state)
gameobject:AddLoot(itemEntry, amount, ...)
gameobject:GetDisplayId()
gameobject:SetDisplayId(displayId)
gameobject:GetRespawnDelay()
gameobject:SetRespawnDelay(delay)
gameobject:SetRespawnTime(delay)
gameobject:IsSpawned()
gameobject:HasQuest(questId)
gameobject:IsTransport()
gameobject:IsActive()
gameobject:IsDestructible()
gameobject:IsVisible()
gameobject:SetVisible(visible)
gameobject:GetOwner()
gameobject:Use(user)
gameobject:UseDoorOrButton(delay)
gameobject:Respawn()
gameobject:Despawn()
gameobject:Refresh()
gameobject:SaveToDB()
gameobject:RemoveFromWorld(deleteFromDB)
```

说明：

- `gameobject:GetTemplate()` / `gameobject:GetGameObjectInfo()` / `gameobject:GetGOInfo()` 返回只读 `GameObjectTemplate` 模板对象。
- `GetLootRecipient()` / `GetLootRecipientGroup()` 当前先返回 `nil`。Turtle 1.12 的 GameObject 只公开允许拾取判断，没有 3.3.5 Eluna 那种直接取原始拾取玩家/队伍的公开接口。
- `AddLoot(itemEntry, amount, ...)` 会把固定物品加入当前 GameObject 的临时 loot，返回值为兼容占位的 `0`。Turtle 这里不是立即创建真实背包物品实例，所以没有可返回的真实物品 GUID。
- `IsDestructible()` 当前返回 `false`，因为 Turtle 1.12 没有 3.3.5 的可破坏建筑 GameObject 系统。
- `RemoveFromWorld(deleteFromDB)` 会把对象从世界移除；`deleteFromDB=true` 时还会删除数据库记录。这个接口会让当前对象指针失效，脚本里调用后不要继续使用同一个 `gameobject` 变量。

## GameObjectTemplate 方法

可以通过 `GetGameObjectTemplate(entry)`、`GetGameObjectInfo(entry)`、`GetGOTemplate(entry)`、`GetGOInfo(entry)` 或 `gameobject:GetTemplate()` 取得 GameObject 模板对象。
这是 `gameobject_template` / `GameObjectInfo` 的只读数据，不是地图上已经刷出的动态 GameObject 实例。

```lua
template:GetId()
template:GetEntry()
template:GetType()
template:GetGoType()
template:GetGOType()
template:GetDisplayId()
template:GetDisplayID()
template:GetName()
template:GetFaction()
template:GetFactionTemplate()
template:GetFlags()
template:GetSize()
template:GetData(index)
template:GetRawData(index)
template:GetDataCount()
template:GetMinMoneyLoot()
template:GetMaxMoneyLoot()
template:GetMinGold()
template:GetMaxGold()
template:GetPhaseQuestId()
template:GetScriptId()
template:IsDespawnAtAction()
template:IsUsableMounted()
template:GetLockId()
template:GetDespawnPossibility()
template:CannotBeUsedUnderImmunity()
template:GetCharges()
template:GetCooldown()
template:GetLinkedGameObjectEntry()
template:GetLinkedTrapId()
template:GetAutoCloseTime()
template:GetLootId()
template:GetGossipMenuId()
template:IsLargeGameObject()
template:IsInfiniteGameObject()
template:IsServerOnly()
template:GetInteractionDistance()
template:GetEventScriptId()
template:GetDeactivateTime()
template:GetQuestId()
template:GetSpellId()
template:GetRadius()
template:GetLevel()
template:GetObjectGuid(lowguid)
```

说明：

- `GetData(index)` / `GetRawData(index)` 读取 `gameobject_template` 的 `data0` 到 `data23`，下标使用 Lua 风格的 `1` 到 `24`。
- 不同 GameObject 类型的专属字段可以先用 `GetType()` 判断，再按核心结构读取对应 `data` 下标。
- `GetQuestId()`、`GetSpellId()`、`GetRadius()`、`GetLevel()` 会按当前 GameObject 类型读取常见字段，类型不适用时返回 `0`。
- `GetObjectGuid(lowguid)` 用当前模板 entry 和传入的低位 GUID 构造 `ObjectGuid`，常用于和地图 GUID 反查函数配合。

## Item 方法

```lua
item:GetName()
item:GetEntry()
item:GetTemplate()
item:GetItemTemplate()
item:GetProto()
item:GetGUIDLow()
item:GetCount()
item:SetCount(count)
item:GetBagSlot()
item:GetSlot()
item:IsEquipped()
item:IsSoulBound()
item:IsBound()
item:IsAccountBound()
item:IsBoundAccountWide()
item:IsBoundByEnchant()
item:IsNotBoundToPlayer(player)
item:IsLocked()
item:IsBag()
item:IsNotEmptyBag()
item:IsBroken()
item:CanBeTraded()
item:IsInTrade()
item:IsInBag()
item:HasQuest(questId)
item:IsPotion()
item:IsConjuredConsumable()
item:IsCurrencyToken()
item:IsWeaponVellum()
item:IsArmorVellum()
item:IsRefundExpired()
item:GetItemLink()
item:GetOwnerGUID()
item:GetOwnerGuid()
item:GetOwner()
item:GetMaxStackCount()
item:GetEnchantmentId(slot)
item:GetSpellId(index)
item:GetSpellTrigger(index)
item:GetClass()
item:GetSubClass()
item:GetDisplayId()
item:GetQuality()
item:GetBuyCount()
item:GetBuyPrice()
item:GetSellPrice()
item:GetInventoryType()
item:GetAllowableClass()
item:GetAllowableRace()
item:GetItemLevel()
item:GetRequiredLevel()
item:GetStatsCount()
item:GetRandomProperty()
item:GetRandomPropertyId()
item:GetRandomSuffix()
item:GetItemSet()
item:GetBagSize()
item:SetOwner(player)
item:SetBinding(soulbound)
item:SetEnchantment(enchantId, slot)
item:ClearEnchantment(slot)
item:SaveToDB(direct)
```

说明：

- `item:GetTemplate()` / `item:GetItemTemplate()` / `item:GetProto()` 返回只读 `ItemTemplate` 模板对象。
- `GetSpellId(index)` / `GetSpellTrigger(index)` 的 `index` 使用核心物品模板下标 `0` 到 `4`。
- `GetEnchantmentId(slot)`、`SetEnchantment(enchantId, slot)`、`ClearEnchantment(slot)` 使用 Turtle/MaNGOS 的附魔槽位编号。
- `SetEnchantment` 和 `ClearEnchantment` 会返回 `true` / `false` 表示是否成功。
- `GetItemLink()` 按 1.12 客户端可识别的物品链接格式返回字符串。
- `CurrencyToken`、vellum 和 refund 是 3.3.5/WotLK 语义，Turtle 1.12 当前没有等价系统，所以这些判断兼容返回 `false`。
- `GetRandomSuffix()` 当前返回 `0`；Turtle 1.12 物品模板主要使用 `RandomProperty`，没有 3.3.5 那套单独的随机后缀字段。

## ItemTemplate 方法

可以通过 `GetItemTemplate(itemId)`、`GetItemPrototype(itemId)` 或 `item:GetTemplate()` 取得物品模板对象。
这是 `item_template` / `ItemPrototype` 的只读数据，不是玩家背包里的动态物品实例。

```lua
template:GetId()
template:GetEntry()
template:GetItemId()
template:GetName()
template:GetDescription()
template:GetClass()
template:GetSubClass()
template:GetQuality()
template:GetDisplayId()
template:GetDisplayID()
template:GetFlags()
template:GetExtraFlags()
template:GetIcon()
template:GetBuyCount()
template:GetBuyPrice()
template:GetSellPrice()
template:GetInventoryType()
template:GetAllowableClass()
template:GetAllowableRace()
template:GetItemLevel()
template:GetRequiredLevel()
template:GetRequiredSkill()
template:GetRequiredSkillRank()
template:GetRequiredSpell()
template:GetRequiredReputationFaction()
template:GetRequiredReputationRank()
template:GetMaxCount()
template:GetStackable()
template:GetMaxStackSize()
template:GetContainerSlots()
template:GetDelay()
template:GetAmmoType()
template:GetBlock()
template:GetArmor()
template:GetBonding()
template:GetStartQuest()
template:GetItemSet()
template:GetMaxDurability()
template:GetBagFamily()
template:GetFoodType()
template:GetOtherTeamEntry()
template:GetStatsCount()
template:GetStatType(index)
template:GetStatValue(index)
template:GetSpellId(index)
template:GetSpellID(index)
template:GetSpellTrigger(index)
template:GetSpellCharges(index)
template:GetSpellCooldown(index)
template:GetSpellCategory(index)
template:GetSpellCategoryCooldown(index)
template:GetRecoveryTimeForSpell(spellId)
template:GetCategoryRecoveryTimeForSpell(spellId)
template:IsQuestItem()
template:IsPotion()
template:IsConjuredConsumable()
template:IsWeapon()
template:IsRangedWeapon()
template:IsOffHandItem()
template:HasSignature()
```

说明：

- `ItemTemplate` 的 `GetStat*()` 和 `GetSpell*()` 下标使用 Lua 风格的 `1` 到 `10` / `1` 到 `5`；`Item` 实例原有的 `item:GetSpellId(index)` 仍保持核心下标 `0` 到 `4`。
- `GetIcon()` 会读取核心启动时加载的 `ItemDisplayInfo.dbc`，按模板的 `DisplayInfoID` 返回客户端图标名；找不到模板或显示信息时返回空字符串 `""`。
- `GetItemTemplate()` 找不到模板时返回 `nil`。

## Quest 方法

```lua
quest:GetId()
quest:GetQuestId()
quest:GetTitle()
quest:GetLevel()
quest:GetQuestLevel()
quest:GetMinLevel()
quest:GetMaxLevel()
quest:GetFlags()
quest:GetQuestFlags()
quest:HasFlag(flag)
quest:IsRepeatable()
quest:IsDaily()
quest:GetNextQuestId()
quest:GetPrevQuestId()
quest:GetNextQuestInChain()
quest:GetType()
```

说明：

- `IsDaily()` 读取 Turtle 任务的每日特殊标记。
- `GetNextQuestId()`、`GetPrevQuestId()`、`GetNextQuestInChain()` 读取任务链字段，没配置时通常返回 `0`。

## SpellInfo 方法

可以通过 `GetSpellInfo(spellId)` 或 `GetSpellEntry(spellId)` 取得法术模板对象，找不到时返回 `nil`。
这是 DBC/核心里的只读法术数据，不是一次正在释放中的动态 `Spell` 对象。

法术效果下标使用核心的 `0、1、2`，不是 Lua 数组的 `1、2、3`。

常用方法：

```lua
spell:GetId()
spell:GetEntry()
spell:GetSpellId()
spell:GetName(locale)
spell:GetSpellName()
spell:GetRank(locale)
spell:GetSchool()
spell:GetSchoolMask()
spell:GetCategory()
spell:GetDispel()
spell:GetMechanic()
spell:GetAllMechanicMask()
spell:GetAttributes(index)
spell:HasAttribute(flag)
spell:HasAttribute(index, flag)
spell:GetAttributesEx()
spell:GetAttributesEx2()
spell:GetAttributesEx3()
spell:GetAttributesEx4()
spell:GetAttributesEx5()
spell:GetAttributesEx6()
spell:GetAttributesEx7()
spell:GetStances()
spell:GetStancesNot()
spell:GetTargets()
spell:GetTargetCreatureType()
spell:GetRequiresSpellFocus()
spell:GetFacingCasterFlags()
spell:GetCasterAuraState()
spell:GetTargetAuraState()
spell:GetCasterAuraStateNot()
spell:GetTargetAuraStateNot()
spell:GetCasterAuraSpell()
spell:GetTargetAuraSpell()
spell:GetExcludeCasterAuraSpell()
spell:GetExcludeTargetAuraSpell()
spell:GetCastingTimeIndex()
spell:GetRecoveryTime()
spell:GetRawRecoveryTime()
spell:GetCategoryRecoveryTime()
spell:GetStartRecoveryTime()
spell:GetStartRecoveryCategory()
spell:GetInterruptFlags()
spell:GetAuraInterruptFlags()
spell:GetChannelInterruptFlags()
spell:GetProcFlags()
spell:GetProcChance()
spell:GetProcCharges()
spell:GetMaxLevel()
spell:GetBaseLevel()
spell:GetSpellLevel()
spell:GetDurationIndex()
spell:GetDuration()
spell:GetPowerType()
spell:GetManaCost()
spell:GetManaCostPerlevel()
spell:GetManaPerSecond()
spell:GetManaPerSecondPerLevel()
spell:GetManaCostPercentage()
spell:GetRangeIndex()
spell:GetSpeed()
spell:GetStackAmount()
spell:GetSpellFamilyName()
spell:GetSpellFamilyFlags()
spell:GetMaxTargetLevel()
spell:GetMaxAffectedTargets()
spell:GetDmgClass()
spell:GetPreventionType()
spell:GetSpellIconID()
spell:GetSpellPriority()
spell:GetActiveIconID()
spell:GetSpellVisual()
spell:GetTotem(index)
spell:GetTotemCategory([index])
spell:GetReagent(index)
spell:GetReagentCount(index)
spell:GetEquippedItemClass()
spell:GetEquippedItemSubClassMask()
spell:GetEquippedItemInventoryTypeMask()
spell:GetAreaGroupId()
spell:GetRuneCostID()
```

效果相关方法：

```lua
spell:GetEffect(effectIndex)
spell:GetEffectDieSides(effectIndex)
spell:GetEffectRealPointsPerLevel([effectIndex])
spell:GetEffectBaseDice(effectIndex)
spell:GetEffectBasePoints(effectIndex)
spell:GetEffectMechanic(effectIndex)
spell:GetEffectImplicitTargetA(effectIndex)
spell:GetEffectImplicitTargetB(effectIndex)
spell:GetEffectRadiusIndex([effectIndex])
spell:GetEffectApplyAuraName(effectIndex)
spell:GetEffectAmplitude(effectIndex)
spell:GetEffectMultipleValue(effectIndex)
spell:GetEffectValueMultiplier([effectIndex])
spell:GetEffectChainTarget(effectIndex)
spell:GetEffectItemType(effectIndex)
spell:GetEffectMiscValue(effectIndex)
spell:GetEffectMiscValueB([effectIndex])
spell:GetEffectTriggerSpell(effectIndex)
spell:GetEffectPointsPerComboPoint(effectIndex)
spell:GetEffectSpellClassMask([effectIndex])
spell:GetDamageMultiplier(effectIndex)
spell:GetEffectDamageMultiplier([effectIndex])
spell:GetEffectBonusMultiplier([effectIndex])
spell:CalculateSimpleValue(effectIndex)
spell:HasAura(auraType)
spell:HasEffect(effectId)
spell:IsPassive()
spell:IsPositive()
spell:IsBinary()
spell:IsDispel()
spell:IsNonPeriodicDispel()
spell:IsCCSpell()
spell:IsCustomSpell()
spell:IsPvEHeartBeat()
```

3.3.5 兼容查询/判断方法：

```lua
spell:HasAreaAuraEffect()
spell:IsAffectingArea()
spell:IsExplicitDiscovery()
spell:IsLootCrafting()
spell:IsProfessionOrRiding()
spell:IsProfession()
spell:IsPrimaryProfession()
spell:IsPrimaryProfessionFirstRank()
spell:IsAbilityLearnedWithProfession()
spell:IsAbilityOfSkillType(skillType)
spell:IsTargetingArea()
spell:NeedsExplicitUnitTarget()
spell:NeedsToBeTriggeredByCaster(triggeringSpellInfo)
spell:IsSelfCast()
spell:IsAutocastable()
spell:IsStackableWithRanks()
spell:IsPassiveStackableWithRanks()
spell:IsMultiSlotAura()
spell:IsCooldownStartedOnEvent()
spell:IsDeathPersistent()
spell:IsRequiringDeadTarget()
spell:IsAllowingDeadTarget()
spell:CanBeUsedInCombat()
spell:IsPositiveEffect(effectIndex)
spell:IsChanneled()
spell:NeedsComboPoints()
spell:IsBreakingStealth()
spell:IsRangedWeaponSpell()
spell:IsAutoRepeatRangedSpell()
spell:IsAffectedBySpellMods()
spell:CanPierceImmuneAura(auraSpellInfo)
spell:CanDispelAura(auraSpellInfo)
spell:IsSingleTarget()
spell:IsAuraExclusiveBySpecificWith(otherSpellInfo)
spell:IsAuraExclusiveBySpecificPerCasterWith(otherSpellInfo)
spell:CheckShapeshift(form)
spell:CheckLocation(mapId, zoneId, areaId, player, strict)
spell:CheckTarget(caster, target, implicit)
spell:CheckExplicitTarget(caster, target, item)
spell:CheckTargetCreatureType(target)
spell:GetAllEffectsMechanicMask()
spell:GetEffectMechanicMask(effectIndex)
spell:GetSpellMechanicMaskByEffectMask(effectMask)
spell:GetDispelMask([dispelType])
spell:GetExplicitTargetMask()
spell:GetAuraState()
spell:GetSpellSpecific()
```

SpellInfo / SpellEntry 兼容说明：

- `SpellInfo` 的 3.3.5 参考方法名已经补齐；当前 `SpellInfoMethods.h` 差异脚本检查结果为 `ref=57 target=165 missing=0`。`target` 数量更高是因为本适配层还保留了 `GetEntry`、`GetSpellId`、大小写别名和旧 `SpellEntry` 兼容入口。
- 旧 `SpellEntryMethods.h` 的参考方法名也已经并入同一个法术模板对象；当前检查结果为 `ref=92 target=165 missing=0`。
- `GetSpellName()` 返回所有客户端语言的名称 table；`GetName(locale)` 仍按 SpellInfo 风格返回指定语言字符串。
- `GetEffectRealPointsPerLevel()`、`GetEffectRadiusIndex()`、`GetEffectValueMultiplier()`、`GetEffectDamageMultiplier()`、`GetEffectBonusMultiplier()` 这些旧 SpellEntry 风格方法在不传下标时返回 1 到 3 的 table，传入 `effectIndex` 时返回单个效果值。
- `GetAttributesEx5()`、`GetAttributesEx6()`、`GetAttributesEx7()`、`GetFacingCasterFlags()`、`GetCasterAuraStateNot()`、`GetTargetAuraStateNot()`、`GetCasterAuraSpell()`、`GetTargetAuraSpell()`、`GetExcludeCasterAuraSpell()`、`GetExcludeTargetAuraSpell()`、`GetEffectMiscValueB()`、`GetEffectSpellClassMask()`、`GetTotemCategory()`、`GetAreaGroupId()`、`GetRuneCostID()` 是 3.3.5/WotLK 字段，Turtle 1.12 没有对应字段，当前返回 `0` 或全 0 table。
- `CheckLocation()`、`CheckTarget()`、`CheckExplicitTarget()` 当前只做 Turtle 1.12 可直接判断的基础检查，返回核心 `SpellCastResult` 数值；没有完全复刻 3.3.5 的地图区域、条件、脚本目标和物品目标检查。
- `IsExplicitDiscovery()`、`NeedsToBeTriggeredByCaster()` 目前没有 Turtle 1.12 等价结构，保留方法名并兼容返回 `false`。
- `GetAuraState()` 先返回 `CasterAuraState`，没有时返回 `TargetAuraState`；1.12 没有 3.3.5 独立 AuraState 封装。
- `GetAllEffectsMechanicMask()` 当前映射到 Turtle 的 `GetAllSpellMechanicMask()`；会同时包含法术本体 mechanic 和效果 mechanic。
- `CanPierceImmuneAura()`、`IsAutocastable()`、`IsStackableWithRanks()`、`IsMultiSlotAura()` 使用 Turtle 现有字段近似判断，适合脚本兼容和调试，不能当成 3.3.5 全量规则判定。

## Spell 方法

`RegisterSpellEvent(spellId, eventId, function)` 的第三个参数，以及 `RegisterPlayerEvent(5, function(event, player, spell, skipCheck) end)` 里的 `spell` 参数，都是动态 `Spell` 对象，只保证在当前回调期间有效，不要保存到全局变量或定时器里长期使用。

```lua
spell:GetCaster()
spell:GetEntry()
spell:GetId()
spell:GetSpellId()
spell:GetSpellInfo()
spell:GetSpellEntry()
spell:GetCastTime()
spell:GetCastedTime()
spell:GetPowerCost()
spell:GetReagentCost()
spell:GetDuration()
spell:GetTargetDest()
spell:GetTargets()
spell:GetTarget()
spell:GetUnitTarget()
spell:GetGameObjectTarget()
spell:GetGOTarget()
spell:GetItemTarget()
spell:GetCastItem()
spell:IsTriggered()
spell:IsCastByItem()
spell:IsAutoRepeat()
spell:SetAutoRepeat(repeat)
spell:Cast([skipCheck])
spell:Cancel()
spell:Finish([ok])
```

说明：

- `GetSpellInfo()` / `GetSpellEntry()` 返回对应的只读 `SpellInfo` 模板对象。
- `GetTargets()` 返回 `SpellCastTargets` 对象，也只在当前回调期间有效。
- `GetDuration()` 返回法术模板配置的持续时间；如果脚本需要读取目标、坐标、物品等本次施法状态，优先使用动态 `Spell` 或 `SpellCastTargets` 对象。
- `GetReagentCost()` 返回一个 table，key 是只读 `ItemTemplate` 对象，value 是该材料数量；没有材料时返回空表。
- `GetTargetDest()` 在本次施法有目标坐标时返回 `x, y, z` 三个数字；没有目标坐标时返回 `nil, nil, nil`。
- `Cast()`、`Finish()`、`Cancel()`、`SetAutoRepeat()` 会改变当前施法流程，建议只在明确知道核心状态的施法事件里使用。
- `Cancel()` 会调用核心 `Spell::cancel()`；不要在取消事件里反复调用。

## SpellCastTargets 方法

`RegisterItemEvent(entry, 2, function(event, player, item, target) end)` 里的 `target`
现在是 `SpellCastTargets` 对象。它表示本次物品使用传入的施法目标。

```lua
target:GetTargetMask()
target:GetUnitTarget()
target:GetGameObjectTarget()
target:GetGOTarget()
target:GetItemTarget()
target:GetUnitTargetGUID()
target:GetUnitTargetGuid()
target:GetUnitTargetGUIDLow()
target:GetUnitTargetGuidLow()
target:GetGameObjectTargetGUID()
target:GetGameObjectTargetGuid()
target:GetGameObjectTargetGUIDLow()
target:GetGameObjectTargetGuidLow()
target:GetGOTargetGUID()
target:GetGOTargetGuid()
target:GetGOTargetGUIDLow()
target:GetGOTargetGuidLow()
target:GetItemTargetGUID()
target:GetItemTargetGuid()
target:GetItemTargetGUIDLow()
target:GetItemTargetGuidLow()
target:GetItemTargetEntry()
target:GetSource()       -- 返回 x, y, z
target:GetDestination()  -- 返回 x, y, z
target:GetStringTarget()
target:IsEmpty()
```

注意：`SpellCastTargets` 保存的是核心当前回调中的目标指针，Lua 脚本不要把它保存到定时器或全局变量里长期使用。
`Get*TargetGUID()` 返回 `ObjectGuid` 对象；需要旧式低位数字时使用对应的 `Get*TargetGUIDLow()`。

## Corpse 方法

`player:GetCorpse()` 会返回玩家当前尸体对象，没有尸体时返回 `nil`。`Corpse` 也继承常用 `Object` / `WorldObject` 方法，例如 `GetGUID()`、`GetGUIDLow()`、`GetMapId()`、`GetLocation()`、`GetDistance()`、`ToCorpse()` 等。

```lua
corpse:GetOwnerGUID()
corpse:GetOwnerGuid()
corpse:GetGhostTime()
corpse:GetType()
corpse:ResetGhostTime()
corpse:SaveToDB()
```

说明：

- `GetOwnerGUID()` 返回尸体所属玩家的 `ObjectGuid` 对象。
- `GetType()` 使用 Turtle 的 `CorpseType`：`0` 骨骸，`1` 可复活 PvE 尸体，`2` 可复活 PvP 尸体。
- `SaveToDB()` 对骨骸类型当前不执行保存，因为 Turtle 1.12 核心明确禁止保存骨骸，直接保存会触发断言。

## DynamicObject 方法

`DynamicObject` 是核心里用于暴风雪、奉献、光环云雾等动态法术区域的运行时对象。现在 `map:GetDynamicObject(guid)`、`map:GetWorldObject(guid)` 和通用 `WorldObject` 返回路径都可以返回它；它也继承常用 `Object` / `WorldObject` 方法，例如 `GetGUID()`、`GetGUIDLow()`、`GetMapId()`、`GetLocation()`、`GetDistance()`、`IsDynamicObject()`、`ToDynamicObject()` 等。

```lua
dyn:GetName()
dyn:GetCaster()
dyn:GetUnitCaster()
dyn:GetCasterGUID()
dyn:GetCasterGuid()
dyn:GetCasterGUIDLow()
dyn:GetCasterGuidLow()
dyn:GetSpellId()
dyn:GetSpellID()
dyn:GetEffIndex()
dyn:GetEffectIndex()
dyn:GetDuration()
dyn:GetRadius()
dyn:GetType()
dyn:Delay(ms)
dyn:Delete()
dyn:Remove()
```

说明：

- `GetCaster()` 返回创建动态对象的 `WorldObject`，可能是玩家、生物或其他世界对象；`GetUnitCaster()` 只在施法者是单位时返回 `Unit`，否则返回 `nil`。
- `GetSpellId()` / `GetSpellID()` 返回动态对象绑定的法术 ID，`GetEntry()` 也会返回同一个核心 entry 值。
- `Delay(ms)` 会延长或缩短动态对象剩余时间；`Delete()` / `Remove()` 会请求核心删除该动态对象，脚本不要继续长期保存已经删除的对象引用。

## Group 方法

`player:GetGroup()` 会返回玩家当前队伍对象，没有队伍时返回 `nil`。

```lua
group:GetId()
group:GetGUID()
group:GetGuid()
group:GetMembersCount()
group:GetMemberCount()
group:IsRaid()
group:IsRaidGroup()
group:IsFull()
group:IsBGGroup()
group:IsLFGGroup()
group:GetLeaderGUID()
group:GetLeaderGuid()
group:GetLeaderName()
group:GetLeader()
group:IsLeader(player)
group:IsLeader(guid)
group:IsMember(player)
group:IsMember(guid)
group:IsAssistant(playerOrGuid)
group:SameSubGroup(player1, player2)
group:HasFreeSlotSubGroup(subGroup)
group:GetMemberGroup(player)
group:GetMemberGroup(guid)
group:GetMemberGUID(name)
group:GetMemberGuid(name)
group:GetGroupType()
group:GetMembers()
group:SetLeader(playerOrGuid)
group:AddMember(player)
group:RemoveMember(playerOrGuid, method)
group:Disband()
group:ConvertToRaid()
group:SetMembersGroup(playerOrGuid, subGroup)
group:SetTargetIcon(icon, targetGuidOrObject)
group:SetMemberFlag(playerOrGuid, apply, flag)
group:SendPacket(packet)
```

说明：

- `group:GetMembers()` 返回在线玩家对象数组。
- `group:GetLeaderGUID()` / `group:GetMemberGUID(name)` 返回 `ObjectGuid` 对象，需要低位数字时调用 `guid:GetGUIDLow()`。
- `group:GetGUID()` 在 Turtle 1.12 中返回队伍数据库 ID 数字；1.12 核心没有 3.3.5 那种独立 Group GUID。
- `group:GetGroupType()` 按 Eluna 兼容值返回：普通 `0`，战场位 `1`，团队位 `2`，LFG 位 `8`，可能叠加。
- `group:SetMemberFlag()` 支持 `0x01` 助手开关；`0x02` 主坦克和 `0x04` 主助理当前只支持设置，不支持用 `apply=false` 清空。
- `group:SendPacket(packet)` 会把 `WorldPacket` 广播给队伍成员；可选参数仍按当前 C++ 入口支持 `ignorePlayersInBg` 和忽略 GUID。

## Roll 方法

`RegisterPlayerEvent(56, function(event, player, item, count, voteType, roll) end)` 里的 `roll`
现在是掷骰结果快照对象。它在 Lua 回调期间读取安全，脚本不要把它长期保存到全局变量或定时器里当作实时核心对象使用。

```lua
roll:GetItemGUID()
roll:GetItemGUIDLow()
roll:GetItemObjectGuid()
roll:GetItemId()
roll:GetItemEntry()
roll:GetItemRandomPropId()
roll:GetItemRandomSuffix()
roll:GetItemCount()
roll:GetLootedTargetGUID()
roll:GetLootSourceGUID()
roll:GetPlayerVote(playerGuidOrLowGuid)
roll:GetPlayerVoteGUIDs()
roll:GetTotalPlayersRolling()
roll:GetTotalNeed()
roll:GetTotalGreed()
roll:GetTotalPass()
roll:GetItemSlot()
roll:GetRollVoteMask()
```

说明：

- `GetItemGUID()` / `GetItemGUIDLow()` 返回获胜后实际进背包物品的低位 GUID 数字；需要 `ObjectGuid` 对象时用 `GetItemObjectGuid()`。
- `GetLootedTargetGUID()` / `GetLootSourceGUID()` 返回被拾取目标的 `ObjectGuid` 对象，通常是掉落该物品的 Creature。
- `GetPlayerVoteGUIDs()` 返回参与掷骰玩家的 GUID 表；再把其中某个 GUID 传给 `GetPlayerVote(guid)` 可得到该玩家的投票类型。
- Turtle 1.12 没有 3.3.5 的附魔分解掷骰按钮，所以 `GetRollVoteMask()` 当前按可用的 pass/need/greed 返回 `0x07`，`GetItemRandomSuffix()` 返回 `0`。

## Guild 方法

可以通过 `player:GetGuild()`、`GetGuildById(id)`、`GetGuildByName(name)` 取得公会对象。

```lua
guild:GetId()
guild:GetName()
guild:GetMOTD()
guild:GetMotd()
guild:GetInfo()
guild:GetLeaderGUID()
guild:GetLeaderGuid()
guild:GetLeader()
guild:GetMemberCount()
guild:GetMembersCount()
guild:GetMembers()
guild:GetRankName(rankId)
guild:GetRank(player)
guild:GetOnlineMembers()
guild:SendMessage(message)
guild:SendMessage(player, officerOnly, message, language)
guild:Broadcast(message)
guild:SetLeader(player)
guild:SetBankTabText(tabId, text)
guild:SendPacket(packet)
guild:SendPacketToRanked(packet, rankId)
guild:Disband()
guild:AddMember(player, rankId)
guild:DeleteMember(player, isDisbanding)
guild:SetMemberRank(player, rankId)
guild:SetName(name)
guild:UpdateMemberData(player, dataId, value)
guild:MassInviteToEvent(player, minLevel, maxLevel, minRank)
guild:SwapItems(player, tabId, slotId, destTabId, destSlotId, amount)
guild:SwapItemsWithInventory(player, toChar, tabId, slotId, bag, slot, amount)
guild:GetTotalBankMoney()
guild:GetCreatedDate()
guild:ResetTimes()
guild:ModifyBankMoney(amount, add)
```

说明：

- `guild:GetOnlineMembers()` 和 `guild:GetMembers()` 都返回当前在线的公会成员玩家对象数组。
- `guild:GetLeaderGUID()` 返回 `ObjectGuid` 对象，需要低位数字时调用 `guid:GetGUIDLow()`。
- `guild:SendMessage(message)` 会用通知形式发给在线成员；`guild:SendMessage(player, officerOnly, message, language)` 会按公会/官员聊天路径发送。
- `guild:SetBankTabText(tabId, text)` 在 Turtle 的自定义公会银行里映射为修改标签页名称；`tabId=0` 和 `tabId=1` 都按第一个标签页处理。
- `guild:GetCreatedDate()` 返回 `YYYYMMDD` 格式数字，例如 `20260506`。
- `guild:ModifyBankMoney(amount, add)` 直接更新角色库 `guild_bank_money`。如果公会银行插件对象已经在内存中打开，界面刷新可能要等银行重新加载；真实运营脚本里谨慎使用。
- `guild:SendPacket(packet)` 会向公会成员广播 `WorldPacket`；`guild:SendPacketToRanked(packet, rankId)` 会按公会 rank 发送。
- `guild:MassInviteToEvent()`、`guild:SwapItems()`、`guild:SwapItemsWithInventory()`、`guild:ResetTimes()` 当前保留方法名兼容，Turtle 1.12 这份核心没有对应的 3.3.5 原生流程。

## Channel 方法

`RegisterPlayerEvent(22, ...)` 的第 7 个参数会返回当前频道对象；第 6 个参数仍是 3.3.5 Eluna 风格的频道数字 ID。

```lua
channel:GetName()
channel:GetId()
channel:GetChannelId()
channel:GetChannelDBId()
channel:IsValid()
channel:IsConstant()
channel:IsAnnounce()
channel:SetAnnounce(enabled)
channel:IsLevelRestricted()
channel:IsLFG()
channel:GetPassword()
channel:SetPassword(password)
channel:GetNumPlayers()
channel:GetPlayerCount()
channel:GetFlags()
channel:HasFlag(flag)
channel:GetSecurityLevel()
channel:SetSecurityLevel(level)
channel:GetTeam()
channel:IsOn(playerOrGuid)
channel:IsBanned(playerOrGuid)
channel:GetPlayerFlags(playerOrGuid)
channel:SetModerator(playerOrGuid, enabled)
channel:SetMute(playerOrGuid, enabled)
channel:SendPacket(packet, ignoredPlayerOrGuid)
channel:SendToAll(packet, ignoredPlayerOrGuid)
channel:SendToOne(packet, playerOrGuid)
channel:Say(playerOrGuid, message, language, skipCheck)
channel:SendMessage(playerOrGuid, message, language, skipCheck)
```

说明：

- `tostring(channel)` 等同于 `channel:GetName()`，方便脚本临时把频道对象当频道名打印。
- `GetChannelId()` 返回 Turtle DBC 频道 ID；自定义频道通常是 `0`。Turtle 1.12 没有 3.3.5 的自定义频道数据库 ID，所以 `GetChannelDBId()` 当前作为兼容别名也返回这个值。
- `Channel` userdata 内部按频道名和阵营动态反查真实频道；如果频道已经被核心删除，`IsValid()` 返回 `false`，读取类接口返回 `0` / `false` / `nil`，修改和发送类接口不做操作。
- `IsOn()`、`IsBanned()`、`GetPlayerFlags()`、`SetModerator()`、`SetMute()` 可传 `Player`、`ObjectGuid` 或玩家低位 GUID；设置主持/禁言只会作用于已经在频道里的玩家。
- `SendPacket()` / `SendToAll()` 会向频道成员广播 `WorldPacket`；第二个参数可选，用来忽略某个玩家 GUID。`SendToOne()` 只发给指定玩家。
- `Say()` / `SendMessage()` 走核心频道发言路径；`skipCheck=true` 会跳过频道成员和禁言检查，脚本里谨慎使用。

## Map 方法

`player:GetMap()` 会返回玩家当前地图对象。

```lua
map:GetId()
map:GetMapId()
map:GetInstanceId()
map:GetName()
map:GetMapName()
map:IsDungeon()
map:IsRaid()
map:IsBattleground()
map:IsBattleGround()
map:IsArena()
map:IsContinent()
map:IsEmpty()
map:IsHeroic()
map:GetDifficulty()
map:GetHeight(x, y, z)
map:GetAreaId(x, y, z)
map:GetCreatures()
map:GetCreaturesByAreaId(areaId)
map:GetPlayers()
map:GetPlayerCount()
map:GetPlayersCount()
map:GetPlayer(guid)
map:GetCreature(guid)
map:GetAnyTypeCreature(guid)
map:GetGameObject(guid)
map:GetGO(guid)
map:GetDynamicObject(guid)
map:GetDynObject(guid)
map:GetUnit(guid)
map:GetWorldObject(guid)
map:GetWorldObjectOrPlayer(guid)
map:SetWeather(zoneId, weatherType, grade, permanent)
map:GetInstanceData()
map:SaveInstanceData()
```

`map:GetPlayers()` 返回当前地图在线玩家对象数组。
按 GUID 反查对象只查找当前已经加载在该地图里的对象；玩家可以用 `map:GetWorldObjectOrPlayer(guid)` 从当前地图或在线玩家列表里找。当前 Lua 可返回 `Player`、`Creature`、`GameObject`、`Corpse`、`DynamicObject` 这类对象；运输船等还没有专门 userdata，遇到这些类型会返回 `nil`。

Map 兼容说明：

- Turtle 1.12 没有 3.3.5 的英雄/竞技场副本难度系统，所以 `IsHeroic()`、`IsArena()` 当前返回 `false`，`GetDifficulty()` 返回 `0`。
- `GetHeight(x, y, z)` 读取当前地图地形高度，坐标无效或没有高度时返回 `nil`。
- `GetAreaId(x, y, z)` 读取当前坐标所在 Area，失败时返回 `0`。
- `GetCreatures()` / `GetCreaturesByAreaId(areaId)` 当前只枚举副本地图里已经加载的 Creature；普通大陆地图对象仓库没有公开枚举入口，所以会返回空表。
- `SetWeather(zoneId, weatherType, grade, permanent)` 会直接改变天气。`weatherType` 使用核心天气类型，常见值为 `0` 晴天、`1` 雨、`2` 雪、`3` 沙尘。
- `GetInstanceData()` 当前返回 Turtle 1.12 的副本脚本对象，不是 3.3.5 Eluna 的纯 Lua table；没有副本脚本时返回 `nil`。`SaveInstanceData()` 会在当前地图存在实例数据时保存。

## BattleGround 方法

`RegisterBGEvent` 回调里的第二个参数会返回当前战场对象。这个对象映射的是 Turtle/MaNGOS 核心 `BattleGround`，适合读取战场状态、人数、等级段、地图实例和胜负信息。

```lua
bg:GetName()
bg:GetAlivePlayersCountByTeam(team)
bg:GetMap()
bg:GetBonusHonorFromKillCount(kills)
bg:GetEndTime()
bg:GetFreeSlotsForTeam(team)
bg:GetInstanceId()
bg:GetInstanceID()
bg:GetMapId()
bg:GetMapID()
bg:GetTypeId()
bg:GetTypeID()
bg:GetBgTypeId()
bg:GetBGTypeId()
bg:GetMaxLevel()
bg:GetMinLevel()
bg:GetMaxPlayers()
bg:GetMinPlayers()
bg:GetMaxPlayersPerTeam()
bg:GetMinPlayersPerTeam()
bg:GetWinner()
bg:GetStatus()
bg:IsArena()
```

说明：

- `GetMap()` 返回当前战场所在的 `Map` 对象；战场实例还未挂接地图时返回 `nil`。
- `team` 参数使用核心队伍值，联盟/部落和当前服端 `Team` 枚举保持一致。
- `GetTypeId()` / `GetBgTypeId()` 在 Turtle 里都映射到核心 `GetTypeID()`，用于兼容不同脚本习惯。
- 战场销毁前事件里仍可以读取这些字段，但不要长期保存 `bg` 对象；回调结束后该核心对象可能马上释放。

## Ticket 方法

`RegisterTicketEvent` 回调里的 `ticket` 参数会返回当前工单对象。这个对象映射的是 Turtle/MaNGOS 核心 `GmTicket`，可以读取创建者、消息、分配状态，也可以修改分配、备注、回复和完成状态。

```lua
ticket:IsClosed()
ticket:IsCompleted()
ticket:IsFromPlayer(guidOrPlayer)
ticket:IsAssigned()
ticket:IsAssignedTo(guidOrPlayer)
ticket:IsAssignedNotTo(guidOrPlayer)
ticket:GetId()
ticket:GetID()
ticket:GetPlayer()
ticket:GetPlayerName()
ticket:GetMessage()
ticket:GetAssignedPlayer()
ticket:GetAssignedToGUID()
ticket:GetAssignedToGuid()
ticket:GetLastModifiedTime()
ticket:GetResponse()
ticket:GetChatLog()
ticket:SetAssignedTo(guidOrPlayer[, isAdmin])
ticket:SetResolvedBy(guidOrPlayer)
ticket:SetCompleted()
ticket:SetMessage(message)
ticket:SetComment(comment)
ticket:SetViewed()
ticket:SetUnassigned()
ticket:SetPosition(mapId, x, y, z)
ticket:AppendResponse(response)
ticket:DeleteResponse()
```

说明：

- `guidOrPlayer` 可以传 `ObjectGuid`、玩家对象或玩家低位 GUID 数字；传数字时按玩家 GUID 处理。
- 修改类方法会同步保存到 `characters` 数据库里的工单表。
- `SetResolvedBy()` 在 Turtle 里映射到设置关闭者 GUID；`SetCompleted()` 只标记工单已完成。两者都会触发 `RegisterTicketEvent(4, ...)`。
- `GetPlayer()` / `GetAssignedPlayer()` 只能返回当前在线玩家；离线时返回 `nil`。

## InstanceData 方法

`map:GetInstanceData()` 会返回当前地图的副本脚本对象，没有副本脚本时返回 `nil`。这个对象映射的是 Turtle/MaNGOS 核心 `InstanceData`，适合读取和修改副本脚本里暴露的数字状态。

```lua
local data = map:GetInstanceData()
if data then
    data:GetMap()
    data:GetMapId()
    data:GetInstanceId()
    data:IsEncounterInProgress()
    data:GetData(key)
    data:SetData(key, value)
    data:GetData64(key)
    data:SetData64(key, value)
    data:GetGuid(key)
    data:SetGuid(key, guid)
    data:Save()
    data:Load(savedString)
    data:SaveToDB()
end
```

说明：

- `GetData` / `SetData` 和 `GetData64` / `SetData64` 直接调用副本脚本实现；如果该副本脚本没有重写这些方法，核心默认返回 `0` 或不执行。
- `GetGuid(key)` 返回 `ObjectGuid` 对象；`SetGuid(key, guid)` 可以传 `ObjectGuid`、对象本身或玩家低位 GUID。
- `Save()` 返回副本脚本序列化字符串；`Load(savedString)` 会把字符串重新交给副本脚本解析，属于高级入口，普通脚本更建议只调用 `SaveToDB()`。

## 当前限制

这还不是 3.3.5 Eluna 的全量等价移植，但已经具备实际写服端 Lua 脚本的完整基础闭环：

- 脚本加载、重载和关闭事件。
- 玩家创建、删除、登录、登出、施法、击杀、被 Creature 击杀、决斗、经验/金币/声望变化、天赋变化、等级变化、聊天、命令、文字表情、保存、副本绑定、Zone/Area/地图变化、队伍掷骰获物、战场逃亡事件。
- 服务端启动、关闭、Update 事件。
- 地图创建、销毁、玩家进入/离开、地图 Update 事件，以及按 mapId / instanceId 绑定的副本生命周期事件。
- 战场创建、开始、结束、销毁前事件和 `BattleGround` 基础对象。
- 工单创建、玩家更新文本、关闭事件和 `Ticket` 基础对象。
- NPC 战斗、死亡、重生、AI Update。
- 按模板 entry 绑定的 Creature 事件，以及按 `ObjectGuid + instanceId` 绑定的唯一 Creature 事件。
- Creature / GameObject / Item 的任务、Gossip、使用入口。
- Item DummyEffect、Expire、Remove 事件。
- 动态 `Spell` 对象和 Prepare / Cast / Cancel 施法事件。
- `Quest` 对象。
- `ItemTemplate` 物品模板对象。
- `CreatureTemplate` 生物模板对象。
- `GameObjectTemplate` 模板对象。
- `SpellInfo` 法术模板对象。
- `SpellCastTargets` 物品使用目标对象。
- `ObjectGuid` GUID 值对象。
- `ChatHandler` 命令处理器基础对象。
- `Roll` 队伍掷骰结果快照对象。
- `InstanceData` 副本脚本基础对象。
- `Aura` 光环实例基础对象。
- 在线玩家列表、在线人数和已加载地图查询。
- 全局定时器和对象绑定定时器。
- 基础数据库查询和执行。
- Player / Creature / GameObject / Item 常用方法。
- Creature / GameObject 的 Add / Remove 生命周期事件。
- GameObject AIUpdate 事件。
- GameObject Spawn 事件。
- Creature / GameObject / Item DummyEffect 事件。

仍待继续补齐：

- `WorldPacket` 基础对象封装、客户端入包事件和服务端出包事件已接入；未知包事件 `6` 暂不触发，因为 Turtle 队列阶段会直接跳过无处理 opcode。
- `ObjectGuid` 已有值对象封装，并已支持 `Map` 按 GUID 反查 Player / Creature / GameObject / DynamicObject / Corpse / Unit / WorldObject，以及 `Player` 按 GUID 反查自己背包或银行里的物品。Creature / GameObject / DynamicObject / Corpse 不做全局离线查找，需要地图上下文。
- `Creature`、`Player`、`Corpse`、`DynamicObject`、`SpellInfo`、`Group`、`Guild`、`Map`、`BattleGround`、`Ticket`、动态 `Spell`、通用 `Object` 和通用 `WorldObject` 的 3.3.5 参考方法名已补齐或基础接入，不过部分接口按 Turtle 1.12 能力做兼容返回。
- `ItemTemplate` 的 3.3.5 参考方法名已补齐，其中 `GetIcon()` 已接入 `ItemDisplayInfo.dbc` 图标字段；找不到显示信息时才返回空字符串。
- 3.3.5 专属成就、竞技场点数、铭文/双天赋、LFG 和部分邮件/拍卖/银行/训练师细节目前仍是兼容返回或空入口，后续需要按 Turtle 1.12 的真实系统单独补强。
- 3.3.5 玩家事件里 `45` 成就完成和 `50` LFG 入队检查没有 Turtle 1.12 等价系统，当前不接入。
- 载具 Vehicle 对象和真实载具系统在 Turtle 1.12 中不存在，当前只有 `IsOnVehicle` / `GetVehicle` / `GetVehicleKit` 兼容空入口。
- 3.3.5 参考模块的全局公开函数当前已对齐；`RegisterEntryHelper`、`RegisterEventHelper`、`RegisterUniqueHelper`、`DBQueryAsync` 是 Eluna C++ 内部 helper，不是需要暴露给 Lua 脚本的公开 API。`WorldDBQueryAsync`、`CharDBQueryAsync` / `CharacterDBQueryAsync`、`AuthDBQueryAsync` / `LoginDBQueryAsync`、`HttpRequest` 已接入，callback 会回到 Lua 世界线程执行。

## 335 专属功能说明

以下 AzerothCore 3.3.5 功能不能直接照搬到 Turtle 1.12：

- 成就系统相关 API。
- 载具 Vehicle 真实系统和 Vehicle 对象封装。
- 竞技场 ArenaTeam 相关 API。
- 3.3.5 LFG 相关 API。
- WotLK 才有的部分 Spell / Aura / Map / Instance 结构。

这些需要按 Turtle 1.12 的实际结构重新设计或跳过。

## 最小测试脚本

```lua
print("Lua loaded")

RegisterPlayerEvent(3, function(event, player)
    player:SendBroadcastMessage("Lua login hook: " .. player:GetName())
end)

RegisterPlayerEvent(18, function(event, player, message)
    if message == "#lua" then
        player:SendBroadcastMessage("Lua is running on map " .. player:GetMapId())
        return false
    end
end)
```

测试方法：

1. 确认 `mangosd.conf` 中启用了 `Eluna.Enabled = 1`。
2. 把 Lua 文件放进 `E:\TurtleBY\lua_scripts`。
3. 启动 `mangosd.exe`。
4. 登录游戏并输入 `#lua`。

如果收到 Lua 回复，说明当前 Lua 层已经工作。
