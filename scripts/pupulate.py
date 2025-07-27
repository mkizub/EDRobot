import cv2
import glob
import os
import shutil
import sys
import time
import numpy as np

LINE_HEIGHT  = 38
LEADING      = 6
ASCENT       = 24
DESCENT      = 8
BASELINE_Y   = 30

FILES_CHUNK   = 0 #20

class Populate:
    inpDir = None
    outDir = None
    srcFiles = []
    currentBaseName = None
    background_gray = None
    currentImage = None

    def openImage(self, file):
        print(f'Reading image from {file}')
        self.currentImage = cv2.imread(file, cv2.IMREAD_GRAYSCALE)
        hist_g = cv2.calcHist([self.currentImage], [0], None, [256], [0, 256])
        self.background_gray = np.argmax(hist_g)
        self.currentBaseName = os.path.basename(file)[:-9] # strip -gray.png

    def shift(self, image, step):
        height, width = image.shape
        out = np.zeros((height, width), np.uint8)
        out.fill(self.background_gray)
        if step == 0:
            out[0:height, 0:width] = image[0:height, 0:width]
        elif step > 0:
            out[step:height, 0:width] = image[0:height-step, 0:width]
        elif step < 0:
            out[0:height+step, 0:width] = image[-step:height, 0:width]
        return out

    def scale(self, image, step):
        height, width = image.shape
        scaled_height = height + 2*step
        scaled_width = round(width * scaled_height / height);
        #print(f"Scale from {width}:{height} to {scaled_width}:{scaled_height}")
        scaled = cv2.resize(image, (scaled_width,scaled_height))
        if step == 0:
            out = image
        elif step > 0:
            out = scaled[step:height+step, :]
        elif step < 0:
            out = np.full((height, scaled_width), self.background_gray, dtype=np.uint8)
            out[-step:height+step, 0:scaled_width] = scaled[:,:]
        #oh, ow = out.shape
        #print(f"out {ow}:{oh}")
        return out

    def saveImage(self, index, image, suffix):
        outDir = self.outDir
        if FILES_CHUNK > 0:
            chunk = round(index / FILES_CHUNK)
            outDir = f'{outDir}/{chunk:03d}'
        if not os.path.exists(outDir):
            os.mkdir(outDir)
        cv2.imwrite(f'{outDir}/{self.currentBaseName}{suffix}-gray.png', image)
        shutil.copyfile(f'{self.inpDir}/{self.currentBaseName}-gray.gt.txt', f'{outDir}/{self.currentBaseName}{suffix}-gray.gt.txt')

    def run(self):
        if len(sys.argv) != 3 or not os.path.isdir(sys.argv[1]):
            print("python {sys.argv[0]} input_dir output_dir")
            return

        self.inpDir = sys.argv[1]
        self.outDir = sys.argv[2]
        print(f'Populate images from {self.inpDir} to {self.outDir}')
        if not os.path.exists(self.outDir):
          os.mkdir(self.outDir)
        self.srcFiles = glob.glob(f"{self.inpDir}/*-gray.png")
        self.srcFiles.sort()

        for i in range(len(self.srcFiles)):
            file = self.srcFiles[i]
            self.openImage(file)
            src = self.currentImage
            dst = src
            self.saveImage(i,dst,"")

            dst = self.shift(src,-1)
            self.saveImage(i,dst,"_y-1")
            dst = self.shift(src,-2)
            self.saveImage(i,dst,"_y-2")

            dst = self.shift(src, +1)
            self.saveImage(i,dst,"_y+1")
            dst = self.shift(src,+2)
            self.saveImage(i,dst,"_y+2")

            src = self.scale(self.currentImage, -1)
            dst = src
            self.saveImage(i,dst,"_yd+0")
            dst = self.shift(src,-1)
            self.saveImage(i,dst,"_yd-1")
            dst = self.shift(src, +1)
            self.saveImage(i,dst,"_yd+1")

            src = self.scale(self.currentImage, +1)
            dst = src
            self.saveImage(i,dst,"_yu+0")
            dst = self.shift(src,-1)
            self.saveImage(i,dst,"_yu-1")
            dst = self.shift(src, +1)
            self.saveImage(i,dst,"_yu+1")


populate = Populate()
populate.run()
