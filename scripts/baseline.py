import cv2
import glob
import os
import sys
import numpy as np

LINE_HEIGHT  = 38
LEADING      = 6
ASCENT       = 24
DESCENT      = 8
BASELINE_Y   = 30

class Baseline:
    currentDir = '.'
    currentFile = None
    currentFileIndex = -1
    background_gray = None
    currentImage = None
    currentOffset = 0

    def openImage(self):
        print(f'Reading image {self.currentFile}')
        self.currentOffset = 0
        self.currentImage = cv2.imread(self.currentFile, cv2.IMREAD_GRAYSCALE)
        hist_g = cv2.calcHist([self.currentImage], [0], None, [256], [0, 256])
        self.background_gray = np.argmax(hist_g)
        cv2.setWindowTitle('image', os.path.basename(self.currentFile))

    def copyImage(self):
        height, width = self.currentImage.shape
        displayImage = np.zeros((height, width), np.uint8)
        displayImage[:] = self.background_gray
        if self.currentOffset == 0:
            displayImage[0:height, 0:width] = self.currentImage[0:height, 0:width]
        elif self.currentOffset > 0:
            srcROI = self.currentImage[0:height-self.currentOffset, 0:width]
            displayImage[self.currentOffset:height, 0:width] = srcROI
        elif self.currentOffset < 0:
            srcROI = self.currentImage[-self.currentOffset:height, 0:width]
            displayImage[0:height+self.currentOffset, 0:width] = srcROI
        return displayImage

    def showImage(self):
        displayImage = self.copyImage()
        height, width = displayImage.shape
        cv2.line(displayImage,(0,BASELINE_Y),(width,BASELINE_Y),(0),1)
        cv2.resizeWindow('image',width*2,LINE_HEIGHT*2)
        cv2.imshow('image',displayImage)

    def saveImage(self):
        base_name, _ = os.path.splitext(self.currentFile)
        bak_filename = base_name + '.bak'
        if not os.path.exists(bak_filename):
            os.rename(self.currentFile, bak_filename)
        saveImage = self.copyImage()
        cv2.imwrite(self.currentFile,saveImage)

    def nextImage(self):
        png_files = glob.glob(f"{self.currentDir}/*.png")
        png_files.sort()
        if not self.currentFile:
            self.currentFileIndex = 0
        else:
            try:
                self.currentFileIndex = png_files.index(self.currentFile) + 1
            except ValueError:
                self.currentFileIndex = 0
        if self.currentFileIndex >= len(png_files):
            self.currentFileIndex = len(png_files) - 1;
        self.currentFile = png_files[self.currentFileIndex]
        print(f'Current image #{self.currentFileIndex}: {self.currentFile}')
        self.openImage()

    def prevImage(self):
        png_files = glob.glob(f"{self.currentDir}/*.png")
        png_files.sort()
        if not self.currentFile:
            self.currentFileIndex = 0
        else:
            try:
                self.currentFileIndex = png_files.index(self.currentFile) - 1
            except ValueError:
                self.currentFileIndex = 0
        if self.currentFileIndex < 0:
            self.currentFileIndex = 0;
        self.currentFile = png_files[self.currentFileIndex]
        print(f'Current image #{self.currentFileIndex}: {self.currentFile}')
        self.openImage()

    def run(self):
        # Create a window
        cv2.namedWindow('image', cv2.WINDOW_KEEPRATIO)

        if len(sys.argv) > 1:
            if os.path.isdir(sys.argv[1]):
                self.currentDir = sys.argv[1]
                print(f'Using dir {self.currentDir}')
            else:
                self.currentFile = sys.argv[1]
                self.currentDir = os.path.dirname(self.currentFile)
                print(f'Using dir {self.currentDir}')
                print(f'Current image #?: {self.currentFile}')

        if not self.currentFile:
            self.nextImage()
        else:
            self.openImage()

        while(1):
            self.showImage()

            key = cv2.waitKey()
            print(f"Key pressed: {key}")
            if key == 27 or key & 0xFF == ord('q'): # esc or q
                break
            elif key == 13: # enter
                self.saveImage()
                self.nextImage()
            elif key & 0xFF == ord('d'):
                self.nextImage()
            elif key & 0xFF == ord('a'):
                self.prevImage()
            elif key & 0xFF == ord('w'):
                self.currentOffset -= 1
            elif key & 0xFF == ord('s'):
                self.currentOffset += 1

        cv2.destroyAllWindows()

bl = Baseline()
bl.run()
