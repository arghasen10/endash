import http.server as httpserver
import argparse
import os
import json
import time
import signal
import math
import multiprocessing as mp
from util.nestedObject import getObj as getObj
from util import nestedObject
from util import cprint
from abr import BOLA
from abr import Pensieve
from abr import FastMPC
from abr import RobustMPC
import copy
# from abr import EnDash


ABRS = {"bola": BOLA, "pensieve": Pensieve, "fastMPC": FastMPC, "robustMPC": RobustMPC}
NUM_HISTORIES = 5

videoPlayer = None
states = {}
options = None

#http://10.5.20.107:8000/index2.html#dashed/bbb/media/pens.mpd&PENSIEVE


class MyHttpHandler(httpserver.SimpleHTTPRequestHandler):
    def do_GET(self):
        assert False

    def do_POST(self):
        l = int(self.headers.get("Content-Length", 0))
        if l == 0:
            print("Content len 0, returning")
            return #send error

        indata = self.rfile.read(l)
        req = getObj(json.loads(indata))
        data = b"REFRESH"
        print(req.getDict())
        if req.cookie is None:
            clientIp, clientPort = self.client_address
            clientIp = self.headers.get("X-Forwarded-For", clientIp)
            clientIp = clientIp.replace(":", "^")
            serverKey = "{}_{}_{}".format(time.time(), clientIp, clientPort)
            req.cookie = serverKey
            # state = prepareState()
        if req.cookie not in states:
            states.setdefault(req.cookie, {})[req.nextChunkId-1] = prepareState()
        if req.cookie in states:
            state = copy.deepcopy(states[req.cookie][req.nextChunkId-1])
            res = prepareNGetQuality(req, state)
            states.setdefault(req.cookie, {})[req.nextChunkId] = state
            data = {
                    "quality": res,
                    "cookie": req.cookie,
                    "sizes": getNextSizes(req.nextChunkId),
                    "lastSeg": videoPlayer.vidInfo.segmentCount - 1 == req.nextChunkId,
                    "segmentDuration": state.segmentDuration * 1000 #us
                }
            data = json.dumps(data).encode()

        self.send_response(200)
        self.send_header("Content-type", "text/plain")
        self.send_header("Content-Length", len(data))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("X-Cookie", req.cookie)
        self.end_headers()
        self.wfile.write(data)

def getChunkSize(ql, index):
    try:
        return videoPlayer.sizes.video[index][ql]
    except Exception:
        return 0

def getNextSizes(segId):
    return videoPlayer.sizes.video[segId]

def prepareNGetQuality(req, state):
#     print(req.getDict())
    segId = req.nextChunkId - 1
    timeSpent = req.lastChunkFinishTime - req.lastChunkStartTime
    chunkLen = req.lastChunkSize
    throughput = 1000 if timeSpent <= 0 else chunkLen*8*1000/timeSpent
    sobj = getObj({"qualityIndex": req.lastquality, "throughput": throughput, "segId" : segId, "chunkLen": chunkLen, "timeSpent": timeSpent,}) #need to change so that it can be squeeze
    state.bufferUpto = segId*state.segmentDuration
    avaliableBuf = round(req.buffer*1000, 3)
    state.playBackTime = (state.bufferUpto - avaliableBuf)
    if state.segmentDetails is None:
        state.segmentDetails = {}
    state.segmentDetails[segId] = sobj
    nextSegId = req.nextChunkId
    requests    = [state.segmentDetails[i] for i in range(nextSegId - NUM_HISTORIES, nextSegId) if i >= 0]
    stall = req.rebufferTime

    agent = getObj(
                requests                = requests, #place last 10 segs
                bufferUpto              = state.bufferUpto/1000,
                playBackTime            = state.playBackTime/1000,
                maxPlayerBufferLen      = state.maxBuf/1000,
                avaliableBuf            = (avaliableBuf)/1000,
                startingPlaybackTime    = 0,
                segCount                = nextSegId,
                getChunkSize            = getChunkSize,
                agentState              = state.agentState,
                rebufferTime            = stall,
                segDur                  = state.segmentDuration,
                stateless               = True,
        )
#     print(agent)
    abr     = ABRS[state.abr]
    if options.abr_log is not None:
        with open(options.abr_log, "a") as fp:
            print(nestedObject.getJson([state.vidInfo, agent]), file=fp)
    retObj  = abr.getNextQuality(state.vidInfo, agent, True)

    state.agentState = retObj.abrState
    if int(retObj.repId) >= len(state.vidInfo.bitrates):
        print("Error!!", retObj.errors)

    if options.log is not None:
        with open(options.log, "a") as fp:
            print(segId, req.rebufferTime/1000, req.lastquality, file=fp)

    return int(retObj.repId)

def prepareVideoPlayer(loc, vid):
    global videoPlayer
    with open(os.path.join(loc, vid + ".json")) as fp:
        data = json.load(fp)
        data['vidInfo']['segmentCount'] = min(len(data["sizes"]["video"]), math.ceil(data['vidInfo']['duration']/data['vidInfo']['segmentDuration']))
        videoPlayer = getObj(data)
    pass

def prepareState():
    return getObj(
            segmentDetails = None,
            bufferUpto = 0,
            playBackTime = 0,
            maxBuf = 30*1000,
            agentState = None,
            stall = 0,
            segmentDuration = videoPlayer.vidInfo.segmentDuration*1000,
            vidInfo = videoPlayer.vidInfo,
            abr = options.abr,
        )



def parseOptions():
    global options
    parser = argparse.ArgumentParser(description = "Viscous test with post")

    parser.add_argument('-p', '--port', dest="port", default=8333, type=int)
    parser.add_argument('-m', '--meta-dir', dest="meta_dir", type=str, required=True)
    parser.add_argument('-v', '--video', dest="vid", type=str, required=True)
    parser.add_argument('-a', '--abr', dest="abr", type=str, required=True)
    parser.add_argument('-l', '--log', dest="log", type=str, default=None)
    parser.add_argument('-L', '--abr-log', dest="abr_log", type=str, default=None)
    parser.add_argument('-n', '--notification', dest="notPipe", type=str, default=None)

    options = parser.parse_args()
    return options

def notifyInProc():
    if options.notPipe is None:
        return
    time.sleep(5)
    print("Waiting at notify")
    with open(options.notPipe, "w") as p:
        p.write("done")
        p.close()
        print("Done at notify")

def startAbrServers():
    for name, abrObj in ABRS.items():
        try:
            abrObj.startServerInFork(6)
            cprint.green(name, "server started")
        except Exception as e:
            cprint.red(name, "failed to started")

def sigHandler(signum, frame):
    exit(1)

def main():
    signal.signal(signal.SIGUSR1, sigHandler)
    parseOptions()
    prepareVideoPlayer(options.meta_dir, options.vid)
    if options.log is not None:
        fp = open(options.log, "w")
        fp.close()
#     prepareState(options)
#     print(videoPlayer.getDict())
    startAbrServers()
    with httpserver.HTTPServer(("", options.port), MyHttpHandler) as httpd:
        print("serving at port", options.port)
        p = mp.Process(target = notifyInProc)
        p.start()
        print("starting http server")
        httpd.serve_forever()
        p.join()
if __name__ == "__main__":
    main()
