# -*- coding: utf-8 -*-
"""
Created on Mon Dec  7 21:27:47 2020

@author: ASTRA
"""
import pylab
import numpy as np


def loadData(fileName):
    inFile = open(fileName, 'r')
   
    x = np.linspace(0,10000,9999)
    y = []

    for line in inFile:
        trainingSet = line.split(',')
        y.append(int(bin(int(trainingSet[0][:-2]))[-16:-8],2))#进行转换获取帧号信息
    print(y)
    return (x, y)

#绘制该文件中的数据
def plotData(x, y):
    
    pylab.figure(1)
    pylab.scatter(x, y, s=3)
    pylab.xlabel('')
    pylab.ylabel('FRAME_NUM')
    pylab.show()#让绘制的图像在屏幕上显示出来
   
(x, y) = loadData('addresses-locality.txt')
print(x, y)

plotData(x, y)

