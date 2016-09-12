import subprocess
import os
import sys
import time
from threading  import Thread
from Queue import Queue, Empty

UPPER = 255
OPTIONS = [50, 65, 80]

proc_cv = None
pipes_cv = None
t_cv = None
q_cv = None

# Queueing
def enqueue_output(out, queue):
    for line in iter(out.readline, b''):
        queue.put(line)
    out.close()

# create the opencv shell to issue commands to
def setup():
    global proc_cv, pipes_cv, t_cv, q_cv

    proc_cv = subprocess.Popen(['bash'], stdout=subprocess.PIPE, stdin=subprocess.PIPE)
    pipes_cv = [proc_cv.stdin, proc_cv.stdout]

    q_cv = Queue()
    t_cv = Thread(target=enqueue_output, args=(pipes_cv[1], q_cv))
    t_cv.daemon = True
    t_cv.start()
    return True

# close opencv pipes and exit the shell
def teardown():
    global proc_cv, pipes_cv, t_cv, q_cv

    for pipe in pipes_cv:
        try:
            pipe.close()
        except:
            pass

    proc_cv.kill()
    return True

# look through the output (non-blocking using queue)
def collect_output(attempts=30, keys=[], flag="or", debug=False):
    global q_cv

    q = q_cv
    used_attempts = 0
    while (used_attempts < attempts):
        try:
            line = q.get_nowait()
            if debug:
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
        except Exception as e:
            pass
    return None

if __name__ == "__main__":
    setup()
    testfile = "./tests.txt"

    # if testfile specified, override default
    if (len(sys.argv) > 1):
        testfile = sys.argv[1]

    tmp_dest = "./tmp_out"

    with open(testfile, 'r') as input_file:
        successes = 0
        failures = 0
        threshold = -1.0
        firstLine = True
        test_idx = 0
        for line in input_file:
            # read in threshold for success values
            if firstLine:
                threshold = float(line)
                firstLine = False
            else:
                # run test case
                print "Running Test {0}".format(test_idx)
                (input_image, email_addr, should_normalize) = line.replace("\n", "").split("|")
                input_image = input_image.lstrip().rstrip()
                email_addr = email_addr.lstrip().rstrip()
                should_normalize = int(should_normalize)
                print "Beginning Phase 0"
                # start clock
                t0 = time.time()
                failed = [True for x in xrange(len(OPTIONS))]
                highest_acc = -1

                # try removing borders and rotating (multiple options for threshold for outliers)
                for opt_idx in xrange(len(OPTIONS)):
                    pipes_cv[0].write("./template {0} {1} {2} {3}\n".format(input_image, tmp_dest+str(opt_idx)+".png", OPTIONS[opt_idx], UPPER))
                    phase0_out = collect_output(attempts=100, keys=["missing", "invalid", "completed", "failed"], flag="or", debug=False)
                    if (phase0_out is None) or ("missing" in phase0_out) or ("invalid" in phase0_out) or ("failed" in phase0_out):
                        continue
                    else:
                        failed[opt_idx] = False
                        if should_normalize:
                            pipes_cv[0].write("./normalize {0} {1}\n".format(tmp_dest+str(opt_idx)+".png", tmp_dest+str(opt_idx)+".png"))
                # stop clock
                print "Phase 0 Time: {0}".format(time.time()-t0)
                # start secondary clock
                t1 = time.time()
                for opt_idx in xrange(len(OPTIONS)):
                    if (failed[opt_idx] is False):
                        # try decoding
                        pipes_cv[0].write("./border N Y {0} {1} {2}\n".format(email_addr, input_image, tmp_dest+str(opt_idx)+".png"))
                        phase1_out = collect_output(["invalid", "result"], "or")
                        # accuracy of removing borders with this threshold
                        if "invalid" in phase1_out:
                            continue
                        result = phase1_out.split("result")[1]
                        os.remove(tmp_dest+str(opt_idx)+".png")
                        observed = float(result)

                        if (observed > highest_acc):
                            highest_acc = observed
                print highest_acc
                # stop clock
                print "Phase 1 Time: {0}".format(time.time()-t1)
                print "Total Time: {0}".format(time.time()-t0)

                # if able to decode with high enough accruacy this is success
                if (highest_acc > threshold):
                    successes += 1
                else:
                    failures += 1
                test_idx += 1
        print "Correct: {0}, Failures: {1}, Accuracy: {2}".format(successes, failures, (float(successes)/(successes + failures)))

    teardown()
