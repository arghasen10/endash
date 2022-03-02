
import base64
import urllib
import sys
import os
import json
import time
import struct

os.environ['CUDA_VISIBLE_DEVICES']=''

import traceback as tb
import pickle
import socketserver
import socket

import atexit
import numpy as np
import time
import itertools
import tensorflow as tf
from util import a3c
from util.myprint import myprint
from util.nestedObject import getObj
from util import cprint


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
# in format of time_stamp bit_rate buffer_size rebufferTime video_chunk_size download_time reward
NN_MODEL = None
NN_MODEL = sys.path[0] + '/pensievemodel/nn_model_ep_116300.ckpt'
NN_MODEL = sys.path[0] + '/pensievemodel/pretrain_linear_reward.ckpt'
# NN_MODEL = './model/nn_model_ep_529500.ckpt'


class AbrPensieveClassProc:
    def __init__(self, A_DIM, **kws):
        #=====================================
        # SETUP
        #=====================================

        self.sess = tf.Session()

        self.A_DIM = A_DIM

        self.actor = a3c.ActorNetwork(self.sess,
                                 state_dim=[S_INFO, S_LEN], action_dim=A_DIM,
                                 learning_rate=ACTOR_LR_RATE)
        self.critic = a3c.CriticNetwork(self.sess,
                                   state_dim=[S_INFO, S_LEN], action_dim=A_DIM,
                                   learning_rate=CRITIC_LR_RATE)

        self.sess.run(tf.initialize_all_variables())
        self.saver = tf.train.Saver()  # save neural net parameters

        # restore neural net parameters
        nn_model = NN_MODEL
        if nn_model is not None:  # nn_model is the path to file
            self.saver.restore(self.sess, nn_model)
            print("Model restored.")


    def getNextQuality(self, vidInfo, agent, agentState, minimalState=False):

        if len(vidInfo.bitrates) != self.A_DIM:
            return 0, agentState

        lastQuality = agent.requests[-1]["qualityIndex"]

        VIDEO_BIT_RATE = vidInfo.bitratesKbps
        TOTAL_VIDEO_CHUNKS = vidInfo.segmentCount
        CHUNK_TIL_VIDEO_END_CAP = TOTAL_VIDEO_CHUNKS

        # option 1. reward for just quality
        # reward = lastQuality
        # option 2. combine reward for quality and rebuffer time
        #           tune up the knob on rebuf to prevent it more
        # reward = lastQuality - 0.1 * (agent.rebufferTime - self.input_dict['last_total_rebuf'])
        # option 3. give a fixed penalty if video is stalled
        #           this can reduce the variance in reward signal
        # reward = lastQuality - 10 * ((agent.rebufferTime - self.input_dict['last_total_rebuf']) > 0)

        # option 4. use the metric in SIGCOMM MPC paper
        rebufferTime = float(agent.rebufferTime - agentState.lastTotalRebuf)

        # --linear reward--
        reward = VIDEO_BIT_RATE[lastQuality] / M_IN_K \
                - REBUF_PENALTY * rebufferTime / M_IN_K \
                - SMOOTH_PENALTY * np.abs(VIDEO_BIT_RATE[lastQuality] -
                                          agentState.lastBitrate) / M_IN_K

        # --log reward--
        # log_bit_rate = np.log(VIDEO_BIT_RATE[lastQuality] / float(VIDEO_BIT_RATE[0]))
        # log_last_bit_rate = np.log(self.input_dict['last_bit_rate'] / float(VIDEO_BIT_RATE[0]))

        # reward = log_bit_rate \
        #          - 4.3 * rebufferTime / M_IN_K \
        #          - SMOOTH_PENALTY * np.abs(log_bit_rate - log_last_bit_rate)

        # --hd reward--
        # reward = BITRATE_REWARD[lastQuality] \
        #         - 8 * rebufferTime / M_IN_K - np.abs(BITRATE_REWARD[lastQuality] - BITRATE_REWARD_MAP[self.input_dict['last_bit_rate']])

        agentState.lastBitrate = VIDEO_BIT_RATE[lastQuality]
        agentState.lastTotalRebuf = agent.rebufferTime

        # retrieve previous agentState
        if len(agentState.s_batch) == 0:
            state = [np.zeros((S_INFO, S_LEN))]
        else:
            state = np.array(agentState.s_batch[-1], copy=True)

#         print(state)

        # compute bandwidth measurement
        video_chunk_fetch_time = agent.requests[-1].timeSpent #post_data['lastChunkFinishTime'] - post_data['lastChunkStartTime']
        video_chunk_size = agent.requests[-1].chunkLen

        # compute number of video chunks left
        video_chunk_remain = TOTAL_VIDEO_CHUNKS - agent.segCount

        # dequeue history record
        state = np.roll(state, -1, axis=1)

        next_video_chunk_sizes = []
        for i in range(self.A_DIM):
            next_video_chunk_sizes.append(agent.getChunkSize(i, agent.segCount))

        # this should be S_INFO number of terms
        try:
            state[0, -1] = VIDEO_BIT_RATE[lastQuality] / float(np.max(VIDEO_BIT_RATE))
            state[1, -1] = agent.avaliableBuf / BUFFER_NORM_FACTOR
            state[2, -1] = float(video_chunk_size) / float(video_chunk_fetch_time) / M_IN_K  # kilo byte / ms
            state[3, -1] = float(video_chunk_fetch_time) / M_IN_K / BUFFER_NORM_FACTOR  # 10 sec
            state[4, :self.A_DIM] = np.array(next_video_chunk_sizes) / M_IN_K / M_IN_K  # mega byte
            state[5, -1] = np.minimum(video_chunk_remain, CHUNK_TIL_VIDEO_END_CAP) / float(CHUNK_TIL_VIDEO_END_CAP)
        except ZeroDivisionError:
            # this should occur VERY rarely (1 out of 3000), should be a dash issue
            # in this case we ignore the observation and roll back to an eariler one
            if len(agentState.s_batch) == 0:
                state = [np.zeros((S_INFO, S_LEN))]
            else:
                state = np.array(agentState.s_batch[-1], copy=True)

        # log wall_time, bit_rate, buffer_size, rebufferTime, video_chunk_size, download_time, reward

        action_prob = self.actor.predict(np.reshape(state, (1, S_INFO, S_LEN)))
        action_cumsum = np.cumsum(action_prob)
        bit_rate = (action_cumsum > np.random.randint(1, RAND_RANGE) / float(RAND_RANGE)).argmax()
        # Note: we need to discretize the probability into 1/RAND_RANGE steps,
        # because there is an intrinsic discrepancy in passing single state and batch states
        if minimalState:
            agentState.s_batch = [state]
        else:
            agentState.s_batch += [state]

        cprint.blue("results from pensieve")
        # send data to html side
        return bit_rate, agentState


SOCKET_PATH = "/tmp/pensivesocket"

class AbrHandler(socketserver.BaseRequestHandler):
    def handle(self):
        data = self.request.recv(4, socket.MSG_WAITALL)
        l = struct.unpack("=I", data)[0]
        moreData = self.request.recv(l, socket.MSG_WAITALL)
        vidInfo, agent, agentState, minimalState = pickle.loads(moreData)
        bitrate, state = "78", agentState
        errors = ""
        try:
            bitrate, state = self.server.abrObject.getNextQuality(vidInfo, agent, agentState, minimalState)
        except Exception as err:
            track = tb.format_exc()
            print(track)
            errors = track
            pass

        sndData = pickle.dumps((bitrate, state, errors))
        l = len(sndData)
        dt = struct.pack("=I", l)
        self.request.send(dt)
        self.request.send(sndData)

class AbrServer(socketserver.UnixStreamServer):
    def __init__(self, abrObject, *arg, **kwarg):
        self.abrObject = abrObject
        super().__init__(*arg, **kwarg)


def startServerInFork(a_dim):
    if os.path.exists(SOCKET_PATH):
        os.remove(SOCKET_PATH)
    pipein, pipeout = os.pipe()
    pid = os.fork()
    if pid == 0:
        for fd in list(range(3, 256)): #it is supposed to be a daemon. So, close all other fd
            if fd in [pipein.real, pipeout.real]:
                continue
            try:
                os.close(fd)
            except OSError:
                pass
        abrObject = AbrPensieveClassProc(a_dim)
        with AbrServer(abrObject, SOCKET_PATH, AbrHandler) as server:
            os.write(pipeout, b"da")
            os.close(pipeout)
            os.close(pipein)
            server.serve_forever()
        sys.exit()

    if pid > 0:
        os.read(pipein, 1)
        os.close(pipeout)
        os.close(pipein)

def getNextQualityFromServer(sock, vidInfo, agent, agentState, minimalState=False):
    sndData = pickle.dumps((vidInfo, agent, agentState, minimalState))
    l = len(sndData)
    dt = struct.pack("=I", l)
    sock.send(dt)
    sock.send(sndData)

    dt = sock.recv(4, socket.MSG_WAITALL)
#     print(len(dt))
    l = struct.unpack("=I", dt)[0]
    moreData = sock.recv(l, socket.MSG_WAITALL)
    bitrate, state, errors = pickle.loads(moreData)
    sock.close()

    return bitrate, state, errors

def getNextQuality(vidInfo, agent, minimalState=False):
    agentState = agent.agentState

    if agentState is None or len(agent.requests) == 0:
        agentState = getObj(
                    lastBitrate = 0,
                    lastTotalRebuf = 0,
                    s_batch = [np.zeros((S_INFO, S_LEN))],
                )
        return getObj(
                sleepTime = 0, \
                repId = 0, \
                abrState = agentState, \
                errors = None, \
                )

    connected = False
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    for x in range(3):
        try:
            sock.connect(SOCKET_PATH)
            connected = True
            break
        except FileNotFoundError:
            pass
        except ConnectionRefusedError:
            pass
        startServerInFork(len(vidInfo.bitrates))

    if not connected:
        return getObj(
                sleepTime = -1, \
                repId = 0, \
                abrState = agentState, \
                errors = None, \
                )

#     cprint.green(agentState)
    ql, state, errors = getNextQualityFromServer(sock, vidInfo, agent, agentState, minimalState)
    return getObj(
            sleepTime = -1, \
            repId = ql, \
            abrState = state, \
            errors = errors, \
            )


