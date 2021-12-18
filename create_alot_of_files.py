import os
import random

names = ['a','b','c']
paths = []

path = "/home/student/Code/OS_HW4 - 2/OS_HW4/tests_alot_files"
for x in names:
    for y in names:
        paths.append(path + '/'+ str(x) + '/' +str(y))

files = []
for path in paths:
    for i in range (random.randint (100,2000)):
        file = path +"/"+str(i)
        if (random.random() > 0.5):
            file+= ".txt"
        else:
            file+= ".tomer"
        files.append(file)
for file in files:
    print(file)
    try:
        os.makedirs (os.path.dirname(file))
        
    except:
        pass
    finally:
        with open(file,"w") as f:
                f.write(file)
                f.close()
    