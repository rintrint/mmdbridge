from mmdbridge import *
from mmdbridge_vmd import *

# export mode
# 0 = simulated physics bones only
# 1 = all bones except IK, keep 付与親 constraint (MMD compatible, MMD Tools)
# 2 = all bones except IK, bake 付与親 constraint to FK
export_mode = 1

start_frame = get_start_frame()
end_frame = get_end_frame()

framenumber = get_frame_number()
if framenumber == start_frame:
    messagebox("vmd export started")
    start_vmd_export(export_mode)

if framenumber >= start_frame and framenumber <= end_frame:
    execute_vmd_export(framenumber)

if framenumber == end_frame:
    messagebox("vmd export ended at " + str(framenumber))
    end_vmd_export()
