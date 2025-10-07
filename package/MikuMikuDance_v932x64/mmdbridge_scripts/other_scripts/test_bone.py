import mmdbridge
from mmdbridge import *

import os
import io
import math
from math import *
import time
import codecs
import sys


def test():
    for obj_index in range(get_object_size()):
        messagebox(get_object_filename(obj_index).encode("cp932"))
        bone_count = get_bone_size(obj_index)
        messagebox("bone count:" + str(bone_count))
        for bone_index in range(bone_count):
            messagebox(get_bone_name(obj_index, bone_index).encode("cp932"))

        messagebox("bone mat:" + str(get_bone_matrix(obj_index, 0)))


test()
