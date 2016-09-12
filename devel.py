import subprocess
import math
import time

HOLD_TIME = 0.149999999999999994
SLEEP_TIME = .5

'''
proc = subprocess.Popen(['bash'], stdout=subprocess.PIPE, stdin=subprocess.PIPE)
pipe_in = proc.stdin
pipe_out = proc.stdout
pipe_in.write('pwd\n')
out = pipe_out.readline()
while out:
    print out
    out = pipe_out.readline()
pipe_in.write('pwd\n')
while out:
    print out
    out = pipe_out.readline()
pipe_in.close()
pipe_out.close()
proc.kill()
'''

proc_proxy = None
proc_cv = None
pipes_proxy = None
pipes_cv = None

# get current gps coord
def setup():
    global proc_proxy, proc_cv, pipes_proxy, pipes_cv

    proc_proxy = subprocess.Popen(['bash'], stdout=subprocess.PIPE, stdin=subprocess.PIPE)
    pipes_proxy = [proc_proxy.stdin, proc_proxy.stdout]
    proc_cv = subprocess.Popen(['bash'], stdout=subprocess.PIPE, stdin=subprocess.PIPE)
    pipes_cv = [proc_cv.stdin, proc_cv.stdout]
    return True

def teardown():
    global proc_proxy, proc_cv, pipes_proxy, pipes_cv

    for pipe in pipes_proxy:
        pipe.close()
    proc_proxy.kill()
    for pipe in pipes_cv:
        pipe.close()
    proc_cv.kill()
    return True

def write_out_file(lat, lon, alt):
    global HOLD_TIME

    waypoint_output = open("waypoints.txt", "w")
    waypoint_output.write("QGC\tWPL\t110\n")
    waypoint_output.write("0\t1\t0\t16\t{0}}\t0\t0\t0\t{1}\t{2}\t{3}\t1".format(HOLD_TIME, lat, lon, alt))
    waypoint_output.close()

def parse_output(pipe_out):
    out_arr = []
    out = pipe_out.readline()
    while out:
        out_arr.append(out)
        out = pipe_out.readline()
    return out_arr

def init():
    (lat, lon, alt) = (0, 0, 0) # TODO
    write_out_file(lat, lon, alt)
    proxy_pipe_in.write("wp load waypoints.txt\n")
    offsets = [(10,10), (10,-10), (-10, 10), (-10, -10)]  # TODO
    max_accuracy = 0.0
    best_dir = -1
    for i in xrange(len(offsets)):
        # travel to new destination
        write_out_file(lat+offsets[i][0], lon+offsets[i][1], alt)
        proxy_pipe_in.write("wp update waypoints.txt 0\n")
        proxy_pipe_in.write("wp set 0\n")
        time.sleep(SLEEP_TIME)

        # OpenCV Analysis
        captured = parse_output(cv_pipe_in.write("./capture_img captured.png\n"))
        if not captured:
            raise Exception("Unable to access camera feed")
        detected = parse_output(cv_pipe_in.write("./crop_img captured.png cropped.png\n")) # CROP THE BORDER
        if detected[0] == "Success":
            accuracy = parse_output(cv_pipe_in.write("./decode_border cropped.png\n"))[0] # TRY TO PATTERN MATCH
            max_accuracy = max(accuracy, max_accuracy)
            if max_accuracy == accuracy:
                best_dir = i

        # return to home location
        write_out_file(lat, lon, alt)
        proxy_pipe_in.write("wp update waypoints.txt 0\n")
        proxy_pipe_in.write("wp set 0\n")
        time.sleep(SLEEP_TIME)

    if best_dir == -1:
        raise Exception("Unable to locate landing pad")

def main():
    return None
# beginloop
# reevaluate c++ code
# wp move
# wp set
# throttle
# endloop
