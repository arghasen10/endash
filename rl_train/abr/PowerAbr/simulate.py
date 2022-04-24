#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
@Author: somesh

Created on Thu Oct 17 12:37:31 2019
"""

import numpy as np
import pandas as pd
import pickle
from collections import defaultdict, Counter
from time import sleep

from abr.PowerAbr.utils import get_working_data, set_pandas_display
from abr.PowerAbr.constants import hlf_cols_types, working_columns


net_type_enc_loc   = 'pretrainEnDashModel/net_type_enc.pkl'
data_state_enc_loc = 'pretrainEnDashModel/data_state_enc.pkl'
data_act_enc_loc   = 'pretrainEnDashModel/data_act_enc.pkl'
save_model_loc     = 'pretrainEnDashModel/rf_model.sav'
scale_loc          = 'pretrainEnDashModel/rf_model_scaler.pkl'

# kgp_data_loc       = 'pretrainEnDashModel/kgp_tpt_data.csv'
# kol_data_loc       = 'pretrainEnDashModel/kol_tpt_data.csv'
# hlf_data_loc       = 'pretrainEnDashModel/hlf_data.csv'
# lab_enc_data_loc   = 'pretrainEnDashModel/lab_enc_data.csv'
# performance_loc    = 'pretrainEnDashModel/rf_model_performance.csv'
# feature_imp_loc    = 'pretrainEnDashModel/rf_model_feature_imp.csv'



def convert_to_hlf_data_point(data):

    hlf_cols = defaultdict(list)

    for col in data.columns:

       vals_in_window = data[col].values

       if data[col].dtype == 'object':
            # In case the column is str type
            max_occ_in_window  = Counter(vals_in_window).most_common(1)[0][0]
            hlf_name           = col + hlf_cols_types['mode']
            hlf_cols[hlf_name].append(max_occ_in_window)

       else:
            # In case the column is int or float type
            # 1. Mean
            mean_in_window = np.mean(vals_in_window)
            hlf_name       = col + hlf_cols_types['mean']
            hlf_cols[hlf_name].append(mean_in_window)
            # 2. 90 percentile
            pctl_90_val_in_window = np.percentile(vals_in_window, 90)
            hlf_name              = col + hlf_cols_types['90_percentile']
            hlf_cols[hlf_name].append(pctl_90_val_in_window)
            # 3. 75 percentile
            pctl_75_val_in_window = np.percentile(vals_in_window, 75)
            hlf_name              = col + hlf_cols_types['75_percentile']
            hlf_cols[hlf_name].append(pctl_75_val_in_window)
            # 4. Median
            median_val_in_window = np.percentile(vals_in_window, 50)
            hlf_name             = col + hlf_cols_types['median']
            hlf_cols[hlf_name].append(median_val_in_window)
            # 5. 25 percentile
            pctl_25_val_in_window = np.percentile(vals_in_window, 25)
            hlf_name              = col + hlf_cols_types['25_percentile']
            hlf_cols[hlf_name].append(pctl_25_val_in_window)
            # 6. Inter-quartile range
            int_qrt_range_val_in_window = pctl_75_val_in_window - pctl_25_val_in_window
            hlf_name                    = col + hlf_cols_types['inter_qrt_range']
            hlf_cols[hlf_name].append(int_qrt_range_val_in_window)

    return pd.DataFrame(hlf_cols)


def display_output(data, message):
   print ('\n{}\n'.format(message))
   print(data.to_string(index=False))
   print('\n')


def get_label_encoded_data(data, lab_enc_1, lab_enc_2, lab_enc_3):
#     display_output(data, ('The high level feature data point'
#                                 ' derived from input looks like this:'))

    data['net_type_MODE']   = lab_enc_1.transform(data['net_type_MODE'].astype('str'))
    data['data_state_MODE'] = lab_enc_2.transform(data['data_state_MODE'].astype('str'))
    data['data_act_MODE']   = lab_enc_3.transform(data['data_act_MODE'].astype('str'))

    return data

if __name__ ==  '__main__':
   #set_pandas_display()

   model     = pickle.load(open(save_model_loc, 'rb'))
   lab_enc_1 = pickle.load(open(net_type_enc_loc, 'rb'))
   lab_enc_2 = pickle.load(open(data_state_enc_loc, 'rb'))
   lab_enc_3 = pickle.load(open(data_act_enc_loc, 'rb'))
   scale     = pickle.load(open(scale_loc, 'rb'))

   data = pd.read_csv(kgp_data_loc)
   data = data[working_columns]
#    data.rename(columns={'hroughput tcp':'throughput'}, inplace=True)

   start_pointer = 0
   end_pointer   = 20

   while (end_pointer <= len(data)):
      #print("################################################################################################")
      curr_data = data[start_pointer:end_pointer]
      #curr_data = curr_data.reset_index()
      display_output(curr_data, 'The current input looks like this:')
      sleep(1)

      hlf_data = convert_to_hlf_data_point(curr_data)
# =============================================================================
#       display_output(hlf_data, ('The high level feature data point'
#                                 ' derived from input looks like this:'))
#       sleep(2)
# =============================================================================

      enc_data  = get_label_encoded_data(hlf_data, lab_enc_1, lab_enc_2, lab_enc_3)
# =============================================================================
#       display_output(enc_data, 'Label encoded data from high level feature data')
#       sleep(2)
# =============================================================================

      scaled_data = scale.transform(enc_data.values)

      print("\nThe mean throughput for next 12 seconds will be {}\n".format(model.predict(scaled_data)[0]))
      sleep(1)

      start_pointer += 20
      end_pointer   += 20



