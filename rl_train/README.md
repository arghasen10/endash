## Emulation Tool for the Reinforcement Learning based Training
For training the RL buffer engine and the bitrate decision engine we have leveraged a python based emulator that mimics a video client emulation tool. First we capture the real world video traces and the time informations of the video playback is used by the emulator to mimic a video client application.

### Start the emulator:
```python
    python3.7 rl_train/simenv/PowerEnv.py
```

To start the emulation you need to have the video traces inside `train_sim_traces` directory.
