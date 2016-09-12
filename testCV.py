import cv2.cv as cv
import cv2
import qrtools
import qrcode
import numpy as np
from matplotlib import pyplot as plt
import sys

if len(sys.argv) < 2:
    raise Exception("give more args")

if sys.argv[1] == 'r':
    if len(sys.argv) < 3:
        raise Exception("moar args")

    cv.NamedWindow("camera", 1)
    capture = cv.CaptureFromCAM(0)
    count = 0
    while True:
        img = cv.QueryFrame(capture)
        im_gray  = cv.CreateImage(cv.GetSize(img),cv.IPL_DEPTH_8U,1)
        cv.CvtColor(img,im_gray,cv.CV_RGB2GRAY)
        cv.SaveImage('testers{0}.png'.format(count), im_gray)
        if cv.WaitKey(33) == ord('q'):
            break
        count += 1
        if (count >= int(sys.argv[2])):
            break
    cv.DestroyAllWindows()

elif sys.argv[1] == 'd':
    if len(sys.argv) < 3:
        raise Exception("moar args")

    qr = qrtools.QR()
    qr.decode(sys.argv[2])
    print qr.data

elif sys.argv[1] == 'e':
    if len(sys.argv) < 4:
        raise Exception("moar args")

    img = qrcode.make(sys.argv[3])
    img.save(sys.argv[2])

elif sys.argv[1] == 's':
    if len(sys.argv) < 4:
        raise Exception("moar args")

    x_scale = float(sys.argv[2])
    y_scale = float(sys.argv[3])

    im = cv2.imread('./test0.png')
    height, width = im.shape[:2]
    smaller = cv2.resize(im, (0,0), fx=x_scale, fy=y_scale)
    GREEN = [0,255,0]
    bordered = cv2.copyMakeBorder(smaller, int((1.0-(y_scale/2))*height), int((1.0-(y_scale/2))*height), int((1.0-(x_scale/2))*width), int((1.0-(x_scale/2))*width), cv2.BORDER_CONSTANT, value=GREEN);
    cv2.imwrite('./border0.png', bordered)
    cv2.destroyAllWindows()

elif sys.argv[1] == 'c':
    im = cv2.imread('tester0.png')
    imgray = cv2.cvtColor(im,cv2.COLOR_BGR2GRAY)
    ret,thresh = cv2.threshold(imgray,127,255,0)
    contours, hierarchy = cv2.findContours(thresh,cv2.RETR_TREE,cv2.CHAIN_APPROX_SIMPLE)
    parent_map = {}
    corner_parent = None

    for idx in xrange(len(hierarchy[0])):
        info = hierarchy[0][idx]
        parent = info[3]
        if parent in parent_map.keys():
            parent_map[parent].append(idx)
        else:
            parent_map[parent] = [idx]
    # evaluated nested shapes, check for pattern matches
    for (key, val) in parent_map.items():
        if (key == -1 or key == 0):
            continue
        acc = 0
        q = [key]
        container = [key]
        while (len(q) != 0):
            head = q[0]
            q = q[1:]
            if not (head in parent_map.keys()):
                continue
            for child in parent_map[head]:
                q.append(child)
                container.append(child)
                acc += 1
            if acc > 2:
                acc = -1
                print 'too many nested'
                break
        if acc == 2:
            x,y,w,h = cv2.boundingRect(contours[key])
            bounded_area = w*h
            contour_area = cv2.contourArea(contours[key])
            # filter by ratio of area (contour to bounding rect)
            if ((contour_area / bounded_area) < .8):
                print 'bad ratio bounded'
                continue
            inner_contour_area = cv2.contourArea(contours[val[0]])
            # filter by ratio of area (nested to outer)
            if (inner_contour_area/contour_area < .4) or (inner_contour_area/contour_area > .5):
                print 'bad ratio nested'
                continue
            corner_parent = hierarchy[0][key][3]

            # drawing to indicate contours/bounding rect
            '''
            cv2.rectangle(im,(x,y),(x+w,y+h),(0,255,0),2)
            for elem in container:
                cv2.drawContours(im, contours, elem, (0,255,0), 3)
            cv2.drawContours(im, contours, corner_parent, (255,0,0), 3)
            '''

    hw = im.shape[:2]

    # determine border around outer section of QR code and crop
    x,y,w,h = cv2.boundingRect(contours[corner_parent])
    #cv2.rectangle(im,(x,y),(x+w,y+h),(0,255,0),2)
    im = im[y:y+h, x:x+w]

    # resize to original img size (cubic may be too slow)
    im = cv2.resize(im, (hw[0], hw[0]),0,0,cv2.INTER_CUBIC)

    #output
    cv2.imwrite('./derps_contour.png', im)
    cv2.destroyAllWindows()

elif sys.argv[1] == 't':
    img = cv2.imread('derps_contour.png')
    img = cv2.cvtColor(img,cv2.COLOR_BGR2GRAY)
    ret,th1 = cv2.threshold(img,127,255,cv2.THRESH_TOZERO)
    th2 = cv2.adaptiveThreshold(img,255,cv2.ADAPTIVE_THRESH_MEAN_C,cv2.THRESH_BINARY,11,2)
    th3 = cv2.adaptiveThreshold(img,255,cv2.ADAPTIVE_THRESH_GAUSSIAN_C,cv2.THRESH_BINARY,11,2)
    titles = ['Original Image', 'Global Thresholding (v = 127)', 'Adaptive Mean Thresholding', 'Adaptive Gaussian Thresholding']
    images = [img, th1, th2, th3]

    for i in xrange(4):
        plt.subplot(2,2,i+1),plt.imshow(images[i],'gray')
        plt.title(titles[i])
        plt.xticks([]),plt.yticks([])
    plt.show()

    cv2.imwrite('./derps_highlight.png', th1)
    cv2.destroyAllWindows()

else:
    raise Exception('invalid arg')
