import multiprocessing as mp
import time, os
import sys

Pipe = mp.Pipe
Queue = mp.Queue

class Process(mp.Process):
    def __init__(self, outPref=None, errPref=None, group=None, target=None, name=None, args=(), kwargs={}, daemon=None, *argv, **kwargv):
        super().__init__(target=self.myrun, group=group, name=name, daemon=daemon, *argv, **kwargv)
        self.outPref = outPref
        self.errPref = errPref
        self.target = target
        self.args = args
        self.kwargs = kwargs

    def myrun(self):
        outFp = None
        errFp = None
        if self.outPref:
            fname = self.outPref + "_" + str(os.getpid())
            fd = os.open(fname, os.O_WRONLY | os.O_CREAT)
            os.dup2(fd, sys.stdout.fileno())
        if self.errPref:
            fname = self.errPref + "_" + str(os.getpid())
            fd = os.open(fname, os.O_WRONLY | os.O_CREAT)
            os.dup2(fd, sys.stderr.fileno())
        if self.target:
            self.target(*self.args, **self.kwargs)

        exit(0)

def __test():
    print("hello from 1")
    time.sleep(1)

if __name__ == "__main__":
    p1 = Process("/tmp/p1", target=__test)
    p2 = Process("/tmp/p2", target=__test)
    p1.start()
    p2.start()
    p1.join()
    p2.join()
