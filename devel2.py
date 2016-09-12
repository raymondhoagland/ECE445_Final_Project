import subprocess
import math
import time
import select
import json
import re
import sys
import os
from threading  import Thread
from Queue import Queue, Empty

# CONSTANTS
HOLD_TIME = .149999999999999994
SLEEP_TIME = .5
M_FT_CONV = 3.28084
FLYING_ALTITUDE = 30 #ft
FENCE_OUTPUT_FILE = "./fence.txt"
LAT_LON_DIV = 10000000.0
LATITUDE_CONV = 10000.0/90.0*3280.4
LONGITUDE_CONV = 364537.402
EMAIL_ADDRESS = None
THRESHOLD = .75
UPPER = 255
OPTIONS = [65, 80]

# Processes/Pipes
proc_proxy = None
proc_cv = None
proc_ci = None
proc_GPIO = None
proc_APP = None
pipes_proxy = None
pipes_cv = None
pipes_ci = None
pipes_GPIO = None
pipes_APP = None
t_proxy = None
t_ci = None
t_cv = None
t_GPIO = None
t_APP = None
q_proxy = None
q_ci = None
q_cv = None
q_GPIO = None
q_APP = None

# Other variables
DEBUG = True
is_confirmed = False
gpio_high = False
should_terminate = False

# Queueing
def enqueue_output(out, queue):
    for line in iter(out.readline, b''):
        queue.put(line)
    out.close()

def setup():
    global proc_proxy, proc_cv, proc_ci, proc_GPIO, proc_APP
    global pipes_proxy, pipes_cv, pipes_ci, pipes_GPIO, pipes_APP
    global DEBUG
    global t_proxy, t_cv, t_ci, t_GPIO, t_APP
    global  q_proxy, q_cv, q_ci, q_GPIO, q_APP

    # for now assuming SITL device
    if DEBUG:
        '''
        dronekit-sitl copter
        '''
        proc_proxy = subprocess.Popen(["exec mavproxy.py --master=tcp:127.0.0.1:5760 --map"], stdout=subprocess.PIPE, stdin=subprocess.PIPE, shell=True)
    else:
        proc_proxy = subprocess.Popen(["exec mavproxy.py --map"], stdout=subprocess.PIPE, stdin=subprocess.PIPE, shell=True)
    pipes_proxy = [proc_proxy.stdin, proc_proxy.stdout]
    # queueing idea borrowed from http://stackoverflow.com/questions/375427/non-blocking-read-on-a-subprocess-pipe-in-python
    q_proxy = Queue()
    t_proxy = Thread(target=enqueue_output, args=(pipes_proxy[1], q_proxy))
    t_proxy.daemon = True
    t_proxy.start()

    proc_cv = subprocess.Popen(["exec bash"], stdout=subprocess.PIPE, stdin=subprocess.PIPE, shell=True)
    pipes_cv = [proc_cv.stdin, proc_cv.stdout]
    q_cv = Queue()
    t_cv = Thread(target=enqueue_output, args=(pipes_cv[1], q_cv))
    t_cv.daemon = True
    t_cv.start()

    proc_ci = subprocess.Popen(["exec ../stabilize ./tmp_capture.png"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, shell=True)
    pipes_ci = [proc_ci.stdin, proc_ci.stdout]
    q_ci= Queue()
    t_ci = Thread(target=enqueue_output, args=(pipes_ci[1], q_ci))
    t_ci.daemon = True
    t_ci.start()

    proc_GPIO = subprocess.Popen(["exec bash"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, shell=True)
    pipes_GPIO = [proc_GPIO.stdin, proc_GPIO.stdout]
    q_GPIO = Queue()
    t_GPIO = Thread(target=enqueue_output, args=(pipes_GPIO[1], q_GPIO))
    t_GPIO.daemon = True
    t_GPIO.start()

    #if DEBUG:
    #    proc_APP = subprocess.Popen(["exec bash"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, shell=True)
    #else:
    proc_APP = subprocess.Popen(["exec python ../gcm_server.py"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, shell=True)
    pipes_APP = [proc_APP.stdin, proc_APP.stdout]
    q_APP = Queue()
    t_APP = Thread(target=enqueue_output, args=(pipes_APP[1], q_APP))
    t_APP.daemon = True
    t_APP.start()

    return True

# close pipes and kill processes
def teardown():
    global proc_proxy, proc_cv, proc_ci, proc_GPIO, proc_APP
    global pipes_proxy, pipes_cv, pipes_ci, pipes_GPIO, pipes_APP
    global t_proxy, t_cv, t_ci, t_GPIO, t_APP

    def closeup(p, pipes, t):
        for pipe in pipes:
            try:
                pipe.close()
            except:
                pass
        p.terminate()
        try:
            p.kill()
        except Error:
            pass

    closeup(proc_proxy, pipes_proxy, t_proxy)
    closeup(proc_cv, pipes_cv, t_cv)
    closeup(proc_ci, pipes_ci, t_ci)
    closeup(proc_GPIO, pipes_GPIO, t_GPIO)
    closeup(proc_APP, pipes_APP, t_APP)

    return True

# process image (remove borders, decode, or look for direction)
def scan_image(mode="template", args=None):
    global pipes_cv

    if DEBUG:
        return None
    else:
        if mode == "template":
            pipes_cv[0].write("../template ./tmp_capture{0}.png {1} {2}\n").format(args["idx"], argx["lower"], args["upper"])
            output = collect_output("opencv", keys=["missing", "complete", "error", "invalid"], debug=False)
            return output
        elif mode == "decode":
            pipes_cv[0].write("../border N Y {0} {1} {2}\n".format(EMAIL_ADDRESS, "./tmp_capture{0}.png", "./tmp_solution.png").format(args["idx"]))
            output = collect_output("opencv", keys=["invalid", "result"], debug=False)
            return output
        elif mode == "locate":
            pipes_cv[0].write("../locate ./border.png")
            output = collect_output("opencv", keys=["invalid", "LEFT", "RIGHT", "UP", "DOWN", "CENTER"], debug=False)
            return output

def collect_output(proc, attempts=30, keys=["GPS_RAW_INT"], flag="or", debug=False):
    # gather output for timeout seconds, examine
    q = None
    if ("mav" in proc) or ("proxy" in proc):
        global q_proxy
        q = q_proxy
    elif "opencv" in proc:
        global q_cv
        q = q_cv
    elif "capture" in proc:
        global q_ci
        q = q_ci
    elif "GPIO" in proc:
        global q_GPIO
        q = q_GPIO
    elif "APP" in proc:
        global q_APP
        q = q_APP

    used_attempts = 0
    while (used_attempts < attempts):
        try:
            line = q.get_nowait()
            print 'Discovered: '+line
            found = False
            if flag == "and":
                found = True
                for key in keys:
                    if not (key in line):
                        found = False
                        break
            else:
                found = False
                for key in keys:
                    if key in line:
                        found = True
                        break
            if found:
                return line
        except:
            pass
        used_attempts += 1
    return None

# query for gps status on drone, return information decoded
def get_curr_latlon():
    global pipes_proxy

    output = None
    while output is None:
        pipes_proxy[0].write("status GPS_RAW_INT\n")
        output = collect_output("proxy")

        if output is None:
            print "unable to retrieve gps corodinates..try again?"
        else:
            gps_info = output.split("GPS_RAW_INT")[1]
            gps_info_str = json.dumps(gps_info)
            gps_info_arr = (filter(None, re.split("[{, \!?:}\"\n]+", gps_info_str)))[:-1]
            gps_info_dict = {}
            for i in xrange(0, len(gps_info_arr), 2):
                gps_info_dict[gps_info_arr[i]] = gps_info_arr[i+1]

    curr_lat, curr_lon = int(gps_info_dict['lat'])/LAT_LON_DIV, int(gps_info_dict['lon'])/LAT_LON_DIV
    return (curr_lat, curr_lon)

# issue GPS location for drone to travel to
def go_to(lat, lon, fn=None, fn_args = None, ret=False, curr_lat=None, curr_lon=None):
    global pipes_proxy

    pipes_proxy[0].write("guided {0} {1} {2}\n".format(lat, lon, int(math.ceil(FLYING_ALTITUDE/M_FT_CONV))))

    output = None
    if not (fn is None):
        if not (fn_args is None):
            output = fn(fn_args)
        else:
            output = fn()

    if ret and (not (curr_lat is None) and (not curr_lon is None)):
        time.sleep(5)
        pipes_proxy[0].write("guided {0} {1} {2}\n".format(curr_lat, curr_lon, int(math.ceil(FLYING_ALTITUDE/M_FT_CONV))))

    return output

# set the drone in guided and arm
def arm_throttle():
    global pipes_proxy

    pipes_proxy[0].write("mode guided\n")
    pipes_proxy[0].write("arm throttle\n")

# takeoff to flying altitude (beginning/threat avoidance)
def takeoff(arm=True):
    global pipes_proxy

    # optionally arm the throttle
    if arm:
        arm_throttle()
        time.sleep(5)

    pipes_proxy[0].write("takeoff {0}\n".format(FLYING_ALTITUDE/M_FT_CONV))

    print "completed takeoff commands"
    time.sleep(30)

# set the drone into landing mode (not RTL)
def land():
    global pipes_proxy

    pipes_proxy[0].write("mode land\n")
    time.sleep(10)

    print "completed landing commands"

# look for the pad's direction
def initial_search():
    global pipes_proxy

    pipes_proxy[0].write("mode guided\n")

    # establish coordinates to evaluate
    curr_lat, curr_lon = get_curr_latlon()
    lat_u = curr_lat+(50/(LATITUDE_CONV))
    lat_d = curr_lat-(50/(LATITUDE_CONV))
    lon_r = curr_lon+(50/(LONGITUDE_CONV))
    lon_l = curr_lon-(50/(LONGITUDE_CONV))
    best_dir = None

    # chceck if the direction is useful
    def move_helper(direction):
        best_dir = None
        for i in xrange(50):
            output = scan_image(mode="locate")
            if not (output is None):
                best_dir = direction
            time.sleep(0.05)
        return best_dir

    # try all four directions in cross
    output_u = go_to(lat_u, curr_lon, move_helper, "up", True, curr_lat, curr_lon)
    time.sleep(10)
    output_d = go_to(lat_d, curr_lon, move_helper, "down", True, curr_lat, curr_lon)
    time.sleep(10)
    output_r = go_to(curr_lat, lon_r, move_helper, "right", True, curr_lat, curr_lon)
    time.sleep(10)
    output_l = go_to(curr_lat, lon_l, move_helper, "left", True, curr_lat, curr_lon)
    time.sleep(10)

    if not (output_u is None):
        best_dir = output_u
    elif not (output_d is None):
        best_dir = output_d
    elif not (output_r is None):
        best_dir = output_r
    elif not (output_l is None):
        best_dir = output_l

    print "drone initial flight path completed ", best_dir
    return best_dir

def move_towards_landing(direction):
    global pipes_proxy, pipes_cv

    # establish directional lat/lon
    curr_lat, curr_lon = get_curr_latlon()
    lat_u = curr_lat+(20/(LATITUDE_CONV))
    lat_d = curr_lat-(20/(LATITUDE_CONV))
    lon_r = curr_lon+(20/(LONGITUDE_CONV))
    lon_l = curr_lon-(20/(LONGITUDE_CONV))

    # repositino the drone
    if (direction == "LEFT"):
        output = go_to(curr_lat, lon_l)
        time.sleep(10)
    elif (direction == "RIGHT"):
        output = go_to(curr_lat, lon_r)
        time.sleep(10)
    elif (direction == "UP"):
        output = go_to(lat_u, curr_lon)
        time.sleep(10)
    elif (direction == "DOWN"):
        output = go_to(lat_d, curr_lon)
        time.sleep(10)
    elif (direction == "CENTER"):
        print "already in the center of the pad"
        return 1

    print "moved towards landing location ", direction
    return 0

# GPIG pin handler, will just check if a 1 is output indicating threat detected (Thread)
def check_GPIO():
    global pipes_GPIO, gpio_high

    while (1):
        value = collect_output("GPIO", keys=["1"])
        if not (value is None):
            gpio_high = True
            should_terminate = True
        if should_terminate:
            break

# App handler, tell app to send request, break upon confirmation (Thread)
def check_APP():
    global pipes_APP, is_confirmed, should_terminate

    pipes_APP[0].write("send\n")
    while (1):
        output = collect_output("APP", keys=["confirm"])
        if not (output is None):
            print 'done'
            is_confirmed = True
            should_terminate = True
        if should_terminate:
            break

if __name__ == "__main__":
    with open("../email.txt") as email_file:
        for line in email_file:
            EMAIL_ADDRESS = line
            break

    setup()
    time.sleep(20)
    takeoff()
    best_dir = initial_search()
    if best_dir is None:
        pass
    else:
        output = move_to_landing(best_dir)
        while (output != 1):
            best_dir = scan_image(mode="locate")
            output = move_to_landing(best_dir)
        # try decoding
        is_landing = False
        output = scan_image(mode="template", args={"idx": 0, "lower": OPTIONS[0], "upper": UPPER})
        if ("complete" in output):
            result =  scan_image(mode="decode", args={"idx": 0}).split("result")[1]
            if int(result) > THRESHOLD:
                is_landing = True
                land()
        if not is_landing:
            output = scan_image(mode="template", args={"idx": 1, "lower": OPTIONS[1], "upper": UPPER})
            if ("complete" in output):
                result =  scan_image(mode="decode", args={"idx": 1}).split("result")[1]
                if int(result) > THRESHOLD:
                    is_landing = True
                    land()
        if not is_landing:
            pass
            # TODO abort msg
    land()

    # start the GPIO/APP threads
    #thread_app = Thread(target=check_GPIO, args=[])
    #thread_app.daemon = True
    #thread_msg = Thread(target=check_APP, args=[])
    #thread_msg.daemon = True
    check_APP()
    gpio_high = False
    is_confirmed = False

    print 'reaching stopping point'

    last_checked = time.time()
    while(1):
        # keep throttle armed in case we need to take off quickly
        if (abs(time.time() - last_checked)) > 4.0:
            arm_throttle()
        if (gpio_high):
            takeoff(arm=False)
        elif (is_confirmed):
            land()
            # TODO shine_gpio led?
    print 'tearing down now'
    teardown()
