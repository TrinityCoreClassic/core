#!/usr/bin/python3

import constants
import database as db

TRINITY_POOL_TYPE_CREATURE = 0
TRINITY_POOL_TYPE_GAMEOJECT = 1
TRINITY_POOL_TYPE_POOL = 2 

def Clear():
    remove_old()

def Import():
    handle_pool_templates()
    handle_pool_members()
    
def remove_old():
    db.tri_world.execute_raw("DELETE FROM pool_template")
    db.tri_world.execute_raw("DELETE FROM pool_members")
    
def handle_pool_templates():
    
    vm_rows = db.vm_world.get_rows_raw("SELECT entry, max_limit, description FROM pool_template WHERE patch_max = 10")
    
    for vm_row in vm_rows:
       existing = db.tri_world.get_row_raw("SELECT entry FROM pool_template WHERE entry = %s", (vm_row[0],))
       
       if existing == None:
            db.tri_world.execute_raw((
            "INSERT INTO pool_template (entry, max_limit, description) "
            "VALUES (%s, %s, %s)"
            ), (vm_row[0], vm_row[1], vm_row[2],))
       else: 
           db.tri_world.execute_raw(
               "UPDATE pool_template SET max_limit = %s, description=%s WHERE entry = %s",
               (vm_row[1], vm_row[2], vm_row[0],)
           )
           

    
def handle_pool_members():
    vm_member_pools = db.vm_world.select_all(
        db.SelectQuery("pool_pool")
    )
    
    for vm_mp in vm_member_pools:
        _upsert_pool_member(TRINITY_POOL_TYPE_POOL, vm_mp['pool_id'], vm_mp['mother_pool'], vm_mp['chance'], vm_mp['description'])
    
    #Creature Pools
    vm_creature_pools = db.vm_world.select_chunked(
        db.SelectQuery("pool_creature").where('patch_max', '=', 10).order_by("guid ASC, pool_entry ASC"),
        250
    )
    
    for vm_cp in vm_creature_pools:
        tri_guid = tri_closest('creature', vm_cp['guid'])
        if tri_guid:
            inline_pool = check_inline_pool(TRINITY_POOL_TYPE_CREATURE, tri_guid)
            if inline_pool:
                _upsert_pool_member(TRINITY_POOL_TYPE_POOL, inline_pool, vm_cp['pool_entry'], vm_cp['chance'], vm_cp['description'])
            else:
                _upsert_pool_member(TRINITY_POOL_TYPE_CREATURE, tri_guid, vm_cp['pool_entry'], vm_cp['chance'], vm_cp['description'])
    
    #GO Pools
    vm_go_pools = db.vm_world.select_chunked(
        db.SelectQuery("pool_gameobject").where('patch_max', '=', 10).order_by("guid ASC, pool_entry ASC"),
        250
    )
    
    for vm_gp in vm_go_pools:
        tri_guid = tri_closest('gameobject', vm_gp['guid'])
        if tri_guid:
            _upsert_pool_member(TRINITY_POOL_TYPE_GAMEOJECT, tri_guid, vm_gp['pool_entry'], vm_gp['chance'], vm_gp['description'])
                
                
    #Creature Template pools
    vm_creature_pools = db.vm_world.select_chunked(
        db.SelectQuery("pool_creature_template").where('patch_max', '=', 10).order_by("id ASC, pool_entry ASC"),
        250
    )
    
    for vm_ctp in vm_creature_pools:
        guids = db.tri_world.select_all(
            db.SelectQuery('creature').select('guid').where('id', '=', vm_ctp['id'])
        )
        for tri_guid in guids:
            inline_pool = check_inline_pool(TRINITY_POOL_TYPE_CREATURE, tri_guid['guid'])
            if inline_pool:
                _upsert_pool_member(TRINITY_POOL_TYPE_POOL, inline_pool, vm_ctp['pool_entry'], vm_ctp['chance'], vm_ctp['description'])
            else:
                _upsert_pool_member(TRINITY_POOL_TYPE_CREATURE, tri_guid['guid'], vm_ctp['pool_entry'], vm_ctp['chance'], vm_ctp['description'])
        
    
    #GO Template pools
    vm_go_pools = db.vm_world.select_chunked(
        db.SelectQuery("pool_gameobject_template").where('patch_max', '=', 10).order_by("id ASC, pool_entry ASC"),
        250
    )
    
    for vm_gtp in vm_go_pools:
        guids = db.tri_world.select_all(
            db.SelectQuery('gameobject').select('guid').where('id', '=', vm_gtp['id'])
        )
        for tri_guid in guids:
            _upsert_pool_member(TRINITY_POOL_TYPE_GAMEOJECT, tri_guid['guid'], vm_gtp['pool_entry'], vm_gtp['chance'], vm_gtp['description'])
    
            
def _upsert_pool_member(type, spawn_id, pool_spawn_id, chance, desc):   
    existing = db.tri_world.get_row_raw(
        "SELECT type FROM pool_members WHERE type = %s AND spawnId = %s", 
        (type, spawn_id,)
        )
    
    if existing == None:
        db.tri_world.upsert(
            db.UpsertQuery("pool_members").values({
                'type': type,
                'spawnId': spawn_id,
                'poolSpawnId': pool_spawn_id,
                'chance': chance,
                'description': desc
            })
        )
    else:
        db.tri_world.upsert(
            db.UpsertQuery("pool_members").values({
                'chance': chance,
                'description': desc,
                'poolSpawnId': pool_spawn_id
            }).where(
                db.GroupCondition("AND")
                    .condition("type", "=", type)
                    .condition("spawnId", "=", spawn_id)
            )
        )

def check_inline_pool(type, entry):
    tri_row = db.tri_world.select_one(
        db.SelectQuery('pool_members').where(
            db.GroupCondition('AND').condition('type', '=', type).condition('spawnId', '=', entry).condition('description', 'LIKE', '%Vmangos Inline%')
        )
    )
    
    if tri_row:
        return tri_row['poolSpawnId']

    return None

def tri_closest(table, vm_guid):
    vm_row = db.vm_world.select_one(
        db.SelectQuery(table).select("guid, id, map, position_x, position_y, position_z").where('guid', "=", vm_guid)
    )
    
    if vm_row:
        tri_row = db.tri_world.select_one(
            db.SelectQuery(table).select('guid').where(
                db.GroupCondition("AND").condition(
                    "id", "=", vm_row['id']
                ).condition(
                    "map", "=", vm_row['map']
                ).condition(
                    'position_x', 'BETWEEN', [vm_row['position_x'] - 0.001, vm_row['position_x'] + 0.001]
                ).condition(
                    'position_y', 'BETWEEN', [vm_row['position_y'] - 0.001, vm_row['position_y'] + 0.001]
                ).condition(
                    'position_z', 'BETWEEN', [vm_row['position_z'] - 0.001, vm_row['position_z'] + 0.001]
                )
            )
        )
        if tri_row:
            return tri_row['guid']
        
    return None