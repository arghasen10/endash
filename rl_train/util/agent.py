import random
from util.myprint import myprint
import math
import numpy as np
from util.group import Group, GroupManager
from util.calculateMetric import measureQoE

PLAYBACK_DELAY_THRESHOLD = 4
M_IN_K = 1000

class SimpAbr():
    def __init__(self, *kw, **kws):
        pass
    def getNextDownloadTime(self, *kw, **kws):
        return 3, 2

class Agent():
    __count = 0
    def __init__(self, videoInfo, env, abrClass = None, logpath=None, resultpath=None):
        self._id = self.__count
        self.__count += 1
        self._vLogPath = logpath
        self._vResultPath = resultpath
        self._vEnv = env
        self._vVideoInfo = videoInfo
        self._vLastBitrateIndex = 0
        self._vCurrentBitrateIndex = 0
        self._vNextSegmentIndex = 0
        self._vPlaybacktime = 0.0
        self._vBufferUpto = 0
        self._vLastEventTime = 0
        self._vTotalStallTime = 0
        self._vStallsAt = []
        self._vStartUpDelay = 0.0
        self._vQualitiesPlayed = []
        self._vStartedAt = -1
        self._vGlobalStartedAt = -1
        self._vCanSkip = False #in case of live playback can be skipped
        self._vIsStarted = 0
        self._vMaxPlayerBufferLen = 50
        self._vTimeouts = []
        self._vRequests = [] # the rquest object
        self._vAbr = abr = None if not abrClass else abrClass(videoInfo, self, log_file_path=logpath)
        self._vSetQuality = abr.getNextDownloadTime if abr else self._rWhenToDownload
        self._vStartingPlaybackTime = 0
        self._vStartingSegId = 0
        self._vTotalUploaded = 0
        self._vTotalDownloaded = 0
        self._vFinished = False
        self._vPendingRequests = set()
        self._vDownloadPending = False
        self._vDead = False
        self._vBufferLenOverTime = [(0,0)]
        self._vQualitiesPlayedOverTime = []

        self._vFirstSegmentDlTime = 0
        self._vSegmentSkiped = 0
        self._vStartUpCallback = []
        self._vTimeSlipage = [(0,0,0)]
        self._vSyncSegment = -1
        self._vTotalPlayableTime = 0

        self._vSegIdPlaybackTime = {}


    @property
    def bufferUpto(self):
        return self._vBufferUpto

    @bufferUpto.setter
    def bufferUpto(self, value):
        assert self._vMaxPlayerBufferLen >= round(value - self._vPlaybacktime, 3)
        self._vBufferUpto = value

    @property
    def nextSegmentIndex(self):
        return self._vNextSegmentIndex

    @property
    def currentBitrateIndex(self):
        return self._vCurrentBitrateIndex

    @property
    def maxPlayerBufferLen(self):
        return self._vMaxPlayerBufferLen

    @property
    def playbackTime(self):
        now = self._vEnv.getNow()
        timeSpent = now - self._vLastEventTime

        playbackTime = self._vPlaybacktime + timeSpent
        if playbackTime > self.bufferUpto:
            return self.bufferUpto
        return round(playbackTime, 3)

    @property
    def bufferLeft(self):
        now = self._vEnv.getNow()
        timeSpent = now - self._vLastEventTime

        playbackTime = self._vPlaybacktime + timeSpent
        if playbackTime > self.bufferUpto:
            return 0.0
        return self.bufferUpto - playbackTime

    @property
    def stallTime(self):
        now = self._vEnv.getNow()
        timeSpent = now - self._vLastEventTime

        playbackTime = self._vPlaybacktime + timeSpent
        return max(0, playbackTime - self.bufferUpto)

#=============================================
    @property
    def avgQualityIndex(self):
        if len(self._vQualitiesPlayed) == 0: return 0

        bitratePlayed = self._vQualitiesPlayed
        return float(sum(bitratePlayed))/len(bitratePlayed)

#=============================================
    @property
    def bitratePlayed(self):
        return [self._vVideoInfo.bitrates[x] for x in self._vQualitiesPlayed]

#=============================================
    @property
    def avgQualityIndexVariation(self):
        if len(self._vQualitiesPlayed) == 0: return 0

        bitratePlayed = self._vQualitiesPlayed
        avgQualityVariation = [abs(bt - bitratePlayed[x - 1]) for x,bt in enumerate(bitratePlayed) if x > 0]
        avgQualityVariation = 0 if len(avgQualityVariation) == 0 else sum(avgQualityVariation)/float(len(avgQualityVariation))

        return avgQualityVariation

#=============================================
    @property
    def avgBitrate(self):
        if len(self._vQualitiesPlayed) == 0: return 0

        bitratePlayed = self._vQualitiesPlayed
        bitratePlayed = [self._vVideoInfo.bitrates[x] for x in self._vQualitiesPlayed]
        return float(sum(bitratePlayed))/len(bitratePlayed)

#=============================================
    @property
    def avgBitrateVariation(self):
        if len(self._vQualitiesPlayed) == 0: return 0

        bitratePlayed = self._vQualitiesPlayed
        bitratePlayed = [self._vVideoInfo.bitrates[x] for x in self._vQualitiesPlayed]
        avgQualityVariation = [abs(bt - bitratePlayed[x - 1]) for x,bt in enumerate(bitratePlayed) if x > 0]
        avgQualityVariation = 0 if len(avgQualityVariation) == 0 else sum(avgQualityVariation)/float(len(avgQualityVariation))

        return avgQualityVariation

#=============================================
    @property
    def startUpDelay(self):
        return self._vStartUpDelay

#=============================================
    @property
    def totalStallTime(self):
        return self._vTotalStallTime

#=============================================
    @property
    def avgStallTime(self):
        numSegs = len(self.bitratePlayed)
        return self.totalStallTime/numSegs
        
#=============================================
    @property
    def QoE(self):
        numSegs = len(self.bitratePlayed)
#         avgQualityVariation = [abs(bt - bitratePlayed[x - 1]) for x,bt in enumerate(bitratePlayed) if x > 0]
        return self.avgBitrate/1000000 - 4.3*self.totalStallTime/numSegs - self.avgBitrateVariation/1000000

#=============================================
    def addStartupCB(self, func):
        self._vStartUpCallback.append(func)

#=============================================
    def bufferAvailableIn(self):
        return max(0, self._vVideoInfo.segmentDuration - self._vMaxPlayerBufferLen + round(self.bufferLeft, 3))

#=============================================
    def _rNextQuality(self, req):
        if self._vDead: return

        assert req.segId == self._vNextSegmentIndex or (req.syncSeg and req.segId > self._vNextSegmentIndex)
        self._vRequests.append(req)

#=============================================
    def _rWhenToDownload(self, *kw):
        if self._vDead: return

        if len(self._vRequests) == 0:
            return 0, 0
        times, clens = list(zip(*[[req.timetaken, req.clen] for req in self._vRequests[:3]]))
        avg = sum(clens)*8/sum(times)
        level = 0
        for ql, q in enumerate(self._vVideoInfo.bitrates):
            if q > avg:
                break
            level = ql
#         self._vCurrentBitrateIndex = level
        buflen = self.bufferUpto - self._vPlaybacktime
        if (self._vMaxPlayerBufferLen - self._vVideoInfo.segmentDuration) > buflen:
            return 0, level
        sleepTime = buflen + self._vVideoInfo.segmentDuration - self._vMaxPlayerBufferLen
        return sleepTime, level

#=============================================
    def _rSyncNow(self):
        now = self._vEnv.getNow()
        curPlaybackTime = now - self._vGlobalStartedAt
        curSegId = int(curPlaybackTime + self._vVideoInfo.segmentDuration - 1)/self._vVideoInfo.segmentDuration
        curSegId = int(curSegId)
        delay = int(curPlaybackTime)
        if delay < self._vVideoInfo.segmentDuration * 0.8: #just a random thaught
            curSegId += 1

        if self._vNextSegmentIndex < curSegId:
            self._vNextSegmentIndex = curSegId
            self._vSyncSegment = curSegId
            self._rDownloadNextData(0)

#=============================================
    def _rStoreSegmentPlaybackTime(self, req):
        self._vTotalPlayableTime += req.segmentDuration
        segId = req
        curPlaybackTime = self._vPlaybacktime
        segPlaybackStartTime = req.segId*self._vVideoInfo.segmentDuration
        waitTime = segPlaybackStartTime - curPlaybackTime
        assert waitTime >= 0

        self._vSegIdPlaybackTime[req.segId] = (self._vEnv.now + waitTime, req)

#=============================================
    def _rAddToBufferInternal(self, req, simIds = None):
        if self._vDead: return

        self._rNextQuality(req)
        if self._vNextSegmentIndex <= req.segId and req.syncSeg:
            self._vNextSegmentIndex =  self._vSyncSegment = req.segId

        assert (req.segId * self._vVideoInfo.segmentDuration + self._vGlobalStartedAt - (self.bufferLeft * ( 1 - req.syncSeg)) <= self._vEnv.now)

        ql, timetaken, segDur, segId, clen = req.qualityIndex, req.timetaken, req.segmentDuration, req.segId, req.clen

        now = self._vEnv.getNow()
        segPlaybackStartTime = segId * self._vVideoInfo.segmentDuration
        segPlaybackEndTime = segPlaybackStartTime + segDur

        timeSpent = now - self._vLastEventTime
        self._vLastEventTime = now
        stallTime = 0
        playbackTime = self._vPlaybacktime + timeSpent
        if playbackTime > self.bufferUpto:
            stallTime = playbackTime - self.bufferUpto
            playbackTime = self.bufferUpto

        if not self._vIsStarted:
            startUpDelay = now - self._vStartedAt
            expectedPlaybackTime = startUpDelay
            stallTime = 0
            playbackTime = segPlaybackStartTime
            bufferUpto = segPlaybackEndTime
            if self._vGlobalStartedAt != self._vStartedAt:
                expectedPlaybackTime = now - self._vGlobalStartedAt

#             if  self._vCanSkip and expectedPlaybackTime - PLAYBACK_DELAY_THRESHOLD > segPlaybackEndTime:
#                 #need to skip this segment
#                 expectedSegId = int(expectedPlaybackTime / self._vVideoInfo.segmentDuration) + 1
#                 self._vSegmentSkiped += expectedSegId - self._vNextSegmentIndex
#                 self._vNextSegmentIndex = expectedSegId
#                 if self._vNextSegmentIndex >= self._vVideoInfo.segmentCount:
#                     self._vEnv.finishedAfter(1)
#                     return
#                 self._rDownloadNextData(0)
#                 return

            if expectedPlaybackTime < segPlaybackStartTime:
                after = segPlaybackStartTime - expectedPlaybackTime
#                 print("after:", after)
                self._vEnv.runAfter(after, self._rAddToBufferInternal, req, simIds)
                return

#             found = False
#             for x in range(PLAYBACK_DELAY_THRESHOLD + 1):
#                 if segPlaybackStartTime <= expectedPlaybackTime - x <= segPlaybackEndTime:
#                     playbackTime = expectedPlaybackTime - x
#                     found = True
#                     break
#
#             assert found

            self._vIsStarted = True
            self._vStartingPlaybackTime = playbackTime
            self._vStartingSegId = segId
            self._vFirstSegmentDlTime = timetaken
            self._vStartUpDelay = startUpDelay
            for cb in self._vStartUpCallback:
                cb(self)


        if stallTime > 0:
            assert playbackTime > 0
            self._vStallsAt.append((playbackTime, stallTime, ql, req.downloader==self._vEnv))
            self._vTimeSlipage.append((now - stallTime, self._vTotalStallTime, self._vNextSegmentIndex-1))
            self._vTotalStallTime += stallTime
            self._vTimeSlipage.append((now, self._vTotalStallTime, self._vNextSegmentIndex))
#             assert stallTime < (req.timetaken + 100)
            self._vBufferLenOverTime.append((now - stallTime, 0))
            self._vBufferLenOverTime.append((now - 0.001, 0))
        else:
            buflen = self.bufferUpto - playbackTime
            if buflen <= 0:
                buflen = 0
            self._vBufferLenOverTime.append((now - 0.001, buflen))

        if self._vSyncSegment == req.segId:
            skip = playbackTime - self._vPlaybacktime
#             self._vTotalStallTime += skip
            playbackTime = self._vBufferUpto = self._vVideoInfo.segmentDuration * req.segId


        self._vPlaybacktime = playbackTime
        self.bufferUpto = segPlaybackEndTime
        self._rStoreSegmentPlaybackTime(req)

        buflen = self.bufferUpto - self._vPlaybacktime
        self._vBufferLenOverTime.append((now, buflen))
        self._vQualitiesPlayed.append(ql)
        self._vQualitiesPlayedOverTime.append((now + buflen - segDur, ql, self.nextSegmentIndex))
        self._vNextSegmentIndex += 1
        if self._vNextSegmentIndex == len(self._vVideoInfo.fileSizes[0]):
            self._vEnv.finishedAfter(buflen)
            return
        self._vLastBitrateIndex = self._vCurrentBitrateIndex
        self._rDownloadNextData(buflen)

    def _rSkipToSegId(self, segId):
        self._vPlaybacktime = self._vBufferUpto = self._vVideoInfo.segmentDuration * segId
        self._vNextSegmentIndex = segId

#=============================================
    def _rDownloadNextData(self, buflen):
        if self._vDead: return

        now = self._vEnv.getNow()
        nextSegId = self._vNextSegmentIndex
        nextQuality = self._vCurrentBitrateIndex
        sleepTime, nextQuality = self._vSetQuality(self._vMaxPlayerBufferLen, \
            self.bufferUpto, self._vPlaybacktime, now, self._vNextSegmentIndex)
        self._vCurrentBitrateIndex = nextQuality
        self._vEnv._rDownloadNextData(nextSegId, nextQuality, sleepTime)

#=============================================
#     def _rTimeoutEvent(self, simIds, lastBandwidthPtr, sleepTime):
#         if self._vDead: return
#
#         if simIds != None and REQUESTION_SIMID_KEY in simIds:
#             self._vSimulator.cancelTask(simIds[REQUESTION_SIMID_KEY])
#
#         self._vLastBandwidthPtr = lastBandwidthPtr
#         self._vTimeouts.append((self._vNextSegmentIndex, self._vCurrentBitrateIndex))
#         self._vCurrentBitrateIndex = 0
#         self._rFetchSegment(self._vNextSegmentIndex, self._vCurrentBitrateIndex, sleepTime)



#=============================================
    def _rGetTimeOutTime(self):
        if self._vDead: return

        timeout = self._vVideoInfo.segmentDuration
        bufLeft = self.bufferUpto - self._vPlaybacktime
        if bufLeft - timeout > timeout:
            timeout = bufLeft - timeout
        return timeout

#=============================================
    def _rIsAvailable(self, segId):
        if self._vDead: return -1

        assert segId < self._vVideoInfo.segmentCount
        now = self._vEnv.getNow()
        ePlaybackTime = now - self._vGlobalStartedAt
        avlUpTo = self._vVideoInfo.globalDelayPlayback + ePlaybackTime
        segStartTime = (segId+1)*self._vVideoInfo.segmentDuration
        return segStartTime - avlUpTo
#=============================================
    def _rCalculateQoE(self):
        if self._vDead: return
        if self._vPlaybacktime == 0:
            return 0 #not sure. But I think it is better

        return measureQoE(self._vVideoInfo.bitrates, self._vQualitiesPlayed, self._vTotalStallTime, self._vStartUpDelay, False)

#=============================================
    def _rFinish(self):
        if self._vDead: return
#         assert self.playbackTime > 0
        if self._vAbr and "stopAbr" in dir(self._vAbr) and callable(self._vAbr.stopAbr):
            self._vAbr.stopAbr()
        self._vFinished = True
        self._vBufferLenOverTime.append((self._vEnv.getNow(), 0))
        self._vQualitiesPlayedOverTime.append((self._vEnv.getNow(), 0, -1))
        myprint("Simulation finished at:", self._vEnv.getNow(), "totalStallTime:", self._vTotalStallTime, "startUpDelay:", self._vStartUpDelay, "firstSegDlTime:", self._vFirstSegmentDlTime, "segSkipped:", self._vSegmentSkiped)
        myprint("QoE:", self.QoE)
        myprint("stallTime:", self._vStallsAt)
#         myprint("Quality played:", self._vQualitiesPlayed)
#         myprint("Downloaded:", self._vTotalDownloaded, "uploaded:", self._vTotalUploaded, \
#                 "ration U/D:", self._vTotalUploaded/self._vTotalDownloaded)

#=============================================
    def start(self, startedAt = -1):
        segId = self._vNextSegmentIndex
        now = self._vEnv.getNow()
        self._vStartedAt = self._vGlobalStartedAt = now
        if startedAt >= 0:
            playbackTime = now - startedAt
            self._vNextSegmentIndex = int((playbackTime)/self._vVideoInfo.segmentDuration)
#             while (self._vNextSegmentIndex + 1) * self._vVideoInfo.segmentDuratio`n < playbackTime + PLAYBACK_DELAY_THRESHOLD:
#                 self._vNextSegmentIndex += 1
#             self._vNextSegmentIndex += 1
            self._vCanSkip = True
            self._vGlobalStartedAt = startedAt
        self._vLastEventTime = now
        self._rDownloadNextData(0)

