# -*- coding: utf-8 -*-
"""
Created on Mon Dec  7 21:06:54 2020

@author: ASTRA
"""
f = open("address.txt","w")
n = 10000
n_2 = 0
n_1 = 1
current = 1


for x in range(2, n+1):
  current = n_2 + n_1
  n_2 = n_1
  n_1 = current
  print(str(id(current)),file=f)