from simulator.priorityQueue import PriorityQueue
import random

class Simulator():
    def __init__(self):
        self.queue = PriorityQueue()
        self.now = 0.0

    def runAfter(self, after, callback, *args, **kw):
        return self.runAt(self.now+after, callback, *args, **kw)

    def runAt(self, at, callback, *args, **kw):
        at = float(at)
        assert at >= self.now
        ref = self.queue.insert(at, (at, callback, args, kw))
        return ref

    def getNow(self):
        return self.now

    def cancelTask(self, reference):
        return self.queue.delete(reference)

    def run(self):
        while not self.queue.isEmpty():
            at, callback, args, kw = self.queue.extractMin()
            self.now = at
            callback(*args, **kw)

    def isPending(self, ref):
        return self.queue.isRefExists(ref)

    def runNow(self, callback, *args, **kw):
        nexTime = self.now()

        return self.queue.insert(nexTime, (self.now, callback, args, kw))

def smtest(sm, cmd):
    print(sm.getNow(), cmd)
    if cmd == "add":
        time = sm.getNow() + random.uniform(0, 9)
        sm.runAt(time, smtest, sm, "none")

if __name__ == "__main__":
    sm = Simulator()
    sm.runAt(0.25, smtest, sm, "add")
    sm.runAt(5.25, smtest, sm, "add")
    sm.runAt(3.25, smtest, sm, "add")
    sm.runAt(4.25, smtest, sm, "add")
    sm.runAt(6.25, smtest, sm, "add")
    sm.runAt(7.25, smtest, sm, "add")
    sm.runAt(2.25, smtest, sm, "add")
    sm.run()


