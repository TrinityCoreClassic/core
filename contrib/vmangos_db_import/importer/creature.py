#!/usr/bin/python3

import constants
import database as db

next_entry_guid = 333000
pool_entry_offset = 1000000

imported_entry_guids = {}
cleaned_entry_guids = []

def Import():
    global next_entry_guid
    next_creature_row = db.tri_world.get_row_raw("SELECT MAX(guid) FROM creature")
    next_entry_guid = next_creature_row[0] + 1
    
    clean_templates_check_vmangos()
    clean_entries_check_vmangos()
    import_templates_vmangos()
    import_entries_vmangos()
    update_instance_info()



def clean_templates_check_vmangos():    
    db.tri_world.chunk_raw(
        "SELECT entry, name FROM creature_template LIMIT %s OFFSET %s",
        500,
        _handle_clean_template_row
    )

def _handle_clean_template_row(row):
    
    dest_creature_query = ("SELECT entry, name FROM creature_template "
                    "WHERE entry = %s "
                    "ORDER BY patch DESC LIMIT 1")
    
    delete_src_creature_entity_queries = [
        ("DELETE FROM creature_addon WHERE guid = %s"),
        ("DELETE FROM creature_formations WHERE leaderGUID = %s"),
        ("DELETE FROM creature_formations WHERE memberGUID = %s"),
        ("DELETE FROM creature WHERE guid = %s"),
    ]
    
    delete_src_queries = [
        ("DELETE FROM creature_equip_template WHERE CreatureID = %s"),
        ("DELETE FROM creature_loot_template WHERE Entry = %s"),
        ("DELETE FROM creature_onkill_reputation WHERE creature_id = %s"),
        ("DELETE FROM creature_questender WHERE id = %s"),
        ("DELETE FROM creature_questitem WHERE CreatureEntry = %s"),
        ("DELETE FROM creature_queststarter WHERE id = %s"),
        ("DELETE FROM creature_template_addon WHERE entry = %s"),
        ("DELETE FROM creature_template_locale WHERE entry = %s"),
        ("DELETE FROM creature_template_model WHERE CreatureID = %s"),
        ("DELETE FROM creature_template_movement WHERE CreatureId = %s"),
        ("DELETE FROM creature_template_resistance WHERE CreatureID = %s"),
        ("DELETE FROM creature_template_scaling WHERE Entry = %s"),
        ("DELETE FROM creature_template_spell WHERE CreatureID = %s"),
        ("DELETE FROM creature_text WHERE CreatureID = %s"),
        ("DELETE FROM creature_text_locale WHERE CreatureID = %s"),
        ("DELETE FROM creature_template WHERE entry = %s"),
    ]
    
    match = db.vm_world.get_rows_raw(dest_creature_query, (row[0],))
    
    if len(match) == 0:
        entities = db.tri_world.get_rows_raw("SELECT guid FROM creature WHERE id = %s", (row[0],))
        
        for entity in entities:
            db.tri_world.execute_many_raw(delete_src_creature_entity_queries, (entity[0],))      
                
        db.tri_world.execute_many_raw(delete_src_queries, (row[0],))
            
        return -1
    
    return 0

def clean_entries_check_vmangos(): 
    db.tri_world.chunk_raw(
        "SELECT guid, id, map, position_x, position_y, position_z FROM creature ORDER BY guid ASC LIMIT %s OFFSET %s",
        500,
        _handle_clean_entry_row
    )    
   
def _handle_clean_entry_row(row):
    
    delete_src_obj_creature_queries = [
        ("DELETE FROM creature_addon WHERE guid = %s"),
        ("DELETE FROM creature_formations WHERE leaderGUID = %s"),
        ("DELETE FROM creature_formations WHERE memberGUID = %s"),
        ("DELETE FROM creature WHERE guid = %s"),
    ]
    
    matches = db.vm_world.select_all(
        db.SelectQuery("creature").where(
            db.GroupCondition("AND").condition(
                db.GroupCondition('OR').condition(
                    'id', '=', row[1]
                ).condition(
                    'id2', '=', row[1]
                ).condition(
                    'id3', '=', row[1]
                ).condition(
                    'id4', '=', row[1]
                ).condition(
                    'id5', '=', row[1]
                )
            ).condition(
                'map', '=', row[2]
            ).condition(
                'position_x', 'BETWEEN', [row[3] - 0.001, row[3] + 0.001]
            ).condition(
                'position_y', 'BETWEEN', [row[4] - 0.001, row[4] + 0.001]
            ).condition(
                'position_z', 'BETWEEN', [row[5] - 0.001, row[5] + 0.001]
            )
        )
    )
    
    global cleaned_entry_guids
    
    for match in matches:
        if match['guid'] in cleaned_entry_guids:
            matches.remove(match)
    
    if len(matches) == 0:
        db.tri_world.execute_many_raw(delete_src_obj_creature_queries, (row[0],))                
        return -1
    else:
        cleaned_entry_guids.append(matches[0]['guid'])
    
    return 0
     
def import_templates_vmangos():
    vm_rows = db.vm_world.select_chunked(
        db.SelectQuery("creature_template").order_by("patch ASC"),
        500
    )
    
    for vm_row in vm_rows:
        existing_row = db.tri_world.select_one(
            db.SelectQuery("creature_template").select("entry, name").where("entry", "=", vm_row['entry'])
        )
        _upsert_creature_template(vm_row, existing_row)
                    

def import_entries_vmangos():
    global pool_entry_offset
    global imported_entry_guids
    
    db.tri_world.execute_raw("DELETE FROM game_event_creature")
    db.tri_world.execute_raw("DELETE FROM creature_equip_template") # TODO more intelligent method of loading/resetting equipment.
    
    # strict (attempt exact matches first time.)
    vm_entries = db.vm_world.select_chunked(
        db.SelectQuery("creature").select(
            'guid, id, id2, id3, id4, id5, map, position_x, position_y, position_z'
            ).where(
            'patch_max', '=', 10
            ),
        250
    )
        
    for vm_entry in vm_entries:
        inline_pooled_ids = [vm_entry['id']]
        for num in range(2, 5 + 1):
            key = 'id{}'.format(num)
            if vm_entry[key] > 0:
                inline_pooled_ids.append(vm_entry[key])
        
        for pooled_id in inline_pooled_ids:
            _handle_import_entry_row(vm_entry, pooled_id, -1)
    
    # complete
    vm_entries = db.vm_world.select_chunked(
        db.SelectQuery("creature").select(
            'guid, id, id2, id3, id4, id5, map, position_x, position_y, position_z'
            ).where(
            'patch_max', '=', 10
            ),
        250
    )
    
    for vm_entry in vm_entries:
        inline_pooled_ids = [vm_entry['id']]
        for num in range(2, 5 + 1):
            key = 'id{}'.format(num)
            if vm_entry[key] > 0:
                inline_pooled_ids.append(vm_entry[key])
        
        
        tc_pooled_guids = []
        pooled_index = 0
        for pooled_id in inline_pooled_ids:
            existing = None
            for imported in imported_entry_guids:
                if imported_entry_guids[imported][0] == vm_entry['guid'] and imported_entry_guids[imported][1] == pooled_id:
                    existing = imported
                    break
            
            if existing:
                tc_pooled_guids.append(existing)
            else:            
                tc_pooled_guids.append(
                    _handle_import_entry_row(vm_entry, pooled_id, 15 if pooled_index == 0 else 0.01)
                )
                
            pooled_index = pooled_index + 1
        
        if len(tc_pooled_guids) > 1:
            pool_name = 'Vmangos Inline Creature ({})'.format(vm_entry['guid'])
            pool_id = pool_entry_offset + vm_entry['guid']
            
            existing_pool_template = db.tri_world.select_one(
                    db.SelectQuery('pool_template').where('entry', '=', pool_id)
                )
            
            if existing_pool_template == None:
                db.tri_world.upsert(
                    db.UpsertQuery('pool_template').values({
                        'entry':pool_id,
                        'max_limit': 1,
                        'description': pool_name
                    })
                )
            
            for tc_spawn in tc_pooled_guids:
                #TODO there does appear to be an issue where, where the same GUID can occur in multiple pools
                existing_pool_spawn = db.tri_world.select_one(
                    db.SelectQuery('pool_members').where(
                        db.GroupCondition('AND').condition('type', '=', 0).condition('spawnId', '=', tc_spawn)#.condition('poolSpawnId', '=', pool_id)
                    )
                )
                
                if(existing_pool_spawn == None):                    
                    db.tri_world.upsert(
                        db.UpsertQuery('pool_members').values({
                            'type': 0,
                            'spawnId': tc_spawn,
                            'poolSpawnId': pool_id,
                            'chance': 0,
                            'description': 'Vmangos Inline'
                        })
                    )
                
    
    import_creature_models()

def _handle_import_entry_row(row, id, diff):
    global imported_entry_guids
        
    exact_match = db.tri_world.get_row_raw("SELECT guid, id FROM creature WHERE guid = %s AND id = %s", (row['guid'], id,))
    if exact_match != None:
        return _upsert_creature_entry(row['guid'], id, exact_match[0])
    
    guid_missing = db.tri_world.get_row_raw("SELECT guid, id FROM creature WHERE guid = %s", (row['guid'],))
    if guid_missing == None:
        return _upsert_creature_entry(row['guid'], id, 0 - row['guid'])
    
    if diff == -1:
        return None
    
    cond = db.GroupCondition('AND').condition(
        'id', '=', id
    ).condition(
        'map', '=', row['map']
    ).condition(
        'position_x', 'BETWEEN', [ row['position_x'] - diff, row['position_x'] + diff]
    ).condition(
        'position_y', 'BETWEEN', [ row['position_y'] - diff, row['position_y'] + diff]
    )
    
    matches = db.tri_world.select_all(
        db.SelectQuery("creature").select('guid, id, map, position_x, position_y, position_z').where(cond)
    )

    for match in matches:
        if match['guid'] in imported_entry_guids:
            matches.remove(match)
        
    if(len(matches) > 0):
        nearest_match = matches[0]
        nearest_abs = abs(nearest_match['position_x'] - row['position_x']) + abs(nearest_match['position_y'] - row['position_y'])

        for near_match in matches:
            next_abs = abs(near_match['position_x'] - row['position_x']) + abs(near_match['position_y'] - row['position_y'])
            if(next_abs < nearest_abs):
                nearest_match = near_match
                nearest_abs = next_abs
    
        return _upsert_creature_entry(row['guid'], id, nearest_match['guid'])  
    
    return _upsert_creature_entry(row['guid'], id)

def update_instance_info():
    for instance_id in constants.NormalMaps:
        db.tri_world.upsert(
            db.UpsertQuery("creature").values({
                'spawnDifficulties': "0",
                "VerifiedBuild": constants.TargetBuild
            }).where("map", "=", instance_id)
        )
        
    for instance_id in constants.DungeonMaps:
        db.tri_world.upsert(
            db.UpsertQuery("creature").values({
                'spawnDifficulties': "1",
                "VerifiedBuild": constants.TargetBuild
            }).where("map", "=", instance_id)
        )
        
    for instance_id in constants.Raid20Maps:
        db.tri_world.upsert(
            db.UpsertQuery("creature").values({
                'spawnDifficulties': "148",
                "VerifiedBuild": constants.TargetBuild
            }).where("map", "=", instance_id)
        )

    for instance_id in constants.Raid40Maps:
        db.tri_world.upsert(
            db.UpsertQuery("creature").values({
                'spawnDifficulties': "9",
                "VerifiedBuild": constants.TargetBuild
            }).where("map", "=", instance_id)
        )
    

def _upsert_creature_template(vm_row, tri_row = None) :
    
    #TODO check missing fields, both TC and Vmangos.
    
    type_flags = 0  #TODO check all flags.

    if vm_row['static_flags1'] & 0x00000010:
        type_flags = type_flags | 0x00000001       #CREATURE_TYPE_FLAG_TAMEABLE
    
    if vm_row['static_flags1'] & 0x00200000:
        type_flags = type_flags | 0x00000002       #CREATURE_TYPE_FLAG_VISIBLE_TO_GHOSTS
        
    if vm_row['static_flags1'] & 0x00010000:
        type_flags = type_flags | 0x00000004       #CREATURE_STATIC_FLAG_RAID_BOSS_MOB
        
    if vm_row['static_flags1'] & 0x00800000: 
        type_flags = type_flags | 0x00000008       #CREATURE_TYPE_FLAG_DO_NOT_PLAY_WOUND_ANIM
        
    if vm_row['static_flags1'] & 0x01000000: 
        type_flags = type_flags | 0x00000010       #CREATURE_TYPE_FLAG_NO_FACTION_TOOLTIP
        
    if vm_row['static_flags1'] & 0x40000000: 
        type_flags = type_flags | 0x00000020       #CREATURE_TYPE_FLAG_MORE_AUDIBLE
    
    if vm_row['gossip_menu_id'] > 0:
        type_flags = type_flags | 0x08000000       #force gossip
        
    
    creature_template_upsert = db.UpsertQuery("creature_template").values({
        'difficulty_entry_1': 0,
        'difficulty_entry_2': 0,
        'difficulty_entry_3': 0,
        'KillCredit1': 0,
        'KillCredit2': 0,
        'name': vm_row['name'],
        'femaleName': None,
        'subname': vm_row['subname'],
        'TitleAlt': None,
        'IconName': None,
        'gossip_menu_id': vm_row['gossip_menu_id'],
        'minlevel': vm_row['level_min'],
        'maxlevel': vm_row['level_max'],
        'HealthScalingExpansion': 0,
        'RequiredExpansion': 0,
        'VignetteID': 0,
        'faction': vm_row['faction'],
        'npcflag': constants.ConvertNPCFlags(vm_row['npc_flags']),
        'speed_walk': vm_row['speed_walk'],
        'speed_run': vm_row['speed_run'],
        'rank': vm_row['rank'],
        'dmgschool': vm_row['damage_school'],
        'BaseAttackTime': vm_row['base_attack_time'],
        'RangeAttackTime': vm_row['ranged_attack_time'],
        'BaseVariance': 1.0, #vm_row['damage_variance'],    // TC and VM variance works slightly differently, existing TC looks to always be 1.0f, use that for now.
        'RangeVariance': 1.0, #vm_row['damage_variance'],
        'unit_class': vm_row['unit_class'],
        #'unit_flags'
        #'unit_flags2'
        #'unit_flags3'
        #'dynamicflags'
        'family': vm_row['pet_family'],
        'trainer_class': vm_row['trainer_class'],
        'type':  vm_row['type'],
        'type_flags': type_flags,
        #'type_flags2'
        'lootid': vm_row['loot_id'],
        'pickpocketloot': vm_row['pickpocket_loot_id'],
        'skinloot': vm_row['skinning_loot_id'],
        'mingold': vm_row['gold_min'],
        'maxgold': vm_row['gold_max'],
        'MovementType': vm_row['movement_type'],
        'HealthModifier': vm_row['health_multiplier'],
        #'HealthModiferExtra'
        'ManaModifier': vm_row['mana_multiplier'],
        #'ManaModifierExtra'
        'ArmorModifier': vm_row['armor_multiplier'],
        'DamageModifier': vm_row['damage_multiplier'],
        'ExperienceModifier': vm_row['xp_multiplier'],
        'RacialLeader': vm_row['racial_leader'],
        #'movementId'
        #'CreatureDifficultyID'
        'RegenHealth': (vm_row['static_flags1'] & 0x00000400) != 1, #CREATURE_STATIC_FLAG_NO_AUTOMATIC_REGEN
        'Civilian': vm_row['civilian'],
        #'PetSpellDataId'
        #'mechanic_immune_mask': vm_row['mechanic_immune_mask'],
        #'spell_school_immune_mask': vm_row['spell_school_immune_mask'],
        #'flags_extra': vm_row['flags_extra'],
        'VerifiedBuild': constants.TargetBuild
    })
    
    if tri_row == None:
        creature_template_upsert.values({
            'entry': vm_row['entry'],
            'scale': 1,
            'VehicleId': 0,
            'HoverHeight': 1,
        })
    else:
        creature_template_upsert.where('entry', "=", tri_row['entry'])
        
    db.tri_world.upsert(creature_template_upsert)
    
    
    existing_tc_addon = db.tri_world.select_one(
        db.SelectQuery("creature_template_addon").where('entry', "=", tri_row['entry'])
    )
    
    if existing_tc_addon != None:
        #vmangos seems to assume these always set for creatures (SHEATH_STATE_MELEE, UNIT_BYTE2_FLAG_AURAS), see Creature::UpdateEntry
        addon_bytes_2 = (0x1 << 0) + (0x10 << 8) 
        addon_bytes_2 = addon_bytes_2 | existing_tc_addon['bytes2']
        creature_template_addon_upsert = db.UpsertQuery("creature_template_addon").values({
            'bytes2': addon_bytes_2
        }).where('entry', "=", tri_row['entry'])

        db.tri_world.upsert(creature_template_addon_upsert)
    
    db.tri_world.execute_raw("DELETE FROM creature_template_model WHERE CreatureID = %s", (vm_row['entry'],))
    
    display_index = 0
    for i in range(1, 4 + 1):
        if vm_row['display_id'+str(i)] > 0:
            db.tri_world.upsert(
                db.UpsertQuery("creature_template_model").values({
                    'CreatureID': vm_row['entry'],
                    'Idx': display_index,
                    'CreatureDisplayID': vm_row['display_id'+str(i)],
                    'DisplayScale': vm_row['display_scale'+str(i)],
                    'Probability': vm_row['display_probability'+str(i)],
                    'VerifiedBuild': constants.TargetBuild
                })
            )
            display_index += 1
            
    
    db.tri_world.execute_raw("DELETE FROM creature_template_resistance WHERE CreatureID = %s", (vm_row['entry'],))
    
    resist_fields = {
        'holy_res': 1,
        'fire_res': 2,
        'nature_res': 3,
        'frost_res': 4,
        'shadow_res': 5,
        'arcane_res': 6
    }
    
    for res_name, res_id in resist_fields.items():
        if(vm_row[res_name] > 0):
            db.tri_world.upsert(
                db.UpsertQuery("creature_template_resistance").values({
                    'CreatureID': vm_row['entry'],
                    'School': res_id,
                    'Resistance': vm_row[res_name],
                    'VerifiedBuild': constants.TargetBuild
                })
            )
    
    db.tri_world.execute_raw("DELETE FROM creature_template_spell WHERE CreatureID = %s", (vm_row['entry'],))
    
    spell_index = 0
    for i in range(1, 4+1):
        field_name = 'spell_id'+str(i)
        if(vm_row[field_name] > 0):
            db.tri_world.upsert(
                db.UpsertQuery("creature_template_spell").values({
                    'CreatureID': vm_row['entry'],
                    '`Index`': spell_index,
                    'Spell': vm_row[field_name],
                    'VerifiedBuild': constants.TargetBuild
                })
            )
            spell_index += 1

    
def _upsert_creature_entry(vm_ce_guid, vm_ce_id, tri_ce_guid = None):
    global next_entry_guid
    
    vm_row = db.vm_world.select_one(
        db.SelectQuery("creature").where("guid", "=", vm_ce_guid).order_by("patch_max DESC")
    )
    vm_addon_row = db.vm_world.select_one(
        db.SelectQuery("creature_addon").where('guid', "=", vm_ce_guid).order_by("patch DESC")
    )
    
    if vm_row == None:
        return
    
    if tri_ce_guid == None:
        tri_ce_guid = next_entry_guid
        next_entry_guid += 1
        db.tri_world.execute_raw("INSERT INTO creature (guid, id) VALUES (%s, %s)", (tri_ce_guid, vm_ce_id,))
    elif tri_ce_guid < 0:
        tri_ce_guid = abs(tri_ce_guid)
        db.tri_world.execute_raw("INSERT INTO creature (guid, id) VALUES (%s, %s)", (tri_ce_guid, vm_ce_id,))

    # handle equipment
    tri_equip_id = 0
    vm_equipment_id = 0
    try_vm_template = True
    if vm_addon_row:
        vm_equipment_id = vm_addon_row['equipment_id']
        try_vm_template = vm_equipment_id < 0
    
    if try_vm_template:
        vm_template = db.vm_world.select_one(
            db.SelectQuery("creature_template").select("entry, equipment_id").where("entry", "=", vm_ce_id)
        )
        
        if vm_template:
            vm_equipment_id = vm_template['equipment_id']
            
    if vm_equipment_id > 0:
        vm_equipment_row = db.vm_world.select_one(
            db.SelectQuery("creature_equip_template").where("entry", "=", vm_equipment_id).order_by("patch_max DESC, probability DESC")    #TODO handle multiple probable options, maybe related to TC equipment_id == -1 ?
        )
        
        if vm_equipment_row:
            existing_equip_row = db.tri_world.select_one(
                db.SelectQuery("creature_equip_template").where(
                    db.GroupCondition("AND").condition("CreatureID", "=", vm_row['id'])
                        .condition("ItemID1", "=", vm_equipment_row['item1'])
                        .condition("ItemID2", "=", vm_equipment_row['item2'])
                        .condition("ItemID3", "=", vm_equipment_row['item3'])
                )
            )
            
            if existing_equip_row:
                tri_equip_id = existing_equip_row['ID']
            else: 
                existing_equip_count = db.tri_world.get_row_raw("SELECT COUNT(ID) FROM creature_equip_template WHERE CreatureID = %s", (vm_row['id'],))
                next_equip_id = existing_equip_count[0] + 1
                tri_equip_id = next_equip_id
                db.tri_world.upsert(
                    db.UpsertQuery("creature_equip_template").values({
                        'CreatureID': vm_row['id'],
                        'ID': next_equip_id,
                        'ItemID1': vm_equipment_row['item1'],
                        'AppearanceModID1': 0,
                        'ItemVisual1': 0,
                        'ItemID2': vm_equipment_row['item2'],
                        'AppearanceModID2': 0,
                        'ItemVisual2': 0,
                        'ItemID3': vm_equipment_row['item3'],
                        'AppearanceModID3': 0,
                        'ItemVisual3': 0,
                        'VerifiedBuild': constants.TargetBuild
                    })
                )
        
    #TODO check all fields used.
    creature_upsert = db.UpsertQuery("creature").values({
        'id': vm_ce_id,
        'map': vm_row['map'],
        #...
        'phaseId': 0, #TODO check
        'modelid': vm_addon_row['display_id'] if vm_addon_row != None else 0,
        'equipment_id': tri_equip_id,
        'position_x': vm_row['position_x'],
        'position_y': vm_row['position_y'],
        'position_z': vm_row['position_z'],
        'orientation': vm_row['orientation'],
        'spawntimesecs': vm_row['spawntimesecsmin'],
        'wander_distance': vm_row['wander_distance'],
        #'curhealth': ...
        #'curmana': ...
        'MovementType': vm_row['movement_type'],
        #flags...
        'VerifiedBuild': constants.TargetBuild
    })


    global imported_entry_guids
    imported_entry_guids[tri_ce_guid] = [vm_ce_guid, vm_ce_id]
    
    creature_upsert.where("guid", "=", tri_ce_guid)  
    db.tri_world.upsert(creature_upsert)

    # check if is an event creature.
    vm_event_row = db.vm_world.select_one(
        db.SelectQuery("game_event_creature").where('guid', "=", vm_ce_guid)
    )
    
    if vm_event_row:
        existing_event_row = db.tri_world.select_one(
            db.SelectQuery("game_event_creature").where('guid', "=", tri_ce_guid)
        )
        
        if existing_event_row == None:
            db.tri_world.upsert(
                db.UpsertQuery("game_event_creature").values({
                    'eventEntry': vm_event_row['event'],
                    'guid': tri_ce_guid
                })
            )
            
    return tri_ce_guid
    
def import_creature_models():
    db.tri_world.execute_raw("DELETE FROM creature_model_info")
    
    vm_rows = db.vm_world.select_chunked(
        db.SelectQuery("creature_display_info_addon").order_by("build ASC"),
        500
    )
    
    for vm_row in vm_rows:
        existing_row = db.tri_world.select_one(
            db.SelectQuery("creature_model_info").where("DisplayID", "=", vm_row['display_id'])
        )
        
        upsert = db.UpsertQuery("creature_model_info").values({
            'BoundingRadius': vm_row['bounding_radius'],
            'CombatReach': vm_row['combat_reach'],
            'DisplayID_Other_Gender': vm_row['display_id_other_gender'],
            'VerifiedBuild': constants.TargetBuild
        })
        
        if existing_row == None:
            upsert.values({
                'DisplayID': vm_row['display_id']
            })
        else:
            upsert.where("DisplayID", "=", vm_row['display_id'])
            
        db.tri_world.upsert(upsert)