from simenv.Simple import Simple
from util import load_trace
import util.videoInfo as video
from abr.BOLA import BOLA
from simulator.simulator import Simulator

import numpy as np
import glob

import json
import os
import shutil



POW_ACT, POW_TAIL, POW_INACT = 396, 148, 0 #mili watt
TAIL_TIME = 6.2 #sec
RAMP_ENERGY = 158 #mili joule


class PowerConsum():
    def __init__(self):
        self._vTime2Energy = []
        self._vTotalEnergy = 0
        self._vCurPowerState = "I" # "I", "A", "T"

#=============================================
    def print(self):
        print("Time to Energy", self._vTime2Energy, len(self._vTime2Energy))

#=============================================
    def _rSave(self, fpath):
        with open(fpath, "w") as fp:
            json.dump({'te' : self._vTime2Energy, 'en' : self._vTotalEnergy, 'cp' : self._vCurPowerState}, fp)

#=============================================
    def _rLoad(self, fpath):
        with open(fpath) as fp:
            obj = json.load(fp)
            self._vTime2Energy = obj['te']
            self._vTotalEnergy = obj['en']
            self._vCurPowerState = obj['cp']


#=============================================
    def _rComputePowerUsageFrom(self, now, fromTime):
        #It is a simulation based on ramp-power, active, tail and idle power consumption observed in war drive data.

#         now = self.now
        assert len(self._vTime2Energy) > 0
        wattage = {"I" : POW_INACT, "T" : POW_TAIL, "A": POW_ACT}

        times = [x[0] for x in self._vTime2Energy]
        startIndex = np.searchsorted(times, fromTime, side="right")

        startTime, energy, state = self._vTime2Energy[startIndex - 1]
        st = fromTime - startTime
        assert st >= 0
        energy += wattage[state] * st

        endIndex = np.searchsorted(times, now, side="right")
        endTime, endEnergy, state = self._vTime2Energy[startIndex - 1]
        st = now - endTime
        assert st >= 0
        endEnergy += wattage[state] * st

        assert endEnergy >= energy
        return energy - endEnergy #it is expected to give negative energy

#=============================================
    def _rCalculateEneryConsumption(self, now, setActive = False, setInactive=False):
#         now = self.now
        if len(self._vTime2Energy) == 0:
            assert setActive and self._vCurPowerState == "I"
            self._vTotalEnergy += RAMP_ENERGY
            self._vTime2Energy += [(now, self._vTotalEnergy, "A")]
            self._vCurPowerState = "A"
#             self._vRadioHigh = True
            return

        lastTime = self._vTime2Energy[-1][0]

        assert (setActive != setInactive)# or not setActive)

        spent = now - lastTime
        energySpent = 0 #joule

        if setActive:
            assert self._vCurPowerState != "I"
            if self._vCurPowerState == "T":
                if spent <= TAIL_TIME:
                    energySpent += spent * POW_TAIL
                else:
                    energySpent += TAIL_TIME * POW_TAIL
                    self._vTime2Energy += [(lastTime + TAIL_TIME, self._vTotalEnergy+energySpent, "I")]

                self._vTime2Energy += [(now, self._vTotalEnergy + energySpent + RAMP_ENERGY, "A")]
                self._vTotalEnergy += energySpent + RAMP_ENERGY
                self._vCurPowerState = "A"
            return

        if setInactive:
            assert self._vCurPowerState == "A"
            energySpent = spent * POW_ACT
            self._vTime2Energy += [(now, self._vTotalEnergy + energySpent, "T")]
            self._vTotalEnergy += energySpent
            self._vCurPowerState = "T"


#=============================================
#=============================================

class PowerMeasurementEnv(Simple):
    def __init__(self, *kw, **kws):
        super().__init__(*kw, **kws)
        self._vPowerUsage = PowerConsum()

#=============================================
    #Should be over ridden if we need to create another env
    def _rFetchSegment(self, nextSegId, nextQuality, sleepTime, extraData=None):
        if self._vDead: return
        self._rCalculateEneryConsumption(setActive=True)
        super()._rFetchSegment(nextSegId, nextQuality, sleepTime, extraData)

#=============================================
    def _rComputePowerUsageFrom(self, fromTime):
        return self._vPowerUsage._rComputePowerUsageFrom(self.now, fromTime)

#=============================================
    def _rCalculateEneryConsumption(self, setActive = False, setInactive=False):
        return self._vPowerUsage._rCalculateEneryConsumption(self.now, setActive, setInactive)

#=============================================
    def _rAddToBuffer(self, req, simIds = None):
    #Should be over ridden if we need to create another env
        if self._vDead: return
        self._rCalculateEneryConsumption(setInactive=True)
        super()._rAddToBuffer(req, simIds)


#=============================================
    def _rFinish(self):
#         self._vPowerUsage.print()
#         print("Time to Energy", self._vTime2Energy, len(self._vTime2Energy))
        super()._rFinish()


#=============================================
#=============================================
def experimentSimpleOne(abr=BOLA):
    traces = load_trace.load_trace()
    traces = list(zip(*traces))

    if os.path.isdir("infos"):
        shutil.rmtree("infos")
    os.mkdir("infos")
    allPath = []

    QoEs = []
    totalEnergies = []
    totalTimes = []
    totalStallTime = []
    videoDuration = []
    avgBitrate = []
    avgSmoothness = []

    for i, trce in enumerate(traces):
        allPath += [[]]
        for j, f in enumerate(glob.glob("./videofilesizes/sizes_*.py")):
            vi = video.loadVideoTime(f)
            simulator = Simulator()
            env = PowerMeasurementEnv(vi, trce, simulator, abr, modelPath="ResModel")
            simulator.runAt(5, env.start, 2) # start, start func, global start
            simulator.run()

            fpath = f"infos/oldIndfo_{i}_{j}"
            env._vPowerUsage._rSave(fpath)

            allPath[i] += [[trce[2], f, fpath, ]]

            QoEs += [env._vAgent.QoE]
            totalEnergies += [env._vPowerUsage._vTotalEnergy]
            totalTimes += [env.now]
            totalStallTime += [env._vAgent.totalStallTime]
            videoDuration += [env._vVideoInfo.duration]
            avgBitrate += [env._vAgent.avgBitrate]
            avgSmoothness += [env._vAgent.avgBitrateVariation]

#             break
#         break

    with open("infos/info", "w") as fp:
        json.dump(allPath, fp)

    with open("PowerMeasure.dat", "w") as fp:
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
    env = PowerMeasurementEnv(vi, trce, simulator, abr, modelPath="ResModel")
    simulator.runAt(5, env.start, 2)
    simulator.run()
    env._vPowerUsage._rSave("/tmp/old")
    print("totalEnergy:", env._vPowerUsage._vTotalEnergy)





#=============================================
def main():
    experimentSpecialOne()


if __name__ == "__main__":
    for x in range(1):
        main()
        print("=========================\n")
