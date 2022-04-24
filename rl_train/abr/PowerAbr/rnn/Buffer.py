
import util.myprint
import os
import numpy as np
import tensorflow as tf
from util import a3c
import util.multiprocwrap as mp
import atexit
import glob
import re
import sys
import traceback as tb
import math
from termcolor import colored
from util.misc import getTraceBack

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


class PensiveLearnerCentralAgent():
    def __init__(self, actionset = [], infoDept=S_LEN, infoDim=S_INFO, log_path=None, summary_dir=None, nn_model=None):

        assert summary_dir
        myprint("Central init Params:", actionset, infoDept, log_path, summary_dir, nn_model)
        self.summary_dir = os.path.join(summary_dir, "rnnBuffer")
        self.nn_model = nn_model

        self.a_dim = len(actionset)
        self._vActionset = actionset

        self._vInfoDim = infoDim
        self._vInfoDept = infoDept


        if not os.path.exists(self.summary_dir):
            os.makedirs(self.summary_dir)

        self.sess = tf.Session()
#         log_file = open(os.path.join(log_path, "PensiveLearner", "wb"))


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

        self.epoch = 0

        # restore neural net parameters
        if self.nn_model is None:
            nn_model, epoch = guessSavedSession(self.summary_dir)
            if nn_model:
                self.nn_model = nn_model
                self.epoch = epoch

#         nn_model = NN_MODEL
        if self.nn_model is not None:  # nn_model is the path to file
            self.saver.restore(self.sess, self.nn_model)
            myprint("Model restored.")



        self.actor_gradient_batch = []
        self.critic_gradient_batch = []


    def saveModel(self, s_batch, a_batch, r_batch, entropy_record, end_of_video=False):
        actor_gradient, critic_gradient, td_batch = \
            a3c.compute_gradients(s_batch=np.stack(s_batch, axis=0),  # ignore the first chuck
                                  a_batch=np.vstack(a_batch),  # since we don't have the
                                  r_batch=np.vstack(r_batch),  # control over it
                                  terminal=end_of_video, actor=self.actor, critic=self.critic)
        td_loss = np.mean(td_batch)

        self.actor_gradient_batch.append(actor_gradient)
        self.critic_gradient_batch.append(critic_gradient)

        myprint("====")
        myprint("Master: Quality: Epoch", self.epoch)
        myprint("TD_loss", td_loss, "Avg_reward", np.mean(r_batch), "Avg_entropy", np.mean(entropy_record))
        myprint("====")

        summary_str = self.sess.run(self.summary_ops, feed_dict={
            self.summary_vars[0]: td_loss,
            self.summary_vars[1]: np.mean(r_batch),
            self.summary_vars[2]: np.mean(entropy_record)
        })

        self.writer.add_summary(summary_str, self.epoch)
        self.writer.flush()

        self.entropy_record = []

        if len(self.actor_gradient_batch) >= GRADIENT_BATCH_SIZE:

            assert len(self.actor_gradient_batch) == len(self.critic_gradient_batch)

            for i in range(len(self.actor_gradient_batch)):
                self.actor.apply_gradients(self.actor_gradient_batch[i])
                self.critic.apply_gradients(self.critic_gradient_batch[i])

            self.actor_gradient_batch = []
            self.critic_gradient_batch = []

            self.epoch += 1
            if self.epoch % MODEL_SAVE_INTERVAL == 0:
                # Save the neural net parameters to disk.
                save_path = self.saver.save(self.sess, self.summary_dir + "/nn_model_ep_" +
                                       str(self.epoch) + ".ckpt")
                myprint("Model saved in file: %s" % save_path)

        return self.getParams()

    def getParams(self):
        return self.actor.get_network_params(), self.critic.get_network_params()


class PensiveLearner():
    __instance = None
    @staticmethod
    def getInstance(*arg, **karg):
        if PensiveLearner.__instance == None:
            if os.environ.get("EXP_ENV_LEARN_PROC_BUFFER", "YES") == "NO":
                PensiveLearner.__instance = PensiveLearnerProc(*arg, **karg)
            else:
                PensiveLearner(*arg, **karg)
        return PensiveLearner.__instance

    def __init__(self, actionset = [], infoDept=S_LEN, *arg, **kwarg):
        assert PensiveLearner.__instance == None
        PensiveLearner.__instance = self
        self.recv, self.send = mp.Queue(), mp.Queue()
        self._vActionset = actionset
        self._vInfoDept = infoDept
        self._vRunning = True
        p = mp.Process(target=self.handle, args= (self.send, self.recv, actionset, infoDept)+arg, kwargs=kwarg)
        p.start()
        self.proc = p
        atexit.register(self.cleanup)

    def handle(self, recv, send, *arg, **kwarg):
        orig = PensiveLearnerProc(*arg, **kwarg)
        while True:
            fun, arg, karg = recv.get()
            res = None
            try:
                if fun == "getNextAction":
                    res = orig.getNextAction(*arg, **karg)
                elif fun == "addReward":
                    res = orig.addReward(*arg, **karg)
                elif fun == "saveModel":
                    res = orig.saveModel(*arg, **karg)
                elif fun == "finish":
                    res = orig.finish(*arg, **karg)
                elif fun == "cleanup":
                    send.put({"st": True, "res": "exit"})
                    return
                elif fun in dir(orig) and not callable(getattr(orig, fun)):
                    res = getattr(orig, fun)
                send.put({"st": True, "res": res})
            except:
                trace = sys.exc_info()
                simpTrace = getTraceBack(trace)
                send.put({"st": False, "trace": simpTrace})

    @staticmethod
    def cleanup(*arg, **kwarg):
        if os.environ.get("EXP_ENV_LEARN_PROC_BUFFER", "YES") == "NO":
            return
        if not PensiveLearner.__instance or not PensiveLearner.__instance._vRunning:
            return
        if not PensiveLearner.__instance.proc:
            return
        if not PensiveLearner.__instance.proc.is_alive():
            PensiveLearner.__instance.proc.join()
            return
        PensiveLearner.__instance.cleanupInstance(*arg, **kwarg)

    @staticmethod
    def finish(*arg, **kwarg):
        if os.environ.get("EXP_ENV_LEARN_PROC_BUFFER", "YES") == "NO":
            return
        if not PensiveLearner.__instance or not PensiveLearner.__instance._vRunning:
            return
        PensiveLearner.__instance.finishInstance(*arg, **kwarg)

    def _rSend(self, dt):
        self.send.put(dt)
    def _rRecv(self):
        dt = self.recv.get(timeout=60)
        if not dt.get("st", False):
            myprint(dt.get("trace", ""))
            raise Exception(dt.get("trace", ""))
        return dt["res"]

    def __getattr__(self, name):
        self._rSend((name, [], {}))
        return self._rRecv()

    def cleanupInstance(self, *arg, **kwarg):
        self._rSend(("cleanup", arg, kwarg))
        self._rRecv()
        self.proc.join()
        self.proc = None
        self._vRunning = False

    def getNextAction(self, *arg, **kwarg):
        self._rSend(("getNextAction", arg, kwarg))
        return self._rRecv()

    def addReward(self, *arg, **kwarg):
        self._rSend(("addReward", arg, kwarg))
        return self._rRecv()


    def saveModel(self, *arg, **kwarg):
        self._rSend(("saveModel", arg, kwarg))
        return self._rRecv()

    def finishInstance(self, *arg, **kwarg):
        self._rSend(("finish", arg, kwarg))
        return self._rRecv()





class PensiveLearnerProc():
    def __init__(self, actionset = [], infoDept=S_LEN, infoDim=S_INFO, log_path=None, summary_dir=None, nn_model=None, ipcQueue=None, ipcId=None, readOnly=False):
        assert summary_dir
        assert (not ipcQueue and not ipcId) or (ipcQueue and ipcId)
        myprint("Pensieproc init Params:", actionset, infoDept, log_path, summary_dir, nn_model)

        self.ipcQueue = ipcQueue
        self.pid = os.getpid()
        self.ipcId = ipcId
        self.summary_dir = os.path.join(summary_dir, "rnnBuffer")
        self.nn_model = None if not nn_model else os.path.join(self.summary_dir, nn_model)

        self.a_dim = len(actionset)
        self._vActionset = actionset

        self._vInfoDim = infoDim
        self._vInfoDept = infoDept

        self._vReadOnly = readOnly


        if not os.path.exists(self.summary_dir):
            os.makedirs(self.summary_dir)

        self.sess = tf.Session()
#         log_file = open(os.path.join(log_path, "PensiveLearner", "wb"))


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
        if self.nn_model is None and not self.ipcQueue:
            nn_model, epoch = guessSavedSession(self.summary_dir)
            if nn_model:
                self.nn_model = nn_model
                self.epoch = epoch
#         nn_model = NN_MODEL
        if self.nn_model is not None and not self.ipcQueue:  # nn_model is the path to file
            self.saver.restore(self.sess, self.nn_model)
            myprint("Model restored with `" + self.nn_model + "'")


        if self.ipcQueue:
            self.ipcQueue[0].put({"id":self.ipcId, "pid": self.pid, "cmd":IPC_CMD_PARAM})
            myprint("="*50)
            myprint(self.ipcId , ": waiting for ipc")
            myprint("="*50)
            res = None
            while True:
                res = self.ipcQueue[1].get()
                pid = res["pid"]
                res = res["res"]
                if pid == self.pid:
                    break
            actor_net_params, critic_net_params = res
            self.actor.set_network_params(actor_net_params)
            self.critic.set_network_params(critic_net_params)
            myprint("="*50)
            myprint(self.ipcId , ": ipcOver")
            myprint("="*50)

        self.s_batch = []
        self.a_batch = []
        self.r_batch = []
        self.entropy_record = []

        self.actor_gradient_batch = []
        self.critic_gradient_batch = []

        self.keyedSBatch = {}
        self.keyedActionProb = {}
        self.keyedAction = {}
        self.keyedInputParam = {}

    def getNextAction(self, rnnkey, state): #peerId and segId are Identifier

        #pendings_, curbufs_, pbdelay_, uploaded_, lastDlAt_, players_, deadline = state
#         lastPlayerId_, lastQl_, lastClens_, lastStartsAt_, lastFinishAt_, pendings_, deadline = state
#         thrpt_, lastQl_, lastClens_, clens_, wthrghpt, buf, deadline = state
#         myprint("thrpt_:", thrpt_, '\n'," lastQl_:",  lastQl_, '\n'," lastClens_:",  lastClens_, '\n'," clens_:",  clens_, '\n'," wthrghpt:",  wthrghpt, '\n'," buf:",  buf, '\n'," deadline:",  deadline, '\n')
        inputset = state

#         v_dim = len(thrpt_)

        # reward is video quality - rebuffer penalty - smooth penalty
        # retrieve previous state
#             state = np.zeros((self._vInfoDim, self._vInfoDept))
        if len(self.s_batch) == 0:
            state = np.zeros((self._vInfoDim, self._vInfoDept))
        else:
            state = np.array(self.s_batch[-1], copy=True)

        # dequeue history record
        state = np.roll(state, -1, axis=1)

        for i,x in enumerate(inputset):
            x = np.array(x).reshape(-1)
            assert issubclass(x.dtype.type, np.number) and self._vInfoDept >= len(x)
            if len(x) > 1:
                state[i, :len(x)] = x
            else:
                state[i, :-1] = x
#         state[ 0, :len(thrpt_)]         = thrpt_
#         state[ 1, :len(lastQl_)]        = lastQl_
#         state[ 2, :len(lastClens_)]     = lastClens_
#         state[ 3, :len(clens_)]         = clens_
#         state[ 4, -1]                  = wthrghpt
#         state[ 5, -1]                  = buf
#         state[ 6, -1]                  = deadline


        reshapedInput = np.reshape(state, (1, self._vInfoDim, self._vInfoDept))
        action_prob = self.actor.predict(reshapedInput)
        action_cumsum = np.cumsum(action_prob)
        action = (action_cumsum > np.random.randint(1, RAND_RANGE) / float(RAND_RANGE)).argmax()
        myprint("action:", action,"action cumsum:", action_cumsum.tolist(), "reshapedInput:", reshapedInput.tolist())

#         for i, x in enumerate(state):
#             if np.count_nonzero(x) <= 0:
#                 myprint("Some error=======================================")
#                 myprint(f"\033[1;31mError in param {i}\033[m")

        for x in action_prob[0]:
            if math.isnan(x):
                myprint(inputset, "batch len", len(self.s_batch), "actor out", self.actor.out)
            assert not math.isnan(x)
        # Note: we need to discretize the probability into 1/RAND_RANGE steps,
        # because there is an intrinsic discrepancy in passing single state and batch states

        if not self.nn_model or self._vReadOnly: #i.e. only for training
            self.keyedSBatch[rnnkey] = state
            self.keyedActionProb[rnnkey] = action_prob
            self.keyedAction[rnnkey] = action
            self.keyedInputParam[rnnkey] = inputset

        return self._vActionset[action]

    def addReward(self, rnnkey, reward):
        if self.nn_model or self._vReadOnly: #i.e. no training
            return
        assert rnnkey in self.keyedSBatch and rnnkey in self.keyedActionProb

        state = self.keyedSBatch[rnnkey]
        action_prob = self.keyedActionProb[rnnkey]
        action = self.keyedAction[rnnkey]

        myprint("Training dataset:", {"input" : self.keyedInputParam[rnnkey], "action" : self._vActionset[self.keyedAction[rnnkey]], "key" : rnnkey, "reward": reward, "action_prob": action_prob.tolist()})
        del self.keyedSBatch[rnnkey]
        del self.keyedActionProb[rnnkey]
        del self.keyedAction[rnnkey]
        del self.keyedInputParam[rnnkey]

        self.r_batch.append(reward)

        self.entropy_record.append(a3c.compute_entropy(action_prob[0]))

        self.s_batch.append(state)

        action_vec = np.zeros(self.a_dim)
        action_vec[action] = 1
        self.a_batch.append(action_vec)


        if len(self.r_batch) >= TRAIN_SEQ_LEN:  # do training once
            self.saveModel()

    def saveModel(self, end_of_video=False):
        if self._vReadOnly:
            return
        if self.ipcQueue:
            self.ipcQueue[0].put({"id":self.ipcId, "cmd": IPC_CMD_UPDATE, "pid": self.pid, "data": [self.s_batch, self.a_batch, self.r_batch, self.entropy_record, end_of_video]})
            res = None
            while True:
                res = self.ipcQueue[1].get()
                pid = res["pid"]
                res = res["res"]
                if pid == self.pid:
                    break
            actor_net_params, critic_net_params = res
            self.actor.set_network_params(actor_net_params)
            self.critic.set_network_params(critic_net_params)

            del self.s_batch[:]
            del self.a_batch[:]
            del self.r_batch[:]
            del self.entropy_record[:]

            return

        actor_gradient, critic_gradient, td_batch = \
            a3c.compute_gradients(s_batch=np.stack(self.s_batch, axis=0),  # ignore the first chuck
                                  a_batch=np.vstack(self.a_batch),  # since we don't have the
                                  r_batch=np.vstack(self.r_batch),  # control over it
                                  terminal=end_of_video, actor=self.actor, critic=self.critic)
        td_loss = np.mean(td_batch)

        self.actor_gradient_batch.append(actor_gradient)
        self.critic_gradient_batch.append(critic_gradient)

        myprint("====")
        myprint("Quality: Epoch", self.epoch)
        myprint("TD_loss", td_loss, "Avg_reward", np.mean(self.r_batch), "Avg_entropy", np.mean(self.entropy_record))
        myprint("====")

        summary_str = self.sess.run(self.summary_ops, feed_dict={
            self.summary_vars[0]: td_loss,
            self.summary_vars[1]: np.mean(self.r_batch),
            self.summary_vars[2]: np.mean(self.entropy_record)
        })

        self.writer.add_summary(summary_str, self.epoch)
        self.writer.flush()

        self.entropy_record = []

        if len(self.actor_gradient_batch) >= GRADIENT_BATCH_SIZE:

            assert len(self.actor_gradient_batch) == len(self.critic_gradient_batch)

            for i in range(len(self.actor_gradient_batch)):
                self.actor.apply_gradients(self.actor_gradient_batch[i])
                self.critic.apply_gradients(self.critic_gradient_batch[i])

            self.actor_gradient_batch = []
            self.critic_gradient_batch = []

            self.epoch += 1
            if self.epoch % MODEL_SAVE_INTERVAL == 0:
                # Save the neural net parameters to disk.
                save_path = self.saver.save(self.sess, self.summary_dir + "/nn_model_ep_" +
                                       str(self.epoch) + ".ckpt")
                myprint("Model saved in file: %s" % save_path)

        del self.s_batch[:]
        del self.a_batch[:]
        del self.r_batch[:]

    def finish(self):
        if len(self.r_batch) == 0:
            return
        self.saveModel(True)


CENTRAL_ACTION_SET = None

PENSIEVE_SLAVE_QUEUE = None
PENSIEVE_SLAVE_ID = None
PENSIEVE_IPC_QUEUES = None

def setSlaveId(slaveId):
    global PENSIEVE_SLAVE_QUEUE, PENSIEVE_SLAVE_ID
    assert PENSIEVE_IPC_QUEUES
    assert slaveId in PENSIEVE_IPC_QUEUES[1]
    PENSIEVE_SLAVE_QUEUE = [PENSIEVE_IPC_QUEUES[0], PENSIEVE_IPC_QUEUES[1][slaveId]]
    PENSIEVE_SLAVE_ID = slaveId
    atexit.register(slavecleanup)

def slavecleanup():
    PensiveLearner.cleanup()


def getPensiveLearner(actionset = [], *kw, **kws):
    assert not CENTRAL_ACTION_SET or len(CENTRAL_ACTION_SET) == len(actionset)
    p = PensiveLearner.getInstance(actionset, *kw, ipcQueue=PENSIEVE_SLAVE_QUEUE, ipcId=PENSIEVE_SLAVE_ID, **kws)
    assert p._vActionset == actionset and ("infoDept" not in kws or p._vInfoDept == kws["infoDept"])
    return p

def saveLearner():
        PensiveLearner.finish()

def _runCentralServer(actionset = [], *kw, **kws):
    centralLearner = PensiveLearnerCentralAgent(actionset, *kw, **kws)
    masterQueue, slaveQueues = PENSIEVE_IPC_QUEUES
    while True:
        req = masterQueue.get()
#         myprint("="*40)
#         myprint("req received:", req)
        if "cmd" not in req:
            continue
        if req["cmd"] == IPC_CMD_UPDATE:
            slvId = req["id"]
            slvPid = req["pid"]
            dt = req["data"]
            res = centralLearner.saveModel(*dt)
            slaveQueues[slvId].put({"res":res, "pid": slvPid})
        elif req["cmd"] == IPC_CMD_PARAM:
            slvId = req["id"]
            slvPid = req["pid"]
            res = centralLearner.getParams()
            slaveQueues[slvId].put({"res":res, "pid": slvPid})
        elif req["cmd"] == IPC_CMD_QUIT:
            exit(0)
            break

def quitCentralServer():
    masterQueue, slaveQueues = PENSIEVE_IPC_QUEUES
    masterQueue.put({"cmd":IPC_CMD_QUIT})


def runCentralServer(slaveIds = [], actionset = [], *kw, **kws):
    global CENTRAL_ACTION_SET, PENSIEVE_IPC_QUEUES
    CENTRAL_ACTION_SET = actionset
    masterQueue = mp.Queue()
    slaveQueues = {x:mp.Queue() for x in slaveIds}
    PENSIEVE_IPC_QUEUES = [masterQueue, slaveQueues]

    p = mp.Process(target=_runCentralServer, args=(actionset, )+kw, kwargs=kws)
    p.start()
    return p

