#!/usr/bin/env python3
"""Screenshots the WASM build: headless Chrome + CDP with a real-time wait."""
import asyncio, base64, json, subprocess, sys, time, urllib.request

CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
URL = sys.argv[1] if len(sys.argv) > 1 else "http://localhost:8931/PetStory.html"
OUT = sys.argv[2] if len(sys.argv) > 2 else "wasm_cdp.png"
WAIT = float(sys.argv[3]) if len(sys.argv) > 3 else 25.0
PORT = 9333


async def main():
    proc = subprocess.Popen([CHROME, "--headless=new", f"--remote-debugging-port={PORT}",
                             "--window-size=760,1400", "--user-data-dir=/tmp/chrome-cdp-profile",
                             "about:blank"],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        import websockets
        for _ in range(50):
            try:
                ver = json.load(urllib.request.urlopen(f"http://localhost:{PORT}/json/version"))
                break
            except Exception:
                time.sleep(0.2)
        req = urllib.request.Request(f"http://localhost:{PORT}/json/new?{URL}", method="PUT")
        target = json.loads(urllib.request.urlopen(req).read())
        ws_url = target["webSocketDebuggerUrl"]
        async with websockets.connect(ws_url, max_size=64*1024*1024) as ws:
            mid = 0
            async def cmd(method, **params):
                nonlocal mid
                mid += 1
                await ws.send(json.dumps({"id": mid, "method": method, "params": params}))
                while True:
                    msg = json.loads(await ws.recv())
                    if msg.get("id") == mid:
                        return msg.get("result", {})
            await cmd("Page.enable")
            await asyncio.sleep(WAIT)  # real time: wasm compile + assets mount + scene load
            shot = await cmd("Page.captureScreenshot", format="png")
            open(OUT, "wb").write(base64.b64decode(shot["data"]))
            print("saved", OUT)
    finally:
        proc.terminate()


asyncio.run(main())
