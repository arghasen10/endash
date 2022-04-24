from simenv.Simple import Simple
from util import load_trace
import util.videoInfo as video
from abr.BOLA import BOLA
from simulator.simulator import Simulator

import rnn.Quality as rnnQuality
import rnn.Buffer as rnnBuffer

import numpy as np
import pandas as pd
import glob
from simenv.PowerMeasure import PowerMeasurementEnv, PowerConsum
import json
import os

import abr.EnDash as EnDash

BYTES_IN_MB = 1000000.0

BUFFER_ACTION = [-2, -1, 0, +1, +2]

POW_ACT, POW_TAIL, POW_INACT = 396, 148, 0 #mili watt
TAIL_TIME = 6.2 #sec
RAMP_ENERGY = 158 #mili joule

BUFFER_INTERVAL=30

class LowPowerEnv(PowerMeasurementEnv):
    def __init__(self, traces, modelPath, energyModelPath, endashObj, *kw, **kws):
        super().__init__(traces=traces, *kw, **kws)

        self._vModelPath = modelPath
        self._vEndashObj = endashObj

        readOnly = False
        if 'LOW_POW_ENV_BUFFER_READ_ONLY' in os.environ:
            readOnly = int(os.environ['LOW_POW_ENV_BUFFER_READ_ONLY']) != 0
        #initiate buffer learner
        self._vPensieveBufferLearner = None if not self._vModelPath  else rnnBuffer.getPensiveLearner(BUFFER_ACTION, summary_dir = self._vModelPath, readOnly=readOnly)

        qualityActions = [i for i, _ in enumerate(self._vVideoInfo.bitrates)]
        #initiate Quality learner
        self._vPensieveQualityLearner = None if not self._vModelPath  else rnnQuality.getPensiveLearner(qualityActions, summary_dir = self._vModelPath, readOnly=True)


        self._vPreviousBufferAdjustmentActions = []
        self._vMaxBufLenOverTime = []

        self._vLastBufferAdjustInfo = None #rnnkey and time

        self._vExpectedPlayerBufferLen = self._vAgent._vMaxPlayerBufferLen

        self._vOldEnergyModel = None
        self._vPreditedThrpt = None

        if energyModelPath:
            self._vOldEnergyModel = PowerConsum(); self._vOldEnergyModel._rLoad(energyModelPath)

        self._rLoadPredictedThroughPut(traces)

        self._vBufferInterval = BUFFER_INTERVAL
        if 'LOW_POW_ENV_BUFFER_INTERVAL' in os.environ:
            self._vBufferInterval = int(os.environ['LOW_POW_ENV_BUFFER_INTERVAL'])


#=============================================
    def _rCalculateSleep(self):
        buflen = self._vAgent.bufferUpto - self._vAgent._vPlaybacktime
        if (self._vAgent._vMaxPlayerBufferLen - self._vVideoInfo.segmentDuration) > buflen:
            return 0
        sleepTime = buflen + self._vVideoInfo.segmentDuration - self._vAgent._vMaxPlayerBufferLen
        return sleepTime


#=============================================
    def _rLoadPredictedThroughPut(self, traces):
        fpath = traces[2]
        predictedPath = os.path.join("origTrace/", os.path.basename(fpath))
        self._vPreditedThrpt = pd.read_csv(predictedPath)
#         predictedPath = os.path.join("./predictThrpt/", os.path.basename(fpath))
#         with open(predictedPath) as fp:
#             pth = [[float(x) for x in y.strip().split()] for y in fp]
#             pth = list(zip(*pth))
#             tm, thr = pth
# #             tm = [tm[0]] + [x - tm[i-1] for i, x in enumerate(tm) if i > 0]
# #
# #             while len(tm) < 10000:
# #                 tm += tm[1:]
# #                 thr += thr[1:]
# #
# #             tm = np.cumsum(tm).tolist()
# #
# #             for i, x in enumerate(tm):
# #                 if i == 0: continue
# #                 assert x > tm[i-1]
#
#             assert len(tm) == len(thr)
#             self._vPreditedThrpt = [tm, thr, predictedPath]
#
# #         print(self._vPreditedThrpt)
# #         exit(0)

#=============================================
    def _rGetAveragePredictedThrougput(self, fromTime, toTime):
        assert fromTime <= toTime
        assert fromTime >= self._vBufferInterval

        startTime = round(fromTime - self._vBufferInterval)
        startIndex = (startTime - 1)%len(self._vPreditedThrpt)
        endIndex = (startIndex+self._vBufferInterval)%len(self._vPreditedThrpt)
        dt = None
        if startIndex > endIndex:
            dt = pd.concat([self._vPreditedThrpt[startIndex:], self._vPreditedThrpt[:endIndex]], ignore_index=True)
        else:
            dt = self._vPreditedThrpt[startIndex:endIndex]
        ret = self._vEndashObj.predict_throughput(dt)
        assert ret != None
        return ret
#         stTime = fromTime%self._vPreditedThrpt[0][-1]
#         startIndex = np.searchsorted(self._vPreditedThrpt[0], stTime, side="right")
#         dur = toTime - fromTime
#         assert dur == self._vBufferInterval
#
#         assert stTime >= self._vPreditedThrpt[0][startIndex-1]
#
# #         startIndex -= 1
#
#         durThr = [0,0]
#
#         count = 0
#         while True:
#             d = self._vPreditedThrpt[0][startIndex] - stTime
#             assert d > 0
#             t = self._vPreditedThrpt[1][startIndex]
#             if dur < d:
#                 d = dur
#                 durThr[0] += d
#                 durThr[1] += t * d
#                 break
#
#             stTime = self._vPreditedThrpt[0][startIndex]
#             startIndex += 1
#             if startIndex == len(self._vPreditedThrpt[0]):
#                 startIndex = 1
#                 stTime = 0
#             dur -= d
#             durThr[0] += d
#             durThr[1] += t * d
#             count += 1
#             assert count < 200
#
#         return durThr[1] / durThr[0]


#=============================================
    def _rGetNextQuality(self, nextSegId):
        if len(self._vAgent._vRequests) <= 0:
            return None, 0
        lastReq = self._vAgent._vRequests[-1]
        lastBitrate = self._vVideoInfo.bitrates[lastReq.qualityIndex] / self._vVideoInfo.bitrates[-1]
        bufferLeft = self._vAgent.bufferLeft / self._vVideoInfo.segmentDuration
        lastThroughput = self._vAgent._vRequests[-1].throughput / BYTES_IN_MB #TODO replace with predicted throughput
        lastTimeTaken = lastReq.timetaken #TODO replace with predicted time
        nextChunkSizes = [x[nextSegId]/BYTES_IN_MB for x in self._vVideoInfo.fileSizes]

        state = (lastBitrate, bufferLeft, lastThroughput, lastTimeTaken, nextChunkSizes)

        rnnkey = (self._vPeerId, nextSegId)
        ql = self._vPensieveQualityLearner.getNextAction(rnnkey, state)

        return rnnkey, ql


#=============================================
    def _rAdjustBuffer(self, nextSegId):
#         print(self._vPensieveBufferLearner, self._vPensieveBufferLearner._vInfoDim)
        if len(self._vAgent._vRequests) <= 0:
            return None, 0

        if self._vLastBufferAdjustInfo:
           rnnkey, lastTime = self._vLastBufferAdjustInfo
           energyConsum = self._rComputePowerUsageFrom(lastTime) / (self.now - lastTime)
           if self._vOldEnergyModel:
               energyConsumOld = self._vOldEnergyModel._rComputePowerUsageFrom(self.now, lastTime) / (self.now - lastTime)
               energyConsum = energyConsum - energyConsumOld
           self._vPensieveBufferLearner.addReward(rnnkey, energyConsum)
           self._vLastBufferAdjustInfo = None

        if nextSegId >= self._vVideoInfo.segmentCount:
            return

#         print("Energy gain:",req.downloadFinished - req.downloadStarted, self._rComputePowerUsageFrom(req.downloadStarted))

        lastReq = self._vAgent._vRequests[-1]
        lastBitrate = self._vVideoInfo.bitrates[lastReq.qualityIndex] / self._vVideoInfo.bitrates[-1]
        bufferLeft = self._vAgent.bufferLeft / self._vVideoInfo.segmentDuration
        lastThroughput = self._vAgent._vRequests[-1].throughput / BYTES_IN_MB #TODO replace with predicted throughput
        lastTimeTaken = lastReq.timetaken #TODO replace with predicted time
        nextChunkSizes = [x[nextSegId]/BYTES_IN_MB for x in self._vVideoInfo.fileSizes]

        predictThrpt = self._rGetAveragePredictedThrougput(self.now, self.now + self._vBufferInterval)

        state = (lastBitrate, bufferLeft, lastThroughput, lastTimeTaken, nextChunkSizes, predictThrpt)

        rnnkey = (self._vPeerId, nextSegId)

        adjustment = self._vPensieveBufferLearner.getNextAction(rnnkey, state)

        #TODO Do the adjustment
        sd = self._vVideoInfo.segmentDuration
        self._vExpectedPlayerBufferLen += adjustment*sd
        self._vExpectedPlayerBufferLen = max(self._vExpectedPlayerBufferLen, 2*sd)

        self._vLastBufferAdjustInfo = (rnnkey, self.now)

#=============================================
    def _rQoE(self, curBitrate, lastBitrate, stall):
        alpha = 1
        beta = 1
        gamma = 4.3

#         stall = stall/10 if stall < 8 else stall
        stall = 50 if stall > 2 else stall

        return alpha*curBitrate - beta*abs(curBitrate - lastBitrate) - gamma*stall

#=============================================
    def _rQoEAll(self):
        alpha = 1
        beta = 1
        gamma = 4.3
        qa, st = self._vAgent._vQualitiesPlayed, self._vAgent._vTotalStallTime
        qa = [self._vVideoInfo.bitrates[x]/BYTES_IN_MB for x in qa]
        if len(qa) == 0:
            return 1
        if len(qa) == 1:
            return alpha*qa[0]
        avQaVa = [abs(qa[x-1]-qa[x]) for x, _ in enumerate(qa) if x > 0]
        avQaVa = sum(avQaVa)
        avQa = sum(qa)

        stall = st/10 if st < 30 else st

        qoe = alpha*avQa - beta*avQaVa - gamma*stall
#         myprint("qoe =", qoe)
        return qoe

#=============================================
    #Should be over ridden if we need to create another env
    def _rDownloadNextData(self, nextSegId, nextQuality, sleepTime):
        if self._vDead: return
#         assert abs(self._vAgent._vStartedAt + self._vAgent.playbackTime + self._vAgent.totalStallTime - self.now) < 10

#         self._rAdjustBuffer(nextSegId)
#
        if self._vExpectedPlayerBufferLen != self._vAgent._vMaxPlayerBufferLen:
            self._vAgent._vMaxPlayerBufferLen = self._vExpectedPlayerBufferLen

        self._vMaxBufLenOverTime += [self._vAgent._vMaxPlayerBufferLen]
        sleepTime = self._rCalculateSleep()
        self.runAfter(sleepTime, self._rDownloadAfterSleep, nextSegId)

#=============================================
    def _rDownloadAfterSleep(self, nextSegId):
        rnnkey, nextQuality = self._rGetNextQuality(nextSegId)
        extraData = {}
        if rnnkey:
            extraData = {"rnnkey":rnnkey}
        self._rFetchSegment(nextSegId, nextQuality, 0, extraData=extraData)




#=============================================
    def _rAddToBuffer(self, req, simIds = None):
    #Should be over ridden if we need to create another env
        if self._vDead: return
        segId = req.segId
        lastStalls = self._vAgent._vTotalStallTime
        super()._rAddToBuffer(req, simIds)
        if req.extraData and "rnnkey" in req.extraData:
            rnnkey = req.extraData["rnnkey"]
            curStalls = self._vAgent._vTotalStallTime

            lastQl = self._vAgent._vRequests[-1].qualityIndex
            lastQl = self._vVideoInfo.bitrates[lastQl]/BYTES_IN_MB
            curQl = self._vVideoInfo.bitrates[req.qualityIndex]/BYTES_IN_MB

            qoe = self._rQoE(curQl, lastQl, abs(curStalls - lastStalls))
#             qoe = self._rQoEAll()

            self._vPensieveQualityLearner.addReward(rnnkey, qoe)


#=============================================
    def _rFinish(self):
        print("Quality Played", self._vAgent._vQualitiesPlayed)
#         print("Time to Energy", self._vTime2Energy, len(self._vTime2Energy))
#         print("MaxBufLenOverTime", self._vMaxBufLenOverTime)
#         print("Total energy", self._vTotalEnergy)
        super()._rFinish()

    def _rAdjustBufferInterval(self):
        if self._vDead or self._vFinished: return
        self._rAdjustBuffer(self._vAgent.nextSegmentIndex)
        self.runAfter(self._vBufferInterval, self._rAdjustBufferInterval)

#=============================================
    def start(self, startedAt = -1):
        super().start(startedAt)
        self.runAfter(self._vBufferInterval, self._rAdjustBufferInterval)





#=============================================
#=============================================
def experimentSimpleOne(abr=BOLA):
    traces = load_trace.load_trace()
    traces = list(zip(*traces))
    traces = {x[2]: x for x in traces}

    allPath = []
    with open("infos/info") as fp:
        allPath = json.load(fp)

    QoEs = []
    totalEnergies = []
    totalTimes = []
    totalStallTime = []
    videoDuration = []
    avgBitrate = []
    avgSmoothness = []

    endashObj = EnDash.AbrEnDashCls()
    endashObj.startServer()

#     for i, trce in enumerate(traces):
#         for j, f in enumerate(glob.glob("./videofilesizes/sizes_*.py")):
    for i, apRow in enumerate(allPath):
        for j, oldInfo in enumerate(apRow):
            f = oldInfo[1]
            trce = traces[oldInfo[0]]
            assert oldInfo[0] == trce[2]
            vi = video.loadVideoTime(f)
            simulator = Simulator()
            env = LowPowerEnv(vi=vi, traces=trce, simulator=simulator, abr=abr, modelPath="ResModel", energyModelPath=oldInfo[2], endashObj = endashObj)
            simulator.runAt(5, env.start, 2) # start, start func, global start
            simulator.run()
            QoEs += [env._vAgent.QoE]
            totalEnergies += [env._vPowerUsage._vTotalEnergy]
            totalTimes += [env.now]
            totalStallTime += [env._vAgent.totalStallTime]
            videoDuration += [env._vVideoInfo.duration]
            avgBitrate += [env._vAgent.avgBitrate]
            avgSmoothness += [env._vAgent.avgBitrateVariation]

#             break
#         break

    rnnQuality.saveLearner()
    rnnBuffer.saveLearner()
    endashObj.killProc()

    with open("PowerEnv.dat", "w") as fp:
        for x in zip(QoEs, totalEnergies, totalTimes, totalStallTime, videoDuration, avgBitrate, avgSmoothness):
            print(*x, file=fp)
        #for x,y,z,v,w,m,n in zip(QoEs, totalEnergies, totalTimes, totalStallTime, videoDuration, avgBitrate, avgSmoothness):
        #    print(x, y, z, v, w, m, n, file=fp)

def experimentSpecialOne(abr=BOLA):
    traces = load_trace.load_trace()
    traces = list(zip(*traces))
    trce = [x for x in traces if x[2].endswith("Day_8_Reading_1.csv")][0]
    vi = video.loadVideoTime("videofilesizes/sizes_0b4SVyP0IqI_648.py")
    simulator = Simulator()
    env = LowPowerEnv(vi, trce, simulator, abr, modelPath="ResModel", energyModelPath="/tmp/old")
    simulator.runAt(5, env.start, 2)
    simulator.run()
    print("totalEnergy:", env._vPowerUsage._vTotalEnergy)





#=============================================
def main():
    experimentSpecialOne()


if __name__ == "__main__":
    for x in range(1):
        main()
        print("=========================\n")
