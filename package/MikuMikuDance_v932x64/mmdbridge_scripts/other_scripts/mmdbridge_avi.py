import mmdbridge
from mmdbridge import *
import mmdbridge_avi
from mmdbridge_avi import *
import os
import math
from math import *
import time

# export mode
# 0 = physics + ik
# 1 = physics only
# 2 = all (buggy)
export_mode = 0

outpath = get_base_path().replace("\\", "/") + "out/"
texture_export_dir = outpath

# MMD uses 30 fps as its base and calculates interpolated frames.
# MMD also exports the interpolated frames between the end frame and end frame + 1.
mmd_start_frame = get_start_frame()
mmd_end_frame = get_end_frame()

target_fps = get_export_fps()
mmd_base_fps = 30.0
ratio = target_fps / mmd_base_fps

start_frame = int(mmd_start_frame * ratio)
end_frame = int((mmd_end_frame + 1) * ratio) - 1

framenumber = get_frame_number()
if framenumber == start_frame:
    messagebox("avi export started.")
    start_avi_export("", export_mode)

if start_frame <= framenumber <= end_frame:
    execute_avi_export(framenumber)

if framenumber == end_frame:
    messagebox("avi export ended.")
    end_avi_export()
