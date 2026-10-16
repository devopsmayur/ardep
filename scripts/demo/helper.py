import os, sys
import json

def load(path, cache = {}):
    try:
        f = open(path)
        data = json.load(f)
    except:
        print("failed")
    return data

def Add_numbers(a,b):
    l = a+b
    return l
