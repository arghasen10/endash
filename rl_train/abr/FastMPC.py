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

from util.calculateMetric import measureQoE

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
LOG_FILE = './results/log_file'

class AbrFastMPC():
    def __init__(self, videoInfo, agent, log_file_path=LOG_FILE, *kw, **kws):
        #=====================================
        # SETUP
        #=====================================
        log_file = None if not log_file_path else open(log_file_path, 'wb')

        s_batch = [np.zeros((S_INFO, S_LEN))]

        last_bit_rate = DEFAULT_QUALITY
        last_total_rebuf = 0
        # need this storage, because observation only contains total rebuffering time
        # we compute the difference to get

        video_chunk_count = 0

        input_dict = {'log_file': log_file,
                      'last_bit_rate': last_bit_rate,
                      'last_total_rebuf': last_total_rebuf,
                      'video_chunk_coount': video_chunk_count,
                      's_batch': s_batch}
        #=====================================
        # INIT
        #=====================================

        self.CHUNK_COMBO_OPTIONS = [combo for combo in itertools.product([0,1,2,3,4,5], repeat=5)]
        self.video = videoInfo
        self.agent = agent

        self.input_dict = input_dict
        self.log_file = input_dict['log_file']
        #self.saver = input_dict['saver']
        self.s_batch = input_dict['s_batch']

    def stopAbr(self):
        if self.log_file:
            self.log_file.close()
            self.log_file = None
        del self.input_dict['log_file']

    def get_chunk_size(self, quality, index):
        if index >= self.video.segmentCount: return 0
        return self.video.fileSizes[quality][index]

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
                }
        return self.getSleepTime(bufferLeft), self.do_POST(post_data)

    def do_POST(self, post_data):
        VIDEO_BIT_RATE = self.video.bitratesKbps
        TOTAL_VIDEO_CHUNKS = self.video.segmentCount
        CHUNK_TIL_VIDEO_END_CAP = TOTAL_VIDEO_CHUNKS
        BITRATE_REWARD = self.video.bitrateReward
        # option 1. reward for just quality
        # reward = post_data['lastquality']
        # option 2. combine reward for quality and rebuffer time
        #           tune up the knob on rebuf to prevent it more
        # reward = post_data['lastquality'] - 0.1 * (post_data['RebufferTime'] - self.input_dict['last_total_rebuf'])
        # option 3. give a fixed penalty if video is stalled
        #           this can reduce the variance in reward signal
        # reward = post_data['lastquality'] - 10 * ((post_data['RebufferTime'] - self.input_dict['last_total_rebuf']) > 0)

        # option 4. use the metric in SIGCOMM MPC paper
        rebuffer_time = float(post_data['RebufferTime'] -self.input_dict['last_total_rebuf'])

        # --linear reward--
        reward = VIDEO_BIT_RATE[post_data['lastquality']] / M_IN_K \
                - REBUF_PENALTY * rebuffer_time / M_IN_K \
                - SMOOTH_PENALTY * np.abs(VIDEO_BIT_RATE[post_data['lastquality']] -
                                          self.input_dict['last_bit_rate']) / M_IN_K

        # --log reward--
        # log_bit_rate = np.log(VIDEO_BIT_RATE[post_data['lastquality']] / float(VIDEO_BIT_RATE[0]))
        # log_last_bit_rate = np.log(self.input_dict['last_bit_rate'] / float(VIDEO_BIT_RATE[0]))

        # reward = log_bit_rate \
        #          - 4.3 * rebuffer_time / M_IN_K \
        #          - SMOOTH_PENALTY * np.abs(log_bit_rate - log_last_bit_rate)

        # --hd reward--
        # reward = BITRATE_REWARD[post_data['lastquality']] \
        #         - 8 * rebuffer_time / M_IN_K - np.abs(BITRATE_REWARD[post_data['lastquality']] - BITRATE_REWARD_MAP[self.input_dict['last_bit_rate']])

        self.input_dict['last_bit_rate'] = VIDEO_BIT_RATE[post_data['lastquality']]
        self.input_dict['last_total_rebuf'] = post_data['RebufferTime']

        # retrieve previous state
        if len(self.s_batch) == 0:
            state = [np.zeros((S_INFO, S_LEN))]
        else:
            state = np.array(self.s_batch[-1], copy=True)

        # compute bandwidth measurement
        video_chunk_fetch_time = post_data['lastChunkFinishTime'] - post_data['lastChunkStartTime']
        video_chunk_size = post_data['lastChunkSize']

        # compute number of video chunks left
        video_chunk_remain = TOTAL_VIDEO_CHUNKS - self.input_dict['video_chunk_coount']
        self.input_dict['video_chunk_coount'] += 1

        # dequeue history record
        state = np.roll(state, -1, axis=1)

        # this should be S_INFO number of terms
        try:
            state[0, -1] = VIDEO_BIT_RATE[post_data['lastquality']] / float(np.max(VIDEO_BIT_RATE))
            state[1, -1] = post_data['buffer'] / BUFFER_NORM_FACTOR
            state[2, -1] = rebuffer_time / M_IN_K
            state[3, -1] = float(video_chunk_size) / float(video_chunk_fetch_time) / M_IN_K  # kilo byte / ms
            state[4, -1] = np.minimum(video_chunk_remain, CHUNK_TIL_VIDEO_END_CAP) / float(CHUNK_TIL_VIDEO_END_CAP)
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

        # pick bitrate according to MPC
        # first get harmonic mean of last 5 bandwidths
        past_bandwidths = state[3,-5:]
        while past_bandwidths[0] == 0.0:
            past_bandwidths = past_bandwidths[1:]
        #if ( len(state) < 5 ):
        #    past_bandwidths = state[3,-len(state):]
        #else:
        #    past_bandwidths = state[3,-5:]
        bandwidth_sum = 0
        for past_val in past_bandwidths:
            bandwidth_sum += (1/float(past_val))
        future_bandwidth = 1.0/(bandwidth_sum/len(past_bandwidths))

        # future chunks length (try 4 if that many remaining)
        last_index = int(post_data['lastRequest'])
        future_chunk_length = MPC_FUTURE_CHUNK_COUNT
        if ( TOTAL_VIDEO_CHUNKS - last_index < 4 ):
            future_chunk_length = TOTAL_VIDEO_CHUNKS - last_index

        # all possible combinations of 5 chunk bitrates (9^5 options)
        # iterate over list and for each, compute reward and store max reward combination
        max_reward = -100000000
        best_combo = ()
        start_buffer = float(post_data['buffer'])
        #start = time.time()
        for full_combo in self.CHUNK_COMBO_OPTIONS:
            combo = full_combo[0:future_chunk_length]
            # calculate total rebuffer time for this combination (start with start_buffer and subtract
            # each download time and add 2 seconds in that order)
            curr_rebuffer_time = 0
            curr_buffer = start_buffer
            bitrate_sum = 0
            smoothness_diffs = 0
            last_quality = int(post_data['lastquality'])
            for position in range(0, len(combo)):
                chunk_quality = combo[position]
                index = last_index + position + 1 # e.g., if last chunk is 3, then first iter is 3+0+1=4
                download_time = (self.get_chunk_size(chunk_quality, index)/1000000.)/future_bandwidth # this is MB/MB/s --> seconds
                if ( curr_buffer < download_time ):
                    curr_rebuffer_time += (download_time - curr_buffer)
                    curr_buffer = 0
                else:
                    curr_buffer -= download_time
                curr_buffer += 4

                # linear reward
                #bitrate_sum += VIDEO_BIT_RATE[chunk_quality]
                #smoothness_diffs += abs(VIDEO_BIT_RATE[chunk_quality] - VIDEO_BIT_RATE[last_quality])

                # log reward
                # log_bit_rate = np.log(VIDEO_BIT_RATE[chunk_quality] / float(VIDEO_BIT_RATE[0]))
                # log_last_bit_rate = np.log(VIDEO_BIT_RATE[last_quality] / float(VIDEO_BIT_RATE[0]))
                # bitrate_sum += log_bit_rate
                # smoothness_diffs += abs(log_bit_rate - log_last_bit_rate)

                # hd reward
                bitrate_sum += BITRATE_REWARD[chunk_quality]
                smoothness_diffs += abs(BITRATE_REWARD[chunk_quality] - BITRATE_REWARD[last_quality])

                last_quality = chunk_quality
            # compute reward for this combination (one reward per 5-chunk combo)
            # bitrates are in Mbits/s, rebuffer in seconds, and smoothness_diffs in Mbits/s

            # linear reward
            #reward = (bitrate_sum/1000.) - (4.3*curr_rebuffer_time) - (smoothness_diffs/1000.)

            # log reward
            # reward = (bitrate_sum) - (4.3*curr_rebuffer_time) - (smoothness_diffs)

            # hd reward
            reward = bitrate_sum - (8*curr_rebuffer_time) - (smoothness_diffs)

            if ( reward > max_reward ):
                max_reward = reward
                best_combo = combo

        return best_combo[0]
