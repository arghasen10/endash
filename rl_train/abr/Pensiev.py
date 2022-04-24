
import base64
import urllib
import sys
import os
import json
import time
os.environ['CUDA_VISIBLE_DEVICES']=''

import numpy as np
import time
import itertools
import tensorflow as tf
from util import a3c
from util.myprint import myprint
import traceback as tb

from util.calculateMetric import measureQoE
# from util.multiprocwrap import Process, Pipe
import util.multiprocwrap as mp
import atexit


######################## FAST MPC #######################

S_INFO = 6  # bit_rate, buffer_size, rebuffering_time, bandwidth_measurement, chunk_til_video_end
S_LEN = 8  # take how many frames in the past
MPC_FUTURE_CHUNK_COUNT = 5
# VIDEO_BIT_RATE = [300,750,1200,1850,2850,4300]  # Kbps
# BITRATE_REWARD = [1, 2, 3, 12, 15, 20]
# BITRATE_REWARD_MAP = {0: 0, 300: 1, 750: 2, 1200: 3, 1850: 12, 2850: 15, 4300: 20}
M_IN_K = 1000.0
BUFFER_NORM_FACTOR = 10.0
# CHUNK_TIL_VIDEO_END_CAP = 48.0
# TOTAL_VIDEO_CHUNKS = 48
DEFAULT_QUALITY = 0  # default video quality without agent
REBUF_PENALTY = 4.3  # 1 sec rebuffering -> this number of Mbps
SMOOTH_PENALTY = 1
TRAIN_SEQ_LEN = 100  # take as a train batch
MODEL_SAVE_INTERVAL = 100
RANDOM_SEED = 42
RAND_RANGE = 1000
ACTOR_LR_RATE = 0.0001
CRITIC_LR_RATE = 0.001
SUMMARY_DIR = './results'
LOG_FILE = './results/log'
# in format of time_stamp bit_rate buffer_size rebuffer_time video_chunk_size download_time reward
NN_MODEL = None
NN_MODEL = './model/nn_model_ep_116300.ckpt'
NN_MODEL = './model/pretrain_linear_reward.ckpt'
# NN_MODEL = './model/nn_model_ep_529500.ckpt'

class AbrPensieve:
    def __init__(self, videoInfo, agent, log_file_path=LOG_FILE, *kw, **kws):
        self.video = videoInfo
        self.agent = agent
        self.abr = AbrPensieveClass.getInstance(videoInfo, agent, log_file_path, *kw, **kws)

        last_bit_rate = videoInfo.bitratesKbps[DEFAULT_QUALITY]
        last_total_rebuf = 0
        video_chunk_count = 0
        self.input_dict = {
                  'last_bit_rate': last_bit_rate,
                  'last_total_rebuf': last_total_rebuf,
                  'video_chunk_coount': video_chunk_count,
                }

    def stopAbr(self):
        if self.abr:
            self.abr.stopAbr()
#             myprint("="*30, "\n", sys.getrefcount(self.abr),  "\n", "="*30)
            self.abr = None

    def getSleepTime(self, buflen):
        if (self.agent._vMaxPlayerBufferLen - self.video.segmentDuration) > buflen:
            return 0
        sleepTime = buflen + self.video.segmentDuration - self.agent._vMaxPlayerBufferLen
        return sleepTime

    def getNextDownloadTime(self, *kw, **kws):
        if len(self.agent._vRequests) == 0:
            return 0, 0
        req = self.agent._vRequests[-1]
        bufferLeft = self.agent._vBufferUpto - self.agent._vPlaybacktime
        if bufferLeft < 0:
            bufferLeft = 0

        post_data = {
                'lastquality': self.agent._vLastBitrateIndex,
                'RebufferTime': self.agent._vTotalStallTime,
                'lastChunkFinishTime': req.downloadFinished * M_IN_K,
                'lastChunkStartTime': req.downloadStarted * M_IN_K,
                'lastChunkSize': req.clen,
                'buffer': bufferLeft,
                'lastRequest': self.agent.nextSegmentIndex,

                'a_dim' : len(self.video.bitratesKbps),
                'bitrates' : self.video.bitratesKbps,
                'tot_chunks' : self.video.segmentCount,
                'bitrate_reward' : self.video.bitrateReward,
                'video' : self.video,
                'input' : self.input_dict,
                }
        return self.getSleepTime(bufferLeft), self.abr.do_POST(post_data)

class AbrPensieveClass:
    __instance = None
    @staticmethod
    def getInstance(videoInfo, agent, log_file_path=LOG_FILE, *kw, **kws):
        if AbrPensieveClass.__instance == None:
            AbrPensieveClass(videoInfo, agent, log_file_path, *kw, **kws)
        AbrPensieveClass.__instance.count += 1
        return AbrPensieveClass.__instance

    @staticmethod
    def clear():
        AbrPensieveClass.__instance = None

    def __init__(self, videoInfo, agent, log_file_path=LOG_FILE, *kw, **kws):
        assert AbrPensieveClass.__instance == None
        self.recv = mp.Queue()
        self.send = mp.Queue()
        self.proc = mp.Process(target=self.handle, args=(self.send, self.recv, videoInfo, agent, log_file_path) + kw, kwargs=kws)
        self.proc.start()
        AbrPensieveClass.__instance = self
        self.count = 0
        self.agents = []
        atexit.register(self.cleanup)

    def handle(self, recv, send, *args, **kwargs):
        self.orig = AbrPensieveClassProc(*args, **kwargs)
        funcs = { func : getattr(self.orig, func) for func in dir(self.orig) if not func.startswith("__") and not func.endswith("__") and callable(getattr(self.orig, func)) }
        while True:
            func, args, kwargs = recv.get()
            try:
                if func in funcs:
                    res = funcs[func](*args, **kwargs)
                    send.put({"st": True, "res": res})
                elif func == "cleanup":
                    send.put({"st": True, "res":"exit"})
                    myprint("proc cleanup")
                    recv = None
                    send = None
                    return
                else:
                    send.put({"st": True, "res":None})
                    myprint("unknown:", func, args, kwargs)

            except:
                trace = sys.exc_info()
                simpTrace = getTraceBack(trace)
                send.put({"st": False, "trace": simpTrace})
                myprint(simpTrace)

    def _rSend(self, dt):
        self.send.put(dt)
    def _rRecv(self):
        dt = self.recv.get(timeout=60)
        if not dt.get("st", False):
            myprint(dt.get("trace", ""))
            raise Exception(dt.get("trace", ""))
        return dt["res"]

    def do_POST(self, *args, **kwargs):
        self._rSend(("do_POST", args, kwargs))
        return self._rRecv()

    def get_chunk_size(self, *args, **kwargs):
        self._rSend(("get_chunk_size", args, kwargs))
        return self._rRecv()

    def stopAbr(self):

        self.count -= 1
        if self.count != 0:
            return

        if self.proc:
            self._rSend(("cleanup", None, None))
            self.send = None
            pop = self._rRecv()
            self.recv = None
            self.proc.join()
            self.proc = None
            AbrPensieveClass.__instance = None
            myprint("="*30, "cleaned up", "="*30, sep="\n")
#             import pdb; pdb.set_trace()
            return pop

    def cleanup(self):
        while self.count > 0:
            self.stopAbr()

def getTraceBack(exc_info):
    error = str(exc_info[0]) + "\n"
    error += str(exc_info[1]) + "\n\n"
    error += "\n".join(tb.format_tb(exc_info[2]))
    return error

class AbrPensieveClassProc:
    def __init__(self, videoInfo, agent, log_file_path=LOG_FILE, *kw, **kws):
        self.video = None
        #=====================================
        # SETUP
        #=====================================
        log_file = None if not log_file_path else open(log_file_path, 'wb')
        sess = tf.Session()

        A_DIM = len(videoInfo.bitratesKbps)

        actor = a3c.ActorNetwork(sess,
                                 state_dim=[S_INFO, S_LEN], action_dim=A_DIM,
                                 learning_rate=ACTOR_LR_RATE)
        critic = a3c.CriticNetwork(sess,
                                   state_dim=[S_INFO, S_LEN], action_dim=A_DIM,
                                   learning_rate=CRITIC_LR_RATE)

        sess.run(tf.initialize_all_variables())
        saver = tf.train.Saver()  # save neural net parameters

        # restore neural net parameters
        nn_model = NN_MODEL
        if nn_model is not None:  # nn_model is the path to file
            saver.restore(sess, nn_model)
            print("Model restored.")

        init_action = np.zeros(A_DIM)
        init_action[DEFAULT_QUALITY] = 1

        s_batch = [np.zeros((S_INFO, S_LEN))]
        a_batch = [init_action]
        r_batch = []

        train_counter = 0

        # need this storage, because observation only contains total rebuffering time
        # we compute the difference to get


        input_dict = {'sess': sess, 'log_file': log_file,
                      'actor': actor, 'critic': critic,
                      'saver': saver, 'train_counter': train_counter,
                      's_batch': s_batch, 'a_batch': a_batch, 'r_batch': r_batch,
                      }

        #=====================================
        # INIT
        #=====================================

        self.input_dict = input_dict
        self.sess = input_dict['sess']
        self.log_file = input_dict['log_file']
        self.actor = input_dict['actor']
        self.critic = input_dict['critic']
        self.saver = input_dict['saver']
        self.s_batch = input_dict['s_batch']
        self.a_batch = input_dict['a_batch']
        self.r_batch = input_dict['r_batch']


    def get_chunk_size(self, quality, index):
        if index >= self.video.segmentCount: return 0
        return self.video.fileSizes[quality][index]


    def do_POST(self, post_data):

        A_DIM = post_data['a_dim']
        VIDEO_BIT_RATE = post_data['bitrates']
        TOTAL_VIDEO_CHUNKS = post_data['tot_chunks']
        CHUNK_TIL_VIDEO_END_CAP = TOTAL_VIDEO_CHUNKS
        BITRATE_REWARD = post_data['bitrate_reward']

        self.video = post_data['video']

        # option 1. reward for just quality
        # reward = post_data['lastquality']
        # option 2. combine reward for quality and rebuffer time
        #           tune up the knob on rebuf to prevent it more
        # reward = post_data['lastquality'] - 0.1 * (post_data['RebufferTime'] - self.input_dict['last_total_rebuf'])
        # option 3. give a fixed penalty if video is stalled
        #           this can reduce the variance in reward signal
        # reward = post_data['lastquality'] - 10 * ((post_data['RebufferTime'] - self.input_dict['last_total_rebuf']) > 0)

        # option 4. use the metric in SIGCOMM MPC paper
        rebuffer_time = float(post_data['RebufferTime'] - post_data['input']['last_total_rebuf'])

        # --linear reward--
        reward = VIDEO_BIT_RATE[post_data['lastquality']] / M_IN_K \
                - REBUF_PENALTY * rebuffer_time / M_IN_K \
                - SMOOTH_PENALTY * np.abs(VIDEO_BIT_RATE[post_data['lastquality']] -
                                          post_data['input']['last_bit_rate']) / M_IN_K

        # --log reward--
        # log_bit_rate = np.log(VIDEO_BIT_RATE[post_data['lastquality']] / float(VIDEO_BIT_RATE[0]))
        # log_last_bit_rate = np.log(self.input_dict['last_bit_rate'] / float(VIDEO_BIT_RATE[0]))

        # reward = log_bit_rate \
        #          - 4.3 * rebuffer_time / M_IN_K \
        #          - SMOOTH_PENALTY * np.abs(log_bit_rate - log_last_bit_rate)

        # --hd reward--
        # reward = BITRATE_REWARD[post_data['lastquality']] \
        #         - 8 * rebuffer_time / M_IN_K - np.abs(BITRATE_REWARD[post_data['lastquality']] - BITRATE_REWARD_MAP[self.input_dict['last_bit_rate']])

        post_data['input']['last_bit_rate'] = VIDEO_BIT_RATE[post_data['lastquality']]
        post_data['input']['last_total_rebuf'] = post_data['RebufferTime']

        # retrieve previous state
        if len(self.s_batch) == 0:
            state = [np.zeros((S_INFO, S_LEN))]
        else:
            state = np.array(self.s_batch[-1], copy=True)

        # compute bandwidth measurement
        video_chunk_fetch_time = post_data['lastChunkFinishTime'] - post_data['lastChunkStartTime']
        video_chunk_size = post_data['lastChunkSize']

        # compute number of video chunks left
        video_chunk_remain = TOTAL_VIDEO_CHUNKS - post_data['input']['video_chunk_coount']
        post_data['input']['video_chunk_coount'] += 1

        # dequeue history record
        state = np.roll(state, -1, axis=1)

        next_video_chunk_sizes = []
        for i in range(A_DIM):
            next_video_chunk_sizes.append(self.get_chunk_size(i, post_data['input']['video_chunk_coount']))

        # this should be S_INFO number of terms
        try:
            state[0, -1] = VIDEO_BIT_RATE[post_data['lastquality']] / float(np.max(VIDEO_BIT_RATE))
            state[1, -1] = post_data['buffer'] / BUFFER_NORM_FACTOR
            state[2, -1] = float(video_chunk_size) / float(video_chunk_fetch_time) / M_IN_K  # kilo byte / ms
            state[3, -1] = float(video_chunk_fetch_time) / M_IN_K / BUFFER_NORM_FACTOR  # 10 sec
            state[4, :A_DIM] = np.array(next_video_chunk_sizes) / M_IN_K / M_IN_K  # mega byte
            state[5, -1] = np.minimum(video_chunk_remain, CHUNK_TIL_VIDEO_END_CAP) / float(CHUNK_TIL_VIDEO_END_CAP)
        except ZeroDivisionError:
            # this should occur VERY rarely (1 out of 3000), should be a dash issue
            # in this case we ignore the observation and roll back to an eariler one
            if len(self.s_batch) == 0:
                state = [np.zeros((S_INFO, S_LEN))]
            else:
                state = np.array(self.s_batch[-1], copy=True)

        # log wall_time, bit_rate, buffer_size, rebuffer_time, video_chunk_size, download_time, reward
        if self.log_file:
            self.log_file.write(str(time.time()) + '\t' +
                            str(VIDEO_BIT_RATE[post_data['lastquality']]) + '\t' +
                            str(post_data['buffer']) + '\t' +
                            str(rebuffer_time / M_IN_K) + '\t' +
                            str(video_chunk_size) + '\t' +
                            str(video_chunk_fetch_time) + '\t' +
                            str(reward) + '\n')
            self.log_file.flush()

        action_prob = self.actor.predict(np.reshape(state, (1, S_INFO, S_LEN)))
        action_cumsum = np.cumsum(action_prob)
        bit_rate = (action_cumsum > np.random.randint(1, RAND_RANGE) / float(RAND_RANGE)).argmax()
        # Note: we need to discretize the probability into 1/RAND_RANGE steps,
        # because there is an intrinsic discrepancy in passing single state and batch states

        # send data to html side
        return bit_rate

