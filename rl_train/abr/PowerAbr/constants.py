#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
@Author: somesh

Created on Fri Sep 20 12:12:19 2019
"""
############## General imports ###############################################
from sklearn.ensemble import RandomForestRegressor
############# Package imports ################################################
##############################################################################

# The columns we want in our raw data.
needed_columns = {'location_data': ['latitude', 'longitude', 'speed'],
                  'network_data' : ['gsm_neighbors', 'umts_neighbors', 'lte_neighbors',
                                    'rssi_strongest', 'net_type', 'data_state', 'data_act',
                                    'mnc', 'mcc', 'node_id'],
                  'tpt_data'     : ['Throughput tcp']}

# The data columns we want to work with.
working_columns = ['speed', 'gsm_neighbors', 'umts_neighbors', 'lte_neighbors',
                   'rssi_strongest', 'net_type', 'data_state', 'data_act',
                   'throughput']

# The location specific columns
location_columns = ['latitude', 'longitude', 'mcc', 'mnc', 'node_id']

# Types of high level features.
hlf_cols_types = {"mode"           : "_MODE",
                  "mean"           : "_MEAN",
                  "90_percentile"  : "_90_PERCENTILE",
                  "75_percentile"  : "_75_PERCENTILE",
                  "median"         : "_MEDIAN",
                  "25_percentile"  : "_25_PERCENTILE",
                  "inter_qrt_range": "_INTER_QUARTILE_RANGE"}

# Data source location
data_sources = {'kgp': {'primary'   : 'kgp_data/kgp_tpt_data.csv',
                        'raw'       : 'kgp_data/kgp_raw_data.csv'},
                'kol': {'primary'   : 'kol_data/kol_tpt_data.csv',
                        'raw'       : 'kol_data/kol_raw_data.csv',
                        'location'  : 'kol_data/location_speed_data.csv',
                        'network'   : 'kol_data/network_data.csv',
                        'throughput': 'kol_data/throughput_data.csv'}}

# History window size
history = 20

# Prediction window size
horizon = 12

# Models
models = {'rf': RandomForestRegressor(n_estimators=100, random_state=0)}
