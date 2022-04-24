import importlib
import os
import numpy as np

GLOBAL_DELAY_PLAYBACK = 50 #Total arbit


# VIDEO_BIT_RATE = [300,750,1200,1850,2850,4300]  # Kbps
# BITRATE_REWARD = [1, 2, 3, 4, 7, 12, 15, 20]
# BITRATE_REWARD_MAP = {0: 0, 300: 1, 750: 2, 1200: 3, 1850: 12, 2850: 15, 4300: 20}


class VideoInfo():
    def __init__(s, vi):
        s.fileSizes = vi.sizes
        s.segmentDuration = vi.segmentDuration
        s.bitrates = vi.bitrates
        s.bitratesKbps = [x/1000 for x in s.bitrates]
        s.duration = vi.duration
        s.minimumBufferTime = vi.minimumBufferTime
        s.segmentDurations = []
        s.segmentCount = len(s.fileSizes[0])
        s.bitrateReward = vi.bitrateReward
        s.birateRewardMap = {x:y for x, y in zip(s.bitrates, s.bitrateReward)}
        s.globalDelayPlayback = GLOBAL_DELAY_PLAYBACK
        s.maxFileSize = np.amax(s.fileSizes)

    def getSegDuration(s, segId):
        assert segId < s.segmentCount
        if segId == s.segmentCount - 1:
            return int(s.duration)% int(s.segmentDuration)
        return int(s.segmentDuration)
#         dur = 0
#         for x in s.fileSizes[0]:
#             if s.duration - dur > vi.segmentDuration:
#                 s.segmentDurations.append(vi.segmentDuration)
#             else:
#                 s.segmentDurations.append(s.duration - dur)
#             dur += vi.segmentDuration
#         print dur, s.duration

class PenseivVideoInfo():
    def __init__(s, vi):
        s.fileSizes = vi.sizes if len(vi.sizes) != 8 else [x for i, x in enumerate(vi.sizes) if i not in [0,3]]
        s.segmentDuration = vi.segmentDuration
        s.bitrates = vi.bitrates if len(vi.bitrates) != 8 else [x for i, x in enumerate(vi.bitrates) if i not in [0,3]]
        s.bitratesKbps = [x/1000 for x in s.bitrates]
        s.duration = vi.duration
        s.minimumBufferTime = vi.minimumBufferTime
        s.segmentDurations = []
        s.segmentCount = len(s.fileSizes[0])
        s.bitrateReward = vi.bitrateReward if len(vi.sizes) != 8 else [1, 2, 3, 12, 15, 20]
        s.birateRewardMap = {x:y for x, y in zip(s.bitrates, s.bitrateReward)}
        s.globalDelayPlayback = GLOBAL_DELAY_PLAYBACK
        s.maxFileSize = np.amax(s.fileSizes)

    def getSegDuration(s, segId):
        assert segId < s.segmentCount
        if segId == s.segmentCount - 1:
            return int(s.duration)% int(s.segmentDuration)
        return int(s.segmentDuration)


def loadVideoTime(fileName):
    assert os.path.isfile(fileName)
    dirname = os.path.dirname(fileName)
    basename = os.path.basename(fileName)
    module,ext = basename.rsplit(".", 1)
    path = importlib.sys.path
    if len(dirname) != 0:
        importlib.sys.path.append(dirname)
    videoInfo = importlib.import_module(module)

    if len(dirname) != 0:
        importlib.sys.path = path

    if hasattr(videoInfo, "makePensieveReady") and videoInfo.makePensieveReady:
        return PenseivVideoInfo(videoInfo)
    return VideoInfo(videoInfo)

def dummyVideoInfo():
    class dummy():
        def __init__(s):
            s.bitrates = [200000, 400000, 600000, 800000, 1000000, 1500000, 2500000, 4000000]
            numseg = int(np.random.uniform(90,230))
            s.segmentDuration = 8
            s.minimumBufferTime = 16
            s.duration = s.segmentDuration * numseg
            s.sizes = []
            for br in s.bitrates:
                sz = []
                for x in range(numseg):
                    l = float(br) / 8 * s.segmentDuration * np.random.uniform(0.95, 1.05)
                    sz.append(int(l))
                s.sizes.append(sz)
    return VideoInfo(dummy())



if __name__ == "__main__":
    import sys
    x  = loadVideoTime(sys.argv[1])
    print(x.segmentDurations)
    print(x.bitrates)
    print(x.duration)
    print(x.minimumBufferTime)
    #print(x.sizes)
