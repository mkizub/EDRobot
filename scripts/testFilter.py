import math
import sys
import numpy as np
import cv2


def nothing(x):
    pass


class Trackbar:

    def __init__(self, name: str, window: str, min_val: int, max_val: int, init_val: int):
        self.name = name
        self.window = window
        self.min_val = min_val
        self.min_val = max_val
        cv2.createTrackbar(self.name, self.window, init_val, max_val, nothing)
        cv2.setTrackbarMax(self.name, self.window, max_val)
        cv2.setTrackbarMin(self.name, self.window, min_val)
        cv2.setTrackbarPos(self.name, self.window, init_val)

    def get(self):
        return cv2.getTrackbarPos(self.name, self.window)


class Filter:

    def __init__(self, name: str):
        self.name = name
        cv2.namedWindow(name)

    def getTrackbarPos(self, tb: str):
        return cv2.getTrackbarPos(tb, self.name)

    def apply(self, image):
        return image

    def title(self, title:str):
        cv2.setWindowTitle(self.name, title)

    def show(self, image):
        cv2.imshow(self.name, image)


class ThresholdFilter(Filter):
    tb_thr: Trackbar

    def __init__(self):
        super().__init__('Threshold')
        self.tb_thr = Trackbar('thr', self.name, 0, 255, 127)

    def apply(self, image):
        thr = self.tb_thr.get()
        _, out = cv2.threshold(image, thr, 255, cv2.THRESH_BINARY)
        self.title(f'Threshold: {thr}')
        self.show(out)
        return out


class GaussFilter(Filter):

    tb_ky: Trackbar
    tb_kx: Trackbar

    def __init__(self):
        super().__init__('Gauss')
        self.tb_kx = Trackbar('kx', self.name, 1, 9, 1)
        self.tb_ky = Trackbar('ky', self.name, 1, 9, 1)

    def apply(self, image):
        kx = self.tb_kx.get()
        ky = self.tb_ky.get()
        if (kx % 2) == 0:
            kx += 1
        if (ky % 2) == 0:
            ky += 1
        self.title(f'Gauss: kx={kx} ky={ky}')
        if kx == 1 and ky == 1:
            self.show(image)
            return image
        out = cv2.GaussianBlur(image, (kx, ky), 0)
        self.show(out)
        return out


class DoGFilter(Filter):

    def __init__(self):
        super().__init__('Gauss')
        self.tb_sigm1 = Trackbar('Sigma1', self.name, 50, 300, 300)
        self.tb_sigm2 = Trackbar('Sigma2', self.name, 50, 300, 500)
        self.tb_scale = Trackbar('Scale', self.name, 10, 100, 50)

    def apply(self, image):
        s1 = self.tb_sigm1.get() * 0.01
        s2 = self.tb_sigm2.get() * 0.01
        if s1 < s2:
            s1, s2 = s2, s1
        scl = self.tb_scale.get() * 0.1
        self.title(f'DoG: Sigma1={s1:.2f} Sigma2={s2:.2f} Scale={scl:.1f}')
        g1 = cv2.GaussianBlur(image, (0, 0), s1)
        g2 = cv2.GaussianBlur(image, (0, 0), s2)
        dog = cv2.subtract(g2, g1)
        out = cv2.convertScaleAbs(dog * scl)
        self.show(out)
        return out


class GainBiasFilter(Filter):

    tb_bias: Trackbar
    tb_gain: Trackbar

    def __init__(self):
        super().__init__('Scale')
        self.tb_gain = Trackbar('Gain', self.name, -100, +100, 0)
        self.tb_bias = Trackbar('Bias', self.name, -100, +100, 0)

    def apply(self, image):
        gain = self.tb_gain.get()
        bias = self.tb_bias.get()
        self.title(f'Gain={1.0 + gain * 0.01:.2f} Bias={bias}')
        if gain == 0 and bias == 0:
            self.show(image)
            return image
        out = cv2.convertScaleAbs(image, alpha=(1.0 + gain * 0.01), beta=bias)
        self.show(out)
        return out


class HSVFilter(Filter):

    def __init__(self):
        super().__init__('HSV')
        self.tb_mode = Trackbar('Mode', self.name, 0, 3, 0)
        self.tb_HMin = Trackbar('HMin', self.name, 0, 179, 0)
        self.tb_HMax = Trackbar('HMax', self.name, 0, 179, 179)
        self.tb_SMin = Trackbar('SMin', self.name, 0, 255, 0)
        self.tb_SMax = Trackbar('SMax', self.name, 0, 255, 255)
        self.tb_VMin = Trackbar('VMin', self.name, 0, 255, 0)
        self.tb_VMax = Trackbar('VMax', self.name, 0, 255, 255)

    def apply(self, image):
        mode = self.tb_mode.get()
        hMin = self.tb_HMin.get()
        hMax = self.tb_HMax.get()
        sMin = self.tb_SMin.get()
        sMax = self.tb_SMax.get()
        vMin = self.tb_VMin.get()
        vMax = self.tb_VMax.get()
        lower = np.array([hMin, sMin, vMin])
        upper = np.array([hMax, sMax, vMax])
        hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)
        mask = cv2.inRange(hsv, lower, upper)
        if mode == 0:
            self.title(f'HSV [{hMin},{sMin},{vMin} .. {hMax},{sMax},{vMax}] -> Color')
            out = cv2.bitwise_and(image, image, mask=mask)
        elif mode == 1:
            self.title(f'HSV [{hMin},{sMin},{vMin} .. {hMax},{sMax},{vMax}] -> Gray')
            gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
            out = cv2.bitwise_and(gray, gray, mask=mask)
        elif mode == 2:
            self.title(f'HSV [{hMin},{sMin},{vMin} .. {hMax},{sMax},{vMax}] -> Mask')
            out = mask
        elif mode == 3:
            self.title(f'HSV [{hMin},{sMin},{vMin} .. {hMax},{sMax},{vMax}] -> Value')
            gray = cv2.split(hsv)[2]
            out = cv2.bitwise_and(gray, gray, mask=mask)
        else:
            self.title(f'HSV [{hMin},{sMin},{vMin} .. {hMax},{sMax},{vMax}] -> ???')
            out = mask
        self.show(out)
        return out


class ChannelFilter(Filter):
    tb_mode: Trackbar

    def __init__(self):
        super().__init__('Channel')
        self.tb_mode = Trackbar('Mode', self.name, 0, 6, 0)

    def apply(self, image):
        mode = self.tb_mode.get()
        if len(image.shape) == 2:
            self.title('Channel error: grayscale')
            out = image
        elif mode == 1 or mode == 2 or mode == 3:
            b, g, r = cv2.split(image)
            if mode == 3:
                self.title('BGR -> Blue')
                out = b
            elif mode == 2:
                self.title('BGR -> Green')
                out = g
            else:
                self.title('BGR -> Red')
                out = r
        elif mode == 4 or mode == 5 or mode == 6:
            hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)
            h, s, v = cv2.split(image)
            if mode == 6:
                self.title('BGR -> Value')
                out = v
            elif mode == 5:
                self.title('BGR -> Saturate')
                out = s
            else:
                self.title('BGR -> Hue')
                out = h
        else:
            self.title('BGR -> Gray')
            out = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)

        self.show(out)
        return out


class SobelFilter(Filter):

    def __init__(self):
        super().__init__('Sobel')
        cv2.createTrackbar('kern', 'Sobel', 1, 4, nothing)
        cv2.setTrackbarMax('kern', 'Sobel', 9)
        cv2.setTrackbarMin('kern', 'Sobel', 1)
        cv2.setTrackbarPos('kern', 'Sobel', 1)
        cv2.createTrackbar('scale', 'Sobel', 0, 200, nothing)
        cv2.setTrackbarMax('scale', 'Sobel', 100)
        cv2.setTrackbarMin('scale', 'Sobel', -100)
        cv2.setTrackbarPos('scale', 'Sobel', 0)
        cv2.createTrackbar('delta', 'Sobel', 0, 200, nothing)
        cv2.setTrackbarMax('delta', 'Sobel', 100)
        cv2.setTrackbarMin('delta', 'Sobel', -100)
        cv2.setTrackbarPos('delta', 'Sobel', 0)
        cv2.createTrackbar('derv', 'Sobel', 1, 2, nothing)
        cv2.setTrackbarMax('derv', 'Sobel', 2)
        cv2.setTrackbarMin('derv', 'Sobel', 1)
        cv2.setTrackbarPos('derv', 'Sobel', 1)

    def apply(self, image):
        kern = cv2.getTrackbarPos('kern', 'Sobel')
        if (kern % 2) == 0:
            kern = kern + 1
        cv2.setWindowTitle('Sobel', f'Sobel kern: {kern}')
        scale = cv2.getTrackbarPos('scale', 'Sobel')
        delta = cv2.getTrackbarPos('delta', 'Sobel')
        derv = cv2.getTrackbarPos('derv', 'Sobel')
        if scale <= -100:
            scale = -99
        f_grad_x = cv2.Sobel(image, cv2.CV_32FC1, derv, 0, ksize=kern, scale=(1 + scale / 100) / 255, delta=delta / 100)
        u_grad_x = cv2.convertScaleAbs(f_grad_x, alpha=255)
        f_grad_y = cv2.Sobel(image, cv2.CV_32FC1, 0, derv, ksize=kern, scale=(1 + scale / 100) / 255, delta=delta / 100)
        u_grad_y = cv2.convertScaleAbs(f_grad_y, alpha=255)
        out = cv2.addWeighted(u_grad_x, 0.5, u_grad_y, 0.5, 0)
        cv2.imshow('Sobel', out)
        return out


class LaplacianFilter(Filter):

    def __init__(self):
        super().__init__('Laplacian')
        cv2.createTrackbar('kern', 'Laplacian', 1, 9, nothing)
        cv2.setTrackbarMax('kern', 'Laplacian', 9)
        cv2.setTrackbarMin('kern', 'Laplacian', 1)
        cv2.setTrackbarPos('kern', 'Laplacian', 1)
        cv2.createTrackbar('scale', 'Laplacian', 0, 200, nothing)
        cv2.setTrackbarMax('scale', 'Laplacian', 100)
        cv2.setTrackbarMin('scale', 'Laplacian', -100)
        cv2.setTrackbarPos('scale', 'Laplacian', 0)
        cv2.createTrackbar('delta', 'Laplacian', 0, 200, nothing)
        cv2.setTrackbarMax('delta', 'Laplacian', 100)
        cv2.setTrackbarMin('delta', 'Laplacian', -100)
        cv2.setTrackbarPos('delta', 'Laplacian', 0)

    def apply(self, image):
        kern = cv2.getTrackbarPos('kern', 'Laplacian')
        if (kern % 2) == 0:
            kern = kern + 1
        cv2.setWindowTitle('Laplacian', f'Laplacian kern: {kern}')
        scale = cv2.getTrackbarPos('scale', 'Laplacian')
        delta = cv2.getTrackbarPos('delta', 'Laplacian')
        if scale <= -100:
            scale = -99
        lapl = cv2.Laplacian(image, cv2.CV_32FC1, ksize=kern, scale=1 + scale / 100, delta=delta)
        out = cv2.convertScaleAbs(lapl)
        cv2.imshow('Laplacian', out)
        return out


class HorzLineFilter(Filter):

    def __init__(self):
        super().__init__('HorzLine')
        cv2.createTrackbar('scale', 'HorzLine', 0, 200, nothing)
        cv2.setTrackbarMax('scale', 'HorzLine', 100)
        cv2.setTrackbarMin('scale', 'HorzLine', -100)
        cv2.setTrackbarPos('scale', 'HorzLine', 0)
        cv2.createTrackbar('thr', 'HorzLine', 0, 255, nothing)
        cv2.setTrackbarMax('thr', 'HorzLine', 255)
        cv2.setTrackbarMin('thr', 'HorzLine', 0)
        cv2.setTrackbarPos('thr', 'HorzLine', 127)
        cv2.createTrackbar('dilPos', 'HorzLine', 2, 4, nothing)
        cv2.setTrackbarMax('dilPos', 'HorzLine', 3)
        cv2.setTrackbarMin('dilPos', 'HorzLine', 0)
        cv2.setTrackbarPos('dilPos', 'HorzLine', 2)
        cv2.createTrackbar('dilNeg', 'HorzLine', 2, 4, nothing)
        cv2.setTrackbarMax('dilNeg', 'HorzLine', 3)
        cv2.setTrackbarMin('dilNeg', 'HorzLine', 0)
        cv2.setTrackbarPos('dilNeg', 'HorzLine', 2)
        cv2.createTrackbar('erode', 'HorzLine', 0, 4, nothing)
        cv2.setTrackbarMax('erode', 'HorzLine', 3)
        cv2.setTrackbarMin('erode', 'HorzLine', 0)
        cv2.setTrackbarPos('erode', 'HorzLine', 0)

    def apply(self, image):
        scale = cv2.getTrackbarPos('scale', 'HorzLine')
        thr = cv2.getTrackbarPos('thr', 'HorzLine')
        dilPos = cv2.getTrackbarPos('dilPos', 'HorzLine')
        dilNeg = cv2.getTrackbarPos('dilNeg', 'HorzLine')
        erode = cv2.getTrackbarPos('erode', 'HorzLine')
        kernel_2 = cv2.getStructuringElement(cv2.MORPH_RECT, (1, 2))
        kernel_3 = cv2.getStructuringElement(cv2.MORPH_RECT, (1, 3))
        anchor = (0, 1)

        grad = cv2.Scharr(image, cv2.CV_32F, 0, 1, scale=(1 + scale / 100) / 255. / 16.)

        grad_pos = cv2.convertScaleAbs(cv2.max(grad, 0.0), alpha=255)
        _, grad_pos = cv2.threshold(grad_pos, thr, 255, cv2.THRESH_BINARY)
        if dilPos > 0:
            grad_pos = cv2.dilate(grad_pos, kernel_2, anchor=anchor, iterations=dilPos)

        grad_neg = cv2.convertScaleAbs(cv2.min(grad, 0.0), alpha=255.0)
        _, grad_neg = cv2.threshold(grad_neg, thr, 255, cv2.THRESH_BINARY)
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
        cv2.imshow('HorzLine', out)
        return out


class VertLineFilter(Filter):

    def __init__(self):
        super().__init__('VertLine')
        cv2.createTrackbar('scale', 'VertLine', 0, 200, nothing)
        cv2.setTrackbarMax('scale', 'VertLine', 100)
        cv2.setTrackbarMin('scale', 'VertLine', -100)
        cv2.setTrackbarPos('scale', 'VertLine', 0)
        cv2.createTrackbar('thr', 'VertLine', 0, 255, nothing)
        cv2.setTrackbarMax('thr', 'VertLine', 255)
        cv2.setTrackbarMin('thr', 'VertLine', 0)
        cv2.setTrackbarPos('thr', 'VertLine', 127)
        cv2.createTrackbar('dilPos', 'VertLine', 2, 4, nothing)
        cv2.setTrackbarMax('dilPos', 'VertLine', 3)
        cv2.setTrackbarMin('dilPos', 'VertLine', 0)
        cv2.setTrackbarPos('dilPos', 'VertLine', 2)
        cv2.createTrackbar('dilNeg', 'VertLine', 2, 4, nothing)
        cv2.setTrackbarMax('dilNeg', 'VertLine', 3)
        cv2.setTrackbarMin('dilNeg', 'VertLine', 0)
        cv2.setTrackbarPos('dilNeg', 'VertLine', 2)
        cv2.createTrackbar('erode', 'VertLine', 0, 4, nothing)
        cv2.setTrackbarMax('erode', 'VertLine', 3)
        cv2.setTrackbarMin('erode', 'VertLine', 0)
        cv2.setTrackbarPos('erode', 'VertLine', 0)

    def apply(self, image):
        scale = cv2.getTrackbarPos('scale', 'VertLine')
        thr = cv2.getTrackbarPos('thr', 'VertLine')
        dilPos = cv2.getTrackbarPos('dilPos', 'VertLine')
        dilNeg = cv2.getTrackbarPos('dilNeg', 'VertLine')
        erode = cv2.getTrackbarPos('erode', 'VertLine')
        kernel_2 = cv2.getStructuringElement(cv2.MORPH_RECT, (2, 1))
        kernel_3 = cv2.getStructuringElement(cv2.MORPH_RECT, (3, 1))
        anchor = (1, 0)

        grad = cv2.Scharr(image, cv2.CV_32F, 1, 0, scale=(1 + scale / 100) / 255. / 16.)

        grad_pos = cv2.convertScaleAbs(cv2.max(grad, 0.0), alpha=255)
        _, grad_pos = cv2.threshold(grad_pos, thr, 255, cv2.THRESH_BINARY)
        if dilPos > 0:
            grad_pos = cv2.dilate(grad_pos, kernel_2, anchor=anchor, iterations=dilPos)

        grad_neg = cv2.convertScaleAbs(cv2.min(grad, 0.0), alpha=255.0)
        _, grad_neg = cv2.threshold(grad_neg, thr, 255, cv2.THRESH_BINARY)
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
        cv2.imshow('VertLine', out)
        return out


class LinesFilter(Filter):

    def __init__(self):
        super().__init__('Lines')
        cv2.createTrackbar('Rho', self.name, 1, 8, nothing)
        cv2.setTrackbarMax('Rho', self.name, 8)
        cv2.setTrackbarMin('Rho', self.name, 1)
        cv2.setTrackbarPos('Rho', self.name, 1)

        cv2.createTrackbar('Theta', self.name, 1, 16, nothing)
        cv2.setTrackbarMax('Theta', self.name, 8)
        cv2.setTrackbarMin('Theta', self.name, -7)
        cv2.setTrackbarPos('Theta', self.name, 4)

        cv2.createTrackbar('Votes', self.name, 0, 100, nothing)
        cv2.setTrackbarMax('Votes', self.name, 1200)
        cv2.setTrackbarMin('Votes', self.name, 10)
        cv2.setTrackbarPos('Votes', self.name, 600)

        cv2.createTrackbar('Angle', self.name, 0, 41, nothing)
        cv2.setTrackbarMax('Angle', self.name, +20)
        cv2.setTrackbarMin('Angle', self.name, -20)
        cv2.setTrackbarPos('Angle', self.name, 0)

    def apply(self, image):
        hRho = self.getTrackbarPos('Rho')
        hTheta = self.getTrackbarPos('Theta')
        hVotes = self.getTrackbarPos('Votes')
        hAngle = self.getTrackbarPos('Angle')

        img = image.astype('uint8')

        out = image_in.copy()
        if hTheta < 1:
            theta = 1 - hTheta
        else:
            theta = 1 / hTheta
        if cv2.ocl.useOpenCL():
            linesX = cv2.UMat(100, 1, cv2.CV_32FC2)
            linesX = cv2.HoughLines(img, hRho, np.radians(theta), hVotes, linesX)
            lines = cv2.UMat.get(linesX)
        else:
            lines = cv2.HoughLines(img, hRho, np.radians(theta), hVotes, min_theta=np.radians(90 + hAngle - 10),
                                   max_theta=np.radians(90 + hAngle + 10))
        if lines is not None:
            for i in range(0, min(20, len(lines))):
                rho = lines[i][0][0]
                theta = lines[i][0][1]
                thetaDeg = theta * 180 / math.pi
                a = math.cos(theta)
                b = math.sin(theta)
                x0 = a * rho
                y0 = b * rho
                pt1 = (int(x0 + 1000 * (-b)), int(y0 + 1000 * (a)))
                pt2 = (int(x0 - 1000 * (-b)), int(y0 - 1000 * (a)))
                cv2.line(out, pt1, pt2, (255, 0, 0), 1, cv2.LINE_AA)
        cv2.imshow(self.name, out)
        return out


filters = []
for arg in range(1, len(sys.argv)):
    if sys.argv[arg] == '--gauss':
        filters.append(GaussFilter())
    elif sys.argv[arg] == '--dog' or sys.argv[arg] == '--DoG':
        filters.append(DoGFilter())
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
    elif sys.argv[arg] == '--lines':
        filters.append(LinesFilter())
    elif sys.argv[arg].startswith('-'):
        print(f'Filter {sys.argv[arg]} not known')
    else:
        image_in = cv2.imread(sys.argv[arg])

# Create a window
cv2.namedWindow('image')
cv2.imshow('image', image_in)
cv2.waitKey(100)

# create trackbars for Hough lines

print(f"cv2.ocl.haveOpenCL: {cv2.ocl.haveOpenCL()}")
cv2.ocl.setUseOpenCL(True)
print(f"cv2.ocl.useOpenCL: {cv2.ocl.useOpenCL()}")
print(f"cv2.ocl.Device.name: {cv2.ocl.Device.getDefault().name()}")

waitTime = 200

while (1):

    img = image_in
    for f in filters:
        img = f.apply(img)

    # Wait longer to prevent freeze for videos.
    if cv2.waitKey(waitTime) & 0xFF == ord('q'):
        break

cv2.destroyAllWindows()
quit()
