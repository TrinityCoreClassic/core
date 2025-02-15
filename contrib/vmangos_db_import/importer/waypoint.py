#!/usr/bin/python3

import constants
import database as db

def Import():    
    pass
    #TODO
    # vm_waypoint_entries = db.vm_world.select_all(
    #     db.SelectQuery("script_waypoint").select("entry").group_by("entry")
    # )
    
    # vm_waypoint_entries = [e.get('entry') for e in vm_waypoint_entries]
    
    # for vm_waypoint_entry in vm_waypoint_entries:
    #     existing_entry = db.tri_world.select_one(
    #         db.SelectQuery("waypoints").where("entry","=", vm_waypoint_entry)
    #     )
        
    #     #be safe for now - dont adjust existing waypoints.
    #     if(existing_entry == None):
    #         vm_waypoints = db.vm_world.select_all(
    #             db.SelectQuery("script_waypoint").where('entry', '=', vm_waypoint_entry).order_by("pointId ASC")
    #         )
            
    #         for vm_waypoint in vm_waypoints:
    #             insert = db.UpsertQuery("waypoints").values({
    #                 'entry': vm_waypoint['entry'],
    #                 'pointId': vm_waypoint['pointid'],
    #                 'position_x': vm_waypoint['location_x'],
    #                 'position_y': vm_waypoint['location_y'],
    #                 'position_z': vm_waypoint['location_z'],
    #                 'orientation': None,
    #                 'delay': vm_waypoint['waittime'],
    #                 'point_comment': "(VM) " + vm_waypoint['point_comment']
    #             })
                
    #             db.tri_world.upsert(insert)