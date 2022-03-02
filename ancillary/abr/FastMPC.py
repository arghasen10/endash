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
from util.nestedObject import getObj
from util.exceptions import ChunkIndexError

# from util.calculateMetric import measureQoE

######################## FAST MPC #######################

S_INFO = 5  # bit_rate, buffer_size, rebuffering_time, bandwidth_measurement, chunk_til_video_end
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
SUMMARY_DIR = './results'

CHUNK_COMBO_OPTIONS = [combo for combo in itertools.product([0,1,2,3,4,5], repeat=5)]

def getChunkSize(agent, chunk_quality, index):
    try:
        return agent.getChunkSize(chunk_quality, index)
    except ChunkIndexError:
        return 0

def getNextQualityUsingFast(vidInfo, agent, agentState, minimalState=False):
    if len(agent.requests) <= 0:
        return 0, agentState

    agentState.segId += 1

    lastQuality = agent.requests[-1]["qualityIndex"]

    VIDEO_BIT_RATE = vidInfo.bitratesKbps
    TOTAL_VIDEO_CHUNKS = vidInfo.segmentCount
    CHUNK_TIL_VIDEO_END_CAP = TOTAL_VIDEO_CHUNKS


    rebufferTime = float(agent.rebufferTime - agentState.lastTotalRebuf)

    # --linear reward--
    reward = VIDEO_BIT_RATE[lastQuality] / M_IN_K \
            - REBUF_PENALTY * rebufferTime / M_IN_K \
            - SMOOTH_PENALTY * np.abs(VIDEO_BIT_RATE[lastQuality] -
                                      agentState.lastBitrate) / M_IN_K

    agentState.lastBitrate = VIDEO_BIT_RATE[lastQuality]
    agentState.lastTotalRebuf = agent.rebufferTime


    # compute bandwidth measurement
    video_chunk_fetch_time = agent.requests[-1].timeSpent #post_data['lastChunkFinishTime'] - post_data['lastChunkStartTime']
    video_chunk_size = agent.requests[-1].chunkLen

    # compute number of video chunks left
    video_chunk_remain = TOTAL_VIDEO_CHUNKS - agent.segCount

    past_bandwidths = [x.throughput/1000000/8 for x in agent.requests[-5:]]#tate[3,-5:] #TODO
    while past_bandwidths[0] == 0.0:
        past_bandwidths = past_bandwidths[1:]

    bandwidth_sum = 0
    for past_val in past_bandwidths:
        bandwidth_sum += (1/float(past_val))
    future_bandwidth = 1.0/(bandwidth_sum/len(past_bandwidths))

    # future chunks length (try 4 if that many remaining)
    last_index = agent.requests[-1].segId
    future_chunk_length = MPC_FUTURE_CHUNK_COUNT
    if ( TOTAL_VIDEO_CHUNKS - last_index < 4 ):
        future_chunk_length = TOTAL_VIDEO_CHUNKS - last_index

    # all possible combinations of 5 chunk bitrates (9^5 options)
    # iterate over list and for each, compute reward and store max reward combination
    max_reward = -100000000
    best_combo = ()
    start_buffer = float(agent.avaliableBuf)
    segDur = agent.segDur/1000
    #start = time.time()
    for full_combo in CHUNK_COMBO_OPTIONS:
        combo = full_combo[0:future_chunk_length]
        # calculate total rebuffer time for this combination (start with start_buffer and subtract
        # each download time and add 2 seconds in that order)
        curr_rebuffer_time = 0
        curr_buffer = start_buffer
        bitrate_sum = 0
        smoothness_diffs = 0
        for position in range(0, len(combo)):
            chunk_quality = combo[position]
            index = last_index + position + 1 # e.g., if last chunk is 3, then first iter is 3+0+1=4
            download_time = (getChunkSize(agent, chunk_quality, index)/1000000.)/future_bandwidth # this is MB/MB/s --> seconds
            if ( curr_buffer < download_time ):
                curr_rebuffer_time += (download_time - curr_buffer)
                curr_buffer = 0
            else:
                curr_buffer -= download_time
            curr_buffer += segDur

            # linear reward
            bitrate_sum += VIDEO_BIT_RATE[chunk_quality]
            smoothness_diffs += abs(VIDEO_BIT_RATE[chunk_quality] - VIDEO_BIT_RATE[lastQuality])

            lastQuality = chunk_quality

        # linear reward
        reward = (bitrate_sum/1000.) - (4.3*curr_rebuffer_time) - (smoothness_diffs/1000.)


        if ( reward > max_reward ):
            max_reward = reward
            best_combo = combo
#         print("")
    if len(best_combo) == 0:
        best_combo = (0,)
#     if agentState.segId < 2:
#         return 0, agentState, None
    return best_combo[0], agentState, None

def getNextQuality(vidInfo, agent, minimalState=False):
    agentState = agent.agentState

    if agentState is None or len(agent.requests) == 0:
        agentState = getObj(
                    lastBitrate = 0,
                    lastTotalRebuf = 0,
                    segId = 0,
                )
        return getObj(
                sleepTime = 0, \
                repId = 0, \
                abrState = agentState, \
                errors = None, \
                )

    ql, state, errors = getNextQualityUsingFast(vidInfo, agent, agentState, minimalState)
    return getObj(
            sleepTime = -1, \
            repId = ql, \
            abrState = state, \
            errors = errors, \
            )

