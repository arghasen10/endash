import numpy as np
import math
import traceback as tb
from util.nestedObject import getObj

def getNextQualityBola(vidInfo, agent):
    V = 0.93
    lambdaP = 5 # 13
    sleepTime = 0
    if len(agent.requests) == 0:
        return 0, 0

    SM = float(vidInfo.bitrates[-1])
    vms = [math.log(sm/SM) for sm in vidInfo.bitrates]
    buflen = agent.bufferUpto - agent.playBackTime
    if (agent.maxPlayerBufferLen - vidInfo.segmentDuration) <= buflen:
        sleepTime = buflen + vidInfo.segmentDuration - agent.maxPlayerBufferLen

    p = vidInfo.segmentDuration
    lastM = agent.requests[-1].qualityIndex #last bitrateindex
    Q = buflen/p
    Qmax = agent.maxPlayerBufferLen/p
    ts = agent.playBackTime - agent.startingPlaybackTime
    te = vidInfo.duration - agent.playBackTime
    t = min(ts, te)
    tp = max(t/2, 3 * p)
    QDmax = min(Qmax, tp/p)

    VD = (QDmax - 1)/(vms[0] + lambdaP)

    M = np.argmax([((VD * vms[m] + VD*lambdaP - Q)/sm) \
            for m,sm in enumerate(vidInfo.bitrates)])
    print("Last throughput", agent.requests[-1].throughput, vidInfo.bitrates)
    if M < lastM:
        r = agent.requests[-1].throughput #throughput
        mp = min([m for m,sm in enumerate(vidInfo.bitrates) if sm/p < max(r, SM)] + [len(vidInfo.bitrates)])
        mp = 0 if mp >= len(vidInfo.bitrates) else mp
        if mp <= M:
            mp = M
        elif mp > lastM:
            mp = lastM
        elif False: #some special parameter TODO
            pass
        else:
            mp = mp - 1
        M = mp
    sleepTime = max(p * (Q - QDmax + 1), 0)
    return sleepTime, int(M)

def getNextQuality(vidInfo, agent, minimalState=False):
    sl, M = 0, -1
    errors = None
    try:
        sl, M =  getNextQualityBola(vidInfo, agent)
    except Exception as err:
        track = tb.format_exc()
        print(track)
        errors = track

    return getObj(sleepTime = sl, repId = M, abrState = None, errors = errors)
