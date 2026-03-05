#!/usr/bin/env python3
"""
TizenClaw Skill: List Apps
Uses Tizen CAPI appfw app_manager to list installed applications, filtering out common system services.
"""
import ctypes
import sys
import json

def get_installed_apps():
    try:
        libappmanager = ctypes.CDLL("libcapi-appfw-app-manager.so.0")
    except OSError as e:
        return {"error": f"Failed to load libcapi-appfw-app-manager: {e}"}

    CMPFUNC = ctypes.CFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)
    app_manager_foreach_app_info = libappmanager.app_manager_foreach_app_info
    app_manager_foreach_app_info.argtypes = [CMPFUNC, ctypes.c_void_p]
    
    app_info_get_app_id = libappmanager.app_info_get_app_id
    app_info_get_app_id.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_char_p)]
    
    app_info_get_label = libappmanager.app_info_get_label
    app_info_get_label.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_char_p)]

    discovered_apps = []

    def app_info_cb(app_info, user_data):
        app_id_ptr = ctypes.c_char_p()
        label_ptr = ctypes.c_char_p()
        
        app_info_get_app_id(app_info, ctypes.byref(app_id_ptr))
        app_info_get_label(app_info, ctypes.byref(label_ptr))
        
        app_id = app_id_ptr.value.decode('utf-8') if app_id_ptr.value else ""
        label = label_ptr.value.decode('utf-8') if label_ptr.value else ""
        
        # Filter out common headless or hidden services
        ignored_keywords = ["service", "daemon", "system", "bootstrap", "setting", "engine"]
        is_hidden = False
        for kw in ignored_keywords:
            if kw in app_id.lower() or kw in label.lower():
                is_hidden = True
                break
                
        # Basic filtering to limit to UI apps that a user might want
        # Also limit the list size to avoid overwhelming context
        if not is_hidden and app_id.startswith("org.tizen") and len(discovered_apps) < 30:
            discovered_apps.append({"id": app_id, "label": label})
            
        return True

    cb = CMPFUNC(app_info_cb)
    ret = app_manager_foreach_app_info(cb, None)
    
    if ret != 0:
        return {"error": f"app_manager_foreach_app_info failed with code {ret}"}
        
    return {"apps": discovered_apps}

if __name__ == "__main__":
    import os, json
    # Support agent CLAW_ARGS if provided directly
    claw_args = os.environ.get("CLAW_ARGS")
    if claw_args:
        pass # we don't need parameters for list_apps

    result = get_installed_apps()
    print(json.dumps(result))
