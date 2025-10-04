import os
import shutil
from collections import namedtuple

from mmdbridge import *
from mmdbridge_vmd import *


# =================================== settings ===================================


# --- Bone Animation Settings ---
# Export FK bone animation
export_fk_bone_animation = True
# FK bone animation export mode:
# 0 = Simulated physics bones only
# 1 = All FK bones, keep 付与親 constraint (for MMD, MMD Tools)
# 2 = All FK bones, bake 付与親 constraint to FK
export_fk_bone_animation_mode = 1

# Export IK bone animation
export_ik_bone_animation = False

# Add a keyframe at the start to turn off all IK chains.
add_turn_off_ik_keyframe = True


# --- Morph Animation Settings ---
# Export morph animation
export_morph_animation = True


# --- General Settings ---
# destination directory (eg. "C:/Users/user/Desktop/")
dst_dir = "out"


# ================================================================================


FileSnapshot = namedtuple("FileSnapshot", ["path", "mtime"])


def get_file_snapshots(directory):
    """Get snapshot set of all files in directory"""
    file_snapshots = set()
    if not os.path.exists(directory):
        return file_snapshots

    for filename in os.listdir(directory):
        filepath = os.path.join(directory, filename)
        if os.path.isfile(filepath):
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

# Check if there is anything to export
is_anything_to_export = export_fk_bone_animation or export_ik_bone_animation or export_morph_animation

if is_anything_to_export:
    if framenumber == start_frame:
        messagebox("VMD export started.")
        start_vmd_export(
            export_fk_bone_animation_mode=export_fk_bone_animation_mode if export_fk_bone_animation else -1,  # Pass -1 if FK is off
            export_ik_bone_animation=export_ik_bone_animation,
            add_turn_off_ik_keyframe=add_turn_off_ik_keyframe,
            export_morph_animation=export_morph_animation,
        )

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
else:
    if framenumber == start_frame:
        messagebox("VMD export skipped: All export options are disabled.")
