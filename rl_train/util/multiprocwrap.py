import util.multiproc as mp
import os
Pipe = mp.Pipe
Queue = mp.Queue

DEFAULT_STD_DIR = "stdouterrdir"

def Process(outPref=None, errPref=None, *argv, **kwargv):
    if not outPref and not errPref:
        if not os.path.isdir(DEFAULT_STD_DIR):
            os.makedirs(DEFAULT_STD_DIR)
        outPref = os.path.join(DEFAULT_STD_DIR, "out_" + str(os.getpid()))
        errPref = os.path.join(DEFAULT_STD_DIR, "err_" + str(os.getpid()))
    return mp.Process(outPref = outPref, errPref = errPref, *argv, **kwargv)
