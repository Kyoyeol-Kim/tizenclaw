#!/usr/bin/env python3
"""
TizenClaw Telegram Listener
Polls a Telegram Bot for messages and translates them into Tizen AppControl
launch requests directed to the TizenClaw service (org.tizen.tizenclaw).
"""

import urllib.request
import urllib.parse
import json
import socket
import sys
import time
import os

def send_prompt_to_tizenclaw(prompt_text):
    try:
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        # The abstract namespace socket starts with a null byte
        sock.connect("\0tizenclaw.ipc")
        sock.sendall(prompt_text.encode('utf-8'))
        sock.close()
        return True
    except OSError as e:
        print(f"Error connecting to TizenClaw IPC socket: {e}")
        return False

def poll_telegram_bot(token):
    offset = 0
    url = f"https://api.telegram.org/bot{token}"
    print(f"Starting Telegram polling for TizenClaw...")
    
    while True:
        try:
            req_url = f"{url}/getUpdates?offset={offset}&timeout=30"
            req = urllib.request.Request(req_url)
            with urllib.request.urlopen(req, timeout=40) as response:
                data = json.loads(response.read().decode())
                
                if data.get("ok"):
                    for result in data["result"]:
                        offset = result["update_id"] + 1
                        message = result.get("message", {})
                        text = message.get("text")
                        
                        if text:
                            print(f"Received Telegram Message: '{text}' -> Forwarding to AgentCore")
                            success = send_prompt_to_tizenclaw(text)
                            if not success:
                                print(f"Failed to forward message.")
        except urllib.error.URLError as e:
            print(f"Network error: {e}")
            time.sleep(5)
        except Exception as e:
            print(f"Exception during polling: {e}")
            time.sleep(5)

if __name__ == "__main__":
    import os, json
    claw_args = os.environ.get("CLAW_ARGS")
    if claw_args:
        try:
            parsed = json.loads(claw_args)
            for k, v in parsed.items():
                globals()[k] = v # crude but effective mapping for args
            
            # Simple wrapper mapping based on script name
            script_name = os.path.basename(__file__)
            if "launch_app" in script_name:
                launch_app(parsed.get("app_id", ""))
                sys.exit(0)
            elif "vibrate_device" in script_name:
                print(json.dumps(vibrate(parsed.get("duration_ms", 1000))))
                sys.exit(0)
            elif "schedule_alarm" in script_name:
                print(json.dumps(schedule_prompt(parsed.get("delay_sec", 600), parsed.get("prompt_text", ""))))
                sys.exit(0)
            elif "web_search" in script_name:
                print(json.dumps(search_wikipedia(parsed.get("query", ""))))
                sys.exit(0)
        except Exception as e:
            print(json.dumps({"error": f"Failed to parse CLAW_ARGS: {e}"}))

    bot_token = os.environ.get("TELEGRAM_BOT_TOKEN")
    if not bot_token:
        print("Please set TELEGRAM_BOT_TOKEN environment variable.")
        sys.exit(1)
        
    poll_telegram_bot(bot_token)
