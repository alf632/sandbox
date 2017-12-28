#/usr/bin/python

import os,sys
from pylib import clients
import clib_interface
import server
import threading
from PIL import Image
from Queue import Queue
from time import sleep, time

# set client lists
kinects=[]
beamer =[]

# start webserver
## init picture_queue
pqueue=Queue(maxsize=1)
## init command_queue
cqueue=Queue()
server=server.sandcontrol(pqueue=pqueue, cqueue=cqueue, kinects, beamer)
serverd=threading.Thread(target = server.launch_control, args=(pqueue,))
serverd.daemon = True
serverd.start()

# set simulation defaults
height_shift = 0
height_scale = 1
screen_resolution_x = 1920
screen_resolution_y = 1080
map_offset_x = 0.0
map_offset_y = 0.0
map_zoom = 1.0

# initiate simulation
clib_interface.set_height_config(height_shift, height_scale)
clib_interface.reset_map_drag()
clib_interface.drag_map(map_offset_x, map_offset_y)

# prepare target pic
cimg = Image.new("L", (screen_resolution_x,screen_resolution_y), color=0)

while True:

    # handle client requests
    if not cqueue.empty():
        data=cqueue.get(block=False)
        if data["state"] == "init":
            if data["type"] == "kinect":
                new_kinect=clients.kinect_client(id=len(kinects), data=data)
                kinects.append(new_kinect)
            elif request["type"] == "beamer":
                new_beamer=clients.beamer_client(id=len(beamer), data=data)
                beamer.append(new_beamer)
    else:
        sleep(1)
