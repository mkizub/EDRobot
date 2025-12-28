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
SPACE_X      = 20

class Merge:
    background_gray = 100
    inputImage = None
    inputGtTxt = ""
    mergedImage = None
    mergedGtTxt = ""

    def openImage(self, filename):
        print(f'Reading image {filename}')
        self.inputImage = cv2.imread(filename, cv2.IMREAD_GRAYSCALE)
        histImage = self.inputImage[0:, 0:5]
        hist_g = cv2.calcHist([histImage], [0], None, [256], [0, 256])
        self.background_gray = np.argmax(hist_g)
        gt_filename = filename[:-4] + ".gt.txt"
        print(f'Reading groundtruth {gt_filename}')
        with open(gt_filename, 'r', encoding='utf-8') as file:
            for line in file:
                self.inputGtTxt = line.strip();
                break

    def appendImage(self):
        if self.mergedImage is None:
            self.mergedImage = self.inputImage.copy()
            self.mergedGtTxt = self.inputGtTxt
        else:
            bgImage = np.full((LINE_HEIGHT, SPACE_X), self.background_gray, dtype=np.uint8)
            self.mergedImage = np.concatenate((self.mergedImage, bgImage, self.inputImage), axis=1)
            self.mergedGtTxt = self.mergedGtTxt + " " + self.inputGtTxt

    def saveImage(self, filename):
        cv2.imwrite(filename,self.mergedImage)
        gt_filename = filename[:-4] + ".gt.txt"
        print(f'Writing groundtruth {gt_filename}')
        encoded_bytes = self.mergedGtTxt.encode('utf-8')
        with open(gt_filename, 'wb') as file:
            file.write(encoded_bytes)
        self.mergedImage = None
        self.mergedGtTxt = ""

    def run(self):
        if len(sys.argv) != 4:
            print("python {sys.argv[0]} -num input*mask.png outputdir/")
            print("python {sys.argv[0]} -abc input*mask*mask.png outputdir/")
            return

        output_dir = sys.argv[3]
        os.makedirs(output_dir, exist_ok=True)

        bgn_index = -1
        end_index = -1
        mode = sys.argv[1]
        if mode == '-num':
            mask = sys.argv[2]
            index = mask.find('*')
            if index == -1:
                print("python {sys.argv[0]} -num input*mask.png outputdir/")
                return
            mask_start = mask[:index]
            mask_end = mask[index+1:]
            for i in range(0,1000):
                fname = mask_start + str(i) + mask_end
                if not os.path.isfile(fname):
                    continue
                if bgn_index < 0:
                    bgn_index = i
                end_index = i
                self.openImage(fname)
                self.appendImage()
                height, width = self.mergedImage.shape
                if width > 1600:
                    fname = mask_start + str(bgn_index) + '~' + str(end_index) + mask_end
                    self.saveImage(os.path.join(output_dir, fname))
                    bgn_index = -1
                    end_index = -1
        if mode == '-abc':
            mask = sys.argv[2]
            index = mask.find('*')
            if index == -1:
                print("python {sys.argv[0]} -abc *input*mask.png outputdir/")
                return
            png_files = glob.glob(mask)
            mask_start = mask[:index]
            mask_end = mask[index+1:]
            mask_end = mask_end.replace('*', '')
            for i in range(0,len(png_files)):
                fname = png_files[i]
                if bgn_index < 0:
                    bgn_index = i
                end_index = i
                self.openImage(fname)
                self.appendImage()
                height, width = self.mergedImage.shape
                if width > 1600:
                    fname = mask_start + str(bgn_index) + '~' + str(end_index) + mask_end
                    self.saveImage(os.path.join(output_dir, fname))
                    bgn_index = -1
                    end_index = -1

        if bgn_index >= 0:
            fname = mask_start + str(bgn_index) + '~' + str(end_index) + mask_end
            self.saveImage(os.path.join(output_dir, fname))

bl = Merge()
bl.run()
