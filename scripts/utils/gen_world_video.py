"""
gen_video.py

This script generates a video file from data stored in a specified experiment directory. 
It uses an ExpReader object to read the data.

Author: Nimesh Nagururu
"""

from argparse import ArgumentParser
from pathlib import Path
from collections import OrderedDict
from natsort import natsorted
import numpy as np
import pandas as pd
from exp_reader import ExpReader 
import cv2
import os

def gen_video(exp_dir):
    """
    Generate a video file from data stored in the specified experiment directory.

    Args:
        exp_dir (Path): Path to the experiment directory. Expects Path object.

    Returns:
        None
    """
    if (exp_dir / '000').exists():
        output_vid_f = exp_dir / ('000/' + 'world.mp4')
        output_timestamps_f = exp_dir / ('000/' +'world_timestamps.npy')
    else:
        output_vid_f = exp_dir / 'world.mp4'
        output_timestamps_f = exp_dir / 'world_timestamps.npy'
    
    reader = ExpReader(exp_dir, verbose = True, ignore_keys = ['depth', 'r_img', 'segm'])
    od = reader._data

    world_timestamps = od['data']['time']

    print(len(world_timestamps))
    # Recording frame rate
    frate = 60

    time_diffs = np.diff(world_timestamps)
    frames_per_timestamp = (time_diffs * frate).round().astype(int)  # Calculate number of frames to replicate
    upsampled_timestamps = []

    current_timestamp = world_timestamps[0]
    for i in range(len(time_diffs)):
        interval_timestamps = np.linspace(current_timestamp, 
                                        current_timestamp + time_diffs[i], 
                                        frames_per_timestamp[i], 
                                        endpoint=False)
        upsampled_timestamps.extend(interval_timestamps)
        current_timestamp += time_diffs[i]

    print(f"Timestamps saved to: {output_timestamps_f}")
    np.save(output_timestamps_f, np.array(upsampled_timestamps))


def main():
    # parser = ArgumentParser()
    # parser.add_argument("--exp_csv", 
    #                         action="store", 
    #                         dest="exp_csv", 
    #                         help="Specify experiments directory", 
    #                         default = '/Users/nimeshnagururu/Documents/tb_skills_analysis/data/SDF_UserStudy_Data/exp_dirs.csv')
    
    # args = parser.parse_args()
    # csv = pd.read_csv(args.exp_csv)

    # exp = list(csv['exp_dir'])
    # for e in exp:
    #     e = Path(e)
    #     if not (e / '000/world.mp4').exists() and not (e / 'world.mp4').exists():
    #         gen_video(Path(e))
    
    # gen_video(Path('/Users/nimeshnagururu/Documents/tb_skills_analysis/data/SDF_UserStudy_Data/Participant_9/2023-02-10 09:45:37_anatE_haptic_P9T5'))
    # gen_video(Path("/home/amunawa2/UserStudy_24_25/Oren/Test_1_New_Calibration_3_min"))
    # gen_video(Path("/home/amunawa2/UserStudy_24_25/jan_16_jonathan/2025-01-16_11:31:24"))
    # gen_video(Path("/home/amunawa2/nasbs_2025/pc sm50m 2/2025-02-07_15:35:43"))
    # gen_video(Path("/home/amunawa2/UserStudy_24_25/test_world_video_2_22/2025-02-22_15:41:21"))
    # gen_video(Path("/home/amunawa2/UserStudy_24_25/mar_8_jonathan/2025-03-08_08:37:17"))
    # gen_video(Path("/home/amunawa2/UserStudy_24_25/mar_8_jonathan/2025-03-08_09:40:31"))
    gen_video(Path("/home/amunawa2/UserStudy_24_25/mar_21_jonathan_smooth/2025-03-21_13:28:42"))
    
if __name__ == "__main__":
	main()