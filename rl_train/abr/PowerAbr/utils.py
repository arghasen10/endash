#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
@Author: somesh

Created on Fri Sep 20 12:15:45 2019
"""
############## General imports ###############################################
from sklearn.compose import ColumnTransformer
from sklearn.preprocessing import OneHotEncoder, LabelEncoder, StandardScaler
from sklearn.model_selection import train_test_split, KFold, cross_val_score
from collections import defaultdict, Counter
import pandas as pd
import numpy as np
import pickle
############# Package imports ################################################
from abr.PowerAbr.constants import working_columns, hlf_cols_types
##############################################################################

def set_pandas_display():
   """Set display parameters for pandas for better view."""
   pd.set_option('display.max_rows', 20000)
   pd.set_option('display.max_columns', 1000)
##############################################################################

def combine_data(loc_data, net_data, tpt_data, filename, sync_col='Time'):
   """
   Iteratively combine data based on C{sync_col}.

     - Iteratively combine C{loc_data}, C{net_data} and C{tpt_data}.
     - Save the combined data at location specified by C{filename}.
   """
   col_indices_loc = {col: np.where(loc_data.columns.values == col)[0][0] for col in loc_data.columns.values}
   col_indices_net = {col: np.where(net_data.columns.values == col)[0][0] for col in net_data.columns.values}
   col_indices_tpt = {col: np.where(tpt_data.columns.values == col)[0][0] for col in tpt_data.columns.values}
   tpt_data_times  = tpt_data[sync_col].values
   final_data      = defaultdict(list)

   for time in tpt_data_times:
       #if time in loc_data_times:
       loc_time_data = loc_data.loc[loc_data[sync_col] == time].values
       net_time_data = net_data.loc[net_data[sync_col] == time].values
       tpt_time_data = tpt_data.loc[tpt_data[sync_col] == time].values

       if len(loc_time_data) and len(net_time_data) and len(tpt_time_data):
           print("Combining data for {}: {}".format(sync_col, time))
           loc_time_data = loc_time_data[0]
           net_time_data = net_time_data[0]
           tpt_time_data = tpt_time_data[0]

           final_data[sync_col].append(time)

           for col, index in col_indices_loc.items():
               if col != sync_col:
                  final_data[col].append(loc_time_data[index])

           for col, index in col_indices_net.items():
               if col != sync_col:
                   final_data[col].append(net_time_data[index])

           for col, index in col_indices_tpt.items():
               if col != sync_col:
                   final_data[col].append(tpt_time_data[index])

   data = pd.DataFrame(final_data)
   data.to_csv(filename)
###############################################################################

def get_data_in_time_interval(data, start_time, end_time, needed_cols, interval_col='Time'):
   """To slice out the C{data} between C{start_time} and C{end_time}."""
   needed_cols.append(interval_col)
   interval_data = {col:[] for col in needed_cols}
   col_indices   = {col: np.where(data.columns.values == col)[0][0] for col in needed_cols}
   flag          = 0

   for row in data.values:
       if row[col_indices[interval_col]] == start_time:
           flag = 1
       if flag == 0:
           continue
       if flag == 1:
           for col in needed_cols:
               interval_data[col].append(row[col_indices[col]])
       if row[col_indices[interval_col]] == end_time:
           flag = 0

   return pd.DataFrame(interval_data)
###############################################################################

def rename_columns(data, renames):
   """Rename columns in C{data}, according to the dict C{renames}."""
   return data.rename(columns=renames, inplace=True)
###############################################################################

def get_working_data(filename):
    """Get working data from csv C{filename}."""
    data          = pd.read_csv(filename)
    data          = data[working_columns]
    data.rename(columns={'Throughput tcp':'throughput'}, inplace=True)

    return data
##############################################################################

def save_model(model, location):
   """Save C{model} at given C{location}."""
   location = 'saved_models/' + location + '.sav'
   pickle.dump(model, open(location, 'wb'))
   print('\nModel saved successfully at {}\n'.format(location))
##############################################################################

def load_model(location):
   """Load model from C{location}."""
   location = 'saved_models/' + location + '.sav'
   print('\nLoading saved model from {}\n'.format(location))

   return pickle.load(open(location, 'rb'))
##############################################################################

def save_output(output_dict, location):
   """Save C{output_dict} as .csv at C{location}."""
   out = pd.DataFrame(output_dict, index=[0])
   out.to_csv('outputs/' + location + '.csv')
##############################################################################

def get_data_with_high_level_features(data, window_size=5, pred_win_size=3):
    """
    Extract high-level features to construct a more meaningful dataset.

    @params
      - C{data} is the low-level feature data.
      - C{window_size} is the number of time-steps for which high level features are extracted at a given time.
      - C{pred_win_size} is the number of time-steps in the future for which you want to predict the average result.
    @return
      - A pandas DataFrame containing high-level feature data.
    """
    window_start   = 0
    window_end     = window_start + window_size
    pred_win_end   = window_end + pred_win_size
    hlf_cols       = defaultdict(list)

    while(pred_win_end <= data.shape[0]):

        for col in data.columns:
            vals_in_window = data[col][window_start:window_end].values

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

        hlf_cols['predicted_throughput'].append(np.mean(data['throughput'][window_end:pred_win_end].values))

        window_start += 1 # this may be kept equal to pred_win_size, sicne prediction has already been made for that time.
        window_end    = window_start + window_size
        pred_win_end  = window_end + pred_win_size

    return pd.DataFrame(hlf_cols)
##############################################################################

def get_label_encoded_data(data):
    """
    Perform pre-processing on the data.

      - Performing LabelEncoding since OneHotEncoding leads to different number
        of columns for each dataset, based on the categories, and thus the model
        cannot be used across datasets.

    @params
      - C{data} is the data containing information on high-level features.
    @return
      - A pandas DataFrame containing pre-processed data.
      """
    categorical_cols = [col for col in data.columns if data[col].dtype == 'object']

    for col in categorical_cols:
        lab_enc = LabelEncoder()
        data[col] = lab_enc.fit_transform(data[col].astype('str'))

    return data
##############################################################################

def get_one_hot_encoded_data(data):
    """
    Perform pre-processing on the data.

      - OneHotEncoding for categorical features.
      - Restore column names after encoding (since encoding gives numerical column names).
      - Remove one dummy column for each categorical variable to prevent dummy variable trap.

    @params
      - C{data} is the data containing information on high-level features.
    @return
      - A pandas DataFrame containing pre-processed data.
      """
    categorical_cols = [col for col in data.columns if data[col].dtype == 'object']
    normal_cols      = [col for col in data.columns if col not in categorical_cols]
    num_categories   = {col: len(np.unique(data[col].values)) for col in categorical_cols}
    # Perform OneHotEncoding for categorical columns
    col_tr = ColumnTransformer([("Encoder", OneHotEncoder(), categorical_cols)], remainder='passthrough')
    data   = col_tr.fit_transform(data)
    data   = pd.DataFrame(data)
    # Column names need to be mapped again (since they are numerical now due to encoding)
    # Mapping categorical columns (which have dummy columns now due to encoding)
    col_num = 0
    for col in categorical_cols:
        categories = num_categories[col]

        for dummy_col in range(categories):
            col_name = col + '_' + str(dummy_col)
            data.rename(columns={col_num: col_name}, inplace=True)
            col_num += 1
    # Mapping the non-categorical columns
    for col in normal_cols:
        data.rename(columns={col_num: col}, inplace=True)
        col_num += 1
    # Remove one column of each categorical variable to prevent dummy variable trap
    for col in categorical_cols:
        data = data.drop(col + '_0', axis=1)

    return data
##############################################################################

def get_standard_scaled_training_and_testing_data(data, feature_cols, result_col, test_size=0.2):
    """
    Obtain training and testing data after performing normalization(scaling).

    @params
      - C{data} is the pre-processed data.
      - C{feature_cols} contains a list of features.
      - C{result_col} contains the label/output.
      - C{test_size} is the percentage of C{data} to be used for testing.
    @return
      - A tuple of training and testing data segregated into features and labels.
    """
    y = data[result_col].values
    X = data[feature_cols].values
    # Split the data into training and testing sets
    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=test_size, shuffle=True)
    # Perform normalization (scaling) on the data
    sc      = StandardScaler().fit(X_train)
    X_train = sc.transform(X_train)
    X_test  = sc.transform(X_test)

    return X_train, X_test, y_train, y_test
##############################################################################

def get_cross_validation_score(data, feature_cols, result_col, model, num_folds=10):
    """
    Perform cross-validation to check if the model is getting overfitted.
    """
    y          = data[result_col].values
    X          = data[feature_cols].values
    X          = StandardScaler().fit_transform(X)
    k_fold     = KFold(n_splits=num_folds, shuffle=True, random_state=0)
    val_scores = cross_val_score(estimator=model, X=X, y=y, cv=k_fold, verbose=1)

    return val_scores
##############################################################################
