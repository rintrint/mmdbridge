import os
import shutil
from collections import namedtuple

from mmdbridge import *
from mmdbridge_vmd import *

# =================================== settings ===================================

# export mode
# 0 = simulated physics bones only
# 1 = all bones except IK, keep 付与親 constraint (MMD compatible, MMD Tools)
# 2 = all bones except IK, bake 付与親 constraint to FK
# Note: All modes will export morph
export_mode = 2

# destination directory (eg. "C:/Users/user/Desktop/")
dst_dir = "out"

# ================================================================================

FileSnapshot = namedtuple("FileSnapshot", ["path", "mtime"])


def get_file_snapshots(directory, extension=".vmd"):
    """Get snapshot set of files in directory"""
    file_snapshots = set()
    if not os.path.exists(directory):
        return file_snapshots

    for filename in os.listdir(directory):
        if filename.endswith(extension):
            filepath = os.path.join(directory, filename)
            mtime = os.path.getmtime(filepath)
            file_snapshots.add(FileSnapshot(filepath, mtime))
    return file_snapshots


def move_files(filepaths, dst_dir):
    """Move files to destination directory"""
    filepaths = set(filepaths)
    for filepath in filepaths:
        shutil.move(filepath, os.path.join(dst_dir, os.path.basename(filepath)))


start_frame = get_start_frame()
end_frame = get_end_frame()

framenumber = get_frame_number()
if framenumber == start_frame:
    messagebox("VMD export started.")
    start_vmd_export(export_mode)

if start_frame <= framenumber <= end_frame:
    execute_vmd_export(framenumber)

if framenumber == end_frame:
    mmd_dir = os.path.normpath(get_base_path())
    out_dir = os.path.join(mmd_dir, "out")

    final_dst_dir = dst_dir
    if not os.path.isabs(final_dst_dir):
        final_dst_dir = os.path.join(mmd_dir, final_dst_dir)
    final_dst_dir = os.path.normpath(final_dst_dir)
    os.makedirs(final_dst_dir, exist_ok=True)

    initial_file_snapshots = get_file_snapshots(out_dir)
    end_vmd_export()
    modified_file_snapshots = get_file_snapshots(out_dir) - initial_file_snapshots
    modified_files = {snapshot.path for snapshot in modified_file_snapshots}
    move_files(modified_files, final_dst_dir)

    message = "VMD export ended at frame " + str(framenumber) + "."
    message += "\n"
    message += "\nExported to:"
    message += "\n" + final_dst_dir
    message += "\n"
    message += "\n" + "Total: " + str(len(modified_files)) + " file(s)"
    messagebox(message)
