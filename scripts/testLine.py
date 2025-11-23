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
cv2.createTrackbar('Show','image',0,5,nothing)

# create trackbars for color change
cv2.createTrackbar('Gain','image',0,200,nothing)
cv2.setTrackbarMax('Gain','image',100)
cv2.setTrackbarMin('Gain','image',-100)
cv2.setTrackbarPos('Gain','image',0)

cv2.createTrackbar('Bias','image',0,200,nothing)
cv2.setTrackbarMax('Bias','image',100)
cv2.setTrackbarMin('Bias','image',-100)
cv2.setTrackbarPos('Bias','image',0)

cv2.createTrackbar('HMin','image',0,179,nothing) # Hue is from 0-179 for Opencv
cv2.createTrackbar('SMin','image',0,255,nothing)
cv2.createTrackbar('VMin','image',0,255,nothing)
cv2.createTrackbar('HMax','image',0,179,nothing)
cv2.createTrackbar('SMax','image',0,255,nothing)
cv2.createTrackbar('VMax','image',0,255,nothing)

# Set default value for MAX HSV trackbars.
cv2.setTrackbarPos('HMax', 'image', 179)
cv2.setTrackbarPos('SMax', 'image', 255)
cv2.setTrackbarPos('VMax', 'image', 255)

cv2.createTrackbar('Kern','image',0,7,nothing) # smooth kernel size

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
cv2.setTrackbarMax('Votes','lines',1000)
cv2.setTrackbarMin('Votes','lines',10)
cv2.setTrackbarPos('Votes','lines',10)

# Initialize to check if HSV min/max value changes
hMin = sMin = vMin = hMax = sMax = vMax = 0

waitTime = 33

while(1):

    # get current positions of all trackbars
    show = cv2.getTrackbarPos('Show','image')

    gain = cv2.getTrackbarPos('Gain','image')
    bias = cv2.getTrackbarPos('Bias','image')

    hMin = cv2.getTrackbarPos('HMin','image')
    sMin = cv2.getTrackbarPos('SMin','image')
    vMin = cv2.getTrackbarPos('VMin','image')

    hMax = cv2.getTrackbarPos('HMax','image')
    sMax = cv2.getTrackbarPos('SMax','image')
    vMax = cv2.getTrackbarPos('VMax','image')

    kern = cv2.getTrackbarPos('Kern','image')

    hRho = cv2.getTrackbarPos('Rho','lines')
    hTheta = cv2.getTrackbarPos('Theta','lines')
    hVotes = cv2.getTrackbarPos('Votes','lines')

    # Set minimum and max HSV values to display
    lower = np.array([hMin, sMin, vMin])
    upper = np.array([hMax, sMax, vMax])

    # Create HSV Image and threshold into a range.
    adj = cv2.convertScaleAbs(img, alpha=(1.0+gain*0.01), beta=bias)
    hsv = cv2.cvtColor(adj, cv2.COLOR_BGR2HSV)
    mask = cv2.inRange(hsv, lower, upper)
    gray = cv2.cvtColor(adj, cv2.COLOR_BGR2GRAY);
    gray = cv2.bitwise_and(gray, gray, mask=mask)
    if kern > 1:
        smooth = cv2.boxFilter(gray, -1, (kern,kern))
        diff = cv2.addWeighted(gray, 2.0, smooth, -2.0, 0, dtype=cv2.CV_8UC1)
    else:
        smooth = diff = gray
    _,thresh = cv2.threshold(diff, 127, 255, cv2.THRESH_BINARY);
    if show == 0:
        image_out = adj
    elif show == 1:
        image_out = cv2.bitwise_and(adj, adj, mask=mask)
    elif show == 2:
        image_out = gray
    elif show == 3:
        image_out = smooth
    elif show == 4:
        image_out = diff
    else:
        image_out = thresh

    lines_out = img.copy()
    lines = cv2.HoughLines(diff, hRho, np.radians(1/hTheta), hVotes, min_theta=np.radians(90-20), max_theta=np.radians(90))
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