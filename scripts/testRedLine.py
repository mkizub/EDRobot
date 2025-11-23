import cv2
import math
import sys
import numpy as np

def nothing(x):
    pass

img = cv2.imread(sys.argv[1])
#img = cv2.resize(img, dsize=None, fx=0.5, fy=0.5)
image_out = img
lines_out = img

# Create a window
cv2.namedWindow('image')
cv2.imshow('image',image_out)
cv2.namedWindow('lines')
cv2.imshow('lines',lines_out)
cv2.waitKey(100)

# create trackbars to show step
cv2.createTrackbar('Show','image',0,4,nothing)

# create trackbars for color change
cv2.createTrackbar('Gain','image',0,200,nothing)
cv2.setTrackbarMax('Gain','image',100)
cv2.setTrackbarMin('Gain','image',-100)
cv2.setTrackbarPos('Gain','image',0)

cv2.createTrackbar('Bias','image',0,200,nothing)
cv2.setTrackbarMax('Bias','image',100)
cv2.setTrackbarMin('Bias','image',-100)
cv2.setTrackbarPos('Bias','image',0)

cv2.createTrackbar('Kern','image',0,4,nothing) # smooth kernel size
cv2.setTrackbarMax('Kern','image',9)
cv2.setTrackbarMin('Kern','image',1)
cv2.setTrackbarPos('Kern','image',1)

cv2.createTrackbar('AddW','image',0,100,nothing)
cv2.setTrackbarMax('AddW','image',100)
cv2.setTrackbarMin('AddW','image',0)
cv2.setTrackbarPos('AddW','image',0)

cv2.createTrackbar('Thr','image',0,250,nothing)
cv2.setTrackbarPos('Thr','image',127)

# create trackbars for Hough lines
cv2.createTrackbar('Rho','lines',0,3,nothing)
cv2.setTrackbarMax('Rho','lines',4)
cv2.setTrackbarMin('Rho','lines',1)
cv2.setTrackbarPos('Rho','lines',1)

cv2.createTrackbar('Theta','lines',0,8,nothing)
cv2.setTrackbarMax('Theta','lines',9)
cv2.setTrackbarMin('Theta','lines',1)
cv2.setTrackbarPos('Theta','lines',1)

cv2.createTrackbar('Votes','lines',0,100,nothing)
cv2.setTrackbarMax('Votes','lines',2000)
cv2.setTrackbarMin('Votes','lines',10)
cv2.setTrackbarPos('Votes','lines',10)

waitTime = 33

while(1):

    # get current positions of all trackbars
    show = cv2.getTrackbarPos('Show','image')

    gain = cv2.getTrackbarPos('Gain','image')
    bias = cv2.getTrackbarPos('Bias','image')

    kern = cv2.getTrackbarPos('Kern','image')
    addw = cv2.getTrackbarPos('AddW','image')
    threshold = cv2.getTrackbarPos('Thr','image')

    hRho = cv2.getTrackbarPos('Rho','lines')
    hTheta = cv2.getTrackbarPos('Theta','lines')
    hVotes = cv2.getTrackbarPos('Votes','lines')

    # Create HSV Image and threshold into a range.
    adj = cv2.convertScaleAbs(img, alpha=(1.0+gain*0.01), beta=bias)
    _, _, gray = cv2.split(adj)
    if kern > 1:
        #if (kern % 2) != 1:
        #    kern = kern+1
        #smooth = cv2.GaussianBlur(red, (kern,kern), 0)
        smooth = cv2.boxFilter(gray, -1, (kern,kern))
        diff = cv2.addWeighted(gray, 1+addw/100, smooth, -(1+addw/100), 0, dtype=cv2.CV_8UC1)
    else:
        smooth = diff = gray
    _,thresh = cv2.threshold(diff, threshold, 255, cv2.THRESH_BINARY);
    if show == 0:
        image_out = adj
    elif show == 1:
        image_out = gray
    elif show == 2:
        image_out = smooth
    elif show == 3:
        image_out = diff
    else:
        image_out = thresh

    lines_out = img.copy()
    lines = cv2.HoughLines(thresh, hRho, np.radians(1/hTheta), hVotes, min_theta=np.radians(90-20), max_theta=np.radians(90))
    if lines is not None:
        for i in range(0, min(20, len(lines))):
            rho = lines[i][0][0]
            theta = lines[i][0][1]
            a = math.cos(theta)
            b = math.sin(theta)
            x0 = a * rho
            y0 = b * rho
            pt1 = (int(x0 + 2000*(-b)), int(y0 + 2000*(a)))
            pt2 = (int(x0 - 2000*(-b)), int(y0 - 2000*(a)))
            cv2.line(lines_out, pt1, pt2, (0,0,255), 1, cv2.LINE_AA)

    # Display output image
    cv2.imshow('image',image_out)
    cv2.imshow('lines',lines_out)

    # Wait longer to prevent freeze for videos.
    if cv2.waitKey(waitTime) & 0xFF == ord('q'):
        break

cv2.destroyAllWindows()
quit()