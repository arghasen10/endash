import util.myprint
import os
import numpy as np
import tensorflow as tf
from util import a3c
import atexit
import glob
import re
import sys
import traceback as tb
import math
from termcolor import colored
# import multiprocessing as mp
import util.multiprocwrap as mp
from util.misc import getTraceBack
from util import cprint

def myprint(*arg, **kw):
    util.myprint.myprint("Quality:", *arg, *kw)


S_INFO = 14  # bit_rate, buffer_size, next_chunk_size, bandwidth_measurement(throughput and time), chunk_til_video_end
S_LEN = 8  # take how many frames in the past
A_DIM = 6
ACTOR_LR_RATE = 0.0001
CRITIC_LR_RATE = 0.001
TRAIN_SEQ_LEN = 100  # take as a train batch
MODEL_SAVE_INTERVAL = 100
M_IN_K = 1000.0
RAND_RANGE = 1000000
GRADIENT_BATCH_SIZE = 16

DEFAULT_ACTION = 0

IPC_CMD_UPDATE = "update"
IPC_CMD_PARAM = "param"
IPC_CMD_QUIT = "quit"
IPC_CMD_READY = "ready"


def guessSavedSession(summary_dir):
    files = glob.glob(summary_dir + "/nn_model_ep*.ckpt.index")
    if len(files) > 0:
        files.sort()
        nn_model = files[-1][:-6]
        epoch = re.findall("nn_model_ep_(\d+)", nn_model)
        if len(epoch) == 1:
            epoch = int(epoch[0])
        else:
            epoch = 0

        return nn_model, epoch
    return None, 0


class PensiveLearner():
    def __init__(self, actionset = [], infoDept=S_LEN, infoDim=S_INFO, log_path=None, summary_dir=None, nn_model=None, readOnly=False, master=False, ipcQueues = None):
        assert summary_dir
        assert readOnly

        assert master == (ipcQueues is not None)

        self.master = master
        cprint.cyan("!!CREATING OBJECT!!")
        if not master:
            cprint.blue("!!SLAVE!!")
            self.recv, self.send = [mp.Queue(), mp.Queue()]
            self.proc = mp.Process(target=PensiveLearner, args=(actionset, infoDept, infoDim, log_path, summary_dir, nn_model, readOnly, True, [self.send, self.recv]))
            self.proc.start()
            while True:
                cmd = self.recv.get()
                if cmd == "ready":
                    break
            return

        cprint.red("!!MASTER!!")

        self.recv, self.send = ipcQueues

#         myprint("Pensieproc init Params:", actionset, infoDept, log_path, summary_dir, nn_model)

        self.pid = os.getpid()
        self.summary_dir = summary_dir
        self.nn_model = None if not nn_model else os.path.join(self.summary_dir, nn_model)

        self.a_dim = len(actionset)
        self._vActionset = actionset

        self._vInfoDim = infoDim
        self._vInfoDept = infoDept

        self._vReadOnly = readOnly


        if not os.path.exists(self.summary_dir):
            os.makedirs(self.summary_dir)

        self.sess = tf.Session()


        self.actor = a3c.ActorNetwork(self.sess,
                                 state_dim=[self._vInfoDim, self._vInfoDept], action_dim=self.a_dim,
                                 learning_rate=ACTOR_LR_RATE)

        self.critic = a3c.CriticNetwork(self.sess,
                                   state_dim=[self._vInfoDim, self._vInfoDept], action_dim=self.a_dim,
                                   learning_rate=CRITIC_LR_RATE)

        self.summary_ops, self.summary_vars = a3c.build_summaries()

        self.sess.run(tf.global_variables_initializer())
        self.writer = tf.summary.FileWriter(self.summary_dir, self.sess.graph)  # training monitor
        self.saver = tf.train.Saver()  # save neural net parameters

        # restore neural net parameters
        self.epoch = 0
        if self.nn_model is None and self.master:
            nn_model, epoch = guessSavedSession(self.summary_dir)
            if nn_model:
                self.nn_model = nn_model
                self.epoch = epoch

        if self.nn_model is not None:  # nn_model is the path to file
            self.saver.restore(self.sess, self.nn_model)
            cprint.red("Model restored with `" + self.nn_model + "'")

        try:
            self.runCmd()
        except Exception as ex:
            track = tb.format_exc()
            cprint.red(track)

    def runCmd(self):
        self.send.put("ready")
        while True:
            try:
                cprint.red("RECEIVING IN SERVER")
                cmd, arg, kwarg = self.recv.get()
                cprint.red("RECEIVED IN SERVER")
            except Exception as e:
                track = tb.format_exc()
                cprint.red(track)
                continue
            if cmd == "getNextAction":
                res = None
                error = None
                try:
                    res = self.getNextAction(*arg, **kwarg)
                except Exception as e:
                    track = tb.format_exc()
                    error = track
                    cprint.red(track)
                self.send.put((res, error))

            elif cmd == "quit":
                self.send.put("done")
                break

            else:
                cprint.red("invalid cmd")

    def sendToCounter(self, cmd, *arg, **kwarg):
        self.send.put((cmd, arg, kwarg))

    def getNextAction(self, agentState, state): #peerId and segId are Identifier
        if not self.master:
#             cprint.blue("SENDING", os.getpid())
            self.sendToCounter("getNextAction", agentState, state)
#             cprint.blue("RECEIVING")
            x = self.recv.get()
#             cprint.blue("RETURING")
            return x

        state = None
        if len(agentState.s_batch) == 0:
            state = np.zeros((self._vInfoDim, self._vInfoDept))
        else:
            state = np.array(agentState.s_batch[-1], copy=True)

        state = np.roll(state, -1, axis=1)

        for i,x in enumerate(state):
            x = np.array(x).reshape(-1)
            assert issubclass(x.dtype.type, np.number) and self._vInfoDept >= len(x)
            state[i, :len(x)] = x

        reshapedInput = np.reshape(state, (1, self._vInfoDim, self._vInfoDept))
        action_prob = self.actor.predict(reshapedInput)
        action_cumsum = np.cumsum(action_prob)
        action = (action_cumsum > np.random.randint(1, RAND_RANGE) / float(RAND_RANGE)).argmax()
#         myprint("action:", action,"action cumsum:", action_cumsum.tolist(), "reshapedInput:", reshapedInput.tolist())

        for x in action_prob[0]:
#             if math.isnan(x):
#                 myprint(agentState, "batch len", len(agentState.s_batch), "actor out", self.actor.out)
            assert not math.isnan(x)

        agentState.s_batch += [state]

        return self._vActionset[action], agentState

