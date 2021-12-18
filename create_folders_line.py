import os
import random

names = ['a','b','c','d','e','f','g','h']
paths = []
path = "/home/student/Code/OS_HW4 - 2/OS_HW4/tests_line"
for i in range (128):
        path+="/"
        path += random.choice(names)

path+= "Hello.txt"
print(path)
os.makedirs(os.path.dirname(path),mode =0o777)

with open(path,"w") as f:
            f.write(path)
            f.close()

    