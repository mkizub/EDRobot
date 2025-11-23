import cv2
import math
import sys
import numpy as np

def nothing(x):
    pass

class Filter:

    def __init__(self, name:str):
        self.name = name
        cv2.namedWindow(name)

    def apply(self, image):
        return image

class ThresholdFilter(Filter):

    def __init__(self):
        super().__init__('Threshold')
        cv2.createTrackbar('thr','Threshold',0,255,nothing)
        cv2.setTrackbarMax('thr','Threshold',255)
        cv2.setTrackbarMin('thr','Threshold',0)
        cv2.setTrackbarPos('thr','Threshold',127)

    def apply(self, image):
        thr = cv2.getTrackbarPos('thr','Threshold')
        _,out = cv2.threshold(image, thr, 255, cv2.THRESH_BINARY)
        cv2.imshow('Threshold',out)
        return out


class GaussFilter(Filter):

    def __init__(self):
        super().__init__('Gauss')
        cv2.createTrackbar('kx','Gauss',1,5,nothing)
        cv2.setTrackbarMax('kx','Gauss',9)
        cv2.setTrackbarMin('kx','Gauss',1)
        cv2.setTrackbarPos('kx','Gauss',1)
        cv2.createTrackbar('ky','Gauss',1,5,nothing)
        cv2.setTrackbarMax('ky','Gauss',9)
        cv2.setTrackbarMin('ky','Gauss',1)
        cv2.setTrackbarPos('ky','Gauss',1)

    def apply(self, image):
        kx = cv2.getTrackbarPos('kx','Gauss')
        ky = cv2.getTrackbarPos('ky','Gauss')
        if kx == 1 and ky == 1:
            cv2.imshow('Gauss',image)
            return image
        if (kx % 2) == 0:
            kx += 1
        if (ky % 2) == 0:
            ky += 1
        out = cv2.GaussianBlur(image, (kx,ky), 0)
        cv2.imshow('Gauss',out)
        return out


class GainBiasFilter(Filter):

    def __init__(self):
        super().__init__('Scale')
        cv2.createTrackbar('Gain','Scale',0,200,nothing)
        cv2.setTrackbarMax('Gain','Scale',100)
        cv2.setTrackbarMin('Gain','Scale',-100)
        cv2.setTrackbarPos('Gain','Scale',0)
        cv2.createTrackbar('Bias','Scale',0,200,nothing)
        cv2.setTrackbarMax('Bias','Scale',100)
        cv2.setTrackbarMin('Bias','Scale',-100)
        cv2.setTrackbarPos('Bias','Scale',0)

    def apply(self, image):
        gain = cv2.getTrackbarPos('Gain','Scale')
        bias = cv2.getTrackbarPos('Bias','Scale')
        if gain == 0 and bias == 0:
            cv2.imshow('Scale',image)
            return image
        out = cv2.convertScaleAbs(image, alpha=(1.0+gain*0.01), beta=bias)
        cv2.imshow('Scale',out)
        return out


class HSVFilter(Filter):

    def __init__(self):
        super().__init__('HSV')
        cv2.createTrackbar('Mode','HSV',0,2,nothing)
        cv2.createTrackbar('HMin','HSV',0,179,nothing) # Hue is from 0-179 for Opencv
        cv2.createTrackbar('HMax','HSV',0,179,nothing)
        cv2.createTrackbar('SMin','HSV',0,255,nothing)
        cv2.createTrackbar('SMax','HSV',0,255,nothing)
        cv2.createTrackbar('VMin','HSV',0,255,nothing)
        cv2.createTrackbar('VMax','HSV',0,255,nothing)
        # Set default value for MAX HSV trackbars.
        cv2.setTrackbarPos('HMax', 'HSV', 179)
        cv2.setTrackbarPos('SMax', 'HSV', 255)
        cv2.setTrackbarPos('VMax', 'HSV', 255)

    def apply(self, image):
        mode = cv2.getTrackbarPos('Mode','HSV')
        hMin = cv2.getTrackbarPos('HMin','HSV')
        hMax = cv2.getTrackbarPos('HMax','HSV')
        sMin = cv2.getTrackbarPos('SMin','HSV')
        sMax = cv2.getTrackbarPos('SMax','HSV')
        vMin = cv2.getTrackbarPos('VMin','HSV')
        vMax = cv2.getTrackbarPos('VMax','HSV')
        lower = np.array([hMin, sMin, vMin])
        upper = np.array([hMax, sMax, vMax])
        hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)
        mask = cv2.inRange(hsv, lower, upper)
        if mode == 0:
            cv2.setWindowTitle('HSV', 'HSV -> Color')
            out = cv2.bitwise_and(image, image, mask=mask)
        elif mode == 1:
            cv2.setWindowTitle('HSV', 'HSV -> Gray')
            gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
            out = cv2.bitwise_and(gray, gray, mask=mask)
        elif mode == 2:
            cv2.setWindowTitle('HSV', 'HSV -> Mask')
            out = mask
        else:
            cv2.setWindowTitle('HSV', 'HSV -> ???')
            out = mask
        cv2.imshow('HSV',out)
        return out


class ChannelFilter(Filter):

    def __init__(self):
        super().__init__('Channel')
        cv2.createTrackbar('Mode','Channel',0,3,nothing)

    def apply(self, image):
        mode = cv2.getTrackbarPos('Mode','Channel')
        if len(image.shape) == 2:
            cv2.setWindowTitle('Channel', 'Channel error: grayscale')
            return image
        if mode == 1 or mode == 2 or mode == 3:
            b, g, r = cv2.split(image)
            if mode == 3:
                cv2.setWindowTitle('Channel', 'BGR -> Blue')
                out = b
            elif mode == 2:
                cv2.setWindowTitle('Channel', 'BGR -> Green')
                out = g
            elif mode == 1:
                cv2.setWindowTitle('Channel', 'BGR -> Red')
                out = r
        elif mode == 0:
            cv2.setWindowTitle('Channel', 'BGR -> Gray')
            out = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
        else:
            cv2.setWindowTitle('Channel', f'Channel error: bad mode {mode}')
            out = image.astype('uint8')

        cv2.imshow('Channel',out)
        return out


class SobelFilter(Filter):

    def __init__(self):
        super().__init__('Sobel')
        cv2.createTrackbar('kern','Sobel',1,4,nothing)
        cv2.setTrackbarMax('kern','Sobel',9)
        cv2.setTrackbarMin('kern','Sobel',1)
        cv2.setTrackbarPos('kern','Sobel',1)
        cv2.createTrackbar('scale','Sobel',0,200,nothing)
        cv2.setTrackbarMax('scale','Sobel',100)
        cv2.setTrackbarMin('scale','Sobel',-100)
        cv2.setTrackbarPos('scale','Sobel',0)
        cv2.createTrackbar('delta','Sobel',0,200,nothing)
        cv2.setTrackbarMax('delta','Sobel',100)
        cv2.setTrackbarMin('delta','Sobel',-100)
        cv2.setTrackbarPos('delta','Sobel',0)
        cv2.createTrackbar('derv','Sobel',1,2,nothing)
        cv2.setTrackbarMax('derv','Sobel',2)
        cv2.setTrackbarMin('derv','Sobel',1)
        cv2.setTrackbarPos('derv','Sobel',1)

    def apply(self, image):
        kern = cv2.getTrackbarPos('kern','Sobel')
        if (kern % 2) == 0:
            kern = kern + 1
        cv2.setWindowTitle('Sobel', f'Sobel kern: {kern}')
        scale = cv2.getTrackbarPos('scale','Sobel')
        delta = cv2.getTrackbarPos('delta','Sobel')
        derv = cv2.getTrackbarPos('derv','Sobel')
        if scale <= -100:
            scale = -99
        f_grad_x = cv2.Sobel(image, cv2.CV_32FC1, derv, 0, ksize=kern, scale=(1+scale/100)/255, delta=delta/100)
        u_grad_x = cv2.convertScaleAbs(f_grad_x, alpha=255)
        f_grad_y = cv2.Sobel(image, cv2.CV_32FC1, 0, derv, ksize=kern, scale=(1+scale/100)/255, delta=delta/100)
        u_grad_y = cv2.convertScaleAbs(f_grad_y, alpha=255)
        out = cv2.addWeighted(u_grad_x, 0.5, u_grad_y, 0.5, 0)
        cv2.imshow('Sobel',out)
        return out


class LaplacianFilter(Filter):

    def __init__(self):
        super().__init__('Laplacian')
        cv2.createTrackbar('kern','Laplacian',1,9,nothing)
        cv2.setTrackbarMax('kern','Laplacian',9)
        cv2.setTrackbarMin('kern','Laplacian',1)
        cv2.setTrackbarPos('kern','Laplacian',1)
        cv2.createTrackbar('scale','Laplacian',0,200,nothing)
        cv2.setTrackbarMax('scale','Laplacian',100)
        cv2.setTrackbarMin('scale','Laplacian',-100)
        cv2.setTrackbarPos('scale','Laplacian',0)
        cv2.createTrackbar('delta','Laplacian',0,200,nothing)
        cv2.setTrackbarMax('delta','Laplacian',100)
        cv2.setTrackbarMin('delta','Laplacian',-100)
        cv2.setTrackbarPos('delta','Laplacian',0)

    def apply(self, image):
        kern = cv2.getTrackbarPos('kern','Laplacian')
        if (kern % 2) == 0:
            kern = kern + 1
        cv2.setWindowTitle('Laplacian', f'Laplacian kern: {kern}')
        scale = cv2.getTrackbarPos('scale','Laplacian')
        delta = cv2.getTrackbarPos('delta','Laplacian')
        if scale <= -100:
            scale = -99
        lapl = cv2.Laplacian(image, cv2.CV_32FC1, ksize=kern, scale=1+scale/100, delta=delta)
        out = cv2.convertScaleAbs(lapl)
        cv2.imshow('Laplacian',out)
        return out


class HorzLineFilter(Filter):

    def __init__(self):
        super().__init__('HorzLine')
        cv2.createTrackbar('scale','HorzLine',0,200,nothing)
        cv2.setTrackbarMax('scale','HorzLine',100)
        cv2.setTrackbarMin('scale','HorzLine',-100)
        cv2.setTrackbarPos('scale','HorzLine',0)
        cv2.createTrackbar('thr','HorzLine',0,255,nothing)
        cv2.setTrackbarMax('thr','HorzLine',255)
        cv2.setTrackbarMin('thr','HorzLine',0)
        cv2.setTrackbarPos('thr','HorzLine',127)
        cv2.createTrackbar('dilPos','HorzLine',2,4,nothing)
        cv2.setTrackbarMax('dilPos','HorzLine',3)
        cv2.setTrackbarMin('dilPos','HorzLine',0)
        cv2.setTrackbarPos('dilPos','HorzLine',2)
        cv2.createTrackbar('dilNeg','HorzLine',2,4,nothing)
        cv2.setTrackbarMax('dilNeg','HorzLine',3)
        cv2.setTrackbarMin('dilNeg','HorzLine',0)
        cv2.setTrackbarPos('dilNeg','HorzLine',2)
        cv2.createTrackbar('erode','HorzLine',0,4,nothing)
        cv2.setTrackbarMax('erode','HorzLine',3)
        cv2.setTrackbarMin('erode','HorzLine',0)
        cv2.setTrackbarPos('erode','HorzLine',0)

    def apply(self, image):
        scale = cv2.getTrackbarPos('scale','HorzLine')
        thr = cv2.getTrackbarPos('thr','HorzLine')
        dilPos = cv2.getTrackbarPos('dilPos','HorzLine')
        dilNeg = cv2.getTrackbarPos('dilNeg','HorzLine')
        erode = cv2.getTrackbarPos('erode','HorzLine')
        kernel_2 = cv2.getStructuringElement(cv2.MORPH_RECT, (1, 2))
        kernel_3 = cv2.getStructuringElement(cv2.MORPH_RECT, (1, 3))
        anchor = (0,1)

        grad = cv2.Scharr(image, cv2.CV_32F, 0, 1, scale=(1+scale/100)/255./16.)

        grad_pos = cv2.convertScaleAbs(cv2.max(grad, 0.0), alpha=255)
        _,grad_pos = cv2.threshold(grad_pos, thr, 255, cv2.THRESH_BINARY)
        if dilPos > 0:
            grad_pos = cv2.dilate(grad_pos, kernel_2, anchor=anchor, iterations=dilPos)

        grad_neg = cv2.convertScaleAbs(cv2.min(grad, 0.0), alpha=255.0)
        _,grad_neg = cv2.threshold(grad_neg, thr, 255, cv2.THRESH_BINARY)
        if dilNeg > 0:
            grad_neg = cv2.flip(grad_neg, 0)
            grad_neg = cv2.dilate(grad_neg, kernel_2, anchor=anchor, iterations=dilNeg)
            grad_neg = cv2.flip(grad_neg, 0)

        if dilPos > 0 or dilNeg > 0:
            out = cv2.bitwise_and(grad_pos, grad_neg)
        else:
            out = cv2.bitwise_or(grad_pos, grad_neg)
        if erode > 0:
            out = cv2.erode(out, kernel_3, anchor=anchor, iterations=erode)
        cv2.imshow('HorzLine',out)
        return out

class VertLineFilter(Filter):

    def __init__(self):
        super().__init__('VertLine')
        cv2.createTrackbar('scale','VertLine',0,200,nothing)
        cv2.setTrackbarMax('scale','VertLine',100)
        cv2.setTrackbarMin('scale','VertLine',-100)
        cv2.setTrackbarPos('scale','VertLine',0)
        cv2.createTrackbar('thr','VertLine',0,255,nothing)
        cv2.setTrackbarMax('thr','VertLine',255)
        cv2.setTrackbarMin('thr','VertLine',0)
        cv2.setTrackbarPos('thr','VertLine',127)
        cv2.createTrackbar('dilPos','VertLine',2,4,nothing)
        cv2.setTrackbarMax('dilPos','VertLine',3)
        cv2.setTrackbarMin('dilPos','VertLine',0)
        cv2.setTrackbarPos('dilPos','VertLine',2)
        cv2.createTrackbar('dilNeg','VertLine',2,4,nothing)
        cv2.setTrackbarMax('dilNeg','VertLine',3)
        cv2.setTrackbarMin('dilNeg','VertLine',0)
        cv2.setTrackbarPos('dilNeg','VertLine',2)
        cv2.createTrackbar('erode','VertLine',0,4,nothing)
        cv2.setTrackbarMax('erode','VertLine',3)
        cv2.setTrackbarMin('erode','VertLine',0)
        cv2.setTrackbarPos('erode','VertLine',0)

    def apply(self, image):
        scale = cv2.getTrackbarPos('scale','VertLine')
        thr = cv2.getTrackbarPos('thr','VertLine')
        dilPos = cv2.getTrackbarPos('dilPos','VertLine')
        dilNeg = cv2.getTrackbarPos('dilNeg','VertLine')
        erode = cv2.getTrackbarPos('erode','VertLine')
        kernel_2 = cv2.getStructuringElement(cv2.MORPH_RECT, (2, 1))
        kernel_3 = cv2.getStructuringElement(cv2.MORPH_RECT, (3, 1))
        anchor = (1,0)

        grad = cv2.Scharr(image, cv2.CV_32F, 1, 0, scale=(1+scale/100)/255./16.)

        grad_pos = cv2.convertScaleAbs(cv2.max(grad, 0.0), alpha=255)
        _,grad_pos = cv2.threshold(grad_pos, thr, 255, cv2.THRESH_BINARY)
        if dilPos > 0:
            grad_pos = cv2.dilate(grad_pos, kernel_2, anchor=anchor, iterations=dilPos)

        grad_neg = cv2.convertScaleAbs(cv2.min(grad, 0.0), alpha=255.0)
        _,grad_neg = cv2.threshold(grad_neg, thr, 255, cv2.THRESH_BINARY)
        if dilNeg > 0:
            grad_neg = cv2.flip(grad_neg, 1)
            grad_neg = cv2.dilate(grad_neg, kernel_2, anchor=anchor, iterations=dilNeg)
            grad_neg = cv2.flip(grad_neg, 1)

        if dilPos > 0 or dilNeg > 0:
            out = cv2.bitwise_and(grad_pos, grad_neg)
        else:
            out = cv2.bitwise_or(grad_pos, grad_neg)
        if erode > 0:
            out = cv2.erode(out, kernel_3, anchor=anchor, iterations=erode)
        cv2.imshow('VertLine',out)
        return out


filters = []
for arg in range(1, len(sys.argv)):
    if sys.argv[arg] == '--gauss':
        filters.append(GaussFilter())
    elif sys.argv[arg] == '--thr' or sys.argv[arg] == '--threshold':
        filters.append(ThresholdFilter())
    elif sys.argv[arg] == '--scale' or sys.argv[arg] == '--gain':
        filters.append(GainBiasFilter())
    elif sys.argv[arg] == '--hsv':
        filters.append(HSVFilter())
    elif sys.argv[arg] == '--channel':
        filters.append(ChannelFilter())
    elif sys.argv[arg] == '--sobel':
        filters.append(SobelFilter())
    elif sys.argv[arg] == '--laplacian' or sys.argv[arg] == '--lapl':
        filters.append(LaplacianFilter())
    elif sys.argv[arg] == '--hline':
        filters.append(HorzLineFilter())
    elif sys.argv[arg] == '--vline':
        filters.append(VertLineFilter())
    elif sys.argv[arg].startswith('-'):
        print(f'Filter {sys.argv[arg]} not known')
    else:
        image_in = cv2.imread(sys.argv[arg])

lines_out = image_in

# Create a window
cv2.namedWindow('image')
cv2.imshow('image',image_in)
cv2.namedWindow('lines')
cv2.imshow('lines',lines_out)
cv2.waitKey(100)

# create trackbars for Hough lines
cv2.createTrackbar('Rho','lines',1,8,nothing)
cv2.setTrackbarMax('Rho','lines',8)
cv2.setTrackbarMin('Rho','lines',1)
cv2.setTrackbarPos('Rho','lines',1)

cv2.createTrackbar('Theta','lines',1,16,nothing)
cv2.setTrackbarMax('Theta','lines',8)
cv2.setTrackbarMin('Theta','lines',-7)
cv2.setTrackbarPos('Theta','lines',4)

cv2.createTrackbar('Votes','lines',0,100,nothing)
cv2.setTrackbarMax('Votes','lines',1200)
cv2.setTrackbarMin('Votes','lines',10)
cv2.setTrackbarPos('Votes','lines',600)

cv2.createTrackbar('Angle','lines',0,41,nothing)
cv2.setTrackbarMax('Angle','lines',+20)
cv2.setTrackbarMin('Angle','lines',-20)
cv2.setTrackbarPos('Angle','lines',0)

print (f"cv2.ocl.haveOpenCL: {cv2.ocl.haveOpenCL()}")
cv2.ocl.setUseOpenCL(True)
print (f"cv2.ocl.useOpenCL: {cv2.ocl.useOpenCL()}")
print (f"cv2.ocl.Device.name: {cv2.ocl.Device.getDefault().name()}")

waitTime = 200

while(1):

    hRho = cv2.getTrackbarPos('Rho','lines')
    hTheta = cv2.getTrackbarPos('Theta','lines')
    hVotes = cv2.getTrackbarPos('Votes','lines')
    hAngle = cv2.getTrackbarPos('Angle','lines')

    img = image_in
    for f in filters:
        img = f.apply(img)

    img = img.astype('uint8')

    lines_out = image_in.copy()
    if hTheta < 1:
        theta = 1-hTheta
    else:
        theta = 1/hTheta
    if cv2.ocl.useOpenCL():
        linesX = cv2.UMat(100, 1, cv2.CV_32FC2)
        linesX = cv2.HoughLines(img, hRho, np.radians(theta), hVotes, linesX)
        lines = cv2.UMat.get(linesX)
    else:
        lines = cv2.HoughLines(img, hRho, np.radians(theta), hVotes, min_theta=np.radians(90+hAngle-10), max_theta=np.radians(90+hAngle+10))
    if lines is not None:
        for i in range(0, min(20, len(lines))):
            rho = lines[i][0][0]
            theta = lines[i][0][1]
            thetaDeg = theta * 180 / math.pi
            a = math.cos(theta)
            b = math.sin(theta)
            x0 = a * rho
            y0 = b * rho
            pt1 = (int(x0 + 1000*(-b)), int(y0 + 1000*(a)))
            pt2 = (int(x0 - 1000*(-b)), int(y0 - 1000*(a)))
            cv2.line(lines_out, pt1, pt2, (255,0,0), 1, cv2.LINE_AA)

    #linesP = cv2.HoughLinesP(img, hRho, np.radians(theta), hVotes, 50, 1)
    #if linesP is not None:
    #    for i in range(0, min(4,len(linesP))):
    #        l = linesP[i][0]
    #        pt1 = (l[0], l[1])
    #        pt2 = (l[2], l[3])
    #        cv2.arrowedLine(lines_out, pt1, pt2, (255,255,255), 1, cv2.LINE_AA)

    # Display output image
    cv2.imshow('image',image_in)
    cv2.imshow('lines',lines_out)

    # Wait longer to prevent freeze for videos.
    if cv2.waitKey(waitTime) & 0xFF == ord('q'):
        break

cv2.destroyAllWindows()
quit()