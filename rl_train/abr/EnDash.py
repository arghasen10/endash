import numpy as np
import pandas as pd
import pickle
import socketserver
import socket
import sys
import os
import struct
import json
from collections import defaultdict, Counter
from time import sleep
import traceback as tb

# from util.nestedObject import getObj
from util import cprint

from abr import BOLA

from abr.PowerAbr.utils import get_working_data, set_pandas_display
from abr.PowerAbr.constants import hlf_cols_types, working_columns
from abr.PowerAbr.simulate import convert_to_hlf_data_point, get_label_encoded_data

BYTES_IN_MB = 1000000.0

BUFFER_ACTION = [-2, -1, 0, +1, +2]

net_type_enc_loc   = 'pretrainEnDashModel/net_type_enc.pkl'
data_state_enc_loc = 'pretrainEnDashModel/data_state_enc.pkl'
data_act_enc_loc   = 'pretrainEnDashModel/data_act_enc.pkl'
save_model_loc     = 'pretrainEnDashModel/rf_model.sav'
scale_loc          = 'pretrainEnDashModel/rf_model_scaler.pkl'


BUFFER_ACTION = [-2, -1, 0, +1, +2]

CMD_DATA = 0
CMD_EXIT = 1

class AbrEnDashSep:
    def __init__(self):
        self.model     = pickle.load(open(save_model_loc, 'rb'))
        self.lab_enc_1 = pickle.load(open(net_type_enc_loc, 'rb'))
        self.lab_enc_2 = pickle.load(open(data_state_enc_loc, 'rb'))
        self.lab_enc_3 = pickle.load(open(data_act_enc_loc, 'rb'))
        self.scale     = pickle.load(open(scale_loc, 'rb'))
        self.bufferInterval = int(os.environ['LOW_POW_ENV_BUFFER_INTERVAL'])

        self.modelPath = "./abr/PowerAbr/ResModel/"

        readOnly = True

    def predict_throughput(self, curr_data):
        if len(curr_data) != self.bufferInterval:
            raise Exception("Data len have to be 20")
        assert type(curr_data) == pd.DataFrame
        curr_data = curr_data[working_columns]
        curr_data.rename(columns={'Throughput tcp':'throughput'}, inplace=True)
        hlf_data = convert_to_hlf_data_point(curr_data)
        enc_data = get_label_encoded_data(hlf_data, self.lab_enc_1, self.lab_enc_2, self.lab_enc_3)
        scaled_data = self.scale.transform(enc_data.values)
        return self.model.predict(scaled_data)[0]

class AbrEnDashCls:
    def __init__(self):
        self.pid = 0
        self.bufferInterval = int(os.environ['LOW_POW_ENV_BUFFER_INTERVAL'])

    def startServer(self):
        self.sock, sock = socket.socketpair()
        self.pid = os.fork()
        if self.pid == 0:
            obj = AbrEnDashSep()

            print("Sending conf")
            sock.send(b"\0")
            print("Sent conf")

            while True:

                dt = sock.recv(4, socket.MSG_WAITALL)
                l = struct.unpack("=I", dt)[0]
                moreData = sock.recv(l, socket.MSG_WAITALL)
                cmd, curr_data = pickle.loads(moreData)

                if cmd == CMD_DATA:
                    retObj = obj.predict_throughput(curr_data)
                    sndData = pickle.dumps(retObj)
                    l = len(sndData)
                    dt = struct.pack("=I", l)
                    sock.send(dt)
                    sock.send(sndData)
                elif cmd == CMD_EXIT:
                    exit(0)
            exit()

        if self.pid <= 0:
            print("error with fork")
            exit(1)


        print("WAITING AT MAIN")
        dt = self.sock.recv(1, socket.MSG_WAITALL)
        if dt[0] != 0:
            print("error in server")
            exit(2)

    def predict_throughput(self, curr_data):
        if len(curr_data) != self.bufferInterval:
            raise Exception("Data len have to be 20")
        assert type(curr_data) == pd.DataFrame and self.pid > 0

        curr_data.rename(columns={'Throughput tcp':'throughput'}, inplace=True)
        sndData = pickle.dumps((CMD_DATA, curr_data))
        l = len(sndData)
        dt = struct.pack("=I", l)
        self.sock.send(dt)
        self.sock.send(sndData)

        dt = self.sock.recv(4, socket.MSG_WAITALL)
        l = struct.unpack("=I", dt)[0]
        moreData = self.sock.recv(l, socket.MSG_WAITALL)
        retObj = pickle.loads(moreData)
        return retObj

    def killProc(self):
        if self.pid == 0:
            return
        sndData = pickle.dumps((CMD_EXIT, None))
        l = len(sndData)
        dt = struct.pack("=I", l)
        self.sock.send(dt)
        sock.send(sndData)

        os.waitpid(self.pid, 0)
        self.pid = 0
